/* DocumentWriter interface */

JNIEXPORT void JNICALL
FUN(DocumentWriter_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document_writer *wri = from_DocumentWriter_safe(env, self);
	if (!ctx || !wri) return;
	(*env)->SetLongField(env, self, fid_DocumentWriter_pointer, 0);
	fz_drop_document_writer(ctx, wri);
}

JNIEXPORT jlong JNICALL
FUN(DocumentWriter_newNativeDocumentWriter)(JNIEnv *env, jobject self, jstring jfilename, jstring jformat, jstring joptions)
{
	fz_context *ctx = get_context(env);
	fz_document_writer *wri = from_DocumentWriter(env, self);
	const char *filename = NULL;
	const char *format = NULL;
	const char *options = NULL;

	if (!ctx || !wri) return 0;
	if (!jfilename) return jni_throw_arg(env, "filename must not be null"), 0;

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
		return jni_rethrow(env, ctx), 0;

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
		return jni_rethrow(env, ctx), NULL;

	return to_Device_safe_own(ctx, env, device);
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
		return jni_rethrow(env, ctx);
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
		return jni_rethrow(env, ctx);
}
