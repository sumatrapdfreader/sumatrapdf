#include "mem.h"
#include "outf.h"
#include "sys.h"

#include <errno.h>
#include <stdarg.h>

#include <sys/stat.h>


/* Define extract_APPLE_IOS if we are on iOS. */
#undef extract_APPLE_IOS
#ifdef __APPLE__
	#include "TargetConditionals.h"
	#ifdef TARGET_OS_IPHONE
		#define extract_APPLE_IOS
	#elif TARGET_IPHONE_SIMULATOR
		#define extract_APPLE_IOS
	#endif
#endif

int extract_systemf(extract_alloc_t *alloc, const char *format, ...)
{
#ifdef extract_APPLE_IOS
	/* system() not available on iOS. */
	(void) alloc;
	(void) format;
	errno = ENOTSUP;
	return -1;
#else

	int      e;
	char    *command;
	va_list  va;

	va_start(va, format);
	e = extract_vasprintf(alloc, &command, format, va);
	va_end(va);
	if (e < 0) return e;
	outf("running: %s", command);
	e = system(command);
	extract_free(alloc, &command);
	if (e > 0) {
		errno = EIO;
	}
	return e;

#endif
}

int  extract_read_all(extract_alloc_t *alloc, FILE *in, char **o_out)
{
	size_t  len = 0;
	size_t  delta = 128;
	for(;;) {
		size_t n;
		if (extract_realloc2(alloc, o_out, len, len + delta + 1)) {
			extract_free(alloc, o_out);
			return -1;
		}
		n = fread(*o_out + len, 1 /*size*/, delta /*nmemb*/, in);
		len += n;
		if (feof(in)) {
			(*o_out)[len] = 0;
			return 0;
		}
		if (ferror(in)) {
			/* It's weird that fread() and ferror() don't set errno. */
			errno = EIO;
			extract_free(alloc, o_out);
			return -1;
		}
	}
}

int  extract_read_all_path(extract_alloc_t *alloc, const char *path, char **o_text)
{
	int e = -1;
	FILE *f = NULL;
	f = fopen(path, "rb");
	if (!f) goto end;
	if (extract_read_all(alloc, f, o_text)) goto end;
	e = 0;
	end:
	if (f) fclose(f);
	if (e) extract_free(alloc, o_text);
	return e;
}

int  extract_write_all(const void *data, size_t data_size, const char *path)
{
	int e = -1;
	FILE *f = fopen(path, "w");
	if (!f) goto end;
	if (fwrite(data, data_size, 1 /*nmemb*/, f) != 1) goto end;
	e = 0;
	end:
	if (f) fclose(f);
	return e;
}

int extract_check_path_shell_safe(const char *path)
/* Returns -1 with errno=EINVAL if <path> contains sequences that could make it
unsafe in shell commands. */
{
	if (0
			|| strstr(path, "..")
			|| strchr(path, '\'')
			|| strchr(path, '"')
			|| strchr(path, ' ')
			) {
		errno = EINVAL;
		return -1;
	}
	return 0;
}
int extract_remove_directory(extract_alloc_t *alloc, const char *path)
{
	if (extract_check_path_shell_safe(path)) {
		outf("path_out is unsafe: %s", path);
		return -1;
	}
	return extract_systemf(alloc, "rm -r '%s'", path);
}

#ifdef _WIN32
#include <direct.h>
int extract_mkdir(const char *path, int mode)
{
	(void) mode;
	return _mkdir(path);
}
#else
int extract_mkdir(const char *path, int mode)
{
	return mkdir(path, mode);
}
#endif
