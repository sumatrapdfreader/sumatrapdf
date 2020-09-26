/* Image interface */

JNIEXPORT void JNICALL
FUN(Image_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_image *image = from_Image_safe(env, self);
	if (!ctx || !image) return;
	(*env)->SetLongField(env, self, fid_Image_pointer, 0);
	fz_drop_image(ctx, image);
}

JNIEXPORT jlong JNICALL
FUN(Image_newNativeFromPixmap)(JNIEnv *env, jobject self, jobject jpixmap)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, jpixmap);
	fz_image *image = NULL;

	if (!ctx) return 0;
	if (!pixmap) jni_throw_arg(env, "pixmap must not be null");

	fz_try(ctx)
		image = fz_new_image_from_pixmap(ctx, pixmap, NULL);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(image);
}

JNIEXPORT jlong JNICALL
FUN(Image_newNativeFromFile)(JNIEnv *env, jobject self, jstring jfilename)
{
	fz_context *ctx = get_context(env);
	const char *filename = "null";
	fz_image *image = NULL;

	if (!ctx) return 0;
	if (!jfilename) jni_throw_arg(env, "filename must not be null");

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return 0;

	fz_try(ctx)
		image = fz_new_image_from_file(ctx, filename);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(image);
}

JNIEXPORT jint JNICALL
FUN(Image_getWidth)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	return image ? image->w : 0;
}

JNIEXPORT jint JNICALL
FUN(Image_getHeight)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	return image ? image->h : 0;
}

JNIEXPORT jint JNICALL
FUN(Image_getNumberOfComponents)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	return image ? image->n : 0;
}

JNIEXPORT jobject JNICALL
FUN(Image_getColorSpace)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_image *image = from_Image(env, self);
	if (!ctx || !image) return NULL;
	return to_ColorSpace_safe(ctx, env, image->colorspace);
}

JNIEXPORT jint JNICALL
FUN(Image_getBitsPerComponent)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	return image ? image->bpc : 0;
}

JNIEXPORT jint JNICALL
FUN(Image_getXResolution)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	return image ? image->xres : 0;
}

JNIEXPORT jint JNICALL
FUN(Image_getYResolution)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	return image ? image->yres : 0;
}

JNIEXPORT jboolean JNICALL
FUN(Image_getImageMask)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	return image && image->imagemask ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(Image_getInterpolate)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	return image && image->interpolate ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobject JNICALL
FUN(Image_getMask)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_image *img = from_Image(env, self);

	if (!ctx || !img) return NULL;

	return to_Image_safe(ctx, env, img->mask);
}

JNIEXPORT jobject JNICALL
FUN(Image_toPixmap)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_image *img = from_Image(env, self);
	fz_pixmap *pixmap = NULL;

	if (!ctx || !img) return NULL;

	fz_try(ctx)
		pixmap = fz_get_pixmap_from_image(ctx, img, NULL, NULL, NULL, NULL);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Pixmap_safe_own(ctx, env, pixmap);
}
