/* DrawDevice interface */

JNIEXPORT jlong JNICALL
FUN(DrawDevice_newNative)(JNIEnv *env, jclass cls, jobject jpixmap)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, jpixmap);
	fz_device *device = NULL;

	if (!ctx) return 0;
	if (!pixmap) jni_throw_arg(env, "pixmap must not be null");

	fz_try(ctx)
		device = fz_new_draw_device(ctx, fz_identity, pixmap);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(device);
}
