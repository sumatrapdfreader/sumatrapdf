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

/* Cookie interface */

JNIEXPORT void JNICALL
FUN(Cookie_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_cookie *cookie = from_Cookie_safe(env, self);
	if (!ctx || !cookie) return;
	(*env)->SetLongField(env, self, fid_Cookie_pointer, 0);
	fz_free(ctx, cookie);
}

JNIEXPORT jlong JNICALL
FUN(Cookie_newNative)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_cookie *cookie = NULL;

	if (!ctx) return 0;

	fz_try(ctx)
		cookie = fz_malloc_struct(ctx, fz_cookie);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(cookie);
}

JNIEXPORT void JNICALL
FUN(Cookie_abort)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_cookie *cookie = from_Cookie(env, self);

	if (!ctx || !cookie) return;

	cookie->abort = 1;
}

JNIEXPORT jint JNICALL
FUN(Cookie_getProgress)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_cookie *cookie = from_Cookie(env, self);

	if (!ctx || !cookie) return 0;

	return cookie->progress;
}

JNIEXPORT jint JNICALL
FUN(Cookie_getProgressMax)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_cookie *cookie = from_Cookie(env, self);

	if (!ctx || !cookie) return 0;

	return cookie->progress_max;
}

JNIEXPORT jint JNICALL
FUN(Cookie_getErrors)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_cookie *cookie = from_Cookie(env, self);

	if (!ctx || !cookie) return 0;

	return cookie->errors;
}

JNIEXPORT jboolean JNICALL
FUN(Cookie_getIncomplete)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_cookie *cookie = from_Cookie(env, self);

	if (!ctx || !cookie) return JNI_FALSE;

	return cookie->incomplete ? JNI_TRUE : JNI_FALSE;
}
