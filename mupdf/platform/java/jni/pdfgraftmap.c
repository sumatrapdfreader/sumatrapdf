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
	if (!obj) jni_throw_arg(env, "object must not be null");

	fz_try(ctx)
		obj = pdf_graft_mapped_object(ctx, map, obj);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe_own(ctx, env, obj);
}

JNIEXPORT void JNICALL
FUN(PDFGraftMap_graftPage)(JNIEnv *env, jobject self, jint pageTo, jobject jobj, jint pageFrom)
{
	fz_context *ctx = get_context(env);
	pdf_document *src = from_PDFDocument(env, jobj);
	pdf_graft_map *map = from_PDFGraftMap(env, self);

	if (!ctx || !map) return;
	if (!src) jni_throw_arg_void(env, "Source Document must not be null");

	fz_try(ctx)
		pdf_graft_mapped_page(ctx, map, pageTo, src, pageFrom);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}
