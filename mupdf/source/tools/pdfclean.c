// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

/*
 * PDF cleaning tool: general purpose pdf syntax washer.
 *
 * Rewrite PDF with pretty printed objects.
 * Garbage collect unreachable objects.
 * Inflate compressed streams.
 * Create subset documents.
 *
 * TODO: linearize document for fast web view
 */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int usage(void)
{
	fprintf(stderr,
		"usage: mutool clean [options] input.pdf [output.pdf] [pages]\n"
		"\t-p -\tpassword\n"
		"\t-g\tgarbage collect unused objects\n"
		"\t-gg\tin addition to -g compact xref table\n"
		"\t-ggg\tin addition to -gg merge duplicate objects\n"
		"\t-gggg\tin addition to -ggg check streams for duplication\n"
		"\t-l\tlinearize PDF (no longer supported!)\n"
		"\t-D\tsave file without encryption\n"
		"\t-E -\tsave file with new encryption (rc4-40, rc4-128, aes-128, or aes-256)\n"
		"\t-O -\towner password (only if encrypting)\n"
		"\t-U -\tuser password (only if encrypting)\n"
		"\t-P -\tpermission flags (only if encrypting)\n"
		"\t-a\tascii hex encode binary streams\n"
		"\t-d\tdecompress streams\n"
		"\t-z\tdeflate uncompressed streams\n"
		"\t-e -\tcompression \"effort\" (0 = default, 1 = min, 100 = max)\n"
		"\t-f\tcompress font streams\n"
		"\t-i\tcompress image streams\n"
		"\t-c\tclean content streams\n"
		"\t-s\tsanitize content streams\n"
		"\t-t\tcompact object syntax\n"
		"\t-tt\tindented object syntax\n"
		"\t-L\twrite object labels\n"
		"\t-v\tvectorize text\n"
		"\t-A\tcreate appearance streams for annotations\n"
		"\t-AA\trecreate appearance streams for annotations\n"
		"\t-m\tpreserve metadata\n"
		"\t-S\tsubset fonts if possible [EXPERIMENTAL!]\n"
		"\t-Z\tuse objstms if possible for extra compression\n"
		"\t--{color,gray,bitonal}-{,lossy-,lossless-}image-subsample-method -\n\t\taverage, bicubic\n"
		"\t--{color,gray,bitonal}-{,lossy-,lossless-}image-subsample-dpi -[,-]\n\t\tDPI at which to subsample [+ target dpi]\n"
		"\t--{color,gray,bitonal}-{,lossy-,lossless-}image-recompress-method -[:quality]\n\t\tnever, same, lossless, jpeg, j2k, fax, jbig2\n"
		"\t--structure=keep|drop\tKeep or drop the structure tree\n"
		"\tpages\tcomma separated list of page numbers and ranges\n"
		);
	return 1;
}

static int encrypt_method_from_string(const char *name)
{
	if (!strcmp(name, "rc4-40")) return PDF_ENCRYPT_RC4_40;
	if (!strcmp(name, "rc4-128")) return PDF_ENCRYPT_RC4_128;
	if (!strcmp(name, "aes-128")) return PDF_ENCRYPT_AES_128;
	if (!strcmp(name, "aes-256")) return PDF_ENCRYPT_AES_256;
	return PDF_ENCRYPT_UNKNOWN;
}

int pdfclean_main(int argc, char **argv)
{
	char *infile;
	char *outfile = "out.pdf";
	char *password = "";
	int c;
	int pretty = -1;
	pdf_clean_options opts = { 0 };
	int errors = 0;
	fz_context *ctx;
	int structure;
	const fz_getopt_long_options longopts[] =
	{
		{ "color-lossy-image-subsample-method=average|bicubic", &opts.image.color_lossy_image_subsample_method, (void *)1 },
		{ "color-lossless-image-subsample-method=average|bicubic", &opts.image.color_lossless_image_subsample_method, (void *)2 },
		{ "color-image-subsample-method=average|bicubic", &opts.image.color_lossy_image_subsample_method, (void *)3 },
		{ "color-lossy-image-subsample-dpi:", &opts.image.color_lossy_image_subsample_threshold, (void *)4 },
		{ "color-lossless-image-subsample-dpi:", &opts.image.color_lossless_image_subsample_threshold, (void *)5 },
		{ "color-image-subsample-dpi:", &opts.image.color_lossless_image_subsample_threshold, (void *)6 },
		{ "color-lossy-image-recompress-method=never|same|lossless|jpeg:|j2k:|fax|jbig2", &opts.image.color_lossy_image_recompress_method, (void *)7 },
		{ "color-lossless-image-recompress-method=never|same|lossless|jpeg:|j2k:|fax|jbig2", &opts.image.color_lossless_image_recompress_method, (void *)8 },
		{ "color-image-recompress-method=never|same|lossless|jpeg:|j2k:|fax|jbig2", &opts.image.color_lossless_image_recompress_method, (void *)9 },

		{ "gray-lossy-image-subsample-method=average|bicubic", &opts.image.gray_lossy_image_subsample_method, (void *)10 },
		{ "gray-lossless-image-subsample-method=average|bicubic", &opts.image.gray_lossless_image_subsample_method, (void *)11 },
		{ "gray-image-subsample-method=average|bicubic", &opts.image.gray_lossy_image_subsample_method, (void *)12 },
		{ "gray-lossy-image-subsample-dpi:", &opts.image.gray_lossy_image_subsample_threshold, (void *)13 },
		{ "gray-lossless-image-subsample-dpi:", &opts.image.gray_lossless_image_subsample_threshold, (void *)14 },
		{ "gray-image-subsample-dpi:", &opts.image.gray_lossless_image_subsample_threshold, (void *)15 },
		{ "gray-lossy-image-recompress-method=never|same|lossless|jpeg:|j2k:|fax|jbig2", &opts.image.gray_lossy_image_recompress_method, (void *)16 },
		{ "gray-lossless-image-recompress-method=never|same|lossless|jpeg:|j2k:|fax|jbig2", &opts.image.gray_lossless_image_recompress_method, (void *)17 },
		{ "gray-image-recompress-method=never|same|lossless|jpeg:|j2k:|fax|jbig2", &opts.image.gray_lossless_image_recompress_method, (void *)18 },

		{ "bitonal-image-subsample-method=average|bicubic", &opts.image.bitonal_image_subsample_method, (void *)19 },
		{ "bitonal-image-subsample-dpi:", &opts.image.bitonal_image_subsample_threshold, (void *)20 },
		{ "bitonal-image-recompress-method=never|same|lossless|jpeg:|j2k:|fax|jbig2", &opts.image.bitonal_image_recompress_method, (void *)21 },

		{ "structure=drop|keep", &structure, (void *)22 },

		{ NULL, NULL, NULL }
	};


	opts.write = pdf_default_write_options;
	opts.write.dont_regenerate_id = 1;

	while ((c = fz_getopt_long(argc, argv, "ade:fgilmp:stcvzDAE:LO:U:P:SZ", longopts)) != -1)
	{
		switch (c)
		{
		case 'p': password = fz_optarg; break;

		case 'd': opts.write.do_decompress += 1; break;
		case 'z': opts.write.do_compress += 1; break;
		case 'f': opts.write.do_compress_fonts += 1; break;
		case 'i': opts.write.do_compress_images += 1; break;
		case 'a': opts.write.do_ascii += 1; break;
		case 'e': opts.write.compression_effort = fz_atoi(fz_optarg); break;
		case 'g': opts.write.do_garbage += 1; break;
		case 'l': opts.write.do_linear += 1; break;
		case 'c': opts.write.do_clean += 1; break;
		case 's': opts.write.do_sanitize += 1; break;
		case 't': pretty = (pretty < 0) ? 0 : 1; break;
		case 'A': opts.write.do_appearance += 1; break;
		case 'L': opts.write.do_labels = 1; break;

		case 'D': opts.write.do_encrypt = PDF_ENCRYPT_NONE; break;
		case 'E': opts.write.do_encrypt = encrypt_method_from_string(fz_optarg); break;
		case 'P': opts.write.permissions = fz_atoi(fz_optarg); break;
		case 'O': fz_strlcpy(opts.write.opwd_utf8, fz_optarg, sizeof opts.write.opwd_utf8); break;
		case 'U': fz_strlcpy(opts.write.upwd_utf8, fz_optarg, sizeof opts.write.upwd_utf8); break;
		case 'm': opts.write.do_preserve_metadata = 1; break;
		case 'S': opts.subset_fonts = 1; break;
		case 'Z': opts.write.do_use_objstms = 1; break;
		case 'v': opts.vectorize++; break;
		case 0:
		{
			switch((int)(intptr_t)fz_optlong->opaque)
			{
			default:
			case 0:
				assert(!"Never happens");
				break;

			case 1: /* color-lossy-image-subsample-method */
			case 2: /* color-lossless-image-subsample-method */
				break;
			case 3: /* color-image-subsample-method */
				opts.image.color_lossless_image_subsample_method = opts.image.color_lossy_image_subsample_method;
				break;
			case 4: /* color-lossy-image-subsample-dpi */
				opts.image.color_lossy_image_subsample_to = (fz_optarg ? fz_atoi(fz_optarg) : opts.image.color_lossy_image_subsample_threshold);
				break;
			case 5: /* color-lossless-image-subsample-dpi */
				opts.image.color_lossless_image_subsample_to = (fz_optarg ? fz_atoi(fz_optarg) : opts.image.color_lossless_image_subsample_threshold);
				break;
			case 6: /* color-image-subsample-dpi */
				opts.image.color_lossless_image_subsample_to = (fz_optarg ? fz_atoi(fz_optarg) : opts.image.color_lossless_image_subsample_threshold);
				opts.image.color_lossy_image_subsample_threshold = opts.image.color_lossless_image_subsample_threshold;
				opts.image.color_lossy_image_subsample_to = opts.image.color_lossless_image_subsample_to;
				break;
			case 7: /* color-lossy-image-recompress-method */
				opts.image.color_lossless_image_recompress_quality = fz_optarg;
				break;
			case 8: /* color-lossless-image-recompress-method */
				opts.image.color_lossy_image_recompress_quality = fz_optarg;
				break;
			case 9: /* color-image-recompress-method */
				opts.image.color_lossless_image_recompress_quality = fz_optarg;
				opts.image.color_lossy_image_recompress_method = opts.image.color_lossless_image_recompress_method;
				opts.image.color_lossy_image_recompress_quality = opts.image.color_lossless_image_recompress_quality;
				break;

			case 10: /* gray-lossy-image-subsample-method */
			case 11: /* gray-lossless-image-subsample-method */
				break;
			case 12: /* gray-image-subsample-method */
				opts.image.gray_lossless_image_subsample_method = opts.image.gray_lossy_image_subsample_method;
				break;
			case 13: /* gray-lossy-image-subsample-dpi */
				opts.image.gray_lossy_image_subsample_to = (fz_optarg ? fz_atoi(fz_optarg) : opts.image.gray_lossless_image_subsample_threshold);
				break;
			case 14: /* gray-lossless-image-subsample-dpi */
				opts.image.gray_lossless_image_subsample_to = (fz_optarg ? fz_atoi(fz_optarg) : opts.image.gray_lossless_image_subsample_threshold);
				break;
			case 15: /* gray-image-subsample-dpi */
				opts.image.gray_lossless_image_subsample_to = (fz_optarg ? fz_atoi(fz_optarg) : opts.image.gray_lossy_image_subsample_threshold);
				opts.image.gray_lossy_image_subsample_threshold = opts.image.gray_lossless_image_subsample_threshold;
				opts.image.gray_lossy_image_subsample_to = opts.image.gray_lossless_image_subsample_to;
				break;
			case 16: /* gray-lossy-image-recompress-method */
				opts.image.gray_lossless_image_recompress_quality = fz_optarg;
				break;
			case 17: /* gray-lossless-image-recompress-method */
				opts.image.gray_lossy_image_recompress_quality = fz_optarg;
				break;
			case 18: /* gray-image-recompress-method */
				opts.image.gray_lossless_image_recompress_quality = fz_optarg;
				opts.image.gray_lossy_image_recompress_method = opts.image.gray_lossless_image_recompress_method;
				opts.image.gray_lossy_image_recompress_quality = opts.image.gray_lossless_image_recompress_quality;
				break;

			case 19: /* bitonal-image-subsample-method */
				break;
			case 20: /* bitonal-image-subsample-dpi */
				opts.image.bitonal_image_subsample_to = (fz_optarg ? fz_atoi(fz_optarg) : opts.image.bitonal_image_subsample_threshold);
				break;
			case 21: /* bitonal-image-recompress-method */
				opts.image.bitonal_image_recompress_quality = fz_optarg;
				if (fz_optarg)
					return usage();
				break;
			case 22: /* structure */
				opts.structure = structure; /* Allow for int/enum size mismatch. */
				break;
			}
			break;
		}
		default: return usage();
		}
	}

	if (pretty < 0)
	{
		if ((opts.write.do_ascii || opts.write.do_decompress) && !opts.write.do_compress)
			pretty = 1;
		else
			pretty = 0;
	}
	opts.write.do_pretty = pretty;

	if (argc - fz_optind < 1)
		return usage();

	infile = argv[fz_optind++];

	if (argc - fz_optind > 0 &&
		(strstr(argv[fz_optind], ".pdf") || strstr(argv[fz_optind], ".PDF")))
	{
		outfile = fz_optpath(argv[fz_optind++]);
	}

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	if (opts.write.do_compress > 1)
		fz_warn(ctx, "Brotli compression is currently non-standard and experimental. Files may not be readable in other software.");

	fz_try(ctx)
	{
		pdf_clean_file(ctx, infile, outfile, password, &opts, argc - fz_optind, &argv[fz_optind]);
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		errors++;
	}
	fz_drop_context(ctx);

	return errors != 0;
}
