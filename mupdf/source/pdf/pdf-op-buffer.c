#include "pdf-interpret-imp.h"

typedef struct pdf_buffer_state_s
{
	fz_context *ctx;
	fz_buffer *buffer;
	fz_output *out;
}
pdf_buffer_state;

static void
put_hexstring(pdf_csi *csi, fz_output *out)
{
	int i;

	fz_printf(out, "<");
	for (i = 0; i < csi->string_len; i++)
		fz_printf(out, "%02x", csi->string[i]);
	fz_printf(out, ">");
}

static void
put_string(pdf_csi *csi, fz_output *out)
{
	int i;

	for (i=0; i < csi->string_len; i++)
		if (csi->string[i] < 32 || csi->string[i] >= 127)
			break;
	if (i < csi->string_len)
		put_hexstring(csi, out);
	else
	{
		fz_printf(out, "(");
		for (i = 0; i < csi->string_len; i++)
		{
			char c = csi->string[i];
			switch (c)
			{
			case '(':
				fz_printf(out, "\\(");
				break;
			case ')':
				fz_printf(out, "\\)");
				break;
			case '\\':
				fz_printf(out, "\\\\");
				break;
			default:
				fz_printf(out, "%c", csi->string[i]);
				break;
			}
		}
		fz_printf(out, ")");
	}
}

static void
put_string_or_obj(pdf_csi *csi, fz_output *out)
{
	if (csi->string_len)
		put_string(csi, out);
	else
		pdf_output_obj(out, csi->obj, 1);
}

static void
pdf_buffer_dquote(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f ", csi->stack[0], csi->stack[1]);
	put_string_or_obj(csi, state->out);
	fz_printf(state->out, " \"\n");
}

static void
pdf_buffer_squote(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	put_string_or_obj(csi, state->out);
	fz_printf(state->out, " \'\n");
}

static void
pdf_buffer_B(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "B\n");
}

static void
pdf_buffer_Bstar(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "B*\n");
}

static void
pdf_buffer_BDC(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "/%s ", csi->name);
	pdf_output_obj(state->out, csi->obj, 1);
	fz_printf(state->out, " BDC\n");
}

static void
pdf_buffer_BI(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;
	int len, i;
	unsigned char *data;
	fz_compressed_buffer *cbuf;
	fz_buffer *buffer;
	const char *match;
	const char *match2;
	pdf_obj *filter;
	fz_context *ctx = csi->doc->ctx;

	if (csi->img == NULL)
		return;
	cbuf = csi->img->buffer;
	if (cbuf == NULL)
		return;
	buffer = cbuf->buffer;
	if (buffer == NULL)
		return;

	/* Tweak the /Filter entry in csi->obj to match the buffer params */
	switch (cbuf->params.type)
	{
	case FZ_IMAGE_JPEG:
		match = "DCTDecode";
		match2 = "DCT";
		break;
	case FZ_IMAGE_FAX:
		match = "CCITTFaxDecode";
		match2 = "CCF";
		break;
	case FZ_IMAGE_RAW:
		match = NULL;
		match2 = NULL;
		break;
	case FZ_IMAGE_RLD:
		match = "RunLengthDecode";
		match2 = "RL";
		break;
	case FZ_IMAGE_FLATE:
		match = "FlateDecode";
		match2 = "Fl";
		break;
	case FZ_IMAGE_LZW:
		match = "LZWDecode";
		match2 = "LZW";
		break;
	default:
		fz_warn(ctx, "Unsupported type (%d) of inline image", cbuf->params.type);
		return;
	}

	filter = pdf_dict_gets(csi->obj, "Filter");
	if (filter == NULL)
		filter = pdf_dict_gets(csi->obj, "F");
	if (match == NULL)
	{
		/* Remove any filter entry (e.g. Ascii85Decode) */
		if (filter)
		{
			pdf_dict_dels(csi->obj, "Filter");
			pdf_dict_dels(csi->obj, "F");
		}
		pdf_dict_dels(csi->obj, "DecodeParms");
		pdf_dict_dels(csi->obj, "DP");
	}
	else if (pdf_is_array(filter))
	{
		int l = pdf_array_len(filter);
		pdf_obj *o = (l == 0 ? NULL : pdf_array_get(filter, l-1));
		const char *fil = pdf_to_name(o);

		if (l == 0 || (strcmp(fil, match) && strcmp(fil, match2)))
		{
			fz_warn(ctx, "Unexpected Filter configuration in inline image");
			return;
		}
		pdf_dict_puts(csi->obj, "F", o);

		o = pdf_dict_gets(csi->obj, "DecodeParms");
		if (o == NULL)
			o = pdf_dict_gets(csi->obj, "DP");
		if (o)
		{
			o = pdf_array_get(o, l-1);
			if (o)
				pdf_dict_puts(csi->obj, "DP", o);
			else
				pdf_dict_dels(csi->obj, "DP");
			pdf_dict_dels(csi->obj, "DecodeParms");
		}
	}
	else
	{
		/* It's a singleton. It must be correct */
	}

	fz_printf(state->out, "BI\n");

	len = pdf_dict_len(csi->obj);
	for (i = 0; i < len; i++)
	{
		pdf_output_obj(state->out, pdf_dict_get_key(csi->obj, i), 1);
		pdf_output_obj(state->out, pdf_dict_get_val(csi->obj, i), 1);
	}
	fz_printf(state->out, "ID\n");

	buffer = csi->img->buffer->buffer;
	len = buffer->len;
	data = buffer->data;
	for (i = 0; i < len; i++)
	{
		fz_printf(state->out, "%c", data[i]);
	}

	fz_printf(state->out, "\nEI\n");
}

static void
pdf_buffer_BMC(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "/%s BMC\n", csi->name);
}

static void
pdf_buffer_BT(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "BT\n");
}

static void
pdf_buffer_BX(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "BX\n");
}

static void
pdf_buffer_CS(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "/%s CS\n", csi->name);
}

static void
pdf_buffer_DP(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "/%s ", csi->name);
	pdf_output_obj(state->out, csi->obj, 1);
	fz_printf(state->out, " DP\n");
}

static void
pdf_buffer_EMC(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "EMC\n");
}

static void
pdf_buffer_ET(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "ET\n");
}

static void
pdf_buffer_EX(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "EX\n");
}

static void
pdf_buffer_F(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "F\n");
}

static void
pdf_buffer_G(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f G\n", csi->stack[0]);
}

static void
pdf_buffer_J(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%d J\n", (int)csi->stack[0]);
}

static void
pdf_buffer_K(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f %f %f K\n", csi->stack[0],
		csi->stack[1], csi->stack[2], csi->stack[3]);
}

static void
pdf_buffer_M(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f M\n", csi->stack[0]);
}

static void
pdf_buffer_MP(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "/%s MP\n", csi->name);
}

static void
pdf_buffer_Q(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "Q\n");
}

static void
pdf_buffer_RG(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f %f RG\n", csi->stack[0], csi->stack[1], csi->stack[2]);
}

static void
pdf_buffer_S(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "S\n");
}

static void
pdf_buffer_SC(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;
	int i;

	for (i = 0; i < csi->top; i++)
		fz_printf(state->out, "%f ", csi->stack[i]);
	fz_printf(state->out, "SC\n");
}

static void
pdf_buffer_SCN(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;
	int i;

	for (i = 0; i < csi->top; i++)
		fz_printf(state->out, "%f ", csi->stack[i]);
	if (csi->name[0])
		fz_printf(state->out, "/%s ", csi->name);
	fz_printf(state->out, "SCN\n");
}

static void
pdf_buffer_Tstar(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "T*\n");
}

static void
pdf_buffer_TD(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f TD\n", csi->stack[0], csi->stack[1]);
}

static void
pdf_buffer_TJ(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	pdf_output_obj(state->out, csi->obj, 1);
	fz_printf(state->out, " TJ\n");
}

static void
pdf_buffer_TL(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f TL\n", csi->stack[0]);
}

static void
pdf_buffer_Tc(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f Tc\n", csi->stack[0]);
}

static void
pdf_buffer_Td(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f Td\n", csi->stack[0], csi->stack[1]);
}

static void
pdf_buffer_Tj(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	put_string_or_obj(csi, state->out);
	fz_printf(state->out, " Tj\n");
}

static void
pdf_buffer_Tm(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f %f %f %f %f Tm\n",
		csi->stack[0], csi->stack[1], csi->stack[2],
		csi->stack[3], csi->stack[4], csi->stack[5]);
}

static void
pdf_buffer_Tr(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f Tr\n", csi->stack[0]);
}

static void
pdf_buffer_Ts(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f Ts\n", csi->stack[0]);
}

static void
pdf_buffer_Tw(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f Tw\n", csi->stack[0]);
}

static void
pdf_buffer_Tz(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f Tz\n", csi->stack[0]);
}

static void
pdf_buffer_W(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "W\n");
}

static void
pdf_buffer_Wstar(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "W*\n");
}

static void
pdf_buffer_b(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "b\n");
}

static void
pdf_buffer_bstar(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "b*\n");
}

static void
pdf_buffer_c(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f %f %f %f %f c\n",
		csi->stack[0], csi->stack[1], csi->stack[2],
		csi->stack[3], csi->stack[4], csi->stack[5]);
}

static void
pdf_buffer_cm(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f %f %f %f %f cm\n",
		csi->stack[0], csi->stack[1], csi->stack[2],
		csi->stack[3], csi->stack[4], csi->stack[5]);
}

static void
pdf_buffer_cs(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "/%s cs\n", csi->name);
}

static void
pdf_buffer_d(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	pdf_output_obj(state->out, csi->obj, 1);
	fz_printf(state->out, " %f d\n", csi->stack[0]);
}

static void
pdf_buffer_d0(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f d0\n", csi->stack[0], csi->stack[1]);
}

static void
pdf_buffer_d1(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f %f %f %f %f d1\n",
		csi->stack[0], csi->stack[1], csi->stack[2],
		csi->stack[3], csi->stack[4], csi->stack[5]);
}

static void
pdf_buffer_f(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "f\n");
}

static void
pdf_buffer_fstar(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "f*\n");
}

static void
pdf_buffer_g(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f g\n", csi->stack[0]);
}

static void
pdf_buffer_h(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "h\n");
}

static void
pdf_buffer_i(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f i\n", csi->stack[0]);
}

static void
pdf_buffer_j(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%d j\n", (int)csi->stack[0]);
}

static void
pdf_buffer_k(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f %f %f k\n", csi->stack[0],
		csi->stack[1], csi->stack[2], csi->stack[3]);
}

static void
pdf_buffer_l(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f l\n", csi->stack[0], csi->stack[1]);
}

static void
pdf_buffer_m(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f m\n", csi->stack[0], csi->stack[1]);
}

static void
pdf_buffer_n(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "n\n");
}

static void
pdf_buffer_q(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "q\n");
}

static void
pdf_buffer_re(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f %f %f re\n", csi->stack[0],
		csi->stack[1], csi->stack[2], csi->stack[3]);
}

static void
pdf_buffer_rg(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f %f rg\n",
		csi->stack[0], csi->stack[1], csi->stack[2]);
}

static void
pdf_buffer_ri(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "/%s ri\n", csi->name);
}

static void
pdf_buffer_s(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "s\n");
}

static void
pdf_buffer_sc(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;
	int i;

	for (i = 0; i < csi->top; i++)
		fz_printf(state->out, "%f ", csi->stack[i]);
	fz_printf(state->out, "sc\n");
}

static void
pdf_buffer_scn(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;
	int i;

	for (i = 0; i < csi->top; i++)
		fz_printf(state->out, "%f ", csi->stack[i]);
	if (csi->name[0])
		fz_printf(state->out, "/%s ", csi->name);
	fz_printf(state->out, "scn\n");
}

static void
pdf_buffer_v(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f %f %f v\n", csi->stack[0],
		csi->stack[1], csi->stack[2], csi->stack[3]);
}

static void
pdf_buffer_w(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f w\n", csi->stack[0]);
}

static void
pdf_buffer_y(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "%f %f %f %f y\n", csi->stack[0],
		csi->stack[1], csi->stack[2], csi->stack[3]);
}

static void
pdf_buffer_Do(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "/%s Do\n", csi->name);
}

static void
pdf_buffer_Tf(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "/%s %f Tf\n", csi->name, csi->stack[0]);
}

static void
pdf_buffer_gs(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "/%s gs\n", csi->name);
}

static void
pdf_buffer_sh(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;

	fz_printf(state->out, "/%s sh\n", csi->name);
}

static void
free_processor_buffer(pdf_csi *csi, void *state_)
{
	pdf_buffer_state *state = (pdf_buffer_state *)state_;
	fz_context *ctx = state->ctx;

	fz_close_output(state->out);
	fz_free(ctx, state);
}

static void
process_annot(pdf_csi *csi, void *state, pdf_obj *resources, pdf_annot *annot)
{
	fz_context *ctx = csi->doc->ctx;
	pdf_xobject *xobj = annot->ap;

	/* Avoid infinite recursion */
	if (xobj == NULL || pdf_mark_obj(xobj->me))
		return;

	fz_try(ctx)
	{
		if (xobj->resources)
			resources = xobj->resources;

		pdf_process_contents_object(csi, resources, xobj->contents);
	}
	fz_always(ctx)
	{
		pdf_unmark_obj(xobj->me);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
process_stream(pdf_csi *csi, void *state, pdf_lexbuf *buf)
{
	pdf_process_stream(csi, buf);
}

static void
process_contents(pdf_csi *csi, void *state, pdf_obj *resources, pdf_obj *contents)
{
	pdf_process_contents_object(csi, resources, contents);
}

static const pdf_processor pdf_processor_buffer =
{
	{
	pdf_buffer_dquote,
	pdf_buffer_squote,
	pdf_buffer_B,
	pdf_buffer_Bstar,
	pdf_buffer_BDC,
	pdf_buffer_BI,
	pdf_buffer_BMC,
	pdf_buffer_BT,
	pdf_buffer_BX,
	pdf_buffer_CS,
	pdf_buffer_DP,
	pdf_buffer_EMC,
	pdf_buffer_ET,
	pdf_buffer_EX,
	pdf_buffer_F,
	pdf_buffer_G,
	pdf_buffer_J,
	pdf_buffer_K,
	pdf_buffer_M,
	pdf_buffer_MP,
	pdf_buffer_Q,
	pdf_buffer_RG,
	pdf_buffer_S,
	pdf_buffer_SC,
	pdf_buffer_SCN,
	pdf_buffer_Tstar,
	pdf_buffer_TD,
	pdf_buffer_TJ,
	pdf_buffer_TL,
	pdf_buffer_Tc,
	pdf_buffer_Td,
	pdf_buffer_Tj,
	pdf_buffer_Tm,
	pdf_buffer_Tr,
	pdf_buffer_Ts,
	pdf_buffer_Tw,
	pdf_buffer_Tz,
	pdf_buffer_W,
	pdf_buffer_Wstar,
	pdf_buffer_b,
	pdf_buffer_bstar,
	pdf_buffer_c,
	pdf_buffer_cm,
	pdf_buffer_cs,
	pdf_buffer_d,
	pdf_buffer_d0,
	pdf_buffer_d1,
	pdf_buffer_f,
	pdf_buffer_fstar,
	pdf_buffer_g,
	pdf_buffer_h,
	pdf_buffer_i,
	pdf_buffer_j,
	pdf_buffer_k,
	pdf_buffer_l,
	pdf_buffer_m,
	pdf_buffer_n,
	pdf_buffer_q,
	pdf_buffer_re,
	pdf_buffer_rg,
	pdf_buffer_ri,
	pdf_buffer_s,
	pdf_buffer_sc,
	pdf_buffer_scn,
	pdf_buffer_v,
	pdf_buffer_w,
	pdf_buffer_y,
	pdf_buffer_Do,
	pdf_buffer_Tf,
	pdf_buffer_gs,
	pdf_buffer_sh,
	free_processor_buffer
	},
	process_annot,
	process_stream,
	process_contents
};

pdf_process *pdf_process_buffer(pdf_process *process, fz_context *ctx, fz_buffer *buffer)
{
	fz_output *out = fz_new_output_with_buffer(ctx, buffer);
	pdf_buffer_state *p = NULL;

	fz_var(p);

	fz_try(ctx)
	{
		p = fz_malloc_struct(ctx, pdf_buffer_state);
		p->buffer = buffer;
		p->out = out;
		p->ctx = ctx;
	}
	fz_catch(ctx)
	{
		fz_close_output(out);
		fz_rethrow(ctx);
	}

	process->state = p;
	process->processor = &pdf_processor_buffer;
	return process;
}
