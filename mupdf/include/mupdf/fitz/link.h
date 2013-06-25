#ifndef MUPDF_FITZ_LINK_H
#define MUPDF_FITZ_LINK_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/math.h"

/*
	Links

	NOTE: The link destination struct is scheduled for imminent change!
	Use at your own peril.
*/

typedef struct fz_link_s fz_link;

typedef struct fz_link_dest_s fz_link_dest;

typedef enum fz_link_kind_e
{
	FZ_LINK_NONE = 0,
	FZ_LINK_GOTO,
	FZ_LINK_URI,
	FZ_LINK_LAUNCH,
	FZ_LINK_NAMED,
	FZ_LINK_GOTOR
} fz_link_kind;

enum {
	fz_link_flag_l_valid = 1, /* lt.x is valid */
	fz_link_flag_t_valid = 2, /* lt.y is valid */
	fz_link_flag_r_valid = 4, /* rb.x is valid */
	fz_link_flag_b_valid = 8, /* rb.y is valid */
	fz_link_flag_fit_h = 16, /* Fit horizontally */
	fz_link_flag_fit_v = 32, /* Fit vertically */
	fz_link_flag_r_is_zoom = 64 /* rb.x is actually a zoom figure */
};

/*
	fz_link_dest: This structure represents the destination of
	an fz_link; this may be a page to display, a new file to open,
	a javascript action to perform, etc.

	kind: This identifies the kind of link destination. Different
	kinds use different sections of the union.

	For FZ_LINK_GOTO or FZ_LINK_GOTOR:

		gotor.page: The target page number to move to (0 being the
		first page in the document).

		gotor.flags: A bitfield consisting of fz_link_flag_*
		describing the validity and meaning of the different parts
		of gotor.lt and gotor.rb. Link destinations are constructed
		(as far as possible) so that lt and rb can be treated as a
		bounding box, though the validity flags indicate which of the
		values was actually specified in the file.

		gotor.lt: The top left corner of the destination bounding box.

		gotor.rb: The bottom right corner of the destination bounding
		box. If fz_link_flag_r_is_zoom is set, then the r figure
		should actually be interpretted as a zoom ratio.

		gotor.file_spec: If set, this destination should cause a new
		file to be opened; this field holds a pointer to a remote
		file specification (UTF-8). Always NULL in the FZ_LINK_GOTO
		case.

		gotor.new_window: If true, the destination should open in a
		new window.

	For FZ_LINK_URI:

		uri.uri: A UTF-8 encoded URI to launch.

		uri.is_map: If true, the x and y coords (as ints, in user
		space) should be appended to the URI before launch.

	For FZ_LINK_LAUNCH:

		launch.file_spec: A UTF-8 file specification to launch.

		launch.new_window: If true, the destination should be launched
		in a new window.

	For FZ_LINK_NAMED:

		named.named: The named action to perform. Likely to be
		client specific.
*/
struct fz_link_dest_s
{
	fz_link_kind kind;
	union
	{
		struct
		{
			int page;
			int flags;
			fz_point lt;
			fz_point rb;
			char *file_spec;
			int new_window;
			char *rname; /* SumatraPDF: allow to resolve against remote documents */
		}
		gotor;
		struct
		{
			char *uri;
			int is_map;
		}
		uri;
		struct
		{
			char *file_spec;
			int new_window;
			/* SumatraPDF: support launching embedded files */
			int embedded_num, embedded_gen;
			/* SumatraPDF: support URL /Filespec */
			int is_url;
		}
		launch;
		struct
		{
			char *named;
		}
		named;
	}
	ld;
};

/*
	fz_link is a list of interactive links on a page.

	There is no relation between the order of the links in the
	list and the order they appear on the page. The list of links
	for a given page can be obtained from fz_load_links.

	A link is reference counted. Dropping a reference to a link is
	done by calling fz_drop_link.

	rect: The hot zone. The area that can be clicked in
	untransformed coordinates.

	dest: Link destinations come in two forms: Page and area that
	an application should display when this link is activated. Or
	as an URI that can be given to a browser.

	next: A pointer to the next link on the same page.
*/
struct fz_link_s
{
	int refs;
	fz_rect rect;
	fz_link_dest dest;
	fz_link *next;
};

fz_link *fz_new_link(fz_context *ctx, const fz_rect *bbox, fz_link_dest dest);
fz_link *fz_keep_link(fz_context *ctx, fz_link *link);

/*
	fz_drop_link: Drop and free a list of links.

	Does not throw exceptions.
*/
void fz_drop_link(fz_context *ctx, fz_link *link);

void fz_free_link_dest(fz_context *ctx, fz_link_dest *dest);

#endif
