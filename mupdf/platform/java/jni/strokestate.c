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
FUN(StrokeState_newStrokeState)(JNIEnv *env, jobject self, jint startCap, jint dashCap, jint endCap, jint lineJoin, jfloat lineWidth, jfloat miterLimit, jfloat dashPhase, jfloatArray dash)
{
	fz_context *ctx = get_context(env);
	fz_stroke_state *stroke = NULL;
	jsize len = 0;

	if (!ctx) return 0;
	if (!dash) jni_throw_arg(env, "dash must not be null");

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

	(*env)->GetFloatArrayRegion(env, dash, 0, len, &stroke->dash_list[0]);
	if ((*env)->ExceptionCheck(env)) return 0;

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
