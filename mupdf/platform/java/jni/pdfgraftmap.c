/* PDFGraftMap interface */

JNIEXPORT void JNICALL
FUN(PDFGraftMap_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_graft_map *map = from_PDFGraftMap_safe(env, self);
	if (!ctx || !map) return;
	(*env)->SetLongField(env, self, fid_PDFGraftMap_pointer, 0);
	pdf_drop_graft_map(ctx, map);
}

JNIEXPORT jobject JNICALL
FUN(PDFGraftMap_graftObject)(JNIEnv *env, jobject self, jobject jobj)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, jobj);
	pdf_graft_map *map = from_PDFGraftMap(env, self);

	if (!ctx || !map) return NULL;
	if (!obj) return jni_throw_arg(env, "object must not be null"), NULL;

	fz_try(ctx)
		obj = pdf_graft_mapped_object(ctx, map, obj);
	fz_catch(ctx)
		return jni_rethrow(env, ctx), NULL;

	return to_PDFObject_safe_own(ctx, env, self, obj);
}
