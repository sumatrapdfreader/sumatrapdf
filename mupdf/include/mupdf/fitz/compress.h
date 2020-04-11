#ifndef MUPDF_FITZ_COMPRESS_H
#define MUPDF_FITZ_COMPRESS_H

#include "mupdf/fitz/system.h"

typedef enum
{
	FZ_DEFLATE_NONE = 0,
	FZ_DEFLATE_BEST_SPEED = 1,
	FZ_DEFLATE_BEST = 9,
	FZ_DEFLATE_DEFAULT = -1
} fz_deflate_level;

/*
	Returns the upper bound on the
	size of flated data of length size.
 */
size_t fz_deflate_bound(fz_context *ctx, size_t size);

/*
	Compress source_length bytes of data starting
	at source, into a buffer of length *destLen, starting at dest.
	*compressed_length will be updated on exit to contain the size
	actually used.
 */
void fz_deflate(fz_context *ctx, unsigned char *dest, size_t *compressed_length, const unsigned char *source, size_t source_length, fz_deflate_level level);

/*
	Compress source_length bytes of data starting
	at source, into a new memory block malloced for that purpose.
	*compressed_length is updated on exit to contain the size used.
	Ownership of the block is returned from this function, and the
	caller is therefore responsible for freeing it. The block may be
	considerably larger than is actually required. The caller is
	free to fz_realloc it down if it wants to.
*/
unsigned char *fz_new_deflated_data(fz_context *ctx, size_t *compressed_length, const unsigned char *source, size_t source_length, fz_deflate_level level);

/*
	Compress the contents of a fz_buffer into a
	new block malloced for that purpose. *compressed_length is
	updated on exit to contain the size used. Ownership of the block
	is returned from this function, and the caller is therefore
	responsible for freeing it. The block may be considerably larger
	than is actually required. The caller is free to fz_realloc it
	down if it wants to.
*/
unsigned char *fz_new_deflated_data_from_buffer(fz_context *ctx, size_t *compressed_length, fz_buffer *buffer, fz_deflate_level level);

/*
	Compress bitmap data as CCITT Group 3 1D fax image.
	Creates a stream assuming the default PDF parameters,
	except the number of columns.
*/
fz_buffer *fz_compress_ccitt_fax_g3(fz_context *ctx, const unsigned char *data, int columns, int rows);

/*
	Compress bitmap data as CCITT Group 4 2D fax image.
	Creates a stream assuming the default PDF parameters, except
	K=-1 and the number of columns.
*/
fz_buffer *fz_compress_ccitt_fax_g4(fz_context *ctx, const unsigned char *data, int columns, int rows);

#endif
