#ifndef MUPDF_FITZ_META_H
#define MUPDF_FITZ_META_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/document.h"

/*
	fz_meta: Perform a meta operation on a document.

	(In development - Subject to change in future versions)

	Meta operations provide a way to perform format specific
	operations on a document. The meta operation scheme is
	designed to be extensible so that new features can be
	transparently added in later versions of the library.

	doc: The document on which to perform the meta operation.

	key: The meta operation to try. If a particular operation
	is unsupported on a given document, the function will return
	FZ_META_UNKNOWN_KEY.

	ptr: An operation dependent (possibly NULL) pointer.

	size: An operation dependent integer. Often this will
	be the size of the block pointed to by ptr, but not always.

	Returns an operation dependent value; FZ_META_UNKNOWN_KEY
	always means "unknown operation for this document". In general
	FZ_META_OK should be used to indicate successful operation.
*/
int fz_meta(fz_document *doc, int key, void *ptr, int size);

enum
{
	FZ_META_UNKNOWN_KEY = -1,
	FZ_META_OK = 0,

	/*
		ptr: Pointer to block (uninitialised on entry)
		size: Size of block (at least 64 bytes)
		Returns: Document format as a brief text string.
		All formats should support this.
	*/
	FZ_META_FORMAT_INFO = 1,

	/*
		ptr: Pointer to block (uninitialised on entry)
		size: Size of block (at least 64 bytes)
		Returns: Encryption info as a brief text string.
	*/
	FZ_META_CRYPT_INFO = 2,

	/*
		ptr: NULL
		size: Which permission to check
		Returns: 1 if permitted, 0 otherwise.
	*/
	FZ_META_HAS_PERMISSION = 3,

	FZ_PERMISSION_PRINT = 0,
	FZ_PERMISSION_CHANGE = 1,
	FZ_PERMISSION_COPY = 2,
	FZ_PERMISSION_NOTES = 3,

	/*
		ptr: Pointer to block. First entry in the block is
		a pointer to a UTF8 string to lookup. The rest of the
		block is uninitialised on entry.
		size: size of the block in bytes.
		Returns: 0 if not found. 1 if found. The string
		result is copied into the block (truncated to size
		and NULL terminated)

	*/
	FZ_META_INFO = 4,
};

#endif
