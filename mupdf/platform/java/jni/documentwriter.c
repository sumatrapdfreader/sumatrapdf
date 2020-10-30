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
FUN(DocumentWriter_newNativeDocumentWriter)(JNIEnv *env, jobject self, jstring jfilename, jstring jformat, jstring joptions)
{
	fz_context *ctx = get_context(env);
	fz_document_writer *wri = from_DocumentWriter(env, self);
	const char *filename = NULL;
	const char *format = NULL;
	const char *options = NULL;

	if (!ctx || !wri) return 0;
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
jni_ocr_progress(fz_context *ctx, void *arg, int percent)
{
	jobject ref = (jobject)arg;
	jboolean cancel;
	JNIEnv *env = NULL;
	jboolean detach = JNI_FALSE;

	env = jni_attach_thread(ctx, &detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in jni_ocr_progress");

	cancel = (*env)->CallBooleanMethod(env, ref, mid_DocumentWriter_OCRListener_progress, percent);
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
