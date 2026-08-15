// Copyright (C) 2004-2025 Artifex Software, Inc.
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

JNIEXPORT jlong JNICALL
FUN(Pixmap_newNativeFromColorAndMask)(JNIEnv *env, jobject self, jobject jcolor, jobject jmask)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *color = from_Pixmap_safe(env, jcolor);
	fz_pixmap *mask = from_Pixmap_safe(env, jmask);
	fz_pixmap *pixmap = NULL;

	if (!ctx) return 0;
	if (!jcolor) jni_throw_arg(env, "color must not be null");
	if (!jmask) jni_throw_arg(env, "mask must not be null");

	fz_try(ctx)
		pixmap = fz_new_pixmap_from_color_and_mask(ctx, color, mask);
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

JNIEXPORT jobject JNICALL
FUN(Pixmap_asPNG)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	fz_buffer *buf = NULL;

	if (!ctx || !pixmap) return NULL;

	fz_try(ctx)
		buf = fz_new_buffer_from_pixmap_as_png(ctx, pixmap, fz_default_color_params);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Buffer_safe_own(ctx, env, buf);
}

JNIEXPORT jobject JNICALL
FUN(Pixmap_asJPEG)(JNIEnv *env, jobject self, jint quality, jboolean invert_cmyk)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	fz_buffer *buf = NULL;

	if (!ctx || !pixmap) return NULL;

	fz_try(ctx)
		buf = fz_new_buffer_from_pixmap_as_jpeg(ctx, pixmap, fz_default_color_params, quality, invert_cmyk);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Buffer_safe_own(ctx, env, buf);
}

JNIEXPORT jobject JNICALL
FUN(Pixmap_asPAM)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	fz_buffer *buf = NULL;

	if (!ctx || !pixmap) return NULL;

	fz_try(ctx)
		buf = fz_new_buffer_from_pixmap_as_pam(ctx, pixmap, fz_default_color_params);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Buffer_safe_own(ctx, env, buf);
}

JNIEXPORT jobject JNICALL
FUN(Pixmap_asPNM)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	fz_buffer *buf = NULL;

	if (!ctx || !pixmap) return NULL;

	fz_try(ctx)
		buf = fz_new_buffer_from_pixmap_as_pnm(ctx, pixmap, fz_default_color_params);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Buffer_safe_own(ctx, env, buf);
}

JNIEXPORT jobject JNICALL
FUN(Pixmap_asPBM)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	fz_buffer *buf = NULL;

	if (!ctx || !pixmap) return NULL;

	fz_try(ctx)
		buf = fz_new_buffer_from_pixmap_as_pbm(ctx, pixmap, fz_default_color_params);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Buffer_safe_own(ctx, env, buf);
}

JNIEXPORT jobject JNICALL
FUN(Pixmap_asPKM)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	fz_buffer *buf = NULL;

	if (!ctx || !pixmap) return NULL;

	fz_try(ctx)
		buf = fz_new_buffer_from_pixmap_as_pkm(ctx, pixmap, fz_default_color_params);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Buffer_safe_own(ctx, env, buf);
}

JNIEXPORT jobject JNICALL
FUN(Pixmap_asJPX)(JNIEnv *env, jobject self, jint quality)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	fz_buffer *buf = NULL;

	if (!ctx || !pixmap) return NULL;

	fz_try(ctx)
		buf = fz_new_buffer_from_pixmap_as_jpx(ctx, pixmap, fz_default_color_params, quality);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Buffer_safe_own(ctx, env, buf);
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

JNIEXPORT void JNICALL
FUN(Pixmap_saveAsJPEG)(JNIEnv *env, jobject self, jstring jfilename, jint quality)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	const char *filename = "null";

	if (!ctx || !pixmap) return;
	if (!jfilename) jni_throw_arg_void(env, "filename must not be null");

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return;

	fz_try(ctx)
		fz_save_pixmap_as_jpeg(ctx, pixmap, filename, quality);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Pixmap_saveAsPAM)(JNIEnv *env, jobject self, jstring jfilename)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	const char *filename = "null";

	if (!ctx || !pixmap) return;
	if (!jfilename) jni_throw_arg_void(env, "filename must not be null");

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return;

	fz_try(ctx)
		fz_save_pixmap_as_pam(ctx, pixmap, filename);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Pixmap_saveAsPNM)(JNIEnv *env, jobject self, jstring jfilename)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	const char *filename = "null";

	if (!ctx || !pixmap) return;
	if (!jfilename) jni_throw_arg_void(env, "filename must not be null");

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return;

	fz_try(ctx)
		fz_save_pixmap_as_pnm(ctx, pixmap, filename);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Pixmap_saveAsPBM)(JNIEnv *env, jobject self, jstring jfilename)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	const char *filename = "null";

	if (!ctx || !pixmap) return;
	if (!jfilename) jni_throw_arg_void(env, "filename must not be null");

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return;

	fz_try(ctx)
		fz_save_pixmap_as_pbm(ctx, pixmap, filename);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Pixmap_saveAsPKM)(JNIEnv *env, jobject self, jstring jfilename)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	const char *filename = "null";

	if (!ctx || !pixmap) return;
	if (!jfilename) jni_throw_arg_void(env, "filename must not be null");

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return;

	fz_try(ctx)
		fz_save_pixmap_as_pkm(ctx, pixmap, filename);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Pixmap_saveAsJPX)(JNIEnv *env, jobject self, jstring jfilename, jint quality)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	const char *filename = "null";

	if (!ctx || !pixmap) return;
	if (!jfilename) jni_throw_arg_void(env, "filename must not be null");

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return;

	fz_try(ctx)
		fz_save_pixmap_as_jpx(ctx, pixmap, filename, quality);
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

JNIEXPORT void JNICALL
FUN(Pixmap_setResolution)(JNIEnv *env, jobject self, jint xres, jint yres)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);

	if (!ctx || !pixmap) return;

	fz_try(ctx)
		fz_set_pixmap_resolution(ctx, pixmap, xres, yres);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(Pixmap_convertToColorSpace)(JNIEnv *env, jobject self, jobject jcs, jobject jproof, jobject jdefaultcs, jint jcolorparams, jboolean keep_alpha)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_colorspace *proof = from_ColorSpace(env, jproof);
	fz_default_colorspaces *default_cs = from_DefaultColorSpaces(env, jdefaultcs);
	fz_color_params color_params = from_ColorParams_safe(env, jcolorparams);
	fz_pixmap *dst = NULL;

	if (!ctx || !pixmap) return NULL;
	if (!cs) jni_throw_arg(env, "destination colorspace must not be null");

	fz_try(ctx)
		dst = fz_convert_pixmap(ctx, pixmap, cs, proof, default_cs, color_params, keep_alpha);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Pixmap_safe_own(ctx, env, dst);
}

JNIEXPORT jobject JNICALL
FUN(Pixmap_computeMD5)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	unsigned char digest[16] = { 0 };
	jbyteArray arr;

	if (!ctx || !pixmap) return NULL;

	fz_try(ctx)
		fz_md5_pixmap(ctx, pixmap, digest);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	arr = (*env)->NewByteArray(env, 16);
	if (!arr || (*env)->ExceptionCheck(env)) jni_throw_run(env, "cannot create byte array");

	(*env)->SetByteArrayRegion(env, arr, 0, 16, (const jbyte *)digest);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return arr;
}

JNIEXPORT jlong JNICALL
FUN(Pixmap_newNativeDeskew)(JNIEnv *env, jobject self, jfloat ang, jint border)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	fz_pixmap *dest;

	if (!ctx || !pixmap) return 0;

	fz_try(ctx)
		dest = fz_deskew_pixmap(ctx, pixmap, ang, border);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(dest);
}

JNIEXPORT jfloat JNICALL
FUN(Pixmap_detectSkew)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	float ang;

	if (!ctx || !pixmap) return 0;

	fz_try(ctx)
		ang = fz_detect_skew(ctx, pixmap);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (jfloat)ang;
}

JNIEXPORT jfloatArray JNICALL
FUN(Pixmap_detectDocument)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	fz_quad points = { 0 };
	int found = 0;

	if (!ctx || !pixmap) return NULL;

	fz_try(ctx)
		found = fz_detect_document(ctx, &points, pixmap);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	if (!found)
		return NULL;

	return to_Quad_safe(ctx, env, points);
}

JNIEXPORT jobject JNICALL
FUN(Pixmap_warp)(JNIEnv *env, jobject self, jobject jpoints, jint width, jint height)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	fz_quad points = from_Quad(env, jpoints);
	fz_pixmap *dest = NULL;

	if (!ctx || !pixmap) return NULL;
	if (!jpoints) jni_throw_arg(env, "points not be null");

	fz_try(ctx)
		dest = fz_warp_pixmap(ctx, pixmap, points, width, height);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Pixmap_safe_own(ctx, env, dest);
}

JNIEXPORT jobject JNICALL
FUN(Pixmap_autowarp)(JNIEnv *env, jobject self, jobject jpoints)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	fz_quad points = from_Quad(env, jpoints);
	fz_pixmap *dest = NULL;

	if (!ctx || !pixmap) return NULL;
	if (!jpoints) jni_throw_arg(env, "points not be null");

	fz_try(ctx)
		dest = fz_autowarp_pixmap(ctx, pixmap, points);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Pixmap_safe_own(ctx, env, dest);
}

JNIEXPORT jobject JNICALL
FUN(Pixmap_decodeBarcode)(JNIEnv *env, jobject self, jfloat rotate)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	fz_barcode_type type = FZ_BARCODE_NONE;
	char *contents = NULL;
	jobject jcontents;
	jobject jbarcodeinfo;

	if (!ctx || !pixmap)
		return NULL;

	fz_try(ctx)
		contents = fz_decode_barcode_from_pixmap(ctx, &type, pixmap, rotate);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	jcontents = (*env)->NewStringUTF(env, contents);
	fz_free(ctx, contents);
	if (!jcontents || (*env)->ExceptionCheck(env))
		return NULL;

	jbarcodeinfo = (*env)->NewObject(env, cls_BarcodeInfo, mid_BarcodeInfo_init, type, jcontents);
	if (!jbarcodeinfo || (*env)->ExceptionCheck(env))
		return NULL;

	return jbarcodeinfo;
}

JNIEXPORT jobject JNICALL
FUN(Pixmap_encodeBarcode)(JNIEnv *env, jobject self, jint barcode_type, jstring jcontents, jint size, jint ec, jboolean quiet, jboolean hrt)
{
	fz_context *ctx = get_context(env);
	const char *contents = NULL;
	fz_pixmap *pix = NULL;

	if (!ctx)
		return NULL;

	contents = (*env)->GetStringUTFChars(env, jcontents, NULL);
	if (!contents) jni_throw_run(env, "cannot get characters in contents string");

	fz_try(ctx)
		pix = fz_new_barcode_pixmap(ctx, barcode_type, contents, size, ec, quiet, hrt);
	fz_always(ctx)
		if (contents)
			(*env)->ReleaseStringUTFChars(env, jcontents, contents);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Pixmap_safe_own(ctx, env, pix);
}
