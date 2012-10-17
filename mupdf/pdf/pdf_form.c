#include "fitz-internal.h"
#include "mupdf-internal.h"

#define MATRIX_COEFS (6)

#define FZ_WIDGET_TYPE_NOT_WIDGET (-1)

enum
{
	Ff_Multiline = 1 << (13-1),
	Ff_Password = 1 << (14-1),
	Ff_NoToggleToOff = 1 << (15-1),
	Ff_Radio = 1 << (16-1),
	Ff_Pushbutton = 1 << (17-1),
	Ff_Combo = 1 << (18-1),
	Ff_FileSelect = 1 << (21-1),
	Ff_MultiSelect = 1 << (22-1),
	Ff_DoNotSpellCheck = 1 << (23-1),
	Ff_DoNotScroll = 1 << (24-1),
	Ff_Comb = 1 << (25-1),
	Ff_RadioInUnison = 1 << (26-1)
};

enum
{
	F_Invisible = 1 << (1-1),
	F_Hidden = 1 << (2-1),
	F_Print = 1 << (3-1),
	F_NoZoom = 1 << (4-1),
	F_NoRotate = 1 << (5-1),
	F_NoView = 1 << (6-1),
	F_ReadOnly = 1 << (7-1),
	F_Locked = 1 << (8-1),
	F_ToggleNoView = 1 << (9-1),
	F_LockedContents = 1 << (10-1)
};

enum
{
	BS_Solid,
	BS_Dashed,
	BS_Beveled,
	BS_Inset,
	BS_Underline
};

/* Must be kept in sync with definitions in pdf_util.js */
enum
{
	Display_Visible,
	Display_Hidden,
	Display_NoPrint,
	Display_NoView
};


enum
{
	Q_Left  = 0,
	Q_Cent  = 1,
	Q_Right = 2
};

typedef struct da_info_s
{
	char *font_name;
	int font_size;
	float col[4];
	int col_size;
} da_info;

typedef struct font_info_s
{
	da_info da_rec;
	pdf_font_desc *font;
} font_info;

typedef struct text_widget_info_s
{
	pdf_obj *dr;
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
		*mat = fz_concat(fz_rotate(rot), fz_translate(width, 0));
		rect->x1 = height;
		rect->y1 = width;
		break;
	case 180:
		*mat = fz_concat(fz_rotate(rot), fz_translate(width, height));
		break;
	case 270:
		*mat = fz_concat(fz_rotate(rot), fz_translate(0, height));
		rect->x1 = height;
		rect->y1 = width;
		break;
	}
}

static pdf_obj *get_inheritable(pdf_document *doc, pdf_obj *obj, char *key)
{
	pdf_obj *fobj = NULL;

	while (!fobj && obj)
	{
		fobj = pdf_dict_gets(obj, key);

		if (!fobj)
			obj = pdf_dict_gets(obj, "Parent");
	}

	return fobj ? fobj
				: pdf_dict_gets(pdf_dict_gets(pdf_dict_gets(doc->trailer, "Root"), "AcroForm"), key);
}

static char *get_string_or_stream(pdf_document *doc, pdf_obj *obj)
{
	fz_context *ctx = doc->ctx;
	int len = 0;
	char *buf = NULL;
	fz_buffer *strmbuf = NULL;
	char *text = NULL;

	fz_var(strmbuf);
	fz_var(text);
	fz_try(ctx)
	{
		if (pdf_is_string(obj))
		{
			len = pdf_to_str_len(obj);
			buf = pdf_to_str_buf(obj);
		}
		else if (pdf_is_stream(doc, pdf_to_num(obj), pdf_to_gen(obj)))
		{
			strmbuf = pdf_load_stream(doc, pdf_to_num(obj), pdf_to_gen(obj));
			len = fz_buffer_storage(ctx, strmbuf, (unsigned char **)&buf);
		}

		if (buf)
		{
			text = fz_malloc(ctx, len+1);
			memcpy(text, buf, len);
			text[len] = 0;
		}
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, strmbuf);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, text);
		fz_rethrow(ctx);
	}

	return text;
}

static char *get_field_type_name(pdf_document *doc, pdf_obj *obj)
{
	return pdf_to_name(get_inheritable(doc, obj, "FT"));
}

static int get_field_flags(pdf_document *doc, pdf_obj *obj)
{
	return pdf_to_int(get_inheritable(doc, obj, "Ff"));
}

int pdf_field_type(pdf_document *doc, pdf_obj *obj)
{
	char *type = get_field_type_name(doc, obj);
	int   flags = get_field_flags(doc, obj);

	if (!strcmp(type, "Btn"))
	{
		if (flags & Ff_Pushbutton)
			return FZ_WIDGET_TYPE_PUSHBUTTON;
		else if (flags & Ff_Radio)
			return FZ_WIDGET_TYPE_RADIOBUTTON;
		else
			return FZ_WIDGET_TYPE_CHECKBOX;
	}
	else if (!strcmp(type, "Tx"))
		return FZ_WIDGET_TYPE_TEXT;
	else if (!strcmp(type, "Ch"))
	{
		if (flags & Ff_Combo)
			return FZ_WIDGET_TYPE_COMBOBOX;
		else
			return FZ_WIDGET_TYPE_LISTBOX;
	}
	else
		return FZ_WIDGET_TYPE_NOT_WIDGET;
}

/* Find the point in a field hierarchy where all descendents
 * share the same name */
static pdf_obj *find_head_of_field_group(pdf_obj *obj)
{
	if (obj == NULL || pdf_dict_gets(obj, "T"))
		return obj;
	else
		return find_head_of_field_group(pdf_dict_gets(obj, "Parent"));
}

static void pdf_field_mark_dirty(fz_context *ctx, pdf_obj *field)
{
	if (!pdf_dict_gets(field, "Dirty"))
	{
		pdf_obj *nullobj = pdf_new_null(ctx);
		fz_try(ctx)
		{
			pdf_dict_puts(field, "Dirty", nullobj);
		}
		fz_always(ctx)
		{
			pdf_drop_obj(nullobj);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}
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

static void da_info_fin(fz_context *ctx, da_info *di)
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

static void parse_da(fz_context *ctx, char *da, da_info *di)
{
	float stack[32];
	int top = 0;
	int tok;
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

	parse_da(ctx, da, &font_rec->da_rec);
	if (font_rec->da_rec.font_name == NULL)
		fz_throw(ctx, "No font name in default appearance");
	font_rec->font = pdf_load_font(doc, dr, pdf_dict_gets(pdf_dict_gets(dr, "Font"), font_rec->da_rec.font_name));
}

static void font_info_fin(fz_context *ctx, font_info *font_rec)
{
	pdf_drop_font(ctx, font_rec->font);
	font_rec->font = NULL;
	da_info_fin(ctx, &font_rec->da_rec);
}

static void get_text_widget_info(pdf_document *doc, pdf_obj *widget, text_widget_info *info)
{
	char *da = pdf_to_str_buf(get_inheritable(doc, widget, "DA"));
	int ff = get_field_flags(doc, widget);
	pdf_obj *ml = get_inheritable(doc, widget, "MaxLen");

	info->dr = get_inheritable(doc, widget, "DR");
	info->q = pdf_to_int(get_inheritable(doc, widget, "Q"));
	info->multiline = (ff & Ff_Multiline) != 0;
	info->comb = (ff & (Ff_Multiline|Ff_Password|Ff_FileSelect|Ff_Comb)) == Ff_Comb;

	if (ml == NULL)
		info->comb = 0;
	else
		info->max_len = pdf_to_int(ml);

	get_font_info(doc, info->dr, da, &info->font_rec);
}

static void fzbuf_print_da(fz_context *ctx, fz_buffer *fzbuf, da_info *di)
{
	if (di->font_name != NULL && di->font_size != 0)
		fz_buffer_printf(ctx, fzbuf, "/%s %d Tf", di->font_name, di->font_size);

	if (di->col_size != 0)
		fz_buffer_printf(ctx, fzbuf, " %f %f %f rg", di->col[0], di->col[1], di->col[2]);
	else
		fz_buffer_printf(ctx, fzbuf, " 0 g");
}

static fz_rect measure_text(pdf_document *doc, font_info *font_rec, const fz_matrix *tm, char *text)
{
	fz_rect bbox = pdf_measure_text(doc->ctx, font_rec->font, (unsigned char *)text, strlen(text));

	bbox.x0 *= font_rec->da_rec.font_size * tm->a;
	bbox.y0 *= font_rec->da_rec.font_size * tm->d;
	bbox.x1 *= font_rec->da_rec.font_size * tm->a;
	bbox.y1 *= font_rec->da_rec.font_size * tm->d;

	return bbox;
}

static void fzbuf_print_text(fz_context *ctx, fz_buffer *fzbuf, fz_rect *clip, font_info *font_rec, fz_matrix *tm, char *text)
{
	fz_buffer_printf(ctx, fzbuf, fmt_q);
	if (clip)
	{
		fz_buffer_printf(ctx, fzbuf, fmt_re, clip->x0, clip->y0, clip->x1 - clip->x0, clip->y1 - clip->y0);
		fz_buffer_printf(ctx, fzbuf, fmt_W);
		fz_buffer_printf(ctx, fzbuf, fmt_n);
	}

	fz_buffer_printf(ctx, fzbuf, fmt_BT);

	fzbuf_print_da(ctx, fzbuf, &font_rec->da_rec);

	fz_buffer_printf(ctx, fzbuf, "\n");
	if (tm)
		fz_buffer_printf(ctx, fzbuf, fmt_Tm, tm->a, tm->b, tm->c, tm->d, tm->e, tm->f);

	fz_buffer_cat_pdf_string(ctx, fzbuf, text);
	fz_buffer_printf(ctx, fzbuf, fmt_Tj);
	fz_buffer_printf(ctx, fzbuf, fmt_ET);
	fz_buffer_printf(ctx, fzbuf, fmt_Q);
}

static fz_buffer *create_text_buffer(fz_context *ctx, fz_rect *clip, font_info *font_rec, fz_matrix *tm, char *text)
{
	fz_buffer *fzbuf = fz_new_buffer(ctx, 0);

	fz_try(ctx)
	{
		fz_buffer_printf(ctx, fzbuf, fmt_Tx_BMC);
		fzbuf_print_text(ctx, fzbuf, clip, font_rec, tm, text);
		fz_buffer_printf(ctx, fzbuf, fmt_EMC);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, fzbuf);
		fz_rethrow(ctx);
	}

	return fzbuf;
}

static fz_buffer *create_aligned_text_buffer(pdf_document *doc, fz_rect *clip, text_widget_info *info, fz_matrix *tm, char *text)
{
	fz_context *ctx = doc->ctx;
	fz_matrix atm = *tm;

	if (info->q != Q_Left)
	{
		fz_rect rect = measure_text(doc, &info->font_rec, tm, text);

		atm.e -= info->q == Q_Right ? rect.x1
							  : (rect.x1 - rect.x0) / 2;
	}

	return create_text_buffer(ctx, clip, &info->font_rec, &atm, text);
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
		bbox = measure_text(doc, &tinf, &fz_identity, testtext);
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

static void fzbuf_print_text_start(fz_context *ctx, fz_buffer *fzbuf, fz_rect *clip, font_info *font, fz_matrix *tm)
{
	fz_buffer_printf(ctx, fzbuf, fmt_Tx_BMC);
	fz_buffer_printf(ctx, fzbuf, fmt_q);

	if (clip)
	{
		fz_buffer_printf(ctx, fzbuf, fmt_re, clip->x0, clip->y0, clip->x1 - clip->x0, clip->y1 - clip->y0);
		fz_buffer_printf(ctx, fzbuf, fmt_W);
		fz_buffer_printf(ctx, fzbuf, fmt_n);
	}

	fz_buffer_printf(ctx, fzbuf, fmt_BT);

	fzbuf_print_da(ctx, fzbuf, &font->da_rec);
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

static fz_buffer *create_text_appearance(pdf_document *doc, fz_rect *bbox, fz_matrix *oldtm, text_widget_info *info, char *text)
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

			fzbuf_print_text_start(ctx, fzbuf, &rect, &info->font_rec, &tm);

			fz_buffer_cat(ctx, fzbuf, fztmp);

			fzbuf_print_text_end(ctx, fzbuf);
		}
		else if (info->comb)
		{
			int i, n = fz_mini((int)strlen(text), info->max_len);
			float comb_width = full_width/info->max_len;
			float char_width = pdf_text_stride(ctx, info->font_rec.font, fontsize, (unsigned char *)"M", 1, FLT_MAX, NULL);
			float init_skip = (comb_width - char_width)/2.0;

			tm = fz_identity;
			tm.e = rect.x0;
			tm.f = rect.y1 - (height+(ascent-descent)*fontsize)/2.0;

			fzbuf = fz_new_buffer(ctx, 0);

			fzbuf_print_text_start(ctx, fzbuf, &rect, &info->font_rec, &tm);

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
				tm = fz_identity;
				tm.e = rect.x0;
				tm.f = rect.y1 - (height+(ascent-descent)*fontsize)/2.0;

				switch(info->q)
				{
				case Q_Right: tm.e += width; break;
				case Q_Cent: tm.e += width/2; break;
				}
			}

			if (variable)
			{
				tbox = measure_text(doc, &info->font_rec, &tm, text);

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

static void update_marked_content(pdf_document *doc, pdf_xobject *form, fz_buffer *fzbuf)
{
	fz_context *ctx = doc->ctx;
	int tok;
	pdf_lexbuf lbuf;
	fz_stream *str_outer = NULL;
	fz_stream *str_inner = NULL;
	unsigned char *buf;
	int            len;
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

				coefs[coef_i++] = tok == PDF_TOK_INT ? lbuf.i
													 : lbuf.f;
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
			fz_rect bbox = pdf_to_rect(ctx, pdf_dict_gets(form->contents, "BBox"));

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

static void update_text_field_value(fz_context *ctx, pdf_obj *obj, char *text)
{
	pdf_obj *sobj = NULL;
	pdf_obj *grp;

	if (!text)
		text = "";

	/* All fields of the same name should be updated, so
	 * set the value at the head of the group */
	grp = find_head_of_field_group(obj);
	if (grp)
		obj = grp;

	fz_var(sobj);
	fz_try(ctx)
	{
		sobj = pdf_new_string(ctx, text, strlen(text));
		pdf_dict_puts(obj, "V", sobj);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(sobj);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	pdf_field_mark_dirty(ctx, obj);
}

static pdf_xobject *load_or_create_form(pdf_document *doc, pdf_obj *obj, fz_rect *rect)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *ap = NULL;
	pdf_obj *tobj = NULL;
	fz_matrix mat;
	int rot;
	pdf_obj *formobj = NULL;
	pdf_xobject *form = NULL;
	char *dn = "N";
	fz_buffer *fzbuf = NULL;
	int create_form = 0;

	fz_var(formobj);
	fz_var(tobj);
	fz_var(form);
	fz_var(fzbuf);
	fz_try(ctx)
	{
		rot = pdf_to_int(pdf_dict_getp(obj, "MK/R"));
		*rect = pdf_to_rect(ctx, pdf_dict_gets(obj, "Rect"));
		rect->x1 -= rect->x0;
		rect->y1 -= rect->y0;
		rect->x0 = rect->y0 = 0;
		account_for_rot(rect, &mat, rot);

		ap = pdf_dict_gets(obj, "AP");
		if (ap == NULL)
		{
			tobj = pdf_new_dict(ctx, 1);
			pdf_dict_puts(obj, "AP", tobj);
			ap = tobj;
			tobj = NULL;
		}

		formobj = pdf_dict_gets(ap, dn);
		if (formobj == NULL)
		{
			tobj = pdf_new_xobject(doc, rect, &mat);
			pdf_dict_puts(ap, dn, tobj);
			formobj = tobj;
			tobj = NULL;
			create_form = 1;
		}

		form = pdf_load_xobject(doc, formobj);
		if (create_form)
		{
			fzbuf = fz_new_buffer(ctx, 1);
			pdf_update_xobject_contents(doc, form, fzbuf);
		}

		copy_resources(form->resources, get_inheritable(doc, obj, "DR"));
	}
	fz_always(ctx)
	{
		pdf_drop_obj(tobj);
		fz_drop_buffer(ctx, fzbuf);
	}
	fz_catch(ctx)
	{
		pdf_drop_xobject(ctx, form);
		fz_rethrow(ctx);
	}

	return form;
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

static void update_text_appearance(pdf_document *doc, pdf_obj *obj, char *eventValue)
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

static void update_combobox_appearance(pdf_document *doc, pdf_obj *obj)
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

		val = get_inheritable(doc, obj, "V");

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

static void fzbuf_print_color(fz_context *ctx, fz_buffer *fzbuf, pdf_obj *arr, int stroke, float adj)
{
	switch(pdf_array_len(arr))
	{
	case 1:
		fz_buffer_printf(ctx, fzbuf, stroke?"%f G\n":"%f g\n",
			pdf_to_real(pdf_array_get(arr, 0)) + adj);
		break;
	case 3:
		fz_buffer_printf(ctx, fzbuf, stroke?"%f %f %f rg\n":"%f %f %f rg\n",
			pdf_to_real(pdf_array_get(arr, 0)) + adj,
			pdf_to_real(pdf_array_get(arr, 1)) + adj,
			pdf_to_real(pdf_array_get(arr, 2)) + adj);
		break;
	case 4:
		fz_buffer_printf(ctx, fzbuf, stroke?"%f %f %f %f k\n":"%f %f %f %f k\n",
			pdf_to_real(pdf_array_get(arr, 0)),
			pdf_to_real(pdf_array_get(arr, 1)),
			pdf_to_real(pdf_array_get(arr, 2)),
			pdf_to_real(pdf_array_get(arr, 3)));
		break;
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

static void update_pushbutton_appearance(pdf_document *doc, pdf_obj *obj)
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
			char *da = pdf_to_str_buf(pdf_dict_gets(obj, "DA"));
			char *text = pdf_to_str_buf(tobj);

			clip.x0 += btotal;
			clip.y0 += btotal;
			clip.x1 -= btotal;
			clip.y1 -= btotal;

			get_font_info(doc, form->resources, da, &font_rec);
			bounds = measure_text(doc, &font_rec, &fz_identity, text);
			mat = fz_translate((rect.x1 - bounds.x1)/2, (rect.y1 - bounds.y1)/2);
			fzbuf_print_text(ctx, fzbuf, &clip, &font_rec, &mat, text);
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

static pdf_obj *find_field(pdf_obj *dict, char *name, int len)
{
	pdf_obj *field;

	int i, n = pdf_array_len(dict);

	for (i = 0; i < n; i++)
	{
		char *part;

		field = pdf_array_get(dict, i);
		part = pdf_to_str_buf(pdf_dict_gets(field, "T"));
		if (strlen(part) == len && !memcmp(part, name, len))
			return field;
	}

	return NULL;
}

pdf_obj *pdf_lookup_field(pdf_obj *form, char *name)
{
	char *dot;
	char *namep;
	pdf_obj *dict = NULL;
	int len;

	/* Process the fully qualified field name which has
	* the partial names delimited by '.'. Pretend there
	* was a preceding '.' to simplify the loop */
	dot = name - 1;

	while (dot && form)
	{
		namep = dot + 1;
		dot = strchr(namep, '.');
		len = dot ? dot - namep : strlen(namep);
		dict = find_field(form, namep, len);
		if (dot)
			form = pdf_dict_gets(dict, "Kids");
	}

	return dict;
}

void pdf_field_reset(pdf_document *doc, pdf_obj *field)
{
	fz_context *ctx = doc->ctx;
	/* Descend through the hierarchy, setting V to DV where
	 * ever DV is present, and deleting V where DV is not.
	 * FIXME: we assume for now that V has not been set unequal
	 * to DV higher in the hierarchy than "field".
	 *
	 *  At the bottom of the hierarchy we may find widget annotations
	 * that aren't also fields, but DV and V will not be present in their
	 * dictionaries, and attempts to remove V will be harmless. */
	pdf_obj *dv = pdf_dict_gets(field, "DV");
	pdf_obj *kids = pdf_dict_gets(field, "Kids");
	pdf_obj *noreset = pdf_dict_gets(field, "NoReset");

	/* Don't process fields we have marked not to be reset */
	if (noreset)
		return;

	if (dv)
		pdf_dict_puts(field, "V", dv);
	else
		pdf_dict_dels(field, "V");

	if (kids)
	{
		int i, n = pdf_array_len(kids);

		for (i = 0; i < n; i++)
			pdf_field_reset(doc, pdf_array_get(kids, i));
	}
	else
	{
		/* The leaves of the tree are widget annotations
		 * In some cases we need to update the appearance state;
		 * in others we need to mark the field as dirty so that
		 * the appearance stream will be regenerated. */
		switch (pdf_field_type(doc, field))
		{
		case FZ_WIDGET_TYPE_RADIOBUTTON:
		case FZ_WIDGET_TYPE_CHECKBOX:
			{
				pdf_obj *leafv = get_inheritable(doc, field, "V");

				if (leafv)
					pdf_keep_obj(leafv);
				else
					leafv = pdf_new_name(ctx, "Off");

				fz_try(ctx)
				{
					pdf_dict_puts(field, "AS", leafv);
				}
				fz_always(ctx)
				{
					pdf_drop_obj(leafv);
				}
				fz_catch(ctx)
				{
					fz_rethrow(ctx);
				}
			}
			break;

		case FZ_WIDGET_TYPE_PUSHBUTTON:
			break;

		default:
			pdf_field_mark_dirty(ctx, field);
			break;
		}
	}

	doc->dirty = 1;
}


static void reset_form(pdf_document *doc, pdf_obj *fields, int exclude)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *form = pdf_dict_getp(doc->trailer, "Root/AcroForm/Fields");
	int i, n;

	/* The 'fields' array not being present signals that all fields
	 * should be reset, so handle it using the exclude case - excluding none */
	if (exclude || !fields)
	{
		/* mark the fields we don't want to reset */
		pdf_obj *nil = pdf_new_null(ctx);

		fz_try(ctx)
		{
			n = pdf_array_len(fields);

			for (i = 0; i < n; i++)
			{
				pdf_obj *field = pdf_array_get(fields, i);

				if (pdf_is_string(field))
					field = pdf_lookup_field(form, pdf_to_str_buf(field));

				if (field)
					pdf_dict_puts(field, "NoReset", nil);
			}
		}
		fz_always(ctx)
		{
			pdf_drop_obj(nil);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}

		/* reset all unmarked fields */
		n = pdf_array_len(form);

		for (i = 0; i < n; i++)
			pdf_field_reset(doc, pdf_array_get(form, i));

		/* Unmark the marked fields */
		n = pdf_array_len(fields);

		for (i = 0; i < n; i++)
		{
			pdf_obj *field = pdf_array_get(fields, i);

			if (pdf_is_string(field))
				field = pdf_lookup_field(form, pdf_to_str_buf(field));

			if (field)
				pdf_dict_dels(field, "NoReset");
		}
	}
	else
	{
		n = pdf_array_len(fields);

		for (i = 0; i < n; i++)
		{
			pdf_obj *field = pdf_array_get(fields, i);

			if (pdf_is_string(field))
				field = pdf_lookup_field(form, pdf_to_str_buf(field));

			if (field)
				pdf_field_reset(doc, field);
		}
	}
}

static void execute_action(pdf_document *doc, pdf_obj *obj, pdf_obj *a)
{
	fz_context *ctx = doc->ctx;
	if (a)
	{
		char *type = pdf_to_name(pdf_dict_gets(a, "S"));

		if (!strcmp(type, "JavaScript"))
		{
			pdf_obj *js = pdf_dict_gets(a, "JS");
			if (js)
			{
				char *code = pdf_to_utf8(doc, js);
				fz_try(ctx)
				{
					pdf_js_execute(doc->js, code);
				}
				fz_always(ctx)
				{
					fz_free(ctx, code);
				}
				fz_catch(ctx)
				{
					fz_rethrow(ctx);
				}
			}
		}
		else if (!strcmp(type, "ResetForm"))
		{
			reset_form(doc, pdf_dict_gets(a, "Fields"), pdf_to_int(pdf_dict_gets(a, "Flags")) & 1);
		}
		else if (!strcmp(type, "Named"))
		{
			char *name = pdf_to_name(pdf_dict_gets(a, "N"));

			if (!strcmp(name, "Print"))
				pdf_event_issue_print(doc);
		}
	}
}

int pdf_update_appearance(pdf_document *doc, pdf_obj *obj)
{
	if (pdf_dict_gets(obj, "AP") && !pdf_dict_gets(obj, "Dirty"))
		return 0;

	if (!strcmp(pdf_to_name(pdf_dict_gets(obj, "Subtype")), "Widget"))
	{
		switch(pdf_field_type(doc, obj))
		{
		case FZ_WIDGET_TYPE_TEXT:
			{
				pdf_obj *formatting = pdf_dict_getp(obj, "AA/F");
				if (formatting && doc->js)
				{
					/* Apply formatting */
					pdf_js_event e;

					e.target = obj;
					e.value = pdf_field_value(doc, obj);
					pdf_js_setup_event(doc->js, &e);
					execute_action(doc, obj, formatting);
					/* Update appearance from JS event.value */
					update_text_appearance(doc, obj, pdf_js_get_event(doc->js)->value);
				}
				else
				{
					/* Update appearance from field value */
					update_text_appearance(doc, obj, NULL);
				}
			}
			break;
		case FZ_WIDGET_TYPE_PUSHBUTTON:
			update_pushbutton_appearance(doc, obj);
			break;
		case FZ_WIDGET_TYPE_LISTBOX:
		case FZ_WIDGET_TYPE_COMBOBOX:
			/* Treating listbox and combobox identically for now,
			* and the behaviour is most appropriate for a combobox */
			update_combobox_appearance(doc, obj);
			break;
		}
	}

	pdf_dict_dels(obj, "Dirty");

	return 1;
}

static void execute_action_chain(pdf_document *doc, pdf_obj *obj)
{
	pdf_obj *a = pdf_dict_gets(obj, "A");
	pdf_js_event e;

	e.target = obj;
	e.value = "";
	pdf_js_setup_event(doc->js, &e);

	while (a)
	{
		execute_action(doc, obj, a);
		a = pdf_dict_gets(a, "Next");
	}
}

static void execute_additional_action(pdf_document *doc, pdf_obj *obj, char *act)
{
	pdf_obj *a = pdf_dict_getp(obj, act);

	if (a)
	{
		pdf_js_event e;

		e.target = obj;
		e.value = "";
		pdf_js_setup_event(doc->js, &e);
		execute_action(doc, obj, a);
	}
}

static void check_off(fz_context *ctx, pdf_obj *obj)
{
	pdf_obj *off = NULL;

	fz_var(off);
	fz_try(ctx);
	{
		off = pdf_new_name(ctx, "Off");
		pdf_dict_puts(obj, "AS", off);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(off);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void set_check(fz_context *ctx, pdf_obj *chk, char *name)
{
	pdf_obj *n = pdf_dict_getp(chk, "AP/N");
	pdf_obj *val = NULL;

	fz_var(val);
	fz_try(ctx)
	{
		/* If name is a possible value of this check
		* box then use it, otherwise use "Off" */
		if (pdf_dict_gets(n, name))
			val = pdf_new_name(ctx, name);
		else
			val = pdf_new_name(ctx, "Off");

		pdf_dict_puts(chk, "AS", val);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(val);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

/* Set the values of all fields in a group defined by a node
 * in the hierarchy */
static void set_check_grp(fz_context *ctx, pdf_obj *grp, char *val)
{
	pdf_obj *kids = pdf_dict_gets(grp, "Kids");

	if (kids == NULL)
	{
		set_check(ctx, grp, val);
	}
	else
	{
		int i, n = pdf_array_len(kids);

		for (i = 0; i < n; i++)
			set_check_grp(ctx, pdf_array_get(kids, i), val);
	}
}

static void recalculate(pdf_document *doc)
{
	fz_context *ctx = doc->ctx;

	if (doc->recalculating)
		return;

	doc->recalculating = 1;
	fz_try(ctx)
	{
		pdf_obj *co = pdf_dict_getp(doc->trailer, "Root/AcroForm/CO");

		if (co && doc->js)
		{
			int i, n = pdf_array_len(co);

			for (i = 0; i < n; i++)
			{
				pdf_obj *field = pdf_array_get(co, i);
				pdf_obj *calc = pdf_dict_getp(field, "AA/C");

				if (calc)
				{
					pdf_js_event e;

					e.target = field;
					e.value = pdf_field_value(doc, field);
					pdf_js_setup_event(doc->js, &e);
					execute_action(doc, field, calc);
					/* A calculate action, updates event.value. We need
					* to place the value in the field */
					update_text_field_value(doc->ctx, field, pdf_js_get_event(doc->js)->value);
				}
			}
		}
	}
	fz_always(ctx)
	{
		doc->recalculating = 0;
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void toggle_check_box(pdf_document *doc, pdf_obj *obj)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *as = pdf_dict_gets(obj, "AS");
	int ff = get_field_flags(doc, obj);
	int radio = ((ff & (Ff_Pushbutton|Ff_Radio)) == Ff_Radio);
	char *val = NULL;
	pdf_obj *grp = radio ? pdf_dict_gets(obj, "Parent") : find_head_of_field_group(obj);

	if (!grp)
		grp = obj;

	if (as && strcmp(pdf_to_name(as), "Off"))
	{
		/* "as" neither missing nor set to Off. Set it to Off, unless
		 * this is a non-toggle-off radio button. */
		if ((ff & (Ff_Pushbutton|Ff_NoToggleToOff|Ff_Radio)) != (Ff_NoToggleToOff|Ff_Radio))
		{
			check_off(ctx, obj);
			val = "Off";
		}
	}
	else
	{
	    pdf_obj *n, *key = NULL;
		int len, i;

		n = pdf_dict_getp(obj, "AP/N");

		/* Look for a key that isn't "Off" */
		len = pdf_dict_len(n);
		for (i = 0; i < len; i++)
		{
			key = pdf_dict_get_key(n, i);
			if (pdf_is_name(key) && strcmp(pdf_to_name(key), "Off"))
				break;
		}

		/* If we found no alternative value to Off then we have no value to use */
		if (!key)
			return;

		val = pdf_to_name(key);

		if (radio)
		{
			/* For radio buttons, first turn off all buttons in the group and
			 * then set the one that was clicked */
			pdf_obj *kids = pdf_dict_gets(grp, "Kids");
			int i, n = pdf_array_len(kids);

			for (i = 0; i < n; i++)
				check_off(ctx, pdf_array_get(kids, i));

			pdf_dict_puts(obj, "AS", key);
		}
		else
		{
			/* For check boxes, we have located the node of the field hierarchy
			 * below which all fields share a name with the clicked one. Set
			 * all to the same value. This may cause the group to act like
			 * radio buttons, if each have distinct "On" values */
			if (grp)
				set_check_grp(doc->ctx, grp, val);
			else
				set_check(doc->ctx, obj, val);
		}
	}

	if (val && grp)
	{
		pdf_obj *v = NULL;

		fz_var(v);
		fz_try(ctx)
		{
			v = pdf_new_string(ctx, val, strlen(val));
			pdf_dict_puts(grp, "V", v);
		}
		fz_always(ctx)
		{
			pdf_drop_obj(v);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}

		recalculate(doc);
	}
}

int pdf_has_unsaved_changes(pdf_document *doc)
{
	return doc->dirty;
}

int pdf_pass_event(pdf_document *doc, pdf_page *page, fz_ui_event *ui_event)
{
	pdf_annot *annot;
	pdf_hotspot *hp = &doc->hotspot;
	fz_point  *pt = &(ui_event->event.pointer.pt);
	int changed = 0;

	for (annot = page->annots; annot; annot = annot->next)
	{
		if (pt->x >= annot->pagerect.x0 && pt->x <= annot->pagerect.x1)
			if (pt->y >= annot->pagerect.y0 && pt->y <= annot->pagerect.y1)
				break;
	}

	if (annot)
	{
		int f = pdf_to_int(pdf_dict_gets(annot->obj, "F"));

		if (f & (F_Hidden|F_NoView))
			annot = NULL;
	}

	switch (ui_event->etype)
	{
	case FZ_EVENT_TYPE_POINTER:
		{
			switch (ui_event->event.pointer.ptype)
			{
			case FZ_POINTER_DOWN:
				doc->focus = NULL;
				pdf_drop_obj(doc->focus_obj);
				doc->focus_obj = NULL;

				if (annot)
				{
					doc->focus = annot;
					doc->focus_obj = pdf_keep_obj(annot->obj);

					hp->num = pdf_to_num(annot->obj);
					hp->gen = pdf_to_gen(annot->obj);
					hp->state = HOTSPOT_POINTER_DOWN;
					changed = 1;
					/* Exectute the down action */
					execute_additional_action(doc, annot->obj, "AA/D");
				}
				break;

			case FZ_POINTER_UP:
				if (hp->state != 0)
					changed = 1;

				hp->num = 0;
				hp->gen = 0;
				hp->state = 0;

				if (annot)
				{
					switch(annot->type)
					{
					case FZ_WIDGET_TYPE_RADIOBUTTON:
					case FZ_WIDGET_TYPE_CHECKBOX:
						/* FIXME: treating radio buttons like check boxes, for now */
						toggle_check_box(doc, annot->obj);
						changed = 1;
						break;
					}

					/* Execute the up action */
					execute_additional_action(doc, annot->obj, "AA/U");
					/* Execute the main action chain */
					execute_action_chain(doc, annot->obj);
				}
				break;
			}
		}
		break;
	}

	return changed;
}

fz_rect *pdf_poll_screen_update(pdf_document *doc)
{
	return NULL;
}

fz_widget *pdf_focused_widget(pdf_document *doc)
{
	return (fz_widget *)doc->focus;
}

fz_widget *pdf_first_widget(pdf_document *doc, pdf_page *page)
{
	pdf_annot *annot = page->annots;

	while (annot && annot->type == FZ_WIDGET_TYPE_NOT_WIDGET)
		annot = annot->next;

	return (fz_widget *)annot;
}

fz_widget *pdf_next_widget(fz_widget *previous)
{
	pdf_annot *annot = (pdf_annot *)previous;

	if (annot)
		annot = annot->next;

	while (annot && annot->type == FZ_WIDGET_TYPE_NOT_WIDGET)
		annot = annot->next;

	return (fz_widget *)annot;
}

int fz_widget_get_type(fz_widget *widget)
{
	pdf_annot *annot = (pdf_annot *)widget;
	return annot->type;
}

char *pdf_field_value(pdf_document *doc, pdf_obj *field)
{
	return get_string_or_stream(doc, get_inheritable(doc, field, "V"));
}

int pdf_field_set_value(pdf_document *doc, pdf_obj *field, char *text)
{
	pdf_obj *v = pdf_dict_getp(field, "AA/V");

	if (v && doc->js)
	{
		pdf_js_event e;

		e.target = field;
		e.value = text;
		pdf_js_setup_event(doc->js, &e);
		execute_action(doc, field, v);

		if (!pdf_js_get_event(doc->js)->rc)
			return 0;

		text = pdf_js_get_event(doc->js)->value;
	}

	doc->dirty = 1;
	update_text_field_value(doc->ctx, field, text);
	recalculate(doc);

	return 1;
}

char *pdf_field_border_style(pdf_document *doc, pdf_obj *field)
{
	char *bs = pdf_to_name(pdf_dict_getp(field, "BS/S"));

	switch (*bs)
	{
	case 'S': return "Solid";
	case 'D': return "Dashed";
	case 'B': return "Beveled";
	case 'I': return "Inset";
	case 'U': return "Underline";
	}

	return "Solid";
}

void pdf_field_set_border_style(pdf_document *doc, pdf_obj *field, char *text)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *val = NULL;

	if (!strcmp(text, "Solid"))
		val = pdf_new_name(ctx, "S");
	else if (!strcmp(text, "Dashed"))
		val = pdf_new_name(ctx, "D");
	else if (!strcmp(text, "Beveled"))
		val = pdf_new_name(ctx, "B");
	else if (!strcmp(text, "Inset"))
		val = pdf_new_name(ctx, "I");
	else if (!strcmp(text, "Underline"))
		val = pdf_new_name(ctx, "U");
	else
		return;

	fz_try(ctx);
	{
		pdf_dict_putp(field, "BS/S", val);
		pdf_field_mark_dirty(ctx, field);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(val);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void pdf_field_set_button_caption(pdf_document *doc, pdf_obj *field, char *text)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *val = pdf_new_string(ctx, text, strlen(text));

	fz_try(ctx);
	{
		if (pdf_field_type(doc, field) == FZ_WIDGET_TYPE_PUSHBUTTON)
		{
			pdf_dict_putp(field, "MK/CA", val);
			pdf_field_mark_dirty(ctx, field);
		}
	}
	fz_always(ctx)
	{
		pdf_drop_obj(val);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

int pdf_field_display(pdf_document *doc, pdf_obj *field)
{
	pdf_obj *kids;
	int f, res = Display_Visible;

	/* Base response on first of children. Not ideal,
	 * but not clear how to handle children with
	 * differing values */
	while ((kids = pdf_dict_gets(field, "Kids")) != NULL)
		field = pdf_array_get(kids, 0);

	f = pdf_to_int(pdf_dict_gets(field, "F"));

	if (f & F_Hidden)
	{
		res = Display_Hidden;
	}
	else if (f & F_Print)
	{
		if (f & F_NoView)
			res = Display_NoView;
	}
	else
	{
		if (f & F_NoView)
			res = Display_Hidden;
		else
			res = Display_NoPrint;
	}

	return res;
}

void pdf_field_set_display(pdf_document *doc, pdf_obj *field, int d)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *kids = pdf_dict_gets(field, "Kids");

	if (!kids)
	{
		int mask = (F_Hidden|F_Print|F_NoView);
		int f = pdf_to_int(pdf_dict_gets(field, "F")) & ~mask;
		pdf_obj *fo = NULL;

		switch (d)
		{
		case Display_Visible:
			f |= F_Print;
			break;
		case Display_Hidden:
			f |= F_Hidden;
			break;
		case Display_NoView:
			f |= (F_Print|F_NoView);
			break;
		case Display_NoPrint:
			break;
		}

		fz_var(fo);
		fz_try(ctx)
		{
			fo = pdf_new_int(ctx, f);
			pdf_dict_puts(field, "F", fo);
		}
		fz_always(ctx)
		{
			pdf_drop_obj(fo);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}
	}
	else
	{
		int i, n = pdf_array_len(kids);

		for (i = 0; i < n; i++)
			pdf_field_set_display(doc, pdf_array_get(kids, i), d);
	}
}

void pdf_field_set_fill_color(pdf_document *doc, pdf_obj *field, pdf_obj *col)
{
	pdf_dict_putp(field, "MK/BG", col);
	pdf_field_mark_dirty(doc->ctx, field);
}

void pdf_field_set_text_color(pdf_document *doc, pdf_obj *field, pdf_obj *col)
{
	fz_context *ctx = doc->ctx;
	da_info di;
	fz_buffer *fzbuf = NULL;
	char *da = pdf_to_str_buf(pdf_dict_gets(field, "DA"));
	unsigned char *buf;
	int len;
	pdf_obj *daobj = NULL;

	memset(&di, 0, sizeof(di));

	fz_var(fzbuf);
	fz_var(di);
	fz_var(daobj);
	fz_try(ctx)
	{
		parse_da(ctx, da, &di);
		di.col_size = 3;
		di.col[0] = pdf_to_real(pdf_array_get(col, 0));
		di.col[1] = pdf_to_real(pdf_array_get(col, 1));
		di.col[2] = pdf_to_real(pdf_array_get(col, 2));
		fzbuf = fz_new_buffer(ctx, 0);
		fzbuf_print_da(ctx, fzbuf, &di);
		len = fz_buffer_storage(ctx, fzbuf, &buf);
		daobj = pdf_new_string(ctx, (char *)buf, len);
		pdf_dict_puts(field, "DA", daobj);
		pdf_field_mark_dirty(ctx, field);
	}
	fz_always(ctx)
	{
		da_info_fin(ctx, &di);
		fz_drop_buffer(ctx, fzbuf);
		pdf_drop_obj(daobj);
	}
	fz_catch(ctx)
	{
		fz_warn(ctx, "%s", ctx->error->message);
	}
}

fz_rect *fz_widget_bbox(fz_widget *widget)
{
	pdf_annot *annot = (pdf_annot *)widget;

	return &annot->pagerect;
}

char *pdf_text_widget_text(pdf_document *doc, fz_widget *tw)
{
	pdf_annot *annot = (pdf_annot *)tw;
	fz_context *ctx = doc->ctx;
	char *text = NULL;

	fz_var(text);
	fz_try(ctx)
	{
		text = pdf_field_value(doc, annot->obj);
	}
	fz_catch(ctx)
	{
		fz_warn(ctx, "failed allocation in fz_text_widget_text");
	}

	return text;
}

int pdf_text_widget_max_len(pdf_document *doc, fz_widget *tw)
{
	pdf_annot *annot = (pdf_annot *)tw;

	return pdf_to_int(get_inheritable(doc, annot->obj, "MaxLen"));
}

int pdf_text_widget_content_type(pdf_document *doc, fz_widget *tw)
{
	pdf_annot *annot = (pdf_annot *)tw;
	fz_context *ctx = doc->ctx;
	char *code = NULL;
	int type = FZ_WIDGET_CONTENT_UNRESTRAINED;

	fz_var(code);
	fz_try(ctx)
	{
		code = get_string_or_stream(doc, pdf_dict_getp(annot->obj, "AA/F/JS"));
		if (code)
		{
			if (strstr(code, "AFNumber_Format"))
				type = FZ_WIDGET_CONTENT_NUMBER;
			else if (strstr(code, "AFSpecial_Format"))
				type = FZ_WIDGET_CONTENT_SPECIAL;
			else if (strstr(code, "AFDate_FormatEx"))
				type = FZ_WIDGET_CONTENT_DATE;
			else if (strstr(code, "AFTime_FormatEx"))
				type = FZ_WIDGET_CONTENT_TIME;
		}
	}
	fz_always(ctx)
	{
		fz_free(ctx, code);
	}
	fz_catch(ctx)
	{
		fz_warn(ctx, "failure in fz_text_widget_content_type");
	}

	return type;
}

static int run_keystroke(pdf_document *doc, pdf_obj *field, char **text)
{
	pdf_obj *k = pdf_dict_getp(field, "AA/K");

	if (k && doc->js)
	{
		pdf_js_event e;

		e.target = field;
		e.value = *text;
		pdf_js_setup_event(doc->js, &e);
		execute_action(doc, field, k);

		if (!pdf_js_get_event(doc->js)->rc)
			return 0;

		*text = pdf_js_get_event(doc->js)->value;
	}

	return 1;
}

int pdf_text_widget_set_text(pdf_document *doc, fz_widget *tw, char *text)
{
	pdf_annot *annot = (pdf_annot *)tw;
	fz_context *ctx = doc->ctx;
	int accepted = 0;

	fz_try(ctx)
	{
		accepted = run_keystroke(doc, annot->obj, &text);
		if (accepted)
			accepted = pdf_field_set_value(doc, annot->obj, text);
	}
	fz_catch(ctx)
	{
		fz_warn(ctx, "fz_text_widget_set_text failed");
	}

	return accepted;
}

int pdf_choice_widget_options(pdf_document *doc, fz_widget *tw, char *opts[])
{
	pdf_annot *annot = (pdf_annot *)tw;
	pdf_obj *optarr;
	int i, n;

	if (!annot)
		return 0;

	optarr = pdf_dict_gets(annot->obj, "Opt");
	n = pdf_array_len(optarr);

	if (opts)
	{
		for (i = 0; i < n; i++)
		{
			opts[i] = pdf_to_str_buf(pdf_array_get(optarr, i));
		}
	}

	return n;
}

int pdf_choice_widget_is_multiselect(pdf_document *doc, fz_widget *tw)
{
	pdf_annot *annot = (pdf_annot *)tw;

	if (!annot) return 0;

	switch(pdf_field_type(doc, annot->obj))
	{
	case FZ_WIDGET_TYPE_LISTBOX:
	case FZ_WIDGET_TYPE_COMBOBOX:
		return (get_field_flags(doc, annot->obj) & Ff_MultiSelect) != 0;
	default:
		return 0;
	}
}

int pdf_choice_widget_value(pdf_document *doc, fz_widget *tw, char *opts[])
{
	pdf_annot *annot = (pdf_annot *)tw;
	pdf_obj *optarr;
	int i, n;

	if (!annot)
		return 0;

	optarr = pdf_dict_gets(annot->obj, "V");

	if (pdf_is_string(optarr))
	{
		if (opts)
			opts[0] = pdf_to_str_buf(optarr);

		return 1;
	}
	else
	{
		n = pdf_array_len(optarr);

		if (opts)
		{
			for (i = 0; i < n; i++)
			{
				pdf_obj *elem = pdf_array_get(optarr, i);

				if (pdf_is_array(elem))
					elem = pdf_array_get(elem, 1);

				opts[i] = pdf_to_str_buf(elem);
			}
		}

		return n;
	}
}

void pdf_choice_widget_set_value(pdf_document *doc, fz_widget *tw, int n, char *opts[])
{
	fz_context *ctx = doc->ctx;
	pdf_annot *annot = (pdf_annot *)tw;
	pdf_obj *optarr = NULL, *opt = NULL;
	int i;

	if (!annot)
		return;

	fz_var(optarr);
	fz_var(opt);
	fz_try(ctx)
	{
		if (n != 1)
		{
			optarr = pdf_new_array(ctx, n);

			for (i = 0; i < n; i++)
			{
				opt = pdf_new_string(ctx, opts[i], strlen(opts[i]));
				pdf_array_push(optarr, opt);
				pdf_drop_obj(opt);
				opt = NULL;
			}

			pdf_dict_puts(annot->obj, "V", optarr);
			pdf_drop_obj(optarr);
		}
		else
		{
			opt = pdf_new_string(ctx, opts[0], strlen(opts[0]));
			pdf_dict_puts(annot->obj, "V", opt);
			pdf_drop_obj(opt);
		}

		/* FIXME: when n > 1, we should be regenerating the indexes */
		pdf_dict_dels(annot->obj, "I");

		pdf_field_mark_dirty(ctx, annot->obj);
		doc->dirty = 1;
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(optarr);
		pdf_drop_obj(opt);
		fz_rethrow(ctx);
	}
}
