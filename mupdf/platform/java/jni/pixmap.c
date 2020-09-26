/* Pixmap interface */

JNIEXPORT void JNICALL
FUN(Pixmap_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap_safe(env, self);
	if (!ctx || !pixmap) return;
	(*env)->SetLongField(env, self, fid_Pixmap_pointer, 0);
	fz_drop_pixmap(ctx, pixmap);
}

JNIEXPORT jlong JNICALL
FUN(Pixmap_newNative)(JNIEnv *env, jobject self, jobject jcs, jint x, jint y, jint w, jint h, jboolean alpha)
{
	fz_context *ctx = get_context(env);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_pixmap *pixmap = NULL;

	if (!ctx || !cs) return 0;

	fz_try(ctx)
	{
		pixmap = fz_new_pixmap(ctx, cs, w, h, NULL, alpha);
		pixmap->x = x;
		pixmap->y = y;
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(pixmap);
}

JNIEXPORT void JNICALL
FUN(Pixmap_clear)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);

	if (!ctx || !pixmap) return;

	fz_try(ctx)
		fz_clear_pixmap(ctx, pixmap);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Pixmap_clearWithValue)(JNIEnv *env, jobject self, jint value)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);

	if (!ctx || !pixmap) return;

	fz_try(ctx)
		fz_clear_pixmap_with_value(ctx, pixmap, value);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Pixmap_saveAsPNG)(JNIEnv *env, jobject self, jstring jfilename)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	const char *filename = "null";

	if (!ctx || !pixmap) return;
	if (!jfilename) jni_throw_arg_void(env, "filename must not be null");

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return;

	fz_try(ctx)
		fz_save_pixmap_as_png(ctx, pixmap, filename);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(Pixmap_getX)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap ? pixmap->x : 0;
}

JNIEXPORT jint JNICALL
FUN(Pixmap_getY)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap ? pixmap->y : 0;
}

JNIEXPORT jint JNICALL
FUN(Pixmap_getWidth)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap ? pixmap->w : 0;
}

JNIEXPORT jint JNICALL
FUN(Pixmap_getHeight)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap ? pixmap->h : 0;
}

JNIEXPORT jint JNICALL
FUN(Pixmap_getNumberOfComponents)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap ? pixmap->n : 0;
}

JNIEXPORT jboolean JNICALL
FUN(Pixmap_getAlpha)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap && pixmap->alpha ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
FUN(Pixmap_getStride)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap ? pixmap->stride : 0;
}

JNIEXPORT jobject JNICALL
FUN(Pixmap_getColorSpace)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	fz_colorspace *cs;

	if (!ctx | !pixmap) return NULL;

	fz_try(ctx)
		cs = fz_pixmap_colorspace(ctx, pixmap);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_ColorSpace_safe(ctx, env, cs);
}

JNIEXPORT jbyteArray JNICALL
FUN(Pixmap_getSamples)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	int size = pixmap->h * pixmap->stride;
	jbyteArray arr;

	if (!ctx | !pixmap) return NULL;

	arr = (*env)->NewByteArray(env, size);
	if (!arr || (*env)->ExceptionCheck(env)) jni_throw_run(env, "cannot create byte array");

	(*env)->SetByteArrayRegion(env, arr, 0, size, (const jbyte *)pixmap->samples);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return arr;
}

JNIEXPORT jbyte JNICALL
FUN(Pixmap_getSample)(JNIEnv *env, jobject self, jint x, jint y, jint k)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);

	if (!ctx | !pixmap) return 0;

	if (x < 0 || x >= pixmap->w) jni_throw_oob(env, "x out of range");
	if (y < 0 || y >= pixmap->h) jni_throw_oob(env, "y out of range");
	if (k < 0 || k >= pixmap->n) jni_throw_oob(env, "k out of range");

	return pixmap->samples[(x + y * pixmap->w) * pixmap->n + k];
}

JNIEXPORT jintArray JNICALL
FUN(Pixmap_getPixels)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	int size = pixmap->w * pixmap->h;
	jintArray arr;

	if (!ctx | !pixmap) return NULL;

	if (pixmap->n != 4 || !pixmap->alpha)
		jni_throw_run(env, "invalid colorspace for getPixels (must be RGB/BGR with alpha)");
	if (size * 4 != pixmap->h * pixmap->stride)
		jni_throw_run(env, "invalid stride for getPixels");

	arr = (*env)->NewIntArray(env, size);
	if (!arr || (*env)->ExceptionCheck(env)) return NULL;

	(*env)->SetIntArrayRegion(env, arr, 0, size, (const jint *)pixmap->samples);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return arr;
}

JNIEXPORT jint JNICALL
FUN(Pixmap_getXResolution)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap ? pixmap->xres : 0;
}

JNIEXPORT jint JNICALL
FUN(Pixmap_getYResolution)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap ? pixmap->yres : 0;
}

JNIEXPORT void JNICALL
FUN(Pixmap_invert)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);

	if (!ctx || !pixmap) return;

	fz_try(ctx)
		fz_invert_pixmap(ctx, pixmap);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Pixmap_invertLuminance)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);

	if (!ctx || !pixmap) return;

	fz_try(ctx)
		fz_invert_pixmap_luminance(ctx, pixmap);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Pixmap_gamma)(JNIEnv *env, jobject self, jfloat gamma)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);

	if (!ctx || !pixmap) return;

	fz_try(ctx)
		fz_gamma_pixmap(ctx, pixmap, gamma);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Pixmap_tint)(JNIEnv *env, jobject self, jint black, jint white)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);

	if (!ctx || !pixmap) return;

	fz_try(ctx)
		fz_tint_pixmap(ctx, pixmap, black, white);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}
