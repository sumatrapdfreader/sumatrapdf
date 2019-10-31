#ifndef MUPDF_FITZ_BUFFER_H
#define MUPDF_FITZ_BUFFER_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"

/*
	fz_buffer is a wrapper around a dynamically allocated array of bytes.

	Buffers have a capacity (the number of bytes storage immediately
	available) and a current size.
*/
typedef struct fz_buffer_s fz_buffer;

fz_buffer *fz_keep_buffer(fz_context *ctx, fz_buffer *buf);
void fz_drop_buffer(fz_context *ctx, fz_buffer *buf);

size_t fz_buffer_storage(fz_context *ctx, fz_buffer *buf, unsigned char **datap);

const char *fz_string_from_buffer(fz_context *ctx, fz_buffer *buf);

fz_buffer *fz_new_buffer(fz_context *ctx, size_t capacity);

fz_buffer *fz_new_buffer_from_data(fz_context *ctx, unsigned char *data, size_t size);

fz_buffer *fz_new_buffer_from_shared_data(fz_context *ctx, const unsigned char *data, size_t size);

fz_buffer *fz_new_buffer_from_copied_data(fz_context *ctx, const unsigned char *data, size_t size);

fz_buffer *fz_new_buffer_from_base64(fz_context *ctx, const char *data, size_t size);

void fz_resize_buffer(fz_context *ctx, fz_buffer *buf, size_t capacity);

void fz_grow_buffer(fz_context *ctx, fz_buffer *buf);

void fz_trim_buffer(fz_context *ctx, fz_buffer *buf);

void fz_clear_buffer(fz_context *ctx, fz_buffer *buf);

void fz_append_buffer(fz_context *ctx, fz_buffer *destination, fz_buffer *source);

void fz_append_data(fz_context *ctx, fz_buffer *buf, const void *data, size_t len);
void fz_append_string(fz_context *ctx, fz_buffer *buf, const char *data);
void fz_append_byte(fz_context *ctx, fz_buffer *buf, int c);
void fz_append_rune(fz_context *ctx, fz_buffer *buf, int c);
void fz_append_int32_le(fz_context *ctx, fz_buffer *buf, int x);
void fz_append_int16_le(fz_context *ctx, fz_buffer *buf, int x);
void fz_append_int32_be(fz_context *ctx, fz_buffer *buf, int x);
void fz_append_int16_be(fz_context *ctx, fz_buffer *buf, int x);
void fz_append_bits(fz_context *ctx, fz_buffer *buf, int value, int count);
void fz_append_bits_pad(fz_context *ctx, fz_buffer *buf);
void fz_append_printf(fz_context *ctx, fz_buffer *buffer, const char *fmt, ...);
void fz_append_vprintf(fz_context *ctx, fz_buffer *buffer, const char *fmt, va_list args);
void fz_append_pdf_string(fz_context *ctx, fz_buffer *buffer, const char *text);

void fz_terminate_buffer(fz_context *ctx, fz_buffer *buf);

void fz_md5_buffer(fz_context *ctx, fz_buffer *buffer, unsigned char digest[16]);

size_t fz_buffer_extract(fz_context *ctx, fz_buffer *buf, unsigned char **data);

#endif
