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

/* Rect interface */

JNIEXPORT void JNICALL
FUN(Rect_adjustForStroke)(JNIEnv *env, jobject self, jobject jstroke, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_rect rect = from_Rect(env, self);
	fz_stroke_state *stroke = from_StrokeState(env, jstroke);
	fz_matrix ctm = from_Matrix(env, jctm);

	if (!ctx) return;
	if (!stroke) jni_throw_arg_void(env, "stroke must not be null");

	fz_try(ctx)
		rect = fz_adjust_rect_for_stroke(ctx, rect, stroke, ctm);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);

	(*env)->SetFloatField(env, self, fid_Rect_x0, rect.x0);
	(*env)->SetFloatField(env, self, fid_Rect_x1, rect.x1);
	(*env)->SetFloatField(env, self, fid_Rect_y0, rect.y0);
	(*env)->SetFloatField(env, self, fid_Rect_y1, rect.y1);
}
