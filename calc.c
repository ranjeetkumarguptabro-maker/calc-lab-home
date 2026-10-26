/*
 * Ranjeet Kumar Gupta 251ADB005
 * https://github.com/ranjeetkumarguptabro-maker/calc-lab-home
 * Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
 * Resources used: Gemini (Google).
 * Gemini conversation: https://share.gemini.google/7qfXVt5XmPBr
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <dirent.h>
#define STUDENT_FIRST "ranjeet-kumar"
#define STUDENT_LAST "gupta"
#define STUDENT_ID "251ADB005"
typedef struct {
    long long integer;
    double real;
    int is_float;
} Value;
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
static Value make_integer(long long n)
{
    Value v;
    v.integer = n;
    v.real = (double)n;
    v.is_float = 0;
    return v;
}
static Value make_float(double n)
{
    Value v;
    v.integer = 0;
    v.real = n;
    v.is_float = 1;
    return v;
}
static double as_double(Value v)
{
    return v.is_float ? v.real : (double)v.integer;
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
/* Decimal literals require digits on both sides of the decimal point. */
static Value number(void)
{
    spaces();
    const char *start = p;
    if (!isdigit((unsigned char)*p)) {
        fail_at(p);
        return make_integer(0);
    }
    while (isdigit((unsigned char)*p))
        p++;
    int is_float = 0;
    if (*p == '.') {
        is_float = 1;
        p++;
        if (!isdigit((unsigned char)*p)) {
            fail_at(p - 1);
            return make_integer(0);
        }
        while (isdigit((unsigned char)*p))
            p++;
    }
    size_t length = (size_t)(p - start);
    char *token = malloc(length + 1);
    if (token == NULL) {
        fail_at(start);
        return make_integer(0);
    }
    memcpy(token, start, length);
    token[length] = '\0';
    Value value;
    if (is_float) {
        errno = 0;
        char *end = NULL;
        double d = strtod(token, &end);
        if (errno == ERANGE || end == token ||
            *end != '\0' || !isfinite(d)) {
            fail_at(start);
            free(token);
            return make_integer(0);
        }
        value = make_float(d);
    } else {
        long long n = 0;
        for (size_t i = 0; i < length; i++) {
            int digit = token[i] - '0';
            if (n > (LLONG_MAX - digit) / 10) {
                fail_at(start);
                free(token);
                return make_integer(0);
            }
            n = n * 10 + digit;
        }
        value = make_integer(n);
    }
    free(token);
    return value;
}
static Value expression(void);
static Value primary(void)
{
    spaces();
    if (*p == '(') {
        p++;
        Value value = expression();
        if (failed)
            return make_integer(0);
        spaces();
        if (*p != ')') {
            fail_at(p);
            return make_integer(0);
        }
        p++;
        return value;
    }
    return number();
}
/* Exponentiation is right-associative: 2 ** 3 ** 2 means 2 ** (3 ** 2). */
static Value power(void)
{
    Value base = primary();
    if (failed)
        return make_integer(0);
    spaces();
    if (p[0] != '*' || p[1] != '*')
        return base;
    const char *op_start = p;
    p += 2;
    spaces();
    const char *rhs_start = p;
    Value exponent = power();
    if (failed)
        return make_integer(0);
    if (!exponent.is_float && exponent.integer < 0) {
        fail_at(rhs_start);
        return make_integer(0);
    }
    if (base.is_float || exponent.is_float) {
        double result = pow(as_double(base), as_double(exponent));
        if (!isfinite(result)) {
            fail_at(op_start);
            return make_integer(0);
        }
        return make_float(result);
    }
    /* Integer exponentiation by squaring reduces repeated multiplications. */
    long long result = 1;
    long long factor = base.integer;
    unsigned long long remaining = (unsigned long long)exponent.integer;
    while (remaining > 0) {
        if (remaining & 1ULL) {
            if (mul_overflow(result, factor, &result)) {
                fail_at(op_start);
                return make_integer(0);
            }
        }
        remaining >>= 1;
        if (remaining > 0 &&
            mul_overflow(factor, factor, &factor)) {
            fail_at(op_start);
            return make_integer(0);
        }
    }
    return make_integer(result);
}
/* Multiplication and division have lower precedence than exponentiation. */
static Value term(void)
{
    Value value = power();
    while (!failed) {
        spaces();
        char op = *p;
        if (op != '*' && op != '/')
            break;
        if (op == '*' && p[1] == '*')
            break;
        p++;
        spaces();
        const char *rhs_start = p;
        Value rhs = power();
        if (failed)
            break;
        if (value.is_float || rhs.is_float) {
            double divisor = as_double(rhs);
            if (op == '/' && divisor == 0.0) {
                fail_at(rhs_start);
                break;
            }
            double result = op == '*'
                ? as_double(value) * as_double(rhs)
                : as_double(value) / divisor;
            if (!isfinite(result)) {
                fail_at(rhs_start);
                break;
            }
            value = make_float(result);
        } else {
            long long result;
            if (op == '*') {
                if (mul_overflow(value.integer, rhs.integer, &result)) {
                    fail_at(rhs_start);
                    break;
                }
            } else {
                if (rhs.integer == 0) {
                    fail_at(rhs_start);
                    break;
                }
                if (value.integer == LLONG_MIN && rhs.integer == -1) {
                    fail_at(rhs_start);
                    break;
                }
                result = value.integer / rhs.integer;
            }
            value = make_integer(result);
        }
    }
    return value;
}
static Value expression(void)
{
    Value value = term();
    while (!failed) {
        spaces();
        char op = *p;
        if (op != '+' && op != '-')
            break;
        p++;
        spaces();
        const char *rhs_start = p;
        Value rhs = term();
        if (failed)
            break;
        if (value.is_float || rhs.is_float) {
            double result = op == '+'
                ? as_double(value) + as_double(rhs)
                : as_double(value) - as_double(rhs);
            if (!isfinite(result)) {
                fail_at(rhs_start);
                break;
            }
            value = make_float(result);
        } else {
            long long result;
            int overflow = op == '+'
                ? add_overflow(value.integer, rhs.integer, &result)
                : sub_overflow(value.integer, rhs.integer, &result);
            if (overflow) {
                fail_at(rhs_start);
                break;
            }
            value = make_integer(result);
        }
    }
    return value;
}
/* Read the complete file so errors include earlier lines and newlines. */
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
    if (length < 0 || (unsigned long)length >= (size_t)-1 ||
        fseek(f, 0, SEEK_SET) != 0) {
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
static int process_file(const char *input, const char *outdir)
{
    char *base = basename_copy(input);
    if (base == NULL) {
        fprintf(stderr, "Out of memory\n");
        return 1;
    }
    size_t base_len = strlen(base);
    if (base_len >= 4 &&
        strcmp(base + base_len - 4, ".txt") == 0)
        base[base_len - 4] = '\0';
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
    size_t capacity = 16;
    size_t count = 0;
    Value *results = malloc(capacity * sizeof *results);
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
        /* Only lines starting with # after whitespace are comments. */
        if (first < end && source[first] != '#') {
            char saved = source[end];
            source[end] = '\0';
            p = source + pos;
            failed = 0;
            error_at = NULL;
            Value result = expression();
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
                Value *larger = realloc(results, capacity * sizeof *results);
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
        for (size_t i = 0; i < count; i++) {
            if (results[i].is_float)
                fprintf(out, "%.15g\n", results[i].real);
            else
                fprintf(out, "%lld\n", results[i].integer);
        }
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
static int ends_with_txt(const char *name)
{
    size_t length = strlen(name);
    return length >= 4 && strcmp(name + length - 4, ".txt") == 0;
}
static int ensure_directory(const char *path)
{
    struct stat st;
    if (path == NULL || path[0] == '\0') {
        fprintf(stderr, "Invalid output directory\n");
        return 0;
    }
    if (mkdir(path, 0755) == 0)
        return 1;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return 1;
    fprintf(stderr, "Cannot create output directory: %s\n", path);
    return 0;
}
int main(int argc, char **argv)
{
    const char *input = NULL;
    const char *input_dir = NULL;
    const char *outdir = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 ||
            strcmp(argv[i], "--output-dir") == 0) {
            if (++i >= argc || outdir != NULL || argv[i][0] == '\0') {
                fprintf(stderr, "Invalid output directory argument\n");
                return 1;
            }
            outdir = argv[i];
        } else if (strcmp(argv[i], "-d") == 0 ||
                   strcmp(argv[i], "--dir") == 0) {
            if (++i >= argc || input_dir != NULL || argv[i][0] == '\0') {
                fprintf(stderr, "Invalid input directory argument\n");
                return 1;
            }
            input_dir = argv[i];
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
    if ((input == NULL && input_dir == NULL) ||
        (input != NULL && input_dir != NULL)) {
        fprintf(stderr,
                "Usage: calc [-o OUTDIR] input.txt\n"
                "       calc [-d DIR | --dir DIR] [-o OUTDIR]\n");
        return 1;
    }
    char default_dir[1024];
    if (outdir == NULL) {
        char *base = basename_copy(input_dir != NULL ? input_dir : input);
        if (base == NULL) {
            fprintf(stderr, "Out of memory\n");
            return 1;
        }
        if (input_dir == NULL) {
            size_t len = strlen(base);
            if (len >= 4 && strcmp(base + len - 4, ".txt") == 0)
                base[len - 4] = '\0';
        }
        int n = snprintf(default_dir, sizeof default_dir,
                         "%s_%s_%s", base, STUDENT_FIRST, STUDENT_ID);
        free(base);
        if (n < 0 || (size_t)n >= sizeof default_dir) {
            fprintf(stderr, "Output directory name is too long\n");
            return 1;
        }
        outdir = default_dir;
    }
    if (input != NULL)
        return process_file(input, outdir);
    struct stat dir_stat;
    if (stat(input_dir, &dir_stat) != 0 || !S_ISDIR(dir_stat.st_mode)) {
        fprintf(stderr, "Cannot open input directory: %s\n", input_dir);
        return 1;
    }
    DIR *dir = opendir(input_dir);
    if (dir == NULL) {
        fprintf(stderr, "Cannot open input directory: %s\n", input_dir);
        return 1;
    }
    /* Collect paths before processing so output files are not included. */
    char **paths = NULL;
    size_t count = 0;
    size_t capacity = 0;
    int had_error = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!ends_with_txt(entry->d_name))
            continue;
        size_t needed = strlen(input_dir) + strlen(entry->d_name) + 2;
        char *path = malloc(needed);
        if (path == NULL) {
            fprintf(stderr, "Out of memory\n");
            had_error = 1;
            break;
        }
        int n = snprintf(path, needed, "%s%s%s", input_dir,
                         input_dir[strlen(input_dir) - 1] == '/' ? "" : "/",
                         entry->d_name);
        if (n < 0 || (size_t)n >= needed) {
            free(path);
            fprintf(stderr, "Input path is too long\n");
            had_error = 1;
            break;
        }
        struct stat file_stat;
        if (stat(path, &file_stat) != 0 || !S_ISREG(file_stat.st_mode)) {
            free(path);
            continue;
        }
        if (count == capacity) {
            size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
            if (new_capacity < capacity ||
                new_capacity > (size_t)-1 / sizeof *paths) {
                free(path);
                fprintf(stderr, "Too many input files\n");
                had_error = 1;
                break;
            }
            char **new_paths = realloc(paths, new_capacity * sizeof *paths);
            if (new_paths == NULL) {
                free(path);
                fprintf(stderr, "Out of memory\n");
                had_error = 1;
                break;
            }
            paths = new_paths;
            capacity = new_capacity;
        }
        paths[count++] = path;
    }
    if (closedir(dir) != 0) {
        fprintf(stderr, "Failed to close input directory\n");
        had_error = 1;
    }
    if (!ensure_directory(outdir))
        had_error = 1;
    for (size_t i = 0; i < count; i++) {
        if (process_file(paths[i], outdir) != 0)
            had_error = 1;
        free(paths[i]);
    }
    free(paths);
    return had_error ? 1 : 0;
}