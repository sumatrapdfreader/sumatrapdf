#ifndef OPJ_DWT_H
#define OPJ_DWT_H

OPJ_BOOL opj_dwt_encode(opj_tcd_t *p_tcd,
                        opj_tcd_tilecomp_t * tilec);

OPJ_BOOL opj_dwt_decode(opj_tcd_t *p_tcd,
                        opj_tcd_tilecomp_t* tilec,
                        OPJ_UINT32 numres);

OPJ_FLOAT64 opj_dwt_getnorm(OPJ_UINT32 level, OPJ_UINT32 orient);

OPJ_BOOL opj_dwt_encode_real(opj_tcd_t *p_tcd,
                             opj_tcd_tilecomp_t * tilec);

OPJ_BOOL opj_dwt_decode_real(opj_tcd_t *p_tcd,
                             opj_tcd_tilecomp_t* OPJ_RESTRICT tilec,
                             OPJ_UINT32 numres);

OPJ_FLOAT64 opj_dwt_getnorm_real(OPJ_UINT32 level, OPJ_UINT32 orient);

void opj_dwt_calc_explicit_stepsizes(opj_tccp_t * tccp, OPJ_UINT32 prec);

#endif
