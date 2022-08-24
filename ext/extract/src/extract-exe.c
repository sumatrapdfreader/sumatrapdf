/* Command-line programme for extract_ API. */

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../include/extract.h"
#include "../include/extract_alloc.h"

#include "memento.h"
#include "outf.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Error-detecting equivalent to *out = argv[++i].
*/
static int arg_next_string(char** argv, int argc, int* i, const char** out)
{
    if (*i + 1 >= argc) {
        printf("Expected arg after: %s\n", argv[*i]);
        errno = EINVAL;
        return -1;
    }
    *i += 1;
    *out = argv[*i];
    return 0;
}

/* Error-detecting equivalent to *out = atoi(argv[++i]).
*/
static int arg_next_int(char** argv, int argc, int* i, int* out)
{
    if (*i + 1 >= argc) {
        printf("Expected integer arg after: %s\n", argv[*i]);
        errno = EINVAL;
        return -1;
    }
    *i += 1;
    *out = atoi(argv[*i]);
    return 0;
}

static void* s_realloc(void* state, void* prev, size_t size)
{
    assert(state == (void*) 123);
    return realloc(prev, size);
}

int main(int argc, char** argv)
{
    int e = -1;
    const char* docx_out_path       = NULL;
    const char* input_path          = NULL;
    const char* docx_template_path  = NULL;
    const char* content_path        = NULL;
    int         preserve_dir        = 0;
    int         spacing             = 1;
    int         rotation            = 1;
    int         autosplit           = 0;
    int         images              = 1;
    int         alloc_stats         = 0;
    int         format              = -1;
    int         i;

    extract_alloc_t*    alloc = NULL;
    extract_buffer_t*   out_buffer = NULL;
    extract_buffer_t*   intermediate = NULL;
    extract_t*          extract = NULL;

    /* Create an allocator so we test the allocation code. */
    if (extract_alloc_create(s_realloc, (void*) 123, &alloc))
    {
        assert(0);
    }

    for (i=1; i<argc; ++i) {
        const char* arg = argv[i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            printf(
                    "Converts intermediate data from mupdf or gs into a docx file.\n"
                    "\n"
                    "We require a file containing XML output from one of these commands:\n"
                    "    mutool draw -F xmltext ...\n"
                    "    gs -sDEVICE=txtwrite -dTextFormat=4 ...\n"
                    "\n"
                    "We also requires a template docx file.\n"
                    "\n"
                    "Args:\n"
                    "    --alloc-exp-min <bytes>\n"
                    "        Internal: set exponential allocation with minimum alloc size.\n"
                    "    --autosplit 0|1\n"
                    "        If 1, we initially split spans when y coordinate changes. This\n"
                    "        stresses our handling of spans when input is from mupdf.\n"
                    "    -f odt | docx\n"
                    "        Sets output format. Required.\n"
                    "    -i <intermediate-path>\n"
                    "        Path of XML file containing intermediate text spans.\n"
                    "    -o <docx-path>\n"
                    "        If specified, we generate the specified docx file.\n"
                    "    --o-content <path>\n"
                    "        If specified, we write raw docx content to <path>; this is the\n"
                    "        text that we embed inside the template word/document.xml file\n"
                    "        when generating the docx file.\n"
                    "    -p 0|1\n"
                    "        If 1 and -t <docx-template> is specified, we preserve the\n"
                    "        uncompressed <docx-path>.lib/ directory.\n"
                    "    -r 0|1\n"
                    "       If 1, we we output rotated text inside a rotated drawing. Otherwise\n"
                    "       output text is always horizontal.\n"
                    "    -s 0|1\n"
                    "        If 1, we insert extra vertical space between paragraphs and extra\n"
                    "        vertical space between paragraphs that had different ctm matrices\n"
                    "        in the original document.\n"
                    "    -t <docx-template>\n"
                    "        If specified we use <docx-template> as template. Otheerwise we use\n"
                    "        an internal template.\n"
                    "    -v <verbose>\n"
                    "        Set verbose level.\n"
                    "    -v-alloc\n"
                    "        Show alloc stats.\n"
                    );
            if (i + 1 == argc) {
                e = 0;
                goto end;
            }
        }
        else if (!strcmp(arg, "--alloc-exp-min")) {
            int size;
            if (arg_next_int(argv, argc, &i, &size)) goto end;
            outf("Calling alloc_set_min_alloc_size(%i)", size);
            extract_exp_min(extract, size);
        }
        else if (!strcmp(arg, "--autosplit")) {
            if (arg_next_int(argv, argc, &i, &autosplit)) goto end;
        }
        else if (!strcmp(arg, "-f")) {
            const char* format_name;
            if (arg_next_string(argv, argc, &i, &format_name)) goto end;
            if (!strcmp(format_name, "odt")) format = extract_format_ODT;
            else if (!strcmp(format_name, "docx")) format = extract_format_DOCX;
            else if (!strcmp(format_name, "html")) format = extract_format_HTML;
            else
            {
                printf("-f value should be 'odt' or 'docx', not '%s'.\n", format_name);
                errno = EINVAL;
                goto end;
            }
        }
        else if (!strcmp(arg, "-i")) {
            if (arg_next_string(argv, argc, &i, &input_path)) goto end;
        }
        else if (!strcmp(arg, "-o")) {
            if (arg_next_string(argv, argc, &i, &docx_out_path)) goto end;
        }
        else if (!strcmp(arg, "--o-content")) {
            if (arg_next_string(argv, argc, &i, &content_path)) goto end;
        }
        else if (!strcmp(arg, "-p")) {
            if (arg_next_int(argv, argc, &i, &preserve_dir)) goto end;
        }
        else if (!strcmp(arg, "-r")) {
            if (arg_next_int(argv, argc, &i, &rotation)) goto end;
        }
        else if (!strcmp(arg, "-s")) {
            if (arg_next_int(argv, argc, &i, &spacing)) goto end;
        }
        else if (!strcmp(arg, "-t")) {
            if (arg_next_string(argv, argc, &i, &docx_template_path)) goto end;
        }
        else if (!strcmp(arg, "-v")) {
            int verbose;
            if (arg_next_int(argv, argc, &i, &verbose)) goto end;
            extract_outf_verbose_set(verbose);
            outf("Have changed verbose to %i", verbose);
        }
        else if (!strcmp(arg, "--v-alloc")) {
            if (arg_next_int(argv, argc, &i, &alloc_stats)) goto end;
        }
        else {
            printf("Unrecognised arg: '%s'\n", arg);
            errno = EINVAL;
            goto end;
        }

        assert(i < argc);
    }

    if (format == -1)
    {
        printf("'-f odt | docx' must be specified\n");
        errno = EINVAL;
        goto end;
    }

    if (!input_path) {
        printf("-i <input-path> not specified.\n");
        errno = EINVAL;
        goto end;
    }

    if (extract_buffer_open_file(alloc, input_path, 0 /*writable*/, &intermediate)) {
        printf("Failed to open intermediate file: %s\n", input_path);
        goto end;
    }

    if (extract_begin(alloc, format, &extract)) goto end;
    if (extract_read_intermediate(extract, intermediate)) goto end;

    if (extract_process(extract, spacing, rotation, images)) goto end;

    if (content_path) {
        if (extract_buffer_open_file(alloc, content_path, 1 /*writable*/, &out_buffer)) goto end;
        if (extract_write_content(extract, out_buffer)) goto end;
        if (extract_buffer_close(&out_buffer)) goto end;
    }
    if (docx_out_path) {
        if (docx_template_path) {
            if (extract_write_template(
                    extract,
                    docx_template_path,
                    docx_out_path,
                    preserve_dir
                    )) {
                printf("Failed to create docx file: %s\n", docx_out_path);
                goto end;
            }
        }
        else {
            if (extract_buffer_open_file(alloc, docx_out_path, 1 /*writable*/, &out_buffer)) goto end;
            if (extract_write(extract, out_buffer)) {
                printf("Failed to create docx file: %s\n", docx_out_path);
                goto end;
            }
            if (extract_buffer_close(&out_buffer)) goto end;
        }
    }

    e = 0;
    end:

    extract_buffer_close(&intermediate);
    extract_buffer_close(&out_buffer);
    extract_end(&extract);

    if (e) {
        printf("Failed (errno=%i): %s\n", errno, strerror(errno));
        return 1;
    }

    extract_internal_end();

    if (alloc_stats) {
        extract_alloc_stats_t* stats = extract_alloc_stats(alloc);
        printf("Alloc stats: num_malloc=%i num_realloc=%i num_free=%i num_libc_realloc=%i\n",
                stats->num_malloc,
                stats->num_realloc,
                stats->num_free,
                stats->num_libc_realloc
                );
    }

    extract_alloc_destroy(&alloc);
    assert(alloc == NULL);

    printf("Finished.\n");
    return 0;
}
