/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef PACKAGE
#define PACKAGE "jbig2dec"
#endif
#ifndef VERSION
#define VERSION "unknown-version"
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

typedef enum {
    usage,dump,render
} jbig2dec_mode;

typedef enum {
    jbig2dec_format_jbig2,
    jbig2dec_format_pbm,
    jbig2dec_format_png,
    jbig2dec_format_none
} jbig2dec_format;

typedef struct {
	jbig2dec_mode mode;
	int verbose, hash;
        SHA1_CTX *hash_ctx;
	char *output_file;
	jbig2dec_format output_format;
} jbig2dec_params_t;

static int print_version(void);
static int print_usage(void);

/* page hashing functions */
static void
hash_init(jbig2dec_params_t *params)
{
    params->hash_ctx = (SHA1_CTX*)malloc(sizeof(SHA1_CTX));
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
    char digest[2*SHA1_DIGEST_SIZE + 1];
    int i;

    SHA1_Final(params->hash_ctx, md);
    for (i = 0; i < SHA1_DIGEST_SIZE; i++) {
        snprintf(&(digest[2*i]), 3, "%02x", md[i]);
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
	params->output_format=jbig2dec_format_png;
    	return 0;
    }
#endif
    /* default to pbm */
    params->output_format=jbig2dec_format_pbm;

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
		{NULL, 0, NULL, 0}
	};
	int option_idx = 1;
	int option;

	while (1) {
		option = getopt_long(argc, argv,
			"Vh?qv:do:t:", long_options, &option_idx);
		if (option == -1) break;

		switch (option) {
			case 0:	/* unknown long option */
				if (!params->verbose) fprintf(stdout,
					"unrecognized option: --%s\n",
					long_options[option_idx].name);
					break;
			case 'q':
				params->verbose = 0;
				break;
                        case 'v':
                                if (optarg) params->verbose = atoi(optarg);
                                else params->verbose = 2;
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
				params->mode=dump;
				break;
                        case 'm':
                                params->hash = 1;
                                break;
			case 'o':
				params->output_file = strdup(optarg);
				break;
                        case 't':
                        	set_output_format(params, optarg);
                                break;
			default:
				if (!params->verbose) fprintf(stdout,
					"unrecognized option: -%c\n", option);
				break;
		}
	}
	return (optind);
}

static int
print_version (void)
{
    fprintf(stdout, "%s %s\n", PACKAGE, VERSION);
    return 0;
}

static int
print_usage (void)
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
    "    -o <file>      send decoded output to <file>\n"
    "                   Defaults to the the input with a different\n"
    "                   extension. Pass '-' for stdout.\n"
    "    -t <type>      force a particular output file format\n"
 #ifdef HAVE_LIBPNG
    "                   supported options are 'png' and 'pbm'\n"
 #else
    "                   the only supported option is 'pbm'\n"
 #endif
    "\n"
  );

  return 1;
}

static int
error_callback(void *error_callback_data, const char *buf, Jbig2Severity severity,
	       int32_t seg_idx)
{
    const jbig2dec_params_t *params = (jbig2dec_params_t *)error_callback_data;
    char *type;
    char segment[22];

    switch (severity) {
        case JBIG2_SEVERITY_DEBUG:
            if (params->verbose < 3) return 0;
            type = "DEBUG"; break;;
        case JBIG2_SEVERITY_INFO:
            if (params->verbose < 2) return 0;
            type = "info"; break;;
        case JBIG2_SEVERITY_WARNING:
            if (params->verbose < 1) return 0;
            type = "WARNING"; break;;
        case JBIG2_SEVERITY_FATAL: type = "FATAL ERROR"; break;;
        default: type = "unknown message"; break;;
    }
    if (seg_idx == -1) segment[0] = '\0';
    else snprintf(segment, sizeof(segment), "(segment 0x%02x)", seg_idx);

    fprintf(stderr, "jbig2dec %s %s %s\n", type, buf, segment);

    return 0;
}

static char *
make_output_filename(const char *input_filename, const char *extension)
{
    char *output_filename;
    const char *c, *e;
    int len;

    if (extension == NULL) {
        fprintf(stderr, "make_output_filename called with no extension!\n");
        exit (1);
    }

    if (input_filename == NULL)
      c = "out";
    else {
      /* strip any leading path */
      c = strrchr(input_filename, '/'); /* *nix */
      if (c == NULL)
        c = strrchr(input_filename, '\\'); /* win32/dos */
      if (c != NULL)
        c++; /* skip the path separator */
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
    output_filename = (char*)malloc(len + strlen(extension) + 1);
    if (output_filename == NULL) {
        fprintf(stderr, "couldn't allocate memory for output_filename\n");
        exit (1);
    }

    strncpy(output_filename, c, len);
    strncpy(output_filename + len, extension, strlen(extension));
    *(output_filename + len + strlen(extension)) = '\0';

    /* return the new string */
    return (output_filename);
}

static int
write_page_image(jbig2dec_params_t *params, Jbig2Image *image)
{
      if (!strncmp(params->output_file, "-", 2))
        {
	  switch (params->output_format) {
#ifdef HAVE_LIBPNG
            case jbig2dec_format_png:
              jbig2_image_write_png(image, stdout);
              break;
#endif
            case jbig2dec_format_pbm:
              jbig2_image_write_pbm(image, stdout);
              break;
            default:
              fprintf(stderr, "unsupported output format.\n");
              return 1;
          }
        }
      else
        {
          if (params->verbose > 1)
            fprintf(stderr, "saving decoded page as '%s'\n", params->output_file);
          switch (params->output_format) {
#ifdef HAVE_LIBPNG
            case jbig2dec_format_png:
              jbig2_image_write_png_file(image, params->output_file);
              break;
#endif
            case jbig2dec_format_pbm:
              jbig2_image_write_pbm_file(image, params->output_file);
              break;
            default:
              fprintf(stderr, "unsupported output format.\n");
              return 1;
          }
        }

  return 0;
}

static int
write_document_hash(jbig2dec_params_t *params)
{
    FILE *out;

    if (!strncmp(params->output_file, "-", 2)) {
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
main (int argc, char **argv)
{
  FILE *f = NULL, *f_page = NULL;
  Jbig2Ctx *ctx;
  uint8_t buf[4096];
  jbig2dec_params_t params;
  int filearg;

  /* set defaults */
  params.mode = render;
  params.verbose = 1;
  params.hash = 0;
  params.output_file = NULL;
  params.output_format = jbig2dec_format_none;

  filearg = parse_options(argc, argv, &params);

  if (params.hash) hash_init(&params);

  switch (params.mode) {
    case usage:
        print_usage();
        exit (0);
        break;
    case dump:
        fprintf(stderr, "Sorry, segment dump not yet implemented\n");
        break;
    case render:

  if ((argc - filearg) == 1)
  /* only one argument--open as a jbig2 file */
    {
      char *fn = argv[filearg];

      f = fopen(fn, "rb");
      if (f == NULL)
	{
	  fprintf(stderr, "error opening %s\n", fn);
	  return 1;
	}
    }
  else if ((argc - filearg) == 2)
  /* two arguments open as separate global and page streams */
    {
      char *fn = argv[filearg];
      char *fn_page = argv[filearg+1];

      f = fopen(fn, "rb");
      if (f == NULL)
	{
	  fprintf(stderr, "error opening %s\n", fn);
	  return 1;
	}

      f_page = fopen(fn_page, "rb");
      if (f_page == NULL)
	{
	  fprintf(stderr, "error opening %s\n", fn_page);
	  return 1;
	}
    }
  else
  /* any other number of arguments */
    return print_usage();

  ctx = jbig2_ctx_new(NULL, (Jbig2Options)(f_page != NULL ? JBIG2_OPTIONS_EMBEDDED : 0),
		      NULL,
		      error_callback, &params);

  /* pull the whole file/global stream into memory */
  for (;;)
    {
      int n_bytes = fread(buf, 1, sizeof(buf), f);
      if (n_bytes <= 0)
	break;
      if (jbig2_data_in(ctx, buf, n_bytes))
	break;
    }
  fclose(f);

  /* if there's a local page stream read that in its entirety */
  if (f_page != NULL)
    {
      Jbig2GlobalCtx *global_ctx = jbig2_make_global_ctx(ctx);
      ctx = jbig2_ctx_new(NULL, JBIG2_OPTIONS_EMBEDDED, global_ctx,
			 error_callback, &params);
      for (;;)
	{
	  int n_bytes = fread(buf, 1, sizeof(buf), f_page);
	  if (n_bytes <= 0)
	    break;
	  if (jbig2_data_in(ctx, buf, n_bytes))
	    break;
	}
      fclose(f_page);
      jbig2_global_ctx_free(global_ctx);
    }

  /* retrieve and output the returned pages */
  {
    Jbig2Image *image;

    /* work around broken CVision embedded streams */
    if (f_page != NULL)
      jbig2_complete_page(ctx);

    if (params.output_file == NULL)
      {
#ifdef HAVE_LIBPNG
        params.output_file = make_output_filename(argv[filearg], ".png");
        params.output_format = jbig2dec_format_png;
#else
        params.output_file = make_output_filename(argv[filearg], ".pbm");
        params.output_format = jbig2dec_format_pbm;
#endif
      } else {
        int len = strlen(params.output_file);
        if ((len >= 3) && (params.output_format == jbig2dec_format_none))
          /* try to set the output type by the given extension */
          set_output_format(&params, params.output_file + len - 3);
      }

    /* retrieve and write out all the completed pages */
    while ((image = jbig2_page_out(ctx)) != NULL) {
      write_page_image(&params, image);
      if (params.hash) hash_image(&params, image);
      jbig2_release_page(ctx, image);
    }
    if (params.hash) write_document_hash(&params);
  }

  jbig2_ctx_free(ctx);

  } /* end params.mode switch */

  if (params.output_file) free(params.output_file);
  if (params.hash) hash_free(&params);

  /* fin */
  return 0;
}
