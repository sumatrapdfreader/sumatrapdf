#ifndef MUPDF_PDF_ANNOT_H
#define MUPDF_PDF_ANNOT_H

enum pdf_annot_type
{
	PDF_ANNOT_TEXT,
	PDF_ANNOT_LINK,
	PDF_ANNOT_FREE_TEXT,
	PDF_ANNOT_LINE,
	PDF_ANNOT_SQUARE,
	PDF_ANNOT_CIRCLE,
	PDF_ANNOT_POLYGON,
	PDF_ANNOT_POLY_LINE,
	PDF_ANNOT_HIGHLIGHT,
	PDF_ANNOT_UNDERLINE,
	PDF_ANNOT_SQUIGGLY,
	PDF_ANNOT_STRIKE_OUT,
	PDF_ANNOT_REDACT,
	PDF_ANNOT_STAMP,
	PDF_ANNOT_CARET,
	PDF_ANNOT_INK,
	PDF_ANNOT_POPUP,
	PDF_ANNOT_FILE_ATTACHMENT,
	PDF_ANNOT_SOUND,
	PDF_ANNOT_MOVIE,
	PDF_ANNOT_WIDGET,
	PDF_ANNOT_SCREEN,
	PDF_ANNOT_PRINTER_MARK,
	PDF_ANNOT_TRAP_NET,
	PDF_ANNOT_WATERMARK,
	PDF_ANNOT_3D,
	PDF_ANNOT_UNKNOWN = -1
};

/*
	Map an annotation type to a (static) string.

	The returned string must not be freed by the caller.
*/
const char *pdf_string_from_annot_type(fz_context *ctx, enum pdf_annot_type type);

/*
	Map from a (non-NULL, case sensitive) string to an annotation
	type.
*/
enum pdf_annot_type pdf_annot_type_from_string(fz_context *ctx, const char *subtype);

enum
{
	PDF_ANNOT_IS_INVISIBLE = 1 << (1-1),
	PDF_ANNOT_IS_HIDDEN = 1 << (2-1),
	PDF_ANNOT_IS_PRINT = 1 << (3-1),
	PDF_ANNOT_IS_NO_ZOOM = 1 << (4-1),
	PDF_ANNOT_IS_NO_ROTATE = 1 << (5-1),
	PDF_ANNOT_IS_NO_VIEW = 1 << (6-1),
	PDF_ANNOT_IS_READ_ONLY = 1 << (7-1),
	PDF_ANNOT_IS_LOCKED = 1 << (8-1),
	PDF_ANNOT_IS_TOGGLE_NO_VIEW = 1 << (9-1),
	PDF_ANNOT_IS_LOCKED_CONTENTS = 1 << (10-1)
};

enum pdf_line_ending
{
	PDF_ANNOT_LE_NONE = 0,
	PDF_ANNOT_LE_SQUARE,
	PDF_ANNOT_LE_CIRCLE,
	PDF_ANNOT_LE_DIAMOND,
	PDF_ANNOT_LE_OPEN_ARROW,
	PDF_ANNOT_LE_CLOSED_ARROW,
	PDF_ANNOT_LE_BUTT,
	PDF_ANNOT_LE_R_OPEN_ARROW,
	PDF_ANNOT_LE_R_CLOSED_ARROW,
	PDF_ANNOT_LE_SLASH
};

enum
{
	PDF_ANNOT_Q_LEFT = 0,
	PDF_ANNOT_Q_CENTER = 1,
	PDF_ANNOT_Q_RIGHT = 2
};

/*
	Map from a PDF name specifying an annotation line ending
	to an enumerated line ending value.
*/
enum pdf_line_ending pdf_line_ending_from_name(fz_context *ctx, pdf_obj *end);

/*
	Map from a (non-NULL, case sensitive) C string specifying
	an annotation line ending to an enumerated line ending value.
*/
enum pdf_line_ending pdf_line_ending_from_string(fz_context *ctx, const char *end);

/*
	Map from an enumerated line ending to a pdf name object that
	specifies it.
*/
pdf_obj *pdf_name_from_line_ending(fz_context *ctx, enum pdf_line_ending end);

/*
	Map from an enumerated line ending to a C string that specifies
	it.

	The caller must not free the returned string.
*/
const char *pdf_string_from_line_ending(fz_context *ctx, enum pdf_line_ending end);

/*
	Increment the reference count for an annotation.

	Never throws exceptions. Returns the same pointer.
*/
pdf_annot *pdf_keep_annot(fz_context *ctx, pdf_annot *annot);

/*
	Drop the reference count for an annotation.

	When the reference count reaches zero, the annotation will
	be destroyed. Never throws exceptions.
*/
void pdf_drop_annot(fz_context *ctx, pdf_annot *annot);

/*
	Returns a borrowed reference to the first annotation on
	a page, or NULL if none.

	The caller should fz_keep this if it intends to hold the
	pointer. Unless it fz_keeps it, it must not fz_drop it.
*/
pdf_annot *pdf_first_annot(fz_context *ctx, pdf_page *page);

/*
	Returns a borrowed reference to the next annotation
	on a page, or NULL if none.

	The caller should fz_keep this if it intends to hold the
	pointer. Unless it fz_keeps it, it must not fz_drop it.
*/
pdf_annot *pdf_next_annot(fz_context *ctx, pdf_annot *annot);

/*
	Return the rectangle for an annotation on a page.
*/
fz_rect pdf_bound_annot(fz_context *ctx, pdf_annot *annot);

enum pdf_annot_type pdf_annot_type(fz_context *ctx, pdf_annot *annot);

/*
	Interpret an annotation and render it on a device.

	page: A page loaded by pdf_load_page.

	annot: an annotation.

	dev: Device used for rendering, obtained from fz_new_*_device.

	ctm: A transformation matrix applied to the objects on the page,
	e.g. to scale or rotate the page contents as desired.
*/
void pdf_run_annot(fz_context *ctx, pdf_annot *annot, fz_device *dev, fz_matrix ctm, fz_cookie *cookie);

/*
	Lookup needle in the nametree of the document given by which.

	The returned reference is borrowed, and should not be dropped,
	unless it is kept first.
*/
pdf_obj *pdf_lookup_name(fz_context *ctx, pdf_document *doc, pdf_obj *which, pdf_obj *needle);

/*
	Load a nametree, flattening it into a single dictionary.

	The caller is responsible for pdf_dropping the returned
	reference.
*/
pdf_obj *pdf_load_name_tree(fz_context *ctx, pdf_document *doc, pdf_obj *which);

/*
	Lookup needle in the given number tree.

	The returned reference is borrowed, and should not be dropped,
	unless it is kept first.
*/
pdf_obj *pdf_lookup_number(fz_context *ctx, pdf_obj *root, int needle);

/*
	Perform a depth first traversal of a tree.

	Start at tree, looking for children in the array named
	kid_name at each level.

	The arrive callback is called when we arrive at a node (i.e.
	before all the children are walked), and then the leave callback
	is called as we leave it (after all the children have been
	walked).

	names and values are (matching) null terminated arrays of
	names and values to be carried down the tree, to implement
	inheritance. NULL is a permissible value.
*/
void pdf_walk_tree(fz_context *ctx, pdf_obj *tree, pdf_obj *kid_name,
			void (*arrive)(fz_context *, pdf_obj *, void *, pdf_obj **),
			void (*leave)(fz_context *, pdf_obj *, void *),
			void *arg,
			pdf_obj **names,
			pdf_obj **values);

/*
	Resolve a link within a document.
*/
int pdf_resolve_link(fz_context *ctx, pdf_document *doc, const char *uri, float *xp, float *yp);

/*
	Create transform to fit appearance stream to annotation Rect
*/
fz_matrix pdf_annot_transform(fz_context *ctx, pdf_annot *annot);

/*
	create a new annotation of the specified type on the
	specified page. The returned pdf_annot structure is owned by the
	page and does not need to be freed.
*/
pdf_annot *pdf_create_annot_raw(fz_context *ctx, pdf_page *page, enum pdf_annot_type type);

/*
	create a new annotation of the specified type on the
	specified page. Populate it with sensible defaults per the type.

	The page takes a reference, and an additional reference is
	returned to the caller, hence the caller should drop the
	reference once it is done.
*/
pdf_annot *pdf_create_annot(fz_context *ctx, pdf_page *page, enum pdf_annot_type type);

/*
	Delete an annoation from the page.

	This unlinks the annotation from the page structure and drops
	the pages reference to it. Any reference held by the caller
	will not be dropped automatically, so this can safely be used
	on a borrowed reference.
*/
void pdf_delete_annot(fz_context *ctx, pdf_page *page, pdf_annot *annot);

/*
	Edit the associated Popup annotation rectangle.

	Popup annotations are used to store the size and position of the
	popup box that is used to edit the contents of the markup annotation.
*/
void pdf_set_annot_popup(fz_context *ctx, pdf_annot *annot, fz_rect rect);
fz_rect pdf_annot_popup(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has an ink list.
*/
int pdf_annot_has_ink_list(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has quad points data.
*/
int pdf_annot_has_quad_points(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has vertex data.
*/
int pdf_annot_has_vertices(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has line data.
*/
int pdf_annot_has_line(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has an interior color.
*/
int pdf_annot_has_interior_color(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has line ending styles.
*/
int pdf_annot_has_line_ending_styles(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has an icon name.
*/
int pdf_annot_has_icon_name(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has an open action.
*/
int pdf_annot_has_open(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has author data.
*/
int pdf_annot_has_author(fz_context *ctx, pdf_annot *annot);

/*
	Retrieve the annotation flags.
*/
int pdf_annot_flags(fz_context *ctx, pdf_annot *annot);

/*
	Retrieve the annotation bounds in doc space.
*/
fz_rect pdf_annot_rect(fz_context *ctx, pdf_annot *annot);

/*
	Retrieve the annotation border line width in points.
*/
float pdf_annot_border(fz_context *ctx, pdf_annot *annot);

/*
	Retrieve the annotation opacity. (0 transparent, 1 solid).
*/
float pdf_annot_opacity(fz_context *ctx, pdf_annot *annot);

/*
	Retrieve the annotation color.

	n components, each between 0 and 1.
	n = 1 (grey), 3 (rgb) or 4 (cmyk).
*/
void pdf_annot_color(fz_context *ctx, pdf_annot *annot, int *n, float color[4]);

/*
	Retrieve the annotation interior color.

	n components, each between 0 and 1.
	n = 1 (grey), 3 (rgb) or 4 (cmyk).
*/
void pdf_annot_interior_color(fz_context *ctx, pdf_annot *annot, int *n, float color[4]);

/*
	Retrieve the annotation quadding (justification) to use.
		0 = Left-justified
		1 = Centered
		2 = Right-justified
*/
int pdf_annot_quadding(fz_context *ctx, pdf_annot *annot);

/*
	Retrieve the annotations text language (either from the
	annotation, or from the document).
*/
fz_text_language pdf_annot_language(fz_context *ctx, pdf_annot *annot);

/*
	How many quad points does an annotation have?
*/
int pdf_annot_quad_point_count(fz_context *ctx, pdf_annot *annot);

/*
	Get quadpoint i for an annotation.
*/
fz_quad pdf_annot_quad_point(fz_context *ctx, pdf_annot *annot, int i);

/*
	How many strokes in the ink list for an annotation?
*/
int pdf_annot_ink_list_count(fz_context *ctx, pdf_annot *annot);

/*
	How many vertexes in stroke i of the ink list for an annotation?
*/
int pdf_annot_ink_list_stroke_count(fz_context *ctx, pdf_annot *annot, int i);

/*
	Get vertex k from stroke i of the ink list for an annoation, in
	doc space.
*/
fz_point pdf_annot_ink_list_stroke_vertex(fz_context *ctx, pdf_annot *annot, int i, int k);

/*
	Set the flags for an annotation.
*/
void pdf_set_annot_flags(fz_context *ctx, pdf_annot *annot, int flags);

/*
	Set the bounding box for an annotation, in doc space.
*/
void pdf_set_annot_rect(fz_context *ctx, pdf_annot *annot, fz_rect rect);

/*
	Set the border width for an annotation, in points.
*/
void pdf_set_annot_border(fz_context *ctx, pdf_annot *annot, float width);

/*
	Set the opacity for an annotation, between 0 (transparent) and 1
	(solid).
*/
void pdf_set_annot_opacity(fz_context *ctx, pdf_annot *annot, float opacity);

/*
	Set the annotation color.

	n components, each between 0 and 1.
	n = 1 (grey), 3 (rgb) or 4 (cmyk).
*/
void pdf_set_annot_color(fz_context *ctx, pdf_annot *annot, int n, const float *color);

/*
	Set the annotation interior color.

	n components, each between 0 and 1.
	n = 1 (grey), 3 (rgb) or 4 (cmyk).
*/
void pdf_set_annot_interior_color(fz_context *ctx, pdf_annot *annot, int n, const float *color);

/*
	Set the quadding (justification) to use for the annotation.
		0 = Left-justified
		1 = Centered
		2 = Right-justified
*/
void pdf_set_annot_quadding(fz_context *ctx, pdf_annot *annot, int q);

/*
	Set the language for the annotation.
*/
void pdf_set_annot_language(fz_context *ctx, pdf_annot *annot, fz_text_language lang);

/*
	Set the quad points for an annotation to those in the qv array
	of length n.
*/
void pdf_set_annot_quad_points(fz_context *ctx, pdf_annot *annot, int n, const fz_quad *qv);

/*
	Clear the quadpoint data for an annotation.
*/
void pdf_clear_annot_quad_points(fz_context *ctx, pdf_annot *annot);

/*
	Append a new quad point to the quad point data in an annotation.
*/
void pdf_add_annot_quad_point(fz_context *ctx, pdf_annot *annot, fz_quad quad);

/*
	Set the ink list for an annotation.

	n strokes. For 0 <= i < n, stroke i has count[i] points,
	The vertexes for all the strokes are packed into a single
	array, pointed to by v.
*/
void pdf_set_annot_ink_list(fz_context *ctx, pdf_annot *annot, int n, const int *count, const fz_point *v);

/*
	Clear the ink list for an annotation.
*/
void pdf_clear_annot_ink_list(fz_context *ctx, pdf_annot *annot);

/*
	Add a new stroke (initially empty) to the ink list for an
	annotation.
*/
void pdf_add_annot_ink_list_stroke(fz_context *ctx, pdf_annot *annot);

/*
	Add a new vertex to the last stroke in the ink list for an
	annotation.
*/
void pdf_add_annot_ink_list_stroke_vertex(fz_context *ctx, pdf_annot *annot, fz_point p);

/*
	Add a new stroke to the ink list for an annotation, and
	populate it with the n points from stroke[].
*/
void pdf_add_annot_ink_list(fz_context *ctx, pdf_annot *annot, int n, fz_point stroke[]);

/*

*/
void pdf_set_annot_icon_name(fz_context *ctx, pdf_annot *annot, const char *name);
void pdf_set_annot_is_open(fz_context *ctx, pdf_annot *annot, int is_open);

enum pdf_line_ending pdf_annot_line_start_style(fz_context *ctx, pdf_annot *annot);
enum pdf_line_ending pdf_annot_line_end_style(fz_context *ctx, pdf_annot *annot);
void pdf_annot_line_ending_styles(fz_context *ctx, pdf_annot *annot, enum pdf_line_ending *start_style, enum pdf_line_ending *end_style);
void pdf_set_annot_line_start_style(fz_context *ctx, pdf_annot *annot, enum pdf_line_ending s);
void pdf_set_annot_line_end_style(fz_context *ctx, pdf_annot *annot, enum pdf_line_ending e);
void pdf_set_annot_line_ending_styles(fz_context *ctx, pdf_annot *annot, enum pdf_line_ending start_style, enum pdf_line_ending end_style);

const char *pdf_annot_icon_name(fz_context *ctx, pdf_annot *annot);
int pdf_annot_is_open(fz_context *ctx, pdf_annot *annot);

void pdf_annot_line(fz_context *ctx, pdf_annot *annot, fz_point *a, fz_point *b);
void pdf_set_annot_line(fz_context *ctx, pdf_annot *annot, fz_point a, fz_point b);

int pdf_annot_vertex_count(fz_context *ctx, pdf_annot *annot);
fz_point pdf_annot_vertex(fz_context *ctx, pdf_annot *annot, int i);

void pdf_set_annot_vertices(fz_context *ctx, pdf_annot *annot, int n, const fz_point *v);
void pdf_clear_annot_vertices(fz_context *ctx, pdf_annot *annot);
void pdf_add_annot_vertex(fz_context *ctx, pdf_annot *annot, fz_point p);
void pdf_set_annot_vertex(fz_context *ctx, pdf_annot *annot, int i, fz_point p);

const char *pdf_annot_contents(fz_context *ctx, pdf_annot *annot);
void pdf_set_annot_contents(fz_context *ctx, pdf_annot *annot, const char *text);

const char *pdf_annot_author(fz_context *ctx, pdf_annot *annot);
void pdf_set_annot_author(fz_context *ctx, pdf_annot *annot, const char *author);

int64_t pdf_annot_modification_date(fz_context *ctx, pdf_annot *annot);
void pdf_set_annot_modification_date(fz_context *ctx, pdf_annot *annot, int64_t time);
int64_t pdf_annot_creation_date(fz_context *ctx, pdf_annot *annot);
void pdf_set_annot_creation_date(fz_context *ctx, pdf_annot *annot, int64_t time);

void pdf_parse_default_appearance(fz_context *ctx, const char *da, const char **font, float *size, float color[3]);
void pdf_print_default_appearance(fz_context *ctx, char *buf, int nbuf, const char *font, float size, const float color[3]);
void pdf_annot_default_appearance(fz_context *ctx, pdf_annot *annot, const char **font, float *size, float color[3]);
void pdf_set_annot_default_appearance(fz_context *ctx, pdf_annot *annot, const char *font, float size, const float color[3]);

void pdf_dirty_annot(fz_context *ctx, pdf_annot *annot);

/*
	Recreate the appearance stream for an annotation, if necessary.
*/
void pdf_update_appearance(fz_context *ctx, pdf_annot *annot);
void pdf_update_signature_appearance(fz_context *ctx, pdf_annot *annot, const char *name, const char *text, const char *date);

/*
	Regenerate any appearance streams that are out of date and check for
	cases where a different appearance stream should be selected because of
	state changes.

	Note that a call to pdf_pass_event for one page may lead to changes on
	any other, so an app should call pdf_update_annot for every annotation
	it currently displays. Also it is important that the pdf_annot object
	is the one used to last render the annotation. If instead the app were
	to drop the page or annotations and reload them then a call to
	pdf_update_annot would not reliably be able to report all changed
	annotations.

	Returns true if the annotation appearance has changed since the last time
	pdf_update_annot was called or the annotation was first loaded.
*/
int pdf_update_annot(fz_context *ctx, pdf_annot *annot);

/*
	Recalculate form fields if necessary.

	Loop through all annotations on the page and update them. Return true
	if any of them were changed (by either event or javascript actions, or
	by annotation editing) and need re-rendering.

	If you need more granularity, loop through the annotations and call
	pdf_update_annot for each one to detect changes on a per-annotation
	basis.
*/
int pdf_update_page(fz_context *ctx, pdf_page *page);

/*
	Update internal state appropriate for editing this field. When editing
	is true, updating the text of the text widget will not have any
	side-effects such as changing other widgets or running javascript.
	This state is intended for the period when a text widget is having
	characters typed into it. The state should be reverted at the end of
	the edit sequence and the text newly updated.
*/
void pdf_set_widget_editing_state(fz_context *ctx, pdf_widget *widget, int editing);

int pdf_get_widget_editing_state(fz_context *ctx, pdf_widget *widget);

/*
	Toggle the state of a specified annotation. Applies only to check-box
	and radio-button widgets.
*/
int pdf_toggle_widget(fz_context *ctx, pdf_widget *widget);

fz_display_list *pdf_new_display_list_from_annot(fz_context *ctx, pdf_annot *annot);

/*
	Render an annotation suitable for blending on top of the opaque
	pixmap returned by fz_new_pixmap_from_page_contents.
*/
fz_pixmap *pdf_new_pixmap_from_annot(fz_context *ctx, pdf_annot *annot, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha);
fz_stext_page *pdf_new_stext_page_from_annot(fz_context *ctx, pdf_annot *annot, const fz_stext_options *options);

fz_layout_block *pdf_layout_text_widget(fz_context *ctx, pdf_annot *annot);

const char *pdf_guess_mime_type_from_file_name(fz_context *ctx, const char *filename);
pdf_obj *pdf_embedded_file_stream(fz_context *ctx, pdf_obj *fs);
const char *pdf_embedded_file_name(fz_context *ctx, pdf_obj *fs);
const char *pdf_embedded_file_type(fz_context *ctx, pdf_obj *fs);
int pdf_is_embedded_file(fz_context *ctx, pdf_obj *fs);
fz_buffer *pdf_load_embedded_file(fz_context *ctx, pdf_obj *fs);
pdf_obj *pdf_add_embedded_file(fz_context *ctx, pdf_document *doc, const char *filename, const char *mimetype, fz_buffer *contents);

/* Implementation details: Subject to change */

struct pdf_annot
{
	int refs;

	pdf_page *page;
	pdf_obj *obj;

	pdf_obj *ap;

	int is_hot;
	int is_active;

	int needs_new_ap;
	int has_new_ap;
	int ignore_trigger_events;

	pdf_annot *next;
};

char *pdf_parse_link_dest(fz_context *ctx, pdf_document *doc, pdf_obj *obj);
char *pdf_parse_link_action(fz_context *ctx, pdf_document *doc, pdf_obj *obj, int pagenum);
pdf_obj *pdf_lookup_dest(fz_context *ctx, pdf_document *doc, pdf_obj *needle);
fz_link *pdf_load_link_annots(fz_context *ctx, pdf_document *, pdf_obj *annots, int pagenum, fz_matrix page_ctm);
void pdf_load_annots(fz_context *ctx, pdf_page *page, pdf_obj *annots);
void pdf_drop_annots(fz_context *ctx, pdf_annot *annot_list);
void pdf_drop_widgets(fz_context *ctx, pdf_widget *widget_list);

void pdf_annot_MK_BG(fz_context *ctx, pdf_annot *annot, int *n, float color[4]);
void pdf_annot_MK_BC(fz_context *ctx, pdf_annot *annot, int *n, float color[4]);
int pdf_annot_MK_BG_rgb(fz_context *ctx, pdf_annot *annot, float rgb[3]);
int pdf_annot_MK_BC_rgb(fz_context *ctx, pdf_annot *annot, float rgb[3]);


#endif
