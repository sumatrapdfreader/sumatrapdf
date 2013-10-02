#ifndef MUPDF_FITZ_DEVICE_H
#define MUPDF_FITZ_DEVICE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/math.h"
#include "mupdf/fitz/colorspace.h"
#include "mupdf/fitz/image.h"
#include "mupdf/fitz/shade.h"
#include "mupdf/fitz/path.h"
#include "mupdf/fitz/text.h"

/*
	The different format handlers (pdf, xps etc) interpret pages to a
	device. These devices can then process the stream of calls they
	recieve in various ways:
		The trace device outputs debugging information for the calls.
		The draw device will render them.
		The list device stores them in a list to play back later.
		The text device performs text extraction and searching.
		The bbox device calculates the bounding box for the page.
	Other devices can (and will) be written in future.
*/
typedef struct fz_device_s fz_device;

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
	/* Arguably we should have a bit for the dash pattern itself being
	 * undefined, but that causes problems; do we assume that it should
	 * always be set to non-dashing at the start of every glyph? */
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

int fz_lookup_blendmode(char *name);
char *fz_blendmode_name(int blendmode);

struct fz_device_s
{
	int hints;
	int flags;

	void *user;
	void (*free_user)(fz_device *);
	fz_context *ctx;

	void (*begin_page)(fz_device *, const fz_rect *rect, const fz_matrix *ctm);
	void (*end_page)(fz_device *);

	void (*fill_path)(fz_device *, fz_path *, int even_odd, const fz_matrix *, fz_colorspace *, float *color, float alpha);
	void (*stroke_path)(fz_device *, fz_path *, fz_stroke_state *, const fz_matrix *, fz_colorspace *, float *color, float alpha);
	void (*clip_path)(fz_device *, fz_path *, const fz_rect *rect, int even_odd, const fz_matrix *);
	void (*clip_stroke_path)(fz_device *, fz_path *, const fz_rect *rect, fz_stroke_state *, const fz_matrix *);

	void (*fill_text)(fz_device *, fz_text *, const fz_matrix *, fz_colorspace *, float *color, float alpha);
	void (*stroke_text)(fz_device *, fz_text *, fz_stroke_state *, const fz_matrix *, fz_colorspace *, float *color, float alpha);
	void (*clip_text)(fz_device *, fz_text *, const fz_matrix *, int accumulate);
	void (*clip_stroke_text)(fz_device *, fz_text *, fz_stroke_state *, const fz_matrix *);
	void (*ignore_text)(fz_device *, fz_text *, const fz_matrix *);

	void (*fill_shade)(fz_device *, fz_shade *shd, const fz_matrix *ctm, float alpha);
	void (*fill_image)(fz_device *, fz_image *img, const fz_matrix *ctm, float alpha);
	void (*fill_image_mask)(fz_device *, fz_image *img, const fz_matrix *ctm, fz_colorspace *, float *color, float alpha);
	void (*clip_image_mask)(fz_device *, fz_image *img, const fz_rect *rect, const fz_matrix *ctm);

	void (*pop_clip)(fz_device *);

	void (*begin_mask)(fz_device *, const fz_rect *, int luminosity, fz_colorspace *, float *bc);
	void (*end_mask)(fz_device *);
	void (*begin_group)(fz_device *, const fz_rect *, int isolated, int knockout, int blendmode, float alpha);
	void (*end_group)(fz_device *);

	int (*begin_tile)(fz_device *, const fz_rect *area, const fz_rect *view, float xstep, float ystep, const fz_matrix *ctm, int id);
	void (*end_tile)(fz_device *);

	/* SumatraPDF: support transfer functions */
	void (*apply_transfer_function)(fz_device *, fz_transfer_function *tr, int for_mask);

	int error_depth;
	char errmess[256];
};

void fz_begin_page(fz_device *dev, const fz_rect *rect, const fz_matrix *ctm);
void fz_end_page(fz_device *dev);
void fz_fill_path(fz_device *dev, fz_path *path, int even_odd, const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_stroke_path(fz_device *dev, fz_path *path, fz_stroke_state *stroke, const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_clip_path(fz_device *dev, fz_path *path, const fz_rect *rect, int even_odd, const fz_matrix *ctm);
void fz_clip_stroke_path(fz_device *dev, fz_path *path, const fz_rect *rect, fz_stroke_state *stroke, const fz_matrix *ctm);
void fz_fill_text(fz_device *dev, fz_text *text, const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_clip_text(fz_device *dev, fz_text *text, const fz_matrix *ctm, int accumulate);
void fz_clip_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, const fz_matrix *ctm);
void fz_ignore_text(fz_device *dev, fz_text *text, const fz_matrix *ctm);
void fz_pop_clip(fz_device *dev);
void fz_fill_shade(fz_device *dev, fz_shade *shade, const fz_matrix *ctm, float alpha);
void fz_fill_image(fz_device *dev, fz_image *image, const fz_matrix *ctm, float alpha);
void fz_fill_image_mask(fz_device *dev, fz_image *image, const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_clip_image_mask(fz_device *dev, fz_image *image, const fz_rect *rect, const fz_matrix *ctm);
void fz_begin_mask(fz_device *dev, const fz_rect *area, int luminosity, fz_colorspace *colorspace, float *bc);
void fz_end_mask(fz_device *dev);
void fz_begin_group(fz_device *dev, const fz_rect *area, int isolated, int knockout, int blendmode, float alpha);
void fz_end_group(fz_device *dev);
void fz_begin_tile(fz_device *dev, const fz_rect *area, const fz_rect *view, float xstep, float ystep, const fz_matrix *ctm);
int fz_begin_tile_id(fz_device *dev, const fz_rect *area, const fz_rect *view, float xstep, float ystep, const fz_matrix *ctm, int id);
void fz_end_tile(fz_device *dev);
/* SumatraPDF: support transfer functions */
void fz_apply_transfer_function(fz_device *dev, fz_transfer_function *tr, int for_mask);

fz_device *fz_new_device(fz_context *ctx, void *user);

/*
	fz_free_device: Free a devices of any type and its resources.
*/
void fz_free_device(fz_device *dev);

/*
	fz_enable_device_hints : Enable hints in a device.

	hints: mask of hints to enable.

	For example: By default the draw device renders shadings. For some
	purposes (perhaps rendering fast low quality thumbnails) you may want
	to tell it to ignore shadings. For this you would enable the
	FZ_IGNORE_SHADE hint.
*/
void fz_enable_device_hints(fz_device *dev, int hints);

/*
	fz_disable_device_hints : Disable hints in a device.

	hints: mask of hints to disable.

	For example: By default the text extraction device ignores images.
	For some purposes however (such as extracting HTML) you may want to
	enable the capturing of image data too. For this you would disable
	the FZ_IGNORE_IMAGE hint.
*/
void fz_disable_device_hints(fz_device *dev, int hints);

enum
{
	/* Hints */
	FZ_IGNORE_IMAGE = 1,
	FZ_IGNORE_SHADE = 2,
	FZ_DONT_INTERPOLATE_IMAGES = 4,
};

/*
	Cookie support - simple communication channel between app/library.
*/

typedef struct fz_cookie_s fz_cookie;

/*
	Provide two-way communication between application and library.
	Intended for multi-threaded applications where one thread is
	rendering pages and another thread wants read progress
	feedback or abort a job that takes a long time to finish. The
	communication is unsynchronized without locking.

	abort: The appliation should set this field to 0 before
	calling fz_run_page to render a page. At any point when the
	page is being rendered the application my set this field to 1
	which will cause the rendering to finish soon. This field is
	checked periodically when the page is rendered, but exactly
	when is not known, therefore there is no upper bound on
	exactly when the the rendering will abort. If the application
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

	incomplete_ok: If this is set to 1 by the caller, then TRYLATER
	errors are swallowed as they occur, setting the 'incomplete' flag.
	Rendering continues as much as possible ignoring errors. The caller
	is expected to check the 'incomplete' flag at the end to see if the
	rendering may be considered final or not.

	incomplete: Initially should be set to 0. Will be set to non-zero
	if a TRYLATER error is thrown during rendering and the incomplete_ok
	flag is set.
*/
struct fz_cookie_s
{
	int abort;
	int progress;
	int progress_max; /* -1 for unknown */
	int errors;
	int incomplete_ok;
	int incomplete;
};

/*
	fz_new_trace_device: Create a device to print a debug trace of
	all device calls.
*/
fz_device *fz_new_trace_device(fz_context *ctx);

/*
	fz_new_bbox_device: Create a device to compute the bounding
	box of all marks on a page.

	The returned bounding box will be the union of all bounding
	boxes of all objects on a page.
*/
fz_device *fz_new_bbox_device(fz_context *ctx, fz_rect *rectp);

/*
	fz_new_draw_device: Create a device to draw on a pixmap.

	dest: Target pixmap for the draw device. See fz_new_pixmap*
	for how to obtain a pixmap. The pixmap is not cleared by the
	draw device, see fz_clear_pixmap* for how to clear it prior to
	calling fz_new_draw_device. Free the device by calling
	fz_free_device.
*/
fz_device *fz_new_draw_device(fz_context *ctx, fz_pixmap *dest);

/*
	fz_new_draw_device_with_bbox: Create a device to draw on a pixmap.

	dest: Target pixmap for the draw device. See fz_new_pixmap*
	for how to obtain a pixmap. The pixmap is not cleared by the
	draw device, see fz_clear_pixmap* for how to clear it prior to
	calling fz_new_draw_device. Free the device by calling
	fz_free_device.

	clip: Bounding box to restrict any marking operations of the
	draw device.
*/
fz_device *fz_new_draw_device_with_bbox(fz_context *ctx, fz_pixmap *dest, const fz_irect *clip);

fz_device *fz_new_draw_device_type3(fz_context *ctx, fz_pixmap *dest);

/* SumatraPDF: GDI+ draw device */
#ifdef _WIN32
fz_device *fz_new_gdiplus_device(fz_context *ctx, void *dc, const fz_rect *base_clip);
#endif

#endif
