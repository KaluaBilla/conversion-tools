#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>

/*
    printf("DNA encoding maps each 2-bit pair to nucleotides A, T, G, C\n");
    printf("Default mapping: A=00, T=01, G=10, C=11 (can be customized with -m)\n");
    printf("Example mappings:\n");
    printf("  atgc: A=00, T=01, G=10, C=11 (default)\n");
    printf("  agct: A=00, G=01, C=10, T=11 (alternative)\n");
    printf("  cgat: C=00, G=01, A=10, T=11 (GC-rich)\n");
*/

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTION]... [FILE]\n", program_name);
    printf("DNA sequence encode or decode FILE, or standard input, to standard output.\n");
    printf("With no FILE, or when FILE is -, read standard input.\n\n");
    printf("  -d, --decode          decode DNA sequence to binary data\n");
    printf("  -m, --mapping=MAP     nucleotide mapping (default: 'atgc')\n");
    printf("                        MAP is 4 chars representing 00,01,10,11 bit pairs\n");
    printf("  -w, --wrap=COLS       wrap encoded lines after COLS characters (default 80)\n");
    printf("                        Use 0 to disable line wrapping\n");
    printf("  -c, --complement      use complementary base pairs for encoding\n");
    printf("      --help           display this help and exit\n");
    printf("      --version        output version information and exit\n\n");
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
    for (int i = 0; i < 4; i++) {
        if (toupper(mapping[i]) == upper_nuc) {
            return i;
        }
    }
    return -1;  // Invalid nucleotide
}

// Validate mapping string
int validate_mapping(const char *mapping) {
    if (strlen(mapping) != 4) {
        return 0;
    }
    
    // Check for valid nucleotides and no duplicates
    char seen[256] = {0};
    for (int i = 0; i < 4; i++) {
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

// Encode file to DNA sequence
void encode_dna(FILE *input, FILE *output, const char *mapping, int wrap_cols, int use_complement) {
    int byte;
    int col_count = 0;
    
    while ((byte = fgetc(input)) != EOF) {
        // Process each byte as 4 nucleotides (2 bits each)
        for (int i = 6; i >= 0; i -= 2) {
            unsigned char bits = (byte >> i) & 0x03;
            char nucleotide = bits_to_nucleotide(bits, mapping);
            
            if (use_complement) {
                nucleotide = get_complement(nucleotide);
            }
            
            fputc(nucleotide, output);
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

// Decode DNA sequence to file
void decode_dna(FILE *input, FILE *output, const char *mapping, int use_complement) {
    int c;
    unsigned char byte = 0;
    int nucleotide_count = 0;
    
    while ((c = fgetc(input)) != EOF) {
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
            fprintf(stderr, "Warning: ignoring invalid nucleotide '%c'\n", c);
            continue;
        }
        
        // Pack 2 bits into byte (4 nucleotides = 1 byte)
        byte = (byte << 2) | bits;
        nucleotide_count++;
        
        // When we have 4 nucleotides (8 bits), output the byte
        if (nucleotide_count == 4) {
            fputc(byte, output);
            byte = 0;
            nucleotide_count = 0;
        }
    }
    
    // Handle incomplete byte
    if (nucleotide_count > 0) {
        // Left-shift remaining bits to complete the byte
        byte <<= (2 * (4 - nucleotide_count));
        fputc(byte, output);
        fprintf(stderr, "Warning: incomplete DNA sequence, padded with zeros\n");
    }
}

// Display mapping information
void show_mapping_info(const char *mapping) {
    printf("DNA mapping being used:\n");
    const char *bit_patterns[] = {"00", "01", "10", "11"};
    for (int i = 0; i < 4; i++) {
        printf("  %s -> %c\n", bit_patterns[i], toupper(mapping[i]));
    }
    printf("\n");
}

// Calculate and display statistics
void calculate_gc_content(const char *sequence) {
    int total = 0, gc_count = 0;
    
    for (const char *p = sequence; *p; p++) {
        char c = toupper(*p);
        if (c == 'A' || c == 'T' || c == 'G' || c == 'C') {
            total++;
            if (c == 'G' || c == 'C') {
                gc_count++;
            }
        }
    }
    
    if (total > 0) {
        double gc_percent = (double)gc_count / total * 100.0;
        fprintf(stderr, "Sequence stats: %d nucleotides, %.1f%% GC content\n", total, gc_percent);
    }
}

int main(int argc, char *argv[]) {
    int decode_mode = 0;
    int wrap_cols = 80;  // Default wrap for DNA sequences
    int use_complement = 0;
    int show_stats = 0;
    char mapping[5] = "atgc";  // Default mapping: A=00, T=01, G=10, C=11
    const char *filename = NULL;
    FILE *input = stdin;
    FILE *output = stdout;
    
    // Parse command line options
    static struct option long_options[] = {
        {"decode", no_argument, 0, 'd'},
        {"mapping", required_argument, 0, 'm'},
        {"wrap", required_argument, 0, 'w'},
        {"complement", no_argument, 0, 'c'},
        {"stats", no_argument, 0, 's'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "dm:w:cshv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                decode_mode = 1;
                break;
            case 'm':
                if (!validate_mapping(optarg)) {
                    fprintf(stderr, "Error: invalid mapping '%s'. Must be 4 unique nucleotides (A,T,G,C)\n", optarg);
                    return 1;
                }
                strncpy(mapping, optarg, 4);
                mapping[4] = '\0';
                break;
            case 'w':
                wrap_cols = atoi(optarg);
                if (wrap_cols < 0) {
                    fprintf(stderr, "Error: wrap columns must be >= 0\n");
                    return 1;
                }
                break;
            case 'c':
                use_complement = 1;
                break;
            case 's':
                show_stats = 1;
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
    
    // Show mapping information if requested
    if (show_stats && !decode_mode) {
        show_mapping_info(mapping);
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
        decode_dna(input, output, mapping, use_complement);
    } else {
        encode_dna(input, output, mapping, wrap_cols, use_complement);
    }
    
    // Cleanup
    if (input != stdin) {
        fclose(input);
    }
    
    return 0;
}
