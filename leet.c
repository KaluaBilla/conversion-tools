#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>

#define VERSION "1.0"
#define PROGRAM_NAME "leetspeak"

static int decode_mode = 0;
static int ignore_case = 0;
static int level = 1; // 1 = basic, 2 = advanced, 3 = extreme

// Leetspeak conversion tables
static const char* basic_leet[][2] = {
    {"a", "4"}, {"A", "4"},
    {"e", "3"}, {"E", "3"},
    {"i", "1"}, {"I", "1"},
    {"l", "1"}, {"L", "1"},
    {"o", "0"}, {"O", "0"},
    {"s", "5"}, {"S", "5"},
    {"t", "7"}, {"T", "7"},
    {NULL, NULL}
};

static const char* advanced_leet[][2] = {
    {"a", "4"}, {"A", "4"},
    {"b", "6"}, {"B", "6"},
    {"e", "3"}, {"E", "3"},
    {"g", "9"}, {"G", "9"},
    {"i", "1"}, {"I", "1"},
    {"l", "1"}, {"L", "1"},
    {"o", "0"}, {"O", "0"},
    {"s", "5"}, {"S", "5"},
    {"t", "7"}, {"T", "7"},
    {"z", "2"}, {"Z", "2"},
    {NULL, NULL}
};

static const char* extreme_leet[][2] = {
    {"a", "4"}, {"A", "@"},
    {"b", "6"}, {"B", "|3"},
    {"c", "<"}, {"C", "("},
    {"d", "|)"}, {"D", "|)"},
    {"e", "3"}, {"E", "3"},
    {"f", "|="}, {"F", "|="},
    {"g", "9"}, {"G", "6"},
    {"h", "#"}, {"H", "|-|"},
    {"i", "1"}, {"I", "!"},
    {"j", "_|"}, {"J", "_|"},
    {"k", "|<"}, {"K", "|<"},
    {"l", "1"}, {"L", "|_"},
    {"m", "|\\/|"}, {"M", "|\\/|"},
    {"n", "|\\|"}, {"N", "|\\|"},
    {"o", "0"}, {"O", "0"},
    {"p", "|>"}, {"P", "|>"},
    {"q", "9"}, {"Q", "0_"},
    {"r", "|2"}, {"R", "|2"},
    {"s", "5"}, {"S", "$"},
    {"t", "7"}, {"T", "7"},
    {"u", "|_|"}, {"U", "|_|"},
    {"v", "\\/"}, {"V", "\\/"},
    {"w", "VV"}, {"W", "VV"},
    {"x", "><"}, {"X", "><"},
    {"y", "`/"}, {"Y", "`/"},
    {"z", "2"}, {"Z", "2"},
    {NULL, NULL}
};

static void usage(void) {
    printf("Usage: %s [OPTION]... [FILE]\n", PROGRAM_NAME);
    printf("Convert text to leetspeak or decode leetspeak, or standard input, to standard output.\n");
    printf("With no FILE, or when FILE is -, read standard input.\n\n");
    printf("Mandatory arguments to long options are mandatory for short options too.\n");
    printf("  -d, --decode          decode leetspeak back to normal text\n");
    printf("  -l, --level=LEVEL     leetspeak level: 1=basic, 2=advanced, 3=extreme (default 1)\n");
    printf("  -i, --ignore-case     ignore case when decoding\n");
    printf("      --help            display this help and exit\n");
    printf("      --version         output version information and exit\n\n");
}

static void version(void) {
    printf("%s %s\n", PROGRAM_NAME, VERSION);
    printf("Leetspeak encoder/decoder\n");
}

static const char* find_leet_char(char c, const char* table[][2]) {
    for (int i = 0; table[i][0] != NULL; i++) {
        if (table[i][0][0] == c) {
            return table[i][1];
        }
    }
    return NULL;
}

static char find_normal_char(const char* leet_str, int len, const char* table[][2]) {
    for (int i = 0; table[i][0] != NULL; i++) {
        if (strlen(table[i][1]) == len && strncmp(table[i][1], leet_str, len) == 0) {
            return table[i][0][0];
        }
    }
    return 0;
}

static void encode_leetspeak(FILE* input, FILE* output) {
    const char* (*table)[2];
    
    switch (level) {
        case 2: table = advanced_leet; break;
        case 3: table = extreme_leet; break;
        default: table = basic_leet; break;
    }
    
    int c;
    while ((c = fgetc(input)) != EOF) {
        const char* leet = find_leet_char(c, table);
        if (leet) {
            fputs(leet, output);
        } else {
            fputc(c, output);
        }
    }
}

static void decode_leetspeak(FILE* input, FILE* output) {
    const char* (*table)[2];
    char buffer[1024];
    int buf_pos = 0;
    
    switch (level) {
        case 2: table = advanced_leet; break;
        case 3: table = extreme_leet; break;
        default: table = basic_leet; break;
    }
    
    // Read entire input into buffer for multi-character matching
    int c;
    while ((c = fgetc(input)) != EOF && buf_pos < sizeof(buffer) - 1) {
        buffer[buf_pos++] = c;
    }
    buffer[buf_pos] = '\0';
    
    int i = 0;
    while (i < buf_pos) {
        int found = 0;
        
        // Try to match multi-character leetspeak sequences (longest first)
        for (int len = 4; len >= 1 && !found; len--) {
            if (i + len <= buf_pos) {
                char normal = find_normal_char(buffer + i, len, table);
                if (normal) {
                    fputc(normal, output);
                    i += len;
                    found = 1;
                }
            }
        }
        
        if (!found) {
            fputc(buffer[i], output);
            i++;
        }
    }
}

int main(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"decode", no_argument, 0, 'd'},
        {"level", required_argument, 0, 'l'},
        {"ignore-case", no_argument, 0, 'i'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "dl:ihv", long_options, NULL)) != -1) {
        switch (c) {
            case 'd':
                decode_mode = 1;
                break;
            case 'l':
                level = atoi(optarg);
                if (level < 1 || level > 3) {
                    fprintf(stderr, "%s: invalid level '%s'\n", PROGRAM_NAME, optarg);
                    fprintf(stderr, "Valid levels are 1 (basic), 2 (advanced), 3 (extreme)\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'i':
                ignore_case = 1;
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
        decode_leetspeak(input, stdout);
    } else {
        encode_leetspeak(input, stdout);
    }
    
    if (input != stdin) {
        fclose(input);
    }
    
    return EXIT_SUCCESS;
}
