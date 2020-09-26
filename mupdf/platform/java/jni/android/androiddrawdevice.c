/* AndroidDrawDevice interface */

static jlong
newNativeAndroidDrawDevice(JNIEnv *env, jobject self, fz_context *ctx, jobject obj, jint width, jint height, NativeDeviceLockFn *lock, NativeDeviceUnlockFn *unlock, jint xOrigin, jint yOrigin, jint patchX0, jint patchY0, jint patchX1, jint patchY1)
{
	fz_device *device = NULL;
	fz_pixmap *pixmap = NULL;
	unsigned char dummy;
	NativeDeviceInfo *ninfo = NULL;
	NativeDeviceInfo *info;
	fz_irect bbox;
	int err;

	if (!ctx) return 0;

	/* Ensure patch fits inside bitmap. */
	if (patchX0 < 0) patchX0 = 0;
	if (patchY0 < 0) patchY0 = 0;
	if (patchX1 > width) patchX1 = width;
	if (patchY1 > height) patchY1 = height;

	bbox.x0 = xOrigin + patchX0;
	bbox.y0 = yOrigin + patchY0;
	bbox.x1 = xOrigin + patchX1;
	bbox.y1 = yOrigin + patchY1;

	fz_var(pixmap);
	fz_var(ninfo);

	fz_try(ctx)
	{
		pixmap = fz_new_pixmap_with_bbox_and_data(ctx, fz_device_rgb(ctx), bbox, NULL, 1, &dummy);
		pixmap->stride = width * sizeof(int32_t);
		ninfo = Memento_label(fz_malloc(ctx, sizeof(*ninfo)), "newNativeAndroidDrawDevice");
		ninfo->pixmap = pixmap;
		ninfo->lock = lock;
		ninfo->unlock = unlock;
		ninfo->xOffset = patchX0;
		ninfo->yOffset = patchY0;
		ninfo->width = width;
		ninfo->height = height;
		ninfo->object = obj;
		(*env)->SetLongField(env, self, fid_NativeDevice_nativeInfo, jlong_cast(ninfo));
		(*env)->SetObjectField(env, self, fid_NativeDevice_nativeResource, obj);
		info = lockNativeDevice(env,self,&err);
		if (!err)
		{
			fz_clear_pixmap_with_value(ctx, pixmap, 0xff);
			unlockNativeDevice(env,ninfo);
			device = fz_new_draw_device(ctx, fz_identity, pixmap);
		}
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pixmap);
		fz_free(ctx, ninfo);
		jni_rethrow(env, ctx);
	}

	/* lockNativeDevice will already have raised a JNI error if there was one. */
	if (err)
	{
		fz_drop_pixmap(ctx, pixmap);
		fz_free(ctx, ninfo);
		return 0;
	}

	return jlong_cast(device);
}

static int androidDrawDevice_lock(JNIEnv *env, NativeDeviceInfo *info)
{
	uint8_t *pixels;
	int ret;
	int phase = 0;
	fz_context *ctx = get_context(env);
	size_t size = info->width * info->height * 4;

	if (!ctx) jni_throw_run(env, "no context in DrawDevice call");

	assert(info);
	assert(info->object);

	while (1)
	{
		ret = AndroidBitmap_lockPixels(env, info->object, (void **)&pixels);
		if (ret == ANDROID_BITMAP_RESULT_SUCCESS)
			break;
		if (!fz_store_scavenge_external(ctx, size, &phase))
			break; /* Failed to free any */
	}
	if (ret != ANDROID_BITMAP_RESULT_SUCCESS)
	{
		info->pixmap->samples = NULL;
		jni_throw_run(env, "bitmap lock failed in DrawDevice call");
	}

	/* Now offset pixels to allow for the page offsets */
	pixels += sizeof(int32_t) * (info->xOffset + info->width * info->yOffset);

	info->pixmap->samples = pixels;

	return 0;
}

static void androidDrawDevice_unlock(JNIEnv *env, NativeDeviceInfo *info)
{
	assert(info);
	assert(info->object);

	info->pixmap->samples = NULL;
	if (AndroidBitmap_unlockPixels(env, info->object) != ANDROID_BITMAP_RESULT_SUCCESS)
		jni_throw_run_void(env, "bitmap unlock failed in DrawDevice call");
}

JNIEXPORT void JNICALL
FUN(android_AndroidDrawDevice_invertLuminance)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;

	fz_try(ctx)
		fz_invert_pixmap_luminance(ctx, info->pixmap);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jlong JNICALL
FUN(android_AndroidDrawDevice_newNative)(JNIEnv *env, jclass self, jobject jbitmap, jint xOrigin, jint yOrigin, jint pX0, jint pY0, jint pX1, jint pY1)
{
	fz_context *ctx = get_context(env);
	AndroidBitmapInfo info;
	jlong device = 0;
	int ret;

	if (!ctx) return 0;
	if (!jbitmap) jni_throw_arg(env, "bitmap must not be null");

	if ((ret = AndroidBitmap_getInfo(env, jbitmap, &info)) != ANDROID_BITMAP_RESULT_SUCCESS)
		jni_throw_run(env, "new DrawDevice failed to get bitmap info");
	if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888)
		jni_throw_run(env, "new DrawDevice failed as bitmap format is not RGBA_8888");
	if (info.stride != info.width * 4)
		jni_throw_run(env, "new DrawDevice failed as bitmap width != stride");

	fz_try(ctx)
		device = newNativeAndroidDrawDevice(env, self, ctx, jbitmap, info.width, info.height, androidDrawDevice_lock, androidDrawDevice_unlock, xOrigin, yOrigin, pX0, pY0, pX1, pY1);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return device;
}
