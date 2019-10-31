#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>
#include <math.h>

/* Maximum number of errors before aborting */
#define MAX_SYNTAX_ERRORS 100

void *
pdf_new_processor(fz_context *ctx, int size)
{
	return Memento_label(fz_calloc(ctx, 1, size), "pdf_processor");
}

void
pdf_close_processor(fz_context *ctx, pdf_processor *proc)
{
	if (proc && proc->close_processor)
	{
		proc->close_processor(ctx, proc);
		proc->close_processor = NULL;
	}
}

void
pdf_drop_processor(fz_context *ctx, pdf_processor *proc)
{
	if (proc)
	{
		if (proc->close_processor)
			fz_warn(ctx, "dropping unclosed PDF processor");
		if (proc->drop_processor)
			proc->drop_processor(ctx, proc);
	}
	fz_free(ctx, proc);
}

static void
pdf_init_csi(fz_context *ctx, pdf_csi *csi, pdf_document *doc, pdf_obj *rdb, pdf_lexbuf *buf, fz_cookie *cookie)
{
	memset(csi, 0, sizeof *csi);
	csi->doc = doc;
	csi->rdb = rdb;
	csi->buf = buf;
	csi->cookie = cookie;
}

static void
pdf_clear_stack(fz_context *ctx, pdf_csi *csi)
{
	int i;

	pdf_drop_obj(ctx, csi->obj);
	csi->obj = NULL;

	csi->name[0] = 0;
	csi->string_len = 0;
	for (i = 0; i < csi->top; i++)
		csi->stack[i] = 0;

	csi->top = 0;
}

static pdf_font_desc *
pdf_try_load_font(fz_context *ctx, pdf_document *doc, pdf_obj *rdb, pdf_obj *font, fz_cookie *cookie)
{
	pdf_font_desc *desc = NULL;
	fz_try(ctx)
		desc = pdf_load_font(ctx, doc, rdb, font);
	fz_catch(ctx)
	{
		if (fz_caught(ctx) == FZ_ERROR_TRYLATER)
			if (cookie)
				cookie->incomplete++;
	}
	if (desc == NULL)
		desc = pdf_load_hail_mary_font(ctx, doc);
	return desc;
}

static fz_image *
parse_inline_image(fz_context *ctx, pdf_csi *csi, fz_stream *stm, char *csname, int cslen)
{
	pdf_document *doc = csi->doc;
	pdf_obj *rdb = csi->rdb;
	pdf_obj *obj = NULL;
	pdf_obj *cs;
	fz_image *img = NULL;
	int ch, found;

	fz_var(obj);
	fz_var(img);

	fz_try(ctx)
	{
		obj = pdf_parse_dict(ctx, doc, stm, &doc->lexbuf.base);

		if (csname)
		{
			cs = pdf_dict_get(ctx, obj, PDF_NAME(CS));
			if (!pdf_is_indirect(ctx, cs) && pdf_is_name(ctx, cs))
				fz_strlcpy(csname, pdf_to_name(ctx, cs), cslen);
			else
				csname[0] = 0;
		}

		/* read whitespace after ID keyword */
		ch = fz_read_byte(ctx, stm);
		if (ch == '\r')
			if (fz_peek_byte(ctx, stm) == '\n')
				fz_read_byte(ctx, stm);

		img = pdf_load_inline_image(ctx, doc, rdb, obj, stm);

		/* find EI */
		found = 0;
		ch = fz_read_byte(ctx, stm);
		do
		{
			while (ch != 'E' && ch != EOF)
				ch = fz_read_byte(ctx, stm);
			if (ch == 'E')
			{
				ch = fz_read_byte(ctx, stm);
				if (ch == 'I')
				{
					ch = fz_peek_byte(ctx, stm);
					if (ch == ' ' || ch <= 32 || ch == EOF || ch == '<' || ch == '/')
					{
						found = 1;
						break;
					}
				}
			}
		} while (ch != EOF);
		if (!found)
			fz_throw(ctx, FZ_ERROR_SYNTAX, "syntax error after inline image");
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, obj);
	}
	fz_catch(ctx)
	{
		fz_drop_image(ctx, img);
		fz_rethrow(ctx);
	}

	return img;
}

static void
pdf_process_extgstate(fz_context *ctx, pdf_processor *proc, pdf_csi *csi, pdf_obj *dict)
{
	pdf_obj *obj;

	obj = pdf_dict_get(ctx, dict, PDF_NAME(LW));
	if (pdf_is_number(ctx, obj) && proc->op_w)
		proc->op_w(ctx, proc, pdf_to_real(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(LC));
	if (pdf_is_int(ctx, obj) && proc->op_J)
		proc->op_J(ctx, proc, fz_clampi(pdf_to_int(ctx, obj), 0, 2));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(LJ));
	if (pdf_is_int(ctx, obj) && proc->op_j)
		proc->op_j(ctx, proc, fz_clampi(pdf_to_int(ctx, obj), 0, 2));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(ML));
	if (pdf_is_number(ctx, obj) && proc->op_M)
		proc->op_M(ctx, proc, pdf_to_real(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(D));
	if (pdf_is_array(ctx, obj) && proc->op_d)
	{
		pdf_obj *dash_array = pdf_array_get(ctx, obj, 0);
		pdf_obj *dash_phase = pdf_array_get(ctx, obj, 1);
		proc->op_d(ctx, proc, dash_array, pdf_to_real(ctx, dash_phase));
	}

	obj = pdf_dict_get(ctx, dict, PDF_NAME(RI));
	if (pdf_is_name(ctx, obj) && proc->op_ri)
		proc->op_ri(ctx, proc, pdf_to_name(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(FL));
	if (pdf_is_number(ctx, obj) && proc->op_i)
		proc->op_i(ctx, proc, pdf_to_real(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(Font));
	if (pdf_is_array(ctx, obj) && proc->op_Tf)
	{
		pdf_obj *font_ref = pdf_array_get(ctx, obj, 0);
		pdf_obj *font_size = pdf_array_get(ctx, obj, 1);
		pdf_font_desc *font;
		if (pdf_is_dict(ctx, font_ref))
			font = pdf_try_load_font(ctx, csi->doc, csi->rdb, font_ref, csi->cookie);
		else
			font = pdf_load_hail_mary_font(ctx, csi->doc);
		fz_try(ctx)
			proc->op_Tf(ctx, proc, "ExtGState", font, pdf_to_real(ctx, font_size));
		fz_always(ctx)
			pdf_drop_font(ctx, font);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}

	/* overprint and color management */

	obj = pdf_dict_get(ctx, dict, PDF_NAME(OP));
	if (pdf_is_bool(ctx, obj) && proc->op_gs_OP)
		proc->op_gs_OP(ctx, proc, pdf_to_bool(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(op));
	if (pdf_is_bool(ctx, obj) && proc->op_gs_op)
		proc->op_gs_op(ctx, proc, pdf_to_bool(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(OPM));
	if (pdf_is_int(ctx, obj) && proc->op_gs_OPM)
		proc->op_gs_OPM(ctx, proc, pdf_to_int(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(UseBlackPtComp));
	if (pdf_is_name(ctx, obj) && proc->op_gs_UseBlackPtComp)
		proc->op_gs_UseBlackPtComp(ctx, proc, obj);

	/* transfer functions */

	obj = pdf_dict_get(ctx, dict, PDF_NAME(TR2));
	if (pdf_is_name(ctx, obj))
		if (!pdf_name_eq(ctx, obj, PDF_NAME(Identity)) && !pdf_name_eq(ctx, obj, PDF_NAME(Default)))
			fz_warn(ctx, "ignoring transfer function");
	if (!obj) /* TR is ignored in the presence of TR2 */
	{
		pdf_obj *tr = pdf_dict_get(ctx, dict, PDF_NAME(TR));
		if (pdf_is_name(ctx, tr))
			if (!pdf_name_eq(ctx, tr, PDF_NAME(Identity)))
				fz_warn(ctx, "ignoring transfer function");
	}

	/* transparency state */

	obj = pdf_dict_get(ctx, dict, PDF_NAME(CA));
	if (pdf_is_number(ctx, obj) && proc->op_gs_CA)
		proc->op_gs_CA(ctx, proc, pdf_to_real(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(ca));
	if (pdf_is_number(ctx, obj) && proc->op_gs_ca)
		proc->op_gs_ca(ctx, proc, pdf_to_real(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(BM));
	if (pdf_is_array(ctx, obj))
		obj = pdf_array_get(ctx, obj, 0);
	if (pdf_is_name(ctx, obj) && proc->op_gs_BM)
		proc->op_gs_BM(ctx, proc, pdf_to_name(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(SMask));
	if (proc->op_gs_SMask)
	{
		if (pdf_is_dict(ctx, obj))
		{
			pdf_obj *xobj, *s, *bc, *tr;
			float softmask_bc[FZ_MAX_COLORS];
			fz_colorspace *colorspace;
			int colorspace_n = 1;
			int k, luminosity;

			xobj = pdf_dict_get(ctx, obj, PDF_NAME(G));

			colorspace = pdf_xobject_colorspace(ctx, xobj);
			if (colorspace)
				colorspace_n = fz_colorspace_n(ctx, colorspace);

			/* Default background color is black. */
			for (k = 0; k < colorspace_n; k++)
				softmask_bc[k] = 0;
			/* Which in CMYK means not all zeros! This should really be
			 * a test for subtractive color spaces, but this will have
			 * to do for now. */
			if (fz_colorspace_is_cmyk(ctx, colorspace))
				softmask_bc[3] = 1.0f;
			fz_drop_colorspace(ctx, colorspace);

			bc = pdf_dict_get(ctx, obj, PDF_NAME(BC));
			if (pdf_is_array(ctx, bc))
			{
				for (k = 0; k < colorspace_n; k++)
					softmask_bc[k] = pdf_array_get_real(ctx, bc, k);
			}

			s = pdf_dict_get(ctx, obj, PDF_NAME(S));
			if (pdf_name_eq(ctx, s, PDF_NAME(Luminosity)))
				luminosity = 1;
			else
				luminosity = 0;

			tr = pdf_dict_get(ctx, obj, PDF_NAME(TR));
			if (tr && !pdf_name_eq(ctx, tr, PDF_NAME(Identity)))
				fz_warn(ctx, "ignoring transfer function");

			proc->op_gs_SMask(ctx, proc, xobj, csi->rdb, softmask_bc, luminosity);
		}
		else if (pdf_is_name(ctx, obj) && pdf_name_eq(ctx, obj, PDF_NAME(None)))
		{
			proc->op_gs_SMask(ctx, proc, NULL, NULL, NULL, 0);
		}
	}
}

static void
pdf_process_Do(fz_context *ctx, pdf_processor *proc, pdf_csi *csi)
{
	pdf_obj *xres, *xobj, *subtype;

	xres = pdf_dict_get(ctx, csi->rdb, PDF_NAME(XObject));
	xobj = pdf_dict_gets(ctx, xres, csi->name);
	if (!xobj)
		fz_throw(ctx, FZ_ERROR_MINOR, "cannot find XObject resource '%s'", csi->name);
	subtype = pdf_dict_get(ctx, xobj, PDF_NAME(Subtype));
	if (pdf_name_eq(ctx, subtype, PDF_NAME(Form)))
	{
		pdf_obj *st = pdf_dict_get(ctx, xobj, PDF_NAME(Subtype2));
		if (st)
			subtype = st;
	}
	if (!pdf_is_name(ctx, subtype))
		fz_throw(ctx, FZ_ERROR_MINOR, "no XObject subtype specified");

	if (pdf_is_hidden_ocg(ctx, csi->doc->ocg, csi->rdb, proc->usage, pdf_dict_get(ctx, xobj, PDF_NAME(OC))))
		return;

	if (pdf_name_eq(ctx, subtype, PDF_NAME(Form)))
	{
		if (proc->op_Do_form)
			proc->op_Do_form(ctx, proc, csi->name, xobj, csi->rdb);
	}

	else if (pdf_name_eq(ctx, subtype, PDF_NAME(Image)))
	{
		if (proc->op_Do_image)
		{
			fz_image *image = pdf_load_image(ctx, csi->doc, xobj);
			fz_try(ctx)
				proc->op_Do_image(ctx, proc, csi->name, image);
			fz_always(ctx)
				fz_drop_image(ctx, image);
			fz_catch(ctx)
				fz_rethrow(ctx);
		}
	}

	else if (!strcmp(pdf_to_name(ctx, subtype), "PS"))
		fz_warn(ctx, "ignoring XObject with subtype PS");
	else
		fz_warn(ctx, "ignoring XObject with unknown subtype: '%s'", pdf_to_name(ctx, subtype));
}

static void
pdf_process_CS(fz_context *ctx, pdf_processor *proc, pdf_csi *csi, int stroke)
{
	if (!proc->op_CS || !proc->op_cs)
		return;

	if (!strcmp(csi->name, "Pattern"))
	{
		if (stroke)
			proc->op_CS(ctx, proc, "Pattern", NULL);
		else
			proc->op_cs(ctx, proc, "Pattern", NULL);
	}
	else
	{
		fz_colorspace *cs;

		if (!strcmp(csi->name, "DeviceGray"))
			cs = fz_keep_colorspace(ctx, fz_device_gray(ctx));
		else if (!strcmp(csi->name, "DeviceRGB"))
			cs = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
		else if (!strcmp(csi->name, "DeviceCMYK"))
			cs = fz_keep_colorspace(ctx, fz_device_cmyk(ctx));
		else
		{
			pdf_obj *csres, *csobj;
			csres = pdf_dict_get(ctx, csi->rdb, PDF_NAME(ColorSpace));
			csobj = pdf_dict_gets(ctx, csres, csi->name);
			if (!csobj)
				fz_throw(ctx, FZ_ERROR_MINOR, "cannot find ColorSpace resource '%s'", csi->name);
			cs = pdf_load_colorspace(ctx, csobj);
		}

		fz_try(ctx)
		{
			if (stroke)
				proc->op_CS(ctx, proc, csi->name, cs);
			else
				proc->op_cs(ctx, proc, csi->name, cs);
		}
		fz_always(ctx)
			fz_drop_colorspace(ctx, cs);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
}

static void
pdf_process_SC(fz_context *ctx, pdf_processor *proc, pdf_csi *csi, int stroke)
{
	if (csi->name[0])
	{
		pdf_obj *patres, *patobj, *type;

		patres = pdf_dict_get(ctx, csi->rdb, PDF_NAME(Pattern));
		patobj = pdf_dict_gets(ctx, patres, csi->name);
		if (!patobj)
			fz_throw(ctx, FZ_ERROR_MINOR, "cannot find Pattern resource '%s'", csi->name);

		type = pdf_dict_get(ctx, patobj, PDF_NAME(PatternType));

		if (pdf_to_int(ctx, type) == 1)
		{
			if (proc->op_SC_pattern && proc->op_sc_pattern)
			{
				pdf_pattern *pat = pdf_load_pattern(ctx, csi->doc, patobj);
				fz_try(ctx)
				{
					if (stroke)
						proc->op_SC_pattern(ctx, proc, csi->name, pat, csi->top, csi->stack);
					else
						proc->op_sc_pattern(ctx, proc, csi->name, pat, csi->top, csi->stack);
				}
				fz_always(ctx)
					pdf_drop_pattern(ctx, pat);
				fz_catch(ctx)
					fz_rethrow(ctx);
			}
		}

		else if (pdf_to_int(ctx, type) == 2)
		{
			if (proc->op_SC_shade && proc->op_sc_shade)
			{
				fz_shade *shade = pdf_load_shading(ctx, csi->doc, patobj);
				fz_try(ctx)
				{
					if (stroke)
						proc->op_SC_shade(ctx, proc, csi->name, shade);
					else
						proc->op_sc_shade(ctx, proc, csi->name, shade);
				}
				fz_always(ctx)
					fz_drop_shade(ctx, shade);
				fz_catch(ctx)
					fz_rethrow(ctx);
			}
		}

		else
		{
			fz_throw(ctx, FZ_ERROR_MINOR, "unknown pattern type: %d", pdf_to_int(ctx, type));
		}
	}

	else
	{
		if (proc->op_SC_color && proc->op_sc_color)
		{
			if (stroke)
				proc->op_SC_color(ctx, proc, csi->top, csi->stack);
			else
				proc->op_sc_color(ctx, proc, csi->top, csi->stack);
		}
	}
}

static pdf_obj *
resolve_properties(fz_context *ctx, pdf_csi *csi, pdf_obj *obj)
{
	if (pdf_is_name(ctx, obj))
		return pdf_dict_get(ctx, pdf_dict_get(ctx, csi->rdb, PDF_NAME(Properties)), obj);
	else
		return obj;
}

static void
pdf_process_BDC(fz_context *ctx, pdf_processor *proc, pdf_csi *csi)
{
	if (proc->op_BDC)
		proc->op_BDC(ctx, proc, csi->name, csi->obj, resolve_properties(ctx, csi, csi->obj));

	/* Already hidden, no need to look further */
	if (proc->hidden > 0)
	{
		++proc->hidden;
		return;
	}

	/* We only look at OC groups here */
	if (strcmp(csi->name, "OC"))
		return;

	if (pdf_is_hidden_ocg(ctx, csi->doc->ocg, csi->rdb, proc->usage, csi->obj))
		++proc->hidden;
}

static void
pdf_process_BMC(fz_context *ctx, pdf_processor *proc, pdf_csi *csi, const char *name)
{
	if (proc->op_BMC)
		proc->op_BMC(ctx, proc, name);
	if (proc->hidden > 0)
		++proc->hidden;
}

static void
pdf_process_EMC(fz_context *ctx, pdf_processor *proc, pdf_csi *csi)
{
	if (proc->op_EMC)
		proc->op_EMC(ctx, proc);
	if (proc->hidden > 0)
		--proc->hidden;
}

static void
pdf_process_gsave(fz_context *ctx, pdf_processor *proc, pdf_csi *csi)
{
	if (proc->op_q)
		proc->op_q(ctx, proc);
	++csi->gstate;
}

static void
pdf_process_grestore(fz_context *ctx, pdf_processor *proc, pdf_csi *csi)
{
	if (csi->gstate > 0)
	{
		if (proc->op_Q)
			proc->op_Q(ctx, proc);
		--csi->gstate;
	}
}

static void
pdf_process_end(fz_context *ctx, pdf_processor *proc, pdf_csi *csi)
{
	while (csi->gstate > 0)
		pdf_process_grestore(ctx, proc, csi);
	if (proc->op_END)
		proc->op_END(ctx, proc);
}

#define A(a) (a)
#define B(a,b) (a | b << 8)
#define C(a,b,c) (a | b << 8 | c << 16)

static void
pdf_process_keyword(fz_context *ctx, pdf_processor *proc, pdf_csi *csi, fz_stream *stm, char *word)
{
	float *s = csi->stack;
	char csname[40];
	int key;

	key = word[0];
	if (word[1])
	{
		key |= word[1] << 8;
		if (word[2])
		{
			key |= word[2] << 16;
			if (word[3])
				key = 0;
		}
	}

	switch (key)
	{
	default:
		if (!csi->xbalance)
			fz_throw(ctx, FZ_ERROR_SYNTAX, "unknown keyword: '%s'", word);
		break;

	/* general graphics state */
	case A('w'): if (proc->op_w) proc->op_w(ctx, proc, s[0]); break;
	case A('j'): if (proc->op_j) proc->op_j(ctx, proc, fz_clampi(s[0], 0, 2)); break;
	case A('J'): if (proc->op_J) proc->op_J(ctx, proc, fz_clampi(s[0], 0, 2)); break;
	case A('M'): if (proc->op_M) proc->op_M(ctx, proc, s[0]); break;
	case A('d'): if (proc->op_d) proc->op_d(ctx, proc, csi->obj, s[0]); break;
	case B('r','i'): if (proc->op_ri) proc->op_ri(ctx, proc, csi->name); break;
	case A('i'): if (proc->op_i) proc->op_i(ctx, proc, s[0]); break;

	case B('g','s'):
		{
			pdf_obj *gsres, *gsobj;
			gsres = pdf_dict_get(ctx, csi->rdb, PDF_NAME(ExtGState));
			gsobj = pdf_dict_gets(ctx, gsres, csi->name);
			if (!gsobj)
				fz_throw(ctx, FZ_ERROR_MINOR, "cannot find ExtGState resource '%s'", csi->name);
			if (proc->op_gs_begin)
				proc->op_gs_begin(ctx, proc, csi->name, gsobj);
			pdf_process_extgstate(ctx, proc, csi, gsobj);
			if (proc->op_gs_end)
				proc->op_gs_end(ctx, proc);
		}
		break;

	/* special graphics state */
	case A('q'): pdf_process_gsave(ctx, proc, csi); break;
	case A('Q'): pdf_process_grestore(ctx, proc, csi); break;
	case B('c','m'): if (proc->op_cm) proc->op_cm(ctx, proc, s[0], s[1], s[2], s[3], s[4], s[5]); break;

	/* path construction */
	case A('m'): if (proc->op_m) proc->op_m(ctx, proc, s[0], s[1]); break;
	case A('l'): if (proc->op_l) proc->op_l(ctx, proc, s[0], s[1]); break;
	case A('c'): if (proc->op_c) proc->op_c(ctx, proc, s[0], s[1], s[2], s[3], s[4], s[5]); break;
	case A('v'): if (proc->op_v) proc->op_v(ctx, proc, s[0], s[1], s[2], s[3]); break;
	case A('y'): if (proc->op_y) proc->op_y(ctx, proc, s[0], s[1], s[2], s[3]); break;
	case A('h'): if (proc->op_h) proc->op_h(ctx, proc); break;
	case B('r','e'): if (proc->op_re) proc->op_re(ctx, proc, s[0], s[1], s[2], s[3]); break;

	/* path painting */
	case A('S'): if (proc->op_S) proc->op_S(ctx, proc); break;
	case A('s'): if (proc->op_s) proc->op_s(ctx, proc); break;
	case A('F'): if (proc->op_F) proc->op_F(ctx, proc); break;
	case A('f'): if (proc->op_f) proc->op_f(ctx, proc); break;
	case B('f','*'): if (proc->op_fstar) proc->op_fstar(ctx, proc); break;
	case A('B'): if (proc->op_B) proc->op_B(ctx, proc); break;
	case B('B','*'): if (proc->op_Bstar) proc->op_Bstar(ctx, proc); break;
	case A('b'): if (proc->op_b) proc->op_b(ctx, proc); break;
	case B('b','*'): if (proc->op_bstar) proc->op_bstar(ctx, proc); break;
	case A('n'): if (proc->op_n) proc->op_n(ctx, proc); break;

	/* path clipping */
	case A('W'): if (proc->op_W) proc->op_W(ctx, proc); break;
	case B('W','*'): if (proc->op_Wstar) proc->op_Wstar(ctx, proc); break;

	/* text objects */
	case B('B','T'): csi->in_text = 1; if (proc->op_BT) proc->op_BT(ctx, proc); break;
	case B('E','T'): csi->in_text = 0; if (proc->op_ET) proc->op_ET(ctx, proc); break;

	/* text state */
	case B('T','c'): if (proc->op_Tc) proc->op_Tc(ctx, proc, s[0]); break;
	case B('T','w'): if (proc->op_Tw) proc->op_Tw(ctx, proc, s[0]); break;
	case B('T','z'): if (proc->op_Tz) proc->op_Tz(ctx, proc, s[0]); break;
	case B('T','L'): if (proc->op_TL) proc->op_TL(ctx, proc, s[0]); break;
	case B('T','r'): if (proc->op_Tr) proc->op_Tr(ctx, proc, s[0]); break;
	case B('T','s'): if (proc->op_Ts) proc->op_Ts(ctx, proc, s[0]); break;

	case B('T','f'):
		if (proc->op_Tf)
		{
			pdf_obj *fontres, *fontobj;
			pdf_font_desc *font;
			fontres = pdf_dict_get(ctx, csi->rdb, PDF_NAME(Font));
			fontobj = pdf_dict_gets(ctx, fontres, csi->name);
			if (pdf_is_dict(ctx, fontobj))
				font = pdf_try_load_font(ctx, csi->doc, csi->rdb, fontobj, csi->cookie);
			else
				font = pdf_load_hail_mary_font(ctx, csi->doc);
			fz_try(ctx)
				proc->op_Tf(ctx, proc, csi->name, font, s[0]);
			fz_always(ctx)
				pdf_drop_font(ctx, font);
			fz_catch(ctx)
				fz_rethrow(ctx);
		}
		break;

	/* text positioning */
	case B('T','d'): if (proc->op_Td) proc->op_Td(ctx, proc, s[0], s[1]); break;
	case B('T','D'): if (proc->op_TD) proc->op_TD(ctx, proc, s[0], s[1]); break;
	case B('T','m'): if (proc->op_Tm) proc->op_Tm(ctx, proc, s[0], s[1], s[2], s[3], s[4], s[5]); break;
	case B('T','*'): if (proc->op_Tstar) proc->op_Tstar(ctx, proc); break;

	/* text showing */
	case B('T','J'): if (proc->op_TJ) proc->op_TJ(ctx, proc, csi->obj); break;
	case B('T','j'):
		if (proc->op_Tj)
		{
			if (csi->string_len > 0)
				proc->op_Tj(ctx, proc, csi->string, csi->string_len);
			else
				proc->op_Tj(ctx, proc, pdf_to_str_buf(ctx, csi->obj), pdf_to_str_len(ctx, csi->obj));
		}
		break;
	case A('\''):
		if (proc->op_squote)
		{
			if (csi->string_len > 0)
				proc->op_squote(ctx, proc, csi->string, csi->string_len);
			else
				proc->op_squote(ctx, proc, pdf_to_str_buf(ctx, csi->obj), pdf_to_str_len(ctx, csi->obj));
		}
		break;
	case A('"'):
		if (proc->op_dquote)
		{
			if (csi->string_len > 0)
				proc->op_dquote(ctx, proc, s[0], s[1], csi->string, csi->string_len);
			else
				proc->op_dquote(ctx, proc, s[0], s[1], pdf_to_str_buf(ctx, csi->obj), pdf_to_str_len(ctx, csi->obj));
		}
		break;

	/* type 3 fonts */
	case B('d','0'): if (proc->op_d0) proc->op_d0(ctx, proc, s[0], s[1]); break;
	case B('d','1'): if (proc->op_d1) proc->op_d1(ctx, proc, s[0], s[1], s[2], s[3], s[4], s[5]); break;

	/* color */
	case B('C','S'): pdf_process_CS(ctx, proc, csi, 1); break;
	case B('c','s'): pdf_process_CS(ctx, proc, csi, 0); break;
	case B('S','C'): pdf_process_SC(ctx, proc, csi, 1); break;
	case B('s','c'): pdf_process_SC(ctx, proc, csi, 0); break;
	case C('S','C','N'): pdf_process_SC(ctx, proc, csi, 1); break;
	case C('s','c','n'): pdf_process_SC(ctx, proc, csi, 0); break;

	case A('G'): if (proc->op_G) proc->op_G(ctx, proc, s[0]); break;
	case A('g'): if (proc->op_g) proc->op_g(ctx, proc, s[0]); break;
	case B('R','G'): if (proc->op_RG) proc->op_RG(ctx, proc, s[0], s[1], s[2]); break;
	case B('r','g'): if (proc->op_rg) proc->op_rg(ctx, proc, s[0], s[1], s[2]); break;
	case A('K'): if (proc->op_K) proc->op_K(ctx, proc, s[0], s[1], s[2], s[3]); break;
	case A('k'): if (proc->op_k) proc->op_k(ctx, proc, s[0], s[1], s[2], s[3]); break;

	/* shadings, images, xobjects */
	case B('B','I'):
		{
			fz_image *img = parse_inline_image(ctx, csi, stm, csname, sizeof csname);
			fz_try(ctx)
			{
				if (proc->op_BI)
					proc->op_BI(ctx, proc, img, csname[0] ? csname : NULL);
			}
			fz_always(ctx)
				fz_drop_image(ctx, img);
			fz_catch(ctx)
				fz_rethrow(ctx);
		}
		break;

	case B('s','h'):
		if (proc->op_sh)
		{
			pdf_obj *shaderes, *shadeobj;
			fz_shade *shade;
			shaderes = pdf_dict_get(ctx, csi->rdb, PDF_NAME(Shading));
			shadeobj = pdf_dict_gets(ctx, shaderes, csi->name);
			if (!shadeobj)
				fz_throw(ctx, FZ_ERROR_MINOR, "cannot find Shading resource '%s'", csi->name);
			shade = pdf_load_shading(ctx, csi->doc, shadeobj);
			fz_try(ctx)
				proc->op_sh(ctx, proc, csi->name, shade);
			fz_always(ctx)
				fz_drop_shade(ctx, shade);
			fz_catch(ctx)
				fz_rethrow(ctx);
		}
		break;

	case B('D','o'): pdf_process_Do(ctx, proc, csi); break;

	/* marked content */
	case B('M','P'): if (proc->op_MP) proc->op_MP(ctx, proc, csi->name); break;
	case B('D','P'): if (proc->op_DP) proc->op_DP(ctx, proc, csi->name, csi->obj, resolve_properties(ctx, csi, csi->obj)); break;
	case C('B','M','C'): pdf_process_BMC(ctx, proc, csi, csi->name); break;
	case C('B','D','C'): pdf_process_BDC(ctx, proc, csi); break;
	case C('E','M','C'): pdf_process_EMC(ctx, proc, csi); break;

	/* compatibility */
	case B('B','X'): ++csi->xbalance; if (proc->op_BX) proc->op_BX(ctx, proc); break;
	case B('E','X'): --csi->xbalance; if (proc->op_EX) proc->op_EX(ctx, proc); break;
	}
}

static void
pdf_process_stream(fz_context *ctx, pdf_processor *proc, pdf_csi *csi, fz_stream *stm)
{
	pdf_document *doc = csi->doc;
	pdf_lexbuf *buf = csi->buf;
	fz_cookie *cookie = csi->cookie;

	pdf_token tok = PDF_TOK_ERROR;
	int in_text_array = 0;
	int syntax_errors = 0;

	/* make sure we have a clean slate if we come here from flush_text */
	pdf_clear_stack(ctx, csi);

	fz_var(in_text_array);
	fz_var(tok);

	if (cookie)
	{
		cookie->progress_max = -1;
		cookie->progress = 0;
	}

	do
	{
		fz_try(ctx)
		{
			do
			{
				/* Check the cookie */
				if (cookie)
				{
					if (cookie->abort)
					{
						tok = PDF_TOK_EOF;
						break;
					}
					cookie->progress++;
				}

				tok = pdf_lex(ctx, stm, buf);

				if (in_text_array)
				{
					switch(tok)
					{
					case PDF_TOK_CLOSE_ARRAY:
						in_text_array = 0;
						break;
					case PDF_TOK_REAL:
						pdf_array_push_real(ctx, csi->obj, buf->f);
						break;
					case PDF_TOK_INT:
						pdf_array_push_int(ctx, csi->obj, buf->i);
						break;
					case PDF_TOK_STRING:
						pdf_array_push_string(ctx, csi->obj, buf->scratch, buf->len);
						break;
					case PDF_TOK_EOF:
						break;
					case PDF_TOK_KEYWORD:
						if (buf->scratch[0] == 'T' && (buf->scratch[1] == 'w' || buf->scratch[1] == 'c') && buf->scratch[2] == 0)
						{
							int n = pdf_array_len(ctx, csi->obj);
							if (n > 0)
							{
								pdf_obj *o = pdf_array_get(ctx, csi->obj, n-1);
								if (pdf_is_number(ctx, o))
								{
									csi->stack[0] = pdf_to_real(ctx, o);
									pdf_array_delete(ctx, csi->obj, n-1);
									pdf_process_keyword(ctx, proc, csi, stm, buf->scratch);
								}
							}
						}
						/* Deliberate Fallthrough! */
					default:
						fz_throw(ctx, FZ_ERROR_SYNTAX, "syntax error in array");
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
						pdf_drop_obj(ctx, csi->obj);
						csi->obj = NULL;
					}
					if (csi->in_text)
					{
						in_text_array = 1;
						csi->obj = pdf_new_array(ctx, doc, 4);
					}
					else
					{
						csi->obj = pdf_parse_array(ctx, doc, stm, buf);
					}
					break;

				case PDF_TOK_OPEN_DICT:
					if (csi->obj)
					{
						pdf_drop_obj(ctx, csi->obj);
						csi->obj = NULL;
					}
					csi->obj = pdf_parse_dict(ctx, doc, stm, buf);
					break;

				case PDF_TOK_NAME:
					if (csi->name[0])
					{
						pdf_drop_obj(ctx, csi->obj);
						csi->obj = NULL;
						csi->obj = pdf_new_name(ctx, buf->scratch);
					}
					else
						fz_strlcpy(csi->name, buf->scratch, sizeof(csi->name));
					break;

				case PDF_TOK_INT:
					if (csi->top < (int)nelem(csi->stack)) {
						csi->stack[csi->top] = buf->i;
						csi->top ++;
					}
					else
						fz_throw(ctx, FZ_ERROR_SYNTAX, "stack overflow");
					break;

				case PDF_TOK_REAL:
					if (csi->top < (int)nelem(csi->stack)) {
						csi->stack[csi->top] = buf->f;
						csi->top ++;
					}
					else
						fz_throw(ctx, FZ_ERROR_SYNTAX, "stack overflow");
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
							pdf_drop_obj(ctx, csi->obj);
							csi->obj = NULL;
						}
						csi->obj = pdf_new_string(ctx, buf->scratch, buf->len);
					}
					break;

				case PDF_TOK_KEYWORD:
					pdf_process_keyword(ctx, proc, csi, stm, buf->scratch);
					pdf_clear_stack(ctx, csi);
					break;

				default:
					fz_throw(ctx, FZ_ERROR_SYNTAX, "syntax error in content stream");
				}
			}
			while (tok != PDF_TOK_EOF);
		}
		fz_always(ctx)
		{
			pdf_clear_stack(ctx, csi);
		}
		fz_catch(ctx)
		{
			int caught = fz_caught(ctx);
			if (cookie)
			{
				if (caught == FZ_ERROR_TRYLATER)
				{
					cookie->incomplete++;
					tok = PDF_TOK_EOF;
				}
				else if (caught == FZ_ERROR_ABORT)
				{
					fz_rethrow(ctx);
				}
				else if (caught == FZ_ERROR_MINOR)
				{
					cookie->errors++;
				}
				else if (caught == FZ_ERROR_SYNTAX)
				{
					cookie->errors++;
					if (++syntax_errors >= MAX_SYNTAX_ERRORS)
					{
						fz_warn(ctx, "too many syntax errors; ignoring rest of page");
						tok = PDF_TOK_EOF;
					}
				}
				else
				{
					fz_rethrow(ctx);
				}
			}
			else
			{
				if (caught == FZ_ERROR_TRYLATER)
					tok = PDF_TOK_EOF;
				else if (caught == FZ_ERROR_ABORT)
					fz_rethrow(ctx);
				else if (caught == FZ_ERROR_MINOR)
					/* ignore minor errors */ ;
				else if (caught == FZ_ERROR_SYNTAX)
				{
					if (++syntax_errors >= MAX_SYNTAX_ERRORS)
					{
						fz_warn(ctx, "too many syntax errors; ignoring rest of page");
						tok = PDF_TOK_EOF;
					}
				}
				else
				{
					fz_rethrow(ctx);
				}
			}

			/* If we do catch an error, then reset ourselves to a base lexing state */
			in_text_array = 0;
		}
	}
	while (tok != PDF_TOK_EOF);
}

/* Functions to actually process annotations, glyphs and general stream objects */
void
pdf_process_contents(fz_context *ctx, pdf_processor *proc, pdf_document *doc, pdf_obj *rdb, pdf_obj *stmobj, fz_cookie *cookie)
{
	pdf_csi csi;
	pdf_lexbuf buf;
	fz_stream *stm = NULL;

	if (!stmobj)
		return;

	fz_var(stm);

	pdf_lexbuf_init(ctx, &buf, PDF_LEXBUF_SMALL);
	pdf_init_csi(ctx, &csi, doc, rdb, &buf, cookie);

	fz_try(ctx)
	{
		fz_defer_reap_start(ctx);
		stm = pdf_open_contents_stream(ctx, doc, stmobj);
		pdf_process_stream(ctx, proc, &csi, stm);
		pdf_process_end(ctx, proc, &csi);
	}
	fz_always(ctx)
	{
		fz_defer_reap_end(ctx);
		fz_drop_stream(ctx, stm);
		pdf_clear_stack(ctx, &csi);
		pdf_lexbuf_fin(ctx, &buf);
	}
	fz_catch(ctx)
	{
		proc->close_processor = NULL; /* aborted run, don't warn about unclosed processor */
		fz_rethrow(ctx);
	}
}

void
pdf_process_annot(fz_context *ctx, pdf_processor *proc, pdf_document *doc, pdf_page *page, pdf_annot *annot, fz_cookie *cookie)
{
	int flags = pdf_dict_get_int(ctx, annot->obj, PDF_NAME(F));

	if (flags & (PDF_ANNOT_IS_INVISIBLE | PDF_ANNOT_IS_HIDDEN))
		return;

	/* popup annotations should never be drawn */
	if (pdf_annot_type(ctx, annot) == PDF_ANNOT_POPUP)
		return;

	if (proc->usage)
	{
		if (!strcmp(proc->usage, "Print") && !(flags & PDF_ANNOT_IS_PRINT))
			return;
		if (!strcmp(proc->usage, "View") && (flags & PDF_ANNOT_IS_NO_VIEW))
			return;
	}

	/* TODO: NoZoom and NoRotate */

	/* XXX what resources, if any, to use for this check? */
	if (pdf_is_hidden_ocg(ctx, doc->ocg, NULL, proc->usage, pdf_dict_get(ctx, annot->obj, PDF_NAME(OC))))
		return;

	if (proc->op_q && proc->op_cm && proc->op_Do_form && proc->op_Q && annot->ap)
	{
		fz_matrix matrix = pdf_annot_transform(ctx, annot);
		proc->op_q(ctx, proc);
		proc->op_cm(ctx, proc,
			matrix.a, matrix.b,
			matrix.c, matrix.d,
			matrix.e, matrix.f);
		proc->op_Do_form(ctx, proc, NULL, annot->ap, pdf_page_resources(ctx, page));
		proc->op_Q(ctx, proc);
	}
}

void
pdf_process_glyph(fz_context *ctx, pdf_processor *proc, pdf_document *doc, pdf_obj *rdb, fz_buffer *contents)
{
	pdf_csi csi;
	pdf_lexbuf buf;
	fz_stream *stm = NULL;

	fz_var(stm);

	if (!contents)
		return;

	pdf_lexbuf_init(ctx, &buf, PDF_LEXBUF_SMALL);
	pdf_init_csi(ctx, &csi, doc, rdb, &buf, NULL);

	fz_try(ctx)
	{
		stm = fz_open_buffer(ctx, contents);
		pdf_process_stream(ctx, proc, &csi, stm);
		pdf_process_end(ctx, proc, &csi);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, stm);
		pdf_clear_stack(ctx, &csi);
		pdf_lexbuf_fin(ctx, &buf);
	}
	fz_catch(ctx)
	{
		/* Note: Any SYNTAX errors should have been swallowed
		 * by pdf_process_stream, but in case any escape from other
		 * functions, recast the error type here to be safe. */
		if (fz_caught(ctx) == FZ_ERROR_SYNTAX)
			fz_throw(ctx, FZ_ERROR_GENERIC, "syntax error in content stream");
		fz_rethrow(ctx);
	}
}

void
pdf_tos_save(fz_context *ctx, pdf_text_object_state *tos, fz_matrix save[2])
{
	save[0] = tos->tm;
	save[1] = tos->tlm;
}

void
pdf_tos_restore(fz_context *ctx, pdf_text_object_state *tos, fz_matrix save[2])
{
	tos->tm = save[0];
	tos->tlm = save[1];
}

fz_text *
pdf_tos_get_text(fz_context *ctx, pdf_text_object_state *tos)
{
	fz_text *text = tos->text;

	tos->text = NULL;

	return text;
}

void
pdf_tos_reset(fz_context *ctx, pdf_text_object_state *tos, int render)
{
	tos->text = fz_new_text(ctx);
	tos->text_mode = render;
	tos->text_bbox = fz_empty_rect;
}

int
pdf_tos_make_trm(fz_context *ctx, pdf_text_object_state *tos, pdf_text_state *text, pdf_font_desc *fontdesc, int cid, fz_matrix *trm)
{
	fz_matrix tsm;

	tsm.a = text->size * text->scale;
	tsm.b = 0;
	tsm.c = 0;
	tsm.d = text->size;
	tsm.e = 0;
	tsm.f = text->rise;

	if (fontdesc->wmode == 0)
	{
		pdf_hmtx h = pdf_lookup_hmtx(ctx, fontdesc, cid);
		float w0 = h.w * 0.001f;
		tos->char_tx = (w0 * text->size + text->char_space) * text->scale;
		tos->char_ty = 0;
	}

	if (fontdesc->wmode == 1)
	{
		pdf_vmtx v = pdf_lookup_vmtx(ctx, fontdesc, cid);
		float w1 = v.w * 0.001f;
		tsm.e -= v.x * fabsf(text->size) * 0.001f;
		tsm.f -= v.y * text->size * 0.001f;
		tos->char_tx = 0;
		tos->char_ty = w1 * text->size + text->char_space;
	}

	*trm = fz_concat(tsm, tos->tm);

	tos->cid = cid;
	tos->gid = pdf_font_cid_to_gid(ctx, fontdesc, cid);
	tos->fontdesc = fontdesc;

	/* Compensate for the glyph cache limited positioning precision */
	tos->char_bbox = fz_expand_rect(fz_bound_glyph(ctx, fontdesc->font, tos->gid, *trm), 1);

	return tos->gid;
}

void
pdf_tos_move_after_char(fz_context *ctx, pdf_text_object_state *tos)
{
	tos->text_bbox = fz_union_rect(tos->text_bbox, tos->char_bbox);
	tos->tm = fz_pre_translate(tos->tm, tos->char_tx, tos->char_ty);
}

void
pdf_tos_translate(pdf_text_object_state *tos, float tx, float ty)
{
	tos->tlm = fz_pre_translate(tos->tlm, tx, ty);
	tos->tm = tos->tlm;
}

void
pdf_tos_set_matrix(pdf_text_object_state *tos, float a, float b, float c, float d, float e, float f)
{
	tos->tm.a = a;
	tos->tm.b = b;
	tos->tm.c = c;
	tos->tm.d = d;
	tos->tm.e = e;
	tos->tm.f = f;
	tos->tlm = tos->tm;
}

void
pdf_tos_newline(pdf_text_object_state *tos, float leading)
{
	tos->tlm = fz_pre_translate(tos->tlm, 0, -leading);
	tos->tm = tos->tlm;
}
