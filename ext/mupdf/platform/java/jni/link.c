// Copyright (C) 2004-2022 Artifex Software, Inc.
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

/* Link interface */

JNIEXPORT void JNICALL
FUN(Link_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_link *link = from_Link_safe(env, self);
	if (!ctx || !link) return;
	(*env)->SetLongField(env, self, fid_Link_pointer, 0);
	fz_drop_link(ctx, link);
}

JNIEXPORT jobject JNICALL
FUN(Link_getBounds)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_link *link = from_Link(env, self);

	if (!ctx || !link) return NULL;

	return to_Rect_safe(ctx, env, link->rect);
}

JNIEXPORT void JNICALL
FUN(Link_setBounds)(JNIEnv *env, jobject self, jobject jbbox)
{
	fz_context *ctx = get_context(env);
	fz_link *link = from_Link(env, self);
	fz_rect bbox = from_Rect(env, jbbox);

	if (!ctx || !link) return;

	fz_try(ctx)
		fz_set_link_rect(ctx, link, bbox);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jstring JNICALL
FUN(Link_getURI)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_link *link = from_Link(env, self);

	if (!ctx || !link) return NULL;

	return (*env)->NewStringUTF(env, link->uri);
}

JNIEXPORT void JNICALL
FUN(Link_setURI)(JNIEnv *env, jobject self, jstring juri)
{
	fz_context *ctx = get_context(env);
	fz_link *link = from_Link(env, self);
	const char *uri = NULL;

	if (!ctx || !link) return;

	if (juri)
		uri = (*env)->GetStringUTFChars(env, juri, NULL);

	fz_try(ctx)
		fz_set_link_uri(ctx, link, uri);
	fz_always(ctx)
		if (juri)
			(*env)->ReleaseStringUTFChars(env, juri, uri);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}
