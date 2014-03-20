#include "pdf-interpret-imp.h"

static pdf_csi *
pdf_new_csi(pdf_document *doc, fz_cookie *cookie, const pdf_process *process)
{
	pdf_csi *csi = NULL;
	fz_context *ctx = doc->ctx;

	fz_var(csi);

	fz_try(ctx)
	{
		csi = fz_malloc_struct(ctx, pdf_csi);
		csi->doc = doc;
		csi->in_text = 0;

		csi->top = 0;
		csi->obj = NULL;
		csi->name[0] = 0;
		csi->string_len = 0;
		memset(csi->stack, 0, sizeof csi->stack);

		csi->process = *process;

		csi->xbalance = 0;
		csi->cookie = cookie;
	}
	fz_catch(ctx)
	{
		pdf_process_op(csi, PDF_OP_END, process);
		fz_free(ctx, csi);
		fz_rethrow(ctx);
	}

	return csi;
}

static void
pdf_clear_stack(pdf_csi *csi)
{
	int i;

	fz_drop_image(csi->doc->ctx, csi->img);
	csi->img = NULL;

	pdf_drop_obj(csi->obj);
	csi->obj = NULL;

	csi->name[0] = 0;
	csi->string_len = 0;
	for (i = 0; i < csi->top; i++)
		csi->stack[i] = 0;

	csi->top = 0;
}

static void
pdf_free_csi(pdf_csi *csi)
{
	fz_context *ctx = csi->doc->ctx;

	pdf_process_op(csi, PDF_OP_END, &csi->process);
	fz_free(ctx, csi);
}

#define A(a) (a)
#define B(a,b) (a | b << 8)
#define C(a,b,c) (a | b << 8 | c << 16)

static void
parse_inline_image(pdf_csi *csi)
{
	fz_context *ctx = csi->doc->ctx;
	pdf_obj *rdb = csi->rdb;
	fz_stream *file = csi->file;
	int ch, found;

	fz_drop_image(ctx, csi->img);
	csi->img = NULL;
	pdf_drop_obj(csi->obj);
	csi->obj = NULL;

	csi->obj = pdf_parse_dict(csi->doc, file, &csi->doc->lexbuf.base);

	/* read whitespace after ID keyword */
	ch = fz_read_byte(file);
	if (ch == '\r')
		if (fz_peek_byte(file) == '\n')
			fz_read_byte(file);

	fz_try(ctx)
	{
		csi->img = pdf_load_inline_image(csi->doc, rdb, csi->obj, file);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	/* find EI */
	found = 0;
	ch = fz_read_byte(file);
	do
	{
		while (ch != 'E' && ch != EOF)
			ch = fz_read_byte(file);
		if (ch == 'E')
		{
			ch = fz_read_byte(file);
			if (ch == 'I')
			{
				ch = fz_peek_byte(file);
				if (ch == ' ' || ch <= 32 || ch == EOF || ch == '<' || ch == '/')
				{
					found = 1;
					break;
				}
			}
		}
	} while (ch != EOF);
	if (!found)
		fz_throw(ctx, FZ_ERROR_GENERIC, "syntax error after inline image");
}

static int
pdf_run_keyword(pdf_csi *csi, char *buf)
{
	fz_context *ctx = csi->doc->ctx;
	int key;
	PDF_OP op;

	key = buf[0];
	if (buf[1])
	{
		key |= buf[1] << 8;
		if (buf[2])
		{
			key |= buf[2] << 16;
			if (buf[3])
				key = 0;
		}
	}

	switch (key)
	{
	case A('"'): op = PDF_OP_dquote;break;
	case A('\''): op = PDF_OP_squote; break;
	case A('B'): op = PDF_OP_B; break;
	case B('B','*'): op = PDF_OP_Bstar; break;
	case C('B','D','C'): op = PDF_OP_BDC; break;
	case B('B','I'): op = PDF_OP_BI; break;
	case C('B','M','C'): op = PDF_OP_BMC; break;
	case B('B','T'):
		op = PDF_OP_BT;
		csi->in_text = 1;
		break;
	case B('B','X'):
		op = PDF_OP_BX;
		csi->xbalance++;
		break;
	case B('C','S'): op = PDF_OP_CS; break;
	case B('D','P'): op = PDF_OP_DP; break;
	case B('D','o'): op = PDF_OP_Do; break;
	case C('E','M','C'): op = PDF_OP_EMC; break;
	case B('E','T'):
		op = PDF_OP_ET;
		csi->in_text = 0;
		break;
	case B('E','X'):
		op = PDF_OP_EX;
		csi->xbalance--;
		break;
	case A('F'): op = PDF_OP_F; break;
	case A('G'): op = PDF_OP_G; break;
	case A('J'): op = PDF_OP_J; break;
	case A('K'): op = PDF_OP_K; break;
	case A('M'): op = PDF_OP_M; break;
	case B('M','P'): op = PDF_OP_MP; break;
	case A('Q'): op = PDF_OP_Q; break;
	case B('R','G'): op = PDF_OP_RG; break;
	case A('S'): op = PDF_OP_S; break;
	case B('S','C'): op = PDF_OP_SC; break;
	case C('S','C','N'): op = PDF_OP_SCN; break;
	case B('T','*'): op = PDF_OP_Tstar; break;
	case B('T','D'): op = PDF_OP_TD; break;
	case B('T','J'): op = PDF_OP_TJ; break;
	case B('T','L'): op = PDF_OP_TL; break;
	case B('T','c'): op = PDF_OP_Tc; break;
	case B('T','d'): op = PDF_OP_Td; break;
	case B('T','f'): op = PDF_OP_Tf; break;
	case B('T','j'): op = PDF_OP_Tj; break;
	case B('T','m'): op = PDF_OP_Tm; break;
	case B('T','r'): op = PDF_OP_Tr; break;
	case B('T','s'): op = PDF_OP_Ts; break;
	case B('T','w'): op = PDF_OP_Tw; break;
	case B('T','z'): op = PDF_OP_Tz; break;
	case A('W'): op = PDF_OP_W; break;
	case B('W','*'): op = PDF_OP_Wstar; break;
	case A('b'): op = PDF_OP_b; break;
	case B('b','*'): op = PDF_OP_bstar; break;
	case A('c'): op = PDF_OP_c; break;
	case B('c','m'): op = PDF_OP_cm; break;
	case B('c','s'): op = PDF_OP_cs; break;
	case A('d'): op = PDF_OP_d; break;
	case B('d','0'): op = PDF_OP_d0; break;
	case B('d','1'): op = PDF_OP_d1; break;
	case A('f'): op = PDF_OP_f; break;
	case B('f','*'): op = PDF_OP_fstar; break;
	case A('g'): op = PDF_OP_g; break;
	case B('g','s'): op = PDF_OP_gs; break;
	case A('h'): op = PDF_OP_h; break;
	case A('i'): op = PDF_OP_i; break;
	case A('j'): op = PDF_OP_j; break;
	case A('k'): op = PDF_OP_k; break;
	case A('l'): op = PDF_OP_l; break;
	case A('m'): op = PDF_OP_m; break;
	case A('n'): op = PDF_OP_n; break;
	case A('q'): op = PDF_OP_q; break;
	case B('r','e'): op = PDF_OP_re; break;
	case B('r','g'): op = PDF_OP_rg; break;
	case B('r','i'): op = PDF_OP_ri; break;
	case A('s'): op = PDF_OP_s; break;
	case B('s','c'): op = PDF_OP_sc; break;
	case C('s','c','n'): op = PDF_OP_scn; break;
	case B('s','h'): op = PDF_OP_sh; break;
	case A('v'): op = PDF_OP_v; break;
	case A('w'): op = PDF_OP_w; break;
	case A('y'): op = PDF_OP_y; break;
	default:
		if (!csi->xbalance)
		{
			fz_warn(ctx, "unknown keyword: '%s'", buf);
			return 1;
		}
		return 0;
	}

	if (op == PDF_OP_BI)
	{
		parse_inline_image(csi);
	}

	if (op < PDF_OP_Do)
	{
		pdf_process_op(csi, op, &csi->process);
	}
	else if (op < PDF_OP_END)
	{
		fz_try(ctx)
		{
			pdf_process_op(csi, op, &csi->process);
		}
		fz_catch(ctx)
		{
			switch (op)
			{
			case PDF_OP_Do:
				fz_rethrow_message(ctx, "cannot draw xobject/image");
				break;
			case PDF_OP_Tf:
				fz_rethrow_message(ctx, "cannot set font");
				break;
			case PDF_OP_gs:
				fz_rethrow_message(ctx, "cannot set graphics state");
				break;
			case PDF_OP_sh:
				fz_rethrow_message(ctx, "cannot draw shading");
				break;
			default:
				fz_rethrow(ctx); /* Should never happen */
			}
		}
	}
	return 0;
}

void
pdf_process_stream(pdf_csi *csi, pdf_lexbuf *buf)
{
	fz_context *ctx = csi->doc->ctx;
	fz_stream *file = csi->file;
	pdf_token tok = PDF_TOK_ERROR;
	int in_text_array = 0;
	int ignoring_errors = 0;

	/* make sure we have a clean slate if we come here from flush_text */
	pdf_clear_stack(csi);

	fz_var(in_text_array);
	fz_var(tok);

	if (csi->cookie)
	{
		csi->cookie->progress_max = -1;
		csi->cookie->progress = 0;
	}

	do
	{
		fz_try(ctx)
		{
			do
			{
				/* Check the cookie */
				if (csi->cookie)
				{
					if (csi->cookie->abort)
					{
						tok = PDF_TOK_EOF;
						break;
					}
					csi->cookie->progress++;
				}

				tok = pdf_lex(file, buf);

				if (in_text_array)
				{
					switch(tok)
					{
					case PDF_TOK_CLOSE_ARRAY:
						in_text_array = 0;
						break;
					case PDF_TOK_REAL:
						pdf_array_push_drop(csi->obj, pdf_new_real(csi->doc, buf->f));
						break;
					case PDF_TOK_INT:
						pdf_array_push_drop(csi->obj, pdf_new_int(csi->doc, buf->i));
						break;
					case PDF_TOK_STRING:
						pdf_array_push_drop(csi->obj, pdf_new_string(csi->doc, buf->scratch, buf->len));
						break;
					case PDF_TOK_EOF:
						break;
					case PDF_TOK_KEYWORD:
						if (!strcmp(buf->scratch, "Tw") || !strcmp(buf->scratch, "Tc"))
						{
							int l = pdf_array_len(csi->obj);
							if (l > 0)
							{
								pdf_obj *o = pdf_array_get(csi->obj, l-1);
								if (pdf_is_number(o))
								{
									csi->stack[0] = pdf_to_real(o);
									pdf_array_delete(csi->obj, l-1);
									if (pdf_run_keyword(csi, buf->scratch) == 0)
										break;
								}
							}
						}
						/* Deliberate Fallthrough! */
					default:
						fz_throw(ctx, FZ_ERROR_GENERIC, "syntax error in array");
					}
				}
				else switch (tok)
				{
				case PDF_TOK_ENDSTREAM:
				case PDF_TOK_EOF:
					tok = PDF_TOK_EOF;
					break;

				case PDF_TOK_OPEN_ARRAY:
					if (csi->obj)
					{
						pdf_drop_obj(csi->obj);
						csi->obj = NULL;
					}
					if (csi->in_text)
					{
						in_text_array = 1;
						csi->obj = pdf_new_array(csi->doc, 4);
					}
					else
					{
						csi->obj = pdf_parse_array(csi->doc, file, buf);
					}
					break;

				case PDF_TOK_OPEN_DICT:
					if (csi->obj)
					{
						pdf_drop_obj(csi->obj);
						csi->obj = NULL;
					}
					csi->obj = pdf_parse_dict(csi->doc, file, buf);
					break;

				case PDF_TOK_NAME:
					if (csi->name[0])
					{
						pdf_drop_obj(csi->obj);
						csi->obj = NULL;
						csi->obj = pdf_new_name(csi->doc, buf->scratch);
					}
					else
						fz_strlcpy(csi->name, buf->scratch, sizeof(csi->name));
					break;

				case PDF_TOK_INT:
					if (csi->top < nelem(csi->stack)) {
						csi->stack[csi->top] = buf->i;
						csi->top ++;
					}
					else
						fz_throw(ctx, FZ_ERROR_GENERIC, "stack overflow");
					break;

				case PDF_TOK_REAL:
					if (csi->top < nelem(csi->stack)) {
						csi->stack[csi->top] = buf->f;
						csi->top ++;
					}
					else
						fz_throw(ctx, FZ_ERROR_GENERIC, "stack overflow");
					break;

				case PDF_TOK_STRING:
					if (buf->len <= sizeof(csi->string))
					{
						memcpy(csi->string, buf->scratch, buf->len);
						csi->string_len = buf->len;
					}
					else
					{
						if (csi->obj)
						{
							pdf_drop_obj(csi->obj);
							csi->obj = NULL;
						}
						csi->obj = pdf_new_string(csi->doc, buf->scratch, buf->len);
					}
					break;

				case PDF_TOK_KEYWORD:
					/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1982 */
					if (pdf_run_keyword(csi, buf->scratch) && buf->len > 8)
					{
						tok = PDF_TOK_EOF;
					}
					pdf_clear_stack(csi);
					break;

				default:
					fz_throw(ctx, FZ_ERROR_GENERIC, "syntax error in content stream");
				}
			}
			while (tok != PDF_TOK_EOF);
		}
		fz_always(ctx)
		{
			pdf_clear_stack(csi);
		}
		fz_catch(ctx)
		{
			if (!csi->cookie)
			{
				fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
			}
			else if (fz_caught(ctx) == FZ_ERROR_TRYLATER)
			{
				if (csi->cookie->incomplete_ok)
					csi->cookie->incomplete++;
				else
					fz_rethrow(ctx);
			}
			else
			{
				 csi->cookie->errors++;
			}
			if (!ignoring_errors)
			{
				fz_warn(ctx, "Ignoring errors during rendering");
				ignoring_errors = 1;
			}
			/* If we do catch an error, then reset ourselves to a
			 * base lexing state */
			in_text_array = 0;
			/* SumatraPDF: clear the stack on errors */
			pdf_clear_stack(csi);
		}
	}
	while (tok != PDF_TOK_EOF);
}

/*
 * Entry points
 */

static void
pdf_process_contents_stream(pdf_csi *csi, pdf_obj *rdb, fz_stream *file)
{
	fz_context *ctx = csi->doc->ctx;
	pdf_lexbuf *buf;
	int save_in_text;
	pdf_obj *save_obj;
	pdf_obj *save_rdb = csi->rdb;
	fz_stream *save_file = csi->file;

	fz_var(buf);

	if (file == NULL)
		return;

	buf = fz_malloc(ctx, sizeof(*buf)); /* we must be re-entrant for type3 fonts */
	pdf_lexbuf_init(ctx, buf, PDF_LEXBUF_SMALL);
	save_in_text = csi->in_text;
	csi->in_text = 0;
	save_obj = csi->obj;
	csi->obj = NULL;
	csi->rdb = rdb;
	csi->file = file;
	fz_try(ctx)
	{
		csi->process.processor->process_stream(csi, csi->process.state, buf);
	}
	fz_always(ctx)
	{
		csi->in_text = save_in_text;
		pdf_drop_obj(csi->obj);
		csi->obj = save_obj;
		csi->rdb = save_rdb;
		csi->file = save_file;
		pdf_lexbuf_fin(buf);
		fz_free(ctx, buf);
	}
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		fz_warn(ctx, "Content stream parsing error - rendering truncated");
	}
}

void
pdf_process_annot(pdf_document *doc, pdf_page *page, pdf_annot *annot, const pdf_process *process, fz_cookie *cookie)
{
	fz_context *ctx = doc->ctx;
	pdf_csi *csi;
	int flags;

	csi = pdf_new_csi(doc, cookie, process);
	fz_try(ctx)
	{
		flags = pdf_to_int(pdf_dict_gets(annot->obj, "F"));

		/* Check not invisible (bit 0) and hidden (bit 1) */
		/* TODO: NoZoom and NoRotate */
		if (flags & ((1 << 0) | (1 << 1)))
			break;

		csi->process.processor->process_annot(csi, csi->process.state, page->resources, annot);
	}
	fz_always(ctx)
	{
		pdf_free_csi(csi);
	}
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "cannot parse annotation appearance stream");
	}
}

void
pdf_process_contents_object(pdf_csi *csi, pdf_obj *rdb, pdf_obj *contents)
{
	fz_context *ctx = csi->doc->ctx;
	fz_stream *file = NULL;

	if (contents == NULL)
		return;

	file = pdf_open_contents_stream(csi->doc, contents);
	fz_try(ctx)
	{
		pdf_process_contents_stream(csi, rdb, file);
	}
	fz_always(ctx)
	{
		fz_close(file);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
pdf_process_contents_buffer(pdf_csi *csi, pdf_obj *rdb, fz_buffer *contents)
{
	fz_context *ctx = csi->doc->ctx;
	fz_stream *file = NULL;

	if (contents == NULL)
		return;

	file = fz_open_buffer(ctx, contents);
	fz_try(ctx)
	{
		pdf_process_contents_stream(csi, rdb, file);
	}
	fz_always(ctx)
	{
		fz_close(file);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void
pdf_process_stream_object(pdf_document *doc, pdf_obj *obj, const pdf_process *process, pdf_obj *res, fz_cookie *cookie)
{
	fz_context *ctx = doc->ctx;
	pdf_csi *csi;

	csi = pdf_new_csi(doc, cookie, process);
	fz_try(ctx)
	{
		csi->process.processor->process_contents(csi, csi->process.state, res, obj);
	}
	fz_always(ctx)
	{
		pdf_free_csi(csi);
	}
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "cannot parse content stream");
	}
}

void
pdf_process_glyph(pdf_document *doc, pdf_obj *resources, fz_buffer *contents, pdf_process *process)
{
	pdf_csi *csi;
	fz_context *ctx = doc->ctx;

	csi = pdf_new_csi(doc, NULL, process);
	fz_try(ctx)
	{
		pdf_process_contents_buffer(csi, resources, contents);
	}
	fz_always(ctx)
	{
		pdf_free_csi(csi);
	}
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "cannot parse glyph content stream");
	}
}
