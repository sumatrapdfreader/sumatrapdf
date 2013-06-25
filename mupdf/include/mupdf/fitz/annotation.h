#ifndef MUPDF_FITZ_ANNOTATION_H
#define MUPDF_FITZ_ANNOTATION_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/math.h"
#include "mupdf/fitz/document.h"

typedef enum
{
	FZ_ANNOT_TEXT,
	FZ_ANNOT_LINK,
	FZ_ANNOT_FREETEXT,
	FZ_ANNOT_LINE,
	FZ_ANNOT_SQUARE,
	FZ_ANNOT_CIRCLE,
	FZ_ANNOT_POLYGON,
	FZ_ANNOT_POLYLINE,
	FZ_ANNOT_HIGHLIGHT,
	FZ_ANNOT_UNDERLINE,
	FZ_ANNOT_SQUIGGLY,
	FZ_ANNOT_STRIKEOUT,
	FZ_ANNOT_STAMP,
	FZ_ANNOT_CARET,
	FZ_ANNOT_INK,
	FZ_ANNOT_POPUP,
	FZ_ANNOT_FILEATTACHMENT,
	FZ_ANNOT_SOUND,
	FZ_ANNOT_MOVIE,
	FZ_ANNOT_WIDGET,
	FZ_ANNOT_SCREEN,
	FZ_ANNOT_PRINTERMARK,
	FZ_ANNOT_TRAPNET,
	FZ_ANNOT_WATERMARK,
	FZ_ANNOT_3D
} fz_annot_type;

/*
	fz_get_annot_type: return the type of an annotation
*/
fz_annot_type fz_get_annot_type(fz_annot *annot);

/*
	fz_first_annot: Return a pointer to the first annotation on a page.

	Does not throw exceptions.
*/
fz_annot *fz_first_annot(fz_document *doc, fz_page *page);

/*
	fz_next_annot: Return a pointer to the next annotation on a page.

	Does not throw exceptions.
*/
fz_annot *fz_next_annot(fz_document *doc, fz_annot *annot);

/*
	fz_bound_annot: Return the bounding rectangle of the annotation.

	Does not throw exceptions.
*/
fz_rect *fz_bound_annot(fz_document *doc, fz_annot *annot, fz_rect *rect);

#endif
