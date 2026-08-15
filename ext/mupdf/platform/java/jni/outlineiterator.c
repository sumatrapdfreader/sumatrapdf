// Copyright (C) 2021-2025 Artifex Software, Inc.
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

/* OutlineIterator interface */

JNIEXPORT void JNICALL
FUN(OutlineIterator_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_outline_iterator *iterator = from_OutlineIterator(env, self);
	if (!ctx || !iterator) return;
	(*env)->SetLongField(env, self, fid_OutlineIterator_pointer, 0);
	fz_drop_outline_iterator(ctx, iterator);
}

JNIEXPORT jint JNICALL
FUN(OutlineIterator_next)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_outline_iterator *iterator = from_OutlineIterator(env, self);
	int okay = -1;

	if (!ctx || !iterator) return -1;

	fz_try(ctx)
		okay = fz_outline_iterator_next(ctx, iterator);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return okay;
}

JNIEXPORT jint JNICALL
FUN(OutlineIterator_prev)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_outline_iterator *iterator = from_OutlineIterator(env, self);
	int okay = -1;

	if (!ctx || !iterator) return -1;

	fz_try(ctx)
		okay = fz_outline_iterator_prev(ctx, iterator);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return okay;
}

JNIEXPORT jint JNICALL
FUN(OutlineIterator_up)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_outline_iterator *iterator = from_OutlineIterator(env, self);
	int okay = -1;

	if (!ctx || !iterator) return -1;

	fz_try(ctx)
		okay = fz_outline_iterator_up(ctx, iterator);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return okay;
}

JNIEXPORT jint JNICALL
FUN(OutlineIterator_down)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_outline_iterator *iterator = from_OutlineIterator(env, self);
	int okay = -1;

	if (!ctx || !iterator) return -1;

	fz_try(ctx)
		okay = fz_outline_iterator_down(ctx, iterator);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return okay;
}

JNIEXPORT jint JNICALL
FUN(OutlineIterator_delete)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_outline_iterator *iterator = from_OutlineIterator(env, self);
	int okay = -1;

	if (!ctx || !iterator) return -1;

	fz_try(ctx)
		okay = fz_outline_iterator_delete(ctx, iterator);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return okay;
}

static unsigned int to255(float x)
{
	if (x < 0)
		return 0;
	if (x > 1)
		return 255;
	return (int)(x * 255 + 0.5);
}

JNIEXPORT void JNICALL
FUN(OutlineIterator_update)(JNIEnv *env, jobject self, jstring jtitle, jstring juri, jboolean is_open, jfloat r, jfloat g, jfloat b, int flags)
{
	fz_context *ctx = get_context(env);
	fz_outline_iterator *iterator = from_OutlineIterator(env, self);
	fz_outline_item item = { 0 };

	if (!ctx || !iterator) return;

	item.is_open = is_open;

	fz_try(ctx)
	{
		item.title = jtitle ? (char *)((*env)->GetStringUTFChars(env, jtitle, NULL)) : NULL;
		if (item.title == NULL && jtitle != NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "OutlineIterator_update failed to get title as string");
		item.uri = juri ? (char *)((*env)->GetStringUTFChars(env, juri, NULL)) : NULL;
		if (item.uri == NULL && juri != NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "OutlineIterator_update failed to get uri as string");

		item.r = to255(r);
		item.g = to255(g);
		item.b = to255(b);
		item.flags = flags & 127;

		fz_outline_iterator_update(ctx, iterator, &item);
	}
	fz_always(ctx)
	{
		if (item.title)
			(*env)->ReleaseStringUTFChars(env, jtitle, item.title);
		if (item.uri)
			(*env)->ReleaseStringUTFChars(env, juri, item.uri);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(OutlineIterator_insert)(JNIEnv *env, jobject self, jstring jtitle, jstring juri, jboolean is_open, jfloat r, jfloat g, jfloat b, int flags)
{
	fz_context *ctx = get_context(env);
	fz_outline_iterator *iterator = from_OutlineIterator(env, self);
	int okay = -1;
	fz_outline_item item = { 0 };

	if (!ctx || !iterator) return -1;

	item.is_open = is_open;

	fz_try(ctx)
	{
		item.title = jtitle ? (char *)((*env)->GetStringUTFChars(env, jtitle, NULL)) : NULL;
		if (item.title == NULL && jtitle != NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "OutlineIterator_insert failed to get title as string");
		item.uri = juri ? (char *)((*env)->GetStringUTFChars(env, juri, NULL)) : NULL;
		if (item.uri == NULL && juri != NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "OutlineIterator_insert failed to get uri as string");

		item.r = to255(r);
		item.g = to255(g);
		item.b = to255(b);
		item.flags = flags & 127;

		okay = fz_outline_iterator_insert(ctx, iterator, &item);
	}
	fz_always(ctx)
	{
		if (item.title)
			(*env)->ReleaseStringUTFChars(env, jtitle, item.title);
		if (item.uri)
			(*env)->ReleaseStringUTFChars(env, juri, item.uri);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return okay;
}

JNIEXPORT jobject JNICALL
FUN(OutlineIterator_item)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_outline_iterator *iterator = from_OutlineIterator(env, self);
	fz_outline_item *item = NULL;
	jstring jtitle = NULL;
	jstring juri = NULL;
	float r, g, b;

	if (!ctx || !iterator) return NULL;

	fz_try(ctx)
		item = fz_outline_iterator_item(ctx, iterator);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	if (!item)
		return NULL;

	if (item->title)
	{
		jtitle = (*env)->NewStringUTF(env, item->title);
		if (!jtitle || (*env)->ExceptionCheck(env))
			return NULL;
	}
	if (item->uri)
	{
		juri = (*env)->NewStringUTF(env, item->uri);
		if (!juri || (*env)->ExceptionCheck(env))
			return NULL;
	}
	r = item->r / 255.0f;
	g = item->g / 255.0f;
	b = item->b / 255.0f;
	return  (*env)->NewObject(env, cls_OutlineItem, mid_OutlineItem_init, jtitle, juri, item->is_open, r, g, b, item->flags);
}
