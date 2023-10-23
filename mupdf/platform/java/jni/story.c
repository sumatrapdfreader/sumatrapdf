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

/* Story interface */

JNIEXPORT void JNICALL
FUN(Story_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_story *story = from_Story_safe(env, self);
	if (!ctx || !story) return;
	(*env)->SetLongField(env, self, fid_Story_pointer, 0);
	fz_drop_story(ctx, story);
}

JNIEXPORT jlong JNICALL
FUN(Story_newStory)(JNIEnv *env, jclass cls, jbyteArray content, jbyteArray css, float em, jobject jarch)
{
	fz_context *ctx = get_context(env);
	int content_len, css_len;
	jbyte *content_bytes = NULL;
	jbyte *css_bytes = NULL;
	fz_story *story = NULL;
	fz_buffer *content_buf = NULL;
	fz_buffer *css_buf = NULL;
	fz_archive *arch = from_Archive(env, jarch);

	if (!ctx) return 0;

	if (content)
	{
		content_len = (*env)->GetArrayLength(env, content);
		content_bytes = (*env)->GetByteArrayElements(env, content, NULL);
	}
	else
	{
		content_len = 0;
		content_bytes = NULL;
	}
	if (css)
	{
		css_len = (*env)->GetArrayLength(env, css);
		css_bytes = (*env)->GetByteArrayElements(env, css, NULL);
	}
	else
	{
		css_len = 0;
		css_bytes = NULL;
	}

	fz_var(content_buf);
	fz_var(css_buf);
	fz_var(story);

	fz_try(ctx)
	{
		content_buf = fz_new_buffer_from_copied_data(ctx, (const unsigned char *)content_bytes, content_len);
		css_buf = fz_new_buffer_from_copied_data(ctx, (const unsigned char *)css_bytes, css_len);
		fz_terminate_buffer(ctx, css_buf);

		story = fz_new_story(ctx, content_buf, (const char *)css_buf->data, em, arch);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, content_buf);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}

	return jlong_cast(story);
}

JNIEXPORT jboolean JNICALL
FUN(Story_place)(JNIEnv *env, jobject self, jobject jrect, jobject jfilled)
{
	fz_context *ctx = get_context(env);
	fz_story *story = from_Story_safe(env, self);
	fz_rect rect = from_Rect(env, jrect);
	fz_rect filled = fz_empty_rect;
	int more;

	fz_try(ctx)
	{
		more = fz_place_story(ctx, story, rect, &filled);

		(*env)->SetFloatField(env, jfilled, fid_Rect_x0, filled.x0);
		(*env)->SetFloatField(env, jfilled, fid_Rect_x1, filled.x1);
		(*env)->SetFloatField(env, jfilled, fid_Rect_y0, filled.y0);
		(*env)->SetFloatField(env, jfilled, fid_Rect_y1, filled.y1);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return !!more;
}

JNIEXPORT void JNICALL
FUN(Story_draw)(JNIEnv *env, jobject self, jobject jdev, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_story *story = from_Story_safe(env, self);
	fz_device *dev = from_Device(env, jdev);
	fz_matrix ctm = from_Matrix(env, jctm);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !story) return;
	if (!dev) jni_throw_arg_void(env, "device must not be null");

	info = lockNativeDevice(env, jdev, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_draw_story(ctx, story, dev, ctm);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(Story_document)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_story *story = from_Story_safe(env, self);

	return to_DOM_safe(ctx, env, fz_story_document(ctx, story));
}
