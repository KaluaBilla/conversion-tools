#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <limits.h>

#define VERSION "1.0"
#define PROGRAM_NAME "factoradic"
#define MAX_DIGITS 20  // Enough for 64-bit numbers

static int decode_mode = 0;
static int verbose_mode = 0;

static void usage(void) {
    printf("Usage: %s [OPTION]... [FILE]\n", PROGRAM_NAME);
    printf("Convert decimal numbers to factoradic or decode factoradic, or standard input, to standard output.\n");
    printf("With no FILE, or when FILE is -, read standard input.\n\n");
    printf("The factoradic number system uses factorial bases (1!, 2!, 3!, ...).\n");
    printf("Each digit position n can have values 0 to n.\n");
    printf("Example: 463 (decimal) = 34201 (factoradic)\n\n");
    printf("Mandatory arguments to long options are mandatory for short options too.\n");
    printf("  -d, --decode          decode factoradic numbers to decimal\n");
    printf("  -v, --verbose         show conversion steps\n");
    printf("      --help            display this help and exit\n");
    printf("      --version         output version information and exit\n\n");
}    

static void version(void) {
    printf("%s %s\n", PROGRAM_NAME, VERSION);
    printf("Factoradic number system converter\n");
    printf("Converts between decimal and factorial base representation\n");
}

static unsigned long long factorial(int n) {
    if (n <= 1) return 1;
    unsigned long long result = 1;
    for (int i = 2; i <= n; i++) {
        if (result > ULLONG_MAX / i) {
            return 0; // Overflow
        }
        result *= i;
    }
    return result;
}

static void decimal_to_factoradic(unsigned long long num, FILE* output) {
    if (num == 0) {
        fprintf(output, "0");
        if (verbose_mode) {
            fprintf(output, " (0 = 0 × 1!)");
        }
        fprintf(output, "\n");
        return;
    }
    
    // Find the highest factorial that fits
    int max_pos = 1;
    while (factorial(max_pos + 1) <= num && factorial(max_pos + 1) > 0) {
        max_pos++;
    }
    
    char result[MAX_DIGITS + 1] = {0};
    int result_pos = 0;
    unsigned long long remaining = num;
    
    if (verbose_mode) {
        fprintf(output, "Converting %llu to factoradic:\n", num);
    }
    
    // Convert from highest position down
    for (int pos = max_pos; pos >= 1; pos--) {
        unsigned long long fact = factorial(pos);
        int digit = remaining / fact;
        
        if (digit > pos) {
            fprintf(stderr, "Error: Invalid digit %d for position %d (max allowed: %d)\n", 
                    digit, pos, pos);
            return;
        }
        
        result[result_pos++] = '0' + digit;
        
        if (verbose_mode) {
            fprintf(output, "%llu ÷ %d! (%llu) = %d remainder %llu\n", 
                    remaining, pos, fact, digit, remaining % fact);
        }
        
        remaining %= fact;
    }
    
    result[result_pos] = '\0';
    
    if (verbose_mode) {
        fprintf(output, "Result: ");
    }
    
    fprintf(output, "%s", result);
    
    if (verbose_mode) {
        fprintf(output, " (factoradic)");
    }
    
    fprintf(output, "\n");
}

static void factoradic_to_decimal(const char* factoradic, FILE* output) {
    int len = strlen(factoradic);
    unsigned long long result = 0;
    
    if (verbose_mode) {
        fprintf(output, "Converting %s from factoradic:\n", factoradic);
    }
    
    // Process each digit from left to right
    for (int i = 0; i < len; i++) {
        char c = factoradic[i];
        
        if (!isdigit(c)) {
            fprintf(stderr, "Error: Invalid character '%c' in factoradic number\n", c);
            return;
        }
        
        int digit = c - '0';
        int position = len - i;  // Position in factoradic (rightmost is position 1)
        
        // Check if digit is valid for this position
        if (digit > position) {
            fprintf(stderr, "Error: Digit %d at position %d exceeds maximum allowed (%d)\n", 
                    digit, position, position);
            return;
        }
        
        unsigned long long fact = factorial(position);
        unsigned long long contribution = digit * fact;
        
        // Check for overflow
        if (result > ULLONG_MAX - contribution) {
            fprintf(stderr, "Error: Number too large for conversion\n");
            return;
        }
        
        result += contribution;
        
        if (verbose_mode) {
            fprintf(output, "%d × %d! (%llu) = %llu\n", 
                    digit, position, fact, contribution);
        }
    }
    
    if (verbose_mode) {
        fprintf(output, "Result: ");
    }
    
    fprintf(output, "%llu", result);
    
    if (verbose_mode) {
        fprintf(output, " (decimal)");
    }
    
    fprintf(output, "\n");
}

static void process_input(FILE* input, FILE* output) {
    char line[256];
    char clean[256];
    
    while (fgets(line, sizeof(line), input)) {
        int clean_pos = 0;
        int found_decimal = 0;
        
        // Extract only numeric characters before decimal point
        for (int i = 0; line[i] && clean_pos < sizeof(clean) - 1; i++) {
            if (line[i] == '.' || line[i] == ',') {
                found_decimal = 1;
                break;  // Stop at decimal point
            }
            if (isdigit(line[i])) {
                clean[clean_pos++] = line[i];
            }
        }
        clean[clean_pos] = '\0';
        
        // Skip if no digits found
        if (clean_pos == 0) {
          fprintf(stderr, "Error: No valid digits found in input: %s", line);
        continue;
}

        
        if (decode_mode) {
            factoradic_to_decimal(clean, output);
        } else {
            unsigned long long num = strtoull(clean, NULL, 10);
            decimal_to_factoradic(num, output);
        }
        
        if (found_decimal && verbose_mode) {
            fprintf(stderr, "Note: Truncated fractional part, using integer portion only\n");
        }
    }
}

int main(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"decode", no_argument, 0, 'd'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "dvhV", long_options, NULL)) != -1) {
        switch (c) {
            case 'd':
                decode_mode = 1;
                break;
            case 'v':
                verbose_mode = 1;
                break;
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
            case 'V':
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
    
    process_input(input, stdout);
    
    if (input != stdin) {
        fclose(input);
    }
    
    return EXIT_SUCCESS;
}
