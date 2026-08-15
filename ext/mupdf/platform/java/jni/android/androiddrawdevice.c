// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

/* AndroidDrawDevice interface */

static jlong
newNativeAndroidDrawDevice(JNIEnv *env, jobject self, fz_context *ctx, jobject obj, jint width, jint height, NativeDeviceLockFn *lock, NativeDeviceUnlockFn *unlock, jint xOrigin, jint yOrigin, jint patchX0, jint patchY0, jint patchX1, jint patchY1, jboolean clear)
{
	fz_device *device = NULL;
	fz_pixmap *pixmap = NULL;
	unsigned char dummy;
	NativeDeviceInfo *ninfo = NULL;
	NativeDeviceInfo *info;
	fz_irect bbox;
	int err = 0;

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
		if (clear)
		{
			info = lockNativeDevice(env,self,&err);
			if (!err)
			{
				fz_clear_pixmap_with_value(ctx, pixmap, 0xff);
				unlockNativeDevice(env,ninfo);
			}
		}
		if (!err)
			device = fz_new_draw_device(ctx, fz_identity, pixmap);
	}
	fz_catch(ctx)
	{
		(*env)->SetLongField(env, self, fid_NativeDevice_nativeInfo, 0);
		(*env)->SetObjectField(env, self, fid_NativeDevice_nativeResource, NULL);
		fz_drop_pixmap(ctx, pixmap);
		fz_free(ctx, ninfo);
		jni_rethrow(env, ctx);
	}

	/* lockNativeDevice will already have raised a JNI error if there was one. */
	if (err)
	{
		jthrowable t = (*env)->ExceptionOccurred(env);
		(*env)->ExceptionClear(env);
		(*env)->SetLongField(env, self, fid_NativeDevice_nativeInfo, 0);
		(*env)->SetObjectField(env, self, fid_NativeDevice_nativeResource, NULL);
		fz_drop_pixmap(ctx, pixmap);
		fz_free(ctx, ninfo);
		if ((*env)->Throw(env, t) < 0)
			(*env)->ThrowNew(env, cls_RuntimeException, "could not rethrow exception after cleanup when locking failed");
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

	if (!ctx)
	{
		jni_throw_run_imp(env, "no context in DrawDevice call");
		return 1;
	}

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
		jni_throw_run_imp(env, "bitmap lock failed in DrawDevice call");
		return 1;
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
FUN(android_AndroidDrawDevice_newNative)(JNIEnv *env, jclass self, jobject jbitmap, jint xOrigin, jint yOrigin, jint pX0, jint pY0, jint pX1, jint pY1, jboolean clear)
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
		device = newNativeAndroidDrawDevice(env, self, ctx, jbitmap, info.width, info.height, androidDrawDevice_lock, androidDrawDevice_unlock, xOrigin, yOrigin, pX0, pY0, pX1, pY1, clear);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return device;
}
