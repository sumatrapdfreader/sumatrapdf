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

/* PDFWidget interface */

JNIEXPORT jstring JNICALL
FUN(PDFWidget_getValue)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	const char *text = NULL;

	if (!ctx || !widget) return NULL;

	fz_try(ctx)
		text = pdf_field_value(ctx, pdf_annot_obj(ctx, widget));
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewStringUTF(env, text);
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_setTextValue)(JNIEnv *env, jobject self, jstring jval)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	const char *val = NULL;
	jboolean accepted = JNI_FALSE;

	if (!ctx || !widget) return JNI_FALSE;

	if (jval)
		val = (*env)->GetStringUTFChars(env, jval, NULL);

	fz_var(accepted);
	fz_try(ctx)
		accepted = pdf_set_text_field_value(ctx, widget, val);
	fz_always(ctx)
		if (jval)
			(*env)->ReleaseStringUTFChars(env, jval, val);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return accepted;
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_setChoiceValue)(JNIEnv *env, jobject self, jstring jval)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	const char *val = NULL;
	jboolean accepted = JNI_FALSE;

	if (!ctx || !widget) return JNI_FALSE;

	if (jval)
		val = (*env)->GetStringUTFChars(env, jval, NULL);

	fz_var(accepted);
	fz_try(ctx)
		accepted = pdf_set_choice_field_value(ctx, widget, val);
	fz_always(ctx)
		if (jval)
			(*env)->ReleaseStringUTFChars(env, jval, val);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return accepted;
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_setValue)(JNIEnv *env, jobject self, jstring jval)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	const char *val = NULL;
	jboolean accepted = JNI_FALSE;

	if (!ctx || !widget) return JNI_FALSE;

	if (jval)
		val = (*env)->GetStringUTFChars(env, jval, NULL);

	fz_var(accepted);
	fz_try(ctx)
		accepted = pdf_set_field_value(ctx, pdf_annot_page(ctx, widget)->doc, pdf_annot_obj(ctx, widget), (char *)val, pdf_get_widget_editing_state(ctx, widget));
	fz_always(ctx)
		if (jval)
			(*env)->ReleaseStringUTFChars(env, jval, val);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return accepted;
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_toggle)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	jboolean accepted = JNI_FALSE;

	if (!ctx || !widget) return JNI_FALSE;

	fz_var(accepted);
	fz_try(ctx)
		accepted = pdf_toggle_widget(ctx, widget);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return accepted;
}

JNIEXPORT void JNICALL
FUN(PDFWidget_setEditing)(JNIEnv *env, jobject self, jboolean val)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);

	if (!ctx || !widget) return;

	fz_try(ctx)
		pdf_set_widget_editing_state(ctx, widget, val);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_isEditing)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	jboolean state = JNI_FALSE;

	if (!ctx || !widget) return JNI_FALSE;

	fz_var(state);
	fz_try(ctx)
		state = pdf_get_widget_editing_state(ctx, widget);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return state;
}

JNIEXPORT jobject JNICALL
FUN(PDFWidget_textQuads)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	jobject jquad;
	jobjectArray array;
	int i, nchars;
	fz_stext_page *stext = NULL;

	if (!ctx || !widget) return NULL;

	fz_try(ctx)
	{
		fz_stext_options opts = { 0 };
		opts.flags = FZ_STEXT_INHIBIT_SPACES;
		stext = pdf_new_stext_page_from_annot(ctx, widget, &opts);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	nchars = 0;
	for (fz_stext_block *block = stext->first_block; block; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_TEXT)
		{
			for (fz_stext_line *line = block->u.t.first_line; line; line = line->next)
			{
				for (fz_stext_char *ch = line->first_char; ch; ch = ch->next)
				{
					nchars++;
				}
			}
		}
	}

	array = (*env)->NewObjectArray(env, nchars, cls_Quad, NULL);
	if (!array || (*env)->ExceptionCheck(env))
	{
		fz_drop_stext_page(ctx, stext);
		return NULL;
	}

	i = 0;
	for (fz_stext_block *block = stext->first_block; block; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_TEXT)
		{
			for (fz_stext_line *line = block->u.t.first_line; line; line = line->next)
			{
				for (fz_stext_char *ch = line->first_char; ch; ch = ch->next)
				{
					jquad = to_Quad_safe(ctx, env, ch->quad);
					if (!jquad)
					{
						fz_drop_stext_page(ctx, stext);
						return NULL;
					}

					(*env)->SetObjectArrayElement(env, array, i, jquad);
					if ((*env)->ExceptionCheck(env))
					{
						fz_drop_stext_page(ctx, stext);
						return NULL;
					}

					(*env)->DeleteLocalRef(env, jquad);
					i++;
				}
			}
		}
	}

	fz_drop_stext_page(ctx, stext);
	return array;
}

JNIEXPORT jint JNICALL
FUN(PDFWidget_validateSignature)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	int val = 0;

	if (!ctx || !widget) return 0;

	fz_try(ctx)
		val = pdf_validate_signature(ctx, widget);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return val;
}

JNIEXPORT void JNICALL
FUN(PDFWidget_clearSignature)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);

	if (!ctx || !widget) return;

	fz_try(ctx)
		pdf_clear_signature(ctx, widget);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_isSigned)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	jboolean val = JNI_FALSE;

	if (!ctx || !widget) return JNI_FALSE;

	fz_try(ctx)
		val = !!pdf_widget_is_signed(ctx, widget);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return val;
}

JNIEXPORT jint JNICALL
FUN(PDFWidget_checkCertificate)(JNIEnv *env, jobject self, jobject jverifier)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	pdf_document *pdf = pdf_annot_page(ctx, widget)->doc;
	java_pkcs7_verifier *verifier = from_PKCS7Verifier_safe(env, jverifier);
	pdf_signature_error ret = PDF_SIGNATURE_ERROR_UNKNOWN;

	if (!ctx || !widget || !pdf) return PDF_SIGNATURE_ERROR_UNKNOWN;
	if (!verifier) jni_throw_arg(env, "verifier must not be null");

	fz_try(ctx)
		ret = pdf_check_certificate(ctx, &verifier->base, pdf, pdf_annot_obj(ctx, widget));
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return ret;
}

JNIEXPORT jint JNICALL
FUN(PDFWidget_checkDigest)(JNIEnv *env, jobject self, jobject jverifier)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	java_pkcs7_verifier *verifier = from_PKCS7Verifier_safe(env, jverifier);
	pdf_signature_error ret = PDF_SIGNATURE_ERROR_UNKNOWN;

	if (!ctx || !widget) return PDF_SIGNATURE_ERROR_UNKNOWN;
	if (!verifier) jni_throw_arg(env, "verifier must not be null");

	fz_try(ctx)
		ret = pdf_check_widget_digest(ctx, &verifier->base, widget);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return ret;
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_incrementalChangeSinceSigning)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	jboolean change = JNI_FALSE;

	if (!ctx || !widget) return JNI_FALSE;

	fz_try(ctx)
		change = pdf_incremental_change_since_signing_widget(ctx, widget);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return change;
}

JNIEXPORT jobject JNICALL
FUN(PDFWidget_getDistinguishedName)(JNIEnv *env, jobject self, jobject jverifier)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	java_pkcs7_verifier *verifier = from_PKCS7Verifier_safe(env, jverifier);
	pdf_document *pdf = pdf_annot_page(ctx, widget)->doc;
	jobject jcn, jo, jou, jemail, jc;
	pdf_pkcs7_distinguished_name *name;
	jobject jname;

	if (!ctx || !widget || !pdf) return NULL;
	if (!verifier) jni_throw_arg(env, "verifier must not be null");

	jname = (*env)->NewObject(env, cls_PKCS7DistinguishedName, mid_PKCS7DistinguishedName_init);
	if ((*env)->ExceptionCheck(env)) return NULL;
	if (!jname) jni_throw_run(env, "cannot create distinguished name object");

	fz_try(ctx)
	{
		name = pdf_signature_get_widget_signatory(ctx, &verifier->base, widget);

		jcn = (*env)->NewStringUTF(env, name->cn);
		if (!jcn)
			jni_throw_run(env, "cannot create common name string");
		if ((*env)->ExceptionCheck(env))
			fz_throw_java(ctx, env);
		jo = (*env)->NewStringUTF(env, name->o);
		if (!jo)
			jni_throw_run(env, "cannot create organization string");
		if ((*env)->ExceptionCheck(env))
			fz_throw_java(ctx, env);
		jou = (*env)->NewStringUTF(env, name->ou);
		if (!jou)
			jni_throw_run(env, "cannot create organizational unit string");
		if ((*env)->ExceptionCheck(env))
			fz_throw_java(ctx, env);
		jemail = (*env)->NewStringUTF(env, name->email);
		if (!jemail)
			jni_throw_run(env, "cannot create email string");
		if ((*env)->ExceptionCheck(env))
			fz_throw_java(ctx, env);
		jc = (*env)->NewStringUTF(env, name->c);
		if (!jc)
			jni_throw_run(env, "cannot create country string");
		if ((*env)->ExceptionCheck(env))
			fz_throw_java(ctx, env);
	}
	fz_always(ctx)
		pdf_signature_drop_distinguished_name(ctx, name);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	(*env)->SetObjectField(env, jname, fid_PKCS7DistinguishedName_cn, jcn);
	(*env)->SetObjectField(env, jname, fid_PKCS7DistinguishedName_o, jo);
	(*env)->SetObjectField(env, jname, fid_PKCS7DistinguishedName_ou, jou);
	(*env)->SetObjectField(env, jname, fid_PKCS7DistinguishedName_email, jemail);
	(*env)->SetObjectField(env, jname, fid_PKCS7DistinguishedName_c, jc);

	return jname;
}

JNIEXPORT jstring JNICALL
FUN(PDFWidget_getSignatory)(JNIEnv *env, jobject self, jobject jverifier)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	java_pkcs7_verifier *verifier = from_PKCS7Verifier_safe(env, jverifier);
	pdf_document *pdf = pdf_annot_page(ctx, widget)->doc;
	pdf_pkcs7_distinguished_name *name;
	char *s = NULL;
	char buf[800];

	if (!ctx || !widget || !pdf) return NULL;
	if (!verifier) jni_throw_arg(env, "verifier must not be null");

	fz_var(s);

	fz_try(ctx)
	{
		name = pdf_signature_get_widget_signatory(ctx, &verifier->base, widget);
		if (name)
		{
			s = pdf_signature_format_distinguished_name(ctx, name);
			fz_strlcpy(buf, s, sizeof buf);
			fz_free(ctx, s);
		}
		else
		{
			fz_strlcpy(buf, "Signature information missing.", sizeof buf);
		}
	}
	fz_always(ctx)
		pdf_signature_drop_distinguished_name(ctx, name);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_String_safe(ctx, env, &buf[0]);
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_signNative)(JNIEnv *env, jobject self, jobject jsigner, jint flags, jobject jimage, jstring jreason, jstring jlocation)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	pdf_document *pdf = pdf_annot_page(ctx, widget)->doc;
	pdf_pkcs7_signer *signer = from_PKCS7Signer_safe(env, jsigner);
	fz_image *image = from_Image_safe(env, jimage);
	const char *reason = NULL;
	const char *location = NULL;

	if (!ctx || !widget || !pdf) return JNI_FALSE;
	if (!signer) jni_throw_arg(env, "signer must not be null");

	if (jreason)
		reason = (*env)->GetStringUTFChars(env, jreason, NULL);
	if (jlocation)
		location = (*env)->GetStringUTFChars(env, jlocation, NULL);

	fz_try(ctx)
		pdf_sign_signature(ctx, widget, signer, flags, image, reason, location);
	fz_always(ctx)
	{
		if (jreason)
			(*env)->ReleaseStringUTFChars(env, jreason, reason);
		if (jlocation)
			(*env)->ReleaseStringUTFChars(env, jlocation, location);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return JNI_TRUE;
}

JNIEXPORT jobject JNICALL
FUN(PDFWidget_previewSignatureNative)(JNIEnv *env, jclass cls, jint width, jint height, jint lang, jobject jsigner, jint flags, jobject jimage, jstring jreason, jstring jlocation)
{
	fz_context *ctx = get_context(env);
	pdf_pkcs7_signer *signer = from_PKCS7Signer_safe(env, jsigner);
	fz_image *image = from_Image_safe(env, jimage);
	const char *reason = NULL;
	const char *location = NULL;
	fz_pixmap *pixmap = NULL;

	if (!ctx) return JNI_FALSE;
	if (!signer) jni_throw_arg(env, "signer must not be null");

	if (jreason)
		reason = (*env)->GetStringUTFChars(env, jreason, NULL);
	if (jlocation)
		location = (*env)->GetStringUTFChars(env, jlocation, NULL);

	fz_var(pixmap);

	fz_try(ctx)
		pixmap = pdf_preview_signature_as_pixmap(ctx,
				width, height, lang,
				signer, flags, image,
				reason, location);
	fz_always(ctx)
	{
		if (jreason)
			(*env)->ReleaseStringUTFChars(env, jreason, reason);
		if (jlocation)
			(*env)->ReleaseStringUTFChars(env, jlocation, location);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Pixmap_safe_own(ctx, env, pixmap);
}

JNIEXPORT jobject JNICALL
FUN(PDFWidget_layoutTextWidget)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	fz_layout_block *layout = NULL;
	fz_layout_line *line = NULL;
	fz_layout_char *chr = NULL;
	jobject jlayout, jlines, jmatrix, jinvmatrix;
	fz_rect bounds;
	fz_matrix mat;
	const char *s;
	int nlines = 0;
	int i;

	if (!ctx || !widget) return NULL;

	jlayout = (*env)->NewObject(env, cls_TextWidgetLayout, mid_TextWidgetLayout_init, self);
	if ((*env)->ExceptionCheck(env)) return NULL;
	if (!jlayout) jni_throw_run(env, "cannot create text widget layout object");

	fz_try(ctx)
	{
		bounds = pdf_bound_widget(ctx, widget);
		layout = pdf_layout_text_widget(ctx, widget);
		mat = fz_concat(layout->inv_matrix, fz_translate(-bounds.x0, -bounds.y0));
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jmatrix = to_Matrix_safe(ctx, env, layout->matrix);
	if ((*env)->ExceptionCheck(env))
	{
		fz_drop_layout(ctx, layout);
		return NULL;
	}
	if (!jmatrix)
	{
		fz_drop_layout(ctx, layout);
		jni_throw_run(env, "cannot create text widget layout matrix object");
	}
	(*env)->SetObjectField(env, jlayout, fid_TextWidgetLayout_matrix, jmatrix);

	jinvmatrix = to_Matrix_safe(ctx, env, layout->inv_matrix);
	if ((*env)->ExceptionCheck(env))
	{
		fz_drop_layout(ctx, layout);
		return NULL;
	}
	if (!jinvmatrix)
	{
		fz_drop_layout(ctx, layout);
		jni_throw_run(env, "cannot create text widget layout inverted matrix object");
	}
	(*env)->SetObjectField(env, jlayout, fid_TextWidgetLayout_invMatrix, jinvmatrix);

	for (line = layout->head; line; line = line->next)
		nlines++;

	jlines = (*env)->NewObjectArray(env, nlines, cls_TextWidgetLineLayout, NULL);
	if ((*env)->ExceptionCheck(env))
	{
		fz_drop_layout(ctx, layout);
		return NULL;
	}
	if (!jlines)
	{
		fz_drop_layout(ctx, layout);
		jni_throw_run(env, "cannot create text widget line layout object");
	}
	(*env)->SetObjectField(env, jlayout, fid_TextWidgetLayout_lines, jlines);

	s = layout->head->p;

	i = 0;
	for (line = layout->head; line; line = line->next)
	{
		jobject jlinelayout, jchars, jlrect;
		float y = line->y - line->font_size * 0.2f;
		float b = line->y + line->font_size;
		fz_rect lrect = fz_make_rect(line->x, y, line->x, b);
		lrect = fz_transform_rect(lrect, mat);
		int nchars = 0;
		int k;

		jlinelayout = (*env)->NewObject(env, cls_TextWidgetLineLayout, mid_TextWidgetLineLayout_init, self);
		if ((*env)->ExceptionCheck(env))
		{
			fz_drop_layout(ctx, layout);
			return NULL;
		}
		if (!jlinelayout)
		{
			fz_drop_layout(ctx, layout);
			jni_throw_run(env, "cannot create text widget line layout object");
		}

		(*env)->SetObjectArrayElement(env, jlines, i, jlinelayout);
		if ((*env)->ExceptionCheck(env))
		{
			fz_drop_layout(ctx, layout);
			return NULL;
		}
		i++;

		(*env)->SetFloatField(env, jlinelayout, fid_TextWidgetLineLayout_x, line->x);
		(*env)->SetFloatField(env, jlinelayout, fid_TextWidgetLineLayout_y, line->y);
		(*env)->SetFloatField(env, jlinelayout, fid_TextWidgetLineLayout_fontSize, line->font_size);
		(*env)->SetIntField(env, jlinelayout, fid_TextWidgetLineLayout_index, fz_runeidx(s, line->p));

		for (chr = line->text; chr; chr = chr->next)
			nchars++;

		jchars = (*env)->NewObjectArray(env, nchars, cls_TextWidgetCharLayout, NULL);
		if (!jchars || (*env)->ExceptionCheck(env))
		{
			fz_drop_layout(ctx, layout);
			return NULL;
		}
		(*env)->SetObjectField(env, jlinelayout, fid_TextWidgetLineLayout_chars, jchars);

		k = 0;
		for (chr = line->text; chr; chr = chr->next)
		{
			jobject jcharlayout, jcrect;
			fz_rect crect = fz_make_rect(chr->x, y, chr->x + chr->advance, b);
			crect = fz_transform_rect(crect, mat);
			lrect = fz_union_rect(lrect, crect);

			jcharlayout = (*env)->NewObject(env, cls_TextWidgetCharLayout, mid_TextWidgetCharLayout_init, self);
			if ((*env)->ExceptionCheck(env))
			{
				fz_drop_layout(ctx, layout);
				return NULL;
			}
			if (!jcharlayout)
			{
				fz_drop_layout(ctx, layout);
				jni_throw_run(env, "cannot create text widget character layout object");
			}

			(*env)->SetObjectArrayElement(env, jchars, k, jcharlayout);
			if ((*env)->ExceptionCheck(env))
			{
				fz_drop_layout(ctx, layout);
				return NULL;
			}
			k++;

			jcrect = to_Rect_safe(ctx, env, crect);
			(*env)->SetObjectField(env, jcharlayout, fid_TextWidgetCharLayout_rect, jcrect);
			(*env)->SetFloatField(env, jcharlayout, fid_TextWidgetCharLayout_x, chr->x);
			(*env)->SetFloatField(env, jcharlayout, fid_TextWidgetCharLayout_advance, chr->advance);
			(*env)->SetIntField(env, jcharlayout, fid_TextWidgetCharLayout_index, fz_runeidx(s, chr->p));

			(*env)->DeleteLocalRef(env, jcrect);
			(*env)->DeleteLocalRef(env, jcharlayout);
		}

		jlrect = to_Rect_safe(ctx, env, lrect);
		(*env)->SetObjectField(env, jlinelayout, fid_TextWidgetLineLayout_rect, jlrect);

		(*env)->DeleteLocalRef(env, jlrect);
		(*env)->DeleteLocalRef(env, jchars);
		(*env)->DeleteLocalRef(env, jlinelayout);
	}

	fz_drop_layout(ctx, layout);

	return jlayout;
}

JNIEXPORT jstring JNICALL
FUN(PDFWidget_getLabel)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	const char *text = NULL;

	if (!ctx || !widget) return NULL;

	fz_try(ctx)
		text = pdf_annot_field_label(ctx, widget);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewStringUTF(env, text);
}

JNIEXPORT jstring JNICALL
FUN(PDFWidget_getName)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	char *name = NULL;
	jstring jname;

	if (!ctx || !widget) return NULL;

	fz_try(ctx)
		name = pdf_load_field_name(ctx, pdf_annot_obj(ctx, widget));
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jname = (*env)->NewStringUTF(env, name);
	fz_free(ctx, name);
	if (!jname || (*env)->ExceptionCheck(env))
		jni_throw_run(env, "cannot create widget name string");
	return jname;
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_getEditingState)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);
	jboolean editing = JNI_FALSE;

	if (!ctx || !widget) return JNI_FALSE;

	fz_try(ctx)
		editing = pdf_get_widget_editing_state(ctx, widget);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return editing;
}

JNIEXPORT void JNICALL
FUN(PDFWidget_setEditingState)(JNIEnv *env, jobject self, jboolean editing)
{
	fz_context *ctx = get_context(env);
	pdf_annot *widget = from_PDFWidget_safe(env, self);

	if (!ctx || !widget) return;

	fz_try(ctx)
		pdf_set_widget_editing_state(ctx, widget, editing);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}
