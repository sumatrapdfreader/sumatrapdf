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

#ifndef MUPDF_FITZ_DEVICE_H
#define MUPDF_FITZ_DEVICE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/image.h"
#include "mupdf/fitz/shade.h"
#include "mupdf/fitz/path.h"
#include "mupdf/fitz/text.h"

/**
	The different format handlers (pdf, xps etc) interpret pages to
	a device. These devices can then process the stream of calls
	they receive in various ways:
		The trace device outputs debugging information for the calls.
		The draw device will render them.
		The list device stores them in a list to play back later.
		The text device performs text extraction and searching.
		The bbox device calculates the bounding box for the page.
	Other devices can (and will) be written in the future.
*/
typedef struct fz_device fz_device;

enum
{
	/* Flags */
	FZ_DEVFLAG_MASK = 1,
	FZ_DEVFLAG_COLOR = 2,
	FZ_DEVFLAG_UNCACHEABLE = 4,
	FZ_DEVFLAG_FILLCOLOR_UNDEFINED = 8,
	FZ_DEVFLAG_STROKECOLOR_UNDEFINED = 16,
	FZ_DEVFLAG_STARTCAP_UNDEFINED = 32,
	FZ_DEVFLAG_DASHCAP_UNDEFINED = 64,
	FZ_DEVFLAG_ENDCAP_UNDEFINED = 128,
	FZ_DEVFLAG_LINEJOIN_UNDEFINED = 256,
	FZ_DEVFLAG_MITERLIMIT_UNDEFINED = 512,
	FZ_DEVFLAG_LINEWIDTH_UNDEFINED = 1024,
	FZ_DEVFLAG_BBOX_DEFINED = 2048,
	FZ_DEVFLAG_GRIDFIT_AS_TILED = 4096,
	FZ_DEVFLAG_DASH_PATTERN_UNDEFINED = 8192,
};

enum
{
	/* PDF 1.4 -- standard separable */
	FZ_BLEND_NORMAL,
	FZ_BLEND_MULTIPLY,
	FZ_BLEND_SCREEN,
	FZ_BLEND_OVERLAY,
	FZ_BLEND_DARKEN,
	FZ_BLEND_LIGHTEN,
	FZ_BLEND_COLOR_DODGE,
	FZ_BLEND_COLOR_BURN,
	FZ_BLEND_HARD_LIGHT,
	FZ_BLEND_SOFT_LIGHT,
	FZ_BLEND_DIFFERENCE,
	FZ_BLEND_EXCLUSION,

	/* PDF 1.4 -- standard non-separable */
	FZ_BLEND_HUE,
	FZ_BLEND_SATURATION,
	FZ_BLEND_COLOR,
	FZ_BLEND_LUMINOSITY,

	/* For packing purposes */
	FZ_BLEND_MODEMASK = 15,
	FZ_BLEND_ISOLATED = 16,
	FZ_BLEND_KNOCKOUT = 32
};

/**
	Map from (case sensitive) blend mode string to enumeration.
*/
int fz_lookup_blendmode(const char *name);

/**
	Map from enumeration to blend mode string.

	The string is static, with arbitrary lifespan.
*/
const char *fz_blendmode_name(int blendmode);

/*
	Generic function type.

	Different function implementations will derive from this.
*/
typedef struct fz_function fz_function;

typedef void (fz_function_eval_fn)(fz_context *, fz_function *, const float *, float *);

enum
{
	FZ_FUNCTION_MAX_N = FZ_MAX_COLORS,
	FZ_FUNCTION_MAX_M = FZ_MAX_COLORS
};

struct fz_function
{
	fz_storable storable;
	size_t size;
	int m;					/* number of input values */
	int n;					/* number of output values */

	fz_function_eval_fn *eval;
};

fz_function *fz_new_function_of_size(fz_context *ctx, int size, size_t size2, int m, int n, fz_function_eval_fn *eval, fz_store_drop_fn *drop);

#define fz_new_derived_function(CTX,TYPE,SIZE,M,N,EVAL,DROP) \
	((TYPE*)Memento_label(fz_new_function_of_size(CTX,sizeof(TYPE),SIZE,M,N,EVAL,DROP), #TYPE))

/*
	Evaluate a function.

	Input vector = (in[0], ..., in[inlen-1])
	Output vector = (out[0], ..., out[outlen-1])

	If inlen or outlen do not match that expected by the function, this
	routine will truncate or extend the input/output (with 0's) as
	required.
*/
void fz_eval_function(fz_context *ctx, fz_function *func, const float *in, int inlen, float *out, int outlen);

/*
	Keep a function reference.
*/
fz_function *fz_keep_function(fz_context *ctx, fz_function *func);

/*
	Drop a function reference.
*/
void fz_drop_function(fz_context *ctx, fz_function *func);

/*
	Function size
*/
size_t fz_function_size(fz_context *ctx, fz_function *func);

/**
	The device structure is public to allow devices to be
	implemented outside of fitz.

	Device methods should always be called using e.g.
	fz_fill_path(ctx, dev, ...) rather than
	dev->fill_path(ctx, dev, ...)
*/

/**
	Devices can keep track of containers (clips/masks/groups/tiles)
	as they go to save callers having to do it.
*/
typedef struct
{
	fz_rect scissor;
	int type;
	int user;
} fz_device_container_stack;

enum
{
	fz_device_container_stack_is_clip,
	fz_device_container_stack_is_mask,
	fz_device_container_stack_is_group,
	fz_device_container_stack_is_tile,
};

/* Structure types */
typedef enum
{
	FZ_STRUCTURE_INVALID = -1,

	/* Grouping elements (PDF 1.7 - Table 10.20) */
	FZ_STRUCTURE_DOCUMENT,
	FZ_STRUCTURE_PART,
	FZ_STRUCTURE_ART,
	FZ_STRUCTURE_SECT,
	FZ_STRUCTURE_DIV,
	FZ_STRUCTURE_BLOCKQUOTE,
	FZ_STRUCTURE_CAPTION,
	FZ_STRUCTURE_TOC,
	FZ_STRUCTURE_TOCI,
	FZ_STRUCTURE_INDEX,
	FZ_STRUCTURE_NONSTRUCT,
	FZ_STRUCTURE_PRIVATE,
	/* Grouping elements (PDF 2.0 - Table 364) */
	FZ_STRUCTURE_DOCUMENTFRAGMENT,
	/* Grouping elements (PDF 2.0 - Table 365) */
	FZ_STRUCTURE_ASIDE,
	/* Grouping elements (PDF 2.0 - Table 366) */
	FZ_STRUCTURE_TITLE,
	FZ_STRUCTURE_FENOTE,
	/* Grouping elements (PDF 2.0 - Table 367) */
	FZ_STRUCTURE_SUB,

	/* Paragraphlike elements (PDF 1.7 - Table 10.21) */
	FZ_STRUCTURE_P,
	FZ_STRUCTURE_H,
	FZ_STRUCTURE_H1,
	FZ_STRUCTURE_H2,
	FZ_STRUCTURE_H3,
	FZ_STRUCTURE_H4,
	FZ_STRUCTURE_H5,
	FZ_STRUCTURE_H6,

	/* List elements (PDF 1.7 - Table 10.23) */
	FZ_STRUCTURE_LIST,
	FZ_STRUCTURE_LISTITEM,
	FZ_STRUCTURE_LABEL,
	FZ_STRUCTURE_LISTBODY,

	/* Table elements (PDF 1.7 - Table 10.24) */
	FZ_STRUCTURE_TABLE,
	FZ_STRUCTURE_TR,
	FZ_STRUCTURE_TH,
	FZ_STRUCTURE_TD,
	FZ_STRUCTURE_THEAD,
	FZ_STRUCTURE_TBODY,
	FZ_STRUCTURE_TFOOT,

	/* Inline elements (PDF 1.7 - Table 10.25) */
	FZ_STRUCTURE_SPAN,
	FZ_STRUCTURE_QUOTE,
	FZ_STRUCTURE_NOTE,
	FZ_STRUCTURE_REFERENCE,
	FZ_STRUCTURE_BIBENTRY,
	FZ_STRUCTURE_CODE,
	FZ_STRUCTURE_LINK,
	FZ_STRUCTURE_ANNOT,
	/* Inline elements (PDF 2.0 - Table 368) */
	FZ_STRUCTURE_EM,
	FZ_STRUCTURE_STRONG,

	/* Ruby inline element (PDF 1.7 - Table 10.26) */
	FZ_STRUCTURE_RUBY,
	FZ_STRUCTURE_RB,
	FZ_STRUCTURE_RT,
	FZ_STRUCTURE_RP,

	/* Warichu inline element (PDF 1.7 - Table 10.26) */
	FZ_STRUCTURE_WARICHU,
	FZ_STRUCTURE_WT,
	FZ_STRUCTURE_WP,

	/* Illustration elements (PDF 1.7 - Table 10.27) */
	FZ_STRUCTURE_FIGURE,
	FZ_STRUCTURE_FORMULA,
	FZ_STRUCTURE_FORM,

	/* Artifact structure type (PDF 2.0 - Table 375) */
	FZ_STRUCTURE_ARTIFACT
} fz_structure;

const char *fz_structure_to_string(fz_structure type);
fz_structure fz_structure_from_string(const char *str);

typedef enum
{
	FZ_METATEXT_ACTUALTEXT,
	FZ_METATEXT_ALT,
	FZ_METATEXT_ABBREVIATION,
	FZ_METATEXT_TITLE
} fz_metatext;

struct fz_device
{
	int refs;
	int hints;
	int flags;

	void (*close_device)(fz_context *, fz_device *);
	void (*drop_device)(fz_context *, fz_device *);

	void (*fill_path)(fz_context *, fz_device *, const fz_path *, int even_odd, fz_matrix, fz_colorspace *, const float *color, float alpha, fz_color_params );
	void (*stroke_path)(fz_context *, fz_device *, const fz_path *, const fz_stroke_state *, fz_matrix, fz_colorspace *, const float *color, float alpha, fz_color_params );
	void (*clip_path)(fz_context *, fz_device *, const fz_path *, int even_odd, fz_matrix, fz_rect scissor);
	void (*clip_stroke_path)(fz_context *, fz_device *, const fz_path *, const fz_stroke_state *, fz_matrix, fz_rect scissor);

	void (*fill_text)(fz_context *, fz_device *, const fz_text *, fz_matrix, fz_colorspace *, const float *color, float alpha, fz_color_params );
	void (*stroke_text)(fz_context *, fz_device *, const fz_text *, const fz_stroke_state *, fz_matrix, fz_colorspace *, const float *color, float alpha, fz_color_params );
	void (*clip_text)(fz_context *, fz_device *, const fz_text *, fz_matrix, fz_rect scissor);
	void (*clip_stroke_text)(fz_context *, fz_device *, const fz_text *, const fz_stroke_state *, fz_matrix, fz_rect scissor);
	void (*ignore_text)(fz_context *, fz_device *, const fz_text *, fz_matrix );

	void (*fill_shade)(fz_context *, fz_device *, fz_shade *shd, fz_matrix ctm, float alpha, fz_color_params color_params);
	void (*fill_image)(fz_context *, fz_device *, fz_image *img, fz_matrix ctm, float alpha, fz_color_params color_params);
	void (*fill_image_mask)(fz_context *, fz_device *, fz_image *img, fz_matrix ctm, fz_colorspace *, const float *color, float alpha, fz_color_params color_params);
	void (*clip_image_mask)(fz_context *, fz_device *, fz_image *img, fz_matrix ctm, fz_rect scissor);

	void (*pop_clip)(fz_context *, fz_device *);

	void (*begin_mask)(fz_context *, fz_device *, fz_rect area, int luminosity, fz_colorspace *, const float *bc, fz_color_params );
	void (*end_mask)(fz_context *, fz_device *, fz_function *fn);
	void (*begin_group)(fz_context *, fz_device *, fz_rect area, fz_colorspace *cs, int isolated, int knockout, int blendmode, float alpha);
	void (*end_group)(fz_context *, fz_device *);

	int (*begin_tile)(fz_context *, fz_device *, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm, int id, int doc_id);
	void (*end_tile)(fz_context *, fz_device *);

	void (*render_flags)(fz_context *, fz_device *, int set, int clear);
	void (*set_default_colorspaces)(fz_context *, fz_device *, fz_default_colorspaces *);

	void (*begin_layer)(fz_context *, fz_device *, const char *layer_name);
	void (*end_layer)(fz_context *, fz_device *);

	void (*begin_structure)(fz_context *, fz_device *, fz_structure standard, const char *raw, int idx);
	void (*end_structure)(fz_context *, fz_device *);

	void (*begin_metatext)(fz_context *, fz_device *, fz_metatext meta, const char *text);
	void (*end_metatext)(fz_context *, fz_device *);

	fz_rect d1_rect;

	int container_len;
	int container_cap;
	fz_device_container_stack *container;
};

/**
	Device calls; graphics primitives and containers.
*/
void fz_fill_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm, fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params);
void fz_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params);
void fz_clip_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm, fz_rect scissor);
void fz_clip_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor);
void fz_fill_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm, fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params);
void fz_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm, fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params);
void fz_clip_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm, fz_rect scissor);
void fz_clip_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor);
void fz_ignore_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm);
void fz_pop_clip(fz_context *ctx, fz_device *dev);
void fz_fill_shade(fz_context *ctx, fz_device *dev, fz_shade *shade, fz_matrix ctm, float alpha, fz_color_params color_params);
void fz_fill_image(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm, float alpha, fz_color_params color_params);
void fz_fill_image_mask(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm, fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params);
void fz_clip_image_mask(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm, fz_rect scissor);
void fz_begin_mask(fz_context *ctx, fz_device *dev, fz_rect area, int luminosity, fz_colorspace *colorspace, const float *bc, fz_color_params color_params);
void fz_end_mask(fz_context *ctx, fz_device *dev);
void fz_end_mask_tr(fz_context *ctx, fz_device *dev, fz_function *fn);
void fz_begin_group(fz_context *ctx, fz_device *dev, fz_rect area, fz_colorspace *cs, int isolated, int knockout, int blendmode, float alpha);
void fz_end_group(fz_context *ctx, fz_device *dev);
void fz_begin_tile(fz_context *ctx, fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm);
int fz_begin_tile_id(fz_context *ctx, fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm, int id);
int fz_begin_tile_tid(fz_context *ctx, fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm, int id, int doc_id);
void fz_end_tile(fz_context *ctx, fz_device *dev);
void fz_render_flags(fz_context *ctx, fz_device *dev, int set, int clear);
void fz_set_default_colorspaces(fz_context *ctx, fz_device *dev, fz_default_colorspaces *default_cs);
void fz_begin_layer(fz_context *ctx, fz_device *dev, const char *layer_name);
void fz_end_layer(fz_context *ctx, fz_device *dev);
void fz_begin_structure(fz_context *ctx, fz_device *dev, fz_structure standard, const char *raw, int idx);
void fz_end_structure(fz_context *ctx, fz_device *dev);
void fz_begin_metatext(fz_context *ctx, fz_device *dev, fz_metatext meta, const char *text);
void fz_end_metatext(fz_context *ctx, fz_device *dev);

/**
	Devices are created by calls to device implementations, for
	instance: foo_new_device(). These will be implemented by calling
	fz_new_derived_device(ctx, foo_device) where foo_device is a
	structure "derived from" fz_device, for instance
	typedef struct { fz_device base;  ...extras...} foo_device;
*/
fz_device *fz_new_device_of_size(fz_context *ctx, int size);
#define fz_new_derived_device(CTX, TYPE) \
	((TYPE *)Memento_label(fz_new_device_of_size(ctx,sizeof(TYPE)),#TYPE))

/**
	Signal the end of input, and flush any buffered output.
	This is NOT called implicitly on fz_drop_device. This
	may throw exceptions.
*/
void fz_close_device(fz_context *ctx, fz_device *dev);

/**
	Reduce the reference count on a device. When the reference count
	reaches zero, the device and its resources will be freed.
	Don't forget to call fz_close_device before dropping the device,
	or you may get incomplete output!

	Never throws exceptions.
*/
void fz_drop_device(fz_context *ctx, fz_device *dev);

/**
	Increment the reference count for a device. Returns the same
	pointer.

	Never throws exceptions.
*/
fz_device *fz_keep_device(fz_context *ctx, fz_device *dev);

/**
	Enable (set) hint bits within the hint bitfield for a device.
*/
void fz_enable_device_hints(fz_context *ctx, fz_device *dev, int hints);

/**
	Disable (clear) hint bits within the hint bitfield for a device.
*/
void fz_disable_device_hints(fz_context *ctx, fz_device *dev, int hints);

/**
	Find current scissor region as tracked by the device.
*/
fz_rect fz_device_current_scissor(fz_context *ctx, fz_device *dev);

enum
{
	/* Hints */
	FZ_DONT_INTERPOLATE_IMAGES = 1,
	FZ_NO_CACHE = 2,
	FZ_DONT_DECODE_IMAGES = 4
};

/**
	Cookie support - simple communication channel between app/library.
*/

/**
	Provide two-way communication between application and library.
	Intended for multi-threaded applications where one thread is
	rendering pages and another thread wants to read progress
	feedback or abort a job that takes a long time to finish. The
	communication is unsynchronized without locking.

	abort: The application should set this field to 0 before
	calling fz_run_page to render a page. At any point when the
	page is being rendered the application my set this field to 1
	which will cause the rendering to finish soon. This field is
	checked periodically when the page is rendered, but exactly
	when is not known, therefore there is no upper bound on
	exactly when the rendering will abort. If the application
	did not provide a set of locks to fz_new_context, it must also
	await the completion of fz_run_page before issuing another
	call to fz_run_page. Note that once the application has set
	this field to 1 after it called fz_run_page it may not change
	the value again.

	progress: Communicates rendering progress back to the
	application and is read only. Increments as a page is being
	rendered. The value starts out at 0 and is limited to less
	than or equal to progress_max, unless progress_max is -1.

	progress_max: Communicates the known upper bound of rendering
	back to the application and is read only. The maximum value
	that the progress field may take. If there is no known upper
	bound on how long the rendering may take this value is -1 and
	progress is not limited. Note that the value of progress_max
	may change from -1 to a positive value once an upper bound is
	known, so take this into consideration when comparing the
	value of progress to that of progress_max.

	errors: count of errors during current rendering.

	incomplete: Initially should be set to 0. Will be set to
	non-zero if a TRYLATER error is thrown during rendering.
*/
typedef struct
{
	int abort;
	int progress;
	size_t progress_max; /* (size_t)-1 for unknown */
	int errors;
	int incomplete;
} fz_cookie;

/**
	Create a device to print a debug trace of all device calls.
*/
fz_device *fz_new_trace_device(fz_context *ctx, fz_output *out);

/**
	Create a device to output raw information.
*/
fz_device *fz_new_xmltext_device(fz_context *ctx, fz_output *out);

/**
	Create a device to compute the bounding
	box of all marks on a page.

	The returned bounding box will be the union of all bounding
	boxes of all objects on a page.
*/
fz_device *fz_new_bbox_device(fz_context *ctx, fz_rect *rectp);

/**
	Create a device to test for features.

	Currently only tests for the presence of non-grayscale colors.

	is_color: Possible values returned:
		0: Definitely greyscale
		1: Probably color (all colors were grey, but there
		were images or shadings in a non grey colorspace).
		2: Definitely color

	threshold: The difference from grayscale that will be tolerated.
	Typical values to use are either 0 (be exact) and 0.02 (allow an
	imperceptible amount of slop).

	options: A set of bitfield options, from the FZ_TEST_OPT set.

	passthrough: A device to pass all calls through to, or NULL.
	If set, then the test device can both test and pass through to
	an underlying device (like, say, the display list device). This
	means that a display list can be created and at the end we'll
	know if it's colored or not.

	In the absence of a passthrough device, the device will throw
	an exception to stop page interpretation when color is found.
*/
fz_device *fz_new_test_device(fz_context *ctx, int *is_color, float threshold, int options, fz_device *passthrough);

enum
{
	/* If set, test every pixel of images exhaustively.
	 * If clear, just look at colorspaces for images. */
	FZ_TEST_OPT_IMAGES = 1,

	/* If set, test every pixel of shadings. */
	/* If clear, just look at colorspaces for shadings. */
	FZ_TEST_OPT_SHADINGS = 2
};

/**
	Create a device to draw on a pixmap.

	dest: Target pixmap for the draw device. See fz_new_pixmap*
	for how to obtain a pixmap. The pixmap is not cleared by the
	draw device, see fz_clear_pixmap* for how to clear it prior to
	calling fz_new_draw_device. Free the device by calling
	fz_drop_device.

	transform: Transform from user space in points to device space
	in pixels.
*/
fz_device *fz_new_draw_device(fz_context *ctx, fz_matrix transform, fz_pixmap *dest);

/**
	Create a device to draw on a pixmap.

	dest: Target pixmap for the draw device. See fz_new_pixmap*
	for how to obtain a pixmap. The pixmap is not cleared by the
	draw device, see fz_clear_pixmap* for how to clear it prior to
	calling fz_new_draw_device. Free the device by calling
	fz_drop_device.

	transform: Transform from user space in points to device space
	in pixels.

	clip: Bounding box to restrict any marking operations of the
	draw device.
*/
fz_device *fz_new_draw_device_with_bbox(fz_context *ctx, fz_matrix transform, fz_pixmap *dest, const fz_irect *clip);

/**
	Create a device to draw on a pixmap.

	dest: Target pixmap for the draw device. See fz_new_pixmap*
	for how to obtain a pixmap. The pixmap is not cleared by the
	draw device, see fz_clear_pixmap* for how to clear it prior to
	calling fz_new_draw_device. Free the device by calling
	fz_drop_device.

	transform: Transform from user space in points to device space
	in pixels.

	proof_cs: Intermediate color space to map though when mapping to
	color space defined by pixmap.
*/
fz_device *fz_new_draw_device_with_proof(fz_context *ctx, fz_matrix transform, fz_pixmap *dest, fz_colorspace *proof_cs);

/**
	Create a device to draw on a pixmap.

	dest: Target pixmap for the draw device. See fz_new_pixmap*
	for how to obtain a pixmap. The pixmap is not cleared by the
	draw device, see fz_clear_pixmap* for how to clear it prior to
	calling fz_new_draw_device. Free the device by calling
	fz_drop_device.

	transform: Transform from user space in points to device space
	in pixels.

	clip: Bounding box to restrict any marking operations of the
	draw device.

	proof_cs: Color space to render to prior to mapping to color
	space defined by pixmap.
*/
fz_device *fz_new_draw_device_with_bbox_proof(fz_context *ctx, fz_matrix transform, fz_pixmap *dest, const fz_irect *clip, fz_colorspace *cs);

fz_device *fz_new_draw_device_type3(fz_context *ctx, fz_matrix transform, fz_pixmap *dest);

/**
	struct fz_draw_options: Options for creating a pixmap and draw
	device.
*/
typedef struct
{
	int rotate;
	int x_resolution;
	int y_resolution;
	int width;
	int height;
	fz_colorspace *colorspace;
	int alpha;
	int graphics;
	int text;
} fz_draw_options;

FZ_DATA extern const char *fz_draw_options_usage;

/**
	Parse draw device options from a comma separated key-value string.
*/
fz_draw_options *fz_parse_draw_options(fz_context *ctx, fz_draw_options *options, const char *string);

/**
	Create a new pixmap and draw device, using the specified options.

	options: Options to configure the draw device, and choose the
	resolution and colorspace.

	mediabox: The bounds of the page in points.

	pixmap: An out parameter containing the newly created pixmap.
*/
fz_device *fz_new_draw_device_with_options(fz_context *ctx, const fz_draw_options *options, fz_rect mediabox, fz_pixmap **pixmap);

#endif
