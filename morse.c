#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>

// Morse code lookup table
typedef struct {
    char character;
    const char* morse;
} MorseEntry;

// Standard International Morse Code
static const MorseEntry morse_table[] = {
    // Letters
    {'A', ".-"}, {'B', "-..."}, {'C', "-.-."}, {'D', "-.."}, {'E', "."}, {'F', "..-."}, 
    {'G', "--."}, {'H', "...."}, {'I', ".."}, {'J', ".---"}, {'K', "-.-"}, {'L', ".-.."}, 
    {'M', "--"}, {'N', "-."}, {'O', "---"}, {'P', ".--."}, {'Q', "--.-"}, {'R', ".-."}, 
    {'S', "..."}, {'T', "-"}, {'U', "..-"}, {'V', "...-"}, {'W', ".--"}, {'X', "-..-"}, 
    {'Y', "-.--"}, {'Z', "--.."}, 
    // Numbers
    {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"}, {'4', "....-"}, 
    {'5', "....."}, {'6', "-...."}, {'7', "--..."}, {'8', "---.."}, {'9', "----."},
    // Punctuation
    {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."}, {'\'', ".----."}, {'!', "-.-.--"},
    {'/', "-..-."}, {'(', "-.--."}, {')', "-.--.-"}, {'&', ".-..."}, {':', "---..."},
    {';', "-.-.-."}, {'=', "-...-"}, {'+', ".-.-."}, {'-', "-....-"}, {'_', "..--.-"},
    {'"', ".-..-."}, {'$', "...-..-"}, {'@', ".--.-."}, {' ', "/"},  // Space as separator
    {'\0', NULL}  // End marker
};

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTION]... [FILE]\n", program_name);
    printf("Morse code encode or decode FILE, or standard input, to standard output.\n");
    printf("With no FILE, or when FILE is -, read standard input.\n\n");
    printf("  -d, --decode          decode morse code (convert morse to text)\n");
    printf("  -s, --separator=SEP   character separator for encoding (default: space)\n");
    printf("  -w, --word-sep=SEP    word separator for encoding (default: ' / ')\n");
    printf("      --help           display this help and exit\n");
    printf("      --version        output version information and exit\n\n");
    printf("Encoding: Converts readable text to morse code using dots and dashes\n");
    printf("Decoding: Converts morse code back to text (use spaces between letters, '/' between words)\n");
    printf("Supported: A-Z, 0-9, and common punctuation marks\n");
}

void print_version() {
    printf("morse 1.0\n");
    printf("Simple morse code encoder/decoder\n");
}

// Find morse code for a character
const char* char_to_morse(char c) {
    c = toupper(c);  // Convert to uppercase
    for (int i = 0; morse_table[i].character != '\0'; i++) {
        if (morse_table[i].character == c) {
            return morse_table[i].morse;
        }
    }
    return NULL;  // Character not found
}

// Find character for morse code
char morse_to_char(const char* morse) {
    for (int i = 0; morse_table[i].character != '\0'; i++) {
        if (strcmp(morse_table[i].morse, morse) == 0) {
            return morse_table[i].character;
        }
    }
    return '?';  // Morse not found
}

// Encode text to morse code
void encode_morse(FILE *input, FILE *output, const char *char_sep, const char *word_sep) {
    int c;
    int first_char = 1;
    int word_started = 0;
    
    while ((c = fgetc(input)) != EOF) {
        if (c == '\n') {
            fprintf(output, "\n");
            first_char = 1;
            word_started = 0;
            continue;
        }
        
        const char* morse = char_to_morse(c);
        if (morse) {
            if (c == ' ') {
                // Space becomes word separator
                if (word_started) {
                    fprintf(output, "%s", word_sep);
                    word_started = 0;
                }
                first_char = 1;
            } else {
                // Regular character
                if (!first_char) {
                    fprintf(output, "%s", char_sep);
                }
                fprintf(output, "%s", morse);
                first_char = 0;
                word_started = 1;
            }
        } else {
            // Unsupported character - skip with warning to stderr
            fprintf(stderr, "Warning: skipping unsupported character '%c' (0x%02X)\n", 
                    isprint(c) ? c : '?', (unsigned char)c);
        }
    }
    
    fprintf(output, "\n");
}

// Decode morse code to text
void decode_morse(FILE *input, FILE *output) {
    char morse_buffer[10] = {0};  // Buffer for morse code sequence
    int buffer_pos = 0;
    int c;
    
    while ((c = fgetc(input)) != EOF) {
        if (c == ' ' || c == '\t') {
            // Space separates morse characters
            if (buffer_pos > 0) {
                morse_buffer[buffer_pos] = '\0';
                char decoded = morse_to_char(morse_buffer);
                fputc(decoded, output);
                buffer_pos = 0;
            }
        } else if (c == '/' || c == '\n') {
            // Forward slash or newline separates words
            if (buffer_pos > 0) {
                morse_buffer[buffer_pos] = '\0';
                char decoded = morse_to_char(morse_buffer);
                fputc(decoded, output);
                buffer_pos = 0;
            }
            if (c == '/') {
                fputc(' ', output);  // Word separator becomes space
            } else {
                fputc('\n', output); // Preserve newlines
            }
        } else if (c == '.' || c == '-') {
            // Morse code characters
            if (buffer_pos < sizeof(morse_buffer) - 1) {
                morse_buffer[buffer_pos++] = c;
            } else {
                fprintf(stderr, "Warning: morse sequence too long, truncating\n");
            }
        }
        // Ignore other characters
    }
    
    // Handle any remaining morse in buffer
    if (buffer_pos > 0) {
        morse_buffer[buffer_pos] = '\0';
        char decoded = morse_to_char(morse_buffer);
        fputc(decoded, output);
    }
    
    fprintf(output, "\n");
}

int main(int argc, char *argv[]) {
    int decode_mode = 0;
    const char *char_separator = " ";      // Default separator between characters
    const char *word_separator = " / ";    // Default separator between words
    const char *filename = NULL;
    FILE *input = stdin;
    FILE *output = stdout;
    
    // Parse command line options
    static struct option long_options[] = {
        {"decode", no_argument, 0, 'd'},
        {"separator", required_argument, 0, 's'},
        {"word-sep", required_argument, 0, 'w'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "ds:w:hv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                decode_mode = 1;
                break;
            case 's':
                char_separator = optarg;
                break;
            case 'w':
                word_separator = optarg;
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
        decode_morse(input, output);
    } else {
        encode_morse(input, output, char_separator, word_separator);
    }
    
    // Cleanup
    if (input != stdin) {
        fclose(input);
    }
    
    return 0;
}
