/* Cookie interface */

JNIEXPORT void JNICALL
FUN(Cookie_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_cookie *cookie = from_Cookie_safe(env, self);
	if (!ctx || !cookie) return;
	(*env)->SetLongField(env, self, fid_Cookie_pointer, 0);
	fz_free(ctx, cookie);
}

JNIEXPORT jlong JNICALL
FUN(Cookie_newNative)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_cookie *cookie = NULL;

	if (!ctx) return 0;

	fz_try(ctx)
		cookie = fz_malloc_struct(ctx, fz_cookie);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(cookie);
}

JNIEXPORT void JNICALL
FUN(Cookie_abort)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_cookie *cookie = from_Cookie(env, self);

	if (!ctx || !cookie) return;

	cookie->abort = 1;
}
