#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>

#define ASCII85_GROUP_SIZE 4
#define ASCII85_ENCODED_SIZE 5
#define MAX_LINE_LENGTH 32768
#define DEFAULT_WRAP_COLS 76

typedef enum {
    RESULT_SUCCESS = 0,
    RESULT_ERROR_FILE,
    RESULT_ERROR_MEMORY,
    RESULT_ERROR_ARGS,
    RESULT_ERROR_IO,
    RESULT_ERROR_DECODE
} result_t;

static const char ascii85_chars[] = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstu";

void print_usage(const char *program_name) {
    if (program_name == NULL) return;
    
    printf("Usage: %s [OPTION]... [FILE]\n", program_name);
    printf("ASCII85 encode or decode FILE, or standard input, to standard output.\n");
    printf("With no FILE, or when FILE is -, read standard input.\n\n");
    printf("  -d, --decode          decode ASCII85 data\n");
    printf("  -w, --wrap=COLS       wrap encoded lines after COLS characters (default %d)\n", DEFAULT_WRAP_COLS);
    printf("                        Use 0 to disable line wrapping\n");
    printf("  -z, --zero-compress   use 'z' for all-zero groups (Adobe standard)\n");
    printf("  -y, --space-compress  use 'y' for all-space groups (Adobe standard)\n");
    printf("      --help           display this help and exit\n");
    printf("      --version        output version information and exit\n\n");
}

void print_version(void) {
    printf("ascii85 1.0\n");
    printf("ASCII85 encoder/decoder (RFC 1924 compatible)\n");
}

static int ascii85_decode_char(unsigned char c) {
    if (c >= '!' && c <= 'u') {
        return c - '!';
    }
    return -1;
}

static int is_valid_wrap_cols(const char *str, int *value) {
    if (str == NULL || value == NULL) {
        return 0;
    }
    
    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    
    if (errno == ERANGE || val < 0 || val > INT_MAX || *endptr != '\0') {
        return 0;
    }
    
    *value = (int)val;
    return 1;
}

static result_t encode_group(const unsigned char *input, size_t len, char *output, 
                           size_t output_size, int use_z, int use_y, size_t *output_len) {
    if (input == NULL || output == NULL || output_len == NULL || 
        len > ASCII85_GROUP_SIZE || output_size < ASCII85_ENCODED_SIZE + 1) {
        return RESULT_ERROR_ARGS;
    }
    
    uint32_t value = 0;
    
    // Pack bytes into 32-bit value (big-endian)
    for (size_t i = 0; i < ASCII85_GROUP_SIZE; i++) {
        value = (value << 8) | (i < len ? input[i] : 0);
    }
    
    // Check for special compression cases
    if (len == ASCII85_GROUP_SIZE) {
        if (use_z && value == 0) {
            output[0] = 'z';
            output[1] = '\0';
            *output_len = 1;
            return RESULT_SUCCESS;
        }
        if (use_y && value == 0x20202020U) {
            output[0] = 'y';
            output[1] = '\0';
            *output_len = 1;
            return RESULT_SUCCESS;
        }
    }
    
    // Convert to base-85
    for (int i = ASCII85_ENCODED_SIZE - 1; i >= 0; i--) {
        output[i] = ascii85_chars[value % 85];
        value /= 85;
    }
    
    // For partial groups, only output needed characters
    *output_len = (len == ASCII85_GROUP_SIZE) ? ASCII85_ENCODED_SIZE : len + 1;
    output[*output_len] = '\0';
    
    return RESULT_SUCCESS;
}

static result_t decode_group(const char *input, size_t len, unsigned char *output,
                           size_t output_size, size_t *output_len) {
    if (input == NULL || output == NULL || output_len == NULL ||
        len == 0 || len > ASCII85_ENCODED_SIZE || output_size < ASCII85_GROUP_SIZE) {
        return RESULT_ERROR_ARGS;
    }
    
    // Handle special compression
    if (len == 1) {
        if (input[0] == 'z') {
            memset(output, 0, ASCII85_GROUP_SIZE);
            *output_len = ASCII85_GROUP_SIZE;
            return RESULT_SUCCESS;
        }
        if (input[0] == 'y') {
            memset(output, 0x20, ASCII85_GROUP_SIZE);
            *output_len = ASCII85_GROUP_SIZE;
            return RESULT_SUCCESS;
        }
    }
    
    // Validate minimum length for regular groups
    if (len < 2) {
        return RESULT_ERROR_DECODE;
    }
    
    uint32_t value = 0;
    
    // Convert from base-85
    for (size_t i = 0; i < len; i++) {
        int digit = ascii85_decode_char((unsigned char)input[i]);
        if (digit < 0) {
            return RESULT_ERROR_DECODE;
        }
        
        // Check for overflow
        if (value > (UINT32_MAX - digit) / 85) {
            return RESULT_ERROR_DECODE;
        }
        
        value = value * 85 + (uint32_t)digit;
    }
    
    // For partial groups, add padding
    for (size_t i = len; i < ASCII85_ENCODED_SIZE; i++) {
        if (value > (UINT32_MAX - 84) / 85) {
            return RESULT_ERROR_DECODE;
        }
        value = value * 85 + 84;
    }
    
    // Unpack 32-bit value to bytes (big-endian)
    *output_len = (len > 1) ? len - 1 : 0;
    for (size_t i = 0; i < *output_len; i++) {
        output[ASCII85_GROUP_SIZE - 1 - i] = (unsigned char)(value & 0xFF);
        value >>= 8;
    }
    
    return RESULT_SUCCESS;
}

static result_t encode_ascii85(FILE *input, FILE *output, int wrap_cols, int use_z, int use_y) {
    if (input == NULL || output == NULL) {
        return RESULT_ERROR_ARGS;
    }
    
    unsigned char buffer[ASCII85_GROUP_SIZE];
    char encoded[ASCII85_ENCODED_SIZE + 1];
    size_t bytes_read;
    int col_count = 0;
    
    while ((bytes_read = fread(buffer, 1, ASCII85_GROUP_SIZE, input)) > 0) {
        if (ferror(input)) {
            fprintf(stderr, "Error reading input\n");
            return RESULT_ERROR_IO;
        }
        
        size_t encoded_len;
        result_t result = encode_group(buffer, bytes_read, encoded, sizeof(encoded), 
                                     use_z, use_y, &encoded_len);
        if (result != RESULT_SUCCESS) {
            return result;
        }
        
        for (size_t i = 0; i < encoded_len; i++) {
            if (fputc(encoded[i], output) == EOF) {
                return RESULT_ERROR_IO;
            }
            col_count++;
            
            // Add line wrap if specified
            if (wrap_cols > 0 && col_count >= wrap_cols) {
                if (fputc('\n', output) == EOF) {
                    return RESULT_ERROR_IO;
                }
                col_count = 0;
            }
            
            // Prevent extremely long lines
            if (col_count > MAX_LINE_LENGTH) {
                fprintf(stderr, "Error: line too long\n");
                return RESULT_ERROR_IO;
            }
        }
    }
    
    // Add final newline if needed
    if (wrap_cols == 0 || col_count > 0) {
        if (fputc('\n', output) == EOF) {
            return RESULT_ERROR_IO;
        }
    }
    
    return RESULT_SUCCESS;
}

static result_t decode_ascii85(FILE *input, FILE *output) {
    if (input == NULL || output == NULL) {
        return RESULT_ERROR_ARGS;
    }
    
    char buffer[ASCII85_ENCODED_SIZE];
    unsigned char decoded[ASCII85_GROUP_SIZE];
    size_t buffer_pos = 0;
    int c;
    size_t line_length = 0;
    
    while ((c = fgetc(input)) != EOF) {
        if (ferror(input)) {
            fprintf(stderr, "Error reading input\n");
            return RESULT_ERROR_IO;
        }
        
        // Skip whitespace and newlines
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (c == '\n') {
                line_length = 0;
            }
            continue;
        }
        
        line_length++;
        if (line_length > MAX_LINE_LENGTH) {
            fprintf(stderr, "Error: line too long\n");
            return RESULT_ERROR_DECODE;
        }
        
        // Handle special compression characters
        if (c == 'z' || c == 'y') {
            if (buffer_pos > 0) {
                fprintf(stderr, "Error: compression character in middle of group\n");
                return RESULT_ERROR_DECODE;
            }
            
            buffer[0] = (char)c;
            size_t decoded_len;
            result_t result = decode_group(buffer, 1, decoded, sizeof(decoded), &decoded_len);
            if (result != RESULT_SUCCESS) {
                return result;
            }
            
            if (fwrite(decoded, 1, decoded_len, output) != decoded_len) {
                return RESULT_ERROR_IO;
            }
            continue;
        }
        
        // Regular ASCII85 character
        if (ascii85_decode_char((unsigned char)c) >= 0) {
            if (buffer_pos >= ASCII85_ENCODED_SIZE) {
                fprintf(stderr, "Error: buffer overflow\n");
                return RESULT_ERROR_DECODE;
            }
            
            buffer[buffer_pos++] = (char)c;
            
            // Process complete group
            if (buffer_pos == ASCII85_ENCODED_SIZE) {
                size_t decoded_len;
                result_t result = decode_group(buffer, ASCII85_ENCODED_SIZE, decoded, 
                                             sizeof(decoded), &decoded_len);
                if (result != RESULT_SUCCESS) {
                    fprintf(stderr, "Error: invalid ASCII85 sequence\n");
                    return result;
                }
                
                if (fwrite(decoded, 1, decoded_len, output) != decoded_len) {
                    return RESULT_ERROR_IO;
                }
                buffer_pos = 0;
            }
        } else {
            fprintf(stderr, "Warning: ignoring invalid character '%c' (0x%02X)\n", 
                    isprint(c) ? c : '?', (unsigned char)c);
        }
    }
    
    // Handle any remaining partial group
    if (buffer_pos > 0) {
        if (buffer_pos < 2) {
            fprintf(stderr, "Error: incomplete ASCII85 group at end\n");
            return RESULT_ERROR_DECODE;
        }
        
        size_t decoded_len;
        result_t result = decode_group(buffer, buffer_pos, decoded, sizeof(decoded), &decoded_len);
        if (result != RESULT_SUCCESS) {
            fprintf(stderr, "Error: invalid final ASCII85 group\n");
            return result;
        }
        
        if (fwrite(decoded, 1, decoded_len, output) != decoded_len) {
            return RESULT_ERROR_IO;
        }
    }
    
    return RESULT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc == 0 || argv == NULL) {
        return RESULT_ERROR_ARGS;
    }
    
    int decode_mode = 0;
    int wrap_cols = DEFAULT_WRAP_COLS;
    int use_z = 0;
    int use_y = 0;
    const char *filename = NULL;
    FILE *input = NULL;
    FILE *output = stdout;
    result_t result = RESULT_SUCCESS;
    
    static struct option long_options[] = {
        {"decode", no_argument, 0, 'd'},
        {"wrap", required_argument, 0, 'w'},
        {"zero-compress", no_argument, 0, 'z'},
        {"space-compress", no_argument, 0, 'y'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "dw:zyhv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                decode_mode = 1;
                break;
            case 'w':
                if (!is_valid_wrap_cols(optarg, &wrap_cols)) {
                    fprintf(stderr, "Error: invalid wrap columns value '%s'\n", optarg);
                    return RESULT_ERROR_ARGS;
                }
                break;
            case 'z':
                use_z = 1;
                break;
            case 'y':
                use_y = 1;
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
        input = fopen(filename, "rb");
        if (input == NULL) {
            fprintf(stderr, "Error opening '%s': %s\n", filename, strerror(errno));
            return RESULT_ERROR_FILE;
        }
    }
    
    // Process file
    if (decode_mode) {
        result = decode_ascii85(input, output);
    } else {
        result = encode_ascii85(input, output, wrap_cols, use_z, use_y);
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
