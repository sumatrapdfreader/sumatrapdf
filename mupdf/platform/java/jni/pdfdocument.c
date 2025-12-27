// Copyright (C) 2004-2025 Artifex Software, Inc.
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

/* PDFDocument interface */

static void free_event_cb_data(fz_context *ctx, void *data)
{
	jobject jlistener = (jobject)data;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		fz_warn(ctx, "cannot attach to JVM in free_event_cb_data");
	else
	{
		(*env)->DeleteGlobalRef(env, jlistener);
		jni_detach_thread(detach);
	}
}

static void event_cb(fz_context *ctx, pdf_document *pdf, pdf_doc_event *event, void *data)
{
	jobject jlistener = (jobject)data;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in event_cb");

	switch (event->type)
	{
	case PDF_DOCUMENT_EVENT_ALERT:
		{
			pdf_alert_event *alert;
			jstring jtitle = NULL;
			jstring jmessage = NULL;
			jstring jcheckboxmsg = NULL;
			jobject jalertresult;
			jobject jpdf;

			alert = pdf_access_alert_event(ctx, event);

			jpdf = to_PDFDocument_safe(ctx, env, pdf);
			if (!jpdf || (*env)->ExceptionCheck(env))
				fz_throw_java_and_detach_thread(ctx, env, detach);

			jtitle = (*env)->NewStringUTF(env, alert->title);
			if (!jtitle || (*env)->ExceptionCheck(env))
				fz_throw_java_and_detach_thread(ctx, env, detach);

			jmessage = (*env)->NewStringUTF(env, alert->message);
			if (!jmessage || (*env)->ExceptionCheck(env))
				fz_throw_java_and_detach_thread(ctx, env, detach);

			if (alert->has_check_box) {
				jcheckboxmsg = (*env)->NewStringUTF(env, alert->check_box_message);
				if (!jcheckboxmsg || (*env)->ExceptionCheck(env))
					fz_throw_java_and_detach_thread(ctx, env, detach);
			}

			jalertresult = (*env)->CallObjectMethod(env, jlistener, mid_PDFDocument_JsEventListener_onAlert, jpdf, jtitle, jmessage, alert->icon_type, alert->button_group_type, alert->has_check_box, jcheckboxmsg, alert->initially_checked);
			if ((*env)->ExceptionCheck(env))
				fz_throw_java_and_detach_thread(ctx, env, detach);

			if (jalertresult)
			{
				alert->button_pressed = (*env)->GetIntField(env, jalertresult, fid_AlertResult_buttonPressed);
				alert->finally_checked = (*env)->GetBooleanField(env, jalertresult, fid_AlertResult_checkboxChecked);
			}
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
	if (!ctx || !pdf) return;
	FUN(Document_finalize)(env, self); /* Call super.finalize() */
}

JNIEXPORT void JNICALL
FUN(PDFDocument_check)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);

	if (!ctx || !pdf) return;

	fz_try(ctx)
		pdf_check_document(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
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
			out->truncate = SeekableOutputStream_truncate;
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

	if (!ctx || !pdf) return;
	if (!jlistener) jni_throw_arg_void(env, "listener must not be null");

	jlistener = (*env)->NewGlobalRef(env, jlistener);
	if (!jlistener) jni_throw_arg_void(env, "unable to get reference to listener");

	fz_try(ctx)
		pdf_set_doc_event_callback(ctx, pdf, event_cb, free_event_cb_data, jlistener);
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

JNIEXPORT void JNICALL
FUN(PDFDocument_saveJournalWithStream)(JNIEnv *env, jobject self, jobject jstream)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	SeekableStreamState *state = NULL;
	jobject stream = NULL;
	jbyteArray array = NULL;
	fz_output *out = NULL;

	if (!ctx || !pdf)
		return;
	if (!jstream)
		jni_throw_arg_void(env, "stream must not be null");

	fz_var(state);
	fz_var(out);
	fz_var(stream);
	fz_var(array);

	stream = (*env)->NewGlobalRef(env, jstream);
	if (!stream)
		jni_throw_run_void(env, "invalid stream");

	array = (*env)->NewByteArray(env, sizeof state->buffer);
	if ((*env)->ExceptionCheck(env))
	{
		(*env)->DeleteGlobalRef(env, stream);
		return;
	}
	if (!array)
	{
		(*env)->DeleteGlobalRef(env, stream);
		jni_throw_run_void(env, "cannot create byte array");
	}

	array = (*env)->NewGlobalRef(env, array);
	if (!array)
	{
		(*env)->DeleteGlobalRef(env, stream);
		jni_throw_run_void(env, "cannot create global reference");
	}

	fz_try(ctx)
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
		out->truncate = SeekableOutputStream_truncate;
		out->as_stream = SeekableOutputStream_as_stream;

		/* these are now owned by 'out' */
		state = NULL;

		pdf_write_journal(ctx, pdf, out);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		fz_drop_output(ctx, out);
	}
	fz_catch(ctx)
	{
		(*env)->DeleteGlobalRef(env, array);
		(*env)->DeleteGlobalRef(env, stream);
		jni_rethrow_void(env, ctx);
	}
}

JNIEXPORT void JNICALL
FUN(PDFDocument_loadJournalWithStream)(JNIEnv *env, jobject self, jobject jstream)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	SeekableStreamState *state = NULL;
	jobject stream = NULL;
	jbyteArray array = NULL;
	fz_stream *stm = NULL;

	if (!ctx || !pdf)
		return;
	if (!jstream)
		jni_throw_arg_void(env, "stream must not be null");

	fz_var(state);
	fz_var(stm);
	fz_var(stream);
	fz_var(array);

	stream = (*env)->NewGlobalRef(env, jstream);
	if (!stream)
		jni_throw_run_void(env, "invalid stream");

	array = (*env)->NewByteArray(env, sizeof state->buffer);
	if ((*env)->ExceptionCheck(env))
	{
		(*env)->DeleteGlobalRef(env, stream);
		return;
	}
	if (!array)
	{
		(*env)->DeleteGlobalRef(env, stream);
		jni_throw_run_void(env, "cannot create byte array");
	}

	array = (*env)->NewGlobalRef(env, array);
	if (!array)
	{
		(*env)->DeleteGlobalRef(env, stream);
		jni_throw_run_void(env, "cannot create global reference");
	}

	fz_try(ctx)
	{
		/* No exceptions can occur from here to stream owning state, so we must not free state. */
		state = Memento_label(fz_malloc(ctx, sizeof(SeekableStreamState)), "SeekableStreamState_state");
		state->stream = stream;
		state->array = array;

		/* Ownership transferred to state. */
		stream = NULL;
		array = NULL;

		/* Stream takes ownership of accstate. */
		stm = fz_new_stream(ctx, state, SeekableInputStream_next, SeekableInputStream_drop);
		stm->seek = SeekableInputStream_seek;

		pdf_read_journal(ctx, pdf, stm);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, stm);
	}
	fz_catch(ctx)
	{
		(*env)->DeleteGlobalRef(env, array);
		(*env)->DeleteGlobalRef(env, stream);
		jni_rethrow_void(env, ctx);
	}
}

JNIEXPORT void JNICALL
FUN(PDFDocument_saveJournal)(JNIEnv *env, jobject self, jstring jfilename)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	const char *filename = NULL;

	if (!ctx || !pdf)
		return;
	if (!jfilename)
		jni_throw_arg_void(env, "filename must not be null");

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename)
		jni_throw_run_void(env, "cannot get characters in filename");

	fz_try(ctx)
		pdf_save_journal(ctx, pdf, filename);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_loadJournal)(JNIEnv *env, jobject self, jstring jfilename)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	const char *filename = NULL;

	if (!ctx || !pdf)
		return;
	if (!jfilename)
		jni_throw_arg_void(env, "filename must not be null");

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename)
		jni_throw_run_void(env, "cannot get characters in filename");

	fz_try(ctx)
		pdf_load_journal(ctx, pdf, filename);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
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
	jboolean can = JNI_FALSE;

	if (!ctx || !pdf) return JNI_FALSE;

	fz_try(ctx)
		can = pdf_can_undo(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return can;
}

JNIEXPORT jboolean JNICALL
FUN(PDFDocument_canRedo)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	jboolean can = JNI_FALSE;

	if (!ctx || !pdf) return JNI_FALSE;

	fz_try(ctx)
		can = pdf_can_redo(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return can;
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
FUN(PDFDocument_beginImplicitOperation)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);

	if (!ctx || !pdf) return;

	fz_try(ctx)
		pdf_begin_implicit_operation(ctx, pdf);
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

JNIEXPORT void JNICALL
FUN(PDFDocument_abandonOperation)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);

	if (!ctx || !pdf) return;

	fz_try(ctx)
		pdf_abandon_operation(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFDocument_isRedacted)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);

	if (!ctx || !pdf) return JNI_FALSE;
	return pdf->redacted;
}

JNIEXPORT jint JNICALL
FUN(PDFDocument_getLanguage)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	int lang;

	if (!ctx || !pdf) return FZ_LANG_UNSET;

	fz_try(ctx)
		lang = pdf_document_language(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return lang;
}

JNIEXPORT void JNICALL
FUN(PDFDocument_setLanguage)(JNIEnv *env, jobject self, jint lang)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);

	if (!ctx || !pdf) return;

	fz_try(ctx)
		pdf_set_document_language(ctx, pdf, lang);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(PDFDocument_countSignatures)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);
	jint val = -1;

	if (!ctx || !pdf) return -1;

	fz_try(ctx)
		val = pdf_count_signatures(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return val;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addEmbeddedFile)(JNIEnv *env, jobject self, jstring jfilename, jstring jmimetype, jobject jcontents, jlong created, jlong modified, jboolean addChecksum)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);
	const char *filename = NULL;
	const char *mimetype = NULL;
	fz_buffer *contents = from_Buffer(env, jcontents);
	pdf_obj *fs = NULL;

	if (!ctx || !pdf) return NULL;
	if (!jfilename) jni_throw_arg(env, "filename must not be null");

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return NULL;

	if (jmimetype)
	{
		mimetype = (*env)->GetStringUTFChars(env, jmimetype, NULL);
		if (!mimetype)
		{
			(*env)->ReleaseStringUTFChars(env, jfilename, filename);
			return NULL;
		}
	}

	fz_try(ctx)
		fs = pdf_add_embedded_file(ctx, pdf, filename, mimetype, contents, created, modified, addChecksum);
	fz_always(ctx)
	{
		if (mimetype)
			(*env)->ReleaseStringUTFChars(env, jmimetype, mimetype);
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe(ctx, env, fs);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_getFilespecParams)(JNIEnv *env, jobject self, jobject jfs)
{
	fz_context *ctx = get_context(env);
	pdf_obj *fs = from_PDFObject_safe(env, jfs);
	pdf_filespec_params params;
	jstring jfilename = NULL;
	jstring jmimetype = NULL;

	fz_try(ctx)
		pdf_get_filespec_params(ctx, fs, &params);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jfilename = (*env)->NewStringUTF(env, params.filename);
	if (!jfilename || (*env)->ExceptionCheck(env))
		return NULL;

	jmimetype = (*env)->NewStringUTF(env, params.mimetype);
	if (!jmimetype || (*env)->ExceptionCheck(env))
		return NULL;

	return (*env)->NewObject(env, cls_PDFDocument_PDFEmbeddedFileParams, mid_PDFDocument_PDFEmbeddedFileParams_init,
		jfilename, jmimetype, params.size, params.created * 1000, params.modified * 1000);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_loadEmbeddedFileContents)(JNIEnv *env, jobject self, jobject jfs)
{
	fz_context *ctx = get_context(env);
	pdf_obj *fs = from_PDFObject_safe(env, jfs);
	fz_buffer *contents = NULL;

	fz_try(ctx)
		contents = pdf_load_embedded_file_contents(ctx, fs);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Buffer_safe(ctx, env, contents);
}

JNIEXPORT jboolean JNICALL
FUN(PDFDocument_verifyEmbeddedFileChecksum)(JNIEnv *env, jobject self, jobject jfs)
{
	fz_context *ctx = get_context(env);
	pdf_obj *fs = from_PDFObject_safe(env, jfs);
	int valid = 0;

	fz_try(ctx)
		valid = pdf_verify_embedded_file_checksum(ctx, fs);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return valid ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(PDFDocument_isEmbeddedFile)(JNIEnv *env, jobject self, jobject jfs)
{
	fz_context *ctx = get_context(env);
	pdf_obj *fs = from_PDFObject_safe(env, jfs);
	int embedded = 0;

	fz_try(ctx)
		embedded = pdf_is_embedded_file(ctx, fs);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return embedded ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
FUN(PDFDocument_setPageLabels)(JNIEnv *env, jobject self, jint index, jint style, jstring jprefix, jint start)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);
	const char *prefix = NULL;

	if (jprefix)
	{
		prefix = (*env)->GetStringUTFChars(env, jprefix, NULL);
		if (!prefix) return;
	}

	fz_try(ctx)
	{
		pdf_set_page_labels(ctx, pdf, index, style, prefix, start);
	}
	fz_always(ctx)
	{
		if (jprefix)
			(*env)->ReleaseStringUTFChars(env, jprefix, prefix);
	}
	fz_catch(ctx)
	{
		jni_rethrow_void(env, ctx);
	}
}

JNIEXPORT void JNICALL
FUN(PDFDocument_deletePageLabels)(JNIEnv *env, jobject self, jint index)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);
	fz_try(ctx)
		pdf_delete_page_labels(ctx, pdf, index);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(PDFDocument_getVersion)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	int version = 0;

	if (!ctx || !pdf) return 0;

	fz_try(ctx)
		version = pdf_version(ctx, pdf);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return version;
}

JNIEXPORT jstring JNICALL
FUN(PDFDocument_formatURIFromPathAndNamedDest)(JNIEnv *env, jclass cls, jstring jpath, jstring jname)
{
	fz_context *ctx = get_context(env);
	char *uri = NULL;
	jobject juri;
	const char *path = NULL;
	const char *name = NULL;

	if (jpath)
	{
		path = (*env)->GetStringUTFChars(env, jpath, NULL);
		if (!path) return NULL;
	}
	if (jname)
	{
		name = (*env)->GetStringUTFChars(env, jname, NULL);
		if (!name) return NULL;
	}

	fz_try(ctx)
		uri = pdf_new_uri_from_path_and_named_dest(ctx, path, name);
	fz_always(ctx)
	{
		if (jname)
			(*env)->ReleaseStringUTFChars(env, jname, name);
		if (jpath)
			(*env)->ReleaseStringUTFChars(env, jpath, path);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	juri = (*env)->NewStringUTF(env, uri);
	fz_free(ctx, uri);
	return juri;
}

JNIEXPORT jstring JNICALL
FUN(PDFDocument_formatURIFromPathAndExplicitDest)(JNIEnv *env, jclass cls, jstring jpath, jobject jdest)
{
	fz_context *ctx = get_context(env);
	fz_link_dest dest = from_LinkDestination(env, jdest);
	char *uri = NULL;
	jobject juri;
	const char *path = NULL;

	if (jpath)
	{
		path = (*env)->GetStringUTFChars(env, jpath, NULL);
		if (!path) return NULL;
	}

	fz_try(ctx)
		uri = pdf_new_uri_from_path_and_explicit_dest(ctx, path, dest);
	fz_always(ctx)
	{
		if (jpath)
			(*env)->ReleaseStringUTFChars(env, jpath, path);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	juri = (*env)->NewStringUTF(env, uri);
	fz_free(ctx, uri);
	return juri;
}

JNIEXPORT jstring JNICALL
FUN(PDFDocument_appendNamedDestToURI)(JNIEnv *env, jclass cls, jstring jurl, jstring jname)
{
	fz_context *ctx = get_context(env);
	char *uri = NULL;
	jobject juri;
	const char *url = NULL;
	const char *name = NULL;

	if (jurl)
	{
		url = (*env)->GetStringUTFChars(env, jurl, NULL);
		if (!url) return NULL;
	}
	if (jname)
	{
		name = (*env)->GetStringUTFChars(env, jname, NULL);
		if (!name) return NULL;
	}

	fz_try(ctx)
		uri = pdf_append_named_dest_to_uri(ctx, url, name);
	fz_always(ctx)
	{
		if (jname)
			(*env)->ReleaseStringUTFChars(env, jname, name);
		if (jurl)
			(*env)->ReleaseStringUTFChars(env, jurl, url);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	juri = (*env)->NewStringUTF(env, uri);
	fz_free(ctx, uri);
	return juri;
}

JNIEXPORT jstring JNICALL
FUN(PDFDocument_appendExplicitDestToURI)(JNIEnv *env, jclass cls, jstring jurl, jobject jdest)
{
	fz_context *ctx = get_context(env);
	fz_link_dest dest = from_LinkDestination(env, jdest);
	char *uri = NULL;
	jobject juri;
	const char *url = NULL;

	if (jurl)
	{
		url = (*env)->GetStringUTFChars(env, jurl, NULL);
		if (!url) return NULL;
	}

	fz_try(ctx)
		uri = pdf_append_explicit_dest_to_uri(ctx, url, dest);
	fz_always(ctx)
	{
		if (jurl)
			(*env)->ReleaseStringUTFChars(env, jurl, url);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	juri = (*env)->NewStringUTF(env, uri);
	fz_free(ctx, uri);
	return juri;
}

JNIEXPORT void JNICALL
FUN(PDFDocument_rearrangePages)(JNIEnv *env, jobject self, jobject jpages)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	jsize len = 0;
	int *pages = NULL;

	if (!ctx || !pdf) return;

	len = (*env)->GetArrayLength(env, jpages);
	fz_try(ctx)
		pages = fz_malloc_array(ctx, len, int);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);

	(*env)->GetIntArrayRegion(env, jpages, 0, len, pages);
	if ((*env)->ExceptionCheck(env))
	{
		fz_free(ctx, pages);
		return;
	}

	fz_try(ctx)
		pdf_rearrange_pages(ctx, pdf, len, pages, PDF_CLEAN_STRUCTURE_DROP);
	fz_always(ctx)
		fz_free(ctx, pages);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(PDFDocument_countAssociatedFiles)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);
	int n;

	if (!ctx) return 0;

	fz_try(ctx)
		n = pdf_count_document_associated_files(ctx, doc);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return n;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_associatedFile)(JNIEnv *env, jobject self, jint idx)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);
	pdf_obj *af;

	if (!ctx) return NULL;

	fz_try(ctx)
		af = pdf_document_associated_file(ctx, doc, idx);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe_own(ctx, env, af);
}

JNIEXPORT jint JNICALL
FUN(PDFDocument_zugferdProfile)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);
	enum pdf_zugferd_profile p;
	float version;

	if (!ctx) return 0;

	fz_try(ctx)
		p = pdf_zugferd_profile(ctx, doc, &version);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (int)p;
}

JNIEXPORT jfloat JNICALL
FUN(PDFDocument_zugferdVersion)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);
	float version;

	if (!ctx) return 0;

	fz_try(ctx)
		(void) pdf_zugferd_profile(ctx, doc, &version);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (jfloat)version;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_zugferdXML)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);
	fz_buffer *buf;

	if (!ctx) return NULL;

	fz_try(ctx)
		buf = pdf_zugferd_xml(ctx, doc);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Buffer_safe_own(ctx, env, buf);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_loadImage)(JNIEnv *env, jobject self, jobject jobj)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);
	pdf_obj *obj = from_PDFObject(env, jobj);
	fz_image *img;

	if (!ctx) return NULL;

	fz_try(ctx)
		img = pdf_load_image(ctx, doc, obj);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Image_safe_own(ctx, env, img);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_lookupDest)(JNIEnv *env, jobject self, jobject jdest)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);
	pdf_obj *dest = from_PDFObject(env, jdest);
	pdf_obj *obj = NULL;

	if (!ctx) return NULL;

	fz_try(ctx)
		obj = pdf_lookup_dest(ctx, doc, dest);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe_own(ctx, env, obj);
}

JNIEXPORT jint JNICALL
FUN(PDFDocument_countLayerConfigs)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);
	jint configs = 0;

	fz_try(ctx)
		configs = pdf_count_layer_configs(ctx, doc);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return configs;
}

JNIEXPORT jstring JNICALL
FUN(PDFDocument_getLayerConfigName)(JNIEnv *env, jobject self, jint config)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);
	const char *name = NULL;

	fz_try(ctx)
		name = pdf_layer_config_name(ctx, doc, config);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewStringUTF(env, name);
}

JNIEXPORT jstring JNICALL
FUN(PDFDocument_getLayerConfigCreator)(JNIEnv *env, jobject self, jint config)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);
	const char *creator = NULL;

	fz_try(ctx)
		creator = pdf_layer_config_creator(ctx, doc, config);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewStringUTF(env, creator);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_selectLayerConfig)(JNIEnv *env, jobject self, jint config)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);

	fz_try(ctx)
		pdf_select_layer_config(ctx, doc, config);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(PDFDocument_countLayerConfigUIs)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);
	jint configuis = 0;

	fz_try(ctx)
		configuis = pdf_count_layer_config_ui(ctx, doc);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return configuis;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_getLayerConfigUIInfo)(JNIEnv *env, jobject self, jint configui)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);
	pdf_layer_config_ui info = { 0 };
	jobject jinfo = NULL;
	jobject jtext = NULL;

	fz_try(ctx)
		pdf_layer_config_ui_info(ctx, doc, configui, &info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jtext = (*env)->NewStringUTF(env, info.text);
	if (!jtext || (*env)->ExceptionCheck(env))
		return NULL;

	jinfo = (*env)->NewObject(env, cls_PDFDocument_LayerConfigUIInfo, mid_PDFDocument_LayerConfigUIInfo_init);
	if (!jinfo || (*env)->ExceptionCheck(env))
		return NULL;

	(*env)->SetIntField(env, jinfo, fid_PDFDocument_LayerConfigUIInfo_type, info.type);
	(*env)->SetIntField(env, jinfo, fid_PDFDocument_LayerConfigUIInfo_depth, info.depth);
	(*env)->SetBooleanField(env, jinfo, fid_PDFDocument_LayerConfigUIInfo_selected, info.selected);
	(*env)->SetBooleanField(env, jinfo, fid_PDFDocument_LayerConfigUIInfo_locked, info.locked);
	(*env)->SetObjectField(env, jinfo, fid_PDFDocument_LayerConfigUIInfo_text, jtext);

	return jinfo;
}

JNIEXPORT jint JNICALL
FUN(PDFDocument_countLayers)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);
	jint layers = 0;

	fz_try(ctx)
		layers = pdf_count_layers(ctx, doc);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return layers;
}

JNIEXPORT jboolean JNICALL
FUN(PDFDocument_isLayerVisible)(JNIEnv *env, jobject self, jint layer)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);
	jboolean visible = JNI_FALSE;

	fz_try(ctx)
		visible = pdf_layer_is_enabled(ctx, doc, layer);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return visible;
}

JNIEXPORT void JNICALL
FUN(PDFDocument_setLayerVisible)(JNIEnv *env, jobject self, jint layer, jboolean visible)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);

	fz_try(ctx)
		pdf_enable_layer(ctx, doc, layer, visible);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jstring JNICALL
FUN(PDFDocument_getLayerName)(JNIEnv *env, jobject self, jint layer)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);
	const char *name = NULL;

	fz_try(ctx)
		name = pdf_layer_name(ctx, doc, layer);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewStringUTF(env, name);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_subsetFonts)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);

	fz_try(ctx)
		pdf_subset_fonts(ctx, doc, 0, NULL);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_bake)(JNIEnv *env, jobject self, jboolean bake_annots, jboolean bake_widgets)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = from_PDFDocument(env, self);

	fz_try(ctx)
		pdf_bake_document(ctx, doc, bake_annots, bake_widgets);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}
