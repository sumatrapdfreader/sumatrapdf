/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
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

#ifdef JBIG_EXTERNAL_MEMENTO_H
#include JBIG_EXTERNAL_MEMENTO_H
#else
#include "memento.h"
#endif

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
    size_t memory_limit;
} jbig2dec_params_t;

typedef struct {
    int verbose;
    char *last_message;
    Jbig2Severity severity;
    char *type;
    long repeats;
} jbig2dec_error_callback_state_t;

typedef struct {
    Jbig2Allocator super;
    Jbig2Ctx *ctx;
    size_t memory_limit;
    size_t memory_used;
    size_t memory_peak;
} jbig2dec_allocator_t;

static int print_version(void);
static int print_usage(void);

#define ALIGNMENT 16
#define KBYTE 1024
#define MBYTE (1024 * KBYTE)

static void *jbig2dec_reached_limit(jbig2dec_allocator_t *allocator, size_t oldsize, size_t size)
{
    size_t limit_mb = allocator->memory_limit / MBYTE;
    size_t used_mb = allocator->memory_used / MBYTE;
    size_t oldsize_mb = oldsize / MBYTE;
    size_t size_mb = size / MBYTE;

    if (oldsize == 0)
        jbig2_error(allocator->ctx, JBIG2_SEVERITY_FATAL, -1, "memory: limit reached: limit: %zu (%zu Mbyte) used: %zu (%zu Mbyte) allocation: %zu (%zu Mbyte)",
            allocator->memory_limit, limit_mb,
            allocator->memory_used, used_mb,
            size, size_mb);
    else
        jbig2_error(allocator->ctx, JBIG2_SEVERITY_FATAL, -1, "memory: limit reached: limit: %zu (%zu Mbyte) used: %zu (%zu Mbyte) reallocation: %zu (%zu Mbyte) -> %zu (%zu Mbyte)",
            allocator->memory_limit, limit_mb,
            allocator->memory_used, used_mb,
            oldsize, oldsize_mb,
            size, size_mb);

    return NULL;
}

static void jbig2dec_peak(jbig2dec_allocator_t *allocator)
{
    size_t limit_mb = allocator->memory_limit / MBYTE;
    size_t peak_mb = allocator->memory_peak / MBYTE;
    size_t used_mb = allocator->memory_used / MBYTE;

    if (allocator->ctx == NULL)
        return;
    if (used_mb <= peak_mb)
        return;

    allocator->memory_peak = allocator->memory_used;

    jbig2_error(allocator->ctx, JBIG2_SEVERITY_DEBUG, JBIG2_UNKNOWN_SEGMENT_NUMBER, "memory: limit: %lu %sbyte used: %lu %sbyte, peak: %lu %sbyte",
        limit_mb > 0 ? limit_mb : allocator->memory_limit, limit_mb > 0 ? "M" : "",
        used_mb > 0 ? used_mb : allocator->memory_used, used_mb > 0 ? "M" : "",
        peak_mb > 0 ? peak_mb : allocator->memory_peak, peak_mb > 0 ? "M" : "");
}

static void *jbig2dec_alloc(Jbig2Allocator *allocator_, size_t size)
{
    jbig2dec_allocator_t *allocator = (jbig2dec_allocator_t *) allocator_;
    void *ptr;

    if (size == 0)
        return NULL;
    if (size > SIZE_MAX - ALIGNMENT)
        return NULL;

    if (size + ALIGNMENT > allocator->memory_limit - allocator->memory_used)
        return jbig2dec_reached_limit(allocator, 0, size + ALIGNMENT);

    ptr = malloc(size + ALIGNMENT);
    if (ptr == NULL)
        return NULL;
    memcpy(ptr, &size, sizeof(size));
    allocator->memory_used += size + ALIGNMENT;

    jbig2dec_peak(allocator);

    return (unsigned char *) ptr + ALIGNMENT;
}

static void jbig2dec_free(Jbig2Allocator *allocator_, void *p)
{
    jbig2dec_allocator_t *allocator = (jbig2dec_allocator_t *) allocator_;
    size_t size;

    if (p == NULL)
        return;

    memcpy(&size, (unsigned char *) p - ALIGNMENT, sizeof(size));
    allocator->memory_used -= size + ALIGNMENT;
    free((unsigned char *) p - ALIGNMENT);
}

static void *jbig2dec_realloc(Jbig2Allocator *allocator_, void *p, size_t size)
{
    jbig2dec_allocator_t *allocator = (jbig2dec_allocator_t *) allocator_;
    unsigned char *oldp;
    size_t oldsize;

    if (p == NULL)
        return jbig2dec_alloc(allocator_, size);
    if (p < (void *) ALIGNMENT)
        return NULL;

    if (size == 0) {
        jbig2dec_free(allocator_, p);
        return NULL;
    }
    if (size > SIZE_MAX - ALIGNMENT)
        return NULL;

    oldp = (unsigned char *) p - ALIGNMENT;
    memcpy(&oldsize, oldp, sizeof(oldsize));

    if (size + ALIGNMENT > allocator->memory_limit - allocator->memory_used + oldsize + ALIGNMENT)
        return jbig2dec_reached_limit(allocator, oldsize + ALIGNMENT, size + ALIGNMENT);

    p = realloc(oldp, size + ALIGNMENT);
    if (p == NULL)
        return NULL;

    allocator->memory_used -= oldsize + ALIGNMENT;
    memcpy(p, &size, sizeof(size));
    allocator->memory_used += size + ALIGNMENT;

    jbig2dec_peak(allocator);

    return (unsigned char *) p + ALIGNMENT;
}

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
    int ret;

    while (1) {
        option = getopt_long(argc, argv, "Vh?qv:do:t:eM:", long_options, &option_idx);
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
        case 'M':
            ret = sscanf(optarg, "%zu", &params->memory_limit);
            if (ret != 1)
                fprintf(stderr, "could not parse memory limit argument\n");
            break;
        default:
            if (!params->verbose)
                fprintf(stderr, "unrecognized option: -%c\n", option);
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
            "    -h --help       this usage summary\n"
            "    -q --quiet      suppress diagnostic output\n"
            "    -v --verbose    set the verbosity level\n"
            "    -d --dump       print the structure of the jbig2 file\n"
            "                    rather than explicitly decoding\n"
            "    -V --version    program name and version information\n"
            "    -m --hash       print a hash of the decoded document\n"
            "    -e --embedded   expect embedded bit stream without file header\n"
            "    -M <limit>      memory limit expressed in bytes\n"
            "    -o <file>\n"
            "    --output <file> send decoded output to <file>\n"
            "                    Defaults to the the input with a different\n"
            "                    extension. Pass '-' for stdout.\n"
            "    -t <type>\n"
            "    --format <type> force a particular output file format\n"
#ifdef HAVE_LIBPNG
            "                    supported options are 'png' and 'pbm'\n"
#else
            "                    the only supported option is 'pbm'\n"
#endif
            "\n");

    return 1;
}

static void
error_callback(void *error_callback_data, const char *message, Jbig2Severity severity, uint32_t seg_idx)
{
    jbig2dec_error_callback_state_t *state = (jbig2dec_error_callback_state_t *) error_callback_data;
    char *type;
    int ret;

    switch (severity) {
    case JBIG2_SEVERITY_DEBUG:
        if (state->verbose < 3)
            return;
        type = "DEBUG";
        break;
    case JBIG2_SEVERITY_INFO:
        if (state->verbose < 2)
            return;
        type = "info";
        break;
    case JBIG2_SEVERITY_WARNING:
        if (state->verbose < 1)
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

    if (state->last_message != NULL && !strcmp(message, state->last_message) && state->severity == severity && state->type == type) {
        state->repeats++;
        if (state->repeats % 1000000 == 0) {
            ret = fprintf(stderr, "jbig2dec %s last message repeated %ld times so far\n", state->type, state->repeats);
            if (ret < 0)
                goto printerror;
        }
    } else {
        if (state->repeats > 1) {
            ret = fprintf(stderr, "jbig2dec %s last message repeated %ld times\n", state->type, state->repeats);
            if (ret < 0)
                goto printerror;
        }

        if (seg_idx == JBIG2_UNKNOWN_SEGMENT_NUMBER)
            ret = fprintf(stderr, "jbig2dec %s %s\n", type, message);
        else
            ret = fprintf(stderr, "jbig2dec %s %s (segment 0x%08x)\n", type, message, seg_idx);
        if (ret < 0)
            goto printerror;

        state->repeats = 0;
        state->severity = severity;
        state->type = type;
        free(state->last_message);
        state->last_message = NULL;

        if (message) {
            state->last_message = strdup(message);
            if (state->last_message == NULL) {
                ret = fprintf(stderr, "jbig2dec WARNING could not duplicate message\n");
                if (ret < 0)
                    goto printerror;
            }
        }
    }

    return;

printerror:
    fprintf(stderr, "jbig2dec WARNING could not print message\n");
    state->repeats = 0;
    free(state->last_message);
    state->last_message = NULL;
}

static void
flush_errors(jbig2dec_error_callback_state_t *state)
{
    if (state->repeats > 1) {
        fprintf(stderr, "jbig2dec last message repeated %ld times\n", state->repeats);
    }
}

static char *
make_output_filename(const char *input_filename, const char *extension)
{
    char *output_filename;
    const char *c, *e;
    size_t extlen, len;

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

    extlen = strlen(extension);

    /* allocate enough space for the base + ext */
    output_filename = (char *)malloc(len + extlen + 1);
    if (output_filename == NULL) {
        fprintf(stderr, "failed to allocate memory for output filename\n");
        exit(1);
    }

    memcpy(output_filename, c, len);
    memcpy(output_filename + len, extension, extlen);
    *(output_filename + len + extlen) = '\0';

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
    jbig2dec_params_t params;
    jbig2dec_error_callback_state_t error_callback_state;
    jbig2dec_allocator_t allocator_ = { 0 };
    jbig2dec_allocator_t *allocator = &allocator_;
    Jbig2Ctx *ctx = NULL;
    FILE *f = NULL, *f_page = NULL;
    uint8_t buf[4096];
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
    params.memory_limit = 0;

    filearg = parse_options(argc, argv, &params);

    error_callback_state.verbose = params.verbose;
    error_callback_state.severity = JBIG2_SEVERITY_DEBUG;
    error_callback_state.type = NULL;
    error_callback_state.last_message = NULL;
    error_callback_state.repeats = 0;

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

        if (params.memory_limit == 0)
            allocator = NULL;
        else
        {
            allocator->super.alloc = jbig2dec_alloc;
            allocator->super.free = jbig2dec_free;
            allocator->super.realloc = jbig2dec_realloc;
            allocator->ctx = NULL;
            allocator->memory_limit = params.memory_limit;
            allocator->memory_used = 0;
            allocator->memory_peak = 0;
        }

        ctx = jbig2_ctx_new((Jbig2Allocator *) allocator, (Jbig2Options)(f_page != NULL || params.embedded ? JBIG2_OPTIONS_EMBEDDED : 0), NULL, error_callback, &error_callback_state);
        if (ctx == NULL) {
            fclose(f);
            if (f_page)
                fclose(f_page);
            goto cleanup;
        }

        if (allocator != NULL)
            allocator->ctx = ctx;

        /* pull the whole file/global stream into memory */
        for (;;) {
            int n_bytes = fread(buf, 1, sizeof(buf), f);
            if (n_bytes < 0) {
                if (f_page != NULL)
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "unable to read jbig2 global stream");
                else
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "unable to read jbig2 page stream");
            }
            if (n_bytes <= 0)
                break;

            if (jbig2_data_in(ctx, buf, (size_t) n_bytes) < 0)
            {
                if (f_page != NULL)
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "unable to process jbig2 global stream");
                else
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "unable to process jbig2 page stream");
                break;
            }
        }
        fclose(f);

        /* if there's a local page stream read that in its entirety */
        if (f_page != NULL) {
            Jbig2GlobalCtx *global_ctx = jbig2_make_global_ctx(ctx);

            ctx = jbig2_ctx_new((Jbig2Allocator *) allocator, JBIG2_OPTIONS_EMBEDDED, global_ctx, error_callback, &error_callback_state);
            if (ctx != NULL) {
                if (allocator != NULL)
                    allocator->ctx = ctx;

                for (;;) {
                    int n_bytes = fread(buf, 1, sizeof(buf), f_page);
                    if (n_bytes < 0)
                        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "unable to read jbig2 page stream");
                    if (n_bytes <= 0)
                        break;

                    if (jbig2_data_in(ctx, buf, (size_t) n_bytes) < 0)
                    {
                        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "unable to process jbig2 page stream");
                        break;
                    }
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
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "unable to complete page");
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

    if (allocator != NULL && allocator->ctx != NULL) {
        size_t limit_mb = allocator->memory_limit / MBYTE;
        size_t peak_mb = allocator->memory_peak / MBYTE;
        jbig2_error(allocator->ctx, JBIG2_SEVERITY_DEBUG, JBIG2_UNKNOWN_SEGMENT_NUMBER, "memory: limit: %lu Mbyte peak usage: %lu Mbyte", limit_mb, peak_mb);
    }

    /* fin */
    result = 0;

cleanup:
    flush_errors(&error_callback_state);
    jbig2_ctx_free(ctx);
    if (params.output_filename)
        free(params.output_filename);
    if (error_callback_state.last_message)
        free(error_callback_state.last_message);
    if (params.hash)
        hash_free(&params);

    return result;
}
