/* Shade interface */

JNIEXPORT void JNICALL
FUN(Shade_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_shade *shd = from_Shade_safe(env, self);
	if (!ctx || !shd) return;
	(*env)->SetLongField(env, self, fid_Shade_pointer, 0);
	fz_drop_shade(ctx, shd);
}
