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

/* MultiArchive interface */

JNIEXPORT jlong JNICALL
FUN(MultiArchive_newNativeMultiArchive)(JNIEnv *env, jobject self)
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


JNIEXPORT void JNICALL
FUN(MultiArchive_mountArchive)(JNIEnv *env, jobject self, jobject jsub, jstring jpath)
{
	fz_context *ctx = get_context(env);
	fz_archive *arch = from_Archive(env, self);
	fz_archive *sub = from_Archive(env, jsub);
	const char *path = NULL;

	if (!ctx || !arch) return;
	if (!jpath) jni_throw_arg_void(env, "path must not be null");
	path = (*env)->GetStringUTFChars(env, jpath, NULL);

	fz_try(ctx)
		fz_mount_multi_archive(ctx, arch, sub, path);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jpath, path);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}
