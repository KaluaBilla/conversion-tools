#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <errno.h>

#define BUFFER_SIZE 8192
#define MAX_MORSE_LENGTH 10
#define MAX_SEPARATOR_LENGTH 10

// Exit codes
#define EXIT_SUCCESS 0
#define EXIT_INVALID_ARGS 1
#define EXIT_FILE_ERROR 2
#define EXIT_INVALID_DATA 3

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

// Validate separator string
int validate_separator(const char *sep, const char *name) {
    if (!sep) {
        fprintf(stderr, "Error: %s cannot be null\n", name);
        return 0;
    }
    if (strlen(sep) > MAX_SEPARATOR_LENGTH) {
        fprintf(stderr, "Error: %s too long (max %d chars)\n", name, MAX_SEPARATOR_LENGTH);
        return 0;
    }
    return 1;
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
    if (!morse) return '?';
    
    for (int i = 0; morse_table[i].character != '\0'; i++) {
        if (strcmp(morse_table[i].morse, morse) == 0) {
            return morse_table[i].character;
        }
    }
    return '?';  // Morse not found
}

// Safe output functions
int safe_fputs(const char *str, FILE *stream) {
    if (fputs(str, stream) == EOF) {
        fprintf(stderr, "Error: write failed: %s\n", strerror(errno));
        return 0;
    }
    return 1;
}

int safe_fputc(int c, FILE *stream) {
    if (fputc(c, stream) == EOF) {
        fprintf(stderr, "Error: write failed: %s\n", strerror(errno));
        return 0;
    }
    return 1;
}

// Encode text to morse code with buffering
int encode_morse(FILE *input, FILE *output, const char *char_sep, const char *word_sep) {
    char buffer[BUFFER_SIZE];
    size_t chars_read;
    int first_char = 1;
    int word_started = 0;
    int unsupported_count = 0;
    
    while ((chars_read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        for (size_t i = 0; i < chars_read; i++) {
            char c = buffer[i];
            
            if (c == '\n') {
                if (!safe_fputc('\n', output)) {
                    return EXIT_FILE_ERROR;
                }
                first_char = 1;
                word_started = 0;
                continue;
            }
            
            const char* morse = char_to_morse(c);
            if (morse) {
                if (c == ' ') {
                    // Space becomes word separator
                    if (word_started) {
                        if (!safe_fputs(word_sep, output)) {
                            return EXIT_FILE_ERROR;
                        }
                        word_started = 0;
                    }
                    first_char = 1;
                } else {
                    // Regular character
                    if (!first_char) {
                        if (!safe_fputs(char_sep, output)) {
                            return EXIT_FILE_ERROR;
                        }
                    }
                    if (!safe_fputs(morse, output)) {
                        return EXIT_FILE_ERROR;
                    }
                    first_char = 0;
                    word_started = 1;
                }
            } else {
                // Unsupported character - skip with warning to stderr
                unsupported_count++;
                if (unsupported_count <= 10) {  // Limit error messages
                    fprintf(stderr, "Warning: skipping unsupported character '%c' (0x%02X)\n", 
                            isprint(c) ? c : '?', (unsigned char)c);
                }
            }
        }
    }
    
    // Check for read error
    if (ferror(input)) {
        fprintf(stderr, "Error: read failed: %s\n", strerror(errno));
        return EXIT_FILE_ERROR;
    }
    
    if (unsupported_count > 10) {
        fprintf(stderr, "Warning: %d total unsupported characters skipped\n", unsupported_count);
    }
    
    if (!safe_fputc('\n', output)) {
        return EXIT_FILE_ERROR;
    }
    
    return EXIT_SUCCESS;
}

// Decode morse code to text with buffering
int decode_morse(FILE *input, FILE *output) {
    char buffer[BUFFER_SIZE];
    char morse_buffer[MAX_MORSE_LENGTH + 1] = {0};
    size_t chars_read;
    int buffer_pos = 0;
    int invalid_sequences = 0;
    
    while ((chars_read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        for (size_t i = 0; i < chars_read; i++) {
            char c = buffer[i];
            
            if (c == ' ' || c == '\t') {
                // Space separates morse characters
                if (buffer_pos > 0) {
                    morse_buffer[buffer_pos] = '\0';
                    char decoded = morse_to_char(morse_buffer);
                    
                    if (decoded == '?' && strcmp(morse_buffer, "/") != 0) {
                        invalid_sequences++;
                        if (invalid_sequences <= 10) {
                            fprintf(stderr, "Warning: unknown morse sequence '%s'\n", morse_buffer);
                        }
                    }
                    
                    if (!safe_fputc(decoded, output)) {
                        return EXIT_FILE_ERROR;
                    }
                    buffer_pos = 0;
                }
            } else if (c == '/' || c == '\n') {
                // Forward slash or newline separates words
                if (buffer_pos > 0) {
                    morse_buffer[buffer_pos] = '\0';
                    char decoded = morse_to_char(morse_buffer);
                    
                    if (decoded == '?' && strcmp(morse_buffer, "/") != 0) {
                        invalid_sequences++;
                        if (invalid_sequences <= 10) {
                            fprintf(stderr, "Warning: unknown morse sequence '%s'\n", morse_buffer);
                        }
                    }
                    
                    if (!safe_fputc(decoded, output)) {
                        return EXIT_FILE_ERROR;
                    }
                    buffer_pos = 0;
                }
                if (c == '/') {
                    if (!safe_fputc(' ', output)) {  // Word separator becomes space
                        return EXIT_FILE_ERROR;
                    }
                } else {
                    if (!safe_fputc('\n', output)) { // Preserve newlines
                        return EXIT_FILE_ERROR;
                    }
                }
            } else if (c == '.' || c == '-') {
                // Morse code characters
                if (buffer_pos < MAX_MORSE_LENGTH) {
                    morse_buffer[buffer_pos++] = c;
                } else {
                    fprintf(stderr, "Warning: morse sequence too long, truncating\n");
                    buffer_pos = 0;  // Reset buffer
                }
            }
            // Ignore other characters silently
        }
    }
    
    // Check for read error
    if (ferror(input)) {
        fprintf(stderr, "Error: read failed: %s\n", strerror(errno));
        return EXIT_FILE_ERROR;
    }
    
    // Handle any remaining morse in buffer
    if (buffer_pos > 0) {
        morse_buffer[buffer_pos] = '\0';
        char decoded = morse_to_char(morse_buffer);
        
        if (decoded == '?' && strcmp(morse_buffer, "/") != 0) {
            invalid_sequences++;
            if (invalid_sequences <= 10) {
                fprintf(stderr, "Warning: unknown morse sequence '%s'\n", morse_buffer);
            }
        }
        
        if (!safe_fputc(decoded, output)) {
            return EXIT_FILE_ERROR;
        }
    }
    
    if (invalid_sequences > 10) {
        fprintf(stderr, "Warning: %d total invalid morse sequences found\n", invalid_sequences);
    }
    
    if (!safe_fputc('\n', output)) {
        return EXIT_FILE_ERROR;
    }
    
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    int decode_mode = 0;
    const char *char_separator = " ";      // Default separator between characters
    const char *word_separator = " / ";    // Default separator between words
    const char *filename = NULL;
    FILE *input = stdin;
    FILE *output = stdout;
    int result = EXIT_SUCCESS;
    
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
                if (!validate_separator(optarg, "character separator")) {
                    return EXIT_INVALID_ARGS;
                }
                char_separator = optarg;
                break;
            case 'w':
                if (!validate_separator(optarg, "word separator")) {
                    return EXIT_INVALID_ARGS;
                }
                word_separator = optarg;
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
        input = fopen(filename, "r");
        if (!input) {
            fprintf(stderr, "Error: cannot open '%s': %s\n", filename, strerror(errno));
            return EXIT_FILE_ERROR;
        }
    }
    
    // Do the conversion
    if (decode_mode) {
        result = decode_morse(input, output);
    } else {
        result = encode_morse(input, output, char_separator, word_separator);
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
