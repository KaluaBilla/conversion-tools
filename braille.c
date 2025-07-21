#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <wchar.h>
#include <locale.h>

// Braille Unicode characters start at U+2800
#define BRAILLE_BASE 0x2800

// Braille dot patterns (6-dot system)
// Dots are numbered: 1 4
//                   2 5  
//                   3 6
// Each bit represents a dot (bit 0 = dot 1, bit 1 = dot 2, etc.)
typedef struct {
    char character;
    unsigned char pattern;
} BrailleEntry;

// Standard Braille patterns (Grade 1 Braille)
static const BrailleEntry braille_table[] = {
    // Letters (a-z)
    {'A', 0x01}, {'B', 0x03}, {'C', 0x09}, {'D', 0x19}, {'E', 0x11}, {'F', 0x0B},
    {'G', 0x1B}, {'H', 0x13}, {'I', 0x0A}, {'J', 0x1A}, {'K', 0x05}, {'L', 0x07},
    {'M', 0x0D}, {'N', 0x1D}, {'O', 0x15}, {'P', 0x0F}, {'Q', 0x1F}, {'R', 0x17},
    {'S', 0x0E}, {'T', 0x1E}, {'U', 0x25}, {'V', 0x27}, {'W', 0x3A}, {'X', 0x2D},
    {'Y', 0x3D}, {'Z', 0x35},
    
    // Numbers (use number prefix # then letters a-j for 1-0)
    {'1', 0x01}, {'2', 0x03}, {'3', 0x09}, {'4', 0x19}, {'5', 0x11},
    {'6', 0x0B}, {'7', 0x1B}, {'8', 0x13}, {'9', 0x0A}, {'0', 0x1A},
    
    // Common punctuation
    {'.', 0x2C}, {',', 0x02}, {'?', 0x26}, {'!', 0x16}, {';', 0x06}, 
    {':', 0x12}, {'-', 0x24}, {'\'', 0x04}, {'"', 0x10}, {'(', 0x2E}, 
    {')', 0x2E}, {'/', 0x0C}, {' ', 0x00},  // Space is empty pattern
    
    {'\0', 0}  // End marker
};

// Special Braille indicators
#define BRAILLE_CAPITAL    0x20  // Capital letter indicator (dot 6)
#define BRAILLE_NUMBER     0x3C  // Number indicator (dots 3,4,5,6)

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTION]... [FILE]\n", program_name);
    printf("Braille encode or decode FILE, or standard input, to standard output.\n");
    printf("With no FILE, or when FILE is -, read standard input.\n\n");
    printf("  -d, --decode          decode braille (convert braille unicode to text)\n");
    printf("  -t, --text-braille    use text representation (dots/spaces) instead of unicode\n");
    printf("      --help           display this help and exit\n");
    printf("      --version        output version information and exit\n\n");
    printf("Encoding: Converts readable text to braille unicode characters\n");
    printf("Decoding: Converts braille unicode back to text\n");
    printf("Text mode: Uses 6-character patterns with 'o' for raised dots, '.' for empty\n");
    printf("Supported: A-Z, a-z, 0-9, and common punctuation marks\n");
}

void print_version() {
    printf("braille 1.0\n");
    printf("Simple braille encoder/decoder (Grade 1 Braille)\n");
}

// Find braille pattern for a character
unsigned char char_to_braille(char c) {
    char upper_c = toupper(c);
    for (int i = 0; braille_table[i].character != '\0'; i++) {
        if (braille_table[i].character == upper_c) {
            return braille_table[i].pattern;
        }
    }
    return 0xFF;  // Not found marker
}

// Find character for braille pattern
char braille_to_char(unsigned char pattern, int is_number, int is_capital) {
    for (int i = 0; braille_table[i].character != '\0'; i++) {
        if (braille_table[i].pattern == pattern) {
            char c = braille_table[i].character;
            if (is_number && c >= 'A' && c <= 'J') {
                // Convert A-J to 1-0 for numbers
                return (c == 'J') ? '0' : ('1' + (c - 'A'));
            }
            return is_capital ? c : tolower(c);
        }
    }
    return '?';  // Pattern not found
}

// Convert pattern to text representation (6 dots arranged in 2x3)
void pattern_to_text(unsigned char pattern, char *output) {
    // Arrangement: 1 4
    //              2 5
    //              3 6
    output[0] = (pattern & 0x01) ? 'o' : '.';  // dot 1
    output[1] = (pattern & 0x08) ? 'o' : '.';  // dot 4
    output[2] = (pattern & 0x02) ? 'o' : '.';  // dot 2
    output[3] = (pattern & 0x10) ? 'o' : '.';  // dot 5
    output[4] = (pattern & 0x04) ? 'o' : '.';  // dot 3
    output[5] = (pattern & 0x20) ? 'o' : '.';  // dot 6
    output[6] = '\0';
}

// Convert text representation back to pattern
unsigned char text_to_pattern(const char *text) {
    unsigned char pattern = 0;
    if (strlen(text) >= 6) {
        if (text[0] == 'o') pattern |= 0x01;  // dot 1
        if (text[1] == 'o') pattern |= 0x08;  // dot 4
        if (text[2] == 'o') pattern |= 0x02;  // dot 2
        if (text[3] == 'o') pattern |= 0x10;  // dot 5
        if (text[4] == 'o') pattern |= 0x04;  // dot 3
        if (text[5] == 'o') pattern |= 0x20;  // dot 6
    }
    return pattern;
}

// Encode text to braille
void encode_braille(FILE *input, FILE *output, int text_mode) {
    int c;
    int number_mode = 0;
    
    while ((c = fgetc(input)) != EOF) {
        if (c == '\n') {
            fprintf(output, "\n");
            number_mode = 0;
            continue;
        }
        
        unsigned char pattern = char_to_braille(c);
        
        if (pattern == 0xFF) {
            // Unsupported character
            fprintf(stderr, "Warning: skipping unsupported character '%c' (0x%02X)\n", 
                    isprint(c) ? c : '?', (unsigned char)c);
            continue;
        }
        
        // Handle numbers
        if (isdigit(c) && !number_mode) {
            // Need number indicator
            if (text_mode) {
                char text_repr[7];
                pattern_to_text(BRAILLE_NUMBER, text_repr);
                fprintf(output, "%s", text_repr);
            } else {
                fprintf(output, "%lc", (wint_t)(BRAILLE_BASE + BRAILLE_NUMBER));
            }
            number_mode = 1;
        } else if (!isdigit(c) && c != ' ') {
            number_mode = 0;
        }
        
        // Handle capital letters
        if (isupper(c) && isalpha(c)) {
            if (text_mode) {
                char text_repr[7];
                pattern_to_text(BRAILLE_CAPITAL, text_repr);
                fprintf(output, "%s", text_repr);
            } else {
                fprintf(output, "%lc", (wint_t)(BRAILLE_BASE + BRAILLE_CAPITAL));
            }
        }
        
        // Output the character pattern
        if (text_mode) {
            char text_repr[7];
            pattern_to_text(pattern, text_repr);
            fprintf(output, "%s", text_repr);
        } else {
            fprintf(output, "%lc", (wint_t)(BRAILLE_BASE + pattern));
        }
    }
    
    fprintf(output, "\n");
}

// Decode braille to text
void decode_braille(FILE *input, FILE *output, int text_mode) {
    if (text_mode) {
        // Text mode: read 6-character patterns
        char buffer[7];
        int pos = 0;
        int c;
        int number_mode = 0;
        int capital_next = 0;
        
        while ((c = fgetc(input)) != EOF) {
            if (c == '\n') {
                if (pos > 0) {
                    // Process remaining pattern
                    buffer[pos] = '\0';
                    if (pos == 6) {
                        unsigned char pattern = text_to_pattern(buffer);
                        char decoded = braille_to_char(pattern, number_mode, capital_next);
                        fputc(decoded, output);
                    }
                    pos = 0;
                }
                fprintf(output, "\n");
                number_mode = 0;
                capital_next = 0;
                continue;
            }
            
            if (c == 'o' || c == '.') {
                buffer[pos++] = c;
                
                if (pos == 6) {
                    buffer[6] = '\0';
                    unsigned char pattern = text_to_pattern(buffer);
                    
                    if (pattern == BRAILLE_NUMBER) {
                        number_mode = 1;
                    } else if (pattern == BRAILLE_CAPITAL) {
                        capital_next = 1;
                    } else {
                        char decoded = braille_to_char(pattern, number_mode, capital_next);
                        fputc(decoded, output);
                        if (!isdigit(decoded) && decoded != ' ') {
                            number_mode = 0;
                        }
                        capital_next = 0;
                    }
                    pos = 0;
                }
            }
        }
    } else {
        // Unicode mode: read braille unicode characters
        wint_t wc;
        int number_mode = 0;
        int capital_next = 0;
        
        while ((wc = fgetwc(input)) != WEOF) {
            if (wc == L'\n') {
                fprintf(output, "\n");
                number_mode = 0;
                capital_next = 0;
                continue;
            }
            
            if (wc >= BRAILLE_BASE && wc <= BRAILLE_BASE + 0x3F) {
                unsigned char pattern = wc - BRAILLE_BASE;
                
                if (pattern == BRAILLE_NUMBER) {
                    number_mode = 1;
                } else if (pattern == BRAILLE_CAPITAL) {
                    capital_next = 1;
                } else {
                    char decoded = braille_to_char(pattern, number_mode, capital_next);
                    fputc(decoded, output);
                    if (!isdigit(decoded) && decoded != ' ') {
                        number_mode = 0;
                    }
                    capital_next = 0;
                }
            }
        }
    }
    
    fprintf(output, "\n");
}

int main(int argc, char *argv[]) {
    int decode_mode = 0;
    int text_mode = 0;
    const char *filename = NULL;
    FILE *input = stdin;
    FILE *output = stdout;
    
    // Set locale for wide character support
    setlocale(LC_ALL, "");
    
    // Parse command line options
    static struct option long_options[] = {
        {"decode", no_argument, 0, 'd'},
        {"text-braille", no_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "dthv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                decode_mode = 1;
                break;
            case 't':
                text_mode = 1;
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
        input = fopen(filename, "r");
        if (!input) {
            perror("Error opening input file");
            return 1;
        }
    }
    
    // Do the conversion
    if (decode_mode) {
        decode_braille(input, output, text_mode);
    } else {
        encode_braille(input, output, text_mode);
    }
    
    // Cleanup
    if (input != stdin) {
        fclose(input);
    }
    
    return 0;
}
