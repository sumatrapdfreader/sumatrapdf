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

/* Path interface */

JNIEXPORT void JNICALL
FUN(Path_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path_safe(env, self);
	if (!ctx || !path) return;
	(*env)->SetLongField(env, self, fid_Path_pointer, 0);
	fz_drop_path(ctx, path);
}

JNIEXPORT jlong JNICALL
FUN(Path_newNative)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_path *path = NULL;

	if (!ctx) return 0;

	fz_try(ctx)
		path = fz_new_path(ctx);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(path);
}

JNIEXPORT jobject JNICALL
FUN(Path_currentPoint)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);
	fz_point point;

	if (!ctx || !path) return NULL;

	fz_try(ctx)
		point = fz_currentpoint(ctx, path);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Point_safe(ctx, env, point);
}

JNIEXPORT void JNICALL
FUN(Path_moveTo)(JNIEnv *env, jobject self, jfloat x, jfloat y)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);

	if (!ctx || !path) return;

	fz_try(ctx)
		fz_moveto(ctx, path, x, y);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Path_lineTo)(JNIEnv *env, jobject self, jfloat x, jfloat y)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);

	if (!ctx || !path) return;

	fz_try(ctx)
		fz_lineto(ctx, path, x, y);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Path_curveTo)(JNIEnv *env, jobject self, jfloat cx1, jfloat cy1, jfloat cx2, jfloat cy2, jfloat ex, jfloat ey)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);

	if (!ctx || !path) return;

	fz_try(ctx)
		fz_curveto(ctx, path, cx1, cy1, cx2, cy2, ex, ey);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Path_curveToV)(JNIEnv *env, jobject self, jfloat cx, jfloat cy, jfloat ex, jfloat ey)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);

	if (!ctx || !path) return;

	fz_try(ctx)
		fz_curvetov(ctx, path, cx, cy, ex, ey);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Path_curveToY)(JNIEnv *env, jobject self, jfloat cx, jfloat cy, jfloat ex, jfloat ey)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);

	if (!ctx || !path) return;

	fz_try(ctx)
		fz_curvetoy(ctx, path, cx, cy, ex, ey);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Path_rect)(JNIEnv *env, jobject self, jint x1, jint y1, jint x2, jint y2)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);

	if (!ctx || !path) return;

	fz_try(ctx)
		fz_rectto(ctx, path, x1, y1, x2, y2);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Path_closePath)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);

	if (!ctx || !path) return;

	fz_try(ctx)
		fz_closepath(ctx, path);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Path_transform)(JNIEnv *env, jobject self, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);
	fz_matrix ctm = from_Matrix(env, jctm);

	if (!ctx || !path) return;

	fz_try(ctx)
		fz_transform_path(ctx, path, ctm);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jlong JNICALL
FUN(Path_cloneNative)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_path *old_path = from_Path(env, self);
	fz_path *new_path = NULL;

	if (!ctx || !old_path) return 0;

	fz_try(ctx)
		new_path = fz_clone_path(ctx, old_path);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(new_path);
}

JNIEXPORT jobject JNICALL
FUN(Path_getBounds)(JNIEnv *env, jobject self, jobject jstroke, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);
	fz_stroke_state *stroke = from_StrokeState(env, jstroke);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_rect rect;

	if (!ctx || !path) return NULL;
	if (!stroke) jni_throw_arg(env, "stroke must not be null");

	fz_try(ctx)
		rect = fz_bound_path(ctx, path, stroke, ctm);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Rect_safe(ctx, env, rect);
}

typedef struct
{
	JNIEnv *env;
	jobject obj;
} path_walker_state;

static void
pathWalkMoveTo(fz_context *ctx, void *arg, float x, float y)
{
	path_walker_state *state = (path_walker_state *)arg;
	JNIEnv *env = state->env;
	(*env)->CallVoidMethod(env, state->obj, mid_PathWalker_moveTo, x, y);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
pathWalkLineTo(fz_context *ctx, void *arg, float x, float y)
{
	path_walker_state *state = (path_walker_state *)arg;
	JNIEnv *env = state->env;
	(*env)->CallVoidMethod(env, state->obj, mid_PathWalker_lineTo, x, y);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
pathWalkCurveTo(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2, float x3, float y3)
{
	path_walker_state *state = (path_walker_state *)arg;
	JNIEnv *env = state->env;
	(*env)->CallVoidMethod(env, state->obj, mid_PathWalker_curveTo, x1, y1, x2, y2, x3, y3);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
pathWalkClosePath(fz_context *ctx, void *arg)
{
	path_walker_state *state = (path_walker_state *) arg;
	JNIEnv *env = state->env;
	(*env)->CallVoidMethod(env, state->obj, mid_PathWalker_closePath);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static const fz_path_walker java_path_walker =
{
	pathWalkMoveTo,
	pathWalkLineTo,
	pathWalkCurveTo,
	pathWalkClosePath,
	NULL,
	NULL,
	NULL,
	NULL
};

JNIEXPORT void JNICALL
FUN(Path_walk)(JNIEnv *env, jobject self, jobject obj)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);
	path_walker_state state;

	if (!ctx || !path) return;
	if (!obj) jni_throw_arg_void(env, "object must not be null");

	state.env = env;
	state.obj = obj;

	fz_try(ctx)
		fz_walk_path(ctx, path, &java_path_walker, &state);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}
