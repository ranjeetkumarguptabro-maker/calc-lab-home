
/*
 * Ranjeet Kumar Gupta 251ADB005
 * https://github.com/ranjeetkumarguptabro-maker/calc-lab-home
 * Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
 * Resources used: Gemini (Google); see conversation history.
 * AI (ChatGPT): helped explain recursive-descent parsing, operator precedence,
 * and error handling. Conversation history: this ChatGPT conversation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <limits.h>

#define STUDENT_FIRST "ranjeet-kumar"
#define STUDENT_LAST  "gupta"
#define STUDENT_ID    "251ADB005"

static const char *p;
static const char *error_at;
static int failed;

static void fail_at(const char *where)
{
    if (!failed) {
        failed = 1;
        error_at = where;
    }
}

static void spaces(void)
{
    while (*p == ' ' || *p == '\t' || *p == '\r')
        p++;
}

static int add_overflow(long long a, long long b, long long *result)
{
    if ((b > 0 && a > LLONG_MAX - b) ||
        (b < 0 && a < LLONG_MIN - b))
        return 1;

    *result = a + b;
    return 0;
}

static int sub_overflow(long long a, long long b, long long *result)
{
    if ((b < 0 && a > LLONG_MAX + b) ||
        (b > 0 && a < LLONG_MIN + b))
        return 1;

    *result = a - b;
    return 0;
}

static int mul_overflow(long long a, long long b, long long *result)
{
    if (a == 0 || b == 0) {
        *result = 0;
        return 0;
    }

    if ((a == -1 && b == LLONG_MIN) ||
        (b == -1 && a == LLONG_MIN))
        return 1;

    if (a > 0) {
        if (b > 0 && a > LLONG_MAX / b)
            return 1;
        if (b < 0 && b < LLONG_MIN / a)
            return 1;
    } else {
        if (b > 0 && a < LLONG_MIN / b)
            return 1;
        if (b < 0 && a < LLONG_MAX / b)
            return 1;
    }

    *result = a * b;
    return 0;
}

static long long number(void)
{
    spaces();

    const char *start = p;

    if (!isdigit((unsigned char)*p)) {
        fail_at(p);
        return 0;
    }

    long long value = 0;

    while (isdigit((unsigned char)*p)) {
        int digit = *p - '0';

        if (value > (LLONG_MAX - digit) / 10) {
            fail_at(start);
            return 0;
        }

        value = value * 10 + digit;
        p++;
    }

    return value;
}

/*
 * A term handles * and / before expression handles + and -.
 * This grammar structure gives multiplication and division higher precedence.
 */
static long long term(void)
{
    long long value = number();

    while (!failed) {
        spaces();

        char op = *p;

        if (op != '*' && op != '/')
            break;

        p++;
        spaces();

        /* Keep the position of the right operand for division-by-zero errors. */
        const char *rhs_start = p;
        long long rhs = number();

        if (failed)
            break;

        long long result;

        if (op == '*') {
            if (mul_overflow(value, rhs, &result)) {
                fail_at(rhs_start);
                break;
            }
        } else {
            if (rhs == 0) {
                fail_at(rhs_start);
                break;
            }

            if (value == LLONG_MIN && rhs == -1) {
                fail_at(rhs_start);
                break;
            }

            result = value / rhs;
        }

        value = result;
    }

    return value;
}

static long long expression(void)
{
    long long value = term();

    while (!failed) {
        spaces();

        char op = *p;

        if (op != '+' && op != '-')
            break;

        p++;
        long long rhs = term();

        if (failed)
            break;

        long long result;
        int overflow = op == '+'
            ? add_overflow(value, rhs, &result)
            : sub_overflow(value, rhs, &result);

        if (overflow) {
            fail_at(p - 1);
            break;
        }

        value = result;
    }

    return value;
}

static char *read_file(const char *path, size_t *length_out)
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
    *length_out = n;

    return text;
}

static char *basename_copy(const char *path)
{
    size_t n = strlen(path);

    while (n > 1 && path[n - 1] == '/')
        n--;

    size_t start = n;

    while (start > 0 && path[start - 1] != '/')
        start--;

    size_t len = n - start;
    char *copy = malloc(len + 1);

    if (copy != NULL) {
        memcpy(copy, path + start, len);
        copy[len] = '\0';
    }

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
        strcmp(base + base_len - 4, ".txt") == 0)
        base[base_len - 4] = '\0';

    char default_dir[1024];

    if (outdir == NULL) {
        int n = snprintf(default_dir, sizeof default_dir, "%s_%s_%s",
                         base, STUDENT_FIRST, STUDENT_ID);

        if (n < 0 || (size_t)n >= sizeof default_dir) {
            fprintf(stderr, "Output directory name is too long\n");
            free(base);
            return 1;
        }

        outdir = default_dir;
    }

    size_t source_length = 0;
    char *source = read_file(input, &source_length);

    if (source == NULL) {
        fprintf(stderr, "Cannot read input file: %s\n", input);
        free(base);
        return 1;
    }

    if (outdir[0] == '\0') {
        fprintf(stderr, "Invalid output directory\n");
        free(source);
        free(base);
        return 1;
    }

    char output_path[4096];

    int n = snprintf(output_path, sizeof output_path,
                     "%s/%s_%s_%s_%s.txt",
                     outdir, base, STUDENT_FIRST, STUDENT_LAST, STUDENT_ID);

    if (n < 0 || (size_t)n >= sizeof output_path) {
        fprintf(stderr, "Output path is too long\n");
        free(source);
        free(base);
        return 1;
    }

    if (mkdir(outdir, 0755) != 0) {
        struct stat st;

        if (stat(outdir, &st) != 0 || !S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Cannot create output directory: %s\n", outdir);
            free(source);
            free(base);
            return 1;
        }
    }

    /*
     * Save results until the entire file has been checked.
     * If a later line fails, earlier results must not be written.
     */
    size_t capacity = 16;
    size_t count = 0;
    long long *results = malloc(capacity * sizeof *results);

    if (results == NULL) {
        fprintf(stderr, "Out of memory\n");
        free(source);
        free(base);
        return 1;
    }

    size_t pos = 0;
    size_t error_pos = 0;
    int has_error = 0;

    while (pos < source_length) {
        size_t end = pos;

        while (end < source_length && source[end] != '\n')
            end++;

        size_t first = pos;

        while (first < end &&
               (source[first] == ' ' ||
                source[first] == '\t' ||
                source[first] == '\r'))
            first++;

        if (first < end && source[first] != '#') {
            char saved = source[end];
            source[end] = '\0';

            p = source + pos;
            failed = 0;
            error_at = NULL;

            long long result = expression();
            spaces();

            if (!failed && *p != '\0')
                fail_at(p);

            if (failed) {
                has_error = 1;
                error_pos = (size_t)(error_at - source) + 1;
                source[end] = saved;
                break;
            }

            source[end] = saved;

            if (count == capacity) {
                if (capacity > (size_t)-1 / 2 / sizeof *results) {
                    fprintf(stderr, "Too many results\n");
                    free(results);
                    free(source);
                    free(base);
                    return 1;
                }

                capacity *= 2;
                long long *larger =
                    realloc(results, capacity * sizeof *results);

                if (larger == NULL) {
                    fprintf(stderr, "Out of memory\n");
                    free(results);
                    free(source);
                    free(base);
                    return 1;
                }

                results = larger;
            }

            results[count++] = result;
        }

        pos = end < source_length ? end + 1 : source_length;
    }

    FILE *out = fopen(output_path, "w");

    if (out == NULL) {
        fprintf(stderr, "Cannot write output file: %s\n", output_path);
        free(results);
        free(source);
        free(base);
        return 1;
    }

    if (has_error) {
        fprintf(out, "ERROR:%zu\n", error_pos);
    } else {
        for (size_t i = 0; i < count; i++)
            fprintf(out, "%lld\n", results[i]);
    }

    int ok = !ferror(out);

    if (fclose(out) != 0)
        ok = 0;

    free(results);
    free(source);
    free(base);

    if (!ok) {
        fprintf(stderr, "Failed to write output file\n");
        return 1;
    }

    return 0;
}
