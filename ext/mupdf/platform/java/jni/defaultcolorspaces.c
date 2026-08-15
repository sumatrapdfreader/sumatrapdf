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

/* DefaultColorSpaces interface */

JNIEXPORT void JNICALL
FUN(DefaultColorSpaces_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_default_colorspaces *dcs = from_DefaultColorSpaces_safe(env, self);
	if (!ctx || !dcs) return;
	(*env)->SetLongField(env, self, fid_DefaultColorSpaces_pointer, 0);
	fz_drop_default_colorspaces(ctx, dcs);
}

JNIEXPORT void JNICALL
FUN(DefaultColorSpaces_setDefaultGray)(JNIEnv *env, jobject self, jobject jcs)
{
	fz_context *ctx = get_context(env);
	fz_default_colorspaces *dcs = from_DefaultColorSpaces_safe(env, self);
	fz_colorspace *cs = from_ColorSpace_safe(env, jcs);
	if (!ctx || !cs) return;

	fz_try(ctx)
	{
		fz_drop_colorspace(ctx, dcs->gray);
		dcs->gray = fz_keep_colorspace(ctx, cs);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(DefaultColorSpaces_setDefaultRGB)(JNIEnv *env, jobject self, jobject jcs)
{
	fz_context *ctx = get_context(env);
	fz_default_colorspaces *dcs = from_DefaultColorSpaces_safe(env, self);
	fz_colorspace *cs = from_ColorSpace_safe(env, jcs);
	if (!ctx || !cs) return;

	fz_try(ctx)
	{
		fz_drop_colorspace(ctx, dcs->rgb);
		dcs->rgb = fz_keep_colorspace(ctx, cs);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(DefaultColorSpaces_setDefaultCMYK)(JNIEnv *env, jobject self, jobject jcs)
{
	fz_context *ctx = get_context(env);
	fz_default_colorspaces *dcs = from_DefaultColorSpaces_safe(env, self);
	fz_colorspace *cs = from_ColorSpace_safe(env, jcs);
	if (!ctx || !cs) return;

	fz_try(ctx)
	{
		fz_drop_colorspace(ctx, dcs->cmyk);
		dcs->cmyk = fz_keep_colorspace(ctx, cs);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(DefaultColorSpaces_setOutputIntent)(JNIEnv *env, jobject self, jobject jcs)
{
	fz_context *ctx = get_context(env);
	fz_default_colorspaces *dcs = from_DefaultColorSpaces_safe(env, self);
	fz_colorspace *cs = from_ColorSpace_safe(env, jcs);
	if (!ctx || !cs) return;

	fz_try(ctx)
	{
		fz_drop_colorspace(ctx, dcs->oi);
		dcs->oi = fz_keep_colorspace(ctx, cs);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(DefaultColorSpaces_getDefaultGray)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_default_colorspaces *dcs = from_DefaultColorSpaces_safe(env, self);
	if (!ctx) return NULL;
	return to_ColorSpace_safe_own(ctx, env, dcs->gray);
}

JNIEXPORT jobject JNICALL
FUN(DefaultColorSpaces_getDefaultRGB)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_default_colorspaces *dcs = from_DefaultColorSpaces_safe(env, self);
	if (!ctx) return NULL;
	return to_ColorSpace_safe_own(ctx, env, dcs->rgb);
}

JNIEXPORT jobject JNICALL
FUN(DefaultColorSpaces_getDefaultCMYK)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_default_colorspaces *dcs = from_DefaultColorSpaces_safe(env, self);
	if (!ctx) return NULL;
	return to_ColorSpace_safe_own(ctx, env, dcs->cmyk);
}

JNIEXPORT jobject JNICALL
FUN(DefaultColorSpaces_getOutputIntent)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_default_colorspaces *dcs = from_DefaultColorSpaces_safe(env, self);
	if (!ctx) return NULL;
	return to_ColorSpace_safe_own(ctx, env, dcs->oi);
}
