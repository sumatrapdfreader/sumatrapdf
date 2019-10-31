#include "mupdf/fitz.h"

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct fz_svg_writer_s fz_svg_writer;

struct fz_svg_writer_s
{
	fz_document_writer super;
	char *path;
	int count;
	fz_output *out;
	int text_format;
	int reuse_images;
	int id;
};

const char *fz_svg_write_options_usage =
	"SVG output options:\n"
	"\ttext=text: Emit text as <text> elements (inaccurate fonts).\n"
	"\ttext=path: Emit text as <path> elements (accurate fonts).\n"
	"\tno-reuse-images: Do not reuse images using <symbol> definitions.\n"
	"\n"
	;

static fz_device *
svg_begin_page(fz_context *ctx, fz_document_writer *wri_, fz_rect mediabox)
{
	fz_svg_writer *wri = (fz_svg_writer*)wri_;
	char path[PATH_MAX];

	float w = mediabox.x1 - mediabox.x0;
	float h = mediabox.y1 - mediabox.y0;

	wri->count += 1;

	fz_format_output_path(ctx, path, sizeof path, wri->path, wri->count);
	wri->out = fz_new_output_with_path(ctx, path, 0);
	return fz_new_svg_device_with_id(ctx, wri->out, w, h, wri->text_format, wri->reuse_images, &wri->id);
}

static void
svg_end_page(fz_context *ctx, fz_document_writer *wri_, fz_device *dev)
{
	fz_svg_writer *wri = (fz_svg_writer*)wri_;

	fz_try(ctx)
	{
		fz_close_device(ctx, dev);
		fz_close_output(ctx, wri->out);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		fz_drop_output(ctx, wri->out);
		wri->out = NULL;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
svg_drop_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_svg_writer *wri = (fz_svg_writer*)wri_;
	fz_drop_output(ctx, wri->out);
	fz_free(ctx, wri->path);
}

fz_document_writer *
fz_new_svg_writer(fz_context *ctx, const char *path, const char *args)
{
	const char *val;
	fz_svg_writer *wri = fz_new_derived_document_writer(ctx, fz_svg_writer, svg_begin_page, svg_end_page, NULL, svg_drop_writer);

	wri->text_format = FZ_SVG_TEXT_AS_PATH;
	wri->reuse_images = 1;

	fz_try(ctx)
	{
		if (fz_has_option(ctx, args, "text", &val))
		{
			if (fz_option_eq(val, "text"))
				wri->text_format = FZ_SVG_TEXT_AS_TEXT;
			else if (fz_option_eq(val, "path"))
				wri->text_format = FZ_SVG_TEXT_AS_PATH;
		}
		if (fz_has_option(ctx, args, "no-reuse-images", &val))
			if (fz_option_eq(val, "yes"))
				wri->reuse_images = 0;
		wri->path = fz_strdup(ctx, path ? path : "out-%04d.svg");
	}
	fz_catch(ctx)
	{
		fz_free(ctx, wri);
		fz_rethrow(ctx);
	}

	return (fz_document_writer*)wri;
}
