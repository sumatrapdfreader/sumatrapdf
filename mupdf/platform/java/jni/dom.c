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

/* DOM interface */

JNIEXPORT void JNICALL
FUN(DOM_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_xml *xml = from_DOM_safe(env, self);
	if (!ctx || !xml) return;
	(*env)->SetLongField(env, self, fid_DOM_pointer, 0);
	fz_drop_xml(ctx, xml);
}

JNIEXPORT void JNICALL
FUN(DOM_insertBefore)(JNIEnv *env, jobject self, jobject jxml)
{
	fz_context *ctx = get_context(env);
	fz_xml *me = from_DOM_safe(env, self);
	fz_xml *xml = from_DOM_safe(env, jxml);

	fz_try(ctx)
		fz_dom_insert_before(ctx, me, xml);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(DOM_insertAfter)(JNIEnv *env, jobject self, jobject jxml)
{
	fz_context *ctx = get_context(env);
	fz_xml *me = from_DOM_safe(env, self);
	fz_xml *xml = from_DOM_safe(env, jxml);

	fz_try(ctx)
		fz_dom_insert_after(ctx, me, xml);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(DOM_appendChild)(JNIEnv *env, jobject self, jobject jxml)
{
	fz_context *ctx = get_context(env);
	fz_xml *me = from_DOM_safe(env, self);
	fz_xml *xml = from_DOM_safe(env, jxml);

	fz_try(ctx)
		fz_dom_append_child(ctx, me, xml);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(DOM_remove)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_xml *me = from_DOM_safe(env, self);

	fz_try(ctx)
		fz_dom_remove(ctx, me);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(DOM_clone)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_xml *me = from_DOM_safe(env, self);
	fz_xml *clone = NULL;

	fz_var(clone);

	fz_try(ctx)
		clone = fz_dom_clone(ctx, me);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_DOM_safe(ctx, env, clone);
}

JNIEXPORT jobject JNICALL
FUN(DOM_parent)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_xml *me = from_DOM_safe(env, self);
	fz_xml *parent = NULL;

	fz_var(parent);

	fz_try(ctx)
		parent = fz_dom_clone(ctx, me);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_DOM_safe(ctx, env, parent);
}

JNIEXPORT jobject JNICALL
FUN(DOM_firstChild)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_xml *me = from_DOM_safe(env, self);
	fz_xml *child = NULL;

	fz_var(child);

	fz_try(ctx)
		child = fz_dom_first_child(ctx, me);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_DOM_safe(ctx, env, child);
}

JNIEXPORT jobject JNICALL
FUN(DOM_next)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_xml *me = from_DOM_safe(env, self);
	fz_xml *next = NULL;

	fz_var(next);

	fz_try(ctx)
		next = fz_dom_next(ctx, me);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_DOM_safe(ctx, env, next);
}

JNIEXPORT jobject JNICALL
FUN(DOM_previous)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_xml *me = from_DOM_safe(env, self);
	fz_xml *prev = NULL;

	fz_var(prev);

	fz_try(ctx)
		prev = fz_dom_previous(ctx, me);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_DOM_safe(ctx, env, prev);
}

JNIEXPORT jobject JNICALL
FUN(DOM_removeAttribute)(JNIEnv *env, jobject self, jstring jatt)
{
	fz_context *ctx = get_context(env);
	fz_xml *dom = from_DOM_safe(env, self);
	const char *att = NULL;

	if (!jatt)
		return NULL;

	att = (*env)->GetStringUTFChars(env, jatt, NULL);
	if (!att) jni_throw_run(env, "cannot get characters in attribute name");

	fz_try(ctx)
		fz_dom_remove_attribute(ctx, dom, att);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jatt, att);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return self;
}

JNIEXPORT jstring JNICALL
FUN(DOM_attribute)(JNIEnv *env, jobject self, jstring jatt)
{
	fz_context *ctx = get_context(env);
	fz_xml *dom = from_DOM_safe(env, self);
	const char *att = NULL;
	const char *val = NULL;

	if (!jatt)
		return NULL;

	att = (*env)->GetStringUTFChars(env, jatt, NULL);
	if (!att) jni_throw_run(env, "cannot get characters in attribute name");

	fz_try(ctx)
		val = fz_dom_attribute(ctx, dom, att);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jatt, att);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_String_safe(ctx, env, val);
}

JNIEXPORT jobjectArray JNICALL
FUN(DOM_attributes)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_xml *dom = from_DOM_safe(env, self);
	const char *att = NULL;
	const char *val = NULL;
	jobjectArray jarr;
	jstring jatt, jval;
	jobject jobj;
	size_t i;
	int n;

	fz_try(ctx)
	{
		n = 0;
		while (1)
		{
			val = fz_dom_get_attribute(ctx, dom, n, &att);
			if (att == NULL)
				break;
			n++;
		}
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jarr = (*env)->NewObjectArray(env, n, cls_DOMAttribute, NULL);
	if (!jarr) jni_throw_run(env, "cannot create attribute array");

	for (i = 0; i < (size_t)n; i++)
	{
		fz_try(ctx)
			val = fz_dom_get_attribute(ctx, dom, i, &att);
		fz_catch(ctx)
			jni_rethrow(env, ctx);

		jobj = (*env)->NewObject(env, cls_DOMAttribute, mid_DOMAttribute_init);
		if (!jobj) jni_throw_run(env, "cannot create DOMAttribute");
		jatt = (*env)->NewStringUTF(env, att);
		if (!jatt) jni_throw_run(env, "cannot create String from attribute");
		if (val == NULL)
			jval = NULL;
		else {
			jval = (*env)->NewStringUTF(env, val);
			if (!jval) jni_throw_run(env, "cannot create String from attribute");
		}
		(*env)->SetObjectField(env, jobj, fid_DOMAttribute_attribute, jatt);
		(*env)->SetObjectField(env, jobj, fid_DOMAttribute_value, jval);
		(*env)->SetObjectArrayElement(env, jarr, i, jobj);
		if ((*env)->ExceptionCheck(env))
			return NULL;
	}

	return jarr;
}

JNIEXPORT jobject JNICALL
FUN(DOM_find)(JNIEnv *env, jobject self, jstring jtag, jstring jatt, jstring jval)
{
	fz_context *ctx = get_context(env);
	fz_xml *me = from_DOM_safe(env, self);
	const char *tag = NULL;
	const char *att = NULL;
	const char *val = NULL;
	fz_xml *xml;

	if (jtag)
	{
		tag = (*env)->GetStringUTFChars(env, jtag, NULL);
		if (!tag) jni_throw_run(env, "cannot get characters in attribute name");
	}
	if (jatt)
	{
		att = (*env)->GetStringUTFChars(env, jatt, NULL);
		if (!att) jni_throw_run(env, "cannot get characters in attribute name");
	}
	if (jval)
	{
		val = (*env)->GetStringUTFChars(env, jval, NULL);
		if (!val) jni_throw_run(env, "cannot get characters in attribute name");
	}

	fz_try(ctx)
		xml = fz_dom_find(ctx, me, tag, att, val);
	fz_always(ctx)
	{
		(*env)->ReleaseStringUTFChars(env, jtag, tag);
		(*env)->ReleaseStringUTFChars(env, jatt, att);
		(*env)->ReleaseStringUTFChars(env, jval, val);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_DOM_safe(ctx, env, xml);
}

JNIEXPORT jobject JNICALL
FUN(DOM_findNext)(JNIEnv *env, jobject self, jstring jtag, jstring jatt, jstring jval)
{
	fz_context *ctx = get_context(env);
	fz_xml *me = from_DOM_safe(env, self);
	const char *tag = NULL;
	const char *att = NULL;
	const char *val = NULL;
	fz_xml *xml;

	if (jtag)
	{
		tag = (*env)->GetStringUTFChars(env, jtag, NULL);
		if (!tag) jni_throw_run(env, "cannot get characters in attribute name");
	}
	if (jatt)
	{
		att = (*env)->GetStringUTFChars(env, jatt, NULL);
		if (!att) jni_throw_run(env, "cannot get characters in attribute name");
	}
	if (jval)
	{
		val = (*env)->GetStringUTFChars(env, jval, NULL);
		if (!val) jni_throw_run(env, "cannot get characters in attribute name");
	}

	fz_try(ctx)
		xml = fz_dom_find_next(ctx, me, tag, att, val);
	fz_always(ctx)
	{
		(*env)->ReleaseStringUTFChars(env, jtag, tag);
		(*env)->ReleaseStringUTFChars(env, jatt, att);
		(*env)->ReleaseStringUTFChars(env, jval, val);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_DOM_safe(ctx, env, xml);
}

JNIEXPORT jobject JNICALL
FUN(DOM_body)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_xml *dom = from_DOM_safe(env, self);

	return to_DOM_safe(ctx, env, fz_dom_body(ctx, dom));
}

JNIEXPORT jobject JNICALL
FUN(DOM_document)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_xml *dom = from_DOM_safe(env, self);

	return to_DOM_safe(ctx, env, fz_dom_document_element(ctx, dom));
}

JNIEXPORT jobject JNICALL
FUN(DOM_createTextNode)(JNIEnv *env, jobject self, jstring jtext)
{
	fz_context *ctx = get_context(env);
	fz_xml *dom = from_DOM_safe(env, self);
	const char *text = NULL;
	fz_xml *elt;

	if (!jtext)
		return NULL;

	text = (*env)->GetStringUTFChars(env, jtext, NULL);
	if (!text) jni_throw_run(env, "cannot get characters in text string");

	fz_try(ctx)
		elt = fz_dom_create_text_node(ctx, dom, text);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jtext, text);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_DOM_safe(ctx, env, elt);
}

JNIEXPORT jobject JNICALL
FUN(DOM_createElement)(JNIEnv *env, jobject self, jstring jtag)
{
	fz_context *ctx = get_context(env);
	fz_xml *dom = from_DOM_safe(env, self);
	const char *tag = NULL;
	fz_xml *elt;

	if (!jtag)
		return NULL;

	tag = (*env)->GetStringUTFChars(env, jtag, NULL);
	if (!tag) jni_throw_run(env, "cannot get characters in tag");

	fz_try(ctx)
		elt = fz_dom_create_element(ctx, dom, tag);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jtag, tag);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_DOM_safe(ctx, env, elt);
}
