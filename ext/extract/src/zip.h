#ifndef ARTIFEX_EXTRACT_ZIP
#define ARTIFEX_EXTRACT_ZIP

/* Only for internal use by extract code.  */

#include "extract/buffer.h"

#include <stddef.h>


/*
	Support for creating zip file content.

	Content is compressed using deflate.

	Unless otherwise stated, all functions return 0 on success or -1 with errno
	set.
*/

/* Abstract handle for zipfile state. */
typedef struct extract_zip_t extract_zip_t;


/*
	Creates an extract_zip_t that writes to specified buffer.

	We reuse <buffer>'s allocator for zlib's allocation needs.

	buffer:
		Destination for zip file content.
	o_zip:
		Out-param.
*/
int extract_zip_open(extract_buffer_t *buffer, extract_zip_t **o_zip);

/*
	Writes specified data into the zip file.

	Returns same as extract_buffer_write(): 0 on success, +1 if short write due to
	EOF or -1 with errno set.

	zip:
		From extract_zip_open().
	data:
		File contents.
	data_length:
		Length in bytes of file contents.
	name:
		Name of file within the zip file.
*/
int extract_zip_write_file(
		extract_zip_t *zip,
		const void    *data,
		size_t         data_length,
		const char    *name);


/*
	Finishes writing the zip file (e.g. appends Central directory file headers
	and End of central directory record).

	Does not call extract_buffer_close().

	zip:
		From extract_zip_open().
*/
int extract_zip_close(extract_zip_t **pzip);

#endif
