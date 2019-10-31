#ifndef MUPDF_FITZ_ARCHIVE_H
#define MUPDF_FITZ_ARCHIVE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/stream.h"

typedef struct fz_archive_s fz_archive;

struct fz_archive_s
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

fz_archive *fz_open_archive(fz_context *ctx, const char *filename);

fz_archive *fz_open_archive_with_stream(fz_context *ctx, fz_stream *file);

fz_archive *fz_open_directory(fz_context *ctx, const char *path);

int fz_is_directory(fz_context *ctx, const char *path);

void fz_drop_archive(fz_context *ctx, fz_archive *arch);
const char *fz_archive_format(fz_context *ctx, fz_archive *arch);

int fz_count_archive_entries(fz_context *ctx, fz_archive *arch);

const char *fz_list_archive_entry(fz_context *ctx, fz_archive *arch, int idx);

int fz_has_archive_entry(fz_context *ctx, fz_archive *arch, const char *name);

fz_stream *fz_open_archive_entry(fz_context *ctx, fz_archive *arch, const char *name);

fz_buffer *fz_read_archive_entry(fz_context *ctx, fz_archive *arch, const char *name);

int fz_is_tar_archive(fz_context *ctx, fz_stream *file);

fz_archive *fz_open_tar_archive(fz_context *ctx, const char *filename);

fz_archive *fz_open_tar_archive_with_stream(fz_context *ctx, fz_stream *file);

int fz_is_zip_archive(fz_context *ctx, fz_stream *file);

fz_archive *fz_open_zip_archive(fz_context *ctx, const char *path);

fz_archive *fz_open_zip_archive_with_stream(fz_context *ctx, fz_stream *file);

typedef struct fz_zip_writer_s fz_zip_writer;

fz_zip_writer *fz_new_zip_writer(fz_context *ctx, const char *filename);
fz_zip_writer *fz_new_zip_writer_with_output(fz_context *ctx, fz_output *out);
void fz_write_zip_entry(fz_context *ctx, fz_zip_writer *zip, const char *name, fz_buffer *buf, int compress);
void fz_close_zip_writer(fz_context *ctx, fz_zip_writer *zip);
void fz_drop_zip_writer(fz_context *ctx, fz_zip_writer *zip);

#endif
