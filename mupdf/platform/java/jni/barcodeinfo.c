// Copyright (C) 2025 Artifex Software, Inc.
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

/* BarcodeInfo interface */

JNIEXPORT jstring JNICALL
FUN(BarcodeInfo_toString)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_barcode_type barcode_type = FZ_BARCODE_NONE;
	jobject jcontents;
	const char *contents = NULL;
	char *str = NULL;
	jobject jstr;

	if (!ctx || !self) return NULL;

	barcode_type = (*env)->GetIntField(env, self, fid_BarcodeInfo_type);
	jcontents = (*env)->GetObjectField(env, self, fid_BarcodeInfo_contents);

	contents = (*env)->GetStringUTFChars(env, jcontents, NULL);
	if (!contents)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot not get UTF string");

	fz_try(ctx)
		str = fz_asprintf(ctx, "{ type = %d, contents = %s }",
			barcode_type, contents);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jcontents, contents);
	fz_catch(ctx)
		fz_rethrow(ctx);

	jstr = (*env)->NewStringUTF(env, str);
	fz_free(ctx, str);
	if (!jstr || (*env)->ExceptionCheck(env))
		return NULL;

	return jstr;
}
