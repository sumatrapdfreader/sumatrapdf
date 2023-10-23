// Copyright (C) 2004-2023 Artifex Software, Inc.
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

/* Buffer interface */

JNIEXPORT void JNICALL
FUN(Buffer_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer_safe(env, self);
	if (!ctx || !buf) return;
	(*env)->SetLongField(env, self, fid_Buffer_pointer, 0);
	fz_drop_buffer(ctx, buf);
}

JNIEXPORT jlong JNICALL
FUN(Buffer_newNativeBuffer)(JNIEnv *env, jobject self, jint n)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = NULL;

	if (!ctx) return 0;
	if (n < 0) jni_throw_arg(env, "n cannot be negative");

	fz_try(ctx)
		buf = fz_new_buffer(ctx, n);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(buf);
}

JNIEXPORT jint JNICALL
FUN(Buffer_getLength)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);

	if (!ctx || !buf) return -1;

	return (jint)fz_buffer_storage(ctx, buf, NULL);
}

JNIEXPORT jint JNICALL
FUN(Buffer_readByte)(JNIEnv *env, jobject self, jint jat)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	size_t at = (size_t) jat;
	size_t len;
	unsigned char *data;

	if (!ctx || !buf) return -1;
	if (jat < 0) jni_throw_oob(env, "at is negative");

	len = fz_buffer_storage(ctx, buf, &data);
	if (at >= len)
		return -1;

	return data[at];
}

JNIEXPORT jint JNICALL
FUN(Buffer_readBytes)(JNIEnv *env, jobject self, jint jat, jobject jbs)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	size_t at = (size_t) jat;
	jbyte *bs = NULL;
	size_t len;
	size_t remaining_input = 0;
	size_t remaining_output = 0;
	unsigned char *data;

	if (!ctx || !buf) return -1;
	if (jat < 0) jni_throw_oob(env, "at is negative");
	if (!jbs) jni_throw_arg(env, "buffer must not be null");

	len = fz_buffer_storage(ctx, buf, &data);
	if (at >= len)
		return -1;

	remaining_input = len - at;
	remaining_output = (*env)->GetArrayLength(env, jbs);
	len = fz_minz(remaining_output, remaining_input);

	bs = (*env)->GetByteArrayElements(env, jbs, NULL);
	if (!bs) jni_throw_io(env, "cannot get bytes to read");

	memcpy(bs, &data[at], len);
	(*env)->ReleaseByteArrayElements(env, jbs, bs, 0);

	return (jint)len;
}

JNIEXPORT jint JNICALL
FUN(Buffer_readBytesInto)(JNIEnv *env, jobject self, jint jat, jobject jbs, jint joff, jint jlen)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	size_t at = (size_t) jat;
	jbyte *bs = NULL;
	size_t off = (size_t) joff;
	size_t len = (size_t) jlen;
	size_t bslen = 0;
	size_t blen;
	unsigned char *data;

	if (!ctx || !buf) return -1;
	if (jat < 0) jni_throw_oob(env, "at is negative");
	if (!jbs) jni_throw_arg(env, "buffer must not be null");
	if (joff < 0) jni_throw_oob(env, "offset is negative");
	if (jlen < 0) jni_throw_oob(env, "length is negative");

	bslen = (*env)->GetArrayLength(env, jbs);
	if (len > bslen - off) jni_throw_oob(env, "offset + length is outside of buffer");

	blen = fz_buffer_storage(ctx, buf, &data);
	if (at >= blen)
		return -1;

	len = fz_minz(len, blen - at);

	bs = (*env)->GetByteArrayElements(env, jbs, NULL);
	if (!bs) jni_throw_io(env, "cannot get bytes to read");

	memcpy(&bs[off], &data[at], len);
	(*env)->ReleaseByteArrayElements(env, jbs, bs, 0);

	return (jint)len;
}

JNIEXPORT void JNICALL
FUN(Buffer_writeByte)(JNIEnv *env, jobject self, jbyte b)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);

	if (!ctx || !buf) return;

	fz_try(ctx)
		fz_append_byte(ctx, buf, b);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Buffer_writeBytes)(JNIEnv *env, jobject self, jobject jbs)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	jsize len = 0;
	jbyte *bs = NULL;

	if (!ctx || !buf) return;
	if (!jbs) jni_throw_arg_void(env, "buffer must not be null");

	len = (*env)->GetArrayLength(env, jbs);
	bs = (*env)->GetByteArrayElements(env, jbs, NULL);
	if (!bs) jni_throw_io_void(env, "cannot get bytes to write");

	fz_try(ctx)
		fz_append_data(ctx, buf, bs, len);
	fz_always(ctx)
		(*env)->ReleaseByteArrayElements(env, jbs, bs, JNI_ABORT);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Buffer_writeBytesFrom)(JNIEnv *env, jobject self, jobject jbs, jint joff, jint jlen)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	jbyte *bs = NULL;
	jsize off = (jsize) joff;
	jsize len = (jsize) jlen;
	jsize bslen = 0;

	if (!ctx || !buf) return;
	if (!jbs) jni_throw_arg_void(env, "buffer must not be null");

	bslen = (*env)->GetArrayLength(env, jbs);
	if (joff < 0) jni_throw_oob_void(env, "offset is negative");
	if (jlen < 0) jni_throw_oob_void(env, "length is negative");
	if (off + len > bslen) jni_throw_oob_void(env, "offset + length is outside of buffer");

	bs = (*env)->GetByteArrayElements(env, jbs, NULL);
	if (!bs) jni_throw_io_void(env, "cannot get bytes to write");

	fz_try(ctx)
		fz_append_data(ctx, buf, &bs[off], len);
	fz_always(ctx)
		(*env)->ReleaseByteArrayElements(env, jbs, bs, JNI_ABORT);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Buffer_writeBuffer)(JNIEnv *env, jobject self, jobject jbuf)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	fz_buffer *cat = from_Buffer(env, jbuf);

	if (!ctx || !buf) return;
	if (!cat) jni_throw_arg_void(env, "buffer must not be null");

	fz_try(ctx)
		fz_append_buffer(ctx, buf, cat);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Buffer_writeRune)(JNIEnv *env, jobject self, jint rune)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);

	if (!ctx || !buf) return;

	fz_try(ctx)
		fz_append_rune(ctx, buf, rune);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Buffer_writeLine)(JNIEnv *env, jobject self, jstring jline)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	const char *line = NULL;

	if (!ctx || !buf) return;
	if (!jline) jni_throw_arg_void(env, "line must not be null");

	line = (*env)->GetStringUTFChars(env, jline, NULL);
	if (!line) return;

	fz_try(ctx)
	{
		fz_append_string(ctx, buf, line);
		fz_append_byte(ctx, buf, '\n');
	}
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jline, line);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Buffer_writeLines)(JNIEnv *env, jobject self, jobject jlines)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	int i = 0;
	jsize len = 0;

	if (!ctx || !buf) return;
	if (!jlines) jni_throw_arg_void(env, "lines must not be null");

	len = (*env)->GetArrayLength(env, jlines);

	for (i = 0; i < len; ++i)
	{
		const char *line;
		jobject jline;

		jline = (*env)->GetObjectArrayElement(env, jlines, i);
		if ((*env)->ExceptionCheck(env)) return;

		if (!jline)
			continue;
		line = (*env)->GetStringUTFChars(env, jline, NULL);
		if (!line) return;

		fz_try(ctx)
		{
			fz_append_string(ctx, buf, line);
			fz_append_byte(ctx, buf, '\n');
		}
		fz_always(ctx)
			(*env)->ReleaseStringUTFChars(env, jline, line);
		fz_catch(ctx)
			jni_rethrow_void(env, ctx);
	}
}

JNIEXPORT void JNICALL
FUN(Buffer_save)(JNIEnv *env, jobject self, jstring jfilename)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	const char *filename = NULL;

	if (!ctx || !buf) return;
	if (jfilename)
	{
		filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
		if (!filename) return;
	}

	fz_try(ctx)
		fz_save_buffer(ctx, buf, filename);
	fz_always(ctx)
		if (filename)
			(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(Buffer_slice)(JNIEnv *env, jobject self, jint start, jint end)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	fz_buffer *copy = NULL;
	jobject jcopy = NULL;

	if (!ctx || !buf) return NULL;

	fz_try(ctx)
		copy = fz_slice_buffer(ctx, buf, start, end);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jcopy = (*env)->NewObject(env, cls_Buffer, mid_Buffer_init, copy);
	if (!jcopy || (*env)->ExceptionCheck(env))
		return NULL;

	return jcopy;
}
