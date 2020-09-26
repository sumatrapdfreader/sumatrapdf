/* PKCS7Verifier interface */

static void java_pkcs7_drop_verifier(fz_context *ctx, pdf_pkcs7_verifier *verifier_)
{
	java_pkcs7_verifier *verifier = (java_pkcs7_verifier *) verifier_;
	jboolean detach = JNI_FALSE;
	JNIEnv *env = NULL;

	env = jni_attach_thread(ctx, &detach);
	if (!env)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in java_pkcs7_check_digest");

	(*env)->DeleteGlobalRef(env, verifier->jverifier);
	fz_free(ctx, verifier);

	jni_detach_thread(detach);
}

static pdf_signature_error java_pkcs7_check_certificate(fz_context *ctx, pdf_pkcs7_verifier *verifier, unsigned char *signature, size_t len)
{
	java_pkcs7_verifier *pkcs7_verifier = (java_pkcs7_verifier *) verifier;
	jobject jverifier = pkcs7_verifier->jverifier;
	jint result = PDF_SIGNATURE_ERROR_UNKNOWN;
	jboolean detach = JNI_FALSE;
	JNIEnv *env = NULL;
	jobject jsignature = NULL;

	env = jni_attach_thread(ctx, &detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in java_pkcs7_check_digest");

	fz_try(ctx)
		jsignature = to_byteArray(ctx, env, signature, (int)len);
	fz_catch(ctx)
		fz_rethrow_and_detach_thread(ctx, detach);

	result = (*env)->CallIntMethod(env, jverifier, mid_PKCS7Verifier_checkCertificate, jsignature);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);

	jni_detach_thread(detach);

	return result;
}

static pdf_signature_error java_pkcs7_check_digest(fz_context *ctx, pdf_pkcs7_verifier *verifier, fz_stream *stm, unsigned char *signature, size_t len)
{
	java_pkcs7_verifier *pkcs7_verifier = (java_pkcs7_verifier *) verifier;
	jobject jverifier = pkcs7_verifier->jverifier;
	jint result = PDF_SIGNATURE_ERROR_UNKNOWN;
	jboolean detach = JNI_FALSE;
	jobject jsignature = NULL;
	jobject jstm = NULL;
	JNIEnv *env = NULL;

	env = jni_attach_thread(ctx, &detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in java_pkcs7_check_digest");

	fz_try(ctx)
	{
		jsignature = to_byteArray(ctx, env, signature, (int)len);
		jstm = to_FitzInputStream(ctx, env, stm);
	}
	fz_catch(ctx)
		fz_rethrow_and_detach_thread(ctx, detach);

	result = (*env)->CallIntMethod(env, jverifier, mid_PKCS7Verifier_checkDigest, jstm, jsignature);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);

	jni_detach_thread(detach);

	return result;
}

static pdf_pkcs7_verifier *java_pkcs7_new_verifier(fz_context *ctx, jobject jverifier)
{
	java_pkcs7_verifier *verifier = fz_malloc_struct(ctx, java_pkcs7_verifier);
	verifier->base.drop = java_pkcs7_drop_verifier;
	verifier->base.check_digest = java_pkcs7_check_digest;
	verifier->base.check_certificate = java_pkcs7_check_certificate;
	verifier->jverifier = jverifier;
	return &verifier->base;
}

JNIEXPORT jlong JNICALL
FUN(PKCS7Verifier_newNative)(JNIEnv *env, jobject self, jobject jverifier)
{
	fz_context *ctx = get_context(env);
	pdf_pkcs7_verifier *verifier = NULL;

	if (!ctx) return 0;
	if (!jverifier) jni_throw_arg(env, "verifier must not be null");

	jverifier = (*env)->NewGlobalRef(env, jverifier);
	if (!jverifier) jni_throw_arg(env, "unable to get reference to verifier");

	fz_try(ctx)
		verifier = java_pkcs7_new_verifier(ctx, jverifier);
	fz_catch(ctx)
	{
		(*env)->DeleteGlobalRef(env, jverifier);
		jni_rethrow(env, ctx);
	}

	return jlong_cast(verifier);
}

JNIEXPORT void JNICALL
FUN(PKCS7Verifier_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	java_pkcs7_verifier *verifier = from_PKCS7Verifier_safe(env, self);
	if (!ctx || !verifier) return;
	(*env)->SetLongField(env, self, fid_PKCS7Verifier_pointer, 0);
	pdf_drop_verifier(ctx, &verifier->base);
}
