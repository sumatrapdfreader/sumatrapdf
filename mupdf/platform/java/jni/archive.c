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

/* Archive interface */

JNIEXPORT void JNICALL
FUN(Archive_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_archive *arch = from_Archive_safe(env, self);
	if (!ctx || !arch) return;
	(*env)->SetLongField(env, self, fid_Archive_pointer, 0);
	fz_drop_archive(ctx, arch);
}

JNIEXPORT jlong JNICALL
FUN(Archive_newNativeMultiArchive)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_archive *arch = NULL;

	if (!ctx) return 0;

	fz_try(ctx)
		arch = fz_new_multi_archive(ctx);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(arch);
}

JNIEXPORT jlong JNICALL
FUN(Archive_newNativeArchive)(JNIEnv *env, jobject self, jstring jpath)
{
	fz_context *ctx = get_context(env);
	fz_archive *arch = NULL;
	const char *path = NULL;

	if (!ctx) return 0;
	if (!jpath) jni_throw_arg(env, "path must not be null");
	path = (*env)->GetStringUTFChars(env, jpath, NULL);

	fz_try(ctx)
	{
		if (fz_is_directory(ctx, path))
			arch = fz_open_directory(ctx, path);
		else
			arch = fz_open_archive(ctx, path);
	}
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jpath, path);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(arch);
}

JNIEXPORT jstring JNICALL
FUN(Archive_getFormat)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_archive *arch = from_Archive(env, self);
	const char *format = NULL;

	if (!ctx || !arch) return NULL;

	fz_try(ctx)
		format = fz_archive_format(ctx, arch);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewStringUTF(env, format);
}

JNIEXPORT jint JNICALL
FUN(Archive_countEntries)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_archive *arch = from_Archive(env, self);
	int count = -1;

	if (!ctx || !arch) return -1;

	fz_try(ctx)
		count = fz_count_archive_entries(ctx, arch);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return count;
}

JNIEXPORT jstring JNICALL
FUN(Archive_listEntry)(JNIEnv *env, jobject self, jint index)
{
	fz_context *ctx = get_context(env);
	fz_archive *arch = from_Archive(env, self);
	const char *name = NULL;

	if (!ctx || !arch) return NULL;

	fz_try(ctx)
		name = fz_list_archive_entry(ctx, arch, index);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewStringUTF(env, name);
}

JNIEXPORT jboolean JNICALL
FUN(Archive_hasEntry)(JNIEnv *env, jobject self, jstring jname)
{
	fz_context *ctx = get_context(env);
	fz_archive *arch = from_Archive(env, self);
	const char *name = NULL;
	int has = 0;

	if (!ctx || !arch) return JNI_FALSE;
	if (!jname) jni_throw_arg(env, "name must not be null");
	name = (*env)->GetStringUTFChars(env, jname, NULL);

	fz_try(ctx)
		has = fz_has_archive_entry(ctx, arch, name);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jname, name);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return has;
}

JNIEXPORT jobject JNICALL
FUN(Archive_readEntry)(JNIEnv *env, jobject self, jstring jname)
{
	fz_context *ctx = get_context(env);
	fz_archive *arch = from_Archive(env, self);
	const char *name = NULL;
	fz_buffer *buffer = NULL;

	if (!ctx || !arch) return NULL;
	if (!jname) jni_throw_arg(env, "name must not be null");
	name = (*env)->GetStringUTFChars(env, jname, NULL);

	fz_try(ctx)
		buffer = fz_read_archive_entry(ctx, arch, name);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jname, name);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Buffer_safe_own(ctx, env, buffer);
}
