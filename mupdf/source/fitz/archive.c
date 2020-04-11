#include "mupdf/fitz.h"

fz_stream *
fz_open_archive_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	if (!arch->open_entry)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open archive entry");
	return arch->open_entry(ctx, arch, name);
}

fz_buffer *
fz_read_archive_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	if (!arch->read_entry)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot read archive entry");
	return arch->read_entry(ctx, arch, name);
}

int
fz_has_archive_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	if (!arch->has_entry)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot check if archive has entry");
	return arch->has_entry(ctx, arch, name);
}

const char *
fz_list_archive_entry(fz_context *ctx, fz_archive *arch, int idx)
{
	if (!arch->list_entry)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot list archive entries");
	return arch->list_entry(ctx, arch, idx);
}

int
fz_count_archive_entries(fz_context *ctx, fz_archive *arch)
{
	if (!arch->count_entries)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot count archive entries");
	return arch->count_entries(ctx, arch);
}

const char *
fz_archive_format(fz_context *ctx, fz_archive *arch)
{
	return arch->format;
}

fz_archive *
fz_new_archive_of_size(fz_context *ctx, fz_stream *file, int size)
{
	fz_archive *arch;
	arch = Memento_label(fz_calloc(ctx, 1, size), "fz_archive");
	arch->file = fz_keep_stream(ctx, file);
	return arch;
}

fz_archive *
fz_open_archive_with_stream(fz_context *ctx, fz_stream *file)
{
	fz_archive *arch = NULL;

	if (fz_is_zip_archive(ctx, file))
		arch = fz_open_zip_archive_with_stream(ctx, file);
	else if (fz_is_tar_archive(ctx, file))
		arch = fz_open_tar_archive_with_stream(ctx, file);
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot recognize archive");

	return arch;
}

fz_archive *
fz_open_archive(fz_context *ctx, const char *filename)
{
	fz_stream *file;
	fz_archive *arch = NULL;

	file = fz_open_file(ctx, filename);

	fz_try(ctx)
		arch = fz_open_archive_with_stream(ctx, file);
	fz_always(ctx)
		fz_drop_stream(ctx, file);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return arch;
}

void
fz_drop_archive(fz_context *ctx, fz_archive *arch)
{
	if (!arch)
		return;

	if (arch->drop_archive)
		arch->drop_archive(ctx, arch);
	fz_drop_stream(ctx, arch->file);
	fz_free(ctx, arch);
}
