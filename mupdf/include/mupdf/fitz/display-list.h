#ifndef MUPDF_FITZ_DISPLAY_LIST_H
#define MUPDF_FITZ_DISPLAY_LIST_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/device.h"

/*
	Display list device -- record and play back device commands.
*/

/*
	fz_display_list is a list containing drawing commands (text,
	images, etc.). The intent is two-fold: as a caching-mechanism
	to reduce parsing of a page, and to be used as a data
	structure in multi-threading where one thread parses the page
	and another renders pages.

	Create a display list with fz_new_display_list, hand it over to
	fz_new_list_device to have it populated, and later replay the
	list (once or many times) by calling fz_run_display_list. When
	the list is no longer needed drop it with fz_drop_display_list.
*/
typedef struct fz_display_list_s fz_display_list;

fz_display_list *fz_new_display_list(fz_context *ctx, fz_rect mediabox);

fz_device *fz_new_list_device(fz_context *ctx, fz_display_list *list);

void fz_run_display_list(fz_context *ctx, fz_display_list *list, fz_device *dev, fz_matrix ctm, fz_rect scissor, fz_cookie *cookie);

fz_display_list *fz_keep_display_list(fz_context *ctx, fz_display_list *list);
void fz_drop_display_list(fz_context *ctx, fz_display_list *list);

fz_rect fz_bound_display_list(fz_context *ctx, fz_display_list *list);

fz_image *fz_new_image_from_display_list(fz_context *ctx, float w, float h, fz_display_list *list);

int fz_display_list_is_empty(fz_context *ctx, const fz_display_list *list);

#endif
