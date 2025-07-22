#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <limits.h>

#define MAX_WRAP_COLS 1000000

// Global flag for signal handling
static volatile sig_atomic_t interrupted = 0;

void signal_handler(int sig) {
    (void)sig;
    interrupted = 1;
}

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTION]... [FILE]\n", program_name);
    printf("Binary encode or decode FILE, or standard input, to standard output.\n");
    printf("With no FILE, or when FILE is -, read standard input.\n\n");
    printf("  -d, --decode          decode binary data (convert binary text to file)\n");
    printf("  -w, --wrap=COLS       wrap encoded lines after COLS characters (default 64)\n");
    printf("                        Use 0 to disable line wrapping\n");
    printf("      --help           display this help and exit\n");
    printf("      --version        output version information and exit\n");
}

void print_version(void) {
    printf("binary 1.0\n");
    printf("Simple binary encoder/decoder\n");
}

// Secure string to long conversion with validation
int parse_long(const char *str, long *result, long min_val, long max_val) {
    char *endptr;
    long val;
    
    if (!str || !result) return -1;
    
    errno = 0;
    val = strtol(str, &endptr, 10);
    
    if (errno != 0 || *endptr != '\0' || val < min_val || val > max_val) {
        return -1;
    }
    
    *result = val;
    return 0;
}

// Validate file before opening
int validate_file(const char *filename) {
    struct stat st;
    
    if (stat(filename, &st) != 0) {
        return 0; // File doesn't exist, will be caught by fopen
    }
    
    if (S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s is a directory\n", filename);
        return -1;
    }
    
    return 0;
}

// Safe file operations with error checking
int safe_fputc(int c, FILE *stream) {
    if (interrupted) return EOF;
    
    int result = fputc(c, stream);
    if (result == EOF) {
        fprintf(stderr, "Error: failed to write output\n");
        return EOF;
    }
    return result;
}

// Convert file to binary text (encode)
int encode_file(FILE *input, FILE *output, int wrap_cols) {
    int byte;
    int col_count = 0;
    
    while ((byte = fgetc(input)) != EOF) {
        if (interrupted) {
            fprintf(stderr, "Operation interrupted\n");
            return -1;
        }
        
        if (ferror(input)) {
            fprintf(stderr, "Error reading input\n");
            return -1;
        }
        
        // Convert each bit of the byte to '0' or '1'
        for (int i = 7; i >= 0; i--) {
            if (safe_fputc(((byte >> i) & 1) ? '1' : '0', output) == EOF) {
                return -1;
            }
            col_count++;
            
            // Add line wrap if specified
            if (wrap_cols > 0 && col_count >= wrap_cols) {
                if (safe_fputc('\n', output) == EOF) {
                    return -1;
                }
                col_count = 0;
            }
        }
    }
    
    // Add final newline if we haven't wrapped
    if (wrap_cols == 0 || col_count > 0) {
        if (safe_fputc('\n', output) == EOF) {
            return -1;
        }
    }
    
    if (fflush(output) != 0) {
        fprintf(stderr, "Error: failed to flush output\n");
        return -1;
    }
    
    return 0;
}

// Convert binary text to file (decode)
int decode_file(FILE *input, FILE *output) {
    int bit_char;
    unsigned char byte = 0;
    int bit_count = 0;
    long total_bits = 0;
    
    while ((bit_char = fgetc(input)) != EOF) {
        if (interrupted) {
            fprintf(stderr, "Operation interrupted\n");
            return -1;
        }
        
        if (ferror(input)) {
            fprintf(stderr, "Error reading input\n");
            return -1;
        }
        
        // Skip non-binary characters (whitespace, etc.)
        if (bit_char != '0' && bit_char != '1') {
            continue;
        }
        
        // Prevent potential overflow
        if (total_bits > LONG_MAX - 1) {
            fprintf(stderr, "Error: input too large\n");
            return -1;
        }
        total_bits++;
        
        // Build byte from bits
        byte = (byte << 1) | (unsigned char)(bit_char - '0');
        bit_count++;
        
        // When we have 8 bits, output the byte
        if (bit_count == 8) {
            if (safe_fputc(byte, output) == EOF) {
                return -1;
            }
            byte = 0;
            bit_count = 0;
        }
    }
    
    // Warn if incomplete byte (not divisible by 8)
    if (bit_count != 0) {
        fprintf(stderr, "Warning: incomplete byte at end of input (%d bits)\n", bit_count);
    }
    
    if (fflush(output) != 0) {
        fprintf(stderr, "Error: failed to flush output\n");
        return -1;
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    int decode_mode = 0;
    long wrap_cols = 64;  // Default wrap at 64 characters
    const char *filename = NULL;
    FILE *input = stdin;
    FILE *output = stdout;
    int exit_code = 0;
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Parse command line options
    static struct option long_options[] = {
        {"decode", no_argument, 0, 'd'},
        {"wrap", required_argument, 0, 'w'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "dw:hv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                decode_mode = 1;
                break;
            case 'w':
                if (parse_long(optarg, &wrap_cols, 0, MAX_WRAP_COLS) != 0) {
                    fprintf(stderr, "Error: invalid wrap column value (must be 0-%d)\n", MAX_WRAP_COLS);
                    return 1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'v':
                print_version();
                return 0;
            default:
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return 1;
        }
    }
    
    // Get filename if provided
    if (optind < argc) {
        filename = argv[optind];
        
        // Check for too many arguments
        if (optind + 1 < argc) {
            fprintf(stderr, "Error: too many arguments\n");
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return 1;
        }
    }
    
    // Open input file (or use stdin)
    if (filename && strcmp(filename, "-") != 0) {
        // Validate filename length
        if (strlen(filename) > 4095) {
            fprintf(stderr, "Error: filename too long\n");
            return 1;
        }
        
        // Validate file before opening
        if (validate_file(filename) != 0) {
            return 2;
        }
        
        input = fopen(filename, "rb");
        if (!input) {
            fprintf(stderr, "Error opening input file '%s': %s\n", filename, strerror(errno));
            return 2;
        }
    }
    
    // Do the conversion
    int result;
    if (decode_mode) {
        result = decode_file(input, output);
    } else {
        result = encode_file(input, output, (int)wrap_cols);
    }
    
    if (result != 0) {
        exit_code = 3;
    }
    
    // Cleanup
    if (input != stdin && input != NULL) {
        if (fclose(input) != 0) {
            fprintf(stderr, "Warning: error closing input file\n");
        }
    }
    
    return exit_code;
}
