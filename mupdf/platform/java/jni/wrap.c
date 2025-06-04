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

/* Conversion functions: C to Java. These all throw fitz exceptions. */

static inline jobject to_ColorSpace(fz_context *ctx, JNIEnv *env, fz_colorspace *cs)
{
	jobject jcs;

	if (!ctx || !cs) return NULL;

	fz_keep_colorspace(ctx, cs);
	jcs = (*env)->CallStaticObjectMethod(env, cls_ColorSpace, mid_ColorSpace_fromPointer, jlong_cast(cs));
	if (!jcs)
		fz_drop_colorspace(ctx, cs);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jcs;
}

static inline jobject to_DefaultColorSpaces(fz_context *ctx, JNIEnv *env, fz_default_colorspaces *dcs)
{
	jobject jdcs;

	if (!ctx || !dcs) return NULL;

	fz_keep_default_colorspaces(ctx, dcs);
	jdcs = (*env)->NewObject(env, cls_DefaultColorSpaces, mid_DefaultColorSpaces_init, jlong_cast(dcs));
	if (!jdcs)
		fz_drop_default_colorspaces(ctx, dcs);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jdcs;
}

static inline jobject to_FitzInputStream(fz_context *ctx, JNIEnv *env, fz_stream *stm)
{
	jobject jstm;

	if (!ctx || !stm) return NULL;

	fz_keep_stream(ctx, stm);
	jstm = (*env)->NewObject(env, cls_FitzInputStream, mid_FitzInputStream_init, jlong_cast(stm));
	if (!jstm)
		fz_drop_stream(ctx, stm);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jstm;
}

static inline jobject to_Image(fz_context *ctx, JNIEnv *env, fz_image *img)
{
	jobject jimg;

	if (!ctx || !img) return NULL;

	fz_keep_image(ctx, img);
	jimg = (*env)->NewObject(env, cls_Image, mid_Image_init, jlong_cast(img));
	if (!jimg)
		fz_drop_image(ctx, img);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jimg;
}

static inline jobject to_Matrix(fz_context *ctx, JNIEnv *env, fz_matrix mat)
{
	jobject jctm;

	if (!ctx) return NULL;

	jctm = (*env)->NewObject(env, cls_Matrix, mid_Matrix_init, mat.a, mat.b, mat.c, mat.d, mat.e, mat.f);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jctm;
}

static inline jobject to_Path(fz_context *ctx, JNIEnv *env, const fz_path *path)
{
	jobject jpath;

	if (!ctx || !path) return NULL;

	fz_keep_path(ctx, path);
	jpath = (*env)->NewObject(env, cls_Path, mid_Path_init, jlong_cast(path));
	if (!jpath)
		fz_drop_path(ctx, path);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jpath;
}

static inline jobject to_Rect(fz_context *ctx, JNIEnv *env, fz_rect rect)
{
	jobject jrect;

	if (!ctx) return NULL;

	jrect = (*env)->NewObject(env, cls_Rect, mid_Rect_init, rect.x0, rect.y0, rect.x1, rect.y1);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jrect;
}

static inline jobject to_Shade(fz_context *ctx, JNIEnv *env, fz_shade *shd)
{
	jobject jshd;

	if (!ctx || !shd) return NULL;

	fz_keep_shade(ctx, shd);
	jshd = (*env)->NewObject(env, cls_Shade, mid_Shade_init, jlong_cast(shd));
	if (!jshd)
		fz_drop_shade(ctx, shd);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jshd;
}

static inline jobject to_StrokeState(fz_context *ctx, JNIEnv *env, const fz_stroke_state *state)
{
	jobject jstate;

	if (!ctx || !state) return NULL;

	fz_keep_stroke_state(ctx, state);
	jstate = (*env)->NewObject(env, cls_StrokeState, mid_StrokeState_init, jlong_cast(state));
	if (!jstate)
		fz_drop_stroke_state(ctx, state);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jstate;
}

static inline jobject to_Text(fz_context *ctx, JNIEnv *env, const fz_text *text)
{
	jobject jtext;

	if (!ctx) return NULL;

	fz_keep_text(ctx, text);
	jtext = (*env)->NewObject(env, cls_Text, mid_Text_init, jlong_cast(text));
	if (!jtext)
		fz_drop_text(ctx, text);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jtext;
}

static inline jbyteArray to_byteArray(fz_context *ctx, JNIEnv *env, const unsigned char *arr, jint n)
{
	jbyteArray jarr;

	if (!ctx) return NULL;

	jarr = (*env)->NewByteArray(env, n);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
	if (!jarr)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot allocate byte array");

	(*env)->SetByteArrayRegion(env, jarr, 0, n, (jbyte *) arr);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jarr;
}

static inline jfloatArray to_floatArray(fz_context *ctx, JNIEnv *env, const float *arr, jint n)
{
	jfloatArray jarr;

	if (!ctx) return NULL;

	jarr = (*env)->NewFloatArray(env, n);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
	if (!jarr)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot allocate float array");

	(*env)->SetFloatArrayRegion(env, jarr, 0, n, arr);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jarr;
}

static inline jintArray to_intArray(fz_context *ctx, JNIEnv *env, const int *arr, jint n)
{
	jintArray jarr;

	if (!ctx) return NULL;

	jarr = (*env)->NewIntArray(env, n);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
	if (!jarr)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot allocate int array");

	(*env)->SetIntArrayRegion(env, jarr, 0, n, (jint *) arr);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jarr;
}

/* Conversion functions: C to Java. None of these throw fitz exceptions. */

static inline jobject to_Buffer_safe(fz_context *ctx, JNIEnv *env, fz_buffer *buf)
{
	jobject jbuf;

	if (!ctx || !buf) return NULL;

	fz_keep_buffer(ctx, buf);
	jbuf = (*env)->NewObject(env, cls_Buffer, mid_Buffer_init, jlong_cast(buf));
	if (!jbuf)
		fz_drop_buffer(ctx, buf);

	return jbuf;
}

static inline jint to_ColorParams_safe(fz_context *ctx, JNIEnv *env, fz_color_params cp)
{
	if (!ctx) return 0;
	return (((int) (!!cp.bp)<<5) | ((int) (!!cp.op)<<6) | ((int) (!!cp.opm)<<7) | (cp.ri & 31));
}

static inline jobject to_ColorSpace_safe(fz_context *ctx, JNIEnv *env, fz_colorspace *cs)
{
	jobject jcs;

	if (!ctx || !cs) return NULL;

	fz_keep_colorspace(ctx, cs);
	jcs = (*env)->CallStaticObjectMethod(env, cls_ColorSpace, mid_ColorSpace_fromPointer, jlong_cast(cs));
	if (!jcs) fz_drop_colorspace(ctx, cs);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return jcs;
}

static inline jobject to_Font_safe(fz_context *ctx, JNIEnv *env, fz_font *font)
{
	jobject jfont;

	if (!ctx || !font) return NULL;

	fz_keep_font(ctx, font);
	jfont = (*env)->NewObject(env, cls_Font, mid_Font_init, jlong_cast(font));
	if (!jfont)
		fz_drop_font(ctx, font);

	return jfont;
}

static inline jobject to_Image_safe(fz_context *ctx, JNIEnv *env, fz_image *img)
{
	jobject jimg;

	if (!ctx || !img) return NULL;

	fz_keep_image(ctx, img);
	jimg = (*env)->NewObject(env, cls_Image, mid_Image_init, jlong_cast(img));
	if (!jimg)
		fz_drop_image(ctx, img);

	return jimg;
}

static inline jobject to_Link_safe(fz_context *ctx, JNIEnv *env, fz_link *link)
{
	jobject jlink;

	if (!ctx || !link) return NULL;

	fz_keep_link(ctx, link);
	jlink = (*env)->NewObject(env, cls_Link, mid_Link_init, jlong_cast(link));
	if (!jlink)
		fz_drop_link(ctx, link);

	return jlink;
}

static inline jobject to_Matrix_safe(fz_context *ctx, JNIEnv *env, fz_matrix mat)
{
	if (!ctx) return NULL;

	return (*env)->NewObject(env, cls_Matrix, mid_Matrix_init, mat.a, mat.b, mat.c, mat.d, mat.e, mat.f);
}

static inline jobject to_Outline_safe(fz_context *ctx, JNIEnv *env, fz_document *doc, fz_outline *outline)
{
	jobject joutline = NULL;
	jobject jarr = NULL;
	jsize jindex = 0;
	jsize count = 0;
	fz_outline *counter = outline;

	if (!ctx || !outline) return NULL;

	while (counter)
	{
		count++;
		counter = counter->next;
	}

	jarr = (*env)->NewObjectArray(env, count, cls_Outline, NULL);
	if (!jarr || (*env)->ExceptionCheck(env)) return NULL;

	while (outline)
	{
		jstring jtitle = NULL;
		jstring juri = NULL;
		jobject jdown = NULL;

		if (outline->title)
		{
			jtitle = (*env)->NewStringUTF(env, outline->title);
			if (!jtitle || (*env)->ExceptionCheck(env)) return NULL;
		}

		if (outline->uri)
		{
			juri = (*env)->NewStringUTF(env, outline->uri);
			if (!juri || (*env)->ExceptionCheck(env)) return NULL;
		}

		if (outline->down)
		{
			jdown = to_Outline_safe(ctx, env, doc, outline->down);
			if (!jdown) return NULL;
		}

		joutline = (*env)->NewObject(env, cls_Outline, mid_Outline_init, jtitle, juri, jdown);
		if (!joutline) return NULL;

		if (jdown)
			(*env)->DeleteLocalRef(env, jdown);
		if (juri)
			(*env)->DeleteLocalRef(env, juri);
		if (jtitle)
			(*env)->DeleteLocalRef(env, jtitle);

		(*env)->SetObjectArrayElement(env, jarr, jindex++, joutline);
		if ((*env)->ExceptionCheck(env)) return NULL;

		(*env)->DeleteLocalRef(env, joutline);
		outline = outline->next;
	}

	return jarr;
}

static inline jobject to_OutlineIterator_safe(fz_context *ctx, JNIEnv *env, fz_outline_iterator *iterator)
{
	jobject jiterator = NULL;

	if (!ctx || !iterator) return NULL;

	jiterator = (*env)->NewObject(env, cls_OutlineIterator, mid_OutlineIterator_init, (jlong)iterator);
	if (!jiterator || (*env)->ExceptionCheck(env)) return NULL;

	return jiterator;
}

static inline jobject to_PDFAnnotation_safe(fz_context *ctx, JNIEnv *env, pdf_annot *annot)
{
	jobject jannot;

	if (!ctx || !annot) return NULL;

	pdf_keep_annot(ctx, annot);
	jannot = (*env)->NewObject(env, cls_PDFAnnotation, mid_PDFAnnotation_init, jlong_cast(annot));
	if (!jannot)
		pdf_drop_annot(ctx, annot);

	return jannot;
}

static inline jobject to_PDFDocument_safe(fz_context *ctx, JNIEnv *env, pdf_document *pdf)
{
	jobject jpdf;

	if (!ctx || !pdf) return NULL;

	pdf_keep_document(ctx, pdf);
	jpdf = (*env)->NewObject(env, cls_PDFDocument, mid_PDFDocument_init, jlong_cast(pdf));
	if (!jpdf)
		pdf_drop_document(ctx, pdf);

	return jpdf;
}

static inline jobject to_PDFObject_safe(fz_context *ctx, JNIEnv *env, pdf_obj *obj)
{
	jobject jobj;

	if (!ctx) return NULL;

	if (obj == NULL)
		return (*env)->GetStaticObjectField(env, cls_PDFObject, fid_PDFObject_Null);

	pdf_keep_obj(ctx, obj);
	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj));
	if (!jobj)
		pdf_drop_obj(ctx, obj);

	return jobj;
}

static inline jobject to_Point_safe(fz_context *ctx, JNIEnv *env, fz_point point)
{
	if (!ctx) return NULL;

	return (*env)->NewObject(env, cls_Point, mid_Point_init, point.x, point.y);
}

static inline jobject to_Quad_safe(fz_context *ctx, JNIEnv *env, fz_quad quad)
{
	if (!ctx) return NULL;

	return (*env)->NewObject(env, cls_Quad, mid_Quad_init,
		quad.ul.x, quad.ul.y,
		quad.ur.x, quad.ur.y,
		quad.ll.x, quad.ll.y,
		quad.lr.x, quad.lr.y);
}

static inline jobjectArray to_QuadArray_safe(fz_context *ctx, JNIEnv *env, const fz_quad *quads, jint n)
{
	jobjectArray arr;
	int i;

	if (!ctx || !quads) return NULL;

	arr = (*env)->NewObjectArray(env, n, cls_Quad, NULL);
	if (!arr || (*env)->ExceptionCheck(env)) return NULL;

	for (i = 0; i < n; i++)
	{
		jobject jquad = to_Quad_safe(ctx, env, quads[i]);
		if (!jquad) return NULL;

		(*env)->SetObjectArrayElement(env, arr, i, jquad);
		if ((*env)->ExceptionCheck(env)) return NULL;

		(*env)->DeleteLocalRef(env, jquad);
	}

	return arr;
}

static inline jobject to_DOM_safe(fz_context *ctx, JNIEnv *env, fz_xml *xml)
{
	jobject jxml;

	if (!ctx || !xml) return NULL;

	fz_keep_xml(ctx, xml);
	jxml = (*env)->NewObject(env, cls_DOM, mid_DOM_init, jlong_cast(xml));
	if (!jxml) fz_drop_xml(ctx, xml);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return jxml;
}

static inline jobject to_Shade_safe(fz_context *ctx, JNIEnv *env, fz_shade *sh)
{
	jobject jsh;

	if (!ctx || !sh) return NULL;

	fz_keep_shade(ctx, sh);
	jsh = (*env)->NewObject(env, cls_Shade, mid_Shade_init, jlong_cast(sh));
	if (!jsh) fz_drop_shade(ctx, sh);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return jsh;
}

static inline jobject to_String_safe(fz_context *ctx, JNIEnv *env, const char *val)
{
	jstring jval;
	if (!ctx) return NULL;

	jval = (*env)->NewStringUTF(env, val);
	if (!jval || (*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jval;
}

static inline jobject to_Rect_safe(fz_context *ctx, JNIEnv *env, fz_rect rect)
{
	if (!ctx) return NULL;

	return (*env)->NewObject(env, cls_Rect, mid_Rect_init, rect.x0, rect.y0, rect.x1, rect.y1);
}

static inline jobjectArray to_StringArray_safe(fz_context *ctx, JNIEnv *env, const char **strings, int n)
{
	jobjectArray arr;
	int i;

	if (!ctx || !strings) return NULL;

	arr = (*env)->NewObjectArray(env, n, cls_String, NULL);
	if (!arr || (*env)->ExceptionCheck(env)) return NULL;

	for (i = 0; i < n; i++)
	{
		jstring jstring;

		jstring = (*env)->NewStringUTF(env, strings[i]);
		if (!jstring || (*env)->ExceptionCheck(env)) return NULL;

		(*env)->SetObjectArrayElement(env, arr, i, jstring);
		if ((*env)->ExceptionCheck(env)) return NULL;

		(*env)->DeleteLocalRef(env, jstring);
	}

	return arr;
}

static inline jobject to_PDFWidget_safe(fz_context *ctx, JNIEnv *env, pdf_annot *widget)
{
	jobject jwidget;
	int nopts;
	char **opts = NULL;
	jobjectArray jopts = NULL;

	fz_var(opts);

	pdf_keep_annot(ctx, widget);
	jwidget = (*env)->NewObject(env, cls_PDFWidget, mid_PDFWidget_init, jlong_cast(widget));
	if (!jwidget || (*env)->ExceptionCheck(env))
	{
		pdf_drop_annot(ctx, widget);
		jni_throw_null(env, "cannot wrap PDF widget in java object");
	}

	fz_try(ctx)
	{
		int fieldType = pdf_widget_type(ctx, widget);
		int fieldFlags = pdf_field_flags(ctx, pdf_annot_obj(ctx, widget));
		(*env)->SetIntField(env, jwidget, fid_PDFWidget_fieldType, fieldType);
		(*env)->SetIntField(env, jwidget, fid_PDFWidget_fieldFlags, fieldFlags);
		if (fieldType == PDF_WIDGET_TYPE_TEXT)
		{
			(*env)->SetIntField(env, jwidget, fid_PDFWidget_maxLen, pdf_text_widget_max_len(ctx, widget));
			(*env)->SetIntField(env, jwidget, fid_PDFWidget_textFormat, pdf_text_widget_format(ctx, widget));
		}
		if (fieldType == PDF_WIDGET_TYPE_COMBOBOX || fieldType == PDF_WIDGET_TYPE_LISTBOX)
		{
			nopts = pdf_choice_widget_options(ctx, widget, 0, NULL);
			if (nopts > 0)
			{
				opts = Memento_label(fz_malloc(ctx, nopts * sizeof(*opts)), "to_PDFWidget");
				pdf_choice_widget_options(ctx, widget, 0, (const char **)opts);
				jopts = to_StringArray_safe(ctx, env, (const char **)opts, nopts);
				if (!jopts || (*env)->ExceptionCheck(env))
					fz_throw_java(ctx, env);
			}
		}
	}
	fz_always(ctx)
	{
		fz_free(ctx, opts);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}

	(*env)->SetObjectField(env, jwidget, fid_PDFWidget_options, jopts);

	return jwidget;
}

static inline jobject to_VectorInfo_safe(fz_context *ctx, JNIEnv *env, int flags)
{
	jobject jvecinfo;
	if (!ctx) return NULL;
	jvecinfo = (*env)->NewObject(env, cls_StructuredTextWalker_VectorInfo, mid_StructuredTextWalker_VectorInfo_init);
	(*env)->SetBooleanField(env, jvecinfo, fid_StructuredTextWalker_VectorInfo_isStroked, flags & FZ_STEXT_VECTOR_IS_STROKED);
	(*env)->SetBooleanField(env, jvecinfo, fid_StructuredTextWalker_VectorInfo_isRectangle, flags & FZ_STEXT_VECTOR_IS_RECTANGLE);
	return jvecinfo;
}

/* Conversion functions: C to Java. Take ownership of fitz object. None of these throw fitz exceptions. */

static inline jobject to_Buffer_safe_own(fz_context *ctx, JNIEnv *env, fz_buffer *buf)
{
	jobject jobj;

	if (!ctx || !buf) return NULL;

	jobj = (*env)->NewObject(env, cls_Buffer, mid_Buffer_init, jlong_cast(buf));
	if (!jobj)
		fz_drop_buffer(ctx, buf);

	return jobj;
}

static inline jobject to_ColorSpace_safe_own(fz_context *ctx, JNIEnv *env, fz_colorspace *cs)
{
	jobject jobj;

	if (!ctx || !cs) return NULL;

	jobj = (*env)->NewObject(env, cls_ColorSpace, mid_ColorSpace_init, jlong_cast(cs));
	if (!jobj)
		fz_drop_colorspace(ctx, cs);

	return jobj;
}

static inline jobject to_Document_safe_own(fz_context *ctx, JNIEnv *env, fz_document *doc)
{
	jobject obj;
	pdf_document *pdf;

	if (!ctx || !doc) return NULL;

	pdf = pdf_document_from_fz_document(ctx, doc);
	if (pdf)
		/* This relies on the fact that pdf == doc! */
		obj = (*env)->NewObject(env, cls_PDFDocument, mid_PDFDocument_init, jlong_cast(pdf));
	else
		obj = (*env)->NewObject(env, cls_Document, mid_Document_init, jlong_cast(doc));
	if (!obj)
		fz_drop_document(ctx, doc);

	return obj;
}

static inline jobject to_DisplayList_safe_own(fz_context *ctx, JNIEnv *env, fz_display_list *list)
{
	jobject jlist;

	if (!ctx || !list) return NULL;

	jlist = (*env)->NewObject(env, cls_DisplayList, mid_DisplayList_init, jlong_cast(list));
	if (!jlist)
		fz_drop_display_list(ctx, list);

	return jlist;
}

static inline jobject to_Image_safe_own(fz_context *ctx, JNIEnv *env, fz_image *img)
{
	jobject jimg;

	if (!ctx || !img) return NULL;

	jimg = (*env)->NewObject(env, cls_Image, mid_Image_init, jlong_cast(img));
	if (!jimg)
		fz_drop_image(ctx, img);

	return jimg;
}

static inline jobject to_NativeDevice_safe_own(fz_context *ctx, JNIEnv *env, fz_device *device)
{
	jobject jdev;

	if (!ctx || !device) return NULL;

	jdev = (*env)->NewObject(env, cls_NativeDevice, mid_NativeDevice_init, jlong_cast(device));
	if (!jdev)
		fz_drop_device(ctx, device);

	return jdev;
}

static inline jobject to_Page_safe_own(fz_context *ctx, JNIEnv *env, fz_page *page)
{
	jobject jobj;
	pdf_page *ppage;

	if (!ctx || !page) return NULL;

	ppage = pdf_page_from_fz_page(ctx, page);
	if (ppage)
		jobj = (*env)->NewObject(env, cls_PDFPage, mid_PDFPage_init, jlong_cast(page));
	else
		jobj = (*env)->NewObject(env, cls_Page, mid_Page_init, jlong_cast(page));
	if (!jobj)
		fz_drop_page(ctx, page);

	return jobj;
}

static inline jobject to_PDFDocument_safe_own(fz_context *ctx, JNIEnv *env, pdf_document *pdf)
{
	jobject obj;

	if (!ctx || !pdf) return NULL;

	obj = (*env)->NewObject(env, cls_PDFDocument, mid_PDFDocument_init, jlong_cast(pdf));
	if (!obj)
		fz_drop_document(ctx, &pdf->super);

	return obj;
}

static inline jobject to_Link_safe_own(fz_context *ctx, JNIEnv *env, fz_link *link)
{
	jobject jobj;

	if (!ctx || !link) return NULL;

	jobj = (*env)->NewObject(env, cls_Link, mid_Link_init, jlong_cast(link));
	if (!jobj)
		fz_drop_link(ctx, link);

	return jobj;
}

static inline jobject to_PDFAnnotation_safe_own(fz_context *ctx, JNIEnv *env, pdf_annot *annot)
{
	jobject jannot;

	if (!ctx || !annot) return NULL;

	jannot = (*env)->NewObject(env, cls_PDFAnnotation, mid_PDFAnnotation_init, jlong_cast(annot));
	if (!jannot)
		pdf_drop_annot(ctx, annot);

	return jannot;
}

static inline jobject to_PDFWidget_safe_own(fz_context *ctx, JNIEnv *env, pdf_annot *widget)
{
	jobject jwidget;

	if (!ctx || !widget) return NULL;

	jwidget = (*env)->NewObject(env, cls_PDFWidget, mid_PDFWidget_init, jlong_cast(widget));
	if (!jwidget)
		pdf_drop_annot(ctx, widget);

	return jwidget;
}

static inline jobject to_PDFGraftMap_safe_own(fz_context *ctx, JNIEnv *env, jobject pdf, pdf_graft_map *map)
{
	jobject jmap;

	if (!ctx || !map || !pdf) return NULL;

	jmap = (*env)->NewObject(env, cls_PDFGraftMap, mid_PDFGraftMap_init, jlong_cast(map), pdf);
	if (!jmap)
		pdf_drop_graft_map(ctx, map);

	return jmap;
}

static inline jobject to_PDFObject_safe_own(fz_context *ctx, JNIEnv *env, pdf_obj *obj)
{
	jobject jobj;

	if (!ctx || !obj) return NULL;

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj));
	if (!jobj)
		pdf_drop_obj(ctx, obj);

	return jobj;
}

static inline jobject to_Pixmap_safe_own(fz_context *ctx, JNIEnv *env, fz_pixmap *pixmap)
{
	jobject jobj;

	if (!ctx || !pixmap) return NULL;

	jobj = (*env)->NewObject(env, cls_Pixmap, mid_Pixmap_init, jlong_cast(pixmap));
	if (!jobj)
		fz_drop_pixmap(ctx, pixmap);

	return jobj;
}

static inline jobject to_String_safe_own(fz_context *ctx, JNIEnv *env, char *val)
{
	jstring jval;
	if (!ctx) return NULL;

	jval = (*env)->NewStringUTF(env, val);

	fz_free(ctx, val);

	return jval;
}

static inline jobject to_StructuredText_safe_own(fz_context *ctx, JNIEnv *env, fz_stext_page *text)
{
	jobject jtext;

	if (!ctx || !text) return NULL;

	jtext = (*env)->NewObject(env, cls_StructuredText, mid_StructuredText_init, jlong_cast(text));
	if (!jtext)
		fz_drop_stext_page(ctx, text);

	return jtext;
}

/* Conversion functions: Java to C. These all throw java exceptions. */

static inline fz_archive *from_Archive(JNIEnv *env, jobject jobj)
{
	fz_archive *arch;
	if (!jobj) return NULL;
	arch = CAST(fz_archive *, (*env)->GetLongField(env, jobj, fid_Archive_pointer));
	if (!arch) jni_throw_null(env, "cannot use already destroyed Archive");
	return arch;
}

static inline fz_buffer *from_Buffer(JNIEnv *env, jobject jobj)
{
	fz_buffer *buffer;
	if (!jobj) return NULL;
	buffer = CAST(fz_buffer *, (*env)->GetLongField(env, jobj, fid_Buffer_pointer));
	if (!buffer) jni_throw_null(env, "cannot use already destroyed Buffer");
	return buffer;
}

static inline fz_colorspace *from_ColorSpace(JNIEnv *env, jobject jobj)
{
	fz_colorspace *cs;
	if (!jobj) return NULL;
	cs = CAST(fz_colorspace *, (*env)->GetLongField(env, jobj, fid_ColorSpace_pointer));
	if (!cs) jni_throw_null(env, "cannot use already destroyed ColorSpace");
	return cs;
}

static inline fz_cookie *from_Cookie(JNIEnv *env, jobject jobj)
{
	fz_cookie *cookie;
	if (!jobj) return NULL;
	cookie = CAST(fz_cookie *, (*env)->GetLongField(env, jobj, fid_Cookie_pointer));
	if (!cookie) jni_throw_null(env, "cannot use already destroyed Cookie");
	return cookie;
}

static inline fz_default_colorspaces *from_DefaultColorSpaces(JNIEnv *env, jobject jobj)
{
	fz_default_colorspaces *dcs;
	if (!jobj) return NULL;
	dcs = CAST(fz_default_colorspaces *, (*env)->GetLongField(env, jobj, fid_DefaultColorSpaces_pointer));
	if (!dcs) jni_throw_null(env, "cannot use already destroyed DefaultColorSpaces");
	return dcs;
}

static fz_device *from_Device(JNIEnv *env, jobject jobj)
{
	fz_device *dev;
	if (!jobj) return NULL;
	dev = CAST(fz_device *, (*env)->GetLongField(env, jobj, fid_Device_pointer));
	if (!dev) jni_throw_null(env, "cannot use already destroyed Device");
	return dev;
}

static inline fz_display_list *from_DisplayList(JNIEnv *env, jobject jobj)
{
	fz_display_list *list;
	if (!jobj) return NULL;
	list = CAST(fz_display_list *, (*env)->GetLongField(env, jobj, fid_DisplayList_pointer));
	if (!list) jni_throw_null(env, "cannot use already destroyed DisplayList");
	return list;
}

static inline fz_document *from_Document(JNIEnv *env, jobject jobj)
{
	fz_document *doc;
	if (!jobj) return NULL;
	doc = CAST(fz_document *, (*env)->GetLongField(env, jobj, fid_Document_pointer));
	if (!doc) jni_throw_null(env, "cannot use already destroyed Document");
	return doc;
}

static inline fz_document_writer *from_DocumentWriter(JNIEnv *env, jobject jobj)
{
	fz_document_writer *wri;
	if (!jobj) return NULL;
	wri = CAST(fz_document_writer *, (*env)->GetLongField(env, jobj, fid_DocumentWriter_pointer));
	if (!wri) jni_throw_null(env, "cannot use already destroyed DocumentWriter");
	return wri;
}

static inline fz_font *from_Font(JNIEnv *env, jobject jobj)
{
	fz_font *font;
	if (!jobj) return NULL;
	font = CAST(fz_font *, (*env)->GetLongField(env, jobj, fid_Font_pointer));
	if (!font) jni_throw_null(env, "cannot use already destroyed Font");
	return font;
}

static inline fz_image *from_Image(JNIEnv *env, jobject jobj)
{
	fz_image *image;
	if (!jobj) return NULL;
	image = CAST(fz_image *, (*env)->GetLongField(env, jobj, fid_Image_pointer));
	if (!image) jni_throw_null(env, "cannot use already destroyed Image");
	return image;
}

static inline fz_link *from_Link(JNIEnv *env, jobject jobj)
{
	fz_link *link;
	if (!jobj) return NULL;
	link = CAST(fz_link *, (*env)->GetLongField(env, jobj, fid_Link_pointer));
	if (!link) jni_throw_null(env, "cannot use already destroyed Link");
	return link;
}

static inline fz_outline_iterator *from_OutlineIterator(JNIEnv *env, jobject jobj)
{
	fz_outline_iterator *iterator;
	if (!jobj) return NULL;
	iterator = CAST(fz_outline_iterator *, (*env)->GetLongField(env, jobj, fid_OutlineIterator_pointer));
	if (!iterator) jni_throw_null(env, "cannot use already destroyed OutlineIterator");
	return iterator;
}

static inline fz_page *from_Page(JNIEnv *env, jobject jobj)
{
	fz_page *page;
	if (!jobj) return NULL;
	page = CAST(fz_page *, (*env)->GetLongField(env, jobj, fid_Page_pointer));
	if (!page) jni_throw_null(env, "cannot use already destroyed Page");
	return page;
}

static inline fz_path *from_Path(JNIEnv *env, jobject jobj)
{
	fz_path *path;
	if (!jobj) return NULL;
	path = CAST(fz_path *, (*env)->GetLongField(env, jobj, fid_Path_pointer));
	if (!path) jni_throw_null(env, "cannot use already destroyed Path");
	return path;
}

static inline pdf_annot *from_PDFAnnotation(JNIEnv *env, jobject jobj)
{
	pdf_annot *annot;
	if (!jobj) return NULL;
	annot = CAST(pdf_annot *, (*env)->GetLongField(env, jobj, fid_PDFAnnotation_pointer));
	if (!annot) jni_throw_null(env, "cannot use already destroyed PDFAnnotation");
	return annot;
}

static inline pdf_document *from_PDFDocument(JNIEnv *env, jobject jobj)
{
	pdf_document *pdf;
	if (!jobj) return NULL;
	pdf = CAST(pdf_document *, (*env)->GetLongField(env, jobj, fid_PDFDocument_pointer));
	if (!pdf) jni_throw_null(env, "cannot use already destroyed PDFDocument");
	return pdf;
}

static inline pdf_graft_map *from_PDFGraftMap(JNIEnv *env, jobject jobj)
{
	pdf_graft_map *map;
	if (!jobj) return NULL;
	map = CAST(pdf_graft_map *, (*env)->GetLongField(env, jobj, fid_PDFGraftMap_pointer));
	if (!map) jni_throw_null(env, "cannot use already destroyed PDFGraftMap");
	return map;
}

static inline pdf_obj *from_PDFObject(JNIEnv *env, jobject jobj)
{
	pdf_obj *obj;
	if (!jobj) return NULL;
	obj = CAST(pdf_obj *, (*env)->GetLongField(env, jobj, fid_PDFObject_pointer));
	return obj;
}

static inline pdf_page *from_PDFPage(JNIEnv *env, jobject jobj)
{
	pdf_page *page;
	if (!jobj) return NULL;
	page = CAST(pdf_page *, (*env)->GetLongField(env, jobj, fid_PDFPage_pointer));
	if (!page) jni_throw_null(env, "cannot use already destroyed PDFPage");
	return page;
}

static inline fz_pixmap *from_Pixmap(JNIEnv *env, jobject jobj)
{
	fz_pixmap *pixmap;
	if (!jobj) return NULL;
	pixmap = CAST(fz_pixmap *, (*env)->GetLongField(env, jobj, fid_Pixmap_pointer));
	if (!pixmap) jni_throw_null(env, "cannot use already destroyed Pixmap");
	return pixmap;
}

static inline fz_shade *from_Shade(JNIEnv *env, jobject jobj)
{
	fz_shade *shd;
	if (!jobj) return NULL;
	shd = CAST(fz_shade *, (*env)->GetLongField(env, jobj, fid_Shade_pointer));
	if (!shd) jni_throw_null(env, "cannot use already destroyed Shade");
	return shd;
}

static inline fz_stroke_state *from_StrokeState(JNIEnv *env, jobject jobj)
{
	fz_stroke_state *stroke;
	if (!jobj) return NULL;
	stroke = CAST(fz_stroke_state *, (*env)->GetLongField(env, jobj, fid_StrokeState_pointer));
	if (!stroke) jni_throw_null(env, "cannot use already destroyed StrokeState");
	return stroke;
}

static inline fz_stext_page *from_StructuredText(JNIEnv *env, jobject jobj)
{
	fz_stext_page *stext;
	if (!jobj) return NULL;
	stext = CAST(fz_stext_page *, (*env)->GetLongField(env, jobj, fid_StructuredText_pointer));
	if (!stext) jni_throw_null(env, "cannot use already destroyed StructuredText");
	return stext;
}

static inline fz_text *from_Text(JNIEnv *env, jobject jobj)
{
	fz_text *text;
	if (!jobj) return NULL;
	text = CAST(fz_text *, (*env)->GetLongField(env, jobj, fid_Text_pointer));
	if (!text) jni_throw_null(env, "cannot use already destroyed Text");
	return text;
}

static inline int from_jfloatArray(JNIEnv *env, float *color, jint n, jfloatArray jcolor)
{
	jsize len;

	if (!jcolor)
		len = 0;
	else
	{
		len = (*env)->GetArrayLength(env, jcolor);
		if (len > n)
			len = n;
		(*env)->GetFloatArrayRegion(env, jcolor, 0, len, color);
		if ((*env)->ExceptionCheck(env)) return 0;
	}

	if (len < n)
		memset(color+len, 0, (n - len) * sizeof(float));

	return 1;
}

static inline fz_matrix from_Matrix(JNIEnv *env, jobject jmat)
{
	fz_matrix mat;

	if (!jmat)
		return fz_identity;

	mat.a = (*env)->GetFloatField(env, jmat, fid_Matrix_a);
	mat.b = (*env)->GetFloatField(env, jmat, fid_Matrix_b);
	mat.c = (*env)->GetFloatField(env, jmat, fid_Matrix_c);
	mat.d = (*env)->GetFloatField(env, jmat, fid_Matrix_d);
	mat.e = (*env)->GetFloatField(env, jmat, fid_Matrix_e);
	mat.f = (*env)->GetFloatField(env, jmat, fid_Matrix_f);

	return mat;
}

static inline fz_point from_Point(JNIEnv *env, jobject jpt)
{
	fz_point pt;

	if (!jpt)
	{
		pt.x = pt.y = 0;
		return pt;
	}

	pt.x = (*env)->GetFloatField(env, jpt, fid_Point_x);
	pt.y = (*env)->GetFloatField(env, jpt, fid_Point_y);

	return pt;
}

static inline fz_rect from_Rect(JNIEnv *env, jobject jrect)
{
	fz_rect rect;

	if (!jrect)
		return fz_empty_rect;

	rect.x0 = (*env)->GetFloatField(env, jrect, fid_Rect_x0);
	rect.x1 = (*env)->GetFloatField(env, jrect, fid_Rect_x1);
	rect.y0 = (*env)->GetFloatField(env, jrect, fid_Rect_y0);
	rect.y1 = (*env)->GetFloatField(env, jrect, fid_Rect_y1);

	return rect;
}

static inline fz_quad from_Quad(JNIEnv *env, jobject jquad)
{
	fz_quad quad;

	if (!jquad)
		return fz_make_quad(0, 0, 0, 0, 0, 0, 0, 0);

	quad.ul.x = (*env)->GetFloatField(env, jquad, fid_Quad_ul_x);
	quad.ul.y = (*env)->GetFloatField(env, jquad, fid_Quad_ul_y);
	quad.ur.x = (*env)->GetFloatField(env, jquad, fid_Quad_ur_x);
	quad.ur.y = (*env)->GetFloatField(env, jquad, fid_Quad_ur_y);
	quad.ll.x = (*env)->GetFloatField(env, jquad, fid_Quad_ll_x);
	quad.ll.y = (*env)->GetFloatField(env, jquad, fid_Quad_ll_y);
	quad.lr.x = (*env)->GetFloatField(env, jquad, fid_Quad_lr_x);
	quad.lr.y = (*env)->GetFloatField(env, jquad, fid_Quad_lr_y);

	return quad;
}

static fz_link_dest from_LinkDestination(JNIEnv *env, jobject jdest)
{
	fz_link_dest dest;

	if (!jdest)
		return fz_make_link_dest_none();

	dest.loc.chapter = (*env)->GetIntField(env, jdest, fid_LinkDestination_chapter);
	dest.loc.page = (*env)->GetIntField(env, jdest, fid_LinkDestination_page);
	dest.type = (*env)->GetIntField(env, jdest, fid_LinkDestination_type);
	dest.x = (*env)->GetFloatField(env, jdest, fid_LinkDestination_x);
	dest.y = (*env)->GetFloatField(env, jdest, fid_LinkDestination_y);
	dest.w = (*env)->GetFloatField(env, jdest, fid_LinkDestination_width);
	dest.h = (*env)->GetFloatField(env, jdest, fid_LinkDestination_height);
	dest.zoom = (*env)->GetFloatField(env, jdest, fid_LinkDestination_zoom);

	return dest;
}

/* Conversion functions: Java to C. None of these throw java exceptions. */

static inline fz_archive *from_Archive_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_archive *, (*env)->GetLongField(env, jobj, fid_Archive_pointer));
}

static inline fz_buffer *from_Buffer_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_buffer *, (*env)->GetLongField(env, jobj, fid_Buffer_pointer));
}

static inline fz_color_params from_ColorParams_safe(JNIEnv *env, jint params)
{
	fz_color_params p;

	p.bp = (params>>5) & 1;
	p.op = (params>>6) & 1;
	p.opm = (params>>7) & 1;
	p.ri = (params & 31);

	return p;
}

static inline fz_colorspace *from_ColorSpace_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_colorspace *, (*env)->GetLongField(env, jobj, fid_ColorSpace_pointer));
}

static inline fz_cookie *from_Cookie_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_cookie *, (*env)->GetLongField(env, jobj, fid_Cookie_pointer));
}

static inline fz_default_colorspaces *from_DefaultColorSpaces_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_default_colorspaces *, (*env)->GetLongField(env, jobj, fid_DefaultColorSpaces_pointer));
}

static fz_device *from_Device_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_device *, (*env)->GetLongField(env, jobj, fid_Device_pointer));
}

static inline fz_display_list *from_DisplayList_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_display_list *, (*env)->GetLongField(env, jobj, fid_DisplayList_pointer));
}

static inline fz_document *from_Document_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_document *, (*env)->GetLongField(env, jobj, fid_Document_pointer));
}

static inline fz_document_writer *from_DocumentWriter_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_document_writer *, (*env)->GetLongField(env, jobj, fid_DocumentWriter_pointer));
}

static inline fz_font *from_Font_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_font *, (*env)->GetLongField(env, jobj, fid_Font_pointer));
}

static inline fz_story *from_Story_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_story *, (*env)->GetLongField(env, jobj, fid_Story_pointer));
}

static inline fz_image *from_Image_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_image *, (*env)->GetLongField(env, jobj, fid_Image_pointer));
}

static inline fz_link *from_Link_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_link *, (*env)->GetLongField(env, jobj, fid_Link_pointer));
}

static inline fz_page *from_Page_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_page *, (*env)->GetLongField(env, jobj, fid_Page_pointer));
}

static inline fz_path *from_Path_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_path *, (*env)->GetLongField(env, jobj, fid_Path_pointer));
}

static inline pdf_annot *from_PDFAnnotation_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(pdf_annot *, (*env)->GetLongField(env, jobj, fid_PDFAnnotation_pointer));
}

static inline pdf_document *from_PDFDocument_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(pdf_document *, (*env)->GetLongField(env, jobj, fid_PDFDocument_pointer));
}

static inline pdf_graft_map *from_PDFGraftMap_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(pdf_graft_map *, (*env)->GetLongField(env, jobj, fid_PDFGraftMap_pointer));
}

static inline pdf_obj *from_PDFObject_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(pdf_obj *, (*env)->GetLongField(env, jobj, fid_PDFObject_pointer));
}

static inline pdf_annot *from_PDFWidget_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(pdf_annot *, (*env)->GetLongField(env, jobj, fid_PDFWidget_pointer));
}

static inline pdf_pkcs7_signer *from_PKCS7Signer_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(pdf_pkcs7_signer *, (*env)->GetLongField(env, jobj, fid_PKCS7Signer_pointer));
}

static inline java_pkcs7_verifier *from_PKCS7Verifier_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(java_pkcs7_verifier *, (*env)->GetLongField(env, jobj, fid_PKCS7Verifier_pointer));
}

static inline fz_stream *from_FitzInputStream_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_stream *, (*env)->GetLongField(env, jobj, fid_FitzInputStream_pointer));
}

static inline fz_pixmap *from_Pixmap_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_pixmap *, (*env)->GetLongField(env, jobj, fid_Pixmap_pointer));
}

static inline fz_shade *from_Shade_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_shade *, (*env)->GetLongField(env, jobj, fid_Shade_pointer));
}

static inline fz_stroke_state *from_StrokeState_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_stroke_state *, (*env)->GetLongField(env, jobj, fid_StrokeState_pointer));
}

static inline fz_stext_page *from_StructuredText_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_stext_page *, (*env)->GetLongField(env, jobj, fid_StructuredText_pointer));
}

static inline fz_text *from_Text_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_text *, (*env)->GetLongField(env, jobj, fid_Text_pointer));
}

static inline fz_xml *from_DOM_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_xml *, (*env)->GetLongField(env, jobj, fid_DOM_pointer));
}
