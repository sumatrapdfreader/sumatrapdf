#ifndef MUPDF_FITZ_ARCHIVE_H
#define MUPDF_FITZ_ARCHIVE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/stream.h"

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
*/
int fz_is_directory(fz_context *ctx, const char *path);

/**
	Drop the reference to an archive.

	Closes and releases any memory or filehandles associated
	with the archive.
*/
void fz_drop_archive(fz_context *ctx, fz_archive *arch);

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
*/
fz_stream *fz_open_archive_entry(fz_context *ctx, fz_archive *arch, const char *name);

/**
	Reads all bytes in an archive entry
	into a buffer.

	name: Entry name to look for, this must be an exact match to
	the entry name in the archive.
*/
fz_buffer *fz_read_archive_entry(fz_context *ctx, fz_archive *arch, const char *name);

/**
	fz_archive: tar implementation
*/

/**
	Detect if stream object is a tar achieve.

	Assumes that the stream object is seekable.
*/
int fz_is_tar_archive(fz_context *ctx, fz_stream *file);

/**
	Open a tar archive file.

	An exception is throw if the file is not a tar archive as
	indicated by the presence of a tar signature.

	filename: a path to a tar archive file as it would be given to
	open(2).
*/
fz_archive *fz_open_tar_archive(fz_context *ctx, const char *filename);

/**
	Open a tar archive stream.

	Open an archive using a seekable stream object rather than
	opening a file or directory on disk.

	An exception is throw if the stream is not a tar archive as
	indicated by the presence of a tar signature.

*/
fz_archive *fz_open_tar_archive_with_stream(fz_context *ctx, fz_stream *file);

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

	An exception is throw if the file is not a zip archive as
	indicated by the presence of a zip signature.

	filename: a path to a zip archive file as it would be given to
	open(2).
*/
fz_archive *fz_open_zip_archive(fz_context *ctx, const char *path);

/**
	Open a zip archive stream.

	Open an archive using a seekable stream object rather than
	opening a file or directory on disk.

	An exception is throw if the stream is not a zip archive as
	indicated by the presence of a zip signature.

*/
fz_archive *fz_open_zip_archive_with_stream(fz_context *ctx, fz_stream *file);

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
	Implementation details: Subject to change.
*/

struct fz_archive
{
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
