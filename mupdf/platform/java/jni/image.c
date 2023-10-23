// Copyright (C) 2004-2023 Artifex Software, Inc.
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

JNIEXPORT jlong JNICALL
FUN(Image_newNativeFromBytes)(JNIEnv *env, jobject self, jbyteArray jByteArray)
{
	fz_context *ctx = get_context(env);
	fz_image *image = NULL;
	jbyte *bytes = NULL;
	fz_buffer *buffer = NULL;
	int count;

	if (!ctx) return 0;
	if (!jByteArray) jni_throw_arg(env, "jByteArray must not be null");

	count = (*env)->GetArrayLength(env, jByteArray);
	bytes = (*env)->GetByteArrayElements(env, jByteArray, NULL);
	if (!bytes)
		jni_throw_run(env, "cannot get buffer");

	fz_var(buffer);
	fz_try(ctx) {
		buffer = fz_new_buffer_from_copied_data(ctx, (unsigned char *) bytes, count);
		image = fz_new_image_from_buffer(ctx, buffer);
	}
	fz_always(ctx) {
		fz_drop_buffer(ctx, buffer);
		if (bytes) (*env)->ReleaseByteArrayElements(env, jByteArray, bytes, 0);
	}
	fz_catch(ctx) {
		jni_rethrow(env, ctx);
	}

	return jlong_cast(image);
}

JNIEXPORT jlong JNICALL
FUN(Image_newNativeFromBuffer)(JNIEnv *env, jobject self, jobject jbuffer)
{
	fz_context *ctx = get_context(env);
	fz_image *image = NULL;
	fz_buffer *buffer = from_Buffer_safe(env, jbuffer);

	if (!ctx) return 0;
	if (!jbuffer) jni_throw_arg(env, "buffer must not be null");

	fz_try(ctx)
		image = fz_new_image_from_buffer(ctx, buffer);
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
	int xres = 0;
	fz_image_resolution(image, &xres, NULL);
	return xres;
}

JNIEXPORT jint JNICALL
FUN(Image_getYResolution)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	int yres = 0;
	fz_image_resolution(image, NULL, &yres);
	return yres;
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

JNIEXPORT jint JNICALL
FUN(Image_getOrientation)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_image *image = from_Image(env, self);
	return fz_image_orientation(ctx, image);
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

JNIEXPORT jintArray JNICALL
FUN(Image_getColorKey)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_image *img = from_Image(env, self);
	int colorkey[FZ_MAX_COLORS * 2];

	if (!ctx || !img) return NULL;

	if (!img->use_colorkey)
		return NULL;

	memcpy(colorkey, img->colorkey, 2 * img->n * sizeof(int));
	return to_intArray(ctx, env, colorkey, 2 * img->n);
}

JNIEXPORT void JNICALL
FUN(Image_setOrientation)(JNIEnv *env, jobject self, jint orientation)
{
	fz_image *img = from_Image(env, self);
	if (!img) return;
	if (orientation < 0 || orientation > 8) jni_throw_oob_void(env, "orientation out of range");
	img->orientation = orientation;
}

JNIEXPORT jfloatArray JNICALL
FUN(Image_getDecode)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_image *img = from_Image(env, self);
	float decode[FZ_MAX_COLORS * 2];

	if (!ctx || !img) return NULL;

	if (!img->use_decode)
		return NULL;

	memcpy(decode, img->decode, 2 * img->n * sizeof(float));
	return to_floatArray(ctx, env, decode, 2 * img->n);
}
