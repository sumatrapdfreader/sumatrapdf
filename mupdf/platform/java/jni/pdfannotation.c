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

/* Annotation interface */

JNIEXPORT void JNICALL
FUN(PDFAnnotation_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation_safe(env, self);
	if (!ctx || !annot) return;
	(*env)->SetLongField(env, self, fid_PDFAnnotation_pointer, 0);
	pdf_drop_annot(ctx, annot);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_run)(JNIEnv *env, jobject self, jobject jdev, jobject jctm, jobject jcookie)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_device *dev = from_Device(env, jdev);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_cookie *cookie= from_Cookie(env, jcookie);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !annot) return;
	if (!dev) jni_throw_arg_void(env, "device must not be null");

	info = lockNativeDevice(env, jdev, &err);
	if (err)
		return;
	fz_try(ctx)
		pdf_run_annot(ctx, annot, dev, ctm, cookie);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_toPixmap)(JNIEnv *env, jobject self, jobject jctm, jobject jcs, jboolean alpha)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_pixmap *pixmap = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		pixmap = pdf_new_pixmap_from_annot(ctx, annot, ctm, cs, NULL, alpha);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Pixmap_safe_own(ctx, env, pixmap);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getBounds)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_rect rect;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		rect = pdf_bound_annot(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Rect_safe(ctx, env, rect);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_toDisplayList)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_display_list *list = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		list = pdf_new_display_list_from_annot(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_DisplayList_safe_own(ctx, env, list);
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getType)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jint type = 0;

	if (!ctx || !annot) return 0;

	fz_try(ctx)
		type = pdf_annot_type(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return type;
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getFlags)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jint flags = 0;

	if (!ctx || !annot) return 0;

	fz_try(ctx)
		flags = pdf_annot_flags(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return flags;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setFlags)(JNIEnv *env, jobject self, jint flags)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_flags(ctx, annot, flags);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jstring JNICALL
FUN(PDFAnnotation_getContents)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *contents = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		contents = pdf_annot_contents(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewStringUTF(env, contents);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setContents)(JNIEnv *env, jobject self, jstring jcontents)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *contents = "";

	if (!ctx || !annot) return;
	if (jcontents)
	{
		contents = (*env)->GetStringUTFChars(env, jcontents, NULL);
		if (!contents) return;
	}

	fz_try(ctx)
		pdf_set_annot_contents(ctx, annot, contents);
	fz_always(ctx)
		if (contents)
			(*env)->ReleaseStringUTFChars(env, jcontents, contents);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jstring JNICALL
FUN(PDFAnnotation_getAuthor)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *author = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		author = pdf_annot_author(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewStringUTF(env, author);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setAuthor)(JNIEnv *env, jobject self, jstring jauthor)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *author = "";

	if (!ctx || !annot) return;
	if (jauthor)
	{
		author = (*env)->GetStringUTFChars(env, jauthor, NULL);
		if (!author) return;
	}

	fz_try(ctx)
		pdf_set_annot_author(ctx, annot, author);
	fz_always(ctx)
		if (author)
			(*env)->ReleaseStringUTFChars(env, jauthor, author);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jlong JNICALL
FUN(PDFAnnotation_getModificationDateNative)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jlong t;

	if (!ctx || !annot) return -1;

	fz_try(ctx)
		t = pdf_annot_modification_date(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return t * 1000;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setModificationDate)(JNIEnv *env, jobject self, jlong time)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_set_annot_modification_date(ctx, annot, time / 1000);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jlong JNICALL
FUN(PDFAnnotation_getCreationDateNative)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jlong t;

	if (!ctx || !annot) return -1;

	fz_try(ctx)
		t = pdf_annot_creation_date(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return t * 1000;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setCreationDate)(JNIEnv *env, jobject self, jlong time)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_set_annot_creation_date(ctx, annot, time / 1000);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getRect)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_rect rect;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		rect = pdf_annot_rect(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Rect_safe(ctx, env, rect);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setRect)(JNIEnv *env, jobject self, jobject jrect)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_rect rect = from_Rect(env, jrect);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_rect(ctx, annot, rect);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getColor)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n;
	float color[4];
	jfloatArray arr;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		pdf_annot_color(ctx, annot, &n, color);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	arr = (*env)->NewFloatArray(env, n);
	if (!arr || (*env)->ExceptionCheck(env)) return NULL;

	(*env)->SetFloatArrayRegion(env, arr, 0, n, &color[0]);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return arr;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setColor)(JNIEnv *env, jobject self, jobject jcolor)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	float color[4];
	int n = 0;

	if (!ctx || !annot) return;
	if (!from_jfloatArray(env, color, nelem(color), jcolor)) return;
	if (jcolor)
		n = (*env)->GetArrayLength(env, jcolor);

	fz_try(ctx)
		pdf_set_annot_color(ctx, annot, n, color);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getInteriorColor)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n;
	float color[4];
	jfloatArray arr;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		pdf_annot_interior_color(ctx, annot, &n, color);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	arr = (*env)->NewFloatArray(env, n);
	if (!arr || (*env)->ExceptionCheck(env)) return NULL;

	(*env)->SetFloatArrayRegion(env, arr, 0, n, &color[0]);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return arr;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setInteriorColor)(JNIEnv *env, jobject self, jobject jcolor)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	float color[4];
	int n = 0;

	if (!ctx || !annot) return;
	if (!from_jfloatArray(env, color, nelem(color), jcolor)) return;
	if (jcolor)
		n = (*env)->GetArrayLength(env, jcolor);

	fz_try(ctx)
		pdf_set_annot_interior_color(ctx, annot, n, color);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jfloat JNICALL
FUN(PDFAnnotation_getOpacity)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jfloat opacity;

	if (!ctx || !annot) return 0.0f;

	fz_try(ctx)
		opacity = pdf_annot_opacity(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return opacity;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setOpacity)(JNIEnv *env, jobject self, jfloat opacity)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_opacity(ctx, annot, opacity);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getQuadPointCount)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n = 0;

	fz_try(ctx)
		n = pdf_annot_quad_point_count(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return n;
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getQuadPoint)(JNIEnv *env, jobject self, jint i)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_quad q;

	fz_try(ctx)
		q = pdf_annot_quad_point(ctx, annot, i);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Quad_safe(ctx, env, q);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_clearQuadPoints)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_clear_annot_quad_points(ctx, annot);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_addQuadPoint)(JNIEnv *env, jobject self, jobject qobj)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_quad q = from_Quad(env, qobj);

	fz_try(ctx)
		pdf_add_annot_quad_point(ctx, annot, q);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getVertexCount)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n = 0;

	fz_try(ctx)
		n = pdf_annot_vertex_count(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return n;
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getVertex)(JNIEnv *env, jobject self, jint i)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_point v;

	fz_try(ctx)
		v = pdf_annot_vertex(ctx, annot, i);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Point_safe(ctx, env, v);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_clearVertices)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_clear_annot_vertices(ctx, annot);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_addVertex)(JNIEnv *env, jobject self, float x, float y)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_add_annot_vertex(ctx, annot, fz_make_point(x, y));
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getInkListCount)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n = 0;

	fz_try(ctx)
		n = pdf_annot_ink_list_count(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return n;
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getInkListStrokeCount)(JNIEnv *env, jobject self, jint i)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n = 0;

	fz_try(ctx)
		n = pdf_annot_ink_list_stroke_count(ctx, annot, i);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return n;
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getInkListStrokeVertex)(JNIEnv *env, jobject self, jint i, jint k)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_point v;

	fz_try(ctx)
		v = pdf_annot_ink_list_stroke_vertex(ctx, annot, i, k);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Point_safe(ctx, env, v);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_clearInkList)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_clear_annot_ink_list(ctx, annot);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_addInkListStroke)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_add_annot_ink_list_stroke(ctx, annot);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_addInkListStrokeVertex)(JNIEnv *env, jobject self, float x, float y)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_add_annot_ink_list_stroke_vertex(ctx, annot, fz_make_point(x, y));
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasCallout)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_callout(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getCalloutStyle)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	enum pdf_line_ending s = 0;

	fz_try(ctx)
		s = pdf_annot_callout_style(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return s;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setCalloutStyle)(JNIEnv *env, jobject self, jint s)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_try(ctx)
		pdf_set_annot_callout_style(ctx, annot, s);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getCalloutPoint)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_point p;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		p = pdf_annot_callout_point(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewObject(env, cls_Point, mid_Point_init, p.x, p.y);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setCalloutPoint)(JNIEnv *env, jobject self, jobject jp)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_point p;

	if (!ctx || !annot) return;

	if (!jp) jni_throw_arg_void(env, "point must not be null");

	p.x = (*env)->GetFloatField(env, jp, fid_Point_x);
	p.y = (*env)->GetFloatField(env, jp, fid_Point_y);

	fz_try(ctx)
		pdf_set_annot_callout_point(ctx, annot, p);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getCalloutLine)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_point points[3] = { 0 };
	jobject jline = NULL;
	jobject jpoint = NULL;
	int i, n;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		pdf_annot_callout_line(ctx, annot, points, &n);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	if (n == 0)
		return NULL;

	jline = (*env)->NewObjectArray(env, n, cls_Point, NULL);
	if (!jline || (*env)->ExceptionCheck(env)) return NULL;

	for (i = 0; i < n; i++)
	{
		jpoint = (*env)->NewObject(env, cls_Point, mid_Point_init, points[i].x, points[i].y);
		if (!jpoint || (*env)->ExceptionCheck(env)) return NULL;
		(*env)->SetObjectArrayElement(env, jline, i, jpoint);
		if ((*env)->ExceptionCheck(env)) return NULL;
		(*env)->DeleteLocalRef(env, jpoint);
	}

	return jline;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setCalloutLineNative)(JNIEnv *env, jobject self, jint n, jobject ja, jobject jb, jobject jc)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_point line[3];

	if (!ctx || !annot) return;

	if (n >= 2 && !ja) jni_throw_arg_void(env, "points must not be null");
	if (n >= 2 && !jb) jni_throw_arg_void(env, "points must not be null");
	if (n >= 3 && !jc) jni_throw_arg_void(env, "points must not be null");

	if (n >= 2) {
		line[0].x = (*env)->GetFloatField(env, ja, fid_Point_x);
		line[0].y = (*env)->GetFloatField(env, ja, fid_Point_y);
		line[1].x = (*env)->GetFloatField(env, jb, fid_Point_x);
		line[1].y = (*env)->GetFloatField(env, jb, fid_Point_y);
	}
	if (n >= 3) {
		line[2].x = (*env)->GetFloatField(env, jc, fid_Point_x);
		line[2].y = (*env)->GetFloatField(env, jc, fid_Point_y);
	}

	fz_try(ctx)
	{
		if (n == 0 || n == 2 || n == 3)
			pdf_set_annot_callout_line(ctx, annot, line, n);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventEnter)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
	{
		pdf_annot_event_enter(ctx, annot);
		pdf_set_annot_hot(ctx, annot, 1);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventExit)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
	{
		pdf_annot_event_exit(ctx, annot);
		pdf_set_annot_hot(ctx, annot, 0);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventDown)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
	{
		pdf_annot_event_down(ctx, annot);
		pdf_set_annot_active(ctx, annot, 1);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventUp)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
	{
		if (pdf_annot_hot(ctx, annot) && pdf_annot_active(ctx, annot))
			pdf_annot_event_up(ctx, annot);
		pdf_set_annot_active(ctx, annot, 0);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventFocus)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_annot_event_focus(ctx, annot);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventBlur)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_annot_event_blur(ctx, annot);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_update)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean changed = JNI_FALSE;

	if (!ctx || !annot) return JNI_FALSE;

	fz_try(ctx)
		changed = pdf_update_annot(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return changed;
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_getIsOpen)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean open = JNI_FALSE;

	if (!ctx || !annot) return JNI_FALSE;

	fz_try(ctx)
		open = pdf_annot_is_open(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return open;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setIsOpen)(JNIEnv *env, jobject self, jboolean open)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_is_open(ctx, annot, open);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jstring JNICALL
FUN(PDFAnnotation_getIcon)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *name = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		name = pdf_annot_icon_name(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewStringUTF(env, name);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setIcon)(JNIEnv *env, jobject self, jstring jname)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *name = NULL;

	if (!ctx || !annot) return;
	if (!jname) jni_throw_arg_void(env, "icon name must not be null");

	name = (*env)->GetStringUTFChars(env, jname, NULL);
	if (!name) return;

	fz_try(ctx)
		pdf_set_annot_icon_name(ctx, annot, name);
	fz_always(ctx)
		if (name)
			(*env)->ReleaseStringUTFChars(env, jname, name);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jintArray JNICALL
FUN(PDFAnnotation_getLineEndingStyles)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	enum pdf_line_ending s = 0, e = 0;
	int line_endings[2];
	jintArray jline_endings = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		pdf_annot_line_ending_styles(ctx, annot, &s, &e);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	line_endings[0] = s;
	line_endings[1] = e;

	jline_endings = (*env)->NewIntArray(env, 2);
	if (!jline_endings || (*env)->ExceptionCheck(env)) return NULL;

	(*env)->SetIntArrayRegion(env, jline_endings, 0, 2, &line_endings[0]);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return jline_endings;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setLineEndingStyles)(JNIEnv *env, jobject self, jint start_style, jint end_style)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_line_ending_styles(ctx, annot, start_style, end_style);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getObject)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return NULL;

	return to_PDFObject_safe(ctx, env, pdf_annot_obj(ctx, annot));
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getLanguage)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int lang;

	if (!ctx || !annot) return FZ_LANG_UNSET;

	fz_try(ctx)
		lang = pdf_annot_language(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return lang;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setLanguage)(JNIEnv *env, jobject self, jint lang)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_language(ctx, annot, lang);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasQuadding)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_quadding(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getQuadding)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int quadding;

	if (!ctx || !annot) return 0;

	fz_try(ctx)
		quadding = pdf_annot_quadding(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return quadding;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setQuadding)(JNIEnv *env, jobject self, jint quadding)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_quadding(ctx, annot, quadding);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getLine)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_point points[2] = { 0 };
	jobject jline = NULL;
	jobject jpoint = NULL;
	size_t i = 0;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		pdf_annot_line(ctx, annot, &points[0], &points[1]);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jline = (*env)->NewObjectArray(env, nelem(points), cls_Point, NULL);
	if (!jline || (*env)->ExceptionCheck(env)) return NULL;

	for (i = 0; i < nelem(points); i++)
	{
		jpoint = (*env)->NewObject(env, cls_Point, mid_Point_init, points[i].x, points[i].y);
		if (!jpoint || (*env)->ExceptionCheck(env)) return NULL;
		(*env)->SetObjectArrayElement(env, jline, i, jpoint);
		if ((*env)->ExceptionCheck(env)) return NULL;
		(*env)->DeleteLocalRef(env, jpoint);
	}

	return jline;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setLine)(JNIEnv *env, jobject self, jobject ja, jobject jb)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_point a, b;

	if (!ctx || !annot) return;
	if (!ja || !jb) jni_throw_arg_void(env, "line points must not be null");

	a.x = (*env)->GetFloatField(env, ja, fid_Point_x);
	a.y = (*env)->GetFloatField(env, ja, fid_Point_y);
	b.x = (*env)->GetFloatField(env, jb, fid_Point_x);
	b.y = (*env)->GetFloatField(env, jb, fid_Point_y);

	fz_try(ctx)
		pdf_set_annot_line(ctx, annot, a, b);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasDefaultAppearance)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_default_appearance(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getDefaultAppearance)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jobject jda = NULL;
	jobject jfont = NULL;
	jobject jcolor = NULL;
	const char *font = NULL;
	float color[4] = { 0 };
	float size = 0;
	int n = 0;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		pdf_annot_default_appearance(ctx, annot, &font, &size, &n, color);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jfont = (*env)->NewStringUTF(env, font);
	if (!jfont || (*env)->ExceptionCheck(env)) return NULL;

	jcolor = (*env)->NewFloatArray(env, n);
	if (!jcolor || (*env)->ExceptionCheck(env)) return NULL;
	(*env)->SetFloatArrayRegion(env, jcolor, 0, n, color);
	if ((*env)->ExceptionCheck(env)) return NULL;

	jda = (*env)->NewObject(env, cls_DefaultAppearance, mid_DefaultAppearance_init);
	if (!jda) return NULL;

	(*env)->SetObjectField(env, jda, fid_DefaultAppearance_font, jfont);
	(*env)->SetFloatField(env, jda, fid_DefaultAppearance_size, size);
	(*env)->SetObjectField(env, jda, fid_DefaultAppearance_color, jcolor);

	return jda;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setDefaultAppearance)(JNIEnv *env, jobject self, jstring jfont, jfloat size, jobject jcolor)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *font = NULL;
	float color[4] = { 0 };
	int n = 0;

	if (!ctx || !annot) return;
	if (!jfont) jni_throw_arg_void(env, "font must not be null");

	font = (*env)->GetStringUTFChars(env, jfont, NULL);
	if (!font) jni_throw_oom_void(env, "can not get characters in font name string");

	if (!from_jfloatArray(env, color, nelem(color), jcolor)) return;
	if (jcolor)
		n = (*env)->GetArrayLength(env, jcolor);

	fz_try(ctx)
		pdf_set_annot_default_appearance(ctx, annot, font, size, n, color);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jfont, font);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setNativeAppearance)(JNIEnv *env, jobject self, jstring jappearance, jstring jstate, jobject jctm, jobject jbbox, jobject jres, jobject jcontents)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	pdf_obj *res = from_PDFObject(env, jres);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_rect bbox = from_Rect(env, jbbox);
	fz_buffer *contents = from_Buffer(env, jcontents);
	const char *appearance = NULL;
	const char *state = NULL;

	if (!ctx || !annot) return;

	if (jappearance)
	{
		appearance = (*env)->GetStringUTFChars(env, jappearance, NULL);
		if (!appearance)
			jni_throw_oom_void(env, "can not get characters in appearance string");
	}
	if (jstate)
	{
		state = (*env)->GetStringUTFChars(env, jstate, NULL);
		if (!state)
		{
			(*env)->ReleaseStringUTFChars(env, jappearance, appearance);
			jni_throw_oom_void(env, "can not get characters in state string");
		}
	}

	fz_try(ctx)
		pdf_set_annot_appearance(ctx, annot, appearance, state, ctm, bbox, res, contents);
	fz_always(ctx)
	{
		if (jstate)
			(*env)->ReleaseStringUTFChars(env, jstate, state);
		if (jappearance)
			(*env)->ReleaseStringUTFChars(env, jappearance, appearance);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setNativeAppearanceImage)(JNIEnv *env, jobject self, jobject jimage)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_image *image = from_Image(env, jimage);

	if (!ctx || !annot || !image)
		return;

	fz_try(ctx)
		pdf_set_annot_stamp_image(ctx, annot, image);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setNativeAppearanceDisplayList)(JNIEnv *env, jobject self, jstring jappearance, jstring jstate, jobject jctm, jobject jlist)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_display_list *list = from_DisplayList(env, jlist);
	const char *appearance = NULL;
	const char *state = NULL;

	if (!ctx || !annot) return;

	if (jappearance)
	{
		appearance = (*env)->GetStringUTFChars(env, jappearance, NULL);
		if (!appearance)
			jni_throw_oom_void(env, "can not get characters in appearance string");
	}
	if (jstate)
	{
		state = (*env)->GetStringUTFChars(env, jstate, NULL);
		if (!state)
		{
			(*env)->ReleaseStringUTFChars(env, jappearance, appearance);
			jni_throw_oom_void(env, "can not get characters in state string");
		}
	}

	fz_try(ctx)
		pdf_set_annot_appearance_from_display_list(ctx, annot, appearance, state, ctm, list);
	fz_always(ctx)
	{
		if (jstate)
			(*env)->ReleaseStringUTFChars(env, jstate, state);
		if (jappearance)
			(*env)->ReleaseStringUTFChars(env, jappearance, appearance);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasInteriorColor)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_interior_color(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasAuthor)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_author(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasLineEndingStyles)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_line_ending_styles(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasQuadPoints)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_quad_points(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasVertices)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_vertices(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasInkList)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_ink_list(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasIcon)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_icon_name(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasOpen)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_open(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasLine)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_line(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setFilespec)(JNIEnv *env, jobject self, jobject jfs)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	pdf_obj *fs = from_PDFObject(env, jfs);

	fz_try(ctx)
		pdf_set_annot_filespec(ctx, annot, fs);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getFilespec)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	pdf_obj *fs = NULL;

	fz_try(ctx)
		fs = pdf_annot_filespec(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe(ctx, env, fs);
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasBorder)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_border(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getBorderStyle)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	enum pdf_border_style style = PDF_BORDER_STYLE_SOLID;

	if (!ctx || !annot) return 0;

	fz_try(ctx)
		style = pdf_annot_border_style(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return style;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setBorderStyle)(JNIEnv *env, jobject self, jint style)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_set_annot_border_style(ctx, annot, style);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jfloat JNICALL
FUN(PDFAnnotation_getBorderWidth)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	float width;

	if (!ctx || !annot) return 0;

	fz_try(ctx)
		width = pdf_annot_border_width(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return width;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setBorderWidth)(JNIEnv *env, jobject self, jfloat width)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_set_annot_border_width(ctx, annot, width);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getBorderDashCount)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int count;

	if (!ctx || !annot) return 0;

	fz_try(ctx)
		count = pdf_annot_border_dash_count(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return count;
}

JNIEXPORT jfloat JNICALL
FUN(PDFAnnotation_getBorderDashItem)(JNIEnv *env, jobject self, jint i)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int length;

	if (!ctx || !annot) return 0;

	fz_try(ctx)
		length = pdf_annot_border_dash_item(ctx, annot, i);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return length;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_clearBorderDash)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_clear_annot_border_dash(ctx, annot);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_addBorderDashItem)(JNIEnv *env, jobject self, jfloat length)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_add_annot_border_dash_item(ctx, annot, length);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasBorderEffect)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_border_effect(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getBorderEffect)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jint effect = PDF_BORDER_EFFECT_NONE;

	fz_try(ctx)
		effect = pdf_annot_border_effect(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return effect;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setBorderEffect)(JNIEnv *env, jobject self, jint effect)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_set_annot_border_effect(ctx, annot, effect);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jfloat JNICALL
FUN(PDFAnnotation_getBorderEffectIntensity)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jfloat intensity = PDF_BORDER_EFFECT_NONE;

	fz_try(ctx)
		intensity = pdf_annot_border_effect_intensity(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return intensity;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setBorderEffectIntensity)(JNIEnv *env, jobject self, jfloat intensity)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_set_annot_border_effect_intensity(ctx, annot, intensity);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasFilespec)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_filespec(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setHiddenForEditing)(JNIEnv *env, jobject self, jboolean hidden)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation_safe(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_hidden_for_editing(ctx, annot, hidden);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_getHiddenForEditing)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation_safe(env, self);
	jboolean hidden = JNI_FALSE;

	if (!ctx || !annot) return JNI_FALSE;

	fz_try(ctx)
		hidden = pdf_annot_hidden_for_editing(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return hidden;
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_applyRedaction)(JNIEnv *env, jobject self, jboolean blackBoxes, jint imageMethod, jint lineArt, jint text)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation_safe(env, self);
	pdf_redact_options opts = { blackBoxes, imageMethod, lineArt, text };
	jboolean redacted = JNI_FALSE;

	if (!ctx || !annot) return JNI_FALSE;

	fz_try(ctx)
		redacted = pdf_apply_redaction(ctx, annot, &opts);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return redacted;
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasRect)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_rect(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasIntent)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_intent(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getIntent)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	enum pdf_intent intent = PDF_ANNOT_IT_DEFAULT;

	if (!ctx || !annot) return PDF_ANNOT_IT_DEFAULT;

	fz_try(ctx)
		intent = pdf_annot_intent(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return intent;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setIntent)(JNIEnv *env, jobject self, jint intent)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_intent(ctx, annot, intent);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasPopup)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_popup(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getPopup)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_rect rect;

	if (!ctx || !annot) return JNI_FALSE;

	fz_try(ctx)
		rect = pdf_annot_popup(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Rect_safe(ctx, env, rect);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setPopup)(JNIEnv *env, jobject self, jobject jrect)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_rect rect = from_Rect(env, jrect);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_popup(ctx, annot, rect);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jfloat JNICALL
FUN(PDFAnnotation_getLineLeader)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jfloat v;

	if (!ctx || !annot) return 0;

	fz_try(ctx)
		v = pdf_annot_line_leader(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return v;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setLineLeader)(JNIEnv *env, jobject self, jfloat length)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_line_leader(ctx, annot, length);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}


JNIEXPORT jfloat JNICALL
FUN(PDFAnnotation_getLineLeaderExtension)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jfloat v;

	if (!ctx || !annot) return 0;

	fz_try(ctx)
		v = pdf_annot_line_leader_extension(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return v;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setLineLeaderExtension)(JNIEnv *env, jobject self, jfloat extension)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_line_leader_extension(ctx, annot, extension);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jfloat JNICALL
FUN(PDFAnnotation_getLineLeaderOffset)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jfloat v;

	if (!ctx || !annot) return 0;

	fz_try(ctx)
		v = pdf_annot_line_leader_offset(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return v;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setLineLeaderOffset)(JNIEnv *env, jobject self, jfloat offset)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_line_leader_offset(ctx, annot, offset);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_getLineCaption)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean v;

	if (!ctx || !annot) return JNI_FALSE;

	fz_try(ctx)
		v = pdf_annot_line_caption(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return v;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setLineCaption)(JNIEnv *env, jobject self, jboolean caption)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_line_caption(ctx, annot, caption);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getLineCaptionOffset)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_point offset;;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		offset = pdf_annot_line_caption_offset(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Point_safe(ctx, env, offset);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setLeaderLines)(JNIEnv *env, jobject self, jobject joffset)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_point offset = from_Point(env, joffset);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_line_caption_offset(ctx, annot, offset);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_requestSynthesis)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_annot_request_synthesis(ctx, annot);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_requestResynthesis)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_annot_request_resynthesis(ctx, annot);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_getHot)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean hot = JNI_FALSE;

	if (!ctx || !annot) return JNI_FALSE;

	fz_try(ctx)
		hot = pdf_annot_hot(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return hot;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setHot)(JNIEnv *env, jobject self, jboolean hot)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_hot(ctx, annot, hot);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_process)(JNIEnv *env, jobject self, jobject jproc)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	pdf_processor *proc = make_pdf_processor(env, ctx, jproc);

	if (!ctx || !annot) return;
	if (!proc) jni_throw_arg_void(env, "processor must not be null");

	fz_try(ctx)
	{
		pdf_processor_push_resources(ctx, proc, pdf_page_resources(ctx, pdf_annot_page(ctx, annot)));
		pdf_process_annot(ctx, proc, annot, NULL);
		pdf_close_processor(ctx, proc);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, pdf_processor_pop_resources(ctx, proc));
		pdf_drop_processor(ctx, proc);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasRichContents)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	if (!ctx || !annot) return JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_rich_contents(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jstring JNICALL
FUN(PDFAnnotation_getRichContents)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *contents = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		contents = pdf_annot_rich_contents(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_String_safe(ctx, env, contents);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setRichContents)(JNIEnv *env, jobject self, jstring jplaintext, jstring jrichtext)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *plaintext = NULL;
	const char *richtext = NULL;

	if (!ctx || !annot) return;

	if (jplaintext)
	{
		plaintext = (*env)->GetStringUTFChars(env, jplaintext, NULL);
		if (!plaintext) return;
	}
	if (jrichtext)
	{
		richtext = (*env)->GetStringUTFChars(env, jrichtext, NULL);
		if (!richtext)
		{
			if (plaintext)
				(*env)->ReleaseStringUTFChars(env, jplaintext, plaintext);
			return;
		}
	}

	fz_try(ctx)
		pdf_set_annot_rich_contents(ctx, annot, plaintext, richtext);
	fz_always(ctx)
	{
		if (richtext)
			(*env)->ReleaseStringUTFChars(env, jrichtext, richtext);
		if (plaintext)
			(*env)->ReleaseStringUTFChars(env, jplaintext, plaintext);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_hasRichDefaults)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean has = JNI_FALSE;

	if (!ctx || !annot) return JNI_FALSE;

	fz_try(ctx)
		has = pdf_annot_has_rich_defaults(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jstring JNICALL
FUN(PDFAnnotation_getRichDefaults)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *defaults = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		defaults = pdf_annot_rich_defaults(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_String_safe(ctx, env, defaults);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setRichDefault)(JNIEnv *env, jobject self, jstring jstyle)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *style = NULL;

	if (!ctx || !annot) return;

	if (jstyle)
	{
		style = (*env)->GetStringUTFChars(env, jstyle, NULL);
		if (!style) return;
	}

	fz_try(ctx)
		pdf_set_annot_rich_defaults(ctx, annot, style);
	fz_always(ctx)
		if (style)
			(*env)->ReleaseStringUTFChars(env, jstyle, style);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getStampImageObject)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	pdf_obj *obj = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		obj = pdf_annot_stamp_image_obj(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe(ctx, env, obj);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setStampImageObject)(JNIEnv *env, jobject self, jobject jobj)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	pdf_obj *obj = from_PDFObject(env, jobj);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_stamp_image_obj(ctx, annot, obj);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setStampImage)(JNIEnv *env, jobject self, jobject jimg)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_image *img = from_Image(env, jimg);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_stamp_image(ctx, annot, img);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}
