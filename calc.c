
/*
 * Ranjeet Kumar Gupta 251ADB005
 * https://github.com/ranjeetkumarguptabro-maker/calc-lab-home
 * Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
 * Resources used: ChatGPT (OpenAI); see conversation history.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#define STUDENT_FIRST "ranjeet"
#define STUDENT_LAST  "gupta"
#define STUDENT_ID    "251ADB005"

static const char *p;
static int failed;

static void spaces(void)
{
    while (*p == ' ' || *p == '\t')
        p++;
}

static long long number(void)
{
    spaces();

    if (!isdigit((unsigned char)*p)) {
        failed = 1;
        return 0;
    }

    long long value = 0;

    while (isdigit((unsigned char)*p)) {
        int digit = *p - '0';

        if (value > (9223372036854775807LL - digit) / 10) {
            failed = 1;
            return 0;
        }

        value = value * 10 + digit;
        p++;
    }

    return value;
}

/* This milestone supports addition and subtraction. */
static long long expression(void)
{
    long long value = number();

    while (!failed) {
        spaces();

        char op = *p;

        if (op != '+' && op != '-')
            break;

        p++;

        long long rhs = number();

        if (failed)
            break;

        if (op == '+') {
            if (rhs > 0 && value > 9223372036854775807LL - rhs) {
                failed = 1;
                break;
            }
            value += rhs;
        } else {
            if (rhs > 0 && value < (-9223372036854775807LL - 1) + rhs) {
                failed = 1;
                break;
            }
            value -= rhs;
        }
    }

    return value;
}

/* Read the input file into a dynamically allocated buffer. */
static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (f == NULL)
        return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long length = ftell(f);

    if (length < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    char *text = malloc((size_t)length + 1);

    if (text == NULL) {
        fclose(f);
        return NULL;
    }

    size_t n = fread(text, 1, (size_t)length, f);

    if (n != (size_t)length || ferror(f)) {
        free(text);
        fclose(f);
        return NULL;
    }

    text[n] = '\0';
    fclose(f);

    return text;
}

/* Extract the final component of an input path. */
static char *basename_copy(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *name = slash ? slash + 1 : path;

    size_t n = strlen(name);
    char *copy = malloc(n + 1);

    if (copy != NULL)
        memcpy(copy, name, n + 1);

    return copy;
}

int main(int argc, char **argv)
{
    const char *input = NULL;
    const char *outdir = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 ||
            strcmp(argv[i], "--output-dir") == 0) {
            if (++i >= argc || outdir != NULL) {
                fprintf(stderr, "Invalid arguments\n");
                return 1;
            }

            outdir = argv[i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        } else if (input == NULL) {
            input = argv[i];
        } else {
            fprintf(stderr, "Too many input files\n");
            return 1;
        }
    }

    if (input == NULL) {
        fprintf(stderr, "Usage: calc [-o OUTDIR] input.txt\n");
        return 1;
    }

    char *base = basename_copy(input);

    if (base == NULL) {
        fprintf(stderr, "Out of memory\n");
        return 1;
    }

    size_t base_len = strlen(base);

    if (base_len >= 4 &&
        strcmp(base + base_len - 4, ".txt") == 0) {
        base[base_len - 4] = '\0';
    }

    char default_dir[1024];

    if (outdir == NULL) {
        snprintf(default_dir, sizeof default_dir, "%s_%s_%s",
                 base, STUDENT_FIRST, STUDENT_ID);
        outdir = default_dir;
    }

    char *source = read_file(input);

    if (source == NULL) {
        fprintf(stderr, "Cannot read input file: %s\n", input);
        free(base);
        return 1;
    }

    char output_dir[2048];
    snprintf(output_dir, sizeof output_dir, "%s", outdir);

    if (output_dir[0] == '\0') {
        fprintf(stderr, "Invalid output directory\n");
        free(source);
        free(base);
        return 1;
    }

    char command[4096];
    snprintf(command, sizeof command, "%s/%s_%s_%s_%s.txt",
             output_dir, base, STUDENT_FIRST, STUDENT_LAST, STUDENT_ID);

    /*
     * Create the output directory if necessary. If mkdir fails,
     * check whether the path already exists as a directory.
     */
    if (mkdir(output_dir, 0755) != 0) {
        struct stat st;

        if (stat(output_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Cannot create output directory: %s\n",
                    output_dir);
            free(source);
            free(base);
            return 1;
        }
    }

    FILE *out = fopen(command, "w");

    if (out == NULL) {
        fprintf(stderr, "Cannot write output file: %s\n", command);
        free(source);
        free(base);
        return 1;
    }

    size_t pos = 0;
    size_t length = strlen(source);

    while (pos < length) {
        size_t end = pos;

        while (end < length && source[end] != '\n')
            end++;

        size_t first = pos;

        while (first < end &&
               (source[first] == ' ' ||
                source[first] == '\t' ||
                source[first] == '\r')) {
            first++;
        }

        /* Ignore blank lines and lines whose first non-space is '#'. */
        if (first < end && source[first] != '#') {
            char saved = source[end];
            source[end] = '\0';

            p = source + pos;
            failed = 0;

            long long result = expression();
            spaces();

            if (failed || *p != '\0') {
                fprintf(out, "ERROR:%zu\n",
                        (size_t)(p - source) + 1);
                fclose(out);
                free(source);
                free(base);
                return 0;
            }

            fprintf(out, "%lld\n", result);
            source[end] = saved;
        }

        pos = end < length ? end + 1 : length;
    }

    fclose(out);
    free(source);
    free(base);

    return 0;
}
