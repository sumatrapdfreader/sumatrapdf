#include "mupdf/fitz.h"

#ifdef HAVE_LURATECH

#include <string.h>
#include <ldf_jb2.h>

struct info
{
	fz_context *ctx;
	unsigned long width, height;
	unsigned long xres, yres;
	unsigned long stride;
	unsigned long pages;
	fz_colorspace *cspace;
	JB2_Handle_Document doc;

	const unsigned char *data;
	size_t len;

	unsigned char *output;
};

static void * JB2_Callback
jbig2_alloc(unsigned long size, void *userdata)
{
	struct info *state = userdata;
	return Memento_label(fz_malloc(state->ctx, size), "jbig2_alloc");
}

static JB2_Error JB2_Callback
jbig2_free(void *ptr, void *userdata)
{
	struct info *state = userdata;
	fz_free(state->ctx, ptr);
	return cJB2_Error_OK;
}

static void JB2_Callback
jbig2_message(const char *msg, JB2_Message_Level level, void *userdata)
{
	struct info *state = userdata;

	if (msg != NULL && msg[0] != '\0')
		switch (level)
		{
		case cJB2_Message_Information:
#ifdef JBIG2_DEBUG
			fz_warn(state->ctx, "luratech jbig2 info: %s", msg);
#endif
			break;
		case cJB2_Message_Warning:
			fz_warn(state->ctx, "luratech jbig2 warning: %s", msg);
			break;
		case cJB2_Message_Error:
			fz_warn(state->ctx, "luratech jbig2 error: %s", msg);
			break;
		default:
			fz_warn(state->ctx, "luratech jbig2 message: %s", msg);
			break;
		}
}

static JB2_Size_T JB2_Callback
jbig2_read(unsigned char *buf, JB2_Size_T offset, JB2_Size_T size, void *userdata)
{
	struct info *state = userdata;
	size_t available;

	if (state->len <= offset)
		return 0;
	available = fz_minz(state->len - offset, size);
	memcpy(buf, state->data + offset, available);
	return (JB2_Size_T)available;
}

static JB2_Error JB2_Callback
jbig2_write(unsigned char *buf, unsigned long row, unsigned long width, unsigned long bpp, void *userdata)
{
	struct info *state = userdata;
	int stride = (width + 7) >> 3;
	unsigned char *dp = state->output + row * stride;

	if (row >= state->height)
	{
		fz_warn(state->ctx, "row %lu outside of image", row);
		return cJB2_Error_OK;
	}

	while (stride--)
		*(dp++) = *(buf++) ^ 0xff;

	return cJB2_Error_OK;
}


static fz_pixmap *
jbig2_read_image(fz_context *ctx, struct info *jbig2, const unsigned char *buf, size_t len, int only_metadata, int subimage)
{
	struct info *state = jbig2;
	JB2_Error err;
	JB2_Scaling_Factor scale = {1, 1};
	JB2_Rect rect = {0, 0, 0, 0};
	fz_pixmap *pix = NULL;

	fz_var(pix);

	fz_try(ctx)
	{
		state->ctx = ctx;
		state->data = buf;
		state->len = len;

		err = JB2_Document_Start(&state->doc,
				jbig2_alloc, state,
				jbig2_free, state,
				jbig2_read, state,
				jbig2_message, state);
		if (err != cJB2_Error_OK)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open image: %d", (int) err);

#if defined(JB2_LICENSE_NUM_1) && defined(JB2_LICENSE_NUM_2)
		err = JB2_Document_Set_License(doc, JB2_LICENSE_NUM_1, JB2_LICENSE_NUM_2);
		if (err != cJB2_Error_OK)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot set license: %d", (int) err);
#endif

		err = JB2_Document_Get_Property(state->doc, cJB2_Prop_Number_Of_Pages, &state->pages);
		if (err != cJB2_Error_OK)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot get number of pages: %d", (int) err);
		if (subimage != -1)
		{
			if (subimage < 0 || (unsigned long) subimage >= state->pages)
				fz_throw(ctx, FZ_ERROR_GENERIC, "page number out of bounds %d vs %ld", subimage, state->pages);
			err = JB2_Document_Set_Page(state->doc, subimage);
			if (err != cJB2_Error_OK)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot select page: %d", (int) err);
		}

		err = JB2_Document_Get_Property(state->doc, cJB2_Prop_Page_Resolution_X, &state->xres);
		if (err != cJB2_Error_OK)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot get page x resolution: %d", (int) err);
		err = JB2_Document_Get_Property(state->doc, cJB2_Prop_Page_Resolution_Y, &state->yres);
		if (err != cJB2_Error_OK)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot get page y resolution: %d", (int) err);

		err = JB2_Document_Get_Property(state->doc, cJB2_Prop_Page_Width, &state->width);
		if (err != cJB2_Error_OK)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot get page width: %d", (int) err);
		err = JB2_Document_Get_Property(state->doc, cJB2_Prop_Page_Height, &state->height);
		if (err != cJB2_Error_OK)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot get page height: %d", (int) err);

		if (!only_metadata)
		{
			state->stride = (state->width + 7) >> 3;
			state->output = Memento_label(fz_malloc(state->ctx, state->stride * state->height), "jbig2_image");

			err = JB2_Document_Decompress_Page(state->doc, scale, rect, jbig2_write, state);
			if (err != cJB2_Error_OK)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot decode image: %d", (int) err);

			pix = fz_new_pixmap(ctx, fz_device_gray(ctx), state->width, state->height, NULL, 0);
			fz_unpack_tile(ctx, pix, state->output, 1, 1, state->stride, 0);
		}

	}
	fz_always(ctx)
	{
		JB2_Document_End(&state->doc);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pix);
		fz_rethrow(ctx);
	}

	return pix;
}

int
fz_load_jbig2_subimage_count(fz_context *ctx, const unsigned char *buf, size_t len)
{
	struct info jbig2 = { 0 };
	int subimage_count = 0;

	fz_try(ctx)
	{
		jbig2_read_image(ctx, &jbig2, buf, len, 1, -1);
		subimage_count = jbig2.pages;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return subimage_count;
}

void
fz_load_jbig2_info_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep, int subimage)
{
	struct info jbig2 = { 0 };

	jbig2_read_image(ctx, &jbig2, buf, len, 1, subimage);
	*cspacep = fz_keep_colorspace(ctx, jbig2.cspace);
	*wp = jbig2.width;
	*hp = jbig2.height;
	*xresp = jbig2.xres;
	*yresp = jbig2.yres;
}

fz_pixmap *
fz_load_jbig2_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int subimage)
{
	struct info jbig2 = { 0 };
	return jbig2_read_image(ctx, &jbig2, buf, len, 0, subimage);
}

fz_pixmap *
fz_load_jbig2(fz_context *ctx, const unsigned char *buf, size_t len)
{
	return fz_load_jbig2_subimage(ctx, buf, len, 0);
}

void
fz_load_jbig2_info(fz_context *ctx, const unsigned char *buf, size_t len, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep)
{
	fz_load_jbig2_info_subimage(ctx, buf, len, wp, hp, xresp, yresp, cspacep, 0);
}

#else /* HAVE_LURATECH */

#include <jbig2.h>

struct info
{
	int width, height;
	int xres, yres;
	int pages;
	fz_colorspace *cspace;
};

struct fz_jbig2_alloc_s
{
	Jbig2Allocator super;
	fz_context *ctx;
};

static void
error_callback(void *data, const char *msg, Jbig2Severity severity, int32_t seg_idx)
{
	fz_context *ctx = data;
	if (severity == JBIG2_SEVERITY_FATAL)
		fz_warn(ctx, "jbig2dec error: %s (segment %d)", msg, seg_idx);
	else if (severity == JBIG2_SEVERITY_WARNING)
		fz_warn(ctx, "jbig2dec warning: %s (segment %d)", msg, seg_idx);
#ifdef JBIG2_DEBUG
	else if (severity == JBIG2_SEVERITY_INFO)
		fz_warn(ctx, "jbig2dec info: %s (segment %d)", msg, seg_idx);
	else if (severity == JBIG2_SEVERITY_DEBUG)
		fz_warn(ctx, "jbig2dec debug: %s (segment %d)", msg, seg_idx);
#endif
}

static void *fz_jbig2_alloc(Jbig2Allocator *allocator, size_t size)
{
	fz_context *ctx = ((struct fz_jbig2_alloc_s *) allocator)->ctx;
	return fz_malloc_no_throw(ctx, size);
}

static void fz_jbig2_free(Jbig2Allocator *allocator, void *p)
{
	fz_context *ctx = ((struct fz_jbig2_alloc_s *) allocator)->ctx;
	fz_free(ctx, p);
}

static void *fz_jbig2_realloc(Jbig2Allocator *allocator, void *p, size_t size)
{
	fz_context *ctx = ((struct fz_jbig2_alloc_s *) allocator)->ctx;
	if (size == 0)
	{
		fz_free(ctx, p);
		return NULL;
	}
	if (p == NULL)
		return Memento_label(fz_malloc(ctx, size), "jbig2_realloc");
	return Memento_label(fz_realloc_no_throw(ctx, p, size), "jbig2_realloc");
}

static fz_pixmap *
jbig2_read_image(fz_context *ctx, struct info *jbig2, const unsigned char *buf, size_t len, int only_metadata, int subimage)
{
	Jbig2Ctx *jctx = NULL;
	Jbig2Image *page = NULL;
	struct fz_jbig2_alloc_s allocator;
	fz_pixmap *pix = NULL;

	allocator.super.alloc = fz_jbig2_alloc;
	allocator.super.free = fz_jbig2_free;
	allocator.super.realloc = fz_jbig2_realloc;
	allocator.ctx = ctx;

	fz_var(jctx);
	fz_var(page);
	fz_var(pix);

	fz_try(ctx)
	{
		jctx = jbig2_ctx_new((Jbig2Allocator *) &allocator, 0, NULL, error_callback, ctx);
		if (jctx == NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot create jbig2 context");
		if (jbig2_data_in(jctx, buf, len) < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot decode jbig2 image");
		if (jbig2_complete_page(jctx) < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot complete jbig2 image");

		if (only_metadata && subimage < 0)
		{
			while ((page = jbig2_page_out(jctx)) != NULL)
			{
				jbig2_release_page(jctx, page);
				jbig2->pages++;
			}
		}
		else if (only_metadata && subimage >= 0)
		{
			while ((page = jbig2_page_out(jctx)) != NULL && subimage > 0)
			{
				jbig2_release_page(jctx, page);
				subimage--;
			}

			if (page == NULL)
				fz_throw(ctx, FZ_ERROR_GENERIC, "no jbig2 image decoded");

			jbig2->cspace = fz_device_gray(ctx);
			jbig2->width = page->width;
			jbig2->height = page->height;
			jbig2->xres = 72;
			jbig2->yres = 72;
		}
		else if (subimage >= 0)
		{
			while ((page = jbig2_page_out(jctx)) != NULL && subimage > 0)
			{
				jbig2_release_page(jctx, page);
				subimage--;
			}

			if (page == NULL)
				fz_throw(ctx, FZ_ERROR_GENERIC, "no jbig2 image decoded");

			jbig2->cspace = fz_device_gray(ctx);
			jbig2->width = page->width;
			jbig2->height = page->height;
			jbig2->xres = 72;
			jbig2->yres = 72;

			pix = fz_new_pixmap(ctx, jbig2->cspace, jbig2->width, jbig2->height, NULL, 0);
			fz_unpack_tile(ctx, pix, page->data, 1, 1, page->stride, 0);
			fz_invert_pixmap(ctx, pix);
		}
	}
	fz_always(ctx)
	{
		jbig2_release_page(jctx, page);
		jbig2_ctx_free(jctx);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pix);
		fz_rethrow(ctx);
	}

	return pix;
}

int
fz_load_jbig2_subimage_count(fz_context *ctx, const unsigned char *buf, size_t len)
{
	struct info jbig2 = { 0 };
	int subimage_count = 0;

	fz_try(ctx)
	{
		jbig2_read_image(ctx, &jbig2, buf, len, 1, -1);
		subimage_count = jbig2.pages;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return subimage_count;
}

void
fz_load_jbig2_info_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep, int subimage)
{
	struct info jbig2 = { 0 };

	jbig2_read_image(ctx, &jbig2, buf, len, 1, subimage);
	*cspacep = fz_keep_colorspace(ctx, jbig2.cspace);
	*wp = jbig2.width;
	*hp = jbig2.height;
	*xresp = jbig2.xres;
	*yresp = jbig2.yres;
}

fz_pixmap *
fz_load_jbig2_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int subimage)
{
	struct info jbig2 = { 0 };
	return jbig2_read_image(ctx, &jbig2, buf, len, 0, subimage);
}

fz_pixmap *
fz_load_jbig2(fz_context *ctx, const unsigned char *buf, size_t len)
{
	return fz_load_jbig2_subimage(ctx, buf, len, 0);
}

void
fz_load_jbig2_info(fz_context *ctx, const unsigned char *buf, size_t len, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep)
{
	fz_load_jbig2_info_subimage(ctx, buf, len, wp, hp, xresp, yresp, cspacep, 0);
}

#endif /* HAVE_LURATECH */
