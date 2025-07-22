#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#define NUCLEOTIDES_PER_BYTE 4
#define BITS_PER_NUCLEOTIDE 2
#define MAX_WRAP_COLS 10000
#define BUFFER_SIZE 8192
#define MAX_MAPPING_LEN 4

// Exit codes
#define EXIT_SUCCESS 0
#define EXIT_INVALID_ARGS 1
#define EXIT_FILE_ERROR 2
#define EXIT_INVALID_DATA 3

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTION]... [FILE]\n", program_name);
    printf("DNA sequence encode or decode FILE, or standard input, to standard output.\n");
    printf("With no FILE, or when FILE is -, read standard input.\n\n");
    printf("  -d, --decode          decode DNA sequence to binary data\n");
    printf("  -m, --mapping=MAP     nucleotide mapping (default: 'atgc')\n");
    printf("                        MAP is 4 chars representing 00,01,10,11 bit pairs\n");
    printf("  -w, --wrap=COLS       wrap encoded lines after COLS characters (default 80)\n");
    printf("                        Use 0 to disable line wrapping (max %d)\n", MAX_WRAP_COLS);
    printf("  -c, --complement      use complementary base pairs for encoding\n");
    printf("      --help           display this help and exit\n");
    printf("      --version        output version information and exit\n\n");
    printf("DNA encoding maps each 2-bit pair to nucleotides A, T, G, C\n");
    printf("Default mapping: A=00, T=01, G=10, C=11 (can be customized with -m)\n");
}

void print_version() {
    printf("dna 1.0\n");
    printf("DNA sequence encoder/decoder (2 bits per nucleotide)\n");
}

// DNA complement mapping
char get_complement(char base) {
    switch (toupper(base)) {
        case 'A': return 'T';
        case 'T': return 'A';
        case 'G': return 'C';
        case 'C': return 'G';
        default: return base;
    }
}

// Convert 2-bit value to nucleotide using mapping
char bits_to_nucleotide(unsigned char bits, const char *mapping) {
    return toupper(mapping[bits & 0x03]);
}

// Convert nucleotide to 2-bit value using mapping
int nucleotide_to_bits(char nucleotide, const char *mapping) {
    char upper_nuc = toupper(nucleotide);
    for (int i = 0; i < NUCLEOTIDES_PER_BYTE; i++) {
        if (toupper(mapping[i]) == upper_nuc) {
            return i;
        }
    }
    return -1;  // Invalid nucleotide
}

// Validate mapping string
int validate_mapping(const char *mapping) {
    if (!mapping || strlen(mapping) != MAX_MAPPING_LEN) {
        return 0;
    }
    
    // Check for valid nucleotides and no duplicates
    int seen[256] = {0};
    for (int i = 0; i < MAX_MAPPING_LEN; i++) {
        char c = toupper(mapping[i]);
        if (c != 'A' && c != 'T' && c != 'G' && c != 'C') {
            return 0;
        }
        if (seen[(unsigned char)c]) {
            return 0;  // Duplicate
        }
        seen[(unsigned char)c] = 1;
    }
    return 1;
}

// Safe integer parsing
int parse_int(const char *str, int *result, int min_val, int max_val) {
    if (!str || !result) return 0;
    
    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    
    if (errno != 0 || *endptr != '\0' || val < min_val || val > max_val) {
        return 0;
    }
    
    *result = (int)val;
    return 1;
}

// Check for write errors
int safe_fputc(int c, FILE *stream) {
    if (fputc(c, stream) == EOF) {
        fprintf(stderr, "Error: write failed: %s\n", strerror(errno));
        return 0;
    }
    return 1;
}

// Encode file to DNA sequence with buffering
int encode_dna(FILE *input, FILE *output, const char *mapping, int wrap_cols, int use_complement) {
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    int col_count = 0;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            unsigned char byte = buffer[i];
            
            // Process each byte as 4 nucleotides (2 bits each)
            for (int shift = 6; shift >= 0; shift -= BITS_PER_NUCLEOTIDE) {
                unsigned char bits = (byte >> shift) & 0x03;
                char nucleotide = bits_to_nucleotide(bits, mapping);
                
                if (use_complement) {
                    nucleotide = get_complement(nucleotide);
                }
                
                if (!safe_fputc(nucleotide, output)) {
                    return EXIT_FILE_ERROR;
                }
                col_count++;
                
                // Add line wrap if specified
                if (wrap_cols > 0 && col_count >= wrap_cols) {
                    if (!safe_fputc('\n', output)) {
                        return EXIT_FILE_ERROR;
                    }
                    col_count = 0;
                }
            }
        }
    }
    
    // Check for read error
    if (ferror(input)) {
        fprintf(stderr, "Error: read failed: %s\n", strerror(errno));
        return EXIT_FILE_ERROR;
    }
    
    // Add final newline if we haven't wrapped
    if (wrap_cols == 0 || col_count > 0) {
        if (!safe_fputc('\n', output)) {
            return EXIT_FILE_ERROR;
        }
    }
    
    return EXIT_SUCCESS;
}

// Decode DNA sequence to file with buffering
int decode_dna(FILE *input, FILE *output, const char *mapping, int use_complement) {
    char buffer[BUFFER_SIZE];
    size_t chars_read;
    unsigned char byte = 0;
    int nucleotide_count = 0;
    int invalid_chars = 0;
    
    while ((chars_read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        for (size_t i = 0; i < chars_read; i++) {
            char c = buffer[i];
            
            // Skip whitespace and newlines
            if (isspace(c)) {
                continue;
            }
            
            char nucleotide = c;
            if (use_complement) {
                nucleotide = get_complement(nucleotide);
            }
            
            int bits = nucleotide_to_bits(nucleotide, mapping);
            if (bits < 0) {
                invalid_chars++;
                if (invalid_chars <= 10) {  // Limit error messages
                    fprintf(stderr, "Warning: ignoring invalid nucleotide '%c' at position %zu\n", 
                           c, ftell(input) - chars_read + i);
                }
                continue;
            }
            
            // Pack 2 bits into byte (4 nucleotides = 1 byte)
            byte = (byte << BITS_PER_NUCLEOTIDE) | bits;
            nucleotide_count++;
            
            // When we have 4 nucleotides (8 bits), output the byte
            if (nucleotide_count == NUCLEOTIDES_PER_BYTE) {
                if (fputc(byte, output) == EOF) {
                    fprintf(stderr, "Error: write failed: %s\n", strerror(errno));
                    return EXIT_FILE_ERROR;
                }
                byte = 0;
                nucleotide_count = 0;
            }
        }
    }
    
    // Check for read error
    if (ferror(input)) {
        fprintf(stderr, "Error: read failed: %s\n", strerror(errno));
        return EXIT_FILE_ERROR;
    }
    
    // Handle incomplete byte
    if (nucleotide_count > 0) {
        // Left-shift remaining bits to complete the byte
        byte <<= (BITS_PER_NUCLEOTIDE * (NUCLEOTIDES_PER_BYTE - nucleotide_count));
        if (fputc(byte, output) == EOF) {
            fprintf(stderr, "Error: write failed: %s\n", strerror(errno));
            return EXIT_FILE_ERROR;
        }
        fprintf(stderr, "Warning: incomplete DNA sequence (%d nucleotides), padded with zeros\n", 
               nucleotide_count);
    }
    
    if (invalid_chars > 10) {
        fprintf(stderr, "Warning: %d total invalid characters ignored\n", invalid_chars);
    }
    
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    int decode_mode = 0;
    int wrap_cols = 80;  // Default wrap for DNA sequences
    int use_complement = 0;
    char mapping[MAX_MAPPING_LEN + 1] = "atgc";  // Default mapping: A=00, T=01, G=10, C=11
    const char *filename = NULL;
    FILE *input = stdin;
    FILE *output = stdout;
    int result = EXIT_SUCCESS;
    
    // Parse command line options
    static struct option long_options[] = {
        {"decode", no_argument, 0, 'd'},
        {"mapping", required_argument, 0, 'm'},
        {"wrap", required_argument, 0, 'w'},
        {"complement", no_argument, 0, 'c'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "dm:w:chv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                decode_mode = 1;
                break;
            case 'm':
                if (!validate_mapping(optarg)) {
                    fprintf(stderr, "Error: invalid mapping '%s'. Must be 4 unique nucleotides (A,T,G,C)\n", optarg);
                    return EXIT_INVALID_ARGS;
                }
                strncpy(mapping, optarg, MAX_MAPPING_LEN);
                mapping[MAX_MAPPING_LEN] = '\0';
                break;
            case 'w':
                if (!parse_int(optarg, &wrap_cols, 0, MAX_WRAP_COLS)) {
                    fprintf(stderr, "Error: invalid wrap columns '%s'. Must be 0-%d\n", 
                           optarg, MAX_WRAP_COLS);
                    return EXIT_INVALID_ARGS;
                }
                break;
            case 'c':
                use_complement = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'v':
                print_version();
                return EXIT_SUCCESS;
            default:
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return EXIT_INVALID_ARGS;
        }
    }
    
    // Get filename if provided
    if (optind < argc) {
        filename = argv[optind];
        if (optind + 1 < argc) {
            fprintf(stderr, "Error: too many arguments\n");
            return EXIT_INVALID_ARGS;
        }
    }
    
    // Open input file (or use stdin)
    if (filename && strcmp(filename, "-") != 0) {
        input = fopen(filename, "rb");
        if (!input) {
            fprintf(stderr, "Error: cannot open '%s': %s\n", filename, strerror(errno));
            return EXIT_FILE_ERROR;
        }
    }
    
    // Do the conversion
    if (decode_mode) {
        result = decode_dna(input, output, mapping, use_complement);
    } else {
        result = encode_dna(input, output, mapping, wrap_cols, use_complement);
    }
    
    // Ensure output is flushed
    if (fflush(output) != 0) {
        fprintf(stderr, "Error: failed to flush output: %s\n", strerror(errno));
        result = EXIT_FILE_ERROR;
    }
    
    // Cleanup
    if (input != stdin && fclose(input) != 0) {
        fprintf(stderr, "Warning: failed to close input file: %s\n", strerror(errno));
    }
    
    return result;
}
