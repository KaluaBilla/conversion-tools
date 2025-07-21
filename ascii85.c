#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTION]... [FILE]\n", program_name);
    printf("ASCII85 encode or decode FILE, or standard input, to standard output.\n");
    printf("With no FILE, or when FILE is -, read standard input.\n\n");
    printf("  -d, --decode          decode ASCII85 data\n");
    printf("  -w, --wrap=COLS       wrap encoded lines after COLS characters (default 76)\n");
    printf("                        Use 0 to disable line wrapping\n");
    printf("  -z, --zero-compress   use 'z' for all-zero groups (Adobe standard)\n");
    printf("  -y, --space-compress  use 'y' for all-space groups (Adobe standard)\n");
    printf("      --help           display this help and exit\n");
    printf("      --version        output version information and exit\n\n");
    printf("ASCII85 encodes 4 bytes into 5 ASCII characters (20%% more efficient than base64)\n");
    printf("Uses characters: !\"#$%%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstu\n");
}

void print_version() {
    printf("ascii85 1.0\n");
    printf("ASCII85 encoder/decoder (RFC 1924 compatible)\n");
}

// ASCII85 character set (85 printable ASCII chars starting from '!')
static const char ascii85_chars[] = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstu";

// Decode ASCII85 character to value (0-84)
int ascii85_decode_char(char c) {
    if (c >= '!' && c <= 'u') {
        return c - '!';
    }
    return -1;  // Invalid character
}

// Encode 4 bytes to 5 ASCII85 characters
void encode_group(unsigned char *input, int len, char *output, int use_z, int use_y) {
    unsigned long value = 0;
    int i;
    
    // Pack 4 bytes into 32-bit value (big-endian)
    for (i = 0; i < 4; i++) {
        value = (value << 8) | (i < len ? input[i] : 0);
    }
    
    // Check for special compression cases (Adobe standard)
    if (len == 4) {
        if (use_z && value == 0) {
            output[0] = 'z';
            output[1] = '\0';
            return;
        }
        if (use_y && value == 0x20202020) {  // Four spaces
            output[0] = 'y';
            output[1] = '\0';
            return;
        }
    }
    
    // Convert to base-85 (5 characters)
    for (i = 4; i >= 0; i--) {
        output[i] = ascii85_chars[value % 85];
        value /= 85;
    }
    
    // For partial groups, only output needed characters
    output[len + 1] = '\0';
}

// Decode 5 ASCII85 characters to 4 bytes
int decode_group(const char *input, int len, unsigned char *output) {
    unsigned long value = 0;
    int i;
    
    // Handle special compression
    if (len == 1) {
        if (input[0] == 'z') {
            // All zeros
            for (i = 0; i < 4; i++) output[i] = 0;
            return 4;
        }
        if (input[0] == 'y') {
            // All spaces
            for (i = 0; i < 4; i++) output[i] = 0x20;
            return 4;
        }
    }
    
    // Convert from base-85
    for (i = 0; i < len; i++) {
        int digit = ascii85_decode_char(input[i]);
        if (digit < 0) return -1;  // Invalid character
        value = value * 85 + digit;
    }
    
    // For partial groups, add padding
    for (i = len; i < 5; i++) {
        value = value * 85 + 84;  // Pad with 'u' (max value)
    }
    
    // Unpack 32-bit value to bytes (big-endian)
    int output_len = (len > 1) ? len - 1 : 0;
    for (i = 0; i < output_len; i++) {
        output[3 - i] = value & 0xFF;
        value >>= 8;
    }
    
    return output_len;
}

// Encode file to ASCII85
void encode_ascii85(FILE *input, FILE *output, int wrap_cols, int use_z, int use_y) {
    unsigned char buffer[4];
    char encoded[6];
    int bytes_read;
    int col_count = 0;
    
    while ((bytes_read = fread(buffer, 1, 4, input)) > 0) {
        encode_group(buffer, bytes_read, encoded, use_z, use_y);
        
        int encoded_len = strlen(encoded);
        for (int i = 0; i < encoded_len; i++) {
            fputc(encoded[i], output);
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

// Decode ASCII85 to file
void decode_ascii85(FILE *input, FILE *output) {
    char buffer[5];
    unsigned char decoded[4];
    int buffer_pos = 0;
    int c;
    
    while ((c = fgetc(input)) != EOF) {
        // Skip whitespace and newlines
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            continue;
        }
        
        // Handle special compression characters
        if (c == 'z' || c == 'y') {
            if (buffer_pos > 0) {
                fprintf(stderr, "Error: compression character in middle of group\n");
                return;
            }
            buffer[0] = c;
            int decoded_len = decode_group(buffer, 1, decoded);
            if (decoded_len > 0) {
                fwrite(decoded, 1, decoded_len, output);
            }
            continue;
        }
        
        // Regular ASCII85 character
        if (ascii85_decode_char(c) >= 0) {
            buffer[buffer_pos++] = c;
            
            // Process complete group
            if (buffer_pos == 5) {
                int decoded_len = decode_group(buffer, 5, decoded);
                if (decoded_len > 0) {
                    fwrite(decoded, 1, decoded_len, output);
                } else {
                    fprintf(stderr, "Error: invalid ASCII85 sequence\n");
                    return;
                }
                buffer_pos = 0;
            }
        } else {
            fprintf(stderr, "Warning: ignoring invalid character '%c' (0x%02X)\n", 
                    c, (unsigned char)c);
        }
    }
    
    // Handle any remaining partial group
    if (buffer_pos > 0) {
        if (buffer_pos < 2) {
            fprintf(stderr, "Error: incomplete ASCII85 group at end\n");
            return;
        }
        int decoded_len = decode_group(buffer, buffer_pos, decoded);
        if (decoded_len > 0) {
            fwrite(decoded, 1, decoded_len, output);
        }
    }
}

int main(int argc, char *argv[]) {
    int decode_mode = 0;
    int wrap_cols = 76;  // Default wrap like base64
    int use_z = 0;       // Adobe z compression
    int use_y = 0;       // Adobe y compression
    const char *filename = NULL;
    FILE *input = stdin;
    FILE *output = stdout;
    
    // Parse command line options
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
                wrap_cols = atoi(optarg);
                if (wrap_cols < 0) {
                    fprintf(stderr, "Error: wrap columns must be >= 0\n");
                    return 1;
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
        decode_ascii85(input, output);
    } else {
        encode_ascii85(input, output, wrap_cols, use_z, use_y);
    }
    
    // Cleanup
    if (input != stdin) {
        fclose(input);
    }
    
    return 0;
}
