#include "pdf-interpret-imp.h"

typedef struct filter_gstate_s filter_gstate;

typedef enum
{
	FLUSH_CTM = 1,
	FLUSH_COLOR = 2,
	FLUSH_COLOR_S = 4,

	FLUSH_ALL = 7,
	FLUSH_STROKE = 1+4,
	FLUSH_FILL = 1+2
} gstate_flush_flags;

struct filter_gstate_s
{
	filter_gstate *next;
	int pushed;
	fz_matrix ctm;
	fz_matrix current_ctm;
	float color[FZ_MAX_COLORS];
	int color_n;
	float current_color[FZ_MAX_COLORS];
	int current_color_n;
	float color_s[FZ_MAX_COLORS];
	int color_s_n;
	float current_color_s[FZ_MAX_COLORS];
	int current_color_s_n;
	char cs[256];
	char cs_s[256];
	char cs_name[256];
	char cs_name_s[256];
	char current_cs[256];
	char current_cs_s[256];
	char current_cs_name[256];
	char current_cs_name_s[256];
};

typedef struct pdf_filter_state_s
{
	pdf_process process;
	fz_context *ctx;
	filter_gstate *gstate;
	pdf_obj *resources;
} pdf_filter_state;

static void insert_resource_name(pdf_csi *csi, pdf_filter_state *state, const char *key, const char *name)
{
	pdf_obj *xobj;
	pdf_obj *obj;

	if (!state->resources || !name || name[0] == 0)
		return;

	xobj = pdf_dict_gets(csi->rdb, key);
	obj = pdf_dict_gets(xobj, name);

	xobj = pdf_dict_gets(state->resources, key);
	if (xobj == NULL) {
		xobj = pdf_new_dict(csi->doc, 1);
		pdf_dict_puts_drop(state->resources, key, xobj);
	}
	pdf_dict_putp(xobj, name, obj);
}

static void insert_resource(pdf_csi *csi, pdf_filter_state *state, const char *key)
{
	insert_resource_name(csi, state, key, csi->name);
}

static inline void call_op(pdf_csi *csi, pdf_filter_state *state, int op)
{
	pdf_process_op(csi, op, &state->process);
}

static void filter_push(pdf_csi *csi, pdf_filter_state *state)
{
	filter_gstate *gstate = state->gstate;
	filter_gstate *new_gstate = fz_malloc_struct(state->ctx, filter_gstate);

	*new_gstate = *gstate;
	new_gstate->pushed = 0;
	new_gstate->next = gstate;
	state->gstate = new_gstate;
}

static int filter_pop(pdf_csi *csi, pdf_filter_state *state)
{
	filter_gstate *gstate = state->gstate;
	filter_gstate *old = gstate->next;

	/* We are at the top, so nothing to pop! */
	if (old == NULL)
		return 1;

	if (gstate->pushed)
		call_op(csi, state, PDF_OP_Q);

	fz_free(state->ctx, gstate);
	state->gstate = old;
	return 0;
}

static void forward(pdf_csi *csi, pdf_filter_state *state, int op, float *f_argv, int f_argc, char *arg)
{
	int top = csi->top;
	int to_save = top;
	float save_f[FZ_MAX_COLORS];
	char save_name[sizeof(csi->name)];
	int i;

	/* Store the stack */
	if (to_save > f_argc)
		to_save = 6;
	for (i = 0; i < to_save; i++)
	{
		save_f[i] = csi->stack[i];
		csi->stack[i] = f_argv[i];
	}
	for (;i < f_argc; i++)
	{
		csi->stack[i] = f_argv[i];
	}
	csi->top = f_argc;

	/* Store the name */
	fz_strlcpy(save_name, csi->name, sizeof(csi->name));
	if (arg)
	{
		fz_strlcpy(csi->name, arg, sizeof(csi->name));
	}
	else
	{
		csi->name[0] = 0;
	}

	call_op(csi, state, op);

	/* Restore the name */
	fz_strlcpy(csi->name, save_name, sizeof(save_name));

	/* Restore the stack */
	for (i = 0; i < to_save; i++)
		csi->stack[i] = save_f[i];
	csi->top = top;
}

/* We never allow the topmost gstate to be changed. This allows us
 * to pop back to the zeroth level and be sure that our gstate is
 * sane. This is important for being able to add new operators at
 * the end of pages in a sane way. */
static filter_gstate *
gstate_to_update(pdf_csi *csi, pdf_filter_state *state)
{
	filter_gstate *gstate = state->gstate;

	/* If we're not the top, that's fine */
	if (gstate->next != NULL)
		return gstate;

	/* We are the top. Push a group, so we're not */
	filter_push(csi, state);
	gstate = state->gstate;
	gstate->pushed = 1;
	call_op(csi, state, PDF_OP_q);

	return state->gstate;
}

static void filter_flush(pdf_csi *csi, pdf_filter_state *state, int flush)
{
	filter_gstate *gstate = state->gstate;
	int i;

	if (gstate->pushed == 0)
	{
		gstate->pushed = 1;
		call_op(csi, state, PDF_OP_q);
	}

	if (flush & FLUSH_CTM)
	{
		if (gstate->ctm.a != 1 || gstate->ctm.b != 0 ||
			gstate->ctm.c != 0 || gstate->ctm.d != 1 ||
			gstate->ctm.e != 0 || gstate->ctm.f != 0)
		{
			fz_matrix current = gstate->current_ctm;

			forward(csi, state, PDF_OP_cm, (float *)&gstate->ctm.a, 6, NULL);
			fz_concat(&gstate->current_ctm, &current, &gstate->ctm);
			gstate->ctm.a = 1;
			gstate->ctm.b = 0;
			gstate->ctm.c = 0;
			gstate->ctm.d = 1;
			gstate->ctm.e = 0;
			gstate->ctm.f = 0;
		}
	}
	if (flush & FLUSH_COLOR)
	{
		if (strcmp(gstate->cs, gstate->current_cs) ||
			gstate->color_n != gstate->current_color_n)
		{
			/* Colorspace has changed. Send both colorspace (and
			 * color if we have it. */
			if (!strcmp(gstate->cs, "DeviceRGB"))
			{
				forward(csi, state, PDF_OP_rg, gstate->color, 3, NULL);
			}
			else if (!strcmp(gstate->cs, "DeviceGray"))
			{
				forward(csi, state, PDF_OP_g, gstate->color, 1, NULL);
			}
			else if (!strcmp(gstate->cs, "DeviceCMYK"))
			{
				forward(csi, state, PDF_OP_k, gstate->color, 4, NULL);
			}
			else if (gstate->cs_name[0])
			{
				if (strcmp(gstate->cs, gstate->current_cs))
				{
					forward(csi, state, PDF_OP_cs, NULL, 0, gstate->cs);
				}
				forward(csi, state, PDF_OP_scn, gstate->color, gstate->color_n, gstate->cs_name);
			}
			else if (gstate->color_n > 0)
			{
				if (strcmp(gstate->cs, gstate->current_cs))
				{
					forward(csi, state, PDF_OP_cs, NULL, 0, gstate->cs);
				}
				forward(csi, state, PDF_OP_scn, gstate->color, gstate->color_n, NULL);
			}
			else
			{
				forward(csi, state, PDF_OP_cs, NULL, 0, gstate->cs);
			}
			strcpy(gstate->current_cs, gstate->cs);
			strcpy(gstate->current_cs_name, gstate->cs_name);
			gstate->current_color_n = gstate->color_n;
			for (i = 0; i < gstate->color_n; i++)
				gstate->current_color[i] = gstate->color[i];
		}
		else if (strcmp(gstate->cs_name, gstate->current_cs_name))
		{
			/* Pattern name has changed */
			forward(csi, state, PDF_OP_scn, gstate->color, gstate->color_n, gstate->cs_name);
			strcpy(gstate->current_cs_name, gstate->cs_name);
		}
		else
		{
			/* Has the color changed? */
			for (i = 0; i < gstate->color_n; i++)
			{
				if (gstate->color[i] != gstate->current_color[i])
					break;
			}
			if (i == gstate->color_n)
			{
				/* The color has not changed. Do nothing. */
			}
			else if (!strcmp(gstate->cs, "DeviceRGB"))
			{
				forward(csi, state, PDF_OP_rg, gstate->color, 3, NULL);
			}
			else if (!strcmp(gstate->cs, "DeviceGray"))
			{
				forward(csi, state, PDF_OP_g, gstate->color, 1, NULL);
			}
			else if (!strcmp(gstate->cs, "DeviceCMYK"))
			{
				forward(csi, state, PDF_OP_k, gstate->color, 4, NULL);
			}
			else
			{
				forward(csi, state, PDF_OP_scn, gstate->color, gstate->color_n, NULL);
			}
			for (; i < gstate->color_n; i++)
				gstate->current_color[i] = gstate->color[i];
		}
	}
	if (flush & FLUSH_COLOR_S)
	{
		if (strcmp(gstate->cs_s, gstate->current_cs_s) ||
			gstate->color_s_n != gstate->current_color_s_n)
		{
			/* Colorspace has changed. Send both colorspace (and
			 * color if we have it. */
			if (!strcmp(gstate->cs_s, "DeviceRGB"))
			{
				forward(csi, state, PDF_OP_RG, gstate->color_s, 3, NULL);
			}
			else if (!strcmp(gstate->cs_s, "DeviceGray"))
			{
				forward(csi, state, PDF_OP_G, gstate->color_s, 1, NULL);
			}
			else if (!strcmp(gstate->cs_s, "DeviceCMYK"))
			{
				forward(csi, state, PDF_OP_K, gstate->color_s, 4, NULL);
			}
			else if (gstate->cs_name_s[0])
			{
				if (strcmp(gstate->cs_s, gstate->current_cs_s))
				{
					forward(csi, state, PDF_OP_CS, NULL, 0, gstate->cs_s);
				}
				forward(csi, state, PDF_OP_SCN, gstate->color_s, gstate->color_s_n, gstate->cs_name_s);
			}
			else if (gstate->color_s_n > 0)
			{
				if (strcmp(gstate->cs_s, gstate->current_cs_s))
				{
					forward(csi, state, PDF_OP_CS, NULL, 0, gstate->cs_s);
				}
				forward(csi, state, PDF_OP_SCN, gstate->color_s, gstate->color_s_n, NULL);
			}
			else
			{
				forward(csi, state, PDF_OP_CS, NULL, 0, gstate->cs_s);
			}
			strcpy(gstate->current_cs_s, gstate->cs_s);
			strcpy(gstate->current_cs_name_s, gstate->cs_name_s);
			gstate->current_color_s_n = gstate->color_s_n;
			for (i = 0; i < gstate->color_s_n; i++)
				gstate->current_color_s[i] = gstate->color_s[i];
		}
		else if (strcmp(gstate->cs_name_s, gstate->current_cs_name_s))
		{
			/* Pattern name has changed */
			forward(csi, state, PDF_OP_SCN, gstate->color_s, gstate->color_s_n, gstate->cs_name_s);
			strcpy(gstate->current_cs_name_s, gstate->cs_name_s);
		}
		else
		{
			/* Has the color changed? */
			int i;

			for (i = 0; i < gstate->color_s_n; i++)
			{
				if (gstate->color_s[i] != gstate->current_color_s[i])
					break;
			}
			if (i == gstate->color_s_n)
			{
				/* The color has not changed. Do nothing. */
			}
			else if (!strcmp(gstate->cs_s, "DeviceRGB"))
			{
				forward(csi, state, PDF_OP_RG, gstate->color_s, 3, NULL);
			}
			else if (!strcmp(gstate->cs_s, "DeviceGray"))
			{
				forward(csi, state, PDF_OP_G, gstate->color_s, 1, NULL);
			}
			else if (!strcmp(gstate->cs_s, "DeviceCMYK"))
			{
				forward(csi, state, PDF_OP_K, gstate->color_s, 4, NULL);
			}
			else
			{
				forward(csi, state, PDF_OP_SCN, gstate->color_s, gstate->color_s_n, NULL);
			}
			for (; i < gstate->color_s_n; i++)
				gstate->current_color_s[i] = gstate->color_s[i];
		}
	}
}

static void
pdf_filter_dquote(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_ALL);
	call_op(csi, state, PDF_OP_dquote);
}

static void
pdf_filter_squote(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_ALL);
	call_op(csi, state, PDF_OP_squote);
}

static void
pdf_filter_B(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_ALL);
	call_op(csi, state, PDF_OP_B);
}

static void
pdf_filter_Bstar(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_ALL);
	call_op(csi, state, PDF_OP_Bstar);
}

static void
pdf_filter_BDC(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	insert_resource_name(csi, state, "Properties", pdf_to_name(csi->obj));

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_BDC);
}

static void
pdf_filter_BI(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_FILL);
	call_op(csi, state, PDF_OP_BI);
}

static void
pdf_filter_BMC(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_BMC);
}

static void
pdf_filter_BT(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_BT);
}

static void
pdf_filter_BX(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_BX);
}

static void
pdf_filter_CS(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;
	filter_gstate *gstate = gstate_to_update(csi, state);

	insert_resource(csi, state, "ColorSpace");

	fz_strlcpy(gstate->cs_s, csi->name, sizeof(gstate->cs_s));
	gstate->current_color_s_n = 0;
}

static void
pdf_filter_DP(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	insert_resource_name(csi, state, "Properties", pdf_to_name(csi->obj));

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_DP);
}

static void
pdf_filter_EMC(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_EMC);
}

static void
pdf_filter_ET(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_ET);
}

static void
pdf_filter_EX(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_EX);
}

static void
pdf_filter_F(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_FILL);
	call_op(csi, state, PDF_OP_F);
}

static void
pdf_filter_G(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;
	filter_gstate *gstate = gstate_to_update(csi, state);

	strcpy(gstate->cs_s, "DeviceGray");
	strcpy(gstate->cs_name_s, "");
	gstate->color_s[0] = csi->stack[0];
	gstate->color_s_n = 1;
}

static void
pdf_filter_J(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_J);
}

static void
pdf_filter_K(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;
	filter_gstate *gstate = gstate_to_update(csi, state);

	strcpy(gstate->cs_s, "DeviceCMYK");
	strcpy(gstate->cs_name_s, "");
	gstate->color_s[0] = csi->stack[0];
	gstate->color_s[1] = csi->stack[1];
	gstate->color_s[2] = csi->stack[2];
	gstate->color_s[3] = csi->stack[3];
	gstate->color_s_n = 4;
}

static void
pdf_filter_M(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_M);
}

static void
pdf_filter_MP(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_MP);
}

static void
pdf_filter_Q(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_pop(csi, state);
}

static void
pdf_filter_RG(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;
	filter_gstate *gstate = gstate_to_update(csi, state);

	strcpy(gstate->cs_s, "DeviceRGB");
	strcpy(gstate->cs_name_s, "");
	gstate->color_s[0] = csi->stack[0];
	gstate->color_s[1] = csi->stack[1];
	gstate->color_s[2] = csi->stack[2];
	gstate->color_s_n = 3;
}

static void
pdf_filter_S(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_STROKE);
	call_op(csi, state, PDF_OP_S);
}

static void
pdf_filter_SCN(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;
	filter_gstate *gstate = gstate_to_update(csi, state);
	int i;

	if (csi->name[0])
		insert_resource(csi, state, "Pattern");

	fz_strlcpy(gstate->cs_name_s, csi->name, sizeof(csi->name));
	for (i = 0; i < csi->top; i++)
	{
		gstate->color_s[i] = csi->stack[i];
	}
	gstate->color_s_n = csi->top;
}

static void
pdf_filter_Tstar(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_CTM);
	call_op(csi, state, PDF_OP_Tstar);
}

static void
pdf_filter_TD(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_CTM);
	call_op(csi, state, PDF_OP_TD);
}

static void
pdf_filter_TJ(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_ALL);
	call_op(csi, state, PDF_OP_TJ);
}

static void
pdf_filter_TL(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_TL);
}

static void
pdf_filter_Tc(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_Tc);
}

static void
pdf_filter_Td(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_CTM);
	call_op(csi, state, PDF_OP_Td);
}

static void
pdf_filter_Tj(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_ALL);
	call_op(csi, state, PDF_OP_Tj);
}

static void
pdf_filter_Tm(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_CTM);
	call_op(csi, state, PDF_OP_Tm);
}

static void
pdf_filter_Tr(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_Tr);
}

static void
pdf_filter_Ts(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_Ts);
}

static void
pdf_filter_Tw(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_Tw);
}

static void
pdf_filter_Tz(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_Tz);
}

static void
pdf_filter_W(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_CTM);
	call_op(csi, state, PDF_OP_W);
}

static void
pdf_filter_Wstar(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_CTM);
	call_op(csi, state, PDF_OP_Wstar);
}

static void
pdf_filter_b(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_ALL);
	call_op(csi, state, PDF_OP_b);
}

static void
pdf_filter_bstar(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_ALL);
	call_op(csi, state, PDF_OP_bstar);
}

static void
pdf_filter_c(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_CTM);
	call_op(csi, state, PDF_OP_c);
}

static void
pdf_filter_cm(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;
	filter_gstate *gstate = gstate_to_update(csi, state);
	fz_matrix old, ctm;

	ctm.a = (csi->top > 0 ? csi->stack[0] : 0.0);
	ctm.b = (csi->top > 1 ? csi->stack[1] : 0.0);
	ctm.c = (csi->top > 2 ? csi->stack[2] : 0.0);
	ctm.d = (csi->top > 3 ? csi->stack[3] : 0.0);
	ctm.e = (csi->top > 4 ? csi->stack[4] : 0.0);
	ctm.f = (csi->top > 5 ? csi->stack[5] : 0.0);

	/* If we're being given an identity matrix, don't bother sending it */
	if (ctm.a == 1 && ctm.b == 0 && ctm.c == 0 && ctm.d == 1 &&
		ctm.e == 0.0f && ctm.f == 0)
		return;

	old = gstate->ctm;
	fz_concat(&gstate->ctm, &ctm, &old);
}

static void
pdf_filter_cs(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;
	filter_gstate *gstate = gstate_to_update(csi, state);

	insert_resource(csi, state, "ColorSpace");

	fz_strlcpy(gstate->cs, csi->name, sizeof(csi->name));
	gstate->color_n = 0;
}

static void
pdf_filter_d(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_d);
}

static void
pdf_filter_d0(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	call_op(csi, state, PDF_OP_d0);
}

static void
pdf_filter_d1(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	call_op(csi, state, PDF_OP_d1);
}

static void
pdf_filter_f(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_FILL);
	call_op(csi, state, PDF_OP_f);
}

static void
pdf_filter_fstar(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_FILL);
	call_op(csi, state, PDF_OP_fstar);
}

static void
pdf_filter_g(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;
	filter_gstate *gstate = gstate_to_update(csi, state);

	strcpy(gstate->cs, "DeviceGray");
	strcpy(gstate->cs_name, "");
	gstate->color[0] = csi->stack[0];
	gstate->color_n = 1;
}

static void
pdf_filter_h(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_CTM);
	call_op(csi, state, PDF_OP_h);
}

static void
pdf_filter_i(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_i);
}

static void
pdf_filter_j(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_j);
}

static void
pdf_filter_k(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;
	filter_gstate *gstate = gstate_to_update(csi, state);

	strcpy(gstate->cs, "DeviceCMYK");
	strcpy(gstate->cs_name, "");
	gstate->color[0] = csi->stack[0];
	gstate->color[1] = csi->stack[1];
	gstate->color[2] = csi->stack[2];
	gstate->color[3] = csi->stack[3];
	gstate->color_n = 4;
}

static void
pdf_filter_l(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_CTM);
	call_op(csi, state, PDF_OP_l);
}

static void
pdf_filter_m(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_CTM);
	call_op(csi, state, PDF_OP_m);
}

static void
pdf_filter_n(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_CTM);
	call_op(csi, state, PDF_OP_n);
}

static void
pdf_filter_q(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_push(csi, state);
}

static void
pdf_filter_re(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_CTM);
	call_op(csi, state, PDF_OP_re);
}

static void
pdf_filter_rg(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;
	filter_gstate *gstate = gstate_to_update(csi, state);

	strcpy(gstate->cs, "DeviceRGB");
	strcpy(gstate->cs_name, "");
	gstate->color[0] = csi->stack[0];
	gstate->color[1] = csi->stack[1];
	gstate->color[2] = csi->stack[2];
	gstate->color_n = 3;
}

static void
pdf_filter_ri(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_ri);
}

static void
pdf_filter_s(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_STROKE);
	call_op(csi, state, PDF_OP_s);
}

static void
pdf_filter_scn(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;
	filter_gstate *gstate = gstate_to_update(csi, state);
	int i;

	if (csi->name[0])
		insert_resource(csi, state, "Pattern");

	fz_strlcpy(gstate->cs_name, csi->name, sizeof(csi->name));
	for (i = 0; i < csi->top; i++)
	{
		gstate->color[i] = csi->stack[i];
	}
	gstate->color_n = csi->top;
}

static void
pdf_filter_v(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_CTM);
	call_op(csi, state, PDF_OP_v);
}

static void
pdf_filter_w(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_w);
}

static void
pdf_filter_y(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	filter_flush(csi, state, FLUSH_CTM);
	call_op(csi, state, PDF_OP_y);
}

static void
pdf_filter_Do(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	insert_resource(csi, state, "XObject");

	filter_flush(csi, state, FLUSH_ALL);
	call_op(csi, state, PDF_OP_Do);
}

static void
pdf_filter_Tf(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	insert_resource(csi, state, "Font");

	filter_flush(csi, state, 0);
	call_op(csi, state, PDF_OP_Tf);
}

static void
pdf_filter_gs(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	insert_resource(csi, state, "ExtGState");

	filter_flush(csi, state, FLUSH_ALL);
	call_op(csi, state, PDF_OP_gs);
}

static void
pdf_filter_sh(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	insert_resource(csi, state, "Shading");

	filter_flush(csi, state, FLUSH_ALL);
	call_op(csi, state, PDF_OP_sh);
}

static void
free_processor_filter(pdf_csi *csi, void *state_)
{
	pdf_filter_state *state = (pdf_filter_state *)state_;

	/* csi can permissibly be NULL, but only in the case when we have
	 * failed while setting up csi. So there is nothing to pop. */
	if (csi)
	{
		while (!filter_pop(csi, state))
		{
			/* Nothing to do in the loop, all work done above */
		}
	}

	call_op(csi, state, PDF_OP_END);
	fz_free(state->ctx, state->gstate);
	fz_free(state->ctx, state);
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

static const pdf_processor pdf_processor_filter =
{
	{
	pdf_filter_dquote,
	pdf_filter_squote,
	pdf_filter_B,
	pdf_filter_Bstar,
	pdf_filter_BDC,
	pdf_filter_BI,
	pdf_filter_BMC,
	pdf_filter_BT,
	pdf_filter_BX,
	pdf_filter_CS,
	pdf_filter_DP,
	pdf_filter_EMC,
	pdf_filter_ET,
	pdf_filter_EX,
	pdf_filter_F,
	pdf_filter_G,
	pdf_filter_J,
	pdf_filter_K,
	pdf_filter_M,
	pdf_filter_MP,
	pdf_filter_Q,
	pdf_filter_RG,
	pdf_filter_S,
	pdf_filter_SCN,
	pdf_filter_SCN,
	pdf_filter_Tstar,
	pdf_filter_TD,
	pdf_filter_TJ,
	pdf_filter_TL,
	pdf_filter_Tc,
	pdf_filter_Td,
	pdf_filter_Tj,
	pdf_filter_Tm,
	pdf_filter_Tr,
	pdf_filter_Ts,
	pdf_filter_Tw,
	pdf_filter_Tz,
	pdf_filter_W,
	pdf_filter_Wstar,
	pdf_filter_b,
	pdf_filter_bstar,
	pdf_filter_c,
	pdf_filter_cm,
	pdf_filter_cs,
	pdf_filter_d,
	pdf_filter_d0,
	pdf_filter_d1,
	pdf_filter_f,
	pdf_filter_fstar,
	pdf_filter_g,
	pdf_filter_h,
	pdf_filter_i,
	pdf_filter_j,
	pdf_filter_k,
	pdf_filter_l,
	pdf_filter_m,
	pdf_filter_n,
	pdf_filter_q,
	pdf_filter_re,
	pdf_filter_rg,
	pdf_filter_ri,
	pdf_filter_s,
	pdf_filter_scn,
	pdf_filter_scn,
	pdf_filter_v,
	pdf_filter_w,
	pdf_filter_y,
	pdf_filter_Do,
	pdf_filter_Tf,
	pdf_filter_gs,
	pdf_filter_sh,
	free_processor_filter
	},
	process_annot,
	process_stream,
	process_contents
};

pdf_process *
pdf_process_filter(pdf_process *process, fz_context *ctx, pdf_process *underlying, pdf_obj *resources)
{
	pdf_filter_state *p = NULL;

	fz_var(p);

	fz_try(ctx)
	{
		p = fz_malloc_struct(ctx, pdf_filter_state);
		p->ctx = ctx;
		p->process = *underlying;
		p->gstate = fz_malloc_struct(ctx, filter_gstate);
		p->resources = resources;
		p->gstate->ctm = fz_identity;
		p->gstate->current_ctm = fz_identity;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, p);
		pdf_process_op(NULL, PDF_OP_END, underlying);
		fz_rethrow(ctx);
	}

	process->state = p;
	process->processor = &pdf_processor_filter;
	return process;
}
