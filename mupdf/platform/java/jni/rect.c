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
