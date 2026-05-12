#include "tinyld.h"

static void usage(FILE *out)
{
    fputs("usage: tinyld [options] file...\n"
          "  -o FILE              write output to FILE\n"
          "  -L DIR               add static library search path\n"
          "  -l NAME              link libNAME.a\n"
          "  -e SYMBOL            set entry symbol\n"
          "  -r                   write relocatable object output\n"
          "  -s                   accept strip flag\n"
          "  -Ttext=ADDR          set image base address\n"
          "  --whole-archive      load every object from following archives\n"
          "  --no-whole-archive   restore archive-on-demand loading\n",
          out);
}

static void set_string(char **slot, const char *value)
{
    tcc_free(*slot);
    *slot = tcc_strdup(value);
}

static int parse_addr(const char *arg, addr_t *out)
{
    char *end;
    unsigned long long value = strtoull(arg, &end, 0);

    if (!arg[0] || *end)
        return -1;
    *out = (addr_t)value;
    return 0;
}

static int link_arg(TinyLDState *s, const char *arg)
{
    if (arg[0] == '-' && arg[1] == 'l' && arg[2])
        return tinyld_add_library(s, arg + 2);
    return tinyld_add_file(s, arg);
}

int main(int argc, char **argv)
{
    TinyLDState *s;
    const char *outfile = "a.out";
    int i, ret = 1;

    s = tinyld_new();
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (!strcmp(arg, "-o")) {
            if (++i == argc) {
                fputs("tinyld: -o needs an argument\n", stderr);
                goto out;
            }
            outfile = argv[i];
        } else if (!strncmp(arg, "-o", 2) && arg[2]) {
            outfile = arg + 2;
        } else if (!strcmp(arg, "-L")) {
            if (++i == argc) {
                fputs("tinyld: -L needs an argument\n", stderr);
                goto out;
            }
            tinyld_add_library_path(s, argv[i]);
        } else if (!strncmp(arg, "-L", 2) && arg[2]) {
            tinyld_add_library_path(s, arg + 2);
        } else if (!strcmp(arg, "-l")) {
            if (++i == argc) {
                fputs("tinyld: -l needs an argument\n", stderr);
                goto out;
            }
            if (tinyld_add_library(s, argv[i]) < 0)
                goto out;
        } else if (!strncmp(arg, "-l", 2) && arg[2]) {
            if (tinyld_add_library(s, arg + 2) < 0)
                goto out;
        } else if (!strcmp(arg, "-e") || !strcmp(arg, "--entry")) {
            if (++i == argc) {
                fprintf(stderr, "tinyld: %s needs an argument\n", arg);
                goto out;
            }
            set_string(&s->elf_entryname, argv[i]);
        } else if (!strncmp(arg, "--entry=", 8)) {
            set_string(&s->elf_entryname, arg + 8);
        } else if (!strcmp(arg, "-r") || !strcmp(arg, "--relocatable")) {
            s->output_type = TCC_OUTPUT_OBJ;
        } else if (!strncmp(arg, "-Ttext=", 7)) {
            if (parse_addr(arg + 7, &s->text_addr) < 0) {
                fprintf(stderr, "tinyld: invalid -Ttext address: %s\n", arg + 7);
                goto out;
            }
            s->has_text_addr = 1;
        } else if (!strcmp(arg, "-Ttext")) {
            if (++i == argc || parse_addr(argv[i], &s->text_addr) < 0) {
                fputs("tinyld: -Ttext needs an address\n", stderr);
                goto out;
            }
            s->has_text_addr = 1;
        } else if (!strcmp(arg, "--whole-archive")) {
            s->filetype |= AFF_WHOLE_ARCHIVE;
        } else if (!strcmp(arg, "--no-whole-archive")) {
            s->filetype &= ~AFF_WHOLE_ARCHIVE;
        } else if (!strcmp(arg, "-s") || !strcmp(arg, "--strip-all")
                   || !strcmp(arg, "--strip-debug")) {
            continue;
        } else if (!strcmp(arg, "-static") || !strcmp(arg, "-nostdlib")) {
            continue;
        } else if (!strcmp(arg, "--start-group") || !strcmp(arg, "--end-group")) {
            continue;
        } else if (!strcmp(arg, "-z")) {
            if (++i == argc) {
                fputs("tinyld: -z needs an argument\n", stderr);
                goto out;
            }
        } else if (!strncmp(arg, "-z", 2) && arg[2]) {
            continue;
        } else if (!strcmp(arg, "-v") || !strcmp(arg, "--version")) {
            printf("tinyld 0.1 (%s)\n", TINYLD_TARGET_NAME);
            ret = 0;
            goto out;
        } else if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            usage(stdout);
            ret = 0;
            goto out;
        } else if (arg[0] == '-') {
            fprintf(stderr, "tinyld: unsupported option: %s\n", arg);
            goto out;
        } else if (link_arg(s, arg) < 0) {
            goto out;
        }
    }

    if (tinyld_output_file(s, outfile) < 0 || s->nb_errors)
        goto out;
    ret = 0;

out:
    tinyld_delete(s);
    return ret;
}
