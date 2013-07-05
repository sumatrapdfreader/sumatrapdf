#ifndef MUPDF_FITZ_WRITE_DOCUMENT_H
#define MUPDF_FITZ_WRITE_DOCUMENT_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/document.h"

/*
	In calls to fz_write, the following options structure can be used
	to control aspects of the writing process. This structure may grow
	in future, and should be zero-filled to allow forwards compatiblity.
*/
struct fz_write_options_s
{
	int do_incremental; /* Write just the changed objects */
	int do_ascii; /* If non-zero then attempt (where possible) to make
				the output ascii. */
	int do_expand; /* Bitflags; each non zero bit indicates an aspect
				of the file that should be 'expanded' on
				writing. */
	int do_garbage; /* If non-zero then attempt (where possible) to
				garbage collect the file before writing. */
	int do_linear; /* If non-zero then write linearised. */
	int continue_on_error; /* If non-zero, errors are (optionally)
					counted and writing continues. */
	int *errors; /* Pointer to a place to store a count of errors */
};

/*	An enumeration of bitflags to use in the above 'do_expand' field of
	fz_write_options.
*/
enum
{
	fz_expand_images = 1,
	fz_expand_fonts = 2,
	fz_expand_all = -1
};

/*
	fz_write: Write a document out.

	(In development - Subject to change in future versions)

	Save a copy of the current document in its original format.
	Internally the document may change.

	doc: The document to save.

	filename: The filename to save to.

	opts: NULL, or a pointer to an options structure.

	May throw exceptions.
*/
void fz_write_document(fz_document *doc, char *filename, fz_write_options *opts);

#endif
