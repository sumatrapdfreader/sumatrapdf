#ifndef MUPDF_FITZ_UNZIP_H
#define MUPDF_FITZ_UNZIP_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/stream.h"

typedef struct fz_archive_s fz_archive;

fz_archive *fz_open_directory(fz_context *ctx, const char *dirname);
fz_archive *fz_open_archive(fz_context *ctx, const char *filename);
fz_archive *fz_open_archive_with_stream(fz_context *ctx, fz_stream *file);
int fz_has_archive_entry(fz_context *ctx, fz_archive *zip, const char *name);
fz_stream *fz_open_archive_entry(fz_context *ctx, fz_archive *zip, const char *entry);
fz_buffer *fz_read_archive_entry(fz_context *ctx, fz_archive *zip, const char *entry);
void fz_close_archive(fz_context *ctx, fz_archive *ar);

void fz_rebind_archive(fz_archive *zip, fz_context *ctx);

int fz_count_archive_entries(fz_context *ctx, fz_archive *zip);
const char *fz_list_archive_entry(fz_context *ctx, fz_archive *zip, int idx);

#endif
