/* PDFWidget interface */

JNIEXPORT jstring JNICALL
FUN(PDFWidget_getValue)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_widget *widget = from_PDFWidget_safe(env, self);
	const char *text = NULL;

	if (!ctx || !widget) return NULL;

	fz_try(ctx)
		text = pdf_field_value(ctx, widget->obj);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewStringUTF(env, text);
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_setTextValue)(JNIEnv *env, jobject self, jstring jval)
{
	fz_context *ctx = get_context(env);
	pdf_widget *widget = from_PDFWidget_safe(env, self);
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
	pdf_widget *widget = from_PDFWidget_safe(env, self);
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
	pdf_widget *widget = from_PDFWidget_safe(env, self);
	const char *val = NULL;
	jboolean accepted = JNI_FALSE;

	if (!ctx || !widget) return JNI_FALSE;

	if (jval)
		val = (*env)->GetStringUTFChars(env, jval, NULL);

	fz_var(accepted);
	fz_try(ctx)
		accepted = pdf_set_field_value(ctx, widget->page->doc, widget->obj, (char *)val, widget->ignore_trigger_events);
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
	pdf_widget *widget = from_PDFWidget_safe(env, self);
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
	pdf_widget *widget = from_PDFWidget_safe(env, self);

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
	pdf_widget *widget = from_PDFWidget_safe(env, self);
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
	pdf_widget *widget = from_PDFWidget_safe(env, self);
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
	pdf_widget *widget = from_PDFWidget_safe(env, self);
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
	pdf_widget *widget = from_PDFWidget_safe(env, self);

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
	pdf_widget *widget = from_PDFWidget_safe(env, self);
	jboolean val = JNI_FALSE;

	if (!ctx || !widget) return 0;

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
	pdf_widget *widget = from_PDFWidget_safe(env, self);
	pdf_document *pdf = widget->page->doc;
	java_pkcs7_verifier *verifier = from_PKCS7Verifier_safe(env, jverifier);
	pdf_signature_error ret = PDF_SIGNATURE_ERROR_UNKNOWN;

	if (!ctx || !widget || !pdf) return PDF_SIGNATURE_ERROR_UNKNOWN;
	if (!verifier) jni_throw_arg(env, "verifier must not be null");

	fz_try(ctx)
		ret = pdf_check_certificate(ctx, &verifier->base, pdf, widget->obj);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return ret;
}

JNIEXPORT jint JNICALL
FUN(PDFWidget_checkDigest)(JNIEnv *env, jobject self, jobject jverifier)
{
	fz_context *ctx = get_context(env);
	pdf_widget *widget = from_PDFWidget_safe(env, self);
	pdf_document *pdf = widget->page->doc;
	java_pkcs7_verifier *verifier = from_PKCS7Verifier_safe(env, jverifier);
	pdf_signature_error ret = PDF_SIGNATURE_ERROR_UNKNOWN;

	if (!ctx || !widget || !pdf) return PDF_SIGNATURE_ERROR_UNKNOWN;
	if (!verifier) jni_throw_arg(env, "verifier must not be null");

	fz_try(ctx)
		ret = pdf_check_digest(ctx, &verifier->base, pdf, widget->obj);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return ret;
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_incrementalChangeAfterSigning)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_widget *widget = from_PDFWidget_safe(env, self);
	pdf_document *pdf = widget->page->doc;
	jboolean change = JNI_FALSE;

	if (!ctx || !widget || !pdf) return JNI_FALSE;

	fz_try(ctx)
		change = pdf_signature_incremental_change_since_signing(ctx, pdf, widget->obj);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return change;
}

JNIEXPORT jobject JNICALL
FUN(PDFWidget_getDesignatedName)(JNIEnv *env, jobject self, jobject jverifier)
{
	fz_context *ctx = get_context(env);
	pdf_widget *widget = from_PDFWidget_safe(env, self);
	java_pkcs7_verifier *verifier = from_PKCS7Verifier_safe(env, jverifier);
	pdf_document *pdf = widget->page->doc;
	jobject jcn, jo, jou, jemail, jc;
	pdf_pkcs7_designated_name *name;
	jobject jname;

	if (!ctx || !widget || !pdf) return NULL;
	if (!verifier) jni_throw_arg(env, "verifier must not be null");

	jname = (*env)->NewObject(env, cls_PKCS7DesignatedName, mid_PKCS7DesignatedName_init);
	if ((*env)->ExceptionCheck(env)) return NULL;
	if (!jname) jni_throw_run(env, "cannot create designated name object");

	fz_try(ctx)
	{
		name = pdf_signature_get_signatory(ctx, &verifier->base, pdf, widget->obj);

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
		pdf_signature_drop_designated_name(ctx, name);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	(*env)->SetObjectField(env, jname, fid_PKCS7DesignatedName_cn, jcn);
	(*env)->SetObjectField(env, jname, fid_PKCS7DesignatedName_o, jo);
	(*env)->SetObjectField(env, jname, fid_PKCS7DesignatedName_ou, jou);
	(*env)->SetObjectField(env, jname, fid_PKCS7DesignatedName_email, jemail);
	(*env)->SetObjectField(env, jname, fid_PKCS7DesignatedName_c, jc);

	return jname;
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_signNative)(JNIEnv *env, jobject self, jobject signer, jobject jimage)
{
	fz_context *ctx = get_context(env);
	pdf_widget *widget = from_PDFWidget_safe(env, self);
	pdf_document *pdf = widget->page->doc;
	pdf_pkcs7_signer *pkcs7signer = from_PKCS7Signer_safe(env, signer);
	fz_image *image = from_Image_safe(env, jimage);

	if (!ctx || !widget || !pdf) return JNI_FALSE;
	if (!pkcs7signer) jni_throw_arg(env, "signer must not be null");

	fz_try(ctx)
		pdf_sign_signature(ctx, widget, pkcs7signer, image);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return JNI_TRUE;
}
