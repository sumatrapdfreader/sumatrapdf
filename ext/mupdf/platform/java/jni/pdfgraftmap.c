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

/* PDFGraftMap interface */

JNIEXPORT void JNICALL
FUN(PDFGraftMap_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_graft_map *map = from_PDFGraftMap_safe(env, self);
	if (!ctx || !map) return;
	(*env)->SetLongField(env, self, fid_PDFGraftMap_pointer, 0);
	pdf_drop_graft_map(ctx, map);
}

JNIEXPORT jobject JNICALL
FUN(PDFGraftMap_graftObject)(JNIEnv *env, jobject self, jobject jobj)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, jobj);
	pdf_graft_map *map = from_PDFGraftMap(env, self);

	if (!ctx || !map) return NULL;
	if (!obj) jni_throw_arg(env, "object must not be null");

	fz_try(ctx)
		obj = pdf_graft_mapped_object(ctx, map, obj);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_PDFObject_safe_own(ctx, env, obj);
}

JNIEXPORT void JNICALL
FUN(PDFGraftMap_graftPage)(JNIEnv *env, jobject self, jint pageTo, jobject jobj, jint pageFrom)
{
	fz_context *ctx = get_context(env);
	pdf_document *src = from_PDFDocument(env, jobj);
	pdf_graft_map *map = from_PDFGraftMap(env, self);

	if (!ctx || !map) return;
	if (!src) jni_throw_arg_void(env, "Source Document must not be null");

	fz_try(ctx)
		pdf_graft_mapped_page(ctx, map, pageTo, src, pageFrom);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}
