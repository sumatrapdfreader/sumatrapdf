/*
 * pdfextract -- the ultimate way to extract images and fonts from pdfs
 */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <stdlib.h>
#include <stdio.h>

static pdf_document *doc = NULL;
static fz_context *ctx = NULL;
static int dorgb = 0;
static int doicc = 1;

static void usage(void)
{
	fprintf(stderr, "usage: mutool extract [options] file.pdf [object numbers]\n");
	fprintf(stderr, "\t-p\tpassword\n");
	fprintf(stderr, "\t-r\tconvert images to rgb\n");
	fprintf(stderr, "\t-N\tdo not use ICC color conversions\n");
	exit(1);
}

static int isimage(pdf_obj *obj)
{
	pdf_obj *type = pdf_dict_get(ctx, obj, PDF_NAME(Subtype));
	return pdf_name_eq(ctx, type, PDF_NAME(Image));
}

static int isfontdesc(pdf_obj *obj)
{
	pdf_obj *type = pdf_dict_get(ctx, obj, PDF_NAME(Type));
	return pdf_name_eq(ctx, type, PDF_NAME(FontDescriptor));
}

static void writepixmap(fz_context *ctx, fz_pixmap *pix, char *file, int dorgb)
{
	char buf[1024];
	fz_pixmap *rgb = NULL;

	if (!pix)
		return;

	if (dorgb && pix->colorspace && pix->colorspace != fz_device_rgb(ctx))
	{
		rgb = fz_convert_pixmap(ctx, pix, fz_device_rgb(ctx), NULL, NULL, fz_default_color_params /* FIXME */, 1);
		pix = rgb;
	}

	if (!pix->colorspace || pix->colorspace->type == FZ_COLORSPACE_GRAY || pix->colorspace->type == FZ_COLORSPACE_RGB)
	{
		fz_snprintf(buf, sizeof(buf), "%s.png", file);
		printf("extracting image %s\n", buf);
		fz_save_pixmap_as_png(ctx, pix, buf);
	}
	else
	{
		fz_snprintf(buf, sizeof(buf), "%s.pam", file);
		printf("extracting image %s\n", buf);
		fz_save_pixmap_as_pam(ctx, pix, buf);
	}

	fz_drop_pixmap(ctx, rgb);
}

static void
writejpeg(fz_context *ctx, const unsigned char *data, size_t len, const char *file)
{
	char buf[1024];
	fz_output *out;

	fz_snprintf(buf, sizeof(buf), "%s.jpg", file);

	out = fz_new_output_with_path(ctx, buf, 0);
	fz_try(ctx)
	{
		printf("extracting image %s\n", buf);
		fz_write_data(ctx, out, data, len);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void saveimage(pdf_obj *ref)
{
	fz_image *image = NULL;
	fz_pixmap *pix = NULL;
	char buf[32];
	fz_compressed_buffer *cbuf;
	int type;

	fz_var(image);
	fz_var(pix);

	fz_try(ctx)
	{
		image = pdf_load_image(ctx, doc, ref);
		cbuf = fz_compressed_image_buffer(ctx, image);
		fz_snprintf(buf, sizeof(buf), "img-%04d", pdf_to_num(ctx, ref));
		type = cbuf == NULL ? FZ_IMAGE_UNKNOWN : cbuf->params.type;

		if (image->use_colorkey)
			type = FZ_IMAGE_UNKNOWN;
		if (image->use_decode)
			type = FZ_IMAGE_UNKNOWN;
		if (image->mask)
			type = FZ_IMAGE_UNKNOWN;
		if (dorgb)
		{
			enum fz_colorspace_type ctype = fz_colorspace_type(ctx, image->colorspace);
			if (ctype != FZ_COLORSPACE_RGB && ctype != FZ_COLORSPACE_GRAY)
				type = FZ_IMAGE_UNKNOWN;
		}

		if (type == FZ_IMAGE_JPEG)
		{
			unsigned char *data;
			size_t len = fz_buffer_storage(ctx, cbuf->buffer, &data);
			writejpeg(ctx, data, len, buf);
		}
		else
		{
			pix = fz_get_pixmap_from_image(ctx, image, NULL, NULL, 0, 0);
			writepixmap(ctx, pix, buf, dorgb);
		}
	}
	fz_always(ctx)
	{
		fz_drop_image(ctx, image);
		fz_drop_pixmap(ctx, pix);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void savefont(pdf_obj *dict)
{
	char namebuf[1024];
	fz_buffer *buf;
	pdf_obj *stream = NULL;
	pdf_obj *obj;
	char *ext = "";
	fz_output *out;
	const char *fontname = "font";
	size_t len;
	unsigned char *data;

	obj = pdf_dict_get(ctx, dict, PDF_NAME(FontName));
	if (obj)
		fontname = pdf_to_name(ctx, obj);

	obj = pdf_dict_get(ctx, dict, PDF_NAME(FontFile));
	if (obj)
	{
		stream = obj;
		ext = "pfa";
	}

	obj = pdf_dict_get(ctx, dict, PDF_NAME(FontFile2));
	if (obj)
	{
		stream = obj;
		ext = "ttf";
	}

	obj = pdf_dict_get(ctx, dict, PDF_NAME(FontFile3));
	if (obj)
	{
		stream = obj;

		obj = pdf_dict_get(ctx, obj, PDF_NAME(Subtype));
		if (obj && !pdf_is_name(ctx, obj))
			fz_throw(ctx, FZ_ERROR_GENERIC, "invalid font descriptor subtype");

		if (pdf_name_eq(ctx, obj, PDF_NAME(Type1C)))
			ext = "cff";
		else if (pdf_name_eq(ctx, obj, PDF_NAME(CIDFontType0C)))
			ext = "cid";
		else if (pdf_name_eq(ctx, obj, PDF_NAME(OpenType)))
			ext = "otf";
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "unhandled font type '%s'", pdf_to_name(ctx, obj));
	}

	if (!stream)
	{
		fz_warn(ctx, "unhandled font type");
		return;
	}

	buf = pdf_load_stream(ctx, stream);
	len = fz_buffer_storage(ctx, buf, &data);
	fz_try(ctx)
	{
		fz_snprintf(namebuf, sizeof(namebuf), "%s-%04d.%s", fontname, pdf_to_num(ctx, dict), ext);
		printf("extracting font %s\n", namebuf);
		out = fz_new_output_with_path(ctx, namebuf, 0);
		fz_try(ctx)
		{
			fz_write_data(ctx, out, data, len);
			fz_close_output(ctx, out);
		}
		fz_always(ctx)
			fz_drop_output(ctx, out);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void extractobject(int num)
{
	pdf_obj *ref;

	if (!doc)
		fz_throw(ctx, FZ_ERROR_GENERIC, "no file specified");

	fz_try(ctx)
	{
		ref = pdf_new_indirect(ctx, doc, num, 0);
		if (isimage(ref))
			saveimage(ref);
		if (isfontdesc(ref))
			savefont(ref);
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, ref);
	fz_catch(ctx)
		fz_warn(ctx, "ignoring object %d", num);
}

int pdfextract_main(int argc, char **argv)
{
	char *infile;
	char *password = "";
	int c, o;

	while ((c = fz_getopt(argc, argv, "p:rN")) != -1)
	{
		switch (c)
		{
		case 'p': password = fz_optarg; break;
		case 'r': dorgb++; break;
		case 'N': doicc^=1; break;
		default: usage(); break;
		}
	}

	if (fz_optind == argc)
		usage();

	infile = argv[fz_optind++];

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	if (doicc)
		fz_enable_icc(ctx);
	else
		fz_disable_icc(ctx);

	doc = pdf_open_document(ctx, infile);
	if (pdf_needs_password(ctx, doc))
		if (!pdf_authenticate_password(ctx, doc, password))
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot authenticate password: %s", infile);

	if (fz_optind == argc)
	{
		int len = pdf_count_objects(ctx, doc);
		for (o = 1; o < len; o++)
			extractobject(o);
	}
	else
	{
		while (fz_optind < argc)
		{
			extractobject(atoi(argv[fz_optind]));
			fz_optind++;
		}
	}

	pdf_drop_document(ctx, doc);
	fz_flush_warnings(ctx);
	fz_drop_context(ctx);
	return 0;
}
