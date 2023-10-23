// Copyright (C) 2004-2021 Artifex Software, Inc.
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

/* DocumentWriter interface */

JNIEXPORT void JNICALL
FUN(DocumentWriter_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document_writer *wri = from_DocumentWriter_safe(env, self);
	jobject ref;
	if (!ctx || !wri) return;
	ref = (jobject)(*env)->GetLongField(env, self, fid_DocumentWriter_ocrlistener);
	if (ref)
	{
		(*env)->DeleteGlobalRef(env, ref);
		(*env)->SetLongField(env, self, fid_DocumentWriter_ocrlistener, 0);
	}
	(*env)->SetLongField(env, self, fid_DocumentWriter_pointer, 0);
	fz_drop_document_writer(ctx, wri);
}

JNIEXPORT jlong JNICALL
FUN(DocumentWriter_newNativeDocumentWriter)(JNIEnv *env, jclass cls, jstring jfilename, jstring jformat, jstring joptions)
{
	fz_context *ctx = get_context(env);
	fz_document_writer *wri = NULL;
	const char *filename = NULL;
	const char *format = NULL;
	const char *options = NULL;

	if (!ctx) return 0;
	if (!jfilename) jni_throw_arg(env, "filename must not be null");

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return 0;

	if (jformat)
	{
		format = (*env)->GetStringUTFChars(env, jformat, NULL);
		if (!format)
		{
			(*env)->ReleaseStringUTFChars(env, jfilename, filename);
			return 0;
		}
	}
	if (joptions)
	{
		options = (*env)->GetStringUTFChars(env, joptions, NULL);
		if (!options)
		{
			if (format)
				(*env)->ReleaseStringUTFChars(env, jformat, format);
			(*env)->ReleaseStringUTFChars(env, jfilename, filename);
			return 0;
		}
	}

	fz_try(ctx)
		wri = fz_new_document_writer(ctx, filename, format, options);
	fz_always(ctx)
	{
		if (options)
			(*env)->ReleaseStringUTFChars(env, joptions, options);
		if (format)
			(*env)->ReleaseStringUTFChars(env, jformat, format);
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(wri);
}

JNIEXPORT jlong JNICALL
FUN(DocumentWriter_newNativeDocumentWriterWithSeekableOutputStream)(JNIEnv *env, jclass cls, jobject jstream, jstring jformat, jstring joptions)
{
	fz_context *ctx = get_context(env);
	fz_document_writer *wri = NULL;
	SeekableStreamState *state = NULL;
	jobject stream = NULL;
	const char *format = NULL;
	const char *options = NULL;
	jbyteArray array = NULL;
	fz_output *out;

	if (!ctx) return 0;
	if (!jstream) jni_throw_arg(env, "output stream must not be null");

	stream = (*env)->NewGlobalRef(env, jstream);
	if (!stream)
		return 0;

	array = (*env)->NewByteArray(env, sizeof state->buffer);
	if (array)
		array = (*env)->NewGlobalRef(env, array);
	if (!array)
	{
		(*env)->DeleteGlobalRef(env, stream);
		return 0;
	}

	if (jformat)
	{
		format = (*env)->GetStringUTFChars(env, jformat, NULL);
		if (!format)
			return 0;
	}
	if (joptions)
	{
		options = (*env)->GetStringUTFChars(env, joptions, NULL);
		if (!options)
		{
			if (format)
				(*env)->ReleaseStringUTFChars(env, jformat, format);
			return 0;
		}
	}

	fz_var(state);
	fz_var(out);
	fz_var(stream);
	fz_var(array);

	fz_try(ctx)
	{
		fz_output *out_temp;
		state = Memento_label(fz_malloc(ctx, sizeof(SeekableStreamState)), "SeekableStreamState_newNativeDocumentWriterWithSeekableOutputStream");
		state->stream = stream;
		state->array = array;

		out = fz_new_output(ctx, 8192, state, SeekableOutputStream_write, NULL, SeekableOutputStream_drop);
		out->seek = SeekableOutputStream_seek;
		out->tell = SeekableOutputStream_tell;

		/* these are now owned by 'out' */
		state = NULL;
		stream = NULL;
		array = NULL;

		/* out becomes owned by 'wri' as soon as we call, even if it throws. */
		out_temp = out;
		out = NULL;
		wri = fz_new_document_writer_with_output(ctx, out_temp, format, options);
	}
	fz_always(ctx)
	{
		fz_drop_output(ctx, out);
		if (options)
			(*env)->ReleaseStringUTFChars(env, joptions, options);
		if (format)
			(*env)->ReleaseStringUTFChars(env, jformat, format);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(wri);
}

JNIEXPORT jobject JNICALL
FUN(DocumentWriter_beginPage)(JNIEnv *env, jobject self, jobject jmediabox)
{
	fz_context *ctx = get_context(env);
	fz_document_writer *wri = from_DocumentWriter(env, self);
	fz_rect mediabox = from_Rect(env, jmediabox);
	fz_device *device = NULL;

	if (!ctx || !wri) return NULL;

	fz_try(ctx)
		device = fz_begin_page(ctx, wri, mediabox);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_NativeDevice_safe_own(ctx, env, fz_keep_device(ctx, device));
}

JNIEXPORT void JNICALL
FUN(DocumentWriter_endPage)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document_writer *wri = from_DocumentWriter(env, self);

	if (!ctx || !wri) return;

	fz_try(ctx)
		fz_end_page(ctx, wri);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(DocumentWriter_close)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document_writer *wri = from_DocumentWriter(env, self);

	if (!ctx || !wri) return;

	fz_try(ctx)
		fz_close_document_writer(ctx, wri);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

static int
jni_ocr_progress(fz_context *ctx, void *arg, int page, int percent)
{
	jobject ref = (jobject)arg;
	jboolean cancel;
	JNIEnv *env = NULL;
	jboolean detach = JNI_FALSE;

	if (ref == NULL)
		return JNI_FALSE;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in jni_ocr_progress");

	cancel = (*env)->CallBooleanMethod(env, ref, mid_DocumentWriter_OCRListener_progress, page, percent);
	if ((*env)->ExceptionCheck(env))
		cancel = 1;

	jni_detach_thread(detach);

	return !!cancel;
}

JNIEXPORT void JNICALL
FUN(DocumentWriter_addOCRListener)(JNIEnv *env, jobject self, jobject jlistener)
{
	fz_context *ctx = get_context(env);
	fz_document_writer *wri = from_DocumentWriter(env, self);
	jobject ref;

	if (!ctx || !wri) return;

	/* Delete any old OCRListener if there is one. */
	ref = (jobject)(*env)->GetLongField(env, self, fid_DocumentWriter_ocrlistener);
	if (ref != NULL)
	{
		(*env)->DeleteGlobalRef(env, ref);
		(*env)->SetLongField(env, self, fid_DocumentWriter_ocrlistener, 0);
	}

	/* Take a ref and store it for the callback to use */
	ref = (*env)->NewGlobalRef(env, jlistener);
	if (!ref)
		jni_throw_run_void(env, "cannot take reference to listener");
	(*env)->SetLongField(env, self, fid_DocumentWriter_ocrlistener, jlong_cast(ref));

	fz_try(ctx)
		fz_pdfocr_writer_set_progress(ctx, wri, jni_ocr_progress, ref);
	fz_catch(ctx)
	{
		(*env)->DeleteGlobalRef(env, ref);
		(*env)->SetLongField(env, self, fid_DocumentWriter_ocrlistener, 0);
		jni_rethrow_void(env, ctx);
	}

}
