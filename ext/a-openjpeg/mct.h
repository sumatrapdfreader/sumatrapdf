#ifndef OPJ_MCT_H
#define OPJ_MCT_H

void opj_mct_encode(OPJ_INT32* OPJ_RESTRICT c0, OPJ_INT32* OPJ_RESTRICT c1,
                    OPJ_INT32* OPJ_RESTRICT c2, OPJ_SIZE_T n);

void opj_mct_decode(OPJ_INT32* OPJ_RESTRICT c0, OPJ_INT32* OPJ_RESTRICT c1,
                    OPJ_INT32* OPJ_RESTRICT c2, OPJ_SIZE_T n);

OPJ_FLOAT64 opj_mct_getnorm(OPJ_UINT32 compno);

void opj_mct_encode_real(OPJ_FLOAT32* OPJ_RESTRICT c0,
                         OPJ_FLOAT32* OPJ_RESTRICT c1,
                         OPJ_FLOAT32* OPJ_RESTRICT c2, OPJ_SIZE_T n);

void opj_mct_decode_real(OPJ_FLOAT32* OPJ_RESTRICT c0,
                         OPJ_FLOAT32* OPJ_RESTRICT c1, OPJ_FLOAT32* OPJ_RESTRICT c2, OPJ_SIZE_T n);

OPJ_FLOAT64 opj_mct_getnorm_real(OPJ_UINT32 compno);

OPJ_BOOL opj_mct_encode_custom(
    OPJ_BYTE * p_coding_data,
    OPJ_SIZE_T n,
    OPJ_BYTE ** p_data,
    OPJ_UINT32 p_nb_comp,
    OPJ_UINT32 is_signed);

OPJ_BOOL opj_mct_decode_custom(
    OPJ_BYTE * pDecodingData,
    OPJ_SIZE_T n,
    OPJ_BYTE ** pData,
    OPJ_UINT32 pNbComp,
    OPJ_UINT32 isSigned);

void opj_calculate_norms(OPJ_FLOAT64 * pNorms,
                         OPJ_UINT32 p_nb_comps,
                         OPJ_FLOAT32 * pMatrix);

const OPJ_FLOAT64 * opj_mct_get_mct_norms(void);

const OPJ_FLOAT64 * opj_mct_get_mct_norms_real(void);

#endif
