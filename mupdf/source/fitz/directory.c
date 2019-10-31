#include "mupdf/fitz.h"

#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#define stat _stat
#endif

typedef struct fz_directory_s fz_directory;
struct fz_directory_s
{
	fz_archive super;

	char *path;
};

static void drop_directory(fz_context *ctx, fz_archive *arch)
{
	fz_directory *dir = (fz_directory *) arch;

	fz_free(ctx, dir->path);
}

static fz_stream *open_dir_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_directory *dir = (fz_directory *) arch;
	char path[2048];
	fz_strlcpy(path, dir->path, sizeof path);
	fz_strlcat(path, "/", sizeof path);
	fz_strlcat(path, name, sizeof path);
	return fz_open_file(ctx, path);
}

static fz_buffer *read_dir_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_directory *dir = (fz_directory *) arch;
	char path[2048];
	fz_strlcpy(path, dir->path, sizeof path);
	fz_strlcat(path, "/", sizeof path);
	fz_strlcat(path, name, sizeof path);
	return fz_read_file(ctx, path);
}

static int has_dir_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_directory *dir = (fz_directory *) arch;
	char path[2048];
	fz_strlcpy(path, dir->path, sizeof path);
	fz_strlcat(path, "/", sizeof path);
	fz_strlcat(path, name, sizeof path);
	return fz_file_exists(ctx, path);
}

int
fz_is_directory(fz_context *ctx, const char *path)
{
	struct stat info;

	if (stat(path, &info) < 0)
		return 0;

	return S_ISDIR(info.st_mode);
}

/*
	Open a directory as if it was an archive.

	A special case where a directory is opened as if it was an
	archive.

	Note that for directories it is not possible to retrieve the
	number of entries or list the entries. It is however possible
	to check if the archive has a particular entry.

	path: a path to a directory as it would be given to opendir(3).
*/
fz_archive *
fz_open_directory(fz_context *ctx, const char *path)
{
	fz_directory *dir;

	if (!fz_is_directory(ctx, path))
		fz_throw(ctx, FZ_ERROR_GENERIC, "'%s' is not a directory", path);

	dir = fz_new_derived_archive(ctx, NULL, fz_directory);
	dir->super.format = "dir";
	dir->super.has_entry = has_dir_entry;
	dir->super.read_entry = read_dir_entry;
	dir->super.open_entry = open_dir_entry;
	dir->super.drop_archive = drop_directory;

	fz_try(ctx)
	{
		dir->path = fz_strdup(ctx, path);
	}
	fz_catch(ctx)
	{
		fz_drop_archive(ctx, &dir->super);
		fz_rethrow(ctx);
	}

	return &dir->super;
}
