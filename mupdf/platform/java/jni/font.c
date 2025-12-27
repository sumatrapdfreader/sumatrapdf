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

JNIEXPORT jboolean JNICALL
FUN(Font_isMono)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_font *font = from_Font(env, self);
	jboolean result = JNI_FALSE;

	if (!ctx || !font) return JNI_FALSE;

	fz_try(ctx)
		result = fz_font_is_monospaced(ctx, font);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return result;
}

JNIEXPORT jboolean JNICALL
FUN(Font_isSerif)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_font *font = from_Font(env, self);
	jboolean result = JNI_FALSE;

	if (!ctx || !font) return JNI_FALSE;

	fz_try(ctx)
		result = fz_font_is_serif(ctx, font);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return result;
}

JNIEXPORT jboolean JNICALL
FUN(Font_isBold)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_font *font = from_Font(env, self);
	jboolean result = JNI_FALSE;

	if (!ctx || !font) return JNI_FALSE;

	fz_try(ctx)
		result = fz_font_is_bold(ctx, font);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return result;
}

JNIEXPORT jboolean JNICALL
FUN(Font_isItalic)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_font *font = from_Font(env, self);
	jboolean result = JNI_FALSE;

	if (!ctx || !font) return JNI_FALSE;

	fz_try(ctx)
		result = fz_font_is_italic(ctx, font);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return result;
}

JNIEXPORT void JNICALL
FUN(Font_installFontLoader)(JNIEnv *env, jclass cls, jobject jloader)
{
	fz_context *ctx = get_context(env);

	if (!ctx) return;

	(*env)->SetStaticObjectField(env, cls_Font, fid_Font_fontLoader, jloader);

	fz_try(ctx)
		fz_install_load_system_font_funcs(ctx,
			load_java_font,
			load_java_cjk_font,
			load_java_fallback_font);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);

	return;
}
