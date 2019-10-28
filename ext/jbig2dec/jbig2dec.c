/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#else
# include "getopt.h"
#endif

#include "os_types.h"
#include "sha1.h"

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_image.h"
#include "jbig2_image_rw.h"

typedef enum {
    usage, dump, render
} jbig2dec_mode;

typedef enum {
    jbig2dec_format_none,
    jbig2dec_format_jbig2,
    jbig2dec_format_pbm,
#ifdef HAVE_LIBPNG
    jbig2dec_format_png,
#endif
} jbig2dec_format;

typedef struct {
    jbig2dec_mode mode;
    int verbose, hash, embedded;
    SHA1_CTX *hash_ctx;
    char *output_filename;
    jbig2dec_format output_format;
    char *last_message;
    Jbig2Severity severity;
    char *type;
    long repeats;
} jbig2dec_params_t;

static int print_version(void);
static int print_usage(void);

/* page hashing functions */
static void
hash_init(jbig2dec_params_t *params)
{
    params->hash_ctx = (SHA1_CTX *) malloc(sizeof(SHA1_CTX));
    if (params->hash_ctx == NULL) {
        fprintf(stderr, "unable to allocate hash state\n");
        params->hash = 0;
        return;
    } else {
        SHA1_Init(params->hash_ctx);
    }
}

static void
hash_image(jbig2dec_params_t *params, Jbig2Image *image)
{
    unsigned int N = image->stride * image->height;

    SHA1_Update(params->hash_ctx, image->data, N);
}

static void
hash_print(jbig2dec_params_t *params, FILE *out)
{
    unsigned char md[SHA1_DIGEST_SIZE];
    char digest[2 * SHA1_DIGEST_SIZE + 1];
    int i;

    SHA1_Final(params->hash_ctx, md);
    for (i = 0; i < SHA1_DIGEST_SIZE; i++) {
        snprintf(&(digest[2 * i]), 3, "%02x", md[i]);
    }
    fprintf(out, "%s", digest);
}

static void
hash_free(jbig2dec_params_t *params)
{
    free(params->hash_ctx);
    params->hash_ctx = NULL;
}

static int
set_output_format(jbig2dec_params_t *params, const char *format)
{
#ifdef HAVE_LIBPNG
    /* this should really by strncasecmp()
       TODO: we need to provide our own for portability */
    if (!strncmp(format, "png", 3) || !strncmp(format, "PNG", 3)) {
        params->output_format = jbig2dec_format_png;
        return 0;
    }
#endif
    /* default to pbm */
    params->output_format = jbig2dec_format_pbm;

    return 0;
}

static int
parse_options(int argc, char *argv[], jbig2dec_params_t *params)
{
    static struct option long_options[] = {
        {"version", 0, NULL, 'V'},
        {"help", 0, NULL, 'h'},
        {"quiet", 0, NULL, 'q'},
        {"verbose", 2, NULL, 'v'},
        {"dump", 0, NULL, 'd'},
        {"hash", 0, NULL, 'm'},
        {"output", 1, NULL, 'o'},
        {"format", 1, NULL, 't'},
        {"embedded", 0, NULL, 'e'},
        {NULL, 0, NULL, 0}
    };
    int option_idx = 1;
    int option;

    while (1) {
        option = getopt_long(argc, argv, "Vh?qv:do:t:e", long_options, &option_idx);
        if (option == -1)
            break;

        switch (option) {
        case 0:                /* unknown long option */
            if (!params->verbose)
                fprintf(stdout, "unrecognized option: --%s\n", long_options[option_idx].name);
            break;
        case 'q':
            params->verbose = 0;
            break;
        case 'v':
            if (optarg)
                params->verbose = atoi(optarg);
            else
                params->verbose = 2;
            break;
        case 'h':
        case '?':
            params->mode = usage;
            break;
        case 'V':
            /* the GNU Coding Standards suggest --version
               should override all other options */
            print_version();
            exit(0);
            break;
        case 'd':
            params->mode = dump;
            break;
        case 'm':
            params->hash = 1;
            break;
        case 'o':
            params->output_filename = strdup(optarg);
            break;
        case 't':
            set_output_format(params, optarg);
            break;
        case 'e':
            params->embedded = 1;
            break;
        default:
            if (!params->verbose)
                fprintf(stdout, "unrecognized option: -%c\n", option);
            break;
        }
    }
    return (optind);
}

static int
print_version(void)
{
    fprintf(stdout, "jbig2dec %d.%d\n", JBIG2_VERSION_MAJOR, JBIG2_VERSION_MINOR);
    return 0;
}

static int
print_usage(void)
{
    fprintf(stderr,
            "Usage: jbig2dec [options] <file.jbig2>\n"
            "   or  jbig2dec [options] <global_stream> <page_stream>\n"
            "\n"
            "  When invoked with a single file, it attempts to parse it as\n"
            "  a normal jbig2 file. Invoked with two files, it treats the\n"
            "  first as the global segments, and the second as the segment\n"
            "  stream for a particular page. This is useful for examining\n"
            "  embedded streams.\n"
            "\n"
            "  available options:\n"
            "    -h --help      this usage summary\n"
            "    -q --quiet     suppress diagnostic output\n"
            "    -v --verbose   set the verbosity level\n"
            "    -d --dump      print the structure of the jbig2 file\n"
            "                   rather than explicitly decoding\n"
            "       --version   program name and version information\n"
            "       --hash      print a hash of the decoded document\n"
            "    -e --embedded  expect embedded bit stream without file header\n"
            "    -o <file>      send decoded output to <file>\n"
            "                   Defaults to the the input with a different\n"
            "                   extension. Pass '-' for stdout.\n"
            "    -t <type>      force a particular output file format\n"
#ifdef HAVE_LIBPNG
            "                   supported options are 'png' and 'pbm'\n"
#else
            "                   the only supported option is 'pbm'\n"
#endif
            "\n");

    return 1;
}

static void
error_callback(void *error_callback_data, const char *buf, Jbig2Severity severity, int32_t seg_idx)
{
    jbig2dec_params_t *params = (jbig2dec_params_t *) error_callback_data;
    char *type;
    char segment[22];
    int len;
    char *message;

    switch (severity) {
    case JBIG2_SEVERITY_DEBUG:
        if (params->verbose < 3)
            return;
        type = "DEBUG";
        break;
    case JBIG2_SEVERITY_INFO:
        if (params->verbose < 2)
            return;
        type = "info";
        break;
    case JBIG2_SEVERITY_WARNING:
        if (params->verbose < 1)
            return;
        type = "WARNING";
        break;
    case JBIG2_SEVERITY_FATAL:
        type = "FATAL ERROR";
        break;
    default:
        type = "unknown message";
        break;
    }
    if (seg_idx == -1)
        segment[0] = '\0';
    else
        snprintf(segment, sizeof(segment), "(segment 0x%02x)", seg_idx);

    len = snprintf(NULL, 0, "jbig2dec %s %s %s", type, buf, segment);
    if (len < 0) {
        return;
    }

    message = malloc(len + 1);
    if (message == NULL) {
        return;
    }

    len = snprintf(message, len + 1, "jbig2dec %s %s %s", type, buf, segment);
    if (len < 0)
    {
        free(message);
        return;
    }

    if (params->last_message != NULL && strcmp(message, params->last_message)) {
        if (params->repeats > 1)
            fprintf(stderr, "jbig2dec %s last message repeated %ld times\n", params->type, params->repeats);
        fprintf(stderr, "%s\n", message);
        free(params->last_message);
        params->last_message = message;
        params->severity = severity;
        params->type = type;
        params->repeats = 0;
    } else if (params->last_message != NULL) {
        params->repeats++;
        if (params->repeats % 1000000 == 0)
            fprintf(stderr, "jbig2dec %s last message repeated %ld times so far\n", params->type, params->repeats);
        free(message);
    } else if (params->last_message == NULL) {
        fprintf(stderr, "%s\n", message);
        params->last_message = message;
        params->severity = severity;
        params->type = type;
        params->repeats = 0;
    }
}

static void
flush_errors(jbig2dec_params_t *params)
{
    if (params->repeats > 1) {
        fprintf(stderr, "jbig2dec last message repeated %ld times\n", params->repeats);
    }
}

static char *
make_output_filename(const char *input_filename, const char *extension)
{
    char *output_filename;
    const char *c, *e;
    int len;

    if (extension == NULL) {
        fprintf(stderr, "no filename extension; cannot create output filename!\n");
        exit(1);
    }

    if (input_filename == NULL)
        c = "out";
    else {
        /* strip any leading path */
        c = strrchr(input_filename, '/');       /* *nix */
        if (c == NULL)
            c = strrchr(input_filename, '\\');  /* win32/dos */
        if (c != NULL)
            c++;                /* skip the path separator */
        else
            c = input_filename; /* no leading path */
    }

    /* make sure we haven't just stripped the last character */
    if (*c == '\0')
        c = "out";

    /* strip the extension */
    len = strlen(c);
    e = strrchr(c, '.');
    if (e != NULL)
        len -= strlen(e);

    /* allocate enough space for the base + ext */
    output_filename = (char *)malloc(len + strlen(extension) + 1);
    if (output_filename == NULL) {
        fprintf(stderr, "failed to allocate memory for output filename\n");
        exit(1);
    }

    strncpy(output_filename, c, len);
    strncpy(output_filename + len, extension, strlen(extension));
    *(output_filename + len + strlen(extension)) = '\0';

    /* return the new string */
    return (output_filename);
}

static int
write_page_image(jbig2dec_params_t *params, FILE *out, Jbig2Image *image)
{
    switch (params->output_format) {
#ifdef HAVE_LIBPNG
    case jbig2dec_format_png:
        return jbig2_image_write_png(image, out);
#endif
    case jbig2dec_format_pbm:
        return jbig2_image_write_pbm(image, out);
    default:
        fprintf(stderr, "unsupported output format.\n");
        return 1;
    }

    return 0;
}

static int
write_document_hash(jbig2dec_params_t *params)
{
    FILE *out;

    if (!strncmp(params->output_filename, "-", 2)) {
        out = stderr;
    } else {
        out = stdout;
    }

    fprintf(out, "Hash of decoded document: ");
    hash_print(params, out);
    fprintf(out, "\n");

    return 0;
}

int
main(int argc, char **argv)
{
    FILE *f = NULL, *f_page = NULL;
    Jbig2Ctx *ctx = NULL;
    uint8_t buf[4096];
    jbig2dec_params_t params;
    int filearg;
    int result = 1;
    int code;

    /* set defaults */
    params.mode = render;
    params.verbose = 1;
    params.hash = 0;
    params.output_filename = NULL;
    params.output_format = jbig2dec_format_none;
    params.embedded = 0;
    params.last_message = NULL;
    params.repeats = 0;

    filearg = parse_options(argc, argv, &params);

    if (params.hash)
        hash_init(&params);

    switch (params.mode) {
    case usage:
        print_usage();
        exit(0);
        break;
    case dump:
        fprintf(stderr, "Sorry, segment dump not yet implemented\n");
        break;
    case render:

        if ((argc - filearg) == 1) {
            /* only one argument--open as a jbig2 file */
            char *fn = argv[filearg];

            f = fopen(fn, "rb");
            if (f == NULL) {
                fprintf(stderr, "error opening %s\n", fn);
                goto cleanup;
            }
        } else if ((argc - filearg) == 2) {
            /* two arguments open as separate global and page streams */
            char *fn = argv[filearg];
            char *fn_page = argv[filearg + 1];

            f = fopen(fn, "rb");
            if (f == NULL) {
                fprintf(stderr, "error opening %s\n", fn);
                goto cleanup;
            }

            f_page = fopen(fn_page, "rb");
            if (f_page == NULL) {
                fclose(f);
                fprintf(stderr, "error opening %s\n", fn_page);
                goto cleanup;
            }
        } else {
            /* any other number of arguments */
            result = print_usage();
            goto cleanup;
        }

        ctx = jbig2_ctx_new(NULL, (Jbig2Options)(f_page != NULL || params.embedded ? JBIG2_OPTIONS_EMBEDDED : 0), NULL, error_callback, &params);
        if (ctx == NULL) {
            fclose(f);
            if (f_page)
                fclose(f_page);
            goto cleanup;
        }

        /* pull the whole file/global stream into memory */
        for (;;) {
            int n_bytes = fread(buf, 1, sizeof(buf), f);

            if (n_bytes <= 0)
                break;
            if (jbig2_data_in(ctx, buf, n_bytes))
                break;
        }
        fclose(f);

        /* if there's a local page stream read that in its entirety */
        if (f_page != NULL) {
            Jbig2GlobalCtx *global_ctx = jbig2_make_global_ctx(ctx);

            ctx = jbig2_ctx_new(NULL, JBIG2_OPTIONS_EMBEDDED, global_ctx, error_callback, &params);
            if (ctx != NULL) {
                for (;;) {
                    int n_bytes = fread(buf, 1, sizeof(buf), f_page);

                    if (n_bytes <= 0)
                        break;
                    if (jbig2_data_in(ctx, buf, n_bytes))
                        break;
                }
            }
            fclose(f_page);
            jbig2_global_ctx_free(global_ctx);
        }

        /* retrieve and output the returned pages */
        {
            Jbig2Image *image;
            FILE *out;

            /* always complete a page, working around streams that lack end of
            page segments: broken CVision streams, embedded streams or streams
            with parse errors. */
            code = jbig2_complete_page(ctx);
            if (code < 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, -1, "unable to complete page");
                goto cleanup;
            }

            if (params.output_filename == NULL) {
                switch (params.output_format) {
#ifdef HAVE_LIBPNG
                case jbig2dec_format_png:
                    params.output_filename = make_output_filename(argv[filearg], ".png");
                    break;
#endif
                case jbig2dec_format_pbm:
                    params.output_filename = make_output_filename(argv[filearg], ".pbm");
                    break;
                default:
                    fprintf(stderr, "unsupported output format: %d\n", params.output_format);
                    goto cleanup;
                }
            } else {
                int len = strlen(params.output_filename);

                if ((len >= 3) && (params.output_format == jbig2dec_format_none))
                    /* try to set the output type by the given extension */
                    set_output_format(&params, params.output_filename + len - 3);
            }

            if (!strncmp(params.output_filename, "-", 2)) {
                out = stdout;
            } else {
                if (params.verbose > 1)
                    fprintf(stderr, "saving decoded page as '%s'\n", params.output_filename);
                if ((out = fopen(params.output_filename, "wb")) == NULL) {
                    fprintf(stderr, "unable to open '%s' for writing\n", params.output_filename);
                    goto cleanup;
                }
            }

            /* retrieve and write out all the completed pages */
            while ((image = jbig2_page_out(ctx)) != NULL) {
                write_page_image(&params, out, image);
                if (params.hash)
                    hash_image(&params, image);
                jbig2_release_page(ctx, image);
            }

            if (out != stdout)
                fclose(out);
            if (params.hash)
                write_document_hash(&params);
        }


    }                           /* end params.mode switch */

    /* fin */
    result = 0;

cleanup:
    flush_errors(&params);
    jbig2_ctx_free(ctx);
    if (params.output_filename)
        free(params.output_filename);
    if (params.hash)
        hash_free(&params);

    return result;
}
