/* DisplayListDevice interface */

JNIEXPORT jlong JNICALL
FUN(DisplayListDevice_newNative)(JNIEnv *env, jclass cls, jobject jlist)
{
	fz_context *ctx = get_context(env);
	fz_display_list *list = from_DisplayList(env, jlist);
	fz_device *device = NULL;

	if (!ctx) return 0;
	if (!list) return jni_throw_arg(env, "list must not be null"), 0;

	fz_var(device);

	fz_try(ctx)
		device = fz_new_list_device(ctx, list);
	fz_catch(ctx)
		return jni_rethrow(env, ctx), 0;

	return jlong_cast(device);
}
