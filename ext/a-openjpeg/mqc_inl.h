#ifndef OPJ_MQC_INL_H
#define OPJ_MQC_INL_H

#define opj_mqc_mpsexchange_macro(d, curctx, a) \
{ \
    if (a < (*curctx)->qeval) { \
        d = !((*curctx)->mps); \
        *curctx = (*curctx)->nlps; \
    } else { \
        d = (*curctx)->mps; \
        *curctx = (*curctx)->nmps; \
    } \
}

#define opj_mqc_lpsexchange_macro(d, curctx, a) \
{ \
    if (a < (*curctx)->qeval) { \
        a = (*curctx)->qeval; \
        d = (*curctx)->mps; \
        *curctx = (*curctx)->nmps; \
    } else { \
        a = (*curctx)->qeval; \
        d = !((*curctx)->mps); \
        *curctx = (*curctx)->nlps; \
    } \
}

static INLINE OPJ_UINT32 opj_mqc_raw_decode(opj_mqc_t *mqc)
{
    OPJ_UINT32 d;
    if (mqc->ct == 0) {

        if (mqc->c == 0xff) {
            if (*mqc->bp  > 0x8f) {
                mqc->c = 0xff;
                mqc->ct = 8;
            } else {
                mqc->c = *mqc->bp;
                mqc->bp ++;
                mqc->ct = 7;
            }
        } else {
            mqc->c = *mqc->bp;
            mqc->bp ++;
            mqc->ct = 8;
        }
    }
    mqc->ct--;
    d = ((OPJ_UINT32)mqc->c >> mqc->ct) & 0x01U;

    return d;
}

#define opj_mqc_bytein_macro(mqc, c, ct) \
{ \
        OPJ_UINT32 l_c;  \
         \
         \
        l_c = *(mqc->bp + 1); \
        if (*mqc->bp == 0xff) { \
            if (l_c > 0x8f) { \
                c += 0xff00; \
                ct = 8; \
                mqc->end_of_byte_stream_counter ++; \
            } else { \
                mqc->bp++; \
                c += l_c << 9; \
                ct = 7; \
            } \
        } else { \
            mqc->bp++; \
            c += l_c << 8; \
            ct = 8; \
        } \
}

#define opj_mqc_renormd_macro(mqc, a, c, ct) \
{ \
    do { \
        if (ct == 0) { \
            opj_mqc_bytein_macro(mqc, c, ct); \
        } \
        a <<= 1; \
        c <<= 1; \
        ct--; \
    } while (a < 0x8000); \
}

#define opj_mqc_decode_macro(d, mqc, curctx, a, c, ct) \
{ \
     \
     \
     \
     \
    a -= (*curctx)->qeval;  \
    if ((c >> 16) < (*curctx)->qeval) {  \
        opj_mqc_lpsexchange_macro(d, curctx, a);  \
        opj_mqc_renormd_macro(mqc, a, c, ct);  \
    } else {  \
        c -= (*curctx)->qeval << 16;  \
        if ((a & 0x8000) == 0) { \
            opj_mqc_mpsexchange_macro(d, curctx, a); \
            opj_mqc_renormd_macro(mqc, a, c, ct); \
        } else { \
            d = (*curctx)->mps; \
        } \
    } \
}

#define DOWNLOAD_MQC_VARIABLES(mqc, curctx, a, c, ct) \
        register const opj_mqc_state_t **curctx = mqc->curctx; \
        register OPJ_UINT32 c = mqc->c; \
        register OPJ_UINT32 a = mqc->a; \
        register OPJ_UINT32 ct = mqc->ct

#define UPLOAD_MQC_VARIABLES(mqc, curctx, a, c, ct) \
        mqc->curctx = curctx; \
        mqc->c = c; \
        mqc->a = a; \
        mqc->ct = ct;

static INLINE void opj_mqc_bytein(opj_mqc_t *const mqc)
{
    opj_mqc_bytein_macro(mqc, mqc->c, mqc->ct);
}

#define opj_mqc_renormd(mqc) \
    opj_mqc_renormd_macro(mqc, mqc->a, mqc->c, mqc->ct)

#define opj_mqc_decode(d, mqc) \
    opj_mqc_decode_macro(d, mqc, mqc->curctx, mqc->a, mqc->c, mqc->ct)

void opj_mqc_byteout(opj_mqc_t *mqc);

#define opj_mqc_renorme_macro(mqc, a_, c_, ct_) \
{ \
    do { \
        a_ <<= 1; \
        c_ <<= 1; \
        ct_--; \
        if (ct_ == 0) { \
            mqc->c = c_; \
            opj_mqc_byteout(mqc); \
            c_ = mqc->c; \
            ct_ = mqc->ct; \
        } \
    } while( (a_ & 0x8000) == 0); \
}

#define opj_mqc_codemps_macro(mqc, curctx, a, c, ct) \
{ \
    a -= (*curctx)->qeval; \
    if ((a & 0x8000) == 0) { \
        if (a < (*curctx)->qeval) { \
            a = (*curctx)->qeval; \
        } else { \
            c += (*curctx)->qeval; \
        } \
        *curctx = (*curctx)->nmps; \
        opj_mqc_renorme_macro(mqc, a, c, ct); \
    } else { \
        c += (*curctx)->qeval; \
    } \
}

#define opj_mqc_codelps_macro(mqc, curctx, a, c, ct) \
{ \
    a -= (*curctx)->qeval; \
    if (a < (*curctx)->qeval) { \
        c += (*curctx)->qeval; \
    } else { \
        a = (*curctx)->qeval; \
    } \
    *curctx = (*curctx)->nlps; \
    opj_mqc_renorme_macro(mqc, a, c, ct); \
}

#define opj_mqc_encode_macro(mqc, curctx, a, c, ct, d) \
{ \
    if ((*curctx)->mps == (d)) { \
        opj_mqc_codemps_macro(mqc, curctx, a, c, ct); \
    } else { \
        opj_mqc_codelps_macro(mqc, curctx, a, c, ct); \
    } \
}

#define opj_mqc_bypass_enc_macro(mqc, c, ct, d) \
{\
    if (ct == BYPASS_CT_INIT) {\
        ct = 8;\
    }\
    ct--;\
    c = c + ((d) << ct);\
    if (ct == 0) {\
        *mqc->bp = (OPJ_BYTE)c;\
        ct = 8;\
         \
        if (*mqc->bp == 0xff) {\
            ct = 7;\
        }\
        mqc->bp++;\
        c = 0;\
    }\
}

#endif
