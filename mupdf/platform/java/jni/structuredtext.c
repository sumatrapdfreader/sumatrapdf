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

/* StructuredText interface */

JNIEXPORT void JNICALL
FUN(StructuredText_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *text = from_StructuredText_safe(env, self);
	if (!ctx || !text) return;
	(*env)->SetLongField(env, self, fid_StructuredText_pointer, 0);
	fz_drop_stext_page(ctx, text);
}

JNIEXPORT jobject JNICALL
FUN(StructuredText_search)(JNIEnv *env, jobject self, jstring jneedle, jint style)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *text = from_StructuredText(env, self);
	const char *needle = NULL;
	search_state state = { env, NULL, 0 };

	if (!ctx || !text) return NULL;
	if (!jneedle) jni_throw_arg(env, "needle must not be null");

	needle = (*env)->GetStringUTFChars(env, jneedle, NULL);
	if (!needle) return NULL;

	state.hits = (*env)->NewObject(env, cls_ArrayList, mid_ArrayList_init);
	if (!state.hits || (*env)->ExceptionCheck(env)) return NULL;

	fz_try(ctx)
		fz_match_stext_page_cb(ctx, text, needle, hit_callback, &state, style);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jneedle, needle);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	if (state.error)
		return NULL;

	return (*env)->CallObjectMethod(env, state.hits, mid_ArrayList_toArray);
}

JNIEXPORT jobject JNICALL
FUN(StructuredText_highlight)(JNIEnv *env, jobject self, jobject jpt1, jobject jpt2)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *text = from_StructuredText(env, self);
	fz_point pt1 = from_Point(env, jpt1);
	fz_point pt2 = from_Point(env, jpt2);
	fz_quad hits[1000];
	int n = 0;

	if (!ctx || !text) return NULL;

	fz_try(ctx)
		n = fz_highlight_selection(ctx, text, pt1, pt2, hits, nelem(hits));
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_QuadArray_safe(ctx, env, hits, n);
}

JNIEXPORT jobject JNICALL
FUN(StructuredText_snapSelection)(JNIEnv *env, jobject self, jobject jpt1, jobject jpt2, jint mode)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *text = from_StructuredText(env, self);
	fz_point pt1 = from_Point(env, jpt1);
	fz_point pt2 = from_Point(env, jpt2);
	fz_quad quad;

	if (!ctx || !text) return NULL;

	fz_try(ctx)
		quad = fz_snap_selection(ctx, text, &pt1, &pt2, mode);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	(*env)->SetFloatField(env, jpt1, fid_Point_x, pt1.x);
	(*env)->SetFloatField(env, jpt1, fid_Point_y, pt1.y);
	(*env)->SetFloatField(env, jpt2, fid_Point_x, pt2.x);
	(*env)->SetFloatField(env, jpt2, fid_Point_y, pt2.y);

	return to_Quad_safe(ctx, env, quad);
}

JNIEXPORT jobject JNICALL
FUN(StructuredText_copy)(JNIEnv *env, jobject self, jobject jpt1, jobject jpt2)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *text = from_StructuredText(env, self);
	fz_point pt1 = from_Point(env, jpt1);
	fz_point pt2 = from_Point(env, jpt2);
	jstring jstring = NULL;
	char *s = NULL;

	if (!ctx || !text) return NULL;

	fz_var(s);

	fz_try(ctx)
	{
		s = fz_copy_selection(ctx, text, pt1, pt2, 0);
		jstring = (*env)->NewStringUTF(env, s);
	}
	fz_always(ctx)
		fz_free(ctx, s);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return jstring;
}

static void
java_stext_walk(JNIEnv *env, fz_context *ctx, jobject walker, fz_stext_block *block)
{
	fz_stext_line *line = NULL;
	fz_stext_char *ch = NULL;
	jobject jbbox = NULL;
	jobject jtrm = NULL;
	jobject jimage = NULL;
	jobject jdir = NULL;
	jobject jorigin = NULL;
	jobject jfont = NULL;
	jobject jquad = NULL;
	jobject jvecinfo = NULL;

	if (block == NULL)
		return; /* structured text has no blocks to walk */

	for (; block; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_IMAGE)
		{
			jbbox = to_Rect_safe(ctx, env, block->bbox);
			if (!jbbox) return;

			jtrm = to_Matrix_safe(ctx, env, block->u.i.transform);
			if (!jtrm) return;

			jimage = to_Image_safe(ctx, env, block->u.i.image);
			if (!jimage) return;

			(*env)->CallVoidMethod(env, walker, mid_StructuredTextWalker_onImageBlock, jbbox, jtrm, jimage);
			if ((*env)->ExceptionCheck(env)) return;

			(*env)->DeleteLocalRef(env, jbbox);
			(*env)->DeleteLocalRef(env, jimage);
			(*env)->DeleteLocalRef(env, jtrm);
		}
		else if (block->type == FZ_STEXT_BLOCK_TEXT)
		{
			jbbox = to_Rect_safe(ctx, env, block->bbox);
			if (!jbbox) return;

			(*env)->CallVoidMethod(env, walker, mid_StructuredTextWalker_beginTextBlock, jbbox);
			if ((*env)->ExceptionCheck(env)) return;

			(*env)->DeleteLocalRef(env, jbbox);

			for (line = block->u.t.first_line; line; line = line->next)
			{
				jbbox = to_Rect_safe(ctx, env, line->bbox);
				if (!jbbox) return;

				jdir = to_Point_safe(ctx, env, line->dir);
				if (!jdir) return;

				(*env)->CallVoidMethod(env, walker, mid_StructuredTextWalker_beginLine, jbbox, line->wmode, jdir);
				if ((*env)->ExceptionCheck(env)) return;

				(*env)->DeleteLocalRef(env, jdir);
				(*env)->DeleteLocalRef(env, jbbox);

				for (ch = line->first_char; ch; ch = ch->next)
				{
					jorigin = to_Point_safe(ctx, env, ch->origin);
					if (!jorigin) return;

					jfont = to_Font_safe(ctx, env, ch->font);
					if (!jfont) return;

					jquad = to_Quad_safe(ctx, env, ch->quad);
					if (!jquad) return;

					(*env)->CallVoidMethod(env, walker, mid_StructuredTextWalker_onChar,
						ch->c, jorigin, jfont, ch->size, jquad, ch->argb, ch->flags);
					if ((*env)->ExceptionCheck(env)) return;

					(*env)->DeleteLocalRef(env, jquad);
					(*env)->DeleteLocalRef(env, jfont);
					(*env)->DeleteLocalRef(env, jorigin);
				}

				(*env)->CallVoidMethod(env, walker, mid_StructuredTextWalker_endLine);
				if ((*env)->ExceptionCheck(env)) return;
			}

			(*env)->CallVoidMethod(env, walker, mid_StructuredTextWalker_endTextBlock);
			if ((*env)->ExceptionCheck(env)) return;
		}
		else if (block->type == FZ_STEXT_BLOCK_STRUCT)
		{
			jstring jstandard = to_String_safe(ctx, env, fz_structure_to_string(block->u.s.down->standard));
			if (!jstandard) return;

			jstring jraw = to_String_safe(ctx, env, block->u.s.down->raw);
			if (!jraw) return;

			(*env)->CallVoidMethod(env, walker, mid_StructuredTextWalker_beginStruct, jstandard, jraw, block->u.s.index);
			if ((*env)->ExceptionCheck(env)) return;

			(*env)->DeleteLocalRef(env, jraw);
			(*env)->DeleteLocalRef(env, jstandard);

			if (block->u.s.down)
				java_stext_walk(env, ctx, walker, block->u.s.down->first_block);

			(*env)->CallVoidMethod(env, walker, mid_StructuredTextWalker_endStruct);
			if ((*env)->ExceptionCheck(env)) return;
		}
		else if (block->type == FZ_STEXT_BLOCK_VECTOR)
		{
			jbbox = to_Rect_safe(ctx, env, block->bbox);
			if (!jbbox) return;

			jvecinfo = to_VectorInfo_safe(ctx, env, block->u.v.flags);
			if (!jvecinfo) return;

			(*env)->CallVoidMethod(env, walker, mid_StructuredTextWalker_onVector, jbbox, jvecinfo, block->u.v.argb);
			if ((*env)->ExceptionCheck(env)) return;

			(*env)->DeleteLocalRef(env, jvecinfo);
			(*env)->DeleteLocalRef(env, jbbox);
		}
	}
}

JNIEXPORT void JNICALL
FUN(StructuredText_walk)(JNIEnv *env, jobject self, jobject walker)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *page = from_StructuredText(env, self);

	if (!ctx || !page) return;
	if (!walker) jni_throw_arg_void(env, "walker must not be null");

	java_stext_walk(env, ctx, walker, page->first_block);
}

JNIEXPORT jstring JNICALL
FUN(StructuredText_asJSON)(JNIEnv *env, jobject self, jfloat scale)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *page = from_StructuredText(env, self);
	fz_output *out = NULL;
	fz_buffer *buf = NULL;
	char *str = NULL;

	if (!ctx || !page) return NULL;

	fz_var(buf);
	fz_var(out);

	fz_try(ctx)
	{
		buf = fz_new_buffer(ctx, 1024);
		out = fz_new_output_with_buffer(ctx, buf);
		fz_print_stext_page_as_json(ctx, out, page, scale);
		fz_close_output(ctx, out);
		fz_terminate_buffer(ctx, buf);
		(void)fz_buffer_extract(ctx, buf, (unsigned char**)&str);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		jni_rethrow(env, ctx);
	}

	return to_String_safe_own(ctx, env, str);
}

JNIEXPORT jstring JNICALL
FUN(StructuredText_asHTML)(JNIEnv *env, jobject self, jint id)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *page = from_StructuredText(env, self);
	fz_output *out = NULL;
	fz_buffer *buf = NULL;
	char *str = NULL;

	if (!ctx || !page) return NULL;

	fz_var(buf);
	fz_var(out);

	fz_try(ctx)
	{
		buf = fz_new_buffer(ctx, 1024);
		out = fz_new_output_with_buffer(ctx, buf);
		fz_print_stext_page_as_html(ctx, out, page, id);
		fz_close_output(ctx, out);
		fz_terminate_buffer(ctx, buf);
		(void)fz_buffer_extract(ctx, buf, (unsigned char**)&str);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		jni_rethrow(env, ctx);
	}

	return to_String_safe_own(ctx, env, str);
}

JNIEXPORT jstring JNICALL
FUN(StructuredText_asText)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *page = from_StructuredText(env, self);
	fz_output *out = NULL;
	fz_buffer *buf = NULL;
	char *str = NULL;

	if (!ctx || !page) return NULL;

	fz_var(buf);
	fz_var(out);

	fz_try(ctx)
	{
		buf = fz_new_buffer(ctx, 1024);
		out = fz_new_output_with_buffer(ctx, buf);
		fz_print_stext_page_as_text(ctx, out, page);
		fz_close_output(ctx, out);
		fz_terminate_buffer(ctx, buf);
		(void)fz_buffer_extract(ctx, buf, (unsigned char**)&str);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		jni_rethrow(env, ctx);
	}

	return to_String_safe_own(ctx, env, str);
}
