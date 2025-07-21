#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

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

void print_version() {
    printf("binary 1.0\n");
    printf("Simple binary encoder/decoder\n");
}

// Convert file to binary text (encode)
void encode_file(FILE *input, FILE *output, int wrap_cols) {
    int byte;
    int col_count = 0;
    
    while ((byte = fgetc(input)) != EOF) {
        // Convert each bit of the byte to '0' or '1'
        for (int i = 7; i >= 0; i--) {
            fputc(((byte >> i) & 1) ? '1' : '0', output);
            col_count++;
            
            // Add line wrap if specified
            if (wrap_cols > 0 && col_count >= wrap_cols) {
                fputc('\n', output);
                col_count = 0;
            }
        }
    }
    
    // Add final newline if we haven't wrapped
    if (wrap_cols == 0 || col_count > 0) {
        fputc('\n', output);
    }
}

// Convert binary text to file (decode)
void decode_file(FILE *input, FILE *output) {
    int bit_char;
    unsigned char byte = 0;
    int bit_count = 0;
    
    while ((bit_char = fgetc(input)) != EOF) {
        // Skip non-binary characters (whitespace, etc.)
        if (bit_char != '0' && bit_char != '1') {
            continue;
        }
        
        // Build byte from bits
        byte = (byte << 1) | (bit_char - '0');
        bit_count++;
        
        // When we have 8 bits, output the byte
        if (bit_count == 8) {
            fputc(byte, output);
            byte = 0;
            bit_count = 0;
        }
    }
    
    // Warn if incomplete byte (not divisible by 8)
    if (bit_count != 0) {
        fprintf(stderr, "Warning: incomplete byte at end of input\n");
    }
}

int main(int argc, char *argv[]) {
    int decode_mode = 0;
    int wrap_cols = 64;  // Default wrap at 64 characters
    const char *filename = NULL;
    FILE *input = stdin;
    FILE *output = stdout;
    
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
                wrap_cols = atoi(optarg);
                if (wrap_cols < 0) {
                    fprintf(stderr, "Error: wrap columns must be >= 0\n");
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
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Get filename if provided
    if (optind < argc) {
        filename = argv[optind];
    }
    
    // Open input file (or use stdin)
    if (filename && strcmp(filename, "-") != 0) {
        input = fopen(filename, "rb");
        if (!input) {
            perror("Error opening input file");
            return 1;
        }
    }
    
    // Do the conversion
    if (decode_mode) {
        decode_file(input, output);
    } else {
        encode_file(input, output, wrap_cols);
    }
    
    // Cleanup
    if (input != stdin) {
        fclose(input);
    }
    
    return 0;
}
