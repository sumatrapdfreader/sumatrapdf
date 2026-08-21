// Copyright (C) 2004-2024 Artifex Software, Inc.
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

#ifndef MUPDF_FITZ_ARCHIVE_H
#define MUPDF_FITZ_ARCHIVE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/stream.h"
#include "mupdf/fitz/tree.h"

/* PUBLIC API */

/**
	fz_archive:

	fz_archive provides methods for accessing "archive" files.
	An archive file is a conceptual entity that contains multiple
	files, which can be counted, enumerated, and read.

	Implementations of fz_archive based upon directories, zip
	and tar files are included.
*/

typedef struct fz_archive fz_archive;

/**
	Open a zip or tar archive

	Open a file and identify its archive type based on the archive
	signature contained inside.

	filename: a path to a file as it would be given to open(2).
*/
fz_archive *fz_open_archive(fz_context *ctx, const char *filename);

/**
	Open zip or tar archive stream.

	Open an archive using a seekable stream object rather than
	opening a file or directory on disk.
*/
fz_archive *fz_open_archive_with_stream(fz_context *ctx, fz_stream *file);

/**
	Open zip or tar archive stream.

	Does the same as fz_open_archive_with_stream, but will not throw
	an error in the event of failing to recognise the format. Will
	still throw errors in other cases though!
*/
fz_archive *fz_try_open_archive_with_stream(fz_context *ctx, fz_stream *file);

/**
	Open a directory as if it was an archive.

	A special case where a directory is opened as if it was an
	archive.

	Note that for directories it is not possible to retrieve the
	number of entries or list the entries. It is however possible
	to check if the archive has a particular entry.

	path: a path to a directory as it would be given to opendir(3).
*/
fz_archive *fz_open_directory(fz_context *ctx, const char *path);


/**
	Determine if a given path is a directory.

	In the case of the path not existing, or having no access
	we will return 0.
*/
int fz_is_directory(fz_context *ctx, const char *path);

/**
	Drop a reference to an archive.

	When the last reference is dropped, this closes and releases
	any memory or filehandles associated with the archive.
*/
void fz_drop_archive(fz_context *ctx, fz_archive *arch);

/**
	Keep a reference to an archive.
*/
fz_archive *
fz_keep_archive(fz_context *ctx, fz_archive *arch);

/**
	Return a pointer to a string describing the format of the
	archive.

	The lifetime of the string is unspecified (in current
	implementations the string will persist until the archive
	is closed, but this is not guaranteed).
*/
const char *fz_archive_format(fz_context *ctx, fz_archive *arch);

/**
	Number of entries in archive.

	Will always return a value >= 0.

	May throw an exception if this type of archive cannot count the
	entries (such as a directory).
*/
int fz_count_archive_entries(fz_context *ctx, fz_archive *arch);

/**
	Get listed name of entry position idx.

	idx: Must be a value >= 0 < return value from
	fz_count_archive_entries. If not in range NULL will be
	returned.

	May throw an exception if this type of archive cannot list the
	entries (such as a directory).
*/
const char *fz_list_archive_entry(fz_context *ctx, fz_archive *arch, int idx);

/**
	Check if entry by given name exists.

	If named entry does not exist 0 will be returned, if it does
	exist 1 is returned.

	name: Entry name to look for, this must be an exact match to
	the entry name in the archive.
*/
int fz_has_archive_entry(fz_context *ctx, fz_archive *arch, const char *name);

/**
	Opens an archive entry as a stream.

	name: Entry name to look for, this must be an exact match to
	the entry name in the archive.

	Throws an exception if a matching entry cannot be found.
*/
fz_stream *fz_open_archive_entry(fz_context *ctx, fz_archive *arch, const char *name);

/**
	Opens an archive entry as a stream.

	Returns NULL if a matching entry cannot be found, otherwise
	behaves exactly as fz_open_archive_entry.
*/
fz_stream *fz_try_open_archive_entry(fz_context *ctx, fz_archive *arch, const char *name);

/**
	Reads all bytes in an archive entry
	into a buffer.

	name: Entry name to look for, this must be an exact match to
	the entry name in the archive.

	Throws an exception if a matching entry cannot be found.
*/
fz_buffer *fz_read_archive_entry(fz_context *ctx, fz_archive *arch, const char *name);

/**
	Reads all bytes in an archive entry
	into a buffer.

	name: Entry name to look for, this must be an exact match to
	the entry name in the archive.

	Returns NULL if a matching entry cannot be found. Otherwise behaves
	the same as fz_read_archive_entry. Exceptions may be thrown.
*/
fz_buffer *fz_try_read_archive_entry(fz_context *ctx, fz_archive *arch, const char *name);

/**
	fz_archive: tar implementation
*/

/**
	Detect if stream object is a tar archive.

	Assumes that the stream object is seekable.
*/
int fz_is_tar_archive(fz_context *ctx, fz_stream *file);

/**
	Detect if stream object is an archive supported by libarchive.

	Assumes that the stream object is seekable.
*/
int fz_is_libarchive_archive(fz_context *ctx, fz_stream *file);

/**
	Detect if stream object is a cfb archive.

	Assumes that the stream object is seekable.
*/
int fz_is_cfb_archive(fz_context *ctx, fz_stream *file);

/**
	Open a tar archive file.

	An exception is thrown if the file is not a tar archive as
	indicated by the presence of a tar signature.

	filename: a path to a tar archive file as it would be given to
	open(2).
*/
fz_archive *fz_open_tar_archive(fz_context *ctx, const char *filename);

/**
	Open a tar archive stream.

	Open an archive using a seekable stream object rather than
	opening a file or directory on disk.

	An exception is thrown if the stream is not a tar archive as
	indicated by the presence of a tar signature.

*/
fz_archive *fz_open_tar_archive_with_stream(fz_context *ctx, fz_stream *file);

/**
	Open an archive using libarchive.

	An exception is thrown if the file is not supported by libarchive.

	filename: a path to an archive file as it would be given to
	open(2).
*/
fz_archive *fz_open_libarchive_archive(fz_context *ctx, const char *filename);

/**
	Open an archive using libarchive.

	Open an archive using a seekable stream object rather than
	opening a file or directory on disk.

	An exception is thrown if the stream is not supported by libarchive.
*/
fz_archive *fz_open_libarchive_archive_with_stream(fz_context *ctx, fz_stream *file);

/**
	Open a cfb file as an archive.

	An exception is thrown if the file is not recognised as a cfb.

	filename: a path to an archive file as it would be given to
	open(2).
*/
fz_archive *fz_open_cfb_archive(fz_context *ctx, const char *filename);

/**
	Open a cfb file as an archive.

	Open an archive using a seekable stream object rather than
	opening a file or directory on disk.

	An exception is thrown if the file is not recognised as a chm.
*/
fz_archive *fz_open_cfb_archive_with_stream(fz_context *ctx, fz_stream *file);

/**
	fz_archive: zip implementation
*/

/**
	Detect if stream object is a zip archive.

	Assumes that the stream object is seekable.
*/
int fz_is_zip_archive(fz_context *ctx, fz_stream *file);

/**
	Open a zip archive file.

	An exception is thrown if the file is not a zip archive as
	indicated by the presence of a zip signature.

	filename: a path to a zip archive file as it would be given to
	open(2).
*/
fz_archive *fz_open_zip_archive(fz_context *ctx, const char *path);


/**
	Open a zip archive stream.

	Open an archive using a seekable stream object rather than
	opening a file or directory on disk.

	An exception is thrown if the stream is not a zip archive as
	indicated by the presence of a zip signature.

*/
fz_archive *fz_open_zip_archive_with_stream(fz_context *ctx, fz_stream *file);

/**
	Open a zip archive from static data.
*/
fz_archive *fz_open_zip_archive_with_memory(fz_context *ctx, const unsigned char *data, size_t size);

/**
	fz_zip_writer offers methods for creating and writing zip files.
	It can be seen as the reverse of the fz_archive zip
	implementation.
*/

typedef struct fz_zip_writer fz_zip_writer;

/**
	Create a new zip writer that writes to a given file.

	Open an archive using a seekable stream object rather than
	opening a file or directory on disk.
*/
fz_zip_writer *fz_new_zip_writer(fz_context *ctx, const char *filename);

/**
	Create a new zip writer that writes to a given output stream.

	Ownership of out passes in immediately upon calling this function.
	The caller should never drop the fz_output, even if this function throws
	an exception.
*/
fz_zip_writer *fz_new_zip_writer_with_output(fz_context *ctx, fz_output *out);


/**
	Given a buffer of data, (optionally) compress it, and add it to
	the zip file with the given name.
*/
void fz_write_zip_entry(fz_context *ctx, fz_zip_writer *zip, const char *name, fz_buffer *buf, int compress);

/**
	Close the zip file for writing.

	This flushes any pending data to the file. This can throw
	exceptions.
*/
void fz_close_zip_writer(fz_context *ctx, fz_zip_writer *zip);

/**
	Drop the reference to the zipfile.

	In common with other 'drop' methods, this will never throw an
	exception.
*/
void fz_drop_zip_writer(fz_context *ctx, fz_zip_writer *zip);

/**
	Create an archive that holds named buffers.

	tree can either be a preformed tree with fz_buffers as values,
	or it can be NULL for an empty tree.
*/
fz_archive *fz_new_tree_archive(fz_context *ctx, fz_tree *tree);

/**
	Add a named buffer to an existing tree archive.

	The tree will take a new reference to the buffer. Ownership
	is not transferred.
*/
void fz_tree_archive_add_buffer(fz_context *ctx, fz_archive *arch_, const char *name, fz_buffer *buf);

/**
	Add a named block of data to an existing tree archive.

	The data will be copied into a buffer, and so the caller
	may free it as soon as this returns.
*/
void fz_tree_archive_add_data(fz_context *ctx, fz_archive *arch_, const char *name, const void *data, size_t size);

/**
	Create a new multi archive (initially empty).
*/
fz_archive *fz_new_multi_archive(fz_context *ctx);

/**
	Add an archive to the set of archives handled by a multi
	archive.

	If path is NULL, then the archive contents will appear at the
	top level, otherwise, the archives contents will appear prefixed
	by path.
*/
void fz_mount_multi_archive(fz_context *ctx, fz_archive *arch_, fz_archive *sub, const char *path);

typedef int (fz_recognize_archive_fn)(fz_context *, fz_stream *);
typedef fz_archive *(fz_open_archive_fn)(fz_context *, fz_stream *);

typedef struct
{
	fz_recognize_archive_fn *recognize;
	fz_open_archive_fn *open;
}
fz_archive_handler;

FZ_DATA extern const fz_archive_handler fz_libarchive_archive_handler;

void fz_register_archive_handler(fz_context *ctx, const fz_archive_handler *handler);

/**
	Implementation details: Subject to change.
*/

struct fz_archive
{
	int refs;
	fz_stream *file;

	const char *format;

	void (*drop_archive)(fz_context *ctx, fz_archive *arch);
	int (*count_entries)(fz_context *ctx, fz_archive *arch);
	const char *(*list_entry)(fz_context *ctx, fz_archive *arch, int idx);
	int (*has_entry)(fz_context *ctx, fz_archive *arch, const char *name);
	fz_buffer *(*read_entry)(fz_context *ctx, fz_archive *arch, const char *name);
	fz_stream *(*open_entry)(fz_context *ctx, fz_archive *arch, const char *name);
};

fz_archive *fz_new_archive_of_size(fz_context *ctx, fz_stream *file, int size);

#define fz_new_derived_archive(C,F,M) \
	((M*)Memento_label(fz_new_archive_of_size(C, F, sizeof(M)), #M))



#endif
