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

/* Shade interface */

JNIEXPORT void JNICALL
FUN(Shade_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_shade *shade = from_Shade_safe(env, self);
	if (!ctx || !shade) return;
	(*env)->SetLongField(env, self, fid_Shade_pointer, 0);
	fz_drop_shade(ctx, shade);
}

JNIEXPORT jobject JNICALL
FUN(Shade_getBounds)(JNIEnv *env, jobject self, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_shade *shade = from_Shade_safe(env, self);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_rect rect;

	if (!ctx || !shade) return NULL;

	fz_try(ctx)
		rect = fz_bound_shade(ctx, shade, ctm);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Rect_safe(ctx, env, rect);
}
