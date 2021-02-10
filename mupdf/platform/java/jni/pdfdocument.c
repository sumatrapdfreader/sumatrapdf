/* PDFDocument interface */

static void event_cb(fz_context *ctx, pdf_document *doc, pdf_doc_event *event, void *data)
{
	jobject jlistener = (jobject)data;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;

	env = jni_attach_thread(ctx, &detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in event_cb");

	switch (event->type)
	{
	case PDF_DOCUMENT_EVENT_ALERT:
		{
			pdf_alert_event *alert;
			jstring jstring = NULL;

			alert = pdf_access_alert_event(ctx, event);

			jstring = (*env)->NewStringUTF(env, alert->message);
			if (!jstring || (*env)->ExceptionCheck(env))
				fz_throw_java_and_detach_thread(ctx, env, detach);

			(*env)->CallVoidMethod(env, jlistener, mid_PDFDocument_JsEventListener_onAlert, jstring);
			if ((*env)->ExceptionCheck(env))
				fz_throw_java_and_detach_thread(ctx, env, detach);
		}
		break;

	default:
		fz_throw_and_detach_thread(ctx, detach, FZ_ERROR_GENERIC, "event not yet implemented");
		break;
	}

	jni_detach_thread(detach);
}

JNIEXPORT jlong JNICALL
FUN(PDFDocument_newNative)(JNIEnv *env, jclass cls)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = NULL;

	if (!ctx) return 0;

	fz_try(ctx)
		doc = pdf_create_document(ctx);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(doc);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);
	void *data = NULL;
	if (!ctx || !pdf) return;
	data = pdf_get_doc_event_callback_data(ctx, pdf);
	if (data)
		(*env)->DeleteGlobalRef(env, data);
	FUN(Document_finalize)(env, self); /* Call super.finalize() */
}

JNIEXPORT jint JNICALL
FUN(PDFDocument_countObjects)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	int count = 0;

	if (!ctx || !pdf) return 0;

	fz_try(ctx)
		count = pdf_xref_len(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return count;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newNull)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = NULL;
	jobject jobj;

	if (!ctx) return NULL;

	obj = PDF_NULL;

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj));
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newBoolean)(JNIEnv *env, jobject self, jboolean b)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = NULL;
	jobject jobj;

	if (!ctx) return NULL;

	obj = b ? PDF_TRUE : PDF_FALSE;

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj));
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newInteger)(JNIEnv *env, jobject self, jint i)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = NULL;
	jobject jobj;

	if (!ctx) return NULL;

	fz_try(ctx)
		obj = pdf_new_int(ctx, i);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj));
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newReal)(JNIEnv *env, jobject self, jfloat f)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = NULL;
	jobject jobj;

	if (!ctx) return NULL;

	fz_try(ctx)
		obj = pdf_new_real(ctx, f);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj));
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newString)(JNIEnv *env, jobject self, jstring jstring)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = NULL;
	const char *s = NULL;
	jobject jobj;

	if (!ctx) return NULL;
	if (!jstring) jni_throw_arg(env, "string must not be null");

	s = (*env)->GetStringUTFChars(env, jstring, NULL);
	if (!s) return NULL;

	fz_try(ctx)
		obj = pdf_new_text_string(ctx, s);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jstring, s);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj));
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newByteString)(JNIEnv *env, jobject self, jobject jbs)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = NULL;
	jbyte *bs;
	size_t bslen;
	jobject jobj;

	if (!ctx) return NULL;
	if (!jbs) jni_throw_arg(env, "bs must not be null");

	bslen = (*env)->GetArrayLength(env, jbs);

	fz_try(ctx)
		bs = Memento_label(fz_malloc(ctx, bslen), "PDFDocument_newByteString");
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	(*env)->GetByteArrayRegion(env, jbs, 0, bslen, bs);
	if ((*env)->ExceptionCheck(env))
	{
		fz_free(ctx, bs);
		return NULL;
	}

	fz_try(ctx)
		obj = pdf_new_string(ctx, (char *) bs, bslen);
	fz_always(ctx)
		fz_free(ctx, bs);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj));
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newName)(JNIEnv *env, jobject self, jstring jname)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = NULL;
	const char *name = NULL;
	jobject jobj;

	if (!ctx) return NULL;
	if (!jname) jni_throw_arg(env, "name must not be null");

	name = (*env)->GetStringUTFChars(env, jname, NULL);
	if (!name) return NULL;

	fz_try(ctx)
		obj = pdf_new_name(ctx, name);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jname, name);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj));
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newIndirect)(JNIEnv *env, jobject self, jint num, jint gen)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;
	jobject jobj;

	if (!ctx || !pdf) return NULL;

	fz_try(ctx)
		obj = pdf_new_indirect(ctx, pdf, num, gen);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj));
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newArray)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;
	jobject jobj;

	if (!ctx || !pdf) return NULL;

	fz_try(ctx)
		obj = pdf_new_array(ctx, pdf, 0);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj));
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newDictionary)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;
	jobject jobj;

	if (!ctx || !pdf) return NULL;

	fz_try(ctx)
		obj = pdf_new_dict(ctx, pdf, 0);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj));
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_findPage)(JNIEnv *env, jobject self, jint jat)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;

	if (!ctx || !pdf) return NULL;
	if (jat < 0 || jat >= pdf_count_pages(ctx, pdf)) jni_throw_oob(env, "at is not a valid page");

	fz_try(ctx)
		obj = pdf_lookup_page_obj(ctx, pdf, jat);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe(ctx, env, obj);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_getTrailer)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;

	if (!ctx || !pdf) return NULL;

	fz_try(ctx)
		obj = pdf_trailer(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe(ctx, env, obj);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addObject)(JNIEnv *env, jobject self, jobject jobj)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = from_PDFObject(env, jobj);

	if (!ctx || !pdf) return NULL;
	if (!jobj) jni_throw_arg(env, "object must not be null");

	fz_try(ctx)
		obj = pdf_add_object_drop(ctx, pdf, obj);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_createObject)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;

	fz_try(ctx)
		ind = pdf_new_indirect(ctx, pdf, pdf_create_object(ctx, pdf), 0);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe_own(ctx, env, ind);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_deleteObject)(JNIEnv *env, jobject self, jint num)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);

	if (!ctx || !pdf) return;

	fz_try(ctx)
		pdf_delete_object(ctx, pdf, num);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newPDFGraftMap)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_graft_map *map = NULL;

	if (!ctx || !pdf) return NULL;

	fz_try(ctx)
		map = pdf_new_graft_map(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFGraftMap_safe_own(ctx, env, self, map);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_graftObject)(JNIEnv *env, jobject self, jobject jobj)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, jobj);
	pdf_document *dst = from_PDFDocument(env, self);

	if (!ctx || !dst) return NULL;
	if (!dst) jni_throw_arg(env, "dst must not be null");
	if (!obj) jni_throw_arg(env, "object must not be null");

	fz_try(ctx)
		obj = pdf_graft_object(ctx, dst, obj);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe_own(ctx, env, obj);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addStreamBuffer)(JNIEnv *env, jobject self, jobject jbuf, jobject jobj, jboolean compressed)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = from_PDFObject(env, jobj);
	fz_buffer *buf = from_Buffer(env, jbuf);
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;
	if (!jbuf) jni_throw_arg(env, "buffer must not be null");

	fz_try(ctx)
		ind = pdf_add_stream(ctx, pdf, buf, obj, compressed);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe_own(ctx, env, ind);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addStreamString)(JNIEnv *env, jobject self, jstring jbuf, jobject jobj, jboolean compressed)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = from_PDFObject(env, jobj);
	fz_buffer *buf = NULL;
	const char *sbuf = NULL;
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;
	if (!jbuf) jni_throw_arg(env, "buffer must not be null");

	sbuf = (*env)->GetStringUTFChars(env, jbuf, NULL);
	if (!sbuf) return NULL;

	fz_var(buf);

	fz_try(ctx)
	{
		buf = fz_new_buffer_from_copied_data(ctx, (const unsigned char *)sbuf, strlen(sbuf));
		ind = pdf_add_stream(ctx, pdf, buf, obj, compressed);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
		(*env)->ReleaseStringUTFChars(env, jbuf, sbuf);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe_own(ctx, env, ind);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addPageBuffer)(JNIEnv *env, jobject self, jobject jmediabox, jint rotate, jobject jresources, jobject jcontents)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	fz_rect mediabox = from_Rect(env, jmediabox);
	pdf_obj *resources = from_PDFObject(env, jresources);
	fz_buffer *contents = from_Buffer(env, jcontents);
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;
	if (!resources) jni_throw_arg(env, "resources must not be null");
	if (!contents) jni_throw_arg(env, "contents must not be null");

	fz_try(ctx)
		ind = pdf_add_page(ctx, pdf, mediabox, rotate, resources, contents);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe_own(ctx, env, ind);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addPageString)(JNIEnv *env, jobject self, jobject jmediabox, jint rotate, jobject jresources, jstring jcontents)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	fz_rect mediabox = from_Rect(env, jmediabox);
	pdf_obj *resources = from_PDFObject(env, jresources);
	const char *scontents = NULL;
	fz_buffer *contents = NULL;
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;
	if (!resources) jni_throw_arg(env, "resources must not be null");
	if (!jcontents) jni_throw_arg(env, "contents must not be null");

	scontents = (*env)->GetStringUTFChars(env, jcontents, NULL);
	if (!scontents) return NULL;

	fz_var(contents);

	fz_try(ctx)
	{
		contents = fz_new_buffer_from_copied_data(ctx, (const unsigned char *)scontents, strlen(scontents));
		ind = pdf_add_page(ctx, pdf, mediabox, rotate, resources, contents);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, contents);
		(*env)->ReleaseStringUTFChars(env, jcontents, scontents);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe_own(ctx, env, ind);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_insertPage)(JNIEnv *env, jobject self, jint jat, jobject jpage)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	int at = jat;
	pdf_obj *page = from_PDFObject(env, jpage);

	if (!ctx || !pdf) return;
	if (jat != INT_MAX && jat >= pdf_count_pages(ctx, pdf)) jni_throw_oob_void(env, "at is not a valid page");
	if (!page) jni_throw_arg_void(env, "page must not be null");

	fz_try(ctx)
		pdf_insert_page(ctx, pdf, at, page);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_deletePage)(JNIEnv *env, jobject self, jint jat)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	int at = jat;

	if (!ctx || !pdf) return;
	if (jat < 0 || jat >= pdf_count_pages(ctx, pdf)) jni_throw_oob_void(env, "at is not a valid page");

	fz_try(ctx)
		pdf_delete_page(ctx, pdf, at);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addImage)(JNIEnv *env, jobject self, jobject jimage)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	fz_image *image = from_Image(env, jimage);
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;
	if (!image) jni_throw_arg(env, "image must not be null");

	fz_try(ctx)
		ind = pdf_add_image(ctx, pdf, image);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe_own(ctx, env, ind);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addFont)(JNIEnv *env, jobject self, jobject jfont)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	fz_font *font = from_Font(env, jfont);
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;
	if (!font) jni_throw_arg(env, "font must not be null");

	fz_try(ctx)
		ind = pdf_add_cid_font(ctx, pdf, font);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe_own(ctx, env, ind);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addCJKFont)(JNIEnv *env, jobject self, jobject jfont, jint ordering, jint wmode, jboolean serif)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	fz_font *font = from_Font(env, jfont);
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;
	if (!font) jni_throw_arg(env, "font must not be null");

	fz_try(ctx)
		ind = pdf_add_cjk_font(ctx, pdf, font, ordering, wmode, serif);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe_own(ctx, env, ind);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addSimpleFont)(JNIEnv *env, jobject self, jobject jfont, jint encoding)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	fz_font *font = from_Font(env, jfont);
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;
	if (!font) jni_throw_arg(env, "font must not be null");

	fz_try(ctx)
		ind = pdf_add_simple_font(ctx, pdf, font, encoding);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe_own(ctx, env, ind);
}

JNIEXPORT jboolean JNICALL
FUN(PDFDocument_hasUnsavedChanges)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	if (!ctx || !pdf) return JNI_FALSE;
	return pdf_has_unsaved_changes(ctx, pdf) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(PDFDocument_wasRepaired)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	if (!ctx || !pdf) return JNI_FALSE;
	return pdf_was_repaired(ctx, pdf) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(PDFDocument_canBeSavedIncrementally)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	if (!ctx || !pdf) return JNI_FALSE;
	return pdf_can_be_saved_incrementally(ctx, pdf) ? JNI_TRUE : JNI_FALSE;
}

static fz_stream *
SeekableOutputStream_as_stream(fz_context *ctx, void *opaque)
{
	SeekableStreamState *in_state = opaque;
	SeekableStreamState *state;
	fz_stream *stm;

	state = Memento_label(fz_malloc(ctx, sizeof(SeekableStreamState)), "SeekableStreamState_state");
	state->stream = in_state->stream;
	state->array = in_state->array;

	stm = fz_new_stream(ctx, state, SeekableInputStream_next, fz_free);
	stm->seek = SeekableInputStream_seek;

	return stm;
}

JNIEXPORT void JNICALL
FUN(PDFDocument_nativeSaveWithStream)(JNIEnv *env, jobject self, jobject jstream, jstring joptions)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	SeekableStreamState *state = NULL;
	jobject stream = NULL;
	jbyteArray array = NULL;
	fz_output *out = NULL;
	const char *options = NULL;
	pdf_write_options pwo;

	fz_var(state);
	fz_var(out);
	fz_var(stream);
	fz_var(array);

	if (!ctx || !pdf) return;
	if (!jstream) jni_throw_arg_void(env, "stream must not be null");

	if (joptions)
	{
		options = (*env)->GetStringUTFChars(env, joptions, NULL);
		if (!options) return;
	}

	stream = (*env)->NewGlobalRef(env, jstream);
	if (!stream)
	{
		if (options) (*env)->ReleaseStringUTFChars(env, joptions, options);
		return;
	}

	array = (*env)->NewByteArray(env, sizeof state->buffer);
	if ((*env)->ExceptionCheck(env))
	{
		if (options) (*env)->ReleaseStringUTFChars(env, joptions, options);
		(*env)->DeleteGlobalRef(env, stream);
		return;
	}
	if (!array)
	{
		if (options) (*env)->ReleaseStringUTFChars(env, joptions, options);
		(*env)->DeleteGlobalRef(env, stream);
		jni_throw_run_void(env, "cannot create byte array");
	}

	array = (*env)->NewGlobalRef(env, array);
	if (!array)
	{
		if (options) (*env)->ReleaseStringUTFChars(env, joptions, options);
		(*env)->DeleteGlobalRef(env, stream);
		jni_throw_run_void(env, "cannot create global reference");
	}

	fz_try(ctx)
	{
		if (jstream)
		{
			/* No exceptions can occur from here to stream owning state, so we must not free state. */
			state = Memento_label(fz_malloc(ctx, sizeof(SeekableStreamState)), "SeekableStreamState_state");
			state->stream = stream;
			state->array = array;

			/* Ownership transferred to state. */
			stream = NULL;
			array = NULL;

			/* Stream takes ownership of state. */
			out = fz_new_output(ctx, sizeof state->buffer, state, SeekableOutputStream_write, NULL, SeekableOutputStream_drop);
			out->seek = SeekableOutputStream_seek;
			out->tell = SeekableOutputStream_tell;
			out->as_stream = SeekableOutputStream_as_stream;

			/* these are now owned by 'out' */
			state = NULL;
		}

		pdf_parse_write_options(ctx, &pwo, options);
		pdf_write_document(ctx, pdf, out, &pwo);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		fz_drop_output(ctx, out);
		if (options)
			(*env)->ReleaseStringUTFChars(env, joptions, options);
	}
	fz_catch(ctx)
	{
		(*env)->DeleteGlobalRef(env, array);
		(*env)->DeleteGlobalRef(env, stream);
		jni_rethrow_void(env, ctx);
	}
}

JNIEXPORT void JNICALL
FUN(PDFDocument_save)(JNIEnv *env, jobject self, jstring jfilename, jstring joptions)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	const char *filename = NULL;
	const char *options = NULL;
	pdf_write_options pwo;

	if (!ctx || !pdf) return;
	if (!jfilename) jni_throw_arg_void(env, "filename must not be null");

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return;

	if (joptions)
	{
		options = (*env)->GetStringUTFChars(env, joptions, NULL);
		if (!options)
		{
			(*env)->ReleaseStringUTFChars(env, jfilename, filename);
			return;
		}
	}

	fz_try(ctx)
	{
		pdf_parse_write_options(ctx, &pwo, options);
		pdf_save_document(ctx, pdf, filename, &pwo);
	}
	fz_always(ctx)
	{
		if (options)
			(*env)->ReleaseStringUTFChars(env, joptions, options);
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_enableJs)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);

	if (!ctx || !pdf) return;

	fz_try(ctx)
		pdf_enable_js(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_disableJs)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);

	if (!ctx || !pdf) return;

	fz_try(ctx)
		pdf_disable_js(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFDocument_isJsSupported)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);
	jboolean supported = JNI_FALSE;

	if (!ctx || !pdf) return JNI_FALSE;

	fz_try(ctx)
		supported = pdf_js_supported(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return supported;
}

JNIEXPORT void JNICALL
FUN(PDFDocument_setJsEventListener)(JNIEnv *env, jobject self, jobject jlistener)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);
	void *data = NULL;

	if (!ctx || !pdf) return;
	if (!jlistener) jni_throw_arg_void(env, "listener must not be null");

	jlistener = (*env)->NewGlobalRef(env, jlistener);
	if (!jlistener) jni_throw_arg_void(env, "unable to get reference to listener");

	fz_try(ctx)
	{
		data = pdf_get_doc_event_callback_data(ctx, pdf);
		if (data)
			(*env)->DeleteGlobalRef(env, data);
		pdf_set_doc_event_callback(ctx, pdf, event_cb, jlistener);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_calculate)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);

	if (!ctx || !pdf) return;

	fz_try(ctx)
		if (pdf->recalculate)
			pdf_calculate_form(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(PDFDocument_countVersions)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);
	int val = 0;

	if (!ctx || !pdf) return 0;

	fz_try(ctx)
		val = pdf_count_versions(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return val;
}

JNIEXPORT jint JNICALL
FUN(PDFDocument_countUnsavedVersions)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);
	int val = 0;

	if (!ctx || !pdf) return 0;

	fz_try(ctx)
		val = pdf_count_unsaved_versions(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return val;
}

JNIEXPORT jint JNICALL
FUN(PDFDocument_validateChangeHistory)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);
	int val = 0;

	if (!ctx || !pdf) return 0;

	fz_try(ctx)
		val = pdf_validate_change_history(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return val;
}

JNIEXPORT jboolean JNICALL
FUN(PDFDocument_wasPureXFA)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);
	jboolean val = JNI_FALSE;

	if (!ctx || !pdf) return JNI_FALSE;

	fz_try(ctx)
		val = pdf_was_pure_xfa(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return val;
}

JNIEXPORT jboolean JNICALL
FUN(PDFDocument_wasLinearized)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);
	jboolean val = JNI_FALSE;

	if (!ctx || !pdf) return JNI_FALSE;

	fz_try(ctx)
		val = pdf_doc_was_linearized(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return val;
}

JNIEXPORT void JNICALL
FUN(PDFDocument_graftPage)(JNIEnv *env, jobject self, jint pageTo, jobject jobj, jint pageFrom)
{
	fz_context *ctx = get_context(env);
	pdf_document *src = from_PDFDocument(env, jobj);
	pdf_document *dst = from_PDFDocument(env, self);

	if (!ctx || !dst) return;
	if (!src) jni_throw_arg_void(env, "Source Document must not be null");

	fz_try(ctx)
		pdf_graft_page(ctx, dst, pageTo, src, pageFrom);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_enableJournal)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);

	if (!ctx || !pdf) return;

	pdf_enable_journal(ctx, pdf);
}

JNIEXPORT jint JNICALL
FUN(PDFDocument_undoRedoPosition)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	int steps;

	if (!ctx || !pdf) return 0;

	return pdf_undoredo_state(ctx, pdf, &steps);
}

JNIEXPORT jint JNICALL
FUN(PDFDocument_undoRedoSteps)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	int steps;

	if (!ctx || !pdf) return 0;

	(void)pdf_undoredo_state(ctx, pdf, &steps);

	return steps;
}

JNIEXPORT jstring JNICALL
FUN(PDFDocument_undoRedoStep)(JNIEnv *env, jobject self, jint n)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	const char *step;

	if (!ctx || !pdf) return NULL;

	step = pdf_undoredo_step(ctx, pdf, n);

	return (*env)->NewStringUTF(env, step);
}

JNIEXPORT jboolean JNICALL
FUN(PDFDocument_canUndo)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);

	if (!ctx || !pdf) return JNI_FALSE;

	return pdf_can_undo(ctx, pdf);
}

JNIEXPORT jboolean JNICALL
FUN(PDFDocument_canRedo)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);

	if (!ctx || !pdf) return JNI_FALSE;

	return pdf_can_redo(ctx, pdf);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_undo)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);

	if (!ctx || !pdf) return;

	fz_try(ctx)
		pdf_undo(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_redo)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);

	if (!ctx || !pdf) return;

	fz_try(ctx)
		pdf_redo(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_beginOperation)(JNIEnv *env, jobject self, jstring joperation)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	const char *operation = NULL;

	if (!ctx || !pdf) return;
	if (!joperation) jni_throw_arg_void(env, "operation must not be null");

	operation = (*env)->GetStringUTFChars(env, joperation, NULL);
	if (!operation) return;

	fz_try(ctx)
		pdf_begin_operation(ctx, pdf, operation);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, joperation, operation);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_endOperation)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);

	if (!ctx || !pdf) return;

	fz_try(ctx)
		pdf_end_operation(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}
