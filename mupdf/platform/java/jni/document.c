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

/* Document interface */

/* Callbacks to implement fz_stream and fz_output using Java classes */

typedef struct
{
	jobject stream;
	jbyteArray array;
	jbyte buffer[8192];
}
SeekableStreamState;

static int SeekableInputStream_next(fz_context *ctx, fz_stream *stm, size_t max)
{
	SeekableStreamState *state = stm->state;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;
	int n, ch;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableInputStream_next");

	n = (*env)->CallIntMethod(env, state->stream, mid_SeekableInputStream_read, state->array);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);

	if (n > 0)
	{
		(*env)->GetByteArrayRegion(env, state->array, 0, n, state->buffer);
		if ((*env)->ExceptionCheck(env))
			fz_throw_java_and_detach_thread(ctx, env, detach);

		/* update stm->pos so fz_tell knows the current position */
		stm->rp = (unsigned char *)state->buffer;
		stm->wp = stm->rp + n;
		stm->pos += n;

		ch = *stm->rp++;
	}
	else if (n < 0)
	{
		ch = EOF;
	}
	else
		fz_throw_and_detach_thread(ctx, detach, FZ_ERROR_GENERIC, "no bytes read");

	jni_detach_thread(detach);
	return ch;
}

static void SeekableInputStream_seek(fz_context *ctx, fz_stream *stm, int64_t offset, int whence)
{
	SeekableStreamState *state = stm->state;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;
	int64_t pos;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableInputStream_seek");

	pos = (*env)->CallLongMethod(env, state->stream, mid_SeekableStream_seek, offset, whence);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);

	stm->pos = pos;
	stm->rp = stm->wp = (unsigned char *)state->buffer;

	jni_detach_thread(detach);
}

static void SeekableInputStream_drop(fz_context *ctx, void *streamState_)
{
	SeekableStreamState *state = streamState_;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;

	env = jni_attach_thread(&detach);
	if (env == NULL)
	{
		fz_warn(ctx, "cannot attach to JVM in SeekableInputStream_drop; leaking input stream");
		return;
	}

	(*env)->DeleteGlobalRef(env, state->stream);
	(*env)->DeleteGlobalRef(env, state->array);

	fz_free(ctx, state);

	jni_detach_thread(detach);
}

static void SeekableOutputStream_write(fz_context *ctx, void *streamState_, const void *buffer_, size_t count)
{
	SeekableStreamState *state = streamState_;
	const jbyte *buffer = buffer_;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableOutputStream_write");

	while (count > 0)
	{
		size_t n = fz_minz(count, sizeof(state->buffer));

		(*env)->SetByteArrayRegion(env, state->array, 0, n, buffer);
		if ((*env)->ExceptionCheck(env))
			fz_throw_java_and_detach_thread(ctx, env, detach);

		buffer += n;
		count -= n;

		(*env)->CallVoidMethod(env, state->stream, mid_SeekableOutputStream_write, state->array, 0, n);
		if ((*env)->ExceptionCheck(env))
			fz_throw_java_and_detach_thread(ctx, env, detach);
	}

	jni_detach_thread(detach);
}

static int64_t SeekableOutputStream_tell(fz_context *ctx, void *streamState_)
{
	SeekableStreamState *state = streamState_;
	jboolean detach = JNI_FALSE;
	int64_t pos = 0;
	JNIEnv *env;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableOutputStream_tell");

	pos = (*env)->CallLongMethod(env, state->stream, mid_SeekableStream_position);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);

	jni_detach_thread(detach);

	return pos;
}

static void SeekableOutputStream_truncate(fz_context *ctx, void *streamState_)
{
	SeekableStreamState *state = streamState_;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableOutputStream_truncate");

	(*env)->CallVoidMethod(env, state->stream, mid_SeekableOutputStream_truncate);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);

	jni_detach_thread(detach);
}

static void SeekableOutputStream_seek(fz_context *ctx, void *streamState_, int64_t offset, int whence)
{
	SeekableStreamState *state = streamState_;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableOutputStream_seek");

	(void) (*env)->CallLongMethod(env, state->stream, mid_SeekableStream_seek, offset, whence);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);

	jni_detach_thread(detach);
}

static void SeekableOutputStream_drop(fz_context *ctx, void *streamState_)
{
	SeekableStreamState *state = streamState_;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;

	env = jni_attach_thread(&detach);
	if (env == NULL)
	{
		fz_warn(ctx, "cannot attach to JVM in SeekableOutputStream_drop; leaking output stream");
		return;
	}

	(*env)->DeleteGlobalRef(env, state->stream);
	(*env)->DeleteGlobalRef(env, state->array);

	fz_free(ctx, state);

	jni_detach_thread(detach);
}
JNIEXPORT void JNICALL
FUN(Document_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document_safe(env, self);
	if (!ctx || !doc) return;
	(*env)->SetLongField(env, self, fid_Document_pointer, 0);
	fz_drop_document(ctx, doc);

	/* This is a reasonable place to call Memento. */
	Memento_fin();
}

JNIEXPORT jobject JNICALL
FUN(Document_openNativeWithStream)(JNIEnv *env, jclass cls, jstring jmagic, jobject jdocument, jobject jaccelerator)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = NULL;
	fz_stream *docstream = NULL;
	fz_stream *accstream = NULL;
	jobject jdoc = NULL;
	jobject jacc = NULL;
	jbyteArray docarray = NULL;
	jbyteArray accarray = NULL;
	SeekableStreamState *docstate = NULL;
	SeekableStreamState *accstate = NULL;
	const char *magic = NULL;

	fz_var(jdoc);
	fz_var(jacc);
	fz_var(docarray);
	fz_var(accarray);
	fz_var(docstream);
	fz_var(accstream);

	if (!ctx) return NULL;
	if (jmagic)
	{
		magic = (*env)->GetStringUTFChars(env, jmagic, NULL);
		if (!magic) jni_throw_run(env, "cannot get characters in magic string");
	}
	if (jdocument)
	{
		jdoc = (*env)->NewGlobalRef(env, jdocument);
		if (!jdoc)
		{
			if (magic) (*env)->ReleaseStringUTFChars(env, jmagic, magic);
			jni_throw_run(env, "cannot get reference to document stream");
		}
	}
	if (jaccelerator)
	{
		jacc = (*env)->NewGlobalRef(env, jaccelerator);
		if (!jacc)
		{
			(*env)->DeleteGlobalRef(env, jdoc);
			if (magic) (*env)->ReleaseStringUTFChars(env, jmagic, magic);
			jni_throw_run(env, "cannot get reference to accelerator stream");
		}
	}

	docarray = (*env)->NewByteArray(env, sizeof docstate->buffer);
	if (docarray)
		docarray = (*env)->NewGlobalRef(env, docarray);
	if (!docarray)
	{
		(*env)->DeleteGlobalRef(env, jacc);
		(*env)->DeleteGlobalRef(env, jdoc);
		if (magic) (*env)->ReleaseStringUTFChars(env, jmagic, magic);
		jni_throw_run(env, "cannot create internal buffer for document stream");
	}

	accarray = (*env)->NewByteArray(env, sizeof accstate->buffer);
	if (accarray)
		accarray = (*env)->NewGlobalRef(env, accarray);
	if (!accarray)
	{
		(*env)->DeleteGlobalRef(env, docarray);
		(*env)->DeleteGlobalRef(env, jacc);
		(*env)->DeleteGlobalRef(env, jdoc);
		if (magic) (*env)->ReleaseStringUTFChars(env, jmagic, magic);
		jni_throw_run(env, "cannot create internal buffer for accelerator stream");
	}

	fz_try(ctx)
	{
		if (jdoc)
		{
			/* No exceptions can occur from here to stream owning docstate, so we must not free docstate. */
			docstate = Memento_label(fz_malloc(ctx, sizeof(SeekableStreamState)), "SeekableStreamState_docstate");
			docstate->stream = jdoc;
			docstate->array = docarray;

			/* Ownership transferred to docstate. */
			jdoc = NULL;
			docarray = NULL;

			/* Stream takes ownership of docstate. */
			docstream = fz_new_stream(ctx, docstate, SeekableInputStream_next, SeekableInputStream_drop);
			docstream->seek = SeekableInputStream_seek;
		}

		if (jacc)
		{
			/* No exceptions can occur from here to stream owning accstate, so we must not free accstate. */
			accstate = Memento_label(fz_malloc(ctx, sizeof(SeekableStreamState)), "SeekableStreamState_accstate");
			accstate->stream = jacc;
			accstate->array = accarray;

			/* Ownership transferred to accstate. */
			jacc = NULL;
			accarray = NULL;

			/* Stream takes ownership of accstate. */
			accstream = fz_new_stream(ctx, accstate, SeekableInputStream_next, SeekableInputStream_drop);
			accstream->seek = SeekableInputStream_seek;
		}

		doc = fz_open_accelerated_document_with_stream(ctx, magic, docstream, accstream);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, accstream);
		fz_drop_stream(ctx, docstream);
		if (magic)
			(*env)->ReleaseStringUTFChars(env, jmagic, magic);
	}
	fz_catch(ctx)
	{
		(*env)->DeleteGlobalRef(env, accarray);
		(*env)->DeleteGlobalRef(env, docarray);
		(*env)->DeleteGlobalRef(env, jacc);
		(*env)->DeleteGlobalRef(env, jdoc);
		jni_rethrow(env, ctx);
	}

	return to_Document_safe_own(ctx, env, doc);
}

JNIEXPORT jobject JNICALL
FUN(Document_openNativeWithPath)(JNIEnv *env, jclass cls, jstring jfilename, jstring jaccelerator)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = NULL;
	const char *filename = NULL;
	const char *accelerator = NULL;

	if (!ctx) return NULL;
	if (jfilename)
	{
		filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
		if (!filename) jni_throw_run(env, "cannot get characters in filename string");
	}
	if (jaccelerator)
	{
		accelerator = (*env)->GetStringUTFChars(env, jaccelerator, NULL);
		if (!accelerator) jni_throw_run(env, "cannot get characters in accelerator filename string");
	}

	fz_try(ctx)
		doc = fz_open_accelerated_document(ctx, filename, accelerator);
	fz_always(ctx)
	{
		if (accelerator)
			(*env)->ReleaseStringUTFChars(env, jaccelerator, accelerator);
		if (filename)
			(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Document_safe_own(ctx, env, doc);
}


JNIEXPORT jobject JNICALL
FUN(Document_openNativeWithPathAndStream)(JNIEnv *env, jclass cls, jstring jfilename, jobject jaccelerator)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = NULL;
	const char *filename = NULL;
	fz_stream *docstream = NULL;
	fz_stream *accstream = NULL;
	jobject jacc = NULL;
	jbyteArray accarray = NULL;
	SeekableStreamState *accstate = NULL;

	fz_var(jacc);
	fz_var(accarray);
	fz_var(accstream);
	fz_var(docstream);

	if (!ctx) return NULL;
	if (jfilename)
	{
		filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
		if (!filename) jni_throw_run(env, "cannot get characters in filename string");
	}
	if (jaccelerator)
	{
		jacc = (*env)->NewGlobalRef(env, jaccelerator);
		if (!jacc)
		{
			if (jfilename) (*env)->ReleaseStringUTFChars(env, jfilename, filename);
			jni_throw_run(env, "cannot get reference to accelerator stream");
		}
	}

	accarray = (*env)->NewByteArray(env, sizeof accstate->buffer);
	if (accarray)
		accarray = (*env)->NewGlobalRef(env, accarray);
	if (!accarray)
	{
		(*env)->DeleteGlobalRef(env, jacc);
		if (jfilename) (*env)->ReleaseStringUTFChars(env, jfilename, filename);
		jni_throw_run(env, "cannot get create internal buffer for accelerator stream");
	}

	fz_try(ctx)
	{
		if (filename)
			docstream = fz_open_file(ctx, filename);

		if (jacc)
		{
			/* No exceptions can occur from here to stream owning accstate, so we must not free accstate. */
			accstate = Memento_label(fz_malloc(ctx, sizeof(SeekableStreamState)), "SeekableStreamState_accstate2");
			accstate->stream = jacc;
			accstate->array = accarray;

			/* Ownership transferred to accstate. */
			jacc = NULL;
			accarray = NULL;

			/* Stream takes ownership of accstate. */
			accstream = fz_new_stream(ctx, accstate, SeekableInputStream_next, SeekableInputStream_drop);
			accstream->seek = SeekableInputStream_seek;
		}

		doc = fz_open_accelerated_document_with_stream(ctx, filename, docstream, accstream);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, accstream);
		fz_drop_stream(ctx, docstream);
		if (filename) (*env)->ReleaseStringUTFChars(env, jfilename, filename);
	}
	fz_catch(ctx)
	{
		(*env)->DeleteGlobalRef(env, accarray);
		(*env)->DeleteGlobalRef(env, jacc);
		jni_rethrow(env, ctx);
	}

	return to_Document_safe_own(ctx, env, doc);
}

JNIEXPORT jobject JNICALL
FUN(Document_openNativeWithBuffer)(JNIEnv *env, jclass cls, jstring jmagic, jobject jbuffer, jobject jaccelerator)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = NULL;
	const char *magic = NULL;
	fz_stream *docstream = NULL;
	fz_stream *accstream = NULL;
	fz_buffer *docbuf = NULL;
	fz_buffer *accbuf = NULL;
	jbyte *buffer = NULL;
	jbyte *accelerator = NULL;
	int n, m;

	fz_var(docbuf);
	fz_var(accbuf);
	fz_var(docstream);
	fz_var(accstream);

	if (!ctx) return NULL;
	if (jmagic)
	{
		magic = (*env)->GetStringUTFChars(env, jmagic, NULL);
		if (!magic)
			jni_throw_run(env, "cannot get characters in magic string");
	}
	if (jbuffer)
	{
		n = (*env)->GetArrayLength(env, jbuffer);

		buffer = (*env)->GetByteArrayElements(env, jbuffer, NULL);
		if (!buffer)
		{
			if (magic) (*env)->ReleaseStringUTFChars(env, jmagic, magic);
			jni_throw_run(env, "cannot get document bytes to read");
		}
	}
	if (jaccelerator)
	{
		m = (*env)->GetArrayLength(env, jaccelerator);

		accelerator = (*env)->GetByteArrayElements(env, jaccelerator, NULL);
		if (!accelerator)
		{
			if (buffer) (*env)->ReleaseByteArrayElements(env, jbuffer, buffer, 0);
			if (magic) (*env)->ReleaseStringUTFChars(env, jmagic, magic);
			jni_throw_run(env, "cannot get accelerator bytes to read");
		}
	}

	fz_try(ctx)
	{
		if (buffer)
		{
			docbuf = fz_new_buffer(ctx, n);
			fz_append_data(ctx, docbuf, buffer, n);
			docstream = fz_open_buffer(ctx, docbuf);
		}

		if (accelerator)
		{
			accbuf = fz_new_buffer(ctx, m);
			fz_append_data(ctx, accbuf, accelerator, m);
			accstream = fz_open_buffer(ctx, accbuf);
		}

		doc = fz_open_accelerated_document_with_stream(ctx, magic, docstream, accstream);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, accstream);
		fz_drop_buffer(ctx, accbuf);
		fz_drop_stream(ctx, docstream);
		fz_drop_buffer(ctx, docbuf);
		if (accelerator) (*env)->ReleaseByteArrayElements(env, jaccelerator, accelerator, 0);
		if (buffer) (*env)->ReleaseByteArrayElements(env, jbuffer, buffer, 0);
		if (magic) (*env)->ReleaseStringUTFChars(env, jmagic, magic);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}

	return to_Document_safe_own(ctx, env, doc);
}

JNIEXPORT jboolean JNICALL
FUN(Document_recognize)(JNIEnv *env, jclass cls, jstring jmagic)
{
	fz_context *ctx = get_context(env);
	const char *magic = NULL;
	jboolean recognized = JNI_FALSE;

	if (!ctx) return JNI_FALSE;
	if (jmagic)
	{
		magic = (*env)->GetStringUTFChars(env, jmagic, NULL);
		if (!magic) return JNI_FALSE;
	}

	fz_try(ctx)
		recognized = fz_recognize_document(ctx, magic) != NULL;
	fz_always(ctx)
		if (magic) (*env)->ReleaseStringUTFChars(env, jmagic, magic);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return recognized;
}

JNIEXPORT jboolean JNICALL
FUN(Document_supportsAccelerator)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	jboolean support = JNI_FALSE;

	fz_try(ctx)
		support = fz_document_supports_accelerator(ctx, doc);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return support;
}

JNIEXPORT void JNICALL
FUN(Document_saveAccelerator)(JNIEnv *env, jobject self, jstring jfilename)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	const char *filename = "null";

	if (!ctx || !doc) return;
	if (!jfilename) jni_throw_arg_void(env, "filename must not be null");

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return;

	fz_try(ctx)
		fz_save_accelerator(ctx, doc, filename);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Document_outputAccelerator)(JNIEnv *env, jobject self, jobject jstream)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	SeekableStreamState *state = NULL;
	jobject stream = NULL;
	jbyteArray array = NULL;
	fz_output *out;

	fz_var(state);
	fz_var(out);
	fz_var(stream);
	fz_var(array);

	stream = (*env)->NewGlobalRef(env, jstream);
	if (!stream)
		return;

	array = (*env)->NewByteArray(env, sizeof state->buffer);
	if (array)
		array = (*env)->NewGlobalRef(env, array);
	if (!array)
	{
		(*env)->DeleteGlobalRef(env, stream);
		return;
	}

	fz_try(ctx)
	{
		state = Memento_label(fz_malloc(ctx, sizeof(SeekableStreamState)), "SeekableStreamState_outputAccelerator");
		state->stream = stream;
		state->array = array;

		out = fz_new_output(ctx, 8192, state, SeekableOutputStream_write, NULL, SeekableOutputStream_drop);
		out->seek = SeekableOutputStream_seek;
		out->tell = SeekableOutputStream_tell;

		/* these are now owned by 'out' */
		state = NULL;
		stream = NULL;
		array = NULL;

		fz_output_accelerator(ctx, doc, out);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		fz_drop_output(ctx, out);
	}
	fz_catch(ctx)
	{
		(*env)->DeleteGlobalRef(env, stream);
		(*env)->DeleteGlobalRef(env, array);
		fz_free(ctx, state);
		jni_rethrow_void(env, ctx);
	}
}

JNIEXPORT jboolean JNICALL
FUN(Document_needsPassword)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	int okay = 0;

	if (!ctx || !doc) return JNI_FALSE;

	fz_try(ctx)
		okay = fz_needs_password(ctx, doc);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return okay ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(Document_authenticatePassword)(JNIEnv *env, jobject self, jstring jpassword)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	const char *password = NULL;
	int okay = 0;

	if (!ctx || !doc) return JNI_FALSE;

	if (jpassword)
	{
		password = (*env)->GetStringUTFChars(env, jpassword, NULL);
		if (!password) return JNI_FALSE;
	}

	fz_try(ctx)
		okay = fz_authenticate_password(ctx, doc, password);
	fz_always(ctx)
		if (password) (*env)->ReleaseStringUTFChars(env, jpassword, password);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return okay ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
FUN(Document_countChapters)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	int count = 0;

	if (!ctx || !doc) return 0;

	fz_try(ctx)
		count = fz_count_chapters(ctx, doc);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return count;
}

JNIEXPORT jint JNICALL
FUN(Document_countPages)(JNIEnv *env, jobject self, jint chapter)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	int count = 0;

	if (!ctx || !doc) return 0;

	fz_try(ctx)
		count = fz_count_chapter_pages(ctx, doc, chapter);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return count;
}

JNIEXPORT jboolean JNICALL
FUN(Document_isReflowable)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	int is_reflowable = 0;

	if (!ctx || !doc) return JNI_FALSE;

	fz_try(ctx)
		is_reflowable = fz_is_document_reflowable(ctx, doc);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return is_reflowable ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
FUN(Document_layout)(JNIEnv *env, jobject self, jfloat w, jfloat h, jfloat em)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);

	if (!ctx || !doc) return;

	fz_try(ctx)
		fz_layout_document(ctx, doc, w, h, em);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(Document_loadPage)(JNIEnv *env, jobject self, jint chapter, jint number)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	fz_page *page = NULL;

	if (!ctx || !doc) return NULL;

	fz_try(ctx)
		page = fz_load_chapter_page(ctx, doc, chapter, number);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Page_safe_own(ctx, env, page);
}

JNIEXPORT jstring JNICALL
FUN(Document_getMetaData)(JNIEnv *env, jobject self, jstring jkey)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	const char *key = NULL;
	char info[256];

	if (!ctx || !doc) return NULL;
	if (!jkey) jni_throw_arg(env, "key must not be null");

	key = (*env)->GetStringUTFChars(env, jkey, NULL);
	if (!key) return 0;

	fz_try(ctx)
		fz_lookup_metadata(ctx, doc, key, info, sizeof info);
	fz_always(ctx)
		if (key)
			(*env)->ReleaseStringUTFChars(env, jkey, key);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewStringUTF(env, info);
}

JNIEXPORT void JNICALL
FUN(Document_setMetaData)(JNIEnv *env, jobject self, jstring jkey, jstring jvalue)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	const char *key = NULL;
	const char *value = NULL;

	if (!ctx || !doc) return;
	if (!jkey) jni_throw_arg_void(env, "key must not be null");
	if (!jvalue) jni_throw_arg_void(env, "value must not be null");

	key = (*env)->GetStringUTFChars(env, jkey, NULL);
	value = (*env)->GetStringUTFChars(env, jvalue, NULL);
	if (!key || !value)
	{
		if (key)
			(*env)->ReleaseStringUTFChars(env, jkey, key);
		return;
	}

	fz_try(ctx)
		fz_set_metadata(ctx, doc, key, value);
	fz_always(ctx)
	{
		(*env)->ReleaseStringUTFChars(env, jkey, key);
		(*env)->ReleaseStringUTFChars(env, jvalue, value);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(Document_isUnencryptedPDF)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	pdf_document *idoc = pdf_specifics(ctx, doc);
	int cryptVer;

	if (!ctx || !doc) return JNI_FALSE;
	if (!idoc)
		return JNI_FALSE;

	cryptVer = pdf_crypt_version(ctx, idoc->crypt);
	return (cryptVer == 0) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobject JNICALL
FUN(Document_loadOutline)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	fz_outline *outline = NULL;
	jobject joutline = NULL;

	if (!ctx || !doc) return NULL;

	fz_var(outline);

	fz_try(ctx)
	{
		outline = fz_load_outline(ctx, doc);
		if (outline)
		{
			joutline = to_Outline_safe(ctx, env, doc, outline);
			if (!joutline && !(*env)->ExceptionCheck(env))
				fz_throw(ctx, FZ_ERROR_GENERIC, "loadOutline failed");
		}
	}
	fz_always(ctx)
		fz_drop_outline(ctx, outline);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	if ((*env)->ExceptionCheck(env))
		return NULL;

	return joutline;
}

JNIEXPORT jobject JNICALL
FUN(Document_outlineIterator)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	fz_outline_iterator *iterator = NULL;
	jobject jiterator = NULL;

	if (!ctx || !doc) return NULL;

	fz_var(iterator);

	fz_try(ctx)
	{
		iterator = fz_new_outline_iterator(ctx, doc);
		if (iterator)
		{
			jiterator = to_OutlineIterator_safe(ctx, env, iterator);
			if (!jiterator || (*env)->ExceptionCheck(env))
				fz_throw(ctx, FZ_ERROR_GENERIC, "outlineIterator failed");
			iterator = NULL;
		}
	}
	fz_always(ctx)
		fz_drop_outline_iterator(ctx, iterator);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	if ((*env)->ExceptionCheck(env))
		return NULL;

	return jiterator;
}

JNIEXPORT jlong JNICALL
FUN(Document_makeBookmark)(JNIEnv *env, jobject self, jint chapter, jint page)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	fz_bookmark mark = 0;

	fz_try(ctx)
		mark = fz_make_bookmark(ctx, doc, fz_make_location(chapter, page));
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return mark;
}

JNIEXPORT jobject JNICALL
FUN(Document_findBookmark)(JNIEnv *env, jobject self, jlong mark)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	fz_location loc = { -1, -1 };

	fz_try(ctx)
		loc = fz_lookup_bookmark(ctx, doc, mark);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewObject(env, cls_Location, mid_Location_init, loc.chapter, loc.page, 0, 0);
}

JNIEXPORT jobject JNICALL
FUN(Document_resolveLink)(JNIEnv *env, jobject self, jstring juri)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	fz_location loc = { -1, -1 };
	float x = 0, y = 0;
	const char *uri = "";

	if (juri)
	{
		uri = (*env)->GetStringUTFChars(env, juri, NULL);
		if (!uri)
			return NULL;
	}

	fz_try(ctx)
		loc = fz_resolve_link(ctx, doc, uri, &x, &y);
	fz_always(ctx)
		if (juri)
			(*env)->ReleaseStringUTFChars(env, juri, uri);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewObject(env, cls_Location, mid_Location_init, loc.chapter, loc.page, x, y);
}

JNIEXPORT jboolean JNICALL
FUN(Document_hasPermission)(JNIEnv *env, jobject self, jint permission)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	jboolean result = JNI_FALSE;

	fz_try(ctx)
		result = fz_has_permission(ctx, doc, permission);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return result;
}

JNIEXPORT jobject JNICALL
FUN(Document_search)(JNIEnv *env, jobject self, jint chapter, jint page, jstring jneedle)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	fz_quad hits[500];
	int marks[500];
	const char *needle = NULL;
	int n = 0;

	if (!ctx) return NULL;
	if (!jneedle) return NULL;

	needle = (*env)->GetStringUTFChars(env, jneedle, NULL);
	if (!needle) return 0;

	fz_try(ctx)
		n = fz_search_chapter_page_number(ctx, doc, chapter, page, needle, marks, hits, nelem(hits));
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jneedle, needle);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_SearchHits_safe(ctx, env, marks, hits, n);
}

JNIEXPORT jobject JNICALL
FUN(Document_resolveLinkDestination)(JNIEnv *env, jobject self, jstring juri)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	const char *uri = "";
	fz_link_dest dest;
	jobject jdestination;

	if (!ctx || !doc) return NULL;

	if (juri)
	{
		uri = (*env)->GetStringUTFChars(env, juri, NULL);
		if (!uri)
			return NULL;
	}

	fz_try(ctx)
		dest = fz_resolve_link_dest(ctx, doc, uri);
	fz_always(ctx)
		if (juri)
			(*env)->ReleaseStringUTFChars(env, juri, uri);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jdestination = (*env)->NewObject(env, cls_LinkDestination, mid_LinkDestination_init,
		dest.loc.chapter, dest.loc.page, dest.type, dest.x, dest.y, dest.w, dest.h, dest.zoom);
	if (!jdestination || (*env)->ExceptionCheck(env))
		return NULL;

	return jdestination;
}

JNIEXPORT jstring JNICALL
FUN(Document_formatLinkURI)(JNIEnv *env, jobject self, jobject jdest)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	fz_link_dest dest = from_LinkDestination(env, jdest);
	char *uri = NULL;
	jobject juri;

	fz_try(ctx)
		uri = fz_format_link_uri(ctx, doc, dest);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	juri = (*env)->NewStringUTF(env, uri);
	fz_free(ctx, uri);
	if (juri == NULL || (*env)->ExceptionCheck(env))
		return NULL;

	return juri;
}
