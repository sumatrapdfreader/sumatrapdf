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

/* TreeArchive interface */

JNIEXPORT jlong JNICALL
FUN(Archive_newNativeTreeArchive)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_archive *arch = NULL;

	if (!ctx) return 0;

	fz_try(ctx)
		arch = fz_new_tree_archive(ctx, NULL);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(arch);
}

JNIEXPORT void JNICALL
FUN(TreeArchive_add)(JNIEnv *env, jobject self, jstring jname, jobject jbuf)
{
	fz_context *ctx = get_context(env);
	fz_archive *arch = from_Archive(env, self);
	fz_buffer *buf = from_Buffer(env, jbuf);
	const char *name = NULL;

	if (!ctx || !arch) return;
	if (!jname) jni_throw_arg_void(env, "name must not be null");
	name = (*env)->GetStringUTFChars(env, jname, NULL);

	fz_try(ctx)
		fz_tree_archive_add_buffer(ctx, arch, name, buf);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jname, name);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}
