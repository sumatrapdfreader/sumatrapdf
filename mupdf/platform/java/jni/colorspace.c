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

/* ColorSpace interface */

JNIEXPORT void JNICALL
FUN(ColorSpace_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_colorspace *cs = from_ColorSpace_safe(env, self);
	if (!ctx || !cs) return;
	(*env)->SetLongField(env, self, fid_ColorSpace_pointer, 0);
	fz_drop_colorspace(ctx, cs);
}

JNIEXPORT jint JNICALL
FUN(ColorSpace_getNumberOfComponents)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_colorspace *cs = from_ColorSpace(env, self);
	if (!ctx) return 0;
	return fz_colorspace_n(ctx, cs);
}

JNIEXPORT jlong JNICALL
FUN(ColorSpace_nativeDeviceGray)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	if (!ctx) return 0;
	return jlong_cast(fz_device_gray(ctx));
}

JNIEXPORT jlong JNICALL
FUN(ColorSpace_nativeDeviceRGB)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	if (!ctx) return 0;
	return jlong_cast(fz_device_rgb(ctx));
}

JNIEXPORT jlong JNICALL
FUN(ColorSpace_nativeDeviceBGR)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	if (!ctx) return 0;
	return jlong_cast(fz_device_bgr(ctx));
}

JNIEXPORT jlong JNICALL
FUN(ColorSpace_nativeDeviceCMYK)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	if (!ctx) return 0;
	return jlong_cast(fz_device_cmyk(ctx));
}

JNIEXPORT jboolean JNICALL
FUN(ColorSpace_isGray)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_colorspace *cs = from_ColorSpace(env, self);
	int result = 0;
	if (!ctx) return 0;
	fz_try(ctx)
		result = fz_colorspace_is_gray(ctx, cs);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
	return result;
}

JNIEXPORT jboolean JNICALL
FUN(ColorSpace_isRGB)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_colorspace *cs = from_ColorSpace(env, self);
	int result = 0;
	if (!ctx) return 0;
	fz_try(ctx)
		result = fz_colorspace_is_rgb(ctx, cs);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
	return result;
}

JNIEXPORT jboolean JNICALL
FUN(ColorSpace_isCMYK)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_colorspace *cs = from_ColorSpace(env, self);
	int result = 0;
	if (!ctx) return 0;
	fz_try(ctx)
		result = fz_colorspace_is_cmyk(ctx, cs);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
	return result;
}

JNIEXPORT jboolean JNICALL
FUN(ColorSpace_isIndexed)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_colorspace *cs = from_ColorSpace(env, self);
	int result = 0;
	if (!ctx) return 0;
	fz_try(ctx)
		result = fz_colorspace_is_indexed(ctx, cs);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
	return result;
}

JNIEXPORT jboolean JNICALL
FUN(ColorSpace_isLab)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_colorspace *cs = from_ColorSpace(env, self);
	int result = 0;
	if (!ctx) return 0;
	fz_try(ctx)
		result = fz_colorspace_is_lab(ctx, cs);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
	return result;
}

JNIEXPORT jboolean JNICALL
FUN(ColorSpace_isDeviceN)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_colorspace *cs = from_ColorSpace(env, self);
	int result = 0;
	if (!ctx) return 0;
	fz_try(ctx)
		result = fz_colorspace_is_device_n(ctx, cs);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
	return result;
}

JNIEXPORT jboolean JNICALL
FUN(ColorSpace_isSubtractive)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_colorspace *cs = from_ColorSpace(env, self);
	int result = 0;
	if (!ctx) return 0;
	fz_try(ctx)
		result = fz_colorspace_is_subtractive(ctx, cs);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
	return result;
}
