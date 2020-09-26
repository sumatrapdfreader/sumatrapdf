/* Font interface */

JNIEXPORT void JNICALL
FUN(Font_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_font *font = from_Font_safe(env, self);
	if (!ctx || !font) return;
	(*env)->SetLongField(env, self, fid_Font_pointer, 0);
	fz_drop_font(ctx, font);
}

JNIEXPORT jlong JNICALL
FUN(Font_newNative)(JNIEnv *env, jobject self, jstring jname, jint index)
{
	fz_context *ctx = get_context(env);
	const char *name = NULL;
	fz_font *font = NULL;

	if (!ctx) return 0;
	if (jname)
	{
		name = (*env)->GetStringUTFChars(env, jname, NULL);
		if (!name) return 0;
	}

	fz_try(ctx)
	{
		const unsigned char *data;
		int size;

		data = fz_lookup_base14_font(ctx, name, &size);
		if (data)
			font = fz_new_font_from_memory(ctx, name, data, size, index, 0);
		else
			font = fz_new_font_from_file(ctx, name, name, index, 0);
	}
	fz_always(ctx)
		if (name)
			(*env)->ReleaseStringUTFChars(env, jname, name);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jlong_cast(font);
}

JNIEXPORT jstring JNICALL
FUN(Font_getName)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_font *font = from_Font(env, self);

	if (!ctx || !font) return NULL;

	return (*env)->NewStringUTF(env, fz_font_name(ctx, font));
}

JNIEXPORT jint JNICALL
FUN(Font_encodeCharacter)(JNIEnv *env, jobject self, jint unicode)
{
	fz_context *ctx = get_context(env);
	fz_font *font = from_Font(env, self);
	jint glyph = 0;

	if (!ctx || !font) return 0;

	fz_try(ctx)
		glyph = fz_encode_character(ctx, font, unicode);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return glyph;
}

JNIEXPORT jfloat JNICALL
FUN(Font_advanceGlyph)(JNIEnv *env, jobject self, jint glyph, jboolean wmode)
{
	fz_context *ctx = get_context(env);
	fz_font *font = from_Font(env, self);
	float advance = 0;

	if (!ctx || !font) return 0;

	fz_try(ctx)
		advance = fz_advance_glyph(ctx, font, glyph, wmode);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return advance;
}
