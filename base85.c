#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>

#define PROGRAM_NAME "base85"
#define VERSION "1.0.0"
#define DEFAULT_WRAP 76
#define ENCODE_CHUNK 4
#define DECODE_CHUNK 5
#define MAX_WRAP 1000000

// Z85 character set (ZeroMQ Base85 - RFC standard)
static const char z85_alphabet[] = 
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-:+=^!/*?&<>()[]{}@%$#";

// Decoder lookup table for Z85
static const int8_t z85_decoder[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 0-15
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 16-31
    -1,68,-1,84,83,82,72,-1,75,76,70,71,-1,69,74,67,  // 32-47  !"#$%&'()*+,-./
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,64,-1,73,66,-1,65,  // 48-63  0123456789:;<=>?
    81,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,  // 64-79  @ABCDEFGHIJKLMNO
    25,26,27,28,29,30,31,32,33,34,35,77,-1,78,79,80,  // 80-95  PQRSTUVWXYZ[\]^_
    -1,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,  // 96-111 `abcdefghijklmno
    51,52,53,54,55,56,57,58,59,60,61,62,63,-1,-1,-1,  // 112-127 pqrstuvwxyz{|}~
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 128-143
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 144-159
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 160-175
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 176-191
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 192-207
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 208-223
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 224-239
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   // 240-255
};

typedef struct {
    int decode;
    int ignore_garbage;
    int wrap;
    const char *input_file;
} options_t;

static void print_error(const char *msg) {
    fprintf(stderr, "%s: %s\n", PROGRAM_NAME, msg);
}

static void print_error_errno(const char *msg) {
    fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, msg, strerror(errno));
}

static void print_help(void) {
    printf("Usage: %s [OPTION]... [FILE]\n", PROGRAM_NAME);
    printf("Base85 encode or decode FILE, or standard input, to standard output.\n");
    printf("Uses Z85 encoding (ZeroMQ Base85 standard).\n");
    printf("With no FILE, or when FILE is -, read standard input.\n\n");
    printf("Mandatory arguments to long options are mandatory for short options too.\n");
    printf("  -d, --decode          decode data\n");
    printf("  -i, --ignore-garbage  when decoding, ignore non-alphabet characters\n");
    printf("  -w, --wrap=COLS       wrap encoded lines after COLS character (default %d).\n", DEFAULT_WRAP);
    printf("                          Use 0 to disable line wrapping\n");
    printf("      --help            display this help and exit\n");
    printf("      --version         output version information and exit\n");
}

static void print_version(void) {
    printf("%s %s\n", PROGRAM_NAME, VERSION);
    printf("Z85 (ZeroMQ Base85) encoder/decoder\n");
}

static int parse_wrap_value(const char *str, int *wrap) {
    char *endptr;
    long val;
    
    errno = 0;
    val = strtol(str, &endptr, 10);
    
    if (errno != 0 || *endptr != '\0' || val < 0 || val > MAX_WRAP) {
        return -1;
    }
    
    *wrap = (int)val;
    return 0;
}

static int parse_arguments(int argc, char *argv[], options_t *opts) {
    // Initialize with defaults
    memset(opts, 0, sizeof(*opts));
    opts->wrap = DEFAULT_WRAP;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--decode") == 0) {
            opts->decode = 1;
        } 
        else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--ignore-garbage") == 0) {
            opts->ignore_garbage = 1;
        } 
        else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--wrap") == 0) {
            if (i + 1 >= argc) {
                print_error("option requires an argument -- 'w'");
                return -1;
            }
            if (parse_wrap_value(argv[++i], &opts->wrap) != 0) {
                print_error("invalid wrap value");
                return -1;
            }
        }
        else if (strncmp(argv[i], "-w", 2) == 0) {
            if (parse_wrap_value(argv[i] + 2, &opts->wrap) != 0) {
                print_error("invalid wrap value");
                return -1;
            }
        }
        else if (strncmp(argv[i], "--wrap=", 7) == 0) {
            if (parse_wrap_value(argv[i] + 7, &opts->wrap) != 0) {
                print_error("invalid wrap value");
                return -1;
            }
        }
        else if (strcmp(argv[i], "--help") == 0) {
            print_help();
            exit(0);
        }
        else if (strcmp(argv[i], "--version") == 0) {
            print_version();
            exit(0);
        }
        else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            fprintf(stderr, "%s: invalid option -- '%s'\n", PROGRAM_NAME, argv[i] + 1);
            fprintf(stderr, "Try '%s --help' for more information.\n", PROGRAM_NAME);
            return -1;
        }
        else {
            if (opts->input_file != NULL) {
                print_error("too many arguments");
                return -1;
            }
            opts->input_file = argv[i];
        }
    }
    
    return 0;
}

static FILE* open_input_file(const char *filename) {
    if (filename == NULL || strcmp(filename, "-") == 0) {
        return stdin;
    }
    
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        print_error_errno(filename);
    }
    return fp;
}

static int encode_z85(FILE *input, FILE *output, int wrap) {
    uint8_t buffer[ENCODE_CHUNK];
    size_t bytes_read;
    int column = 0;
    int result = 0;
    
    while ((bytes_read = fread(buffer, 1, ENCODE_CHUNK, input)) > 0) {
        // Check for read errors
        if (bytes_read != ENCODE_CHUNK && ferror(input)) {
            print_error("read error");
            result = 1;
            break;
        }
        
        // Pad incomplete blocks with zeros
        if (bytes_read < ENCODE_CHUNK) {
            memset(buffer + bytes_read, 0, ENCODE_CHUNK - bytes_read);
        }
        
        // Pack 4 bytes into 32-bit value (big-endian)
        uint32_t value = 0;
        for (size_t i = 0; i < ENCODE_CHUNK; i++) {
            value = (value << 8) | buffer[i];
        }
        
        // Convert to base85 (5 characters)
        char encoded[DECODE_CHUNK];
        for (int i = DECODE_CHUNK - 1; i >= 0; i--) {
            encoded[i] = z85_alphabet[value % 85];
            value /= 85;
        }
        
        // Output the appropriate number of characters
        size_t output_len = (bytes_read == ENCODE_CHUNK) ? DECODE_CHUNK : bytes_read + 1;
        for (size_t i = 0; i < output_len; i++) {
            if (fputc(encoded[i], output) == EOF) {
                print_error("write error");
                result = 1;
                goto cleanup;
            }
            
            column++;
            if (wrap > 0 && column >= wrap) {
                if (fputc('\n', output) == EOF) {
                    print_error("write error");
                    result = 1;
                    goto cleanup;
                }
                column = 0;
            }
        }
    }
    
    // Add final newline if wrapping is enabled and we're not at start of line
    if (wrap > 0 && column > 0) {
        if (fputc('\n', output) == EOF) {
            print_error("write error");
            result = 1;
        }
    }

cleanup:
    if (fflush(output) != 0) {
        print_error("write error");
        result = 1;
    }
    
    return result;
}

static int decode_z85(FILE *input, FILE *output, int ignore_garbage) {
    int c;
    uint32_t value = 0;
    int count = 0;
    int result = 0;
    
    while ((c = fgetc(input)) != EOF) {
        // Skip whitespace
        if (isspace(c)) {
            continue;
        }
        
        // Validate character
        if (c < 0 || c > 255 || z85_decoder[c] == -1) {
            if (ignore_garbage) {
                continue;
            }
            fprintf(stderr, "%s: invalid character in input: '%c' (0x%02x)\n", 
                    PROGRAM_NAME, isprint(c) ? c : '?', (unsigned char)c);
            return 1;
        }
        
        // Accumulate value
        value = value * 85 + z85_decoder[c];
        count++;
        
        if (count == DECODE_CHUNK) {
            // Output 4 bytes (big-endian)
            uint8_t bytes[ENCODE_CHUNK];
            for (int i = ENCODE_CHUNK - 1; i >= 0; i--) {
                bytes[i] = value & 0xFF;
                value >>= 8;
            }
            
            if (fwrite(bytes, 1, ENCODE_CHUNK, output) != ENCODE_CHUNK) {
                print_error("write error");
                result = 1;
                break;
            }
            
            value = 0;
            count = 0;
        }
    }
    
    // Handle incomplete final group
    if (count > 0) {
        if (count == 1) {
            print_error("invalid input: incomplete final group");
            return 1;
        }
        
        // Pad with the highest value character ('z' in Z85)
        while (count < DECODE_CHUNK) {
            value = value * 85 + 84;  // 84 is the value of 'z'
            count++;
        }
        
        // Output appropriate number of bytes
        int output_bytes = count - 1;  // n encoded chars = n-1 decoded bytes
        uint8_t bytes[ENCODE_CHUNK];
        for (int i = ENCODE_CHUNK - 1; i >= 0; i--) {
            bytes[i] = value & 0xFF;
            value >>= 8;
        }
        
        if (fwrite(bytes, 1, output_bytes, output) != (size_t)output_bytes) {
            print_error("write error");
            result = 1;
        }
    }
    
    if (fflush(output) != 0) {
        print_error("write error");
        result = 1;
    }
    
    return result;
}

int main(int argc, char *argv[]) {
    options_t opts;
    FILE *input = NULL;
    int result = 0;
    
    // Parse command line arguments
    if (parse_arguments(argc, argv, &opts) != 0) {
        return 1;
    }
    
    // Open input file
    input = open_input_file(opts.input_file);
    if (input == NULL) {
        return 1;
    }
    
    // Process the data
    if (opts.decode) {
        result = decode_z85(input, stdout, opts.ignore_garbage);
    } else {
        result = encode_z85(input, stdout, opts.wrap);
    }
    
    // Cleanup
    if (input != stdin) {
        if (fclose(input) != 0) {
            print_error_errno("close");
            result = 1;
        }
    }
    
    return result;
}
