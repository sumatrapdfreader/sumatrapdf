/* FitzInputStream interface */

JNIEXPORT jboolean JNICALL
FUN(FitzInputStream_markSupported)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_stream *stm = from_FitzInputStream_safe(env, self);
	jboolean closed = JNI_TRUE;

	if (!ctx || !stm) return JNI_FALSE;

	closed = (*env)->GetBooleanField(env, self, fid_FitzInputStream_closed);
	if (closed) jni_throw_uoe(env, "stream closed");

	return stm->seek != NULL;
}

JNIEXPORT void JNICALL
FUN(FitzInputStream_mark)(JNIEnv *env, jobject self, jint readlimit)
{
	fz_context *ctx = get_context(env);
	fz_stream *stm = from_FitzInputStream_safe(env, self);
	jlong markpos = 0;
	jboolean closed = JNI_TRUE;

	if (!ctx || !stm) return;
	if (stm->seek == NULL) jni_throw_uoe_void(env, "mark not supported");

	closed = (*env)->GetBooleanField(env, self, fid_FitzInputStream_closed);
	if (closed) jni_throw_uoe_void(env, "stream closed");

	fz_try(ctx)
		markpos = fz_tell(ctx, stm);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);

	(*env)->SetLongField(env, self, fid_FitzInputStream_markpos, markpos);
}

JNIEXPORT void JNICALL
FUN(FitzInputStream_reset)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_stream *stm = from_FitzInputStream_safe(env, self);
	jboolean closed = JNI_TRUE;
	jlong markpos = -1;

	if (!ctx || !stm) return;

	if (stm->seek == NULL) jni_throw_uoe_void(env, "reset not supported");
	closed = (*env)->GetBooleanField(env, self, fid_FitzInputStream_closed);
	if (closed) jni_throw_uoe_void(env, "stream closed");

	markpos = (*env)->GetLongField(env, self, fid_FitzInputStream_markpos);
	if (markpos < 0) jni_throw_io_void(env, "mark not set");

	fz_try(ctx)
		fz_seek(ctx, stm, markpos, SEEK_SET);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(FitzInputStream_available)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_stream *stm = from_FitzInputStream_safe(env, self);
	jint available = 0;
	jboolean closed = JNI_TRUE;

	if (!ctx || !stm) return -1;

	closed = (*env)->GetBooleanField(env, self, fid_FitzInputStream_closed);
	if (closed) jni_throw_uoe(env, "stream closed");

	fz_try(ctx)
		available = fz_available(ctx, stm, 1);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return available;
}

JNIEXPORT jint JNICALL
FUN(FitzInputStream_readByte)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_stream *stm = from_FitzInputStream_safe(env, self);
	jboolean closed = JNI_TRUE;
	jbyte b = 0;

	if (!ctx || !stm) return -1;

	closed = (*env)->GetBooleanField(env, self, fid_FitzInputStream_closed);
	if (closed) jni_throw_uoe(env, "stream closed");

	fz_try(ctx)
		b = fz_read_byte(ctx, stm);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return b;
}

JNIEXPORT jint JNICALL
FUN(FitzInputStream_readArray)(JNIEnv *env, jobject self, jobject jarr, jint off, jint len)
{
	fz_context *ctx = get_context(env);
	fz_stream *stm = from_FitzInputStream_safe(env, self);
	jboolean closed = JNI_TRUE;
	jbyte *arr = NULL;
	jint n = 0;

	if (!ctx || !stm) return -1;
	if (!jarr) jni_throw_arg(env, "buffer must not be null");

	closed = (*env)->GetBooleanField(env, self, fid_FitzInputStream_closed);
	if (closed) jni_throw_uoe(env, "stream closed");

	arr = (*env)->GetByteArrayElements(env, jarr, NULL);
	if (!arr) jni_throw_arg(env, "cannot get buffer to read into");

	fz_try(ctx)
		n = fz_read(ctx, stm, (unsigned char *) arr + off, len);
	fz_always(ctx)
		(*env)->ReleaseByteArrayElements(env, jarr, arr, 0);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return n;
}

JNIEXPORT void JNICALL
FUN(FitzInputStream_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_stream *stm = from_FitzInputStream_safe(env, self);
	if (!ctx || !stm) return;
	(*env)->SetLongField(env, self, fid_FitzInputStream_pointer, 0);
	fz_drop_stream(ctx, stm);
}

JNIEXPORT void JNICALL
FUN(FitzInputStream_close)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_stream *stm = from_FitzInputStream_safe(env, self);
	jboolean closed = JNI_TRUE;

	if (!ctx || !stm) return;

	closed = (*env)->GetBooleanField(env, self, fid_FitzInputStream_closed);
	if (closed) jni_throw_uoe_void(env, "stream closed");

	(*env)->SetBooleanField(env, self, fid_FitzInputStream_closed, JNI_TRUE);
}
