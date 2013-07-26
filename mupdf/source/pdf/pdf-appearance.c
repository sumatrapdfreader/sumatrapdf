#include "mupdf/pdf.h"

#define MATRIX_COEFS (6)
#define STRIKE_HEIGHT (0.375f)
#define UNDERLINE_HEIGHT (0.075f)
#define LINE_THICKNESS (0.07f)
#define SMALL_FLOAT (0.00001)

enum
{
	Q_Left = 0,
	Q_Cent = 1,
	Q_Right = 2
};

enum
{
	BS_Solid,
	BS_Dashed,
	BS_Beveled,
	BS_Inset,
	BS_Underline
};

typedef struct font_info_s
{
	pdf_da_info da_rec;
	pdf_font_desc *font;
} font_info;

typedef struct text_widget_info_s
{
	pdf_obj *dr;
	pdf_obj *col;
	font_info font_rec;
	int q;
	int multiline;
	int comb;
	int max_len;
} text_widget_info;

static const char *fmt_re = "%f %f %f %f re\n";
static const char *fmt_f = "f\n";
static const char *fmt_s = "s\n";
static const char *fmt_g = "%f g\n";
static const char *fmt_m = "%f %f m\n";
static const char *fmt_l = "%f %f l\n";
static const char *fmt_w = "%f w\n";
static const char *fmt_Tx_BMC = "/Tx BMC\n";
static const char *fmt_q = "q\n";
static const char *fmt_W = "W\n";
static const char *fmt_n = "n\n";
static const char *fmt_BT = "BT\n";
static const char *fmt_Tm = "%1.2f %1.2f %1.2f %1.2f %1.2f %1.2f Tm\n";
static const char *fmt_Td = "%f %f Td\n";
static const char *fmt_Tj = " Tj\n";
static const char *fmt_ET = "ET\n";
static const char *fmt_Q = "Q\n";
static const char *fmt_EMC = "EMC\n";

void pdf_da_info_fin(fz_context *ctx, pdf_da_info *di)
{
	fz_free(ctx, di->font_name);
	di->font_name = NULL;
}

static void da_check_stack(float *stack, int *top)
{
	if (*top == 32)
	{
		memmove(stack, stack + 1, 31 * sizeof(stack[0]));
		*top = 31;
	}
}

void pdf_parse_da(fz_context *ctx, char *da, pdf_da_info *di)
{
	float stack[32];
	int top = 0;
	pdf_token tok;
	char *name = NULL;
	pdf_lexbuf lbuf;
	fz_stream *str = fz_open_memory(ctx, (unsigned char *)da, strlen(da));

	pdf_lexbuf_init(ctx, &lbuf, PDF_LEXBUF_SMALL);

	fz_var(str);
	fz_var(name);
	fz_try(ctx)
	{
		for (tok = pdf_lex(str, &lbuf); tok != PDF_TOK_EOF; tok = pdf_lex(str, &lbuf))
		{
			switch (tok)
			{
			case PDF_TOK_NAME:
				fz_free(ctx, name);
				name = fz_strdup(ctx, lbuf.scratch);
				break;

			case PDF_TOK_INT:
				da_check_stack(stack, &top);
				stack[top] = lbuf.i;
				top ++;
				break;

			case PDF_TOK_REAL:
				da_check_stack(stack, &top);
				stack[top] = lbuf.f;
				top ++;
				break;

			case PDF_TOK_KEYWORD:
				if (!strcmp(lbuf.scratch, "Tf"))
				{
					di->font_size = stack[0];
					di->font_name = name;
					name = NULL;
				}
				else if (!strcmp(lbuf.scratch, "rg"))
				{
					di->col[0] = stack[0];
					di->col[1] = stack[1];
					di->col[2] = stack[2];
					di->col_size = 3;
				}

				fz_free(ctx, name);
				name = NULL;
				top = 0;
				break;

			default:
				break;
			}
		}
	}
	fz_always(ctx)
	{
		fz_free(ctx, name);
		fz_close(str);
		pdf_lexbuf_fin(&lbuf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void get_font_info(pdf_document *doc, pdf_obj *dr, char *da, font_info *font_rec)
{
	fz_context *ctx = doc->ctx;

	pdf_parse_da(ctx, da, &font_rec->da_rec);
	if (font_rec->da_rec.font_name == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "No font name in default appearance");
	font_rec->font = pdf_load_font(doc, dr, pdf_dict_gets(pdf_dict_gets(dr, "Font"), font_rec->da_rec.font_name), 0);
}

static void font_info_fin(fz_context *ctx, font_info *font_rec)
{
	pdf_drop_font(ctx, font_rec->font);
	font_rec->font = NULL;
	pdf_da_info_fin(ctx, &font_rec->da_rec);
}

static void get_text_widget_info(pdf_document *doc, pdf_obj *widget, text_widget_info *info)
{
	char *da = pdf_to_str_buf(pdf_get_inheritable(doc, widget, "DA"));
	int ff = pdf_get_field_flags(doc, widget);
	pdf_obj *ml = pdf_get_inheritable(doc, widget, "MaxLen");

	info->dr = pdf_get_inheritable(doc, widget, "DR");
	info->col = pdf_dict_getp(widget, "MK/BG");
	info->q = pdf_to_int(pdf_get_inheritable(doc, widget, "Q"));
	info->multiline = (ff & Ff_Multiline) != 0;
	info->comb = (ff & (Ff_Multiline|Ff_Password|Ff_FileSelect|Ff_Comb)) == Ff_Comb;

	if (ml == NULL)
		info->comb = 0;
	else
		info->max_len = pdf_to_int(ml);

	get_font_info(doc, info->dr, da, &info->font_rec);
}

void pdf_fzbuf_print_da(fz_context *ctx, fz_buffer *fzbuf, pdf_da_info *di)
{
	if (di->font_name != NULL && di->font_size != 0)
		fz_buffer_printf(ctx, fzbuf, "/%s %d Tf", di->font_name, di->font_size);

	switch (di->col_size)
	{
	case 1:
		fz_buffer_printf(ctx, fzbuf, " %f g", di->col[0]);
		break;

	case 3:
		fz_buffer_printf(ctx, fzbuf, " %f %f %f rg", di->col[0], di->col[1], di->col[2]);
		break;

	case 4:
		fz_buffer_printf(ctx, fzbuf, " %f %f %f %f k", di->col[0], di->col[1], di->col[2], di->col[3]);
		break;

	default:
		fz_buffer_printf(ctx, fzbuf, " 0 g");
		break;
	}
}

static fz_rect *measure_text(pdf_document *doc, font_info *font_rec, const fz_matrix *tm, char *text, fz_rect *bbox)
{
	pdf_measure_text(doc->ctx, font_rec->font, (unsigned char *)text, strlen(text), bbox);

	bbox->x0 *= font_rec->da_rec.font_size * tm->a;
	bbox->y0 *= font_rec->da_rec.font_size * tm->d;
	bbox->x1 *= font_rec->da_rec.font_size * tm->a;
	bbox->y1 *= font_rec->da_rec.font_size * tm->d;

	return bbox;
}

static void fzbuf_print_color(fz_context *ctx, fz_buffer *fzbuf, pdf_obj *arr, int stroke, float adj)
{
	switch (pdf_array_len(arr))
	{
	case 1:
		fz_buffer_printf(ctx, fzbuf, stroke?"%f G\n":"%f g\n",
			pdf_to_real(pdf_array_get(arr, 0)) + adj);
		break;
	case 3:
		fz_buffer_printf(ctx, fzbuf, stroke?"%f %f %f RG\n":"%f %f %f rg\n",
			pdf_to_real(pdf_array_get(arr, 0)) + adj,
			pdf_to_real(pdf_array_get(arr, 1)) + adj,
			pdf_to_real(pdf_array_get(arr, 2)) + adj);
		break;
	case 4:
		fz_buffer_printf(ctx, fzbuf, stroke?"%f %f %f %f K\n":"%f %f %f %f k\n",
			pdf_to_real(pdf_array_get(arr, 0)),
			pdf_to_real(pdf_array_get(arr, 1)),
			pdf_to_real(pdf_array_get(arr, 2)),
			pdf_to_real(pdf_array_get(arr, 3)));
		break;
	}
}

static void fzbuf_print_text(fz_context *ctx, fz_buffer *fzbuf, const fz_rect *clip, pdf_obj *col, font_info *font_rec, const fz_matrix *tm, char *text)
{
	fz_buffer_printf(ctx, fzbuf, fmt_q);
	if (clip)
	{
		fz_buffer_printf(ctx, fzbuf, fmt_re, clip->x0, clip->y0, clip->x1 - clip->x0, clip->y1 - clip->y0);
		fz_buffer_printf(ctx, fzbuf, fmt_W);
		if (col)
		{
			fzbuf_print_color(ctx, fzbuf, col, 0, 0.0);
			fz_buffer_printf(ctx, fzbuf, fmt_f);
		}
		else
		{
			fz_buffer_printf(ctx, fzbuf, fmt_n);
		}
	}

	fz_buffer_printf(ctx, fzbuf, fmt_BT);

	pdf_fzbuf_print_da(ctx, fzbuf, &font_rec->da_rec);

	fz_buffer_printf(ctx, fzbuf, "\n");
	if (tm)
		fz_buffer_printf(ctx, fzbuf, fmt_Tm, tm->a, tm->b, tm->c, tm->d, tm->e, tm->f);

	fz_buffer_cat_pdf_string(ctx, fzbuf, text);
	fz_buffer_printf(ctx, fzbuf, fmt_Tj);
	fz_buffer_printf(ctx, fzbuf, fmt_ET);
	fz_buffer_printf(ctx, fzbuf, fmt_Q);
}

static fz_buffer *create_text_buffer(fz_context *ctx, const fz_rect *clip, text_widget_info *info, const fz_matrix *tm, char *text)
{
	fz_buffer *fzbuf = fz_new_buffer(ctx, 0);

	fz_try(ctx)
	{
		fz_buffer_printf(ctx, fzbuf, fmt_Tx_BMC);
		fzbuf_print_text(ctx, fzbuf, clip, info->col, &info->font_rec, tm, text);
		fz_buffer_printf(ctx, fzbuf, fmt_EMC);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, fzbuf);
		fz_rethrow(ctx);
	}

	return fzbuf;
}

static fz_buffer *create_aligned_text_buffer(pdf_document *doc, const fz_rect *clip, text_widget_info *info, const fz_matrix *tm, char *text)
{
	fz_context *ctx = doc->ctx;
	fz_matrix atm = *tm;

	if (info->q != Q_Left)
	{
		fz_rect rect;

		measure_text(doc, &info->font_rec, tm, text, &rect);
		atm.e -= info->q == Q_Right ? rect.x1 : (rect.x1 - rect.x0) / 2;
	}

	return create_text_buffer(ctx, clip, info, &atm, text);
}

static void measure_ascent_descent(pdf_document *doc, font_info *finf, char *text, float *ascent, float *descent)
{
	fz_context *ctx = doc->ctx;
	char *testtext = NULL;
	fz_rect bbox;
	font_info tinf = *finf;

	fz_var(testtext);
	fz_try(ctx)
	{
		/* Heuristic: adding "My" to text will in most cases
		 * produce a measurement that will encompass all chars */
		testtext = fz_malloc(ctx, strlen(text) + 3);
		strcpy(testtext, "My");
		strcat(testtext, text);
		tinf.da_rec.font_size = 1;
		measure_text(doc, &tinf, &fz_identity, testtext, &bbox);
		*descent = -bbox.y0;
		*ascent = bbox.y1;
	}
	fz_always(ctx)
	{
		fz_free(ctx, testtext);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

typedef struct text_splitter_s
{
	font_info *info;
	float width;
	float height;
	float scale;
	float unscaled_width;
	float fontsize;
	char *text;
	int done;
	float x_orig;
	float y_orig;
	float x;
	float x_end;
	int text_start;
	int text_end;
	int max_lines;
	int retry;
} text_splitter;

static void text_splitter_init(text_splitter *splitter, font_info *info, char *text, float width, float height, int variable)
{
	float fontsize = info->da_rec.font_size;

	memset(splitter, 0, sizeof(*splitter));
	splitter->info = info;
	splitter->text = text;
	splitter->width = width;
	splitter->unscaled_width = width;
	splitter->height = height;
	splitter->fontsize = fontsize;
	splitter->scale = 1.0;
	/* RJW: The cast in the following line is important, as otherwise
	 * under MSVC in the variable = 0 case, splitter->max_lines becomes
	 * INT_MIN. */
	splitter->max_lines = variable ? (int)(height/fontsize) : INT_MAX;
}

static void text_splitter_start_pass(text_splitter *splitter)
{
	splitter->text_end = 0;
	splitter->x_orig = 0;
	splitter->y_orig = 0;
}

static void text_splitter_start_line(text_splitter *splitter)
{
	splitter->x_end = 0;
}

static int text_splitter_layout(fz_context *ctx, text_splitter *splitter)
{
	char *text;
	float room;
	float stride;
	int count;
	int len;
	float fontsize = splitter->info->da_rec.font_size;

	splitter->x = splitter->x_end;
	splitter->text_start = splitter->text_end;

	text = splitter->text + splitter->text_start;
	room = splitter->unscaled_width - splitter->x;

	if (strchr("\r\n", text[0]))
	{
		/* Consume return chars and report end of line */
		splitter->text_end += strspn(text, "\r\n");
		splitter->text_start = splitter->text_end;
		splitter->done = (splitter->text[splitter->text_end] == '\0');
		return 0;
	}
	else if (text[0] == ' ')
	{
		/* Treat each space as a word */
		len = 1;
	}
	else
	{
		len = 0;
		while (text[len] != '\0' && !strchr(" \r\n", text[len]))
			len ++;
	}

	stride = pdf_text_stride(ctx, splitter->info->font, fontsize, (unsigned char *)text, len, room, &count);

	/* If not a single char fits although the line is empty, then force one char */
	if (count == 0 && splitter->x == 0.0)
		stride = pdf_text_stride(ctx, splitter->info->font, fontsize, (unsigned char *)text, 1, FLT_MAX, &count);

	if (count < len && splitter->retry)
	{
		/* The word didn't fit and we are in retry mode. Work out the
		 * least additional scaling that may help */
		float fitwidth; /* width if we force the word in */
		float hstretchwidth; /* width if we just bump by 10% */
		float vstretchwidth; /* width resulting from forcing in another line */
		float bestwidth;

		fitwidth = splitter->x +
			pdf_text_stride(ctx, splitter->info->font, fontsize, (unsigned char *)text, len, FLT_MAX, &count);
		/* FIXME: temporary fiddle factor. Would be better to work in integers */
		fitwidth *= 1.001f;

		/* Stretching by 10% is worth trying only if processing the first word on the line */
		hstretchwidth = splitter->x == 0.0
			? splitter->width * 1.1 / splitter->scale
			: FLT_MAX;

		vstretchwidth = splitter->width * (splitter->max_lines + 1) * splitter->fontsize
			/ splitter->height;

		bestwidth = fz_min(fitwidth, fz_min(hstretchwidth, vstretchwidth));

		if (bestwidth == vstretchwidth)
			splitter->max_lines ++;

		splitter->scale = splitter->width / bestwidth;
		splitter->unscaled_width = bestwidth;

		splitter->retry = 0;

		/* Try again */
		room = splitter->unscaled_width - splitter->x;
		stride = pdf_text_stride(ctx, splitter->info->font, fontsize, (unsigned char *)text, len, room, &count);
	}

	/* This is not the first word on the line. Best to give up on this line and push
	 * the word onto the next */
	if (count < len && splitter->x > 0.0)
		return 0;

	splitter->text_end = splitter->text_start + count;
	splitter->x_end = splitter->x + stride;
	splitter->done = (splitter->text[splitter->text_end] == '\0');
	return 1;
}

static void text_splitter_move(text_splitter *splitter, float newy, float *relx, float *rely)
{
	*relx = splitter->x - splitter->x_orig;
	*rely = newy - splitter->y_orig;

	splitter->x_orig = splitter->x;
	splitter->y_orig = newy;
}

static void text_splitter_retry(text_splitter *splitter)
{
	if (splitter->retry)
	{
		/* Already tried expanding lines. Overflow must
		 * be caused by carriage control */
		splitter->max_lines ++;
		splitter->retry = 0;
		splitter->unscaled_width = splitter->width * splitter->max_lines * splitter->fontsize
			/ splitter->height;
		splitter->scale = splitter->width / splitter->unscaled_width;
	}
	else
	{
		splitter->retry = 1;
	}
}

static void fzbuf_print_text_start(fz_context *ctx, fz_buffer *fzbuf, const fz_rect *clip, pdf_obj *col, font_info *font, const fz_matrix *tm)
{
	fz_buffer_printf(ctx, fzbuf, fmt_Tx_BMC);
	fz_buffer_printf(ctx, fzbuf, fmt_q);

	if (clip)
	{
		fz_buffer_printf(ctx, fzbuf, fmt_re, clip->x0, clip->y0, clip->x1 - clip->x0, clip->y1 - clip->y0);
		fz_buffer_printf(ctx, fzbuf, fmt_W);
		if (col)
		{
			fzbuf_print_color(ctx, fzbuf, col, 0, 0.0);
			fz_buffer_printf(ctx, fzbuf, fmt_f);
		}
		else
		{
			fz_buffer_printf(ctx, fzbuf, fmt_n);
		}
	}

	fz_buffer_printf(ctx, fzbuf, fmt_BT);

	pdf_fzbuf_print_da(ctx, fzbuf, &font->da_rec);
	fz_buffer_printf(ctx, fzbuf, "\n");

	fz_buffer_printf(ctx, fzbuf, fmt_Tm, tm->a, tm->b, tm->c, tm->d, tm->e, tm->f);
}

static void fzbuf_print_text_end(fz_context *ctx, fz_buffer *fzbuf)
{
	fz_buffer_printf(ctx, fzbuf, fmt_ET);
	fz_buffer_printf(ctx, fzbuf, fmt_Q);
	fz_buffer_printf(ctx, fzbuf, fmt_EMC);
}

static void fzbuf_print_text_word(fz_context *ctx, fz_buffer *fzbuf, float x, float y, char *text, int count)
{
	int i;

	fz_buffer_printf(ctx, fzbuf, fmt_Td, x, y);
	fz_buffer_printf(ctx, fzbuf, "(");

	for (i = 0; i < count; i++)
		fz_buffer_printf(ctx, fzbuf, "%c", text[i]);

	fz_buffer_printf(ctx, fzbuf, ") Tj\n");
}

static fz_buffer *create_text_appearance(pdf_document *doc, const fz_rect *bbox, const fz_matrix *oldtm, text_widget_info *info, char *text)
{
	fz_context *ctx = doc->ctx;
	int fontsize;
	int variable;
	float height, width, full_width;
	fz_buffer *fzbuf = NULL;
	fz_buffer *fztmp = NULL;
	fz_rect rect;
	fz_rect tbox;
	rect = *bbox;

	if (rect.x1 - rect.x0 > 3.0 && rect.y1 - rect.y0 > 3.0)
	{
		rect.x0 += 1.0;
		rect.x1 -= 1.0;
		rect.y0 += 1.0;
		rect.y1 -= 1.0;
	}

	height = rect.y1 - rect.y0;
	width = rect.x1 - rect.x0;
	full_width = bbox->x1 - bbox->x0;

	fz_var(fzbuf);
	fz_var(fztmp);
	fz_try(ctx)
	{
		float ascent, descent;
		fz_matrix tm;

		variable = (info->font_rec.da_rec.font_size == 0);
		fontsize = variable
			? (info->multiline ? 14.0 : floor(height))
			: info->font_rec.da_rec.font_size;

		info->font_rec.da_rec.font_size = fontsize;

		measure_ascent_descent(doc, &info->font_rec, text, &ascent, &descent);

		if (info->multiline)
		{
			text_splitter splitter;

			text_splitter_init(&splitter, &info->font_rec, text, width, height, variable);

			while (!splitter.done)
			{
				/* Try a layout pass */
				int line = 0;

				fz_drop_buffer(ctx, fztmp);
				fztmp = NULL;
				fztmp = fz_new_buffer(ctx, 0);

				text_splitter_start_pass(&splitter);

				/* Layout unscaled text to a scaled-up width, so that
				 * the scaled-down text will fit the unscaled width */

				while (!splitter.done && line < splitter.max_lines)
				{
					/* Layout a line */
					text_splitter_start_line(&splitter);

					while (!splitter.done && text_splitter_layout(ctx, &splitter))
					{
						if (splitter.text[splitter.text_start] != ' ')
						{
							float x, y;
							char *word = text+splitter.text_start;
							int wordlen = splitter.text_end-splitter.text_start;

							text_splitter_move(&splitter, -line*fontsize, &x, &y);
							fzbuf_print_text_word(ctx, fztmp, x, y, word, wordlen);
						}
					}

					line ++;
				}

				if (!splitter.done)
					text_splitter_retry(&splitter);
			}

			fzbuf = fz_new_buffer(ctx, 0);

			tm.a = splitter.scale;
			tm.b = 0.0;
			tm.c = 0.0;
			tm.d = splitter.scale;
			tm.e = rect.x0;
			tm.f = rect.y1 - (1.0+ascent-descent)*fontsize*splitter.scale/2.0;

			fzbuf_print_text_start(ctx, fzbuf, &rect, info->col, &info->font_rec, &tm);

			fz_buffer_cat(ctx, fzbuf, fztmp);

			fzbuf_print_text_end(ctx, fzbuf);
		}
		else if (info->comb)
		{
			int i, n = fz_mini((int)strlen(text), info->max_len);
			float comb_width = full_width/info->max_len;
			float char_width = pdf_text_stride(ctx, info->font_rec.font, fontsize, (unsigned char *)"M", 1, FLT_MAX, NULL);
			float init_skip = (comb_width - char_width)/2.0;

			fz_translate(&tm, rect.x0, rect.y1 - (height+(ascent-descent)*fontsize)/2.0);

			fzbuf = fz_new_buffer(ctx, 0);

			fzbuf_print_text_start(ctx, fzbuf, &rect, info->col, &info->font_rec, &tm);

			for (i = 0; i < n; i++)
				fzbuf_print_text_word(ctx, fzbuf, i == 0 ? init_skip : comb_width, 0.0, text+i, 1);

			fzbuf_print_text_end(ctx, fzbuf);
		}
		else
		{
			if (oldtm)
			{
				tm = *oldtm;
			}
			else
			{
				fz_translate(&tm, rect.x0, rect.y1 - (height+(ascent-descent)*fontsize)/2.0);

				switch (info->q)
				{
				case Q_Right: tm.e += width; break;
				case Q_Cent: tm.e += width/2; break;
				}
			}

			if (variable)
			{
				measure_text(doc, &info->font_rec, &tm, text, &tbox);

				if (tbox.x1 - tbox.x0 > width)
				{
					/* Scale the text to fit but use the same offset
					* to keep the baseline constant */
					tm.a *= width / (tbox.x1 - tbox.x0);
					tm.d *= width / (tbox.x1 - tbox.x0);
				}
			}

			fzbuf = create_aligned_text_buffer(doc, &rect, info, &tm, text);
		}
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, fztmp);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, fzbuf);
		fz_rethrow(ctx);
	}

	return fzbuf;
}

static int get_matrix(pdf_document *doc, pdf_xobject *form, int q, fz_matrix *mt)
{
	fz_context *ctx = doc->ctx;
	int found = 0;
	pdf_lexbuf lbuf;
	fz_stream *str;

	str = pdf_open_stream(doc, pdf_to_num(form->contents), pdf_to_gen(form->contents));

	pdf_lexbuf_init(ctx, &lbuf, PDF_LEXBUF_SMALL);

	fz_try(ctx)
	{
		int tok;
		float coefs[MATRIX_COEFS];
		int coef_i = 0;

		/* Look for the text matrix Tm in the stream */
		for (tok = pdf_lex(str, &lbuf); tok != PDF_TOK_EOF; tok = pdf_lex(str, &lbuf))
		{
			if (tok == PDF_TOK_INT || tok == PDF_TOK_REAL)
			{
				if (coef_i >= MATRIX_COEFS)
				{
					int i;
					for (i = 0; i < MATRIX_COEFS-1; i++)
						coefs[i] = coefs[i+1];

					coef_i = MATRIX_COEFS-1;
				}

				coefs[coef_i++] = tok == PDF_TOK_INT ? lbuf.i : lbuf.f;
			}
			else
			{
				if (tok == PDF_TOK_KEYWORD && !strcmp(lbuf.scratch, "Tm") && coef_i == MATRIX_COEFS)
				{
					found = 1;
					mt->a = coefs[0];
					mt->b = coefs[1];
					mt->c = coefs[2];
					mt->d = coefs[3];
					mt->e = coefs[4];
					mt->f = coefs[5];
				}

				coef_i = 0;
			}
		}

		if (found)
		{
			fz_rect bbox;
			pdf_to_rect(ctx, pdf_dict_gets(form->contents, "BBox"), &bbox);

			switch (q)
			{
			case Q_Left:
				mt->e = bbox.x0 + 1;
				break;

			case Q_Cent:
				mt->e = (bbox.x1 - bbox.x0) / 2;
				break;

			case Q_Right:
				mt->e = bbox.x1 - 1;
				break;
			}
		}
	}
	fz_always(ctx)
	{
		fz_close(str);
		pdf_lexbuf_fin(&lbuf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return found;
}

static char *to_font_encoding(fz_context *ctx, pdf_font_desc *font, char *utf8)
{
	int i;
	int needs_converting = 0;

	/* Temporay partial solution. We are using a slow lookup in the conversion
	 * below, so we avoid performing the conversion unnecessarily. We check for
	 * top-bit-set chars, and convert only if they are present. We should also
	 * check that the font encoding is one that agrees with utf8 from 0 to 7f,
	 * but for now we get away without doing so. This is after all an improvement
	 * on just strdup */
	for (i = 0; utf8[i] != '\0'; i++)
	{
		if (utf8[i] & 0x80)
			needs_converting = 1;
	}

	/* Even if we need to convert, we cannot do so if the font has no cid_to_ucs mapping */
	if (needs_converting && font->cid_to_ucs)
	{
		char *buf = fz_malloc(ctx, strlen(utf8) + 1);
		char *bufp = buf;

		fz_try(ctx)
		{
			while(*utf8)
			{
				if (*utf8 & 0x80)
				{
					int rune;

					utf8 += fz_chartorune(&rune, utf8);

					/* Slow search for the cid that maps to the unicode value held in 'rune" */
					for (i = 0; i < font->cid_to_ucs_len && font->cid_to_ucs[i] != rune; i++)
						;

					/* If found store the cid */
					if (i < font->cid_to_ucs_len)
						*bufp++ = i;
				}
				else
				{
					*bufp++ = *utf8++;
				}
			}

			*bufp = '\0';
		}
		fz_catch(ctx)
		{
			fz_free(ctx, buf);
			fz_rethrow(ctx);
		}

		return buf;
	}
	else
	{
		/* If either no conversion is needed or the font has no cid_to_ucs
		 * mapping then leave unconverted, although in the latter case the result
		 * is likely incorrect */
		return fz_strdup(ctx, utf8);
	}
}

static void account_for_rot(fz_rect *rect, fz_matrix *mat, int rot)
{
	float width = rect->x1;
	float height = rect->y1;

	switch (rot)
	{
	default:
		*mat = fz_identity;
		break;
	case 90:
		fz_pre_rotate(fz_translate(mat, width, 0), rot);
		rect->x1 = height;
		rect->y1 = width;
		break;
	case 180:
		fz_pre_rotate(fz_translate(mat, width, height), rot);
		break;
	case 270:
		fz_pre_rotate(fz_translate(mat, 0, height), rot);
		rect->x1 = height;
		rect->y1 = width;
		break;
	}
}

static void copy_resources(pdf_obj *dst, pdf_obj *src)
{
	int i, len;

	len = pdf_dict_len(src);
	for (i = 0; i < len; i++)
	{
		pdf_obj *key = pdf_dict_get_key(src, i);

		if (!pdf_dict_get(dst, key))
			pdf_dict_put(dst, key, pdf_dict_get_val(src, i));
	}
}

static pdf_xobject *load_or_create_form(pdf_document *doc, pdf_obj *obj, fz_rect *rect)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *ap = NULL;
	fz_matrix mat;
	int rot;
	pdf_obj *formobj = NULL;
	pdf_xobject *form = NULL;
	char *dn = "N";
	fz_buffer *fzbuf = NULL;
	int create_form = 0;

	fz_var(formobj);
	fz_var(form);
	fz_var(fzbuf);
	fz_try(ctx)
	{
		rot = pdf_to_int(pdf_dict_getp(obj, "MK/R"));
		pdf_to_rect(ctx, pdf_dict_gets(obj, "Rect"), rect);
		rect->x1 -= rect->x0;
		rect->y1 -= rect->y0;
		rect->x0 = rect->y0 = 0;
		account_for_rot(rect, &mat, rot);

		ap = pdf_dict_gets(obj, "AP");
		if (ap == NULL)
		{
			ap = pdf_new_dict(doc, 1);
			pdf_dict_puts_drop(obj, "AP", ap);
		}

		formobj = pdf_dict_gets(ap, dn);
		if (formobj == NULL)
		{
			formobj = pdf_new_xobject(doc, rect, &mat);
			pdf_dict_puts_drop(ap, dn, formobj);
			create_form = 1;
		}

		form = pdf_load_xobject(doc, formobj);
		if (create_form)
		{
			fzbuf = fz_new_buffer(ctx, 1);
			pdf_update_xobject_contents(doc, form, fzbuf);
		}

		copy_resources(form->resources, pdf_get_inheritable(doc, obj, "DR"));
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, fzbuf);
	}
	fz_catch(ctx)
	{
		pdf_drop_xobject(ctx, form);
		fz_rethrow(ctx);
	}

	return form;
}

static void update_marked_content(pdf_document *doc, pdf_xobject *form, fz_buffer *fzbuf)
{
	fz_context *ctx = doc->ctx;
	pdf_token tok;
	pdf_lexbuf lbuf;
	fz_stream *str_outer = NULL;
	fz_stream *str_inner = NULL;
	unsigned char *buf;
	int len;
	fz_buffer *newbuf = NULL;

	pdf_lexbuf_init(ctx, &lbuf, PDF_LEXBUF_SMALL);

	fz_var(str_outer);
	fz_var(str_inner);
	fz_var(newbuf);
	fz_try(ctx)
	{
		int bmc_found;
		int first = 1;

		newbuf = fz_new_buffer(ctx, 0);
		str_outer = pdf_open_stream(doc, pdf_to_num(form->contents), pdf_to_gen(form->contents));
		len = fz_buffer_storage(ctx, fzbuf, &buf);
		str_inner = fz_open_memory(ctx, buf, len);

		/* Copy the existing appearance stream to newbuf while looking for BMC */
		for (tok = pdf_lex(str_outer, &lbuf); tok != PDF_TOK_EOF; tok = pdf_lex(str_outer, &lbuf))
		{
			if (first)
				first = 0;
			else
				fz_buffer_printf(ctx, newbuf, " ");

			pdf_print_token(ctx, newbuf, tok, &lbuf);
			if (tok == PDF_TOK_KEYWORD && !strcmp(lbuf.scratch, "BMC"))
				break;
		}

		bmc_found = (tok != PDF_TOK_EOF);

		if (bmc_found)
		{
			/* Drop Tx BMC from the replacement appearance stream */
			(void)pdf_lex(str_inner, &lbuf);
			(void)pdf_lex(str_inner, &lbuf);
		}

		/* Copy the replacement appearance stream to newbuf */
		for (tok = pdf_lex(str_inner, &lbuf); tok != PDF_TOK_EOF; tok = pdf_lex(str_inner, &lbuf))
		{
			fz_buffer_printf(ctx, newbuf, " ");
			pdf_print_token(ctx, newbuf, tok, &lbuf);
		}

		if (bmc_found)
		{
			/* Drop the rest of the existing appearance stream until EMC found */
			for (tok = pdf_lex(str_outer, &lbuf); tok != PDF_TOK_EOF; tok = pdf_lex(str_outer, &lbuf))
			{
				if (tok == PDF_TOK_KEYWORD && !strcmp(lbuf.scratch, "EMC"))
					break;
			}

			/* Copy the rest of the existing appearance stream to newbuf */
			for (tok = pdf_lex(str_outer, &lbuf); tok != PDF_TOK_EOF; tok = pdf_lex(str_outer, &lbuf))
			{
				fz_buffer_printf(ctx, newbuf, " ");
				pdf_print_token(ctx, newbuf, tok, &lbuf);
			}
		}

		/* Use newbuf in place of the existing appearance stream */
		pdf_update_xobject_contents(doc, form, newbuf);
	}
	fz_always(ctx)
	{
		fz_close(str_outer);
		fz_close(str_inner);
		fz_drop_buffer(ctx, newbuf);
		pdf_lexbuf_fin(&lbuf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static int get_border_style(pdf_obj *obj)
{
	char *sname = pdf_to_name(pdf_dict_getp(obj, "BS/S"));

	if (!strcmp(sname, "D"))
		return BS_Dashed;
	else if (!strcmp(sname, "B"))
		return BS_Beveled;
	else if (!strcmp(sname, "I"))
		return BS_Inset;
	else if (!strcmp(sname, "U"))
		return BS_Underline;
	else
		return BS_Solid;
}

static float get_border_width(pdf_obj *obj)
{
	float w = pdf_to_real(pdf_dict_getp(obj, "BS/W"));
	return w == 0.0 ? 1.0 : w;
}

void pdf_update_text_appearance(pdf_document *doc, pdf_obj *obj, char *eventValue)
{
	fz_context *ctx = doc->ctx;
	text_widget_info info;
	pdf_xobject *form = NULL;
	fz_buffer *fzbuf = NULL;
	fz_matrix tm;
	fz_rect rect;
	int has_tm;
	char *text = NULL;

	memset(&info, 0, sizeof(info));

	fz_var(info);
	fz_var(form);
	fz_var(fzbuf);
	fz_var(text);
	fz_try(ctx)
	{
		get_text_widget_info(doc, obj, &info);

		if (eventValue)
			text = to_font_encoding(ctx, info.font_rec.font, eventValue);
		else
			text = pdf_field_value(doc, obj);

		form = load_or_create_form(doc, obj, &rect);

		has_tm = get_matrix(doc, form, info.q, &tm);
		fzbuf = create_text_appearance(doc, &form->bbox, has_tm ? &tm : NULL, &info,
			text?text:"");
		update_marked_content(doc, form, fzbuf);
	}
	fz_always(ctx)
	{
		fz_free(ctx, text);
		pdf_drop_xobject(ctx, form);
		fz_drop_buffer(ctx, fzbuf);
		font_info_fin(ctx, &info.font_rec);
	}
	fz_catch(ctx)
	{
		fz_warn(ctx, "update_text_appearance failed");
	}
}

void pdf_update_combobox_appearance(pdf_document *doc, pdf_obj *obj)
{
	fz_context *ctx = doc->ctx;
	text_widget_info info;
	pdf_xobject *form = NULL;
	fz_buffer *fzbuf = NULL;
	fz_matrix tm;
	fz_rect rect;
	int has_tm;
	pdf_obj *val;
	char *text;

	memset(&info, 0, sizeof(info));

	fz_var(info);
	fz_var(form);
	fz_var(fzbuf);
	fz_try(ctx)
	{
		get_text_widget_info(doc, obj, &info);

		val = pdf_get_inheritable(doc, obj, "V");

		if (pdf_is_array(val))
			val = pdf_array_get(val, 0);

		text = pdf_to_str_buf(val);

		if (!text)
			text = "";

		form = load_or_create_form(doc, obj, &rect);

		has_tm = get_matrix(doc, form, info.q, &tm);
		fzbuf = create_text_appearance(doc, &form->bbox, has_tm ? &tm : NULL, &info,
			text?text:"");
		update_marked_content(doc, form, fzbuf);
	}
	fz_always(ctx)
	{
		pdf_drop_xobject(ctx, form);
		fz_drop_buffer(ctx, fzbuf);
		font_info_fin(ctx, &info.font_rec);
	}
	fz_catch(ctx)
	{
		fz_warn(ctx, "update_text_appearance failed");
	}
}

void pdf_update_pushbutton_appearance(pdf_document *doc, pdf_obj *obj)
{
	fz_context *ctx = doc->ctx;
	fz_rect rect;
	pdf_xobject *form = NULL;
	fz_buffer *fzbuf = NULL;
	pdf_obj *tobj = NULL;
	font_info font_rec;
	int bstyle;
	float bwidth;
	float btotal;

	memset(&font_rec, 0, sizeof(font_rec));

	fz_var(font_rec);
	fz_var(form);
	fz_var(fzbuf);
	fz_try(ctx)
	{
		form = load_or_create_form(doc, obj, &rect);
		fzbuf = fz_new_buffer(ctx, 0);
		tobj = pdf_dict_getp(obj, "MK/BG");
		if (pdf_is_array(tobj))
		{
			fzbuf_print_color(ctx, fzbuf, tobj, 0, 0.0);
			fz_buffer_printf(ctx, fzbuf, fmt_re,
				rect.x0, rect.y0, rect.x1, rect.y1);
			fz_buffer_printf(ctx, fzbuf, fmt_f);
		}
		bstyle = get_border_style(obj);
		bwidth = get_border_width(obj);
		btotal = bwidth;
		if (bstyle == BS_Beveled || bstyle == BS_Inset)
		{
			btotal += bwidth;

			if (bstyle == BS_Beveled)
				fz_buffer_printf(ctx, fzbuf, fmt_g, 1.0);
			else
				fz_buffer_printf(ctx, fzbuf, fmt_g, 0.33);
			fz_buffer_printf(ctx, fzbuf, fmt_m, bwidth, bwidth);
			fz_buffer_printf(ctx, fzbuf, fmt_l, bwidth, rect.y1 - bwidth);
			fz_buffer_printf(ctx, fzbuf, fmt_l, rect.x1 - bwidth, rect.y1 - bwidth);
			fz_buffer_printf(ctx, fzbuf, fmt_l, rect.x1 - 2 * bwidth, rect.y1 - 2 * bwidth);
			fz_buffer_printf(ctx, fzbuf, fmt_l, 2 * bwidth, rect.y1 - 2 * bwidth);
			fz_buffer_printf(ctx, fzbuf, fmt_l, 2 * bwidth, 2 * bwidth);
			fz_buffer_printf(ctx, fzbuf, fmt_f);
			if (bstyle == BS_Beveled)
				fzbuf_print_color(ctx, fzbuf, tobj, 0, -0.25);
			else
				fz_buffer_printf(ctx, fzbuf, fmt_g, 0.66);
			fz_buffer_printf(ctx, fzbuf, fmt_m, rect.x1 - bwidth, rect.y1 - bwidth);
			fz_buffer_printf(ctx, fzbuf, fmt_l, rect.x1 - bwidth, bwidth);
			fz_buffer_printf(ctx, fzbuf, fmt_l, bwidth, bwidth);
			fz_buffer_printf(ctx, fzbuf, fmt_l, 2 * bwidth, 2 * bwidth);
			fz_buffer_printf(ctx, fzbuf, fmt_l, rect.x1 - 2 * bwidth, 2 * bwidth);
			fz_buffer_printf(ctx, fzbuf, fmt_l, rect.x1 - 2 * bwidth, rect.y1 - 2 * bwidth);
			fz_buffer_printf(ctx, fzbuf, fmt_f);
		}

		tobj = pdf_dict_getp(obj, "MK/BC");
		if (tobj)
		{
			fzbuf_print_color(ctx, fzbuf, tobj, 1, 0.0);
			fz_buffer_printf(ctx, fzbuf, fmt_w, bwidth);
			fz_buffer_printf(ctx, fzbuf, fmt_re,
				bwidth/2, bwidth/2,
				rect.x1 -bwidth/2, rect.y1 - bwidth/2);
			fz_buffer_printf(ctx, fzbuf, fmt_s);
		}

		tobj = pdf_dict_getp(obj, "MK/CA");
		if (tobj)
		{
			fz_rect clip = rect;
			fz_rect bounds;
			fz_matrix mat;
			char *da = pdf_to_str_buf(pdf_get_inheritable(doc, obj, "DA"));
			char *text = pdf_to_str_buf(tobj);

			clip.x0 += btotal;
			clip.y0 += btotal;
			clip.x1 -= btotal;
			clip.y1 -= btotal;

			get_font_info(doc, form->resources, da, &font_rec);
			measure_text(doc, &font_rec, &fz_identity, text, &bounds);
			fz_translate(&mat, (rect.x1 - bounds.x1)/2, (rect.y1 - bounds.y1)/2);
			fzbuf_print_text(ctx, fzbuf, &clip, NULL, &font_rec, &mat, text);
		}

		pdf_update_xobject_contents(doc, form, fzbuf);
	}
	fz_always(ctx)
	{
		font_info_fin(ctx, &font_rec);
		fz_drop_buffer(ctx, fzbuf);
		pdf_drop_xobject(ctx, form);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void pdf_update_text_markup_appearance(pdf_document *doc, pdf_annot *annot, fz_annot_type type)
{
	float color[3];
	float alpha;
	float line_height;
	float line_thickness;

	switch (type)
	{
		case FZ_ANNOT_HIGHLIGHT:
			color[0] = 1.0;
			color[1] = 1.0;
			color[2] = 0.0;
			alpha = 0.5;
			line_thickness = 1.0;
			line_height = 0.5;
			break;
		case FZ_ANNOT_UNDERLINE:
			color[0] = 0.0;
			color[1] = 0.0;
			color[2] = 1.0;
			alpha = 1.0;
			line_thickness = LINE_THICKNESS;
			line_height = UNDERLINE_HEIGHT;
			break;
		case FZ_ANNOT_STRIKEOUT:
			color[0] = 1.0;
			color[1] = 0.0;
			color[2] = 0.0;
			alpha = 1.0;
			line_thickness = LINE_THICKNESS;
			line_height = STRIKE_HEIGHT;
			break;
		default:
			return;
	}

	pdf_set_markup_appearance(doc, annot, color, alpha, line_thickness, line_height);
}

static void update_rect(fz_context *ctx, pdf_annot *annot)
{
	pdf_to_rect(ctx, pdf_dict_gets(annot->obj, "Rect"), &annot->rect);
	annot->pagerect = annot->rect;
	fz_transform_rect(&annot->pagerect, &annot->page->ctm);
}

void pdf_set_annot_appearance(pdf_document *doc, pdf_annot *annot, fz_rect *rect, fz_display_list *disp_list)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *obj = annot->obj;
	const fz_matrix *page_ctm = &annot->page->ctm;
	fz_matrix ctm;
	fz_matrix mat = fz_identity;
	fz_device *dev = NULL;
	pdf_xobject *xobj = NULL;

	fz_invert_matrix(&ctm, page_ctm);

	fz_var(dev);
	fz_try(ctx)
	{
		pdf_obj *ap_obj;
		fz_rect trect = *rect;

		fz_transform_rect(&trect, &ctm);

		pdf_dict_puts_drop(obj, "Rect", pdf_new_rect(doc, &trect));

		/* See if there is a current normal appearance */
		ap_obj = pdf_dict_getp(obj, "AP/N");
		if (!pdf_is_stream(doc, pdf_to_num(ap_obj), pdf_to_gen(ap_obj)))
			ap_obj = NULL;

		if (ap_obj == NULL)
		{
			ap_obj = pdf_new_xobject(doc, &trect, &mat);
			pdf_dict_putp_drop(obj, "AP/N", ap_obj);
		}
		else
		{
			pdf_xref_ensure_incremental_object(doc, pdf_to_num(ap_obj));
			pdf_dict_puts_drop(ap_obj, "Rect", pdf_new_rect(doc, &trect));
			pdf_dict_puts_drop(ap_obj, "Matrix", pdf_new_matrix(doc, &mat));
		}

		dev = pdf_new_pdf_device(doc, ap_obj, pdf_dict_gets(ap_obj, "Resources"), &mat);
		fz_run_display_list(disp_list, dev, &ctm, &fz_infinite_rect, NULL);
		fz_free_device(dev);

		/* Mark the appearance as changed - required for partial update */
		xobj = pdf_load_xobject(doc, ap_obj);
		if (xobj)
		{
			xobj->iteration++;
			pdf_drop_xobject(ctx, xobj);
		}

		doc->dirty = 1;

		update_rect(ctx, annot);
	}
	fz_catch(ctx)
	{
		fz_free_device(dev);
		fz_rethrow(ctx);
	}
}

static fz_point *
quadpoints(pdf_document *doc, pdf_obj *annot, int *nout)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *quad = pdf_dict_gets(annot, "QuadPoints");
	fz_point *qp = NULL;
	int i, n;

	if (!quad)
		return NULL;

	n = pdf_array_len(quad);

	if (n%8 != 0)
		return NULL;

	fz_var(qp);
	fz_try(ctx)
	{
		qp = fz_malloc_array(ctx, n/2, sizeof(fz_point));

		for (i = 0; i < n; i += 2)
		{
			qp[i/2].x = pdf_to_real(pdf_array_get(quad, i));
			qp[i/2].y = pdf_to_real(pdf_array_get(quad, i+1));
		}
	}
	fz_catch(ctx)
	{
		fz_free(ctx, qp);
		fz_rethrow(ctx);
	}

	*nout = n/2;

	return qp;
}

void pdf_set_markup_appearance(pdf_document *doc, pdf_annot *annot, float color[3], float alpha, float line_thickness, float line_height)
{
	fz_context *ctx = doc->ctx;
	const fz_matrix *page_ctm = &annot->page->ctm;
	fz_path *path = NULL;
	fz_stroke_state *stroke = NULL;
	fz_device *dev = NULL;
	fz_display_list *strike_list = NULL;
	int i, n;
	fz_point *qp = quadpoints(doc, annot->obj, &n);

	if (!qp || n <= 0)
		return;

	fz_var(path);
	fz_var(stroke);
	fz_var(dev);
	fz_var(strike_list);
	fz_try(ctx)
	{
		fz_rect rect = fz_empty_rect;

		rect.x0 = rect.x1 = qp[0].x;
		rect.y0 = rect.y1 = qp[0].y;
		for (i = 0; i < n; i++)
			fz_include_point_in_rect(&rect, &qp[i]);

		strike_list = fz_new_display_list(ctx);
		dev = fz_new_list_device(ctx, strike_list);

		for (i = 0; i < n; i += 4)
		{
			fz_point pt0 = qp[i];
			fz_point pt1 = qp[i+1];
			fz_point up;
			float thickness;

			up.x = qp[i+2].x - qp[i+1].x;
			up.y = qp[i+2].y - qp[i+1].y;

			pt0.x += line_height * up.x;
			pt0.y += line_height * up.y;
			pt1.x += line_height * up.x;
			pt1.y += line_height * up.y;

			thickness = sqrtf(up.x * up.x + up.y * up.y) * line_thickness;

			if (!stroke || fz_abs(stroke->linewidth - thickness) < SMALL_FLOAT)
			{
				if (stroke)
				{
					// assert(path)
					fz_stroke_path(dev, path, stroke, page_ctm, fz_device_rgb(ctx), color, alpha);
					fz_drop_stroke_state(ctx, stroke);
					stroke = NULL;
					fz_free_path(ctx, path);
					path = NULL;
				}

				stroke = fz_new_stroke_state(ctx);
				stroke->linewidth = thickness;
				path = fz_new_path(ctx);
			}

			fz_moveto(ctx, path, pt0.x, pt0.y);
			fz_lineto(ctx, path, pt1.x, pt1.y);
		}

		if (stroke)
		{
			fz_stroke_path(dev, path, stroke, page_ctm, fz_device_rgb(ctx), color, alpha);
		}

		fz_transform_rect(&rect, page_ctm);
		pdf_set_annot_appearance(doc, annot, &rect, strike_list);
	}
	fz_always(ctx)
	{
		fz_free(ctx, qp);
		fz_free_device(dev);
		fz_drop_stroke_state(ctx, stroke);
		fz_free_path(ctx, path);
		fz_drop_display_list(ctx, strike_list);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static fz_colorspace *pdf_to_color(pdf_document *doc, pdf_obj *col, float color[4])
{
	fz_colorspace *cs;
	int i, ncol = pdf_array_len(col);

	switch (ncol)
	{
	case 1: cs = fz_device_gray(doc->ctx); break;
	case 3: cs = fz_device_rgb(doc->ctx); break;
	case 4: cs = fz_device_cmyk(doc->ctx); break;
	default: return NULL;
	}

	for (i = 0; i < ncol; i++)
		color[i] = pdf_to_real(pdf_array_get(col, i));

	return cs;
}

void pdf_set_ink_appearance(pdf_document *doc, pdf_annot *annot)
{
	fz_context *ctx = doc->ctx;
	const fz_matrix *page_ctm = &annot->page->ctm;
	fz_path *path = NULL;
	fz_stroke_state *stroke = NULL;
	fz_device *dev = NULL;
	fz_display_list *strike_list = NULL;

	fz_var(path);
	fz_var(stroke);
	fz_var(dev);
	fz_var(strike_list);
	fz_try(ctx)
	{
		fz_rect rect = fz_empty_rect;
		fz_colorspace *cs;
		float color[4];
		float width;
		pdf_obj *list;
		int n, m, i, j;

		cs = pdf_to_color(doc, pdf_dict_gets(annot->obj, "C"), color);
		if (!cs)
		{
			cs = fz_device_rgb(ctx);
			color[0] = 1.0f;
			color[1] = 0.0f;
			color[2] = 0.0f;
		}

		width = pdf_to_real(pdf_dict_gets(pdf_dict_gets(annot->obj, "BS"), "W"));
		if (width == 0.0f)
			width = 1.0f;

		list = pdf_dict_gets(annot->obj, "InkList");

		n = pdf_array_len(list);

		strike_list = fz_new_display_list(ctx);
		dev = fz_new_list_device(ctx, strike_list);
		path = fz_new_path(ctx);
		stroke = fz_new_stroke_state(ctx);
		stroke->linewidth = width;

		for (i = 0; i < n; i ++)
		{
			fz_point pt_last;
			pdf_obj *arc = pdf_array_get(list, i);
			m = pdf_array_len(arc);

			for (j = 0; j < m-1; j += 2)
			{
				fz_point pt;
				pt.x = pdf_to_real(pdf_array_get(arc, j));
				pt.y = pdf_to_real(pdf_array_get(arc, j+1));

				if (i == 0 && j == 0)
				{
					rect.x0 = rect.x1 = pt.x;
					rect.y0 = rect.y1 = pt.y;
				}
				else
				{
					fz_include_point_in_rect(&rect, &pt);
				}

				if (j == 0)
					fz_moveto(ctx, path, pt.x, pt.y);
				else
					fz_curvetov(ctx, path, pt_last.x, pt_last.y, (pt.x + pt_last.x) / 2, (pt.y + pt_last.y) / 2);
				pt_last = pt;
			}
			fz_lineto(ctx, path, pt_last.x, pt_last.y);
		}

		fz_stroke_path(dev, path, stroke, page_ctm, cs, color, 1.0f);

		fz_expand_rect(&rect, width);

		fz_transform_rect(&rect, page_ctm);
		pdf_set_annot_appearance(doc, annot, &rect, strike_list);
	}
	fz_always(ctx)
	{
		fz_free_device(dev);
		fz_drop_stroke_state(ctx, stroke);
		fz_free_path(ctx, path);
		fz_drop_display_list(ctx, strike_list);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}
