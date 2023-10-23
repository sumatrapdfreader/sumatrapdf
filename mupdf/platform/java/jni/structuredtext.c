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
FUN(StructuredText_search)(JNIEnv *env, jobject self, jstring jneedle)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *text = from_StructuredText(env, self);
	fz_quad hits[500];
	int marks[500];
	const char *needle = NULL;
	int n = 0;

	if (!ctx || !text) return NULL;
	if (!jneedle) jni_throw_arg(env, "needle must not be null");

	needle = (*env)->GetStringUTFChars(env, jneedle, NULL);
	if (!needle) return NULL;

	fz_try(ctx)
		n = fz_search_stext_page(ctx, text, needle, marks, hits, nelem(hits));
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jneedle, needle);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_SearchHits_safe(ctx, env, marks, hits, n);
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

JNIEXPORT void JNICALL
FUN(StructuredText_walk)(JNIEnv *env, jobject self, jobject walker)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *page = from_StructuredText(env, self);
	fz_stext_block *block = NULL;
	fz_stext_line *line = NULL;
	fz_stext_char *ch = NULL;
	jobject jbbox = NULL;
	jobject jtrm = NULL;
	jobject jimage = NULL;
	jobject jdir = NULL;
	jobject jorigin = NULL;
	jobject jfont = NULL;
	jobject jquad = NULL;

	if (!ctx || !page) return;
	if (!walker) jni_throw_arg_void(env, "walker must not be null");

	if (page->first_block == NULL)
		return; /* structured text has no blocks to walk */

	for (block = page->first_block; block; block = block->next)
	{
		jbbox = to_Rect_safe(ctx, env, block->bbox);
		if (!jbbox) return;

		if (block->type == FZ_STEXT_BLOCK_IMAGE)
		{
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
						ch->c, jorigin, jfont, ch->size, jquad);
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
	}
}
