#ifndef OPJ_MQC_H
#define OPJ_MQC_H

#include "opj_common.h"

typedef struct opj_mqc_state {

    OPJ_UINT32 qeval;

    OPJ_UINT32 mps;

    const struct opj_mqc_state *nmps;

    const struct opj_mqc_state *nlps;
} opj_mqc_state_t;

#define MQC_NUMCTXS 19

typedef struct opj_mqc {

    OPJ_UINT32 c;

    OPJ_UINT32 a;

    OPJ_UINT32 ct;

    OPJ_UINT32 end_of_byte_stream_counter;

    OPJ_BYTE *bp;

    OPJ_BYTE *start;

    OPJ_BYTE *end;

    const opj_mqc_state_t *ctxs[MQC_NUMCTXS];

    const opj_mqc_state_t **curctx;

    const OPJ_BYTE* lut_ctxno_zc_orient;

    OPJ_BYTE backup[OPJ_COMMON_CBLK_DATA_EXTRA];
} opj_mqc_t;

#define BYPASS_CT_INIT  0xDEADBEEF

#include "mqc_inl.h"

OPJ_UINT32 opj_mqc_numbytes(opj_mqc_t *mqc);

void opj_mqc_resetstates(opj_mqc_t *mqc);

void opj_mqc_setstate(opj_mqc_t *mqc, OPJ_UINT32 ctxno, OPJ_UINT32 msb,
                      OPJ_INT32 prob);

void opj_mqc_init_enc(opj_mqc_t *mqc, OPJ_BYTE *bp);

#define opj_mqc_setcurctx(mqc, ctxno)   (mqc)->curctx = &(mqc)->ctxs[(OPJ_UINT32)(ctxno)]

void opj_mqc_flush(opj_mqc_t *mqc);

void opj_mqc_bypass_init_enc(opj_mqc_t *mqc);

OPJ_UINT32 opj_mqc_bypass_get_extra_bytes(opj_mqc_t *mqc, OPJ_BOOL erterm);

void opj_mqc_bypass_enc(opj_mqc_t *mqc, OPJ_UINT32 d);

void opj_mqc_bypass_flush_enc(opj_mqc_t *mqc, OPJ_BOOL erterm);

void opj_mqc_reset_enc(opj_mqc_t *mqc);

#ifdef notdef

OPJ_UINT32 opj_mqc_restart_enc(opj_mqc_t *mqc);
#endif

void opj_mqc_restart_init_enc(opj_mqc_t *mqc);

void opj_mqc_erterm_enc(opj_mqc_t *mqc);

void opj_mqc_segmark_enc(opj_mqc_t *mqc);

void opj_mqc_init_dec(opj_mqc_t *mqc, OPJ_BYTE *bp, OPJ_UINT32 len,
                      OPJ_UINT32 extra_writable_bytes);

void opj_mqc_raw_init_dec(opj_mqc_t *mqc, OPJ_BYTE *bp, OPJ_UINT32 len,
                          OPJ_UINT32 extra_writable_bytes);

void opq_mqc_finish_dec(opj_mqc_t *mqc);

#endif
