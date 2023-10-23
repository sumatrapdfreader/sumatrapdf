// Copyright (C) 2004-2023 Artifex Software, Inc.
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

#ifndef PDF_INTERPRET_H
#define PDF_INTERPRET_H

#include "mupdf/pdf/font.h"
#include "mupdf/pdf/resource.h"
#include "mupdf/pdf/document.h"

typedef struct pdf_gstate pdf_gstate;
typedef struct pdf_processor pdf_processor;

void *pdf_new_processor(fz_context *ctx, int size);
pdf_processor *pdf_keep_processor(fz_context *ctx, pdf_processor *proc);
void pdf_close_processor(fz_context *ctx, pdf_processor *proc);
void pdf_drop_processor(fz_context *ctx, pdf_processor *proc);

struct pdf_processor
{
	int refs;

	/* close the processor. Also closes any chained processors. */
	void (*close_processor)(fz_context *ctx, pdf_processor *proc);
	void (*drop_processor)(fz_context *ctx, pdf_processor *proc);

	/* At any stage, we can have one set of resources in place.
	 * This function gives us a set of resources to use. We remember
	 * any previous set on a stack, so we can pop back to it later.
	 * Our responsibility (as well as remembering it for our own use)
	 * is to pass either it, or a filtered version of it onto any
	 * chained processor. */
	void (*push_resources)(fz_context *ctx, pdf_processor *proc, pdf_obj *res);
	/* Pop the resources stack. This must be passed on to any chained
	 * processors. This returns a pointer to the resource dict just
	 * popped by the deepest filter. The caller inherits this reference. */
	pdf_obj *(*pop_resources)(fz_context *ctx, pdf_processor *proc);

	/* general graphics state */
	void (*op_w)(fz_context *ctx, pdf_processor *proc, float linewidth);
	void (*op_j)(fz_context *ctx, pdf_processor *proc, int linejoin);
	void (*op_J)(fz_context *ctx, pdf_processor *proc, int linecap);
	void (*op_M)(fz_context *ctx, pdf_processor *proc, float miterlimit);
	void (*op_d)(fz_context *ctx, pdf_processor *proc, pdf_obj *array, float phase);
	void (*op_ri)(fz_context *ctx, pdf_processor *proc, const char *intent);
	void (*op_i)(fz_context *ctx, pdf_processor *proc, float flatness);

	void (*op_gs_begin)(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *extgstate);
	void (*op_gs_BM)(fz_context *ctx, pdf_processor *proc, const char *blendmode);
	void (*op_gs_ca)(fz_context *ctx, pdf_processor *proc, float alpha);
	void (*op_gs_CA)(fz_context *ctx, pdf_processor *proc, float alpha);
	void (*op_gs_SMask)(fz_context *ctx, pdf_processor *proc, pdf_obj *smask, float *bc, int luminosity);
	void (*op_gs_end)(fz_context *ctx, pdf_processor *proc);

	/* special graphics state */
	void (*op_q)(fz_context *ctx, pdf_processor *proc);
	void (*op_Q)(fz_context *ctx, pdf_processor *proc);
	void (*op_cm)(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f);

	/* path construction */
	void (*op_m)(fz_context *ctx, pdf_processor *proc, float x, float y);
	void (*op_l)(fz_context *ctx, pdf_processor *proc, float x, float y);
	void (*op_c)(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x2, float y2, float x3, float y3);
	void (*op_v)(fz_context *ctx, pdf_processor *proc, float x2, float y2, float x3, float y3);
	void (*op_y)(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x3, float y3);
	void (*op_h)(fz_context *ctx, pdf_processor *proc);
	void (*op_re)(fz_context *ctx, pdf_processor *proc, float x, float y, float w, float h);

	/* path painting */
	void (*op_S)(fz_context *ctx, pdf_processor *proc);
	void (*op_s)(fz_context *ctx, pdf_processor *proc);
	void (*op_F)(fz_context *ctx, pdf_processor *proc);
	void (*op_f)(fz_context *ctx, pdf_processor *proc);
	void (*op_fstar)(fz_context *ctx, pdf_processor *proc);
	void (*op_B)(fz_context *ctx, pdf_processor *proc);
	void (*op_Bstar)(fz_context *ctx, pdf_processor *proc);
	void (*op_b)(fz_context *ctx, pdf_processor *proc);
	void (*op_bstar)(fz_context *ctx, pdf_processor *proc);
	void (*op_n)(fz_context *ctx, pdf_processor *proc);

	/* clipping paths */
	void (*op_W)(fz_context *ctx, pdf_processor *proc);
	void (*op_Wstar)(fz_context *ctx, pdf_processor *proc);

	/* text objects */
	void (*op_BT)(fz_context *ctx, pdf_processor *proc);
	void (*op_ET)(fz_context *ctx, pdf_processor *proc);

	/* text state */
	void (*op_Tc)(fz_context *ctx, pdf_processor *proc, float charspace);
	void (*op_Tw)(fz_context *ctx, pdf_processor *proc, float wordspace);
	void (*op_Tz)(fz_context *ctx, pdf_processor *proc, float scale);
	void (*op_TL)(fz_context *ctx, pdf_processor *proc, float leading);
	void (*op_Tf)(fz_context *ctx, pdf_processor *proc, const char *name, pdf_font_desc *font, float size);
	void (*op_Tr)(fz_context *ctx, pdf_processor *proc, int render);
	void (*op_Ts)(fz_context *ctx, pdf_processor *proc, float rise);

	/* text positioning */
	void (*op_Td)(fz_context *ctx, pdf_processor *proc, float tx, float ty);
	void (*op_TD)(fz_context *ctx, pdf_processor *proc, float tx, float ty);
	void (*op_Tm)(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f);
	void (*op_Tstar)(fz_context *ctx, pdf_processor *proc);

	/* text showing */
	void (*op_TJ)(fz_context *ctx, pdf_processor *proc, pdf_obj *array);
	void (*op_Tj)(fz_context *ctx, pdf_processor *proc, char *str, size_t len);
	void (*op_squote)(fz_context *ctx, pdf_processor *proc, char *str, size_t len);
	void (*op_dquote)(fz_context *ctx, pdf_processor *proc, float aw, float ac, char *str, size_t len);

	/* type 3 fonts */
	void (*op_d0)(fz_context *ctx, pdf_processor *proc, float wx, float wy);
	void (*op_d1)(fz_context *ctx, pdf_processor *proc, float wx, float wy, float llx, float lly, float urx, float ury);

	/* color */
	void (*op_CS)(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs);
	void (*op_cs)(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs);
	void (*op_SC_pattern)(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color);
	void (*op_sc_pattern)(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color);
	void (*op_SC_shade)(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade);
	void (*op_sc_shade)(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade);
	void (*op_SC_color)(fz_context *ctx, pdf_processor *proc, int n, float *color);
	void (*op_sc_color)(fz_context *ctx, pdf_processor *proc, int n, float *color);

	void (*op_G)(fz_context *ctx, pdf_processor *proc, float g);
	void (*op_g)(fz_context *ctx, pdf_processor *proc, float g);
	void (*op_RG)(fz_context *ctx, pdf_processor *proc, float r, float g, float b);
	void (*op_rg)(fz_context *ctx, pdf_processor *proc, float r, float g, float b);
	void (*op_K)(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k);
	void (*op_k)(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k);

	/* shadings, images, xobjects */
	void (*op_BI)(fz_context *ctx, pdf_processor *proc, fz_image *image, const char *colorspace_name);
	void (*op_sh)(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade);
	void (*op_Do_image)(fz_context *ctx, pdf_processor *proc, const char *name, fz_image *image);
	void (*op_Do_form)(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *form);

	/* marked content */
	void (*op_MP)(fz_context *ctx, pdf_processor *proc, const char *tag);
	void (*op_DP)(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked);
	void (*op_BMC)(fz_context *ctx, pdf_processor *proc, const char *tag);
	void (*op_BDC)(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked);
	void (*op_EMC)(fz_context *ctx, pdf_processor *proc);

	/* compatibility */
	void (*op_BX)(fz_context *ctx, pdf_processor *proc);
	void (*op_EX)(fz_context *ctx, pdf_processor *proc);

	/* Virtual ops for ExtGState entries */
	void (*op_gs_OP)(fz_context *ctx, pdf_processor *proc, int b);
	void (*op_gs_op)(fz_context *ctx, pdf_processor *proc, int b);
	void (*op_gs_OPM)(fz_context *ctx, pdf_processor *proc, int i);
	void (*op_gs_UseBlackPtComp)(fz_context *ctx, pdf_processor *proc, pdf_obj *name);

	/* END is used to signify end of stream (finalise and close down) */
	void (*op_END)(fz_context *ctx, pdf_processor *proc);

	/* interpreter state that persists across content streams */
	const char *usage;
	int hidden;
};

typedef struct
{
	/* input */
	pdf_document *doc;
	pdf_obj *rdb;
	pdf_lexbuf *buf;
	fz_cookie *cookie;

	/* state */
	int gstate;
	int xbalance;
	int in_text;
	fz_rect d1_rect;

	/* stack */
	pdf_obj *obj;
	char name[256];
	char string[256];
	size_t string_len;
	int top;
	float stack[32];
} pdf_csi;

/* Functions to set up pdf_process structures */

pdf_processor *pdf_new_run_processor(fz_context *ctx, pdf_document *doc, fz_device *dev, fz_matrix ctm, int struct_parent, const char *usage, pdf_gstate *gstate, fz_default_colorspaces *default_cs, fz_cookie *cookie);

/*
	Create a buffer processor.

	This collects the incoming PDF operator stream into an fz_buffer.

	buffer: The (possibly empty) buffer to which operators will be
	appended.

	ahxencode: If 0, then image streams will be send as binary,
	otherwise they will be asciihexencoded.
*/
pdf_processor *pdf_new_buffer_processor(fz_context *ctx, fz_buffer *buffer, int ahxencode);

/*
	Create an output processor. This
	sends the incoming PDF operator stream to an fz_output stream.

	out: The output stream to which operators will be sent.

	ahxencode: If 0, then image streams will be send as binary,
	otherwise they will be asciihexencoded.
*/
pdf_processor *pdf_new_output_processor(fz_context *ctx, fz_output *out, int ahxencode);

typedef struct pdf_filter_options pdf_filter_options;

/*
	Create a filter processor. This filters the PDF operators
	it is fed, and passes them down (with some changes) to the
	child filter.

	chain: The child processor to which the filtered operators
	will be fed.

	The options field contains a pointer to a structure with
	filter specific options in.
*/
typedef pdf_processor *(pdf_filter_factory_fn)(fz_context *ctx, pdf_document *doc, pdf_processor *chain, int struct_parents, fz_matrix transform, pdf_filter_options *options, void *factory_options);

/*
	A pdf_filter_factory is a pdf_filter_factory_fn, plus the options
	needed to instantiate it.
*/
typedef struct
{
	pdf_filter_factory_fn *filter;
	void *options;
} pdf_filter_factory;

/*
	recurse: Filter resources recursively.

	instance_forms: Always recurse on XObject Form resources, but will
	create a new instance of each XObject Form that is used, filtered
	individually.

	ascii: If true, escape all binary data in the output.

	no_update: If true, do not update the document at the end.

	opaque: Opaque value that is passed to the complete function.

	complete: A function called at the end of processing.
	This allows the caller to insert some extra content after
	all other content.

	filters: Pointer to an array of filter factory/options.
	The array is terminated by an entry with a NULL factory pointer.
	Operators will be fed into the filter generated from the first
	factory function in the list, and from there go to the filter
	generated from the second factory in the list etc.
*/
struct pdf_filter_options
{
	int recurse;
	int instance_forms;
	int ascii;
	int no_update;

	void *opaque;
	void (*complete)(fz_context *ctx, fz_buffer *buffer, void *arg);

	pdf_filter_factory *filters;
};

typedef enum
{
	FZ_CULL_PATH_FILL,
	FZ_CULL_PATH_STROKE,
	FZ_CULL_PATH_FILL_STROKE,
	FZ_CULL_CLIP_PATH,
	FZ_CULL_GLYPH,
	FZ_CULL_IMAGE,
	FZ_CULL_SHADING
} fz_cull_type;

/*
	image_filter: A function called to assess whether a given
	image should be removed or not.

	text_filter: A function called to assess whether a given
	character should be removed or not.

	after_text_object: A function called after each text object.
	This allows the caller to insert some extra content if
	desired.

	culler: A function called to see whether each object should
	be culled or not.
*/
typedef struct
{
	void *opaque;
	fz_image *(*image_filter)(fz_context *ctx, void *opaque, fz_matrix ctm, const char *name, fz_image *image);
	int (*text_filter)(fz_context *ctx, void *opaque, int *ucsbuf, int ucslen, fz_matrix trm, fz_matrix ctm, fz_rect bbox);
	void (*after_text_object)(fz_context *ctx, void *opaque, pdf_document *doc, pdf_processor *chain, fz_matrix ctm);
	int (*culler)(fz_context *ctx, void *opaque, fz_rect bbox, fz_cull_type type);
}
pdf_sanitize_filter_options;

/*
	A sanitize filter factory.

	sopts = pointer to pdf_sanitize_filter_options.

	The changes made by a filter generated from this are:

	* No operations are allowed to change the top level gstate.
	Additional q/Q operators are inserted to prevent this.

	* Repeated/unnecessary colour operators are removed (so,
	for example, "0 0 0 rg 0 1 rg 0.5 g" would be sanitised to
	"0.5 g")

	The intention of these changes is to provide a simpler,
	but equivalent stream, repairing problems with mismatched
	operators, maintaining structure (such as BMC, EMC calls)
	and leaving the graphics state in an known (default) state
	so that subsequent operations (such as synthesising new
	operators to be appended to the stream) are easier.

	The net graphical effect of the filtered operator stream
	should be identical to the incoming operator stream.
*/
pdf_processor *pdf_new_sanitize_filter(fz_context *ctx, pdf_document *doc, pdf_processor *chain, int struct_parents, fz_matrix transform, pdf_filter_options *options, void *sopts);

pdf_obj *pdf_filter_xobject_instance(fz_context *ctx, pdf_obj *old_xobj, pdf_obj *page_res, fz_matrix ctm, pdf_filter_options *options, pdf_cycle_list *cycle_up);

void pdf_processor_push_resources(fz_context *ctx, pdf_processor *proc, pdf_obj *res);

pdf_obj *pdf_processor_pop_resources(fz_context *ctx, pdf_processor *proc);

/*
	opaque: Opaque value that is passed to all the filter functions.

	color_rewrite: function pointer called to rewrite a color
		On entry:
			*cs = reference to a pdf object representing the colorspace.

			*n = number of color components

			color = *n color values.

		On exit:
			*cs either the same (for no change in colorspace) or
			updated to be a new one. Reference must be dropped, and
			a new kept reference returned!

			*n = number of color components (maybe updated)

			color = *n color values (maybe updated)

	image_rewrite: function pointer called to rewrite an image
		On entry:
			*image = reference to an fz_image.

		On exit:
			*image either the same (for no change) or updated
			to be a new one. Reference must be dropped, and a
			new kept reference returned.

	share_rewrite: function pointer called to rewrite a shade

	repeated_image_rewrite: If 0, then each image is rewritten only once.
		Otherwise, it is called for every instance (useful if gathering
		information about the ctm).
*/
typedef struct
{
	void *opaque;
	void (*color_rewrite)(fz_context *ctx, void *opaque, pdf_obj **cs, int *n, float color[FZ_MAX_COLORS]);
	void (*image_rewrite)(fz_context *ctx, void *opaque, fz_image **image, fz_matrix ctm, pdf_obj *obj);
	pdf_shade_recolorer *shade_rewrite;
	int repeated_image_rewrite;
} pdf_color_filter_options;

pdf_processor *
pdf_new_color_filter(fz_context *ctx, pdf_document *doc, pdf_processor *chain, int struct_parents, fz_matrix transform, pdf_filter_options *options, void *copts);

/*
	Functions to actually process annotations, glyphs and general stream objects.
*/
void pdf_process_contents(fz_context *ctx, pdf_processor *proc, pdf_document *doc, pdf_obj *res, pdf_obj *stm, fz_cookie *cookie, pdf_obj **out_res);
void pdf_process_annot(fz_context *ctx, pdf_processor *proc, pdf_annot *annot, fz_cookie *cookie);
void pdf_process_glyph(fz_context *ctx, pdf_processor *proc, pdf_document *doc, pdf_obj *resources, fz_buffer *contents);

/*
	Function to process a contents stream without handling the resources.
	The caller is responsible for pushing/popping the resources.
*/
void pdf_process_raw_contents(fz_context *ctx, pdf_processor *proc, pdf_document *doc, pdf_obj *rdb, pdf_obj *stmobj, fz_cookie *cookie);

/* Text handling helper functions */
typedef struct
{
	float char_space;
	float word_space;
	float scale;
	float leading;
	pdf_font_desc *font;
	float size;
	int render;
	float rise;
} pdf_text_state;

typedef struct
{
	fz_text *text;
	fz_rect text_bbox;
	fz_matrix tlm;
	fz_matrix tm;
	int text_mode;

	int cid;
	int gid;
	fz_rect char_bbox;
	pdf_font_desc *fontdesc;
	float char_tx;
	float char_ty;
} pdf_text_object_state;

void pdf_tos_save(fz_context *ctx, pdf_text_object_state *tos, fz_matrix save[2]);
void pdf_tos_restore(fz_context *ctx, pdf_text_object_state *tos, fz_matrix save[2]);
fz_text *pdf_tos_get_text(fz_context *ctx, pdf_text_object_state *tos);
void pdf_tos_reset(fz_context *ctx, pdf_text_object_state *tos, int render);
int pdf_tos_make_trm(fz_context *ctx, pdf_text_object_state *tos, pdf_text_state *text, pdf_font_desc *fontdesc, int cid, fz_matrix *trm);
void pdf_tos_move_after_char(fz_context *ctx, pdf_text_object_state *tos);
void pdf_tos_translate(pdf_text_object_state *tos, float tx, float ty);
void pdf_tos_set_matrix(pdf_text_object_state *tos, float a, float b, float c, float d, float e, float f);
void pdf_tos_newline(pdf_text_object_state *tos, float leading);

#endif
