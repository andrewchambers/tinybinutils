#include "tinyld.h"

static void usage(FILE *out)
{
    fputs("usage: tinyas [options] file\n"
          "  -o FILE              write object to FILE\n"
          "  -I DIR               add preprocessor include path\n"
          "  -D NAME[=VALUE]      define preprocessor symbol\n"
          "  -U NAME              undefine preprocessor symbol\n"
          "  -x assembler         treat input as plain assembly\n"
          "  -x assembler-with-cpp preprocess before assembly\n",
          out);
}

static char *default_output_name(const char *input)
{
    const char *base = tcc_basename(input);
    const char *dot = strrchr(base, '.');
    size_t stem_len = dot ? (size_t)(dot - input) : strlen(input);
    char *out = tcc_malloc(stem_len + 3);

    memcpy(out, input, stem_len);
    strcpy(out + stem_len, ".o");
    return out;
}

int main(int argc, char **argv)
{
    TinyASState *s;
    const char *input = NULL;
    const char *outfile = NULL;
    char *owned_outfile = NULL;
    int preprocess = -1;
    int ret = 1;
    int i;

    s = tinyas_new();
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (!strcmp(arg, "-o")) {
            if (++i == argc) {
                fputs("tinyas: -o needs an argument\n", stderr);
                goto out;
            }
            outfile = argv[i];
        } else if (!strncmp(arg, "-o", 2) && arg[2]) {
            outfile = arg + 2;
        } else if (!strcmp(arg, "-I")) {
            if (++i == argc) {
                fputs("tinyas: -I needs an argument\n", stderr);
                goto out;
            }
            tinyas_add_include_path(s, argv[i]);
        } else if (!strncmp(arg, "-I", 2) && arg[2]) {
            tinyas_add_include_path(s, arg + 2);
        } else if (!strcmp(arg, "-D")) {
            if (++i == argc) {
                fputs("tinyas: -D needs an argument\n", stderr);
                goto out;
            }
            tinyas_define_symbol(s, argv[i]);
        } else if (!strncmp(arg, "-D", 2) && arg[2]) {
            tinyas_define_symbol(s, arg + 2);
        } else if (!strcmp(arg, "-U")) {
            if (++i == argc) {
                fputs("tinyas: -U needs an argument\n", stderr);
                goto out;
            }
            tinyas_undefine_symbol(s, argv[i]);
        } else if (!strncmp(arg, "-U", 2) && arg[2]) {
            tinyas_undefine_symbol(s, arg + 2);
        } else if (!strcmp(arg, "-x")) {
            if (++i == argc) {
                fputs("tinyas: -x needs an argument\n", stderr);
                goto out;
            }
            if (!strcmp(argv[i], "assembler"))
                preprocess = 0;
            else if (!strcmp(argv[i], "assembler-with-cpp"))
                preprocess = 1;
            else {
                fprintf(stderr, "tinyas: unsupported language: %s\n", argv[i]);
                goto out;
            }
        } else if (!strcmp(arg, "-c")) {
            continue;
        } else if (!strcmp(arg, "-v") || !strcmp(arg, "--version")) {
            printf("tinyas 0.1 (%s)\n", TINYLD_TARGET_NAME);
            ret = 0;
            goto out;
        } else if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            usage(stdout);
            ret = 0;
            goto out;
        } else if (arg[0] == '-') {
            fprintf(stderr, "tinyas: unsupported option: %s\n", arg);
            goto out;
        } else if (input) {
            fputs("tinyas: exactly one input file is supported\n", stderr);
            goto out;
        } else {
            input = arg;
        }
    }

    if (!input) {
        usage(stderr);
        goto out;
    }
    if (preprocess < 0) {
        const char *ext = tcc_fileextension(input);
        preprocess = !strcmp(ext, ".S");
    }
    if (!outfile)
        outfile = owned_outfile = default_output_name(input);

    if (tinyas_assemble_file(s, input, preprocess) < 0 || s->nb_errors)
        goto out;
    if (tinyas_output_file(s, outfile) < 0 || s->nb_errors)
        goto out;
    ret = 0;

out:
    tcc_free(owned_outfile);
    tinyas_delete(s);
    return ret;
}
