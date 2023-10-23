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

/* Device interface */

/*
	Devices can either be implemented in C, or in Java.
	We therefore have to think about 4 possible call combinations.

	1) C -> C:
	The standard mupdf case. No special worries here.
	2) C -> Java:
	This can only happen when we call run on a page/annotation/
	displaylist. We need to ensure that the java Device has an
	appropriate fz_java_device generated for it, which is done by the
	Device constructor. The 'run' calls take care to lock/unlock for us.
	3) Java -> C:
	The C device will have a java shim (a subclass of NativeDevice).
	All calls will go through the device methods in NativeDevice,
	which converts the java objects to C ones, and lock/unlock
	any underlying objects as required.
	4) Java -> Java:
	No special worries.
 */

typedef struct
{
	fz_device super;
	JNIEnv *env;
	jobject self;
}
fz_java_device;

static void
fz_java_device_fill_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cs_params)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jpath = to_Path(ctx, env, path);
	jobject jcs = to_ColorSpace(ctx, env, cs);
	jobject jctm = to_Matrix(ctx, env, ctm);
	jfloatArray jcolor = to_floatArray(ctx, env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS);
	int jcp = to_ColorParams_safe(ctx, env, cs_params);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_fillPath, jpath, (jboolean)even_odd, jctm, jcs, jcolor, alpha, jcp);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *state, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cs_params)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jpath = to_Path(ctx, env, path);
	jobject jstate = to_StrokeState(ctx, env, state);
	jobject jcs = to_ColorSpace(ctx, env, cs);
	jobject jctm = to_Matrix(ctx, env, ctm);
	jfloatArray jcolor = to_floatArray(ctx, env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS);
	int jcp = to_ColorParams_safe(ctx, env, cs_params);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_strokePath, jpath, jstate, jctm, jcs, jcolor, alpha, jcp);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_clip_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm, fz_rect scissor)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jpath = to_Path(ctx, env, path);
	jobject jctm = to_Matrix(ctx, env, ctm);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_clipPath, jpath, (jboolean)even_odd, jctm);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_clip_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *state, fz_matrix ctm, fz_rect scissor)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jpath = to_Path(ctx, env, path);
	jobject jstate = to_StrokeState(ctx, env, state);
	jobject jctm = to_Matrix(ctx, env, ctm);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_clipStrokePath, jpath, jstate, jctm);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_fill_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cs_params)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jtext = to_Text(ctx, env, text);
	jobject jctm = to_Matrix(ctx, env, ctm);
	jobject jcs = to_ColorSpace(ctx, env, cs);
	jfloatArray jcolor = to_floatArray(ctx, env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS);
	int jcp = to_ColorParams_safe(ctx, env, cs_params);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_fillText, jtext, jctm, jcs, jcolor, alpha, jcp);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *state, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cs_params)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jtext = to_Text(ctx, env, text);
	jobject jstate = to_StrokeState(ctx, env, state);
	jobject jctm = to_Matrix(ctx, env, ctm);
	jobject jcs = to_ColorSpace(ctx, env, cs);
	jfloatArray jcolor = to_floatArray(ctx, env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS);
	int jcp = to_ColorParams_safe(ctx, env, cs_params);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_strokeText, jtext, jstate, jctm, jcs, jcolor, alpha, jcp);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_clip_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm, fz_rect scissor)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jtext = to_Text(ctx, env, text);
	jobject jctm = to_Matrix(ctx, env, ctm);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_clipText, jtext, jctm);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_clip_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *state, fz_matrix ctm, fz_rect scissor)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jtext = to_Text(ctx, env, text);
	jobject jstate = to_StrokeState(ctx, env, state);
	jobject jctm = to_Matrix(ctx, env, ctm);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_clipStrokeText, jtext, jstate, jctm);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_ignore_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jtext = to_Text(ctx, env, text);
	jobject jctm = to_Matrix(ctx, env, ctm);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_ignoreText, jtext, jctm);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_fill_shade(fz_context *ctx, fz_device *dev, fz_shade *shd, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jshd = to_Shade(ctx, env, shd);
	jobject jctm = to_Matrix(ctx, env, ctm);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_fillShade, jshd, jctm, alpha);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_fill_image(fz_context *ctx, fz_device *dev, fz_image *img, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jimg = to_Image(ctx, env, img);
	jobject jctm = to_Matrix(ctx, env, ctm);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_fillImage, jimg, jctm, alpha);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_fill_image_mask(fz_context *ctx, fz_device *dev, fz_image *img, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cs_params)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jimg = to_Image(ctx, env, img);
	jobject jctm = to_Matrix(ctx, env, ctm);
	jobject jcs = to_ColorSpace(ctx, env, cs);
	jfloatArray jcolor = to_floatArray(ctx, env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS);
	int jcp = to_ColorParams_safe(ctx, env, cs_params);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_fillImageMask, jimg, jctm, jcs, jcolor, alpha, jcp);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_clip_image_mask(fz_context *ctx, fz_device *dev, fz_image *img, fz_matrix ctm, fz_rect scissor)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jimg = to_Image(ctx, env, img);
	jobject jctm = to_Matrix(ctx, env, ctm);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_clipImageMask, jimg, jctm);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_pop_clip(fz_context *ctx, fz_device *dev)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_popClip);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_begin_layer(fz_context *ctx, fz_device *dev, const char *name)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jstring jname;

	jname = (*env)->NewStringUTF(env, name);
	if (!jname || (*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_beginLayer, jname);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_end_layer(fz_context *ctx, fz_device *dev)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_endLayer);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_begin_mask(fz_context *ctx, fz_device *dev, fz_rect rect, int luminosity, fz_colorspace *cs, const float *bc, fz_color_params cs_params)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jrect = to_Rect(ctx, env, rect);
	jobject jcs = to_ColorSpace(ctx, env, cs);
	jfloatArray jbc = to_floatArray(ctx, env, bc, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS);
	int jcp = to_ColorParams_safe(ctx, env, cs_params);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_beginMask, jrect, (jint)luminosity, jcs, jbc, jcp);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_end_mask(fz_context *ctx, fz_device *dev)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_endMask);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_begin_group(fz_context *ctx, fz_device *dev, fz_rect rect, fz_colorspace *cs, int isolated, int knockout, int blendmode, float alpha)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jrect = to_Rect(ctx, env, rect);
	jobject jcs = to_ColorSpace(ctx, env, cs);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_beginGroup, jrect, jcs, (jboolean)isolated, (jboolean)knockout, (jint)blendmode, alpha);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_end_group(fz_context *ctx, fz_device *dev)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_endGroup);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static int
fz_java_device_begin_tile(fz_context *ctx, fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm, int id)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jarea = to_Rect(ctx, env, area);
	jobject jview = to_Rect(ctx, env, view);
	jobject jctm = to_Matrix(ctx, env, ctm);
	int res;

	res = (*env)->CallIntMethod(env, jdev->self, mid_Device_beginTile, jarea, jview, xstep, ystep, jctm, (jint)id);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return res;
}

static void
fz_java_device_end_tile(fz_context *ctx, fz_device *dev)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_endTile);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_render_flags(fz_context *ctx, fz_device *dev, int set, int clear)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_renderFlags, set, clear);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_set_default_colorspaces(fz_context *ctx, fz_device *dev, fz_default_colorspaces *dcs)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jdcs = to_DefaultColorSpaces(ctx, env, dcs);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_setDefaultColorSpaces, jdcs);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_begin_structure(fz_context *ctx, fz_device *dev, fz_structure standard, const char *raw, int uid)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jstring jraw;

	jraw = (*env)->NewStringUTF(env, raw);
	if (!jraw || (*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_beginStructure, standard, jraw, uid);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_end_structure(fz_context *ctx, fz_device *dev)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_endStructure);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_begin_metatext(fz_context *ctx, fz_device *dev, fz_metatext meta, const char *text)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jstring jtext;

	jtext = (*env)->NewStringUTF(env, text);
	if (!jtext || (*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_beginMetatext, meta, jtext);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_end_metatext(fz_context *ctx, fz_device *dev)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_endMetatext);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_drop(fz_context *ctx, fz_device *dev)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;

	(*env)->DeleteGlobalRef(env, jdev->self);
}

static fz_device *fz_new_java_device(fz_context *ctx, JNIEnv *env, jclass cls)
{
	fz_java_device *dev = NULL;
	jobject jself;

	jself = (*env)->NewGlobalRef(env, cls);
	if (!jself) return NULL;

	fz_try(ctx)
	{
		dev = fz_new_derived_device(ctx, fz_java_device);
		dev->env = env;
		dev->self = jself;

		dev->super.drop_device = fz_java_device_drop;

		dev->super.fill_path = fz_java_device_fill_path;
		dev->super.stroke_path = fz_java_device_stroke_path;
		dev->super.clip_path = fz_java_device_clip_path;
		dev->super.clip_stroke_path = fz_java_device_clip_stroke_path;

		dev->super.fill_text = fz_java_device_fill_text;
		dev->super.stroke_text = fz_java_device_stroke_text;
		dev->super.clip_text = fz_java_device_clip_text;
		dev->super.clip_stroke_text = fz_java_device_clip_stroke_text;
		dev->super.ignore_text = fz_java_device_ignore_text;

		dev->super.fill_shade = fz_java_device_fill_shade;
		dev->super.fill_image = fz_java_device_fill_image;
		dev->super.fill_image_mask = fz_java_device_fill_image_mask;
		dev->super.clip_image_mask = fz_java_device_clip_image_mask;

		dev->super.pop_clip = fz_java_device_pop_clip;

		dev->super.begin_mask = fz_java_device_begin_mask;
		dev->super.end_mask = fz_java_device_end_mask;
		dev->super.begin_group = fz_java_device_begin_group;
		dev->super.end_group = fz_java_device_end_group;

		dev->super.begin_tile = fz_java_device_begin_tile;
		dev->super.end_tile = fz_java_device_end_tile;

		dev->super.render_flags = fz_java_device_render_flags;
		dev->super.set_default_colorspaces = fz_java_device_set_default_colorspaces;

		dev->super.begin_layer = fz_java_device_begin_layer;
		dev->super.end_layer = fz_java_device_end_layer;

		dev->super.begin_structure = fz_java_device_begin_structure;
		dev->super.end_structure = fz_java_device_end_structure;

		dev->super.begin_metatext = fz_java_device_begin_metatext;
		dev->super.end_metatext = fz_java_device_end_metatext;
	}
	fz_catch(ctx)
	{
		fz_drop_device(ctx, &dev->super);
		jni_rethrow(env, ctx);
	}

	return (fz_device *)dev;
}

JNIEXPORT jlong JNICALL
FUN(Device_newNative)(JNIEnv *env, jclass cls)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = NULL;

	if (!ctx) return 0;

	fz_try(ctx)
		dev = fz_new_java_device(ctx, env, cls);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(dev);
}

JNIEXPORT void JNICALL
FUN(Device_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device_safe(env, self);
	if (!ctx || !dev) return;
	(*env)->SetLongField(env, self, fid_Device_pointer, 0);
	fz_drop_device(ctx, dev);
}
