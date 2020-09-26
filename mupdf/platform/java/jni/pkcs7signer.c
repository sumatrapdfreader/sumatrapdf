/* PKCS7Signer interface */

typedef struct
{
	pdf_pkcs7_signer base; // Mupdf callbacks
	int refs;
	jobject java_signer;
} java_pkcs7_signer;

static pdf_pkcs7_signer *signer_keep(fz_context *ctx, pdf_pkcs7_signer *signer_)
{
	java_pkcs7_signer *signer = (java_pkcs7_signer *)signer_;

	if (!signer) return NULL;

	return fz_keep_imp(ctx, signer, &signer->refs);
}

static void signer_drop(fz_context *ctx, pdf_pkcs7_signer *signer_)
{
	java_pkcs7_signer *signer = (java_pkcs7_signer *)signer_;

	if (!signer) return;

	if (fz_drop_imp(ctx, signer, &signer->refs))
	{
		jboolean detach = JNI_FALSE;
		JNIEnv *env = NULL;

		env = jni_attach_thread(ctx, &detach);
		if (env == NULL)
		{
			fz_warn(ctx, "cannot attach to JVM in signer_drop");
			fz_free(ctx, signer);
			jni_detach_thread(detach);
			return;
		}

		(*env)->DeleteGlobalRef(env, signer->java_signer);
		fz_free(ctx, signer);

		jni_detach_thread(detach);
	}
}

static char *string_field_to_utfchars(fz_context *ctx, JNIEnv *env, jobject obj, jfieldID fid)
{
	const char *str = NULL;
	char *val = NULL;
	jobject jstr;

	jstr = (*env)->GetObjectField(env, obj, fid);
	if (!jstr) return NULL;

	str = (*env)->GetStringUTFChars(env, jstr, NULL);
	if (!str)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot not get UTF string");

	fz_try(ctx)
		val = Memento_label(fz_strdup(ctx, str), "string field");
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jstr, str);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return val;
}

static pdf_pkcs7_designated_name *signer_designated_name(fz_context *ctx, pdf_pkcs7_signer *signer_)
{
	java_pkcs7_signer *signer = (java_pkcs7_signer *)signer_;
	pdf_pkcs7_designated_name *name = NULL;
	jboolean detach = JNI_FALSE;
	jobject desname = NULL;
	JNIEnv *env = NULL;

	if (signer == NULL) return NULL;

	env = jni_attach_thread(ctx, &detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in pdf_pkcs7_designated_name");

	desname = (*env)->CallObjectMethod(env, signer->java_signer, mid_PKCS7Signer_name);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);
	if (desname == NULL)
		fz_throw_and_detach_thread(ctx, detach, FZ_ERROR_GENERIC, "cannot retrieve designated name");

	fz_var(name);
	fz_try(ctx)
	{
		name = Memento_label(fz_calloc(ctx, 1, sizeof(*name)), "designated name");
		name->cn = string_field_to_utfchars(ctx, env, desname, fid_PKCS7DesignatedName_cn);
		name->o = string_field_to_utfchars(ctx, env, desname, fid_PKCS7DesignatedName_o);
		name->ou = string_field_to_utfchars(ctx, env, desname, fid_PKCS7DesignatedName_ou);
		name->email = string_field_to_utfchars(ctx, env, desname, fid_PKCS7DesignatedName_email);
		name->c = string_field_to_utfchars(ctx, env, desname, fid_PKCS7DesignatedName_c);
	}
	fz_catch(ctx)
	{
		if (name) fz_free(ctx, name->c);
		if (name) fz_free(ctx, name->email);
		if (name) fz_free(ctx, name->ou);
		if (name) fz_free(ctx, name->o);
		if (name) fz_free(ctx, name->cn);
		fz_free(ctx, name);
		fz_rethrow_and_detach_thread(ctx, detach);
	}

	jni_detach_thread(detach);

	return name;
}

static size_t signer_max_digest_size(fz_context *ctx, pdf_pkcs7_signer *signer_)
{
	java_pkcs7_signer *signer = (java_pkcs7_signer *)signer_;
	jboolean detach = JNI_FALSE;
	size_t max_digest = 0;
	int len;

	JNIEnv *env = jni_attach_thread(ctx, &detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in signer_max_digest_size");

	/* get the size in bytes we should allow for the digest buffer */
	len = (*env)->CallIntMethod(env, signer->java_signer, mid_PKCS7Signer_maxDigest);
	if (len < 0)
		len = 0;
	max_digest = (size_t)len;
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);

	jni_detach_thread(detach);

	return max_digest;
}

static int signer_create_digest(fz_context *ctx, pdf_pkcs7_signer *signer_, fz_stream *stm, unsigned char *digest, size_t digest_len)
{
	java_pkcs7_signer *signer = (java_pkcs7_signer *)signer_;
	jobject jsigner = signer->java_signer;
	jboolean detach = JNI_FALSE;
	jobject jdigest;
	jobject jstm;
	int result = 1;

	JNIEnv *env = jni_attach_thread(ctx, &detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in signer_create_digest");

	jstm = to_FitzInputStream(ctx, env, stm);

	jdigest = (*env)->CallObjectMethod(env, jsigner, mid_PKCS7Signer_sign, jstm);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);
	if (!jdigest)
		fz_throw(ctx, FZ_ERROR_GENERIC, "did not receive digest from signer");

	if (digest != NULL)
	{
		jbyte *src = NULL;
		int srclen = 0;

		src = (*env)->GetByteArrayElements(env, jdigest, 0);
		if (src == NULL)
			fz_throw_and_detach_thread(ctx, detach, FZ_ERROR_GENERIC, "cannot get digest");

		srclen = (*env)->GetArrayLength(env, jdigest);

		if ((size_t)srclen > digest_len)
		{
			(*env)->ReleaseByteArrayElements(env, jdigest, src, JNI_ABORT);
			fz_throw_and_detach_thread(ctx, detach, FZ_ERROR_GENERIC, "digest destination shorter than digest");
		}

		memcpy(digest, src, srclen);
		result = srclen;

		(*env)->ReleaseByteArrayElements(env, jdigest, src, JNI_ABORT);
	}

	jni_detach_thread(detach);

	return result;
}

static pdf_pkcs7_signer *pdf_pkcs7_java_signer_create(JNIEnv *env, fz_context *ctx, jobject java_signer)
{
	java_pkcs7_signer *signer = Memento_label(fz_calloc(ctx, 1, sizeof(*signer)), "java_pkcs7_signer");

	if (signer == NULL) return NULL;

	signer->base.keep = signer_keep;
	signer->base.drop = signer_drop;
	signer->base.get_signing_name = signer_designated_name;
	signer->base.max_digest_size = signer_max_digest_size;
	signer->base.create_digest = signer_create_digest;
	signer->refs = 1;

	signer->java_signer = (*env)->NewGlobalRef(env, java_signer);
	if (signer->java_signer == NULL)
	{
		fz_free(ctx, signer);
		return NULL;
	}

	return &signer->base;
}

JNIEXPORT jlong JNICALL
FUN(PKCS7Signer_newNative)(JNIEnv *env, jclass cls, jobject jsigner)
{
	fz_context *ctx = get_context(env);
	pdf_pkcs7_signer *signer = NULL;

	if (!ctx) return 0;
	if (!jsigner) jni_throw_arg(env, "signer must not be null");

	jsigner = (*env)->NewGlobalRef(env, jsigner);
	if (!jsigner) jni_throw_arg(env, "unable to get reference to signer");

	fz_try(ctx)
		signer = pdf_pkcs7_java_signer_create(env, ctx, jsigner);
	fz_catch(ctx)
	{
		(*env)->DeleteGlobalRef(env, jsigner);
		jni_rethrow(env, ctx);
	}

	return jlong_cast(signer);
}


JNIEXPORT void JNICALL
FUN(PKCS7Signer_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_pkcs7_signer *signer = from_PKCS7Signer_safe(env, self);
	if (!ctx || !signer) return;
	(*env)->SetLongField(env, self, fid_PKCS7Signer_pointer, 0);
	pdf_drop_signer(ctx, signer);
}
