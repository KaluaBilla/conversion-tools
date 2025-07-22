#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>

#define VERSION "1.0"
#define PROGRAM_NAME "dancing_man"
#define MAX_LINE_LENGTH 1024

static int decode_mode = 0;
static int compact_mode = 0;

// Dancing Man ASCII representations
// Each letter has a unique stick figure pose
static const char* dancing_man_table[][2] = {
    {"A", " O \n/|\\\n/ \\"},
    {"B", " O \n/||\n/ \\"},
    {"C", " O \n/| \n/ \\"},
    {"D", " O \n |||\n/ \\"},
    {"E", " O \n/|_\n/ \\"},
    {"F", " O \n/|_\n/  "},
    {"G", " O \n/|+\n/ \\"},
    {"H", " O \n||||\n/ \\"},
    {"I", " O \n | \n/ \\"},
    {"J", " O \n  |\n/ \\"},
    {"K", " O \n/|<\n/ \\"},
    {"L", " O \n/| \n/_\\"},
    {"M", " O \n/|\\\\\n/ \\"},
    {"N", " O \n/|/\n/ \\"},
    {"O", " O \n/O\\\n/ \\"},
    {"P", " O \n/|^\n/ \\"},
    {"Q", " O \n/O\\\n/_\\"},
    {"R", " O \n/|>\n/ \\"},
    {"S", " O \n/|~\n/ \\"},
    {"T", " O \n-|-\n/ \\"},
    {"U", " O \n/||\n\\_/"},
    {"V", " O \n/|\\\n \\ "},
    {"W", " O \n/|\\\\\n\\ /"},
    {"X", " O \n<|>\n/ \\"},
    {"Y", " O \n\\|/\n | "},
    {"Z", " O \n/|/\n/_\\"},
    {NULL, NULL}
};

// Compact single-line representations for easier parsing
static const char* compact_table[][2] = {
    {"A", "O/|\\"},
    {"B", "O/||"},
    {"C", "O/|_"},
    {"D", "O|||"},
    {"E", "O/|_"},
    {"F", "O/|^"},
    {"G", "O/|+"},
    {"H", "O||||"},
    {"I", "O_|_"},
    {"J", "O__|"},
    {"K", "O/|<"},
    {"L", "O/|_"},
    {"M", "O/|\\\\"},
    {"N", "O/|/"},
    {"O", "O/O\\"},
    {"P", "O/|^"},
    {"Q", "O/O\\"},
    {"R", "O/|>"},
    {"S", "O/|~"},
    {"T", "O-|-"},
    {"U", "O/||"},
    {"V", "O/|\\"},
    {"W", "O/|\\\\"},
    {"X", "O<|>"},
    {"Y", "O\\|/"},
    {"Z", "O/|/"},
    {NULL, NULL}
};

static void usage(void) {
    printf("Usage: %s [OPTION]... [FILE]\n", PROGRAM_NAME);
    printf("Convert text to Dancing Man cipher or decode Dancing Man figures, or standard input, to standard output.\n");
    printf("With no FILE, or when FILE is -, read standard input.\n\n");
    printf("The Dancing Man cipher uses stick figure poses to represent letters.\n");
    printf("Each letter has a unique pose based on Sherlock Holmes' 'Adventure of the Dancing Men'.\n\n");
    printf("Mandatory arguments to long options are mandatory for short options too.\n");
    printf("  -d, --decode          decode Dancing Man figures back to text\n");
    printf("  -c, --compact         use compact single-line representations\n");
    printf("      --help            display this help and exit\n");
    printf("      --version         output version information and exit\n\n");
}

static void version(void) {
    printf("%s %s\n", PROGRAM_NAME, VERSION);
    printf("Dancing Man cipher encoder/decoder\n");
    printf("Based on Arthur Conan Doyle's 'The Adventure of the Dancing Men'\n");
}

static const char* find_dancing_man(char c) {
    const char* (*table)[2] = compact_mode ? compact_table : dancing_man_table;
    
    c = toupper(c);
    for (int i = 0; table[i][0] != NULL; i++) {
        if (table[i][0][0] == c) {
            return table[i][1];
        }
    }
    return NULL;
}

static char find_letter(const char* figure) {
    const char* (*table)[2] = compact_mode ? compact_table : dancing_man_table;
    
    for (int i = 0; table[i][0] != NULL; i++) {
        if (strcmp(table[i][1], figure) == 0) {
            return table[i][0][0];
        }
    }
    return 0;
}

static void encode_dancing_man(FILE* input, FILE* output) {
    int c;
    int first = 1;
    
    while ((c = fgetc(input)) != EOF) {
        if (isalpha(c)) {
            const char* figure = find_dancing_man(c);
            if (figure) {
                if (!first && !compact_mode) {
                    fprintf(output, "\n\n");  // Separator between figures
                }
                if (!first && compact_mode) {
                    fprintf(output, " ");  // Space separator in compact mode
                }
                fprintf(output, "%s", figure);
                first = 0;
            }
        } else if (isspace(c) && c == ' ') {
            if (!compact_mode) {
                fprintf(output, "\n\n[SPACE]\n\n");
            } else {
                fprintf(output, " [SP] ");
            }
            first = 0;
        } else if (c == '\n' && !compact_mode) {
            fprintf(output, "\n\n[NEWLINE]\n\n");
            first = 0;
        }
    }
    
    if (!compact_mode) {
        fprintf(output, "\n");
    }
}

static void decode_dancing_man(FILE* input, FILE* output) {
    char buffer[4096];
    char figure[256];
    int buf_pos = 0;
    
    // Read entire input
    int c;
    while ((c = fgetc(input)) != EOF && buf_pos < sizeof(buffer) - 1) {
        buffer[buf_pos++] = c;
    }
    buffer[buf_pos] = '\0';
    
    if (compact_mode) {
        // Parse compact format (space-separated)
        char* token = strtok(buffer, " \n");
        while (token != NULL) {
            if (strcmp(token, "[SP]") == 0) {
                fputc(' ', output);
            } else {
                char letter = find_letter(token);
                if (letter) {
                    fputc(letter, output);
                }
            }
            token = strtok(NULL, " \n");
        }
    } else {
        // Parse multi-line format
        char* line = strtok(buffer, "\n");
        char current_figure[256] = "";
        int figure_lines = 0;
        
        while (line != NULL) {
            if (strlen(line) == 0) {
                // Empty line - end of figure
                if (figure_lines > 0) {
                    char letter = find_letter(current_figure);
                    if (letter) {
                        fputc(letter, output);
                    }
                    current_figure[0] = '\0';
                    figure_lines = 0;
                }
            } else if (strstr(line, "[SPACE]")) {
                fputc(' ', output);
            } else if (strstr(line, "[NEWLINE]")) {
                fputc('\n', output);
            } else {
                // Part of a figure
                if (figure_lines > 0) {
                    strcat(current_figure, "\n");
                }
                strcat(current_figure, line);
                figure_lines++;
            }
            line = strtok(NULL, "\n");
        }
        
        // Handle last figure if no trailing newline
        if (figure_lines > 0) {
            char letter = find_letter(current_figure);
            if (letter) {
                fputc(letter, output);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"decode", no_argument, 0, 'd'},
        {"compact", no_argument, 0, 'c'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "dchv", long_options, NULL)) != -1) {
        switch (c) {
            case 'd':
                decode_mode = 1;
                break;
            case 'c':
                compact_mode = 1;
                break;
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
            case 'v':
                version();
                exit(EXIT_SUCCESS);
            case '?':
                exit(EXIT_FAILURE);
        }
    }
    
    FILE* input = stdin;
    
    if (optind < argc && strcmp(argv[optind], "-") != 0) {
        input = fopen(argv[optind], "r");
        if (!input) {
            perror(argv[optind]);
            exit(EXIT_FAILURE);
        }
    }
    
    if (decode_mode) {
        decode_dancing_man(input, stdout);
    } else {
        encode_dancing_man(input, stdout);
    }
    
    if (input != stdin) {
        fclose(input);
    }
    
    return EXIT_SUCCESS;
}
