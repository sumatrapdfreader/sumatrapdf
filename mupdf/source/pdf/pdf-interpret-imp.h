#ifndef PDF_INTERPRET_IMP_H
#define PDF_INTERPRET_IMP_H

#include "mupdf/pdf.h"

typedef struct pdf_csi_s pdf_csi;
typedef struct pdf_gstate_s pdf_gstate;

typedef void (*pdf_operator_fn)(pdf_csi *, void *user);
typedef void (*pdf_process_annot_fn)(pdf_csi *csi, void *user, pdf_obj *resources, pdf_annot *annot);
typedef void (*pdf_process_stream_fn)(pdf_csi *csi, void *user, pdf_lexbuf *buf);
typedef void (*pdf_process_contents_fn)(pdf_csi *csi, void *user, pdf_obj *resources, pdf_obj *contents);

typedef enum {
	/* The first section of op's all run without a try/catch */
	PDF_OP_dquote,
	PDF_OP_squote,
	PDF_OP_B,
	PDF_OP_Bstar,
	PDF_OP_BDC,
	PDF_OP_BI,
	PDF_OP_BMC,
	PDF_OP_BT,
	PDF_OP_BX,
	PDF_OP_CS,
	PDF_OP_DP,
	PDF_OP_EMC,
	PDF_OP_ET,
	PDF_OP_EX,
	PDF_OP_F,
	PDF_OP_G,
	PDF_OP_J,
	PDF_OP_K,
	PDF_OP_M,
	PDF_OP_MP,
	PDF_OP_Q,
	PDF_OP_RG,
	PDF_OP_S,
	PDF_OP_SC,
	PDF_OP_SCN,
	PDF_OP_Tstar,
	PDF_OP_TD,
	PDF_OP_TJ,
	PDF_OP_TL,
	PDF_OP_Tc,
	PDF_OP_Td,
	PDF_OP_Tj,
	PDF_OP_Tm,
	PDF_OP_Tr,
	PDF_OP_Ts,
	PDF_OP_Tw,
	PDF_OP_Tz,
	PDF_OP_W,
	PDF_OP_Wstar,
	PDF_OP_b,
	PDF_OP_bstar,
	PDF_OP_c,
	PDF_OP_cm,
	PDF_OP_cs,
	PDF_OP_d,
	PDF_OP_d0,
	PDF_OP_d1,
	PDF_OP_f,
	PDF_OP_fstar,
	PDF_OP_g,
	PDF_OP_h,
	PDF_OP_i,
	PDF_OP_j,
	PDF_OP_k,
	PDF_OP_l,
	PDF_OP_m,
	PDF_OP_n,
	PDF_OP_q,
	PDF_OP_re,
	PDF_OP_rg,
	PDF_OP_ri,
	PDF_OP_s,
	PDF_OP_sc,
	PDF_OP_scn,
	PDF_OP_v,
	PDF_OP_w,
	PDF_OP_y,
	/* ops in this second section require additional try/catch handling */
	PDF_OP_Do,
	PDF_OP_Tf,
	PDF_OP_gs,
	PDF_OP_sh,
	/* END is used to signify end of stream (finalise and close down) */
	PDF_OP_END,
	/* And finally we have a max */
	PDF_OP_MAX
} PDF_OP;

typedef struct pdf_processor_s {
	pdf_operator_fn op_table[PDF_OP_MAX];
	pdf_process_annot_fn process_annot;
	pdf_process_stream_fn process_stream;
	pdf_process_contents_fn process_contents;
} pdf_processor;

typedef struct pdf_process_s
{
	const pdf_processor *processor;
	void *state;
} pdf_process;

struct pdf_csi_s
{
	pdf_document *doc;

	/* Current resource dict and file. These are in here to reduce param
	 * passing. */
	pdf_obj *rdb;
	fz_stream *file;

	/* Operator table */
	pdf_process process;

	/* interpreter stack */
	pdf_obj *obj;
	char name[256];
	unsigned char string[256];
	int string_len;
	float stack[32];
	int top;
	fz_image *img;

	int xbalance;
	int in_text;

	/* cookie support */
	fz_cookie *cookie;
};

static inline void pdf_process_op(pdf_csi *csi, int op, const pdf_process *process)
{
	process->processor->op_table[op](csi, process->state);
}

/* Helper functions for the filter implementations to call */
void pdf_process_contents_object(pdf_csi *csi, pdf_obj *rdb, pdf_obj *contents);
void pdf_process_stream(pdf_csi *csi, pdf_lexbuf *buf);

/* Functions to set up pdf_process structures */
pdf_process *pdf_process_run(pdf_process *process, fz_device *dev, const fz_matrix *ctm, const char *event, pdf_gstate *gstate, int nested);
pdf_process *pdf_process_buffer(pdf_process *process, fz_context *ctx, fz_buffer *buffer);
pdf_process *pdf_process_filter(pdf_process *process, fz_context *ctx, pdf_process *underlying, pdf_obj *resources);

/* Functions to actually use the pdf_process structures to process
 * annotations, glyphs and general stream objects */
void pdf_process_annot(pdf_document *doc, pdf_page *page, pdf_annot *annot, const pdf_process *process, fz_cookie *cookie);
void pdf_process_glyph(pdf_document *doc, pdf_obj *resources, fz_buffer *contents, pdf_process *process);
void pdf_process_stream_object(pdf_document *doc, pdf_obj *obj, const pdf_process *process, pdf_obj *res, fz_cookie *cookie);

#endif
