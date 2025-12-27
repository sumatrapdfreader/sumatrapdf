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

#ifndef MUPDF_FITZ_STRUCTURED_TEXT_H
#define MUPDF_FITZ_STRUCTURED_TEXT_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/types.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/font.h"
#include "mupdf/fitz/image.h"
#include "mupdf/fitz/output.h"
#include "mupdf/fitz/device.h"
#include "mupdf/fitz/pool.h"

/**
	Simple text layout (for use with annotation editing primarily).
*/
typedef struct fz_layout_char
{
	float x, advance;
	const char *p; /* location in source text of character */
	struct fz_layout_char *next;
} fz_layout_char;

typedef struct fz_layout_line
{
	float x, y, font_size;
	const char *p; /* location in source text of start of line */
	fz_layout_char *text;
	struct fz_layout_line *next;
} fz_layout_line;

typedef struct
{
	fz_pool *pool;
	fz_matrix matrix;
	fz_matrix inv_matrix;
	fz_layout_line *head, **tailp;
	fz_layout_char **text_tailp;
} fz_layout_block;

/**
	Create a new layout block, with new allocation pool, zero
	matrices, and initialise linked pointers.
*/
fz_layout_block *fz_new_layout(fz_context *ctx);

/**
	Drop layout block. Free the pool, and linked blocks.

	Never throws exceptions.
*/
void fz_drop_layout(fz_context *ctx, fz_layout_block *block);

/**
	Add a new line to the end of the layout block.
*/
void fz_add_layout_line(fz_context *ctx, fz_layout_block *block, float x, float y, float h, const char *p);

/**
	Add a new char to the line at the end of the layout block.
*/
void fz_add_layout_char(fz_context *ctx, fz_layout_block *block, float x, float w, const char *p);

/**
	Text extraction device: Used for searching, format conversion etc.

	(In development - Subject to change in future versions)
*/

typedef struct fz_stext_char fz_stext_char;
typedef struct fz_stext_line fz_stext_line;
typedef struct fz_stext_block fz_stext_block;
typedef struct fz_stext_struct fz_stext_struct;
typedef struct fz_stext_grid_positions fz_stext_grid_positions;

/**
	FZ_STEXT_PRESERVE_LIGATURES: If this option is activated
	ligatures are passed through to the application in their
	original form. If this option is deactivated ligatures are
	expanded into their constituent parts, e.g. the ligature ffi is
	expanded into three separate characters f, f and i.

	FZ_STEXT_PRESERVE_WHITESPACE: If this option is activated
	whitespace is passed through to the application in its original
	form. If this option is deactivated any type of horizontal
	whitespace (including horizontal tabs) will be replaced with
	space characters of variable width.

	FZ_STEXT_PRESERVE_IMAGES: If this option is set, then images
	will be stored in the structured text structure. The default is
	to ignore all images.

	FZ_STEXT_INHIBIT_SPACES: If this option is set, we will not try
	to add missing space characters where there are large gaps
	between characters.

	FZ_STEXT_DEHYPHENATE: If this option is set, hyphens at the
	end of a line will be recorded as being soft-hyphens; when flattened
	soft-hyphens at the end of lines will cause the lines to be
	joined.

	FZ_STEXT_PRESERVE_SPANS: If this option is set, spans on the same line
	will not be merged. Each line will thus be a span of text with the same
	font, colour, and size.

	FZ_STEXT_CLIP: If this option is set, characters that would be entirely
	clipped away by the current clipping path (or, more accurate, the smallest
	bbox that contains the current clipping path) will be ignored. The
	clip path is guaranteed to be smaller then the page mediabox, hence
	this option subsumes an older, now deprecated, FZ_STEXT_MEDIABOX_CLIP
	option.

	FZ_STEXT_CLIP_RECT: If this option is set, characters that would be entirely
	clipped away by the specified 'clip' rectangle in the options struct
	will be ignored. This enables content from specific subsections of pages to
	be extracted.

	FZ_STEXT_COLLECT_STRUCTURE: If this option is set, we will collect
	the structure as specified using begin/end_structure calls. This will
	change the returned stext structure from being a simple list of blocks
	into effectively being a 'tree' that should be walked in depth-first
	order.

	FZ_STEXT_COLLECT_VECTORS: If this option is set, we will collect
	details (currently just the bbox) of vector graphics. This is intended
	to be of use in segmentation analysis.

	FZ_STEXT_IGNORE_ACTUALTEXT: If this option is set, we will no longer
	replace text by the ActualText replacement specified in the document.

	FZ_STEXT_SEGMENT: If this option is set, we will attempt to segment
	the page into different regions. This will deliberately not do anything
	to pages with structure information present.

	FZ_STEXT_PARAGRAPH_BREAK: If this option is set, we will break blocks
	of text at what appear to be paragraph boundaries. This only works
	for left-to-right, top-to-bottom paragraphs. Works best on a segmented
	page.

	FZ_STEXT_TABLE_HUNT: If this option is set, we will hunt for tables
	within the stext. Details of the potential tables found will be
	inserted into the stext for the caller to interpret. This will work
	best on a segmented page.

	FZ_STEXT_USE_CID_FOR_UNKNOWN_UNICODE: If this option is set, then
	in the event that we fail to find a unicode value for a given
	character, we we instead return its CID in the unicode field. We
	will set the FZ_STEXT_UNICODE_IS_CID bit in the char flags word to
	indicate that this has happened.

	FZ_STEXT_USE_GID_FOR_UNKNOWN_UNICODE: If this option is set, then
	in the event that we fail to find a unicode value for a given
	character, we we instead return its glyph in the unicode field.
	We will set the FZ_STEXT_UNICODE_IS_GID bit in the char flags word
	to indicate that this has happened.

	Setting both FZ_STEXT_USE_CID_FOR_UNKNOWN_UNICODE and
	FZ_STEXT_USE_GID_FOR_UNKNOWN_UNICODE will give undefined behaviour.

*/
enum
{
	FZ_STEXT_PRESERVE_LIGATURES = 1,
	FZ_STEXT_PRESERVE_WHITESPACE = 2,
	FZ_STEXT_PRESERVE_IMAGES = 4,
	FZ_STEXT_INHIBIT_SPACES = 8,
	FZ_STEXT_DEHYPHENATE = 16,
	FZ_STEXT_PRESERVE_SPANS = 32,
	FZ_STEXT_CLIP = 64,
	FZ_STEXT_USE_CID_FOR_UNKNOWN_UNICODE = 128,
	FZ_STEXT_COLLECT_STRUCTURE = 256,
	FZ_STEXT_ACCURATE_BBOXES = 512,
	FZ_STEXT_COLLECT_VECTORS = 1024,
	FZ_STEXT_IGNORE_ACTUALTEXT = 2048,
	FZ_STEXT_SEGMENT = 4096,
	FZ_STEXT_PARAGRAPH_BREAK = 8192,
	FZ_STEXT_TABLE_HUNT = 16384,
	FZ_STEXT_COLLECT_STYLES = 32768,
	FZ_STEXT_USE_GID_FOR_UNKNOWN_UNICODE = 65536,
	FZ_STEXT_CLIP_RECT = (1<<17),
	FZ_STEXT_ACCURATE_ASCENDERS = (1<<18),
	FZ_STEXT_ACCURATE_SIDE_BEARINGS = (1<<19),

	/* An old, deprecated option. */
	FZ_STEXT_MEDIABOX_CLIP = FZ_STEXT_CLIP
};

/**
 *	A note on stext's handling of structure.
 *
 *	A PDF document can contain a structure tree. This gives the
 *	structure of a document in its entirety as a tree. e.g.
 *
 *	Tree			MCID	INDEX
 *	-------------------------------------
 *	DOC			0	0
 *	 TOC			1	0
 *	  TOC_ITEM		2	0
 *	  TOC_ITEM		3	1
 *	  TOC_ITEM		4	2
 *	  ...
 *	 STORY			100	1
 *	  SECTION		101	0
 *	   HEADING		102	0
 *	   SUBSECTION		103	1
 *	    PARAGRAPH		104	0
 *	    PARAGRAPH		105	1
 *	    PARAGRAPH		106	2
 *	   SUBSECTION		107	2
 *	    PARAGRAPH		108	0
 *	    PARAGRAPH		109	1
 *	    PARAGRAPH		110	2
 *	   ...
 *	  SECTION		200	1
 *      ...
 *
 *	Each different section of the tree is identified as part of an
 *	MCID by a number (this is a slight simplification, but makes the
 *	explanation easier).
 *
 *	The PDF document contains markings that say "Entering MCID 0"
 *	and "Leaving MCID 0". Any content within that region is therefore
 *	identified as appearing in that particular structural region.
 *
 *	This means that content can be sent in the document in a different
 *	order to which it appears 'logically' in the tree.
 *
 *	MuPDF converts this tree form into a nested series of calls to
 *	begin_structure and end_structure.
 *
 *	For instance, if the document started out with MCID 100, then
 *	we'd send:
 *		begin_structure("DOC")
 *		begin_structure("STORY")
 *
 *	The problem with this is that if we send:
 *		begin_structure("DOC")
 *		begin_structure("STORY")
 *		begin_structure("SECTION")
 *		begin_structure("SUBSECTION")
 *
 *	or
 *		begin_structure("DOC")
 *		begin_structure("STORY")
 *		begin_structure("SECTION")
 *		begin_structure("HEADING")
 *
 *	How do I know what order the SECTION and HEADING should appear in?
 *	Are they even in the same STORY? Or the same DOC?
 *
 *	Accordingly, every begin_structure is accompanied not only with the
 *	node type, but with an index. The index is the number of this node
 *	within this level of the tree. Hence:
 *
 *		begin_structure("DOC", 0)
 *		begin_structure("STORY", 0)
 *		begin_structure("SECTION", 0)
 *		begin_structure("HEADING", 0)
 *	and
 *		begin_structure("DOC", 0)
 *		begin_structure("STORY", 0)
 *		begin_structure("SECTION", 0)
 *		begin_structure("SUBSECTION", 1)
 *
 *	are now unambiguous in their describing of the tree.
 *
 *	MuPDF automatically sends the minimal end_structure/begin_structure
 *	pairs to move us between nodes in the tree.
 *
 *	In order to accommodate this information within the structured text
 *	data structures an additional block type is used. Previously a
 *	"page" was just a list of blocks, either text or images. e.g.
 *
 *	[BLOCK:TEXT] <-> [BLOCK:IMG] <-> [BLOCK:TEXT] <-> [BLOCK:TEXT] ...
 *
 *	We now introduce a new type of block, STRUCT, that turns this into
 *	a tree:
 *
 *	[BLOCK:TEXT] <-> [BLOCK:STRUCT(IDX=0)] <-> [BLOCK:TEXT] <-> ...
 *	                      /|\
 *	[STRUCT:TYPE=DOC] <----
 *	    |
 *	[BLOCK:TEXT] <-> [BLOCK:STRUCT(IDX=0)] <-> [BLOCK:TEXT] <-> ...
 *	                      /|\
 *	[STRUCT:TYPE=STORY] <--
 *	    |
 *	   ...
 *
 *	Rather than doing a simple linear traversal of the list to extract
 *	the logical data, a caller now has to do a depth-first traversal.
 */

typedef struct
{
	fz_rect mediabox;
	int chapter;
	int page;
} fz_stext_page_details;

/**
	A text page is a list of blocks, together with an overall
	bounding box.

	The name of this structure is now slightly out of date. It
	should really be fz_stext_document, cos it can contain
	content from multiple pages.
*/
typedef struct
{
	int refs;
	fz_pool *pool;
	fz_rect mediabox;
	fz_stext_block *first_block;

	/* The following fields are only of use to the routines that
	 * build an fz_stext_page. They change during page construction
	 * and their meaning is subject to change. These values should
	 * not be used by anything outside of the stext device. */
	fz_stext_block *last_block;
	fz_stext_struct *last_struct;

	/* An array of fz_stext_page_details */
	fz_pool_array *id_list;
} fz_stext_page;

/**
	Take a new reference to an fz_stext_page.
*/
fz_stext_page *fz_keep_stext_page(fz_context *ctx, fz_stext_page *page);

/**
	Helper function to retrieve the details for a given id from a block.
*/
fz_stext_page_details *fz_stext_page_details_for_block(fz_context *ctx, fz_stext_page *page, fz_stext_block *block);

enum
{
	FZ_STEXT_BLOCK_TEXT = 0,
	FZ_STEXT_BLOCK_IMAGE = 1,
	FZ_STEXT_BLOCK_STRUCT = 2,
	FZ_STEXT_BLOCK_VECTOR = 3,
	FZ_STEXT_BLOCK_GRID = 4
};

enum
{
	FZ_STEXT_TEXT_JUSTIFY_UNKNOWN = 0,
	FZ_STEXT_TEXT_JUSTIFY_LEFT = 1,
	FZ_STEXT_TEXT_JUSTIFY_CENTRE = 2,
	FZ_STEXT_TEXT_JUSTIFY_RIGHT = 3,
	FZ_STEXT_TEXT_JUSTIFY_FULL = 4,
};

enum
{
	/* Indicates that this vector came from a stroked
	 * path. */
	FZ_STEXT_VECTOR_IS_STROKED = 1,

	/* Indicates that this vector came from a rectangular
	 * (axis-aligned) path (or path segment). */
	FZ_STEXT_VECTOR_IS_RECTANGLE = 2,

	/* Indicates that this vector came from a path
	 * segment, and more segments from this same path are
	 * still to come. */
	FZ_STEXT_VECTOR_CONTINUES = 4
};

enum
{
	/* Indicates that cell contents cross the right hand edge. */
	FZ_STEXT_GRID_H_CROSSED = 1,
	/* Indicates that cell contents cross the bottom edge. */
	FZ_STEXT_GRID_V_CROSSED = 2,
	/* Indicates that the cell has a border on the left hand edge. */
	FZ_STEXT_GRID_L_BORDER = 4,
	/* Indicates that the cell has a border on the top edge. */
	FZ_STEXT_GRID_T_BORDER = 8,
	/* Indicates that the cell has content (which may be a space!) */
	FZ_STEXT_GRID_FULL = 16,
};

/* This structure is experimental, and subject to change. */
typedef struct
{
	/* A 2x2 table, will be represented as a 3x3 set of
	 * cells. The rightmost column and bottommost row
	 * exist just to give information about borders on
	 * the edges. For such a table w=h=3.
	 */
	int w;
	int h;
	/* Followed by w*h entries. */
	struct {
		unsigned int flags;
	} info[FZ_FLEXIBLE_ARRAY];
} fz_stext_grid_info;

/**
	A text block is a list of lines of text (typically a paragraph),
	or an image.
*/
struct fz_stext_block
{
	int type;
	int id;
	fz_rect bbox;
	union {
		struct { fz_stext_line *first_line, *last_line; int flags;} t;
		struct { fz_matrix transform; fz_image *image; } i;
		struct { fz_stext_struct *down; int index; } s;
		struct { uint32_t flags; uint32_t argb; } v;
		struct { fz_stext_grid_positions *xs; fz_stext_grid_positions *ys; fz_stext_grid_info *info; } b;
	} u;
	fz_stext_block *prev, *next;
};

typedef enum
{
	FZ_STEXT_LINE_FLAGS_JOINED = 1
} fz_stext_line_flags;

/**
	A text line is a list of characters that share a common baseline.
*/
struct fz_stext_line
{
	uint8_t wmode; /* 0 for horizontal, 1 for vertical */
	uint8_t flags;
	fz_point dir; /* normalized direction of baseline */
	fz_rect bbox;
	fz_stext_char *first_char, *last_char;
	fz_stext_line *prev, *next;
};

/**
	A text char is a unicode character, the style in which is
	appears, and the point at which it is positioned.
*/
struct fz_stext_char
{
	int c; /* unicode character value */
	uint16_t bidi; /* even for LTR, odd for RTL - probably only needs 8 bits? */
	uint16_t flags;
	uint32_t argb; /* sRGB hex color (alpha in top 8 bits, then r, then g, then b in low bits) */
	fz_point origin;
	fz_quad quad;
	float size;
	fz_font *font;
	fz_stext_char *next;
};

enum
{
	FZ_STEXT_STRIKEOUT = 1,
	FZ_STEXT_UNDERLINE = 2,
	FZ_STEXT_SYNTHETIC = 4,
	FZ_STEXT_BOLD = 8, /* Either real or 'fake' bold */
	FZ_STEXT_FILLED = 16,
	FZ_STEXT_STROKED = 32,
	FZ_STEXT_CLIPPED = 64,
	FZ_STEXT_UNICODE_IS_CID = 128,
	FZ_STEXT_UNICODE_IS_GID = 256,
	FZ_STEXT_SYNTHETIC_LARGE = 512,
};

/**
	When we are collecting the structure information from
	PDF structure trees/tags, we end up with a tree of
	nodes. The structure should be walked in depth-first
	traversal order to extract the content.

	An fz_stext_struct pointer can be NULL to indicate that
	we know there is a child there within the complete tree,
	but we don't know what it is yet.
*/
struct fz_stext_struct
{
	/* up points to the block that contains this fz_stext_struct. */
	fz_stext_block *up;
	/* parent points to the struct that has up as one of its children.
	 * parent is useful for doing depth first traversal without having
	 * to store the entire chain of structs in the iterator. */
	fz_stext_struct *parent;

	/* first_block points to the first child of this node (or NULL
	 * if there are none). */
	fz_stext_block *first_block;
	/* last_block points to the last child of this node (or NULL
	 * if there are none). */
	fz_stext_block *last_block;

	/* We have a set of 'standard' structure types. Every structure
	 * element should correspond to one of these. */
	fz_structure standard;
	/* Documents can use their own non-standard structure types, which
	 * are held as 'raw' strings. */
	char raw[FZ_FLEXIBLE_ARRAY];
};

/* An example to show how fz_stext_blocks and fz_stext_structs interact:
 *
 *         [fz_stext_page]
 *             |
 *  first_block|
 *             |
 *            \|/
 *  [fz_stext_block:TEXT]<->[fz_stext_block:STRUCT]<->[fz_stext_block:IMG]
 *                           u.s.down|   /|\
 *                                   |    |
 *                                  \|/   |up
 *                             [fz_stext_struct]<---------.
 *                                |       |               |
 *                     first_block|       |last_block     |
 *         _______________________|       |               |
 *        |                               |               |
 *        |                               |               |
 *       \|/                             \|/              |
 *  [fz_stext_block:...]<->...<->[fz_stext_block:STRUCT]  |
 *                                  |  /|\                |
 *                          u.s.down|   |up               |
 *                                 \|/  |           parent|
 *                               [fz_stext_struct]--------'
 *                                  |   |
 *                       first_block|   |last_block
 *                                  :   :
 */

 struct fz_stext_grid_positions
 {
	int len;
	int max_uncertainty;
	struct {
		int reinforcement;
		float pos;
		float min;
		float max;
		int uncertainty;
	} list[FZ_FLEXIBLE_ARRAY];
 };

FZ_DATA extern const char *fz_stext_options_usage;

/**
	Create an empty text page.

	The text page is filled out by the text device to contain the
	blocks and lines of text on the page.

	mediabox: optional mediabox information.
*/
fz_stext_page *fz_new_stext_page(fz_context *ctx, fz_rect mediabox);
void fz_drop_stext_page(fz_context *ctx, fz_stext_page *page);

/**
	Output structured text to a file in HTML (visual) format.
*/
void fz_print_stext_page_as_html(fz_context *ctx, fz_output *out, fz_stext_page *page, int id);
void fz_print_stext_header_as_html(fz_context *ctx, fz_output *out);
void fz_print_stext_trailer_as_html(fz_context *ctx, fz_output *out);

/**
	Output structured text to a file in XHTML (semantic) format.
*/
void fz_print_stext_page_as_xhtml(fz_context *ctx, fz_output *out, fz_stext_page *page, int id);
void fz_print_stext_header_as_xhtml(fz_context *ctx, fz_output *out);
void fz_print_stext_trailer_as_xhtml(fz_context *ctx, fz_output *out);

/**
	Output structured text to a file in XML format.
*/
void fz_print_stext_page_as_xml(fz_context *ctx, fz_output *out, fz_stext_page *page, int id);

/**
	Output structured text to a file in JSON format.
*/
void fz_print_stext_page_as_json(fz_context *ctx, fz_output *out, fz_stext_page *page, float scale);

/**
	Output structured text to a file in plain-text UTF-8 format.
*/
void fz_print_stext_page_as_text(fz_context *ctx, fz_output *out, fz_stext_page *page);

/**
	Search for occurrence of 'needle' in text page.
	Case insensitive match.

	Return the number of quads and store hit quads in the passed in
	array.

	NOTE: This is an experimental interface and subject to change
	without notice.
*/
int fz_search_stext_page(fz_context *ctx, fz_stext_page *text, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max);

/**
	Callback function for use in searching.

	Called with the list of quads that correspond to a single hit.

	The callback should return with 0 to continue the search, or 1 to abort it.
	All other values are reserved at this point.
*/
typedef int (fz_search_callback_fn)(fz_context *ctx, void *opaque, int num_quads, fz_quad *hit_bbox);

/**
	Callback function for use in searching.

	Called with the list of quads that correspond to a single hit.

	The callback should return with 0 to continue the search, or 1 to abort it.
	All other values are reserved at this point.
*/
typedef int (fz_match_callback_fn)(fz_context *ctx, void *opaque, int num_quads, fz_quad *hit_bbox, int chapter, int page);

/**
	Search for occurrence of 'needle' in text page.

	Call callback once for each hit. This callback will receive
	(potentially) multiple quads for each hit.

	Returns the number of hits - note that this is potentially
	different from (i.e. is not greater than) the number of quads
	as returned by the non callback API.

	NOTE: This is an experimental interface and subject to change
	without notice.
*/
int fz_search_stext_page_cb(fz_context *ctx, fz_stext_page *text, const char *needle, fz_search_callback_fn *cb, void *opaque);

/**
	Return a list of quads to highlight lines inside the selection
	points.
*/
int fz_highlight_selection(fz_context *ctx, fz_stext_page *page, fz_point a, fz_point b, fz_quad *quads, int max_quads);

enum
{
	FZ_SELECT_CHARS,
	FZ_SELECT_WORDS,
	FZ_SELECT_LINES,
};

fz_quad fz_snap_selection(fz_context *ctx, fz_stext_page *page, fz_point *ap, fz_point *bp, int mode);

/**
	Return a newly allocated UTF-8 string with the text for a given
	selection.

	crlf: If true, write "\r\n" style line endings (otherwise "\n"
	only).
*/
char *fz_copy_selection(fz_context *ctx, fz_stext_page *page, fz_point a, fz_point b, int crlf);

/**
	Return a newly allocated UTF-8 string with the text for a given
	selection rectangle.

	crlf: If true, write "\r\n" style line endings (otherwise "\n"
	only).
*/
char *fz_copy_rectangle(fz_context *ctx, fz_stext_page *page, fz_rect area, int crlf);

/**
	Options for creating structured text.
*/
typedef struct
{
	int flags;
	float scale;
	fz_rect clip;
} fz_stext_options;

/**
	Parse stext device options from a comma separated key-value
	string.
*/
fz_stext_options *fz_parse_stext_options(fz_context *ctx, fz_stext_options *opts, const char *string);

/**
	Perform segmentation analysis on an (unstructured) page to look for
	recursive subdivisions.

	Essentially this code attempts to split the page horizontally and/or
	vertically repeatedly into smaller and smaller "segments" (divisions).

	This minimises the reordering of the content, but some reordering
	may be unavoidable.

	Returns 0 if no changes were made to the document.

	This is experimental code, and may change (or be removed) in future
	versions!
*/
int fz_segment_stext_page(fz_context *ctx, fz_stext_page *page);

/**
	Perform segmentation analysis on a rectangle of a given
	stext page.

	Like fz_segment_stext_page, this attempts to split the given page
	region horizontally and/or vertically repeatedly into smaller and
	smaller "segments".

	This works for pages with structure too, but splitting with
	rectangles that cut across structure blocks may not behave as
	expected.

	This minimises the reordering of the content (as viewed from the
	perspective of a depth first traversal), but some reordering may
	be unavoidable.

	This function accepts smaller gaps for segmentation than the full
	page segmentation does.

	Returns 0 if no changes were made to the document.

	This is experimental code, and may change (or be removed) in future
	versions!
*/
int fz_segment_stext_rect(fz_context *ctx, fz_stext_page *page, fz_rect rect);

/**
	Attempt to break paragraphs at plausible places.
*/
void fz_paragraph_break(fz_context *ctx, fz_stext_page *page);

/**
	Hunt for possible tables on a page, and update the stext with
	information.
*/
void fz_table_hunt(fz_context *ctx, fz_stext_page *page);

/**
	Hunt for possible tables within a specific rect on a page, and
	update the stext with information.
*/
void fz_table_hunt_within_bounds(fz_context *ctx, fz_stext_page *page, fz_rect bounds);

/**
	Interpret the bounded contents of a given stext page as
	a table.

	The page contents will be rewritten to contain a Table
	structure with the identified content in it.

	This uses the same logic as for fz_table_hunt, without the
	actual hunting. fz_table_hunt hunts to find possible bounds
	for multiple tables on the page; this routine just finds a
	single table contained within the given rectangle.

	Returns the stext_block list that contains the content of
	the table.
*/
fz_stext_block *
fz_find_table_within_bounds(fz_context *ctx, fz_stext_page *page, fz_rect bounds);

/**
	Create a device to extract the text on a page.

	Gather the text on a page into blocks and lines.

	The reading order is taken from the order the text is drawn in
	the source file, so may not be accurate.

	page: The text page to which content should be added. This will
	usually be a newly created (empty) text page, but it can be one
	containing data already (for example when merging multiple
	pages, or watermarking).

	options: Options to configure the stext device.
*/
fz_device *fz_new_stext_device(fz_context *ctx, fz_stext_page *page, const fz_stext_options *options);

/**
	Create a device to extract the text on a page into an existing
	fz_stext_page structure.

	Gather the text on a page into blocks and lines.

	The reading order is taken from the order the text is drawn in
	the source file, so may not be accurate.

	stext_page: The text page to which content should be added. This will
	usually be a newly created (empty) text page, but it can be one
	containing data already (for example when merging multiple
	pages, or watermarking).

	options: Options to configure the stext device.

	The next 2 parameters are copied into the fz_stext_page structure's
	ids section, so only have to be valid if you expect to interrogate
	that section later.

	chapter_num: The chapter number that this page came from.

	page_num: The page number that this page came from.

	The final parameter is copied into the fz_stext_page structure's
	ids section. The mediabox for the enture fz_stext_page is unioned
	with this, so pass fz_empty_bbox if you don't care about getting
	a valid value back from the ids section, but you don't want to
	upset the value in the page->mediabox field.

	mediabox: The mediabox for this page.
*/
fz_device *
fz_new_stext_device_for_page(fz_context *ctx, fz_stext_page *stext_page, const fz_stext_options *opts, int chapter_num, int page_num, fz_rect mediabox);


/**
	Create a device to OCR the text on the page.

	Renders the page internally to a bitmap that is then OCRd. Text
	is then forwarded onto the target device.

	target: The target device to receive the OCRd text.

	ctm: The transform to apply to the mediabox to get the size for
	the rendered page image. Also used to calculate the resolution
	for the page image. In general, this will be the same as the CTM
	that you pass to fz_run_page (or fz_run_display_list) to feed
	this device.

	mediabox: The mediabox (in points). Combined with the CTM to get
	the bounds of the pixmap used internally for the rendered page
	image.

	with_list: If with_list is false, then all non-text operations
	are forwarded instantly to the target device. This results in
	the target device seeing all NON-text operations, followed by
	all the text operations (derived from OCR).

	If with_list is true, then all the marking operations are
	collated into a display list which is then replayed to the
	target device at the end.

	language: NULL (for "eng"), or a pointer to a string to describe
	the languages/scripts that should be used for OCR (e.g.
	"eng,ara").

	datadir: NULL (for ""), or a pointer to a path string otherwise
	provided to Tesseract in the TESSDATA_PREFIX environment variable.

	progress: NULL, or function to be called periodically to indicate
	progress. Return 0 to continue, or 1 to cancel. progress_arg is
	returned as the void *. The int is a value between 0 and 100 to
	indicate progress.

	progress_arg: A void * value to be parrotted back to the progress
	function.
*/
fz_device *fz_new_ocr_device(fz_context *ctx, fz_device *target, fz_matrix ctm, fz_rect mediabox, int with_list, const char *language,
			const char *datadir, int (*progress)(fz_context *, void *, int), void *progress_arg);

fz_device *fz_new_ocr_device_with_options(fz_context *ctx, fz_device *target, fz_matrix ctm, fz_rect mediabox, int with_list, const char *language,
			const char *datadir, int (*progress)(fz_context *, void *, int), void *progress_arg, const char *options);

fz_document *fz_open_reflowed_document(fz_context *ctx, fz_document *underdoc, const fz_stext_options *opts);

/**
	Simple function to return if a given unicode char is equivalent to a space.
*/
int fz_is_unicode_space_equivalent(int c);

/**
	Simple function to return if a given unicode char is whitespace.
*/
int fz_is_unicode_whitespace(int c);

/**
	Simple function to return if a given unicode char is a hyphen.
*/
int fz_is_unicode_hyphen(int c);

typedef struct fz_search fz_search;

typedef enum
{
	FZ_SEARCH_EXACT = 0,
	FZ_SEARCH_IGNORE_CASE = 1,
	FZ_SEARCH_IGNORE_DIACRITICS = 2,
	FZ_SEARCH_REGEXP = 4,
	FZ_SEARCH_KEEP_LINES = 8,
	FZ_SEARCH_KEEP_PARAGRAPHS = 16,
	FZ_SEARCH_KEEP_HYPHENS = 32
} fz_search_options;

FZ_DATA extern const char *fz_search_options_usage;

fz_search_options fz_parse_search_options(const char *options);

/**
	Create a new search.
*/
fz_search *fz_new_search(fz_context *ctx);

/**
	Change the options/needle to be used for a search.

	If the needle is invalid (in the case of regexps, it fails to compile)
	it will throw an error.

	If the needle changes, the current position of the search within the
	text is kept.

	If the options change, the search position may revert to the beginning
	of the current page.
*/
void fz_search_set_options(fz_context *ctx, fz_search *search, fz_search_options options, const char *needle);

typedef enum
{
	/* Ran out of stext to search. Please feed me some more. */
	FZ_SEARCH_MORE_INPUT = 0,

	/* We have a match. match structure has been populated. */
	FZ_SEARCH_MATCH = 1,

	/* Search complete */
	FZ_SEARCH_COMPLETE
} fz_search_reason;

typedef struct
{
	fz_quad quad;
	int seq;
	int chapter_num;
	int page_num;
} fz_match_quad;

typedef struct
{
	fz_stext_page *page;
	fz_stext_struct *parent;
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;
} fz_stext_position;

typedef struct
{
	int num_quads;
	fz_match_quad *quads;
	fz_stext_position begin;
	fz_stext_position end;
}
fz_search_result_details;

/**
	Structure used to represent the 'result' of a search.
*/
typedef struct
{
	fz_search_reason reason;
	union
	{
		struct
		{
			int seq_needed;
		} more_input;
		struct
		{
			fz_search_result_details *result;
		} match;
	} u;
} fz_search_result;

/**
	Continue searching for the next match.

	Will return with a search result.

	If it asks for more stext, feed it with the requested page (or
	NULL to tell it it's the end of the document) before calling
	this again.

	Several pages may be requested before searching begins.
*/
fz_search_result fz_search_forwards(fz_context *ctx, fz_search *search);

/**
	Continue searching backwards for the next match.

	Will return asking for more stext, having matched, or having
	completed the search.

	If it asks for more stext, then any further calls to this
	function will give the same result, until stext is supplied,
	or a NULL stext is fed in to indicate the end of the document.

	Several pages may be requested before searching begins.
*/
fz_search_result fz_search_backwards(fz_context *ctx, fz_search *search);

/**
	Supply more stext to be searched; ownership of the stext page is
	passed in.

	This can be called immediately after an fz_search has been created
	to give it the first page to search, or it will be requested as soon
	as the first search operation is done on that page.

	If we are calling this in response to fz_search_forwards telling
	us that we need another page, page will be the stext for the next
	page.

	If we are calling this in response to fz_search_backwards telling
	is that we need another page, page will be the stext for the previous
	page.

	seq is a simple integer value that will be parrotted back to us in the
	match (typically the page number within the document).

	The search function will retain the page for a while. When it has
	finished with it, it will call fz_drop_stext_page() to release it.

	Pass page = NULL to indicate that there are no more pages (in this
	direction) to be fed.
*/
void fz_feed_search(fz_context *ctx, fz_search *search, fz_stext_page *page, int seq);

/**
	Free the search structures.
*/
void fz_drop_search(fz_context *ctx, fz_search *search);

/**
	Search for occurrence of 'needle' in text page, matching in a given
	style.

	Return the number of quads and store hit quads in the passed in
	array.

	NOTE: This is an experimental interface and subject to change
	without notice.
*/
int fz_match_stext_page(fz_context *ctx, fz_stext_page *text, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max, fz_search_options options);

/**
	Search for occurrence of 'needle' in text page.

	Call callback once for each hit. This callback will receive
	(potentially) multiple quads for each hit.

	Returns the number of hits - note that this is potentially
	different from (i.e. is not greater than) the number of quads
	as returned by the non callback API.

	NOTE: This is an experimental interface and subject to change
	without notice.
*/
int fz_match_stext_page_cb(fz_context *ctx, fz_stext_page *page, const char *needle, fz_match_callback_fn *cb, void *opaque, fz_search_options options);

/*
	Allocator function to make a new STRUCT stext block to be used in
	a given page (and it's 'down' structure, initially empty). Not
	linked in to the overall page structure yet.
*/
fz_stext_block *fz_new_stext_struct(fz_context *ctx, fz_stext_page *page, fz_structure standard, const char *raw, int index);

/* Iterators for walking over stext pages */

/*
	Iterator definition. The parts of this are subject to change.
*/
typedef struct
{
	fz_stext_page *page;
	fz_stext_struct *parent;
	fz_stext_block *block;
} fz_stext_page_block_iterator;

/*
	Create a new iterator, initialised to point at the first block on the page.
*/
fz_stext_page_block_iterator fz_stext_page_block_iterator_begin(fz_stext_page *page);

/*
	Move to the next block (never moving upwards).

	If there is no next block, iterator.block is returned as NULL.
*/
fz_stext_page_block_iterator fz_stext_page_block_iterator_next(fz_stext_page_block_iterator pos);

/*
	On a structure block, this moves the iterator down to the first child of
	that block.

	On any other block, this does nothing.
*/
fz_stext_page_block_iterator fz_stext_page_block_iterator_down(fz_stext_page_block_iterator pos);

/*
	Move up to the parent of the current block.

	If there is no parent, iterator.block is return as NULL.
*/
fz_stext_page_block_iterator fz_stext_page_block_iterator_up(fz_stext_page_block_iterator pos);

/*
	Move to the next block (in a depth first traversal style).

	The iterator never stops on struct blocks, and instead steps into them.
	At the end of a set of child blocks, it will move back to the parent and
	continue from there.
*/
fz_stext_page_block_iterator fz_stext_page_block_iterator_next_dfs(fz_stext_page_block_iterator pos);

/*
	Return true if the iterator is at the end of a list of blocks.
	(No attempt is made to account for whether there is more data after a
	parent block).
*/
int fz_stext_page_block_iterator_eod(fz_stext_page_block_iterator pos);

/*
	Return true if the iterator is at the end of a depth first traversal
	of the stext page.
*/
int fz_stext_page_block_iterator_eod_dfs(fz_stext_page_block_iterator pos);

/*
	Update a given stext page so that the contents within it that fall
	within the given rectangle are contained within a structure tag of the
	given classification.

	The code tries not to change the ordering of content as seen from
	a depth first traversal as it does this.

	This is an experimental interface. It may be updated or removed in
	future with no warning!
*/
void
fz_classify_stext_rect(fz_context *ctx, fz_stext_page *page, fz_structure classification, fz_rect rect);

/*
	Remove any prefix of large white rectangular vectors that (almost)
	fills the page from the stext.

	This is an experimental interface. It may be updated or removed in
	future with no warning!
*/
int
fz_stext_remove_page_fill(fz_context *ctx, fz_stext_page *page);

#endif
