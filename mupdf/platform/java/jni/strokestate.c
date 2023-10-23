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

/* StrokeState interface */

JNIEXPORT void JNICALL
FUN(StrokeState_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_stroke_state *stroke = from_StrokeState_safe(env, self);
	if (!ctx || !stroke) return;
	(*env)->SetLongField(env, self, fid_StrokeState_pointer, 0);
	fz_drop_stroke_state(ctx, stroke);
}

JNIEXPORT jlong JNICALL
FUN(StrokeState_newNativeStrokeState)(JNIEnv *env, jobject self, jint startCap, jint dashCap, jint endCap, jint lineJoin, jfloat lineWidth, jfloat miterLimit, jfloat dashPhase, jfloatArray dash)
{
	fz_context *ctx = get_context(env);
	fz_stroke_state *stroke = NULL;
	jsize len = 0;

	if (!ctx) return 0;

	if (dash)
		len = (*env)->GetArrayLength(env, dash);

	fz_try(ctx)
	{
		stroke = fz_new_stroke_state_with_dash_len(ctx, len);
		stroke->start_cap = startCap;
		stroke->dash_cap = dashCap;
		stroke->end_cap = endCap;
		stroke->linejoin = lineJoin;
		stroke->linewidth = lineWidth;
		stroke->miterlimit = miterLimit;
		stroke->dash_phase = dashPhase;
		stroke->dash_len = len;
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	if (dash)
	{
		(*env)->GetFloatArrayRegion(env, dash, 0, len, &stroke->dash_list[0]);
		if ((*env)->ExceptionCheck(env)) return 0;
	}

	return jlong_cast(stroke);
}

JNIEXPORT jint JNICALL
FUN(StrokeState_getStartCap)(JNIEnv *env, jobject self)
{
	fz_stroke_state *stroke = from_StrokeState(env, self);
	return stroke ? stroke->start_cap : 0;
}

JNIEXPORT jint JNICALL
FUN(StrokeState_getDashCap)(JNIEnv *env, jobject self)
{
	fz_stroke_state *stroke = from_StrokeState(env, self);
	return stroke ? stroke->dash_cap : 0;
}

JNIEXPORT jint JNICALL
FUN(StrokeState_getEndCap)(JNIEnv *env, jobject self)
{
	fz_stroke_state *stroke = from_StrokeState(env, self);
	return stroke ? stroke->end_cap : 0;
}

JNIEXPORT jint JNICALL
FUN(StrokeState_getLineJoin)(JNIEnv *env, jobject self)
{
	fz_stroke_state *stroke = from_StrokeState(env, self);
	return stroke ? stroke->linejoin : 0;
}

JNIEXPORT float JNICALL
FUN(StrokeState_getLineWidth)(JNIEnv *env, jobject self)
{
	fz_stroke_state *stroke = from_StrokeState(env, self);
	return stroke ? stroke->linewidth : 0;
}

JNIEXPORT float JNICALL
FUN(StrokeState_getMiterLimit)(JNIEnv *env, jobject self)
{
	fz_stroke_state *stroke = from_StrokeState(env, self);
	return stroke ? stroke->miterlimit : 0;
}

JNIEXPORT float JNICALL
FUN(StrokeState_getDashPhase)(JNIEnv *env, jobject self)
{
	fz_stroke_state *stroke = from_StrokeState(env, self);
	return stroke ? stroke->dash_phase : 0;
}

JNIEXPORT jfloatArray JNICALL
FUN(StrokeState_getDashes)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_stroke_state *stroke = from_StrokeState(env, self);
	jfloatArray arr;

	if (!ctx || !stroke) return NULL;

	if (stroke->dash_len == 0)
		return NULL; /* there are no dashes, so return NULL instead of empty array */

	arr = (*env)->NewFloatArray(env, stroke->dash_len);
	if (!arr || (*env)->ExceptionCheck(env)) return NULL;

	(*env)->SetFloatArrayRegion(env, arr, 0, stroke->dash_len, &stroke->dash_list[0]);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return arr;
}
