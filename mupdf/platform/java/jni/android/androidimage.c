/* AndroidImage interface */

JNIEXPORT jlong JNICALL
FUN(AndroidImage_newImageFromBitmap)(JNIEnv *env, jobject self, jobject jbitmap, jlong jmask)
{
	fz_context *ctx = get_context(env);
	fz_image *mask = CAST(fz_image *, jmask);
	fz_image *image = NULL;
	fz_pixmap *pixmap = NULL;
	AndroidBitmapInfo info;
	void *pixels;
	int ret;

	if (!ctx) return 0;
	if (!jbitmap) jni_throw_arg(env, "bitmap must not be null");

	if (mask && mask->mask)
		jni_throw_run(env, "new Image failed as mask cannot be masked");
	if ((ret = AndroidBitmap_getInfo(env, jbitmap, &info)) != ANDROID_BITMAP_RESULT_SUCCESS)
		jni_throw_run(env, "new Image failed to get bitmap info");
	if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888)
		jni_throw_run(env, "new Image failed as bitmap format is not RGBA_8888");
	if (info.stride != info.width)
		jni_throw_run(env, "new Image failed as bitmap width != stride");

	fz_var(pixmap);

	fz_try(ctx)
	{
		int ret;
		int phase = 0;
		size_t size = info.width * info.height * 4;
		pixmap = fz_new_pixmap(ctx, fz_device_rgb(ctx), info.width, info.height, NULL, 1);
		while (1)
		{
			ret = AndroidBitmap_lockPixels(env, jbitmap, (void **)&pixels);
			if (ret == ANDROID_BITMAP_RESULT_SUCCESS)
				break;
			if (!fz_store_scavenge_external(ctx, size, &phase))
				break; /* Failed to free any */
		}
		if (ret != ANDROID_BITMAP_RESULT_SUCCESS)
			fz_throw(ctx, FZ_ERROR_GENERIC, "bitmap lock failed in new Image");
		memcpy(pixmap->samples, pixels, info.width * info.height * 4);
		if (AndroidBitmap_unlockPixels(env, jbitmap) != ANDROID_BITMAP_RESULT_SUCCESS)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Bitmap unlock failed in new Image");
		image = fz_new_image_from_pixmap(ctx, fz_keep_pixmap(ctx, pixmap), fz_keep_image(ctx, mask));
	}
	fz_always(ctx)
		fz_drop_pixmap(ctx, pixmap);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(image);
}
