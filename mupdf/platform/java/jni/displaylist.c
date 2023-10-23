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

/* DisplayList interface */

JNIEXPORT jlong JNICALL
FUN(DisplayList_newNative)(JNIEnv *env, jobject self, jobject jmediabox)
{
	fz_context *ctx = get_context(env);
	fz_rect mediabox = from_Rect(env, jmediabox);

	fz_display_list *list = NULL;

	if (!ctx) return 0;

	fz_try(ctx)
		list = fz_new_display_list(ctx, mediabox);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(list);
}

JNIEXPORT void JNICALL
FUN(DisplayList_run)(JNIEnv *env, jobject self, jobject jdev, jobject jctm, jobject jrect, jobject jcookie)
{
	fz_context *ctx = get_context(env);
	fz_display_list *list = from_DisplayList(env, self);
	fz_device *dev = from_Device(env, jdev);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_cookie *cookie = from_Cookie(env, jcookie);
	NativeDeviceInfo *info;
	fz_rect rect;
	int err;

	if (!ctx || !list) return;
	if (!dev) jni_throw_arg_void(env, "device must not be null");

	/* Use a scissor rectangle if one is supplied */
	if (jrect)
		rect = from_Rect(env, jrect);
	else
		rect = fz_infinite_rect;

	info = lockNativeDevice(env, jdev, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_run_display_list(ctx, list, dev, ctm, rect, cookie);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(DisplayList_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_display_list *list = from_DisplayList_safe(env, self);
	if (!ctx || !list) return;
	(*env)->SetLongField(env, self, fid_DisplayList_pointer, 0);
	fz_drop_display_list(ctx, list);
}

JNIEXPORT jobject JNICALL
FUN(DisplayList_toPixmap)(JNIEnv *env, jobject self, jobject jctm, jobject jcs, jboolean alpha)
{
	fz_context *ctx = get_context(env);
	fz_display_list *list = from_DisplayList(env, self);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_pixmap *pixmap = NULL;

	if (!ctx || !list) return NULL;

	fz_try(ctx)
		pixmap = fz_new_pixmap_from_display_list(ctx, list, ctm, cs, alpha);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Pixmap_safe_own(ctx, env, pixmap);
}

JNIEXPORT jobject JNICALL
FUN(DisplayList_toStructuredText)(JNIEnv *env, jobject self, jstring joptions)
{
	fz_context *ctx = get_context(env);
	fz_display_list *list = from_DisplayList(env, self);
	fz_stext_page *text = NULL;
	const char *options= NULL;
	fz_stext_options opts;

	if (!ctx || !list) return NULL;

	if (joptions)
	{
		options = (*env)->GetStringUTFChars(env, joptions, NULL);
		if (!options) return NULL;
	}

	fz_try(ctx)
	{
		fz_parse_stext_options(ctx, &opts, options);
		text = fz_new_stext_page_from_display_list(ctx, list, &opts);
	}
	fz_always(ctx)
	{
		if (options)
			(*env)->ReleaseStringUTFChars(env, joptions, options);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_StructuredText_safe_own(ctx, env, text);
}

JNIEXPORT jobject JNICALL
FUN(DisplayList_search)(JNIEnv *env, jobject self, jstring jneedle)
{
	fz_context *ctx = get_context(env);
	fz_display_list *list = from_DisplayList(env, self);
	fz_quad hits[500];
	int marks[500];
	const char *needle = NULL;
	int n = 0;

	if (!ctx || !list) return NULL;
	if (!jneedle) jni_throw_arg(env, "needle must not be null");

	needle = (*env)->GetStringUTFChars(env, jneedle, NULL);
	if (!needle) return NULL;

	fz_try(ctx)
		n = fz_search_display_list(ctx, list, needle, marks, hits, nelem(hits));
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jneedle, needle);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_SearchHits_safe(ctx, env, marks, hits, n);
}

JNIEXPORT jobject JNICALL
FUN(DisplayList_getBounds)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_display_list *list = from_DisplayList(env, self);
	fz_rect bounds;

	if (!ctx || !list) return NULL;

	fz_try(ctx)
		bounds = fz_bound_display_list(ctx, list);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Rect(ctx, env, bounds);
}
