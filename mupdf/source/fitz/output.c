#include "mupdf/fitz.h"

static int
file_printf(fz_output *out, const char *fmt, va_list ap)
{
	FILE *file = (FILE *)out->opaque;

	return vfprintf(file, fmt, ap);
}

static int
file_write(fz_output *out, const void *buffer, int count)
{
	FILE *file = (FILE *)out->opaque;

	return fwrite(buffer, 1, count, file);
}

fz_output *
fz_new_output_with_file(fz_context *ctx, FILE *file)
{
	fz_output *out = fz_malloc_struct(ctx, fz_output);
	out->ctx = ctx;
	out->opaque = file;
	out->printf = file_printf;
	out->write = file_write;
	out->close = NULL;
	return out;
}

void
fz_close_output(fz_output *out)
{
	if (!out)
		return;
	if (out->close)
		out->close(out);
	fz_free(out->ctx, out);
}

int
fz_printf(fz_output *out, const char *fmt, ...)
{
	int ret;
	va_list ap;

	if (!out)
		return 0;

	va_start(ap, fmt);
	ret = out->printf(out, fmt, ap);
	va_end(ap);

	return ret;
}

int
fz_write(fz_output *out, const void *data, int len)
{
	if (!out)
		return 0;
	return out->write(out, data, len);
}

int
fz_puts(fz_output *out, const char *str)
{
	if (!out)
		return 0;
	return out->write(out, str, strlen(str));
}

static int
buffer_printf(fz_output *out, const char *fmt, va_list list)
{
	fz_buffer *buffer = (fz_buffer *)out->opaque;

	return fz_buffer_vprintf(out->ctx, buffer, fmt, list);
}

static int
buffer_write(fz_output *out, const void *data, int len)
{
	fz_buffer *buffer = (fz_buffer *)out->opaque;

	fz_write_buffer(out->ctx, buffer, (unsigned char *)data, len);
	return len;
}

fz_output *
fz_new_output_with_buffer(fz_context *ctx, fz_buffer *buf)
{
	fz_output *out = fz_malloc_struct(ctx, fz_output);
	out->ctx = ctx;
	out->opaque = buf;
	out->printf = buffer_printf;
	out->write = buffer_write;
	out->close = NULL;
	return out;
}
