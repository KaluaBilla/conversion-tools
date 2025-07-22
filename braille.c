#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <wchar.h>
#include <locale.h>
#include <errno.h>
#include <limits.h>

#define BRAILLE_BASE 0x2800
#define BRAILLE_CAPITAL 0x20
#define BRAILLE_NUMBER 0x3C
#define MAX_LINE_LENGTH 8192
#define PATTERN_LENGTH 6

typedef struct {
    char character;
    unsigned char pattern;
} BrailleEntry;

static const BrailleEntry braille_table[] = {
    {'A', 0x01}, {'B', 0x03}, {'C', 0x09}, {'D', 0x19}, {'E', 0x11}, {'F', 0x0B},
    {'G', 0x1B}, {'H', 0x13}, {'I', 0x0A}, {'J', 0x1A}, {'K', 0x05}, {'L', 0x07},
    {'M', 0x0D}, {'N', 0x1D}, {'O', 0x15}, {'P', 0x0F}, {'Q', 0x1F}, {'R', 0x17},
    {'S', 0x0E}, {'T', 0x1E}, {'U', 0x25}, {'V', 0x27}, {'W', 0x3A}, {'X', 0x2D},
    {'Y', 0x3D}, {'Z', 0x35},
    {'1', 0x01}, {'2', 0x03}, {'3', 0x09}, {'4', 0x19}, {'5', 0x11},
    {'6', 0x0B}, {'7', 0x1B}, {'8', 0x13}, {'9', 0x0A}, {'0', 0x1A},
    {'.', 0x2C}, {',', 0x02}, {'?', 0x26}, {'!', 0x16}, {';', 0x06}, 
    {':', 0x12}, {'-', 0x24}, {'\'', 0x04}, {'"', 0x10}, {'(', 0x2E}, 
    {')', 0x2E}, {'/', 0x0C}, {' ', 0x00},
    {'\0', 0}
};

typedef enum {
    RESULT_SUCCESS = 0,
    RESULT_ERROR_FILE,
    RESULT_ERROR_MEMORY,
    RESULT_ERROR_ARGS,
    RESULT_ERROR_IO,
    RESULT_ERROR_ENCODING
} result_t;

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTION]... [FILE]\n", program_name);
    printf("Braille encode or decode FILE, or standard input, to standard output.\n");
    printf("With no FILE, or when FILE is -, read standard input.\n\n");
    printf("  -d, --decode          decode braille (convert braille unicode to text)\n");
    printf("  -t, --text-braille    use text representation (dots/spaces) instead of unicode\n");
    printf("      --help           display this help and exit\n");
    printf("      --version        output version information and exit\n\n");
}

void print_version(void) {
    printf("braille 1.0\n");
    printf("Braille encoder/decoder (Grade 1 Braille)\n");
}

static unsigned char char_to_braille(char c) {
    char upper_c = (char)toupper((unsigned char)c);
    for (size_t i = 0; braille_table[i].character != '\0'; i++) {
        if (braille_table[i].character == upper_c) {
            return braille_table[i].pattern;
        }
    }
    return 0xFF;
}

static char braille_to_char(unsigned char pattern, int is_number, int is_capital) {
    for (size_t i = 0; braille_table[i].character != '\0'; i++) {
        if (braille_table[i].pattern == pattern) {
            char c = braille_table[i].character;
            if (is_number && c >= 'A' && c <= 'J') {
                return (c == 'J') ? '0' : ('1' + (c - 'A'));
            }
            return is_capital ? c : (char)tolower((unsigned char)c);
        }
    }
    return '?';
}

static result_t pattern_to_text(unsigned char pattern, char *output, size_t output_size) {
    if (output == NULL || output_size < PATTERN_LENGTH + 1) {
        return RESULT_ERROR_ARGS;
    }
    
    output[0] = (pattern & 0x01) ? 'o' : '.';
    output[1] = (pattern & 0x08) ? 'o' : '.';
    output[2] = (pattern & 0x02) ? 'o' : '.';
    output[3] = (pattern & 0x10) ? 'o' : '.';
    output[4] = (pattern & 0x04) ? 'o' : '.';
    output[5] = (pattern & 0x20) ? 'o' : '.';
    output[6] = '\0';
    
    return RESULT_SUCCESS;
}

static unsigned char text_to_pattern(const char *text) {
    if (text == NULL || strlen(text) < PATTERN_LENGTH) {
        return 0;
    }
    
    unsigned char pattern = 0;
    if (text[0] == 'o') pattern |= 0x01;
    if (text[1] == 'o') pattern |= 0x08;
    if (text[2] == 'o') pattern |= 0x02;
    if (text[3] == 'o') pattern |= 0x10;
    if (text[4] == 'o') pattern |= 0x04;
    if (text[5] == 'o') pattern |= 0x20;
    
    return pattern;
}

static result_t write_pattern(FILE *output, unsigned char pattern, int text_mode) {
    if (output == NULL) {
        return RESULT_ERROR_ARGS;
    }
    
    if (text_mode) {
        char text_repr[PATTERN_LENGTH + 1];
        result_t result = pattern_to_text(pattern, text_repr, sizeof(text_repr));
        if (result != RESULT_SUCCESS) {
            return result;
        }
        if (fprintf(output, "%s", text_repr) < 0) {
            return RESULT_ERROR_IO;
        }
    } else {
        wint_t braille_char = BRAILLE_BASE + pattern;
        if (fputwc(braille_char, output) == WEOF) {
            return RESULT_ERROR_IO;
        }
    }
    
    return RESULT_SUCCESS;
}

static result_t encode_braille(FILE *input, FILE *output, int text_mode) {
    if (input == NULL || output == NULL) {
        return RESULT_ERROR_ARGS;
    }
    
    int c;
    int number_mode = 0;
    size_t line_length = 0;
    
    while ((c = fgetc(input)) != EOF) {
        if (ferror(input)) {
            fprintf(stderr, "Error reading input\n");
            return RESULT_ERROR_IO;
        }
        
        if (c == '\n') {
            if (fputc('\n', output) == EOF) {
                return RESULT_ERROR_IO;
            }
            number_mode = 0;
            line_length = 0;
            continue;
        }
        
        // Prevent extremely long lines
        if (line_length > MAX_LINE_LENGTH) {
            fprintf(stderr, "Warning: line too long, truncating\n");
            continue;
        }
        
        unsigned char pattern = char_to_braille((char)c);
        if (pattern == 0xFF) {
            if (isprint(c)) {
                fprintf(stderr, "Warning: skipping unsupported character '%c'\n", c);
            } else {
                fprintf(stderr, "Warning: skipping unsupported character (0x%02X)\n", (unsigned char)c);
            }
            continue;
        }
        
        // Handle numbers
        if (isdigit(c) && !number_mode) {
            result_t result = write_pattern(output, BRAILLE_NUMBER, text_mode);
            if (result != RESULT_SUCCESS) {
                return result;
            }
            number_mode = 1;
            line_length++;
        } else if (!isdigit(c) && c != ' ') {
            number_mode = 0;
        }
        
        // Handle capital letters
        if (isupper(c) && isalpha(c)) {
            result_t result = write_pattern(output, BRAILLE_CAPITAL, text_mode);
            if (result != RESULT_SUCCESS) {
                return result;
            }
            line_length++;
        }
        
        // Write character pattern
        result_t result = write_pattern(output, pattern, text_mode);
        if (result != RESULT_SUCCESS) {
            return result;
        }
        line_length++;
    }
    
    if (fputc('\n', output) == EOF) {
        return RESULT_ERROR_IO;
    }
    
    return RESULT_SUCCESS;
}

static result_t decode_braille(FILE *input, FILE *output, int text_mode) {
    if (input == NULL || output == NULL) {
        return RESULT_ERROR_ARGS;
    }
    
    if (text_mode) {
        char buffer[PATTERN_LENGTH + 1];
        int pos = 0;
        int c;
        int number_mode = 0;
        int capital_next = 0;
        size_t line_length = 0;
        
        while ((c = fgetc(input)) != EOF) {
            if (ferror(input)) {
                fprintf(stderr, "Error reading input\n");
                return RESULT_ERROR_IO;
            }
            
            if (c == '\n') {
                if (pos > 0 && pos == PATTERN_LENGTH) {
                    buffer[PATTERN_LENGTH] = '\0';
                    unsigned char pattern = text_to_pattern(buffer);
                    char decoded = braille_to_char(pattern, number_mode, capital_next);
                    if (fputc(decoded, output) == EOF) {
                        return RESULT_ERROR_IO;
                    }
                }
                if (fputc('\n', output) == EOF) {
                    return RESULT_ERROR_IO;
                }
                pos = 0;
                number_mode = 0;
                capital_next = 0;
                line_length = 0;
                continue;
            }
            
            if (line_length > MAX_LINE_LENGTH) {
                fprintf(stderr, "Warning: line too long, truncating\n");
                continue;
            }
            
            if (c == 'o' || c == '.') {
                if (pos < PATTERN_LENGTH) {
                    buffer[pos++] = (char)c;
                }
                
                if (pos == PATTERN_LENGTH) {
                    buffer[PATTERN_LENGTH] = '\0';
                    unsigned char pattern = text_to_pattern(buffer);
                    
                    if (pattern == BRAILLE_NUMBER) {
                        number_mode = 1;
                    } else if (pattern == BRAILLE_CAPITAL) {
                        capital_next = 1;
                    } else {
                        char decoded = braille_to_char(pattern, number_mode, capital_next);
                        if (fputc(decoded, output) == EOF) {
                            return RESULT_ERROR_IO;
                        }
                        if (!isdigit(decoded) && decoded != ' ') {
                            number_mode = 0;
                        }
                        capital_next = 0;
                    }
                    pos = 0;
                }
                line_length++;
            }
        }
    } else {
        wint_t wc;
        int number_mode = 0;
        int capital_next = 0;
        size_t line_length = 0;
        
        while ((wc = fgetwc(input)) != WEOF) {
            if (ferror(input)) {
                fprintf(stderr, "Error reading input\n");
                return RESULT_ERROR_IO;
            }
            
            if (wc == L'\n') {
                if (fputc('\n', output) == EOF) {
                    return RESULT_ERROR_IO;
                }
                number_mode = 0;
                capital_next = 0;
                line_length = 0;
                continue;
            }
            
            if (line_length > MAX_LINE_LENGTH) {
                fprintf(stderr, "Warning: line too long, truncating\n");
                continue;
            }
            
            if (wc >= BRAILLE_BASE && wc <= BRAILLE_BASE + 0x3F) {
                unsigned char pattern = (unsigned char)(wc - BRAILLE_BASE);
                
                if (pattern == BRAILLE_NUMBER) {
                    number_mode = 1;
                } else if (pattern == BRAILLE_CAPITAL) {
                    capital_next = 1;
                } else {
                    char decoded = braille_to_char(pattern, number_mode, capital_next);
                    if (fputc(decoded, output) == EOF) {
                        return RESULT_ERROR_IO;
                    }
                    if (!isdigit(decoded) && decoded != ' ') {
                        number_mode = 0;
                    }
                    capital_next = 0;
                }
                line_length++;
            }
        }
    }
    
    if (fputc('\n', output) == EOF) {
        return RESULT_ERROR_IO;
    }
    
    return RESULT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc == 0 || argv == NULL) {
        return RESULT_ERROR_ARGS;
    }
    
    int decode_mode = 0;
    int text_mode = 0;
    const char *filename = NULL;
    FILE *input = NULL;
    FILE *output = stdout;
    result_t result = RESULT_SUCCESS;
    
    if (setlocale(LC_ALL, "") == NULL) {
        fprintf(stderr, "Warning: could not set locale\n");
    }
    
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
                return RESULT_SUCCESS;
            case 'v':
                print_version();
                return RESULT_SUCCESS;
            default:
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return RESULT_ERROR_ARGS;
        }
    }
    
    if (optind < argc) {
        filename = argv[optind];
        if (optind + 1 < argc) {
            fprintf(stderr, "Error: too many arguments\n");
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return RESULT_ERROR_ARGS;
        }
    }
    
    // Open input file
    if (filename == NULL || strcmp(filename, "-") == 0) {
        input = stdin;
    } else {
        input = fopen(filename, "r");
        if (input == NULL) {
            fprintf(stderr, "Error opening '%s': %s\n", filename, strerror(errno));
            return RESULT_ERROR_FILE;
        }
    }
    
    // Process file
    if (decode_mode) {
        result = decode_braille(input, output, text_mode);
    } else {
        result = encode_braille(input, output, text_mode);
    }
    
    // Cleanup
    if (input != NULL && input != stdin) {
        if (fclose(input) != 0) {
            fprintf(stderr, "Warning: error closing input file: %s\n", strerror(errno));
            if (result == RESULT_SUCCESS) {
                result = RESULT_ERROR_FILE;
            }
        }
    }
    
    if (fflush(output) != 0) {
        fprintf(stderr, "Error writing output: %s\n", strerror(errno));
        if (result == RESULT_SUCCESS) {
            result = RESULT_ERROR_IO;
        }
    }
    
    return (int)result;
}
