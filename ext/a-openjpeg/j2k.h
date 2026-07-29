#ifndef OPJ_J2K_H
#define OPJ_J2K_H

#define J2K_CP_CSTY_PRT 0x01
#define J2K_CP_CSTY_SOP 0x02
#define J2K_CP_CSTY_EPH 0x04
#define J2K_CCP_CSTY_PRT 0x01
#define J2K_CCP_CBLKSTY_LAZY 0x01
#define J2K_CCP_CBLKSTY_RESET 0x02
#define J2K_CCP_CBLKSTY_TERMALL 0x04
#define J2K_CCP_CBLKSTY_VSC 0x08
#define J2K_CCP_CBLKSTY_PTERM 0x10
#define J2K_CCP_CBLKSTY_SEGSYM 0x20
#define J2K_CCP_CBLKSTY_HT 0x40
#define J2K_CCP_CBLKSTY_HTMIXED 0x80
#define J2K_CCP_QNTSTY_NOQNT 0
#define J2K_CCP_QNTSTY_SIQNT 1
#define J2K_CCP_QNTSTY_SEQNT 2

#define J2K_MS_SOC 0xff4f
#define J2K_MS_SOT 0xff90
#define J2K_MS_SOD 0xff93
#define J2K_MS_EOC 0xffd9
#define J2K_MS_CAP 0xff50
#define J2K_MS_SIZ 0xff51
#define J2K_MS_COD 0xff52
#define J2K_MS_COC 0xff53
#define J2K_MS_CPF 0xff59
#define J2K_MS_RGN 0xff5e
#define J2K_MS_QCD 0xff5c
#define J2K_MS_QCC 0xff5d
#define J2K_MS_POC 0xff5f
#define J2K_MS_TLM 0xff55
#define J2K_MS_PLM 0xff57
#define J2K_MS_PLT 0xff58
#define J2K_MS_PPM 0xff60
#define J2K_MS_PPT 0xff61
#define J2K_MS_SOP 0xff91
#define J2K_MS_EPH 0xff92
#define J2K_MS_CRG 0xff63
#define J2K_MS_COM 0xff64
#define J2K_MS_CBD 0xff78
#define J2K_MS_MCC 0xff75
#define J2K_MS_MCT 0xff74
#define J2K_MS_MCO 0xff77

#define J2K_MS_UNK 0

#ifdef USE_JPWL
#define J2K_MS_EPC 0xff68
#define J2K_MS_EPB 0xff66
#define J2K_MS_ESD 0xff67
#define J2K_MS_RED 0xff69
#endif
#ifdef USE_JPSEC
#define J2K_MS_SEC 0xff65
#define J2K_MS_INSEC 0xff94
#endif

#define J2K_MAX_POCS    32

#define J2K_TCD_MATRIX_MAX_LAYER_COUNT 10
#define J2K_TCD_MATRIX_MAX_RESOLUTION_COUNT 10

typedef enum J2K_STATUS {
    J2K_STATE_NONE  =  0x0000,
    J2K_STATE_MHSOC  = 0x0001,
    J2K_STATE_MHSIZ  = 0x0002,
    J2K_STATE_MH     = 0x0004,
    J2K_STATE_TPHSOT = 0x0008,
    J2K_STATE_TPH    = 0x0010,
    J2K_STATE_MT     = 0x0020,
    J2K_STATE_NEOC   = 0x0040,
    J2K_STATE_DATA   = 0x0080,

    J2K_STATE_EOC    = 0x0100,
    J2K_STATE_ERR    = 0x8000
} J2K_STATUS;

typedef enum MCT_ELEMENT_TYPE {
    MCT_TYPE_INT16 = 0,
    MCT_TYPE_INT32 = 1,
    MCT_TYPE_FLOAT = 2,
    MCT_TYPE_DOUBLE = 3
} J2K_MCT_ELEMENT_TYPE;

typedef enum MCT_ARRAY_TYPE {
    MCT_TYPE_DEPENDENCY = 0,
    MCT_TYPE_DECORRELATION = 1,
    MCT_TYPE_OFFSET = 2
} J2K_MCT_ARRAY_TYPE;

typedef enum T2_MODE {
    THRESH_CALC = 0,
    FINAL_PASS = 1
} J2K_T2_MODE;

typedef struct opj_stepsize {

    OPJ_INT32 expn;

    OPJ_INT32 mant;
} opj_stepsize_t;

typedef struct opj_tccp {

    OPJ_UINT32 csty;

    OPJ_UINT32 numresolutions;

    OPJ_UINT32 cblkw;

    OPJ_UINT32 cblkh;

    OPJ_UINT32 cblksty;

    OPJ_UINT32 qmfbid;

    OPJ_UINT32 qntsty;

    opj_stepsize_t stepsizes[OPJ_J2K_MAXBANDS];

    OPJ_UINT32 numgbits;

    OPJ_INT32 roishift;

    OPJ_UINT32 prcw[OPJ_J2K_MAXRLVLS];

    OPJ_UINT32 prch[OPJ_J2K_MAXRLVLS];

    OPJ_INT32 m_dc_level_shift;
}
opj_tccp_t;

typedef struct opj_mct_data {
    J2K_MCT_ELEMENT_TYPE m_element_type;
    J2K_MCT_ARRAY_TYPE   m_array_type;
    OPJ_UINT32           m_index;
    OPJ_BYTE *           m_data;
    OPJ_UINT32           m_data_size;
}
opj_mct_data_t;

typedef struct opj_simple_mcc_decorrelation_data {
    OPJ_UINT32           m_index;
    OPJ_UINT32           m_nb_comps;
    opj_mct_data_t *     m_decorrelation_array;
    opj_mct_data_t *     m_offset_array;
    OPJ_BITFIELD         m_is_irreversible : 1;
}
opj_simple_mcc_decorrelation_data_t;

typedef struct opj_ppx_struct {
    OPJ_BYTE*   m_data;
    OPJ_UINT32  m_data_size;
} opj_ppx;

typedef struct opj_tcp {

    OPJ_UINT32 csty;

    OPJ_PROG_ORDER prg;

    OPJ_UINT32 numlayers;
    OPJ_UINT32 num_layers_to_decode;

    OPJ_UINT32 mct;

    OPJ_FLOAT32 rates[100];

    OPJ_UINT32 numpocs;

    opj_poc_t pocs[J2K_MAX_POCS];

    OPJ_UINT32 ppt_markers_count;

    opj_ppx* ppt_markers;

    OPJ_BYTE *ppt_data;

    OPJ_BYTE *ppt_buffer;

    OPJ_UINT32 ppt_data_size;

    OPJ_UINT32 ppt_len;

    OPJ_FLOAT32 distoratio[100];

    opj_tccp_t *tccps;

    OPJ_INT32  m_current_tile_part_number;

    OPJ_UINT32 m_nb_tile_parts;

    OPJ_BYTE *      m_data;

    OPJ_UINT32      m_data_size;

    OPJ_FLOAT64 *   mct_norms;

    OPJ_FLOAT32 *   m_mct_decoding_matrix;

    OPJ_FLOAT32 *   m_mct_coding_matrix;

    opj_mct_data_t * m_mct_records;

    OPJ_UINT32 m_nb_mct_records;

    OPJ_UINT32 m_nb_max_mct_records;

    opj_simple_mcc_decorrelation_data_t * m_mcc_records;

    OPJ_UINT32 m_nb_mcc_records;

    OPJ_UINT32 m_nb_max_mcc_records;

    OPJ_BITFIELD cod : 1;

    OPJ_BITFIELD ppt : 1;

    OPJ_BITFIELD POC : 1;
} opj_tcp_t;

typedef enum {
    RATE_DISTORTION_RATIO = 0,
    FIXED_DISTORTION_RATIO = 1,
    FIXED_LAYER = 2,
} J2K_QUALITY_LAYER_ALLOCATION_STRATEGY;

typedef struct opj_encoding_param {

    OPJ_UINT32 m_max_comp_size;

    OPJ_INT32 m_tp_pos;

    OPJ_INT32 *m_matrice;

    OPJ_BYTE m_tp_flag;

    J2K_QUALITY_LAYER_ALLOCATION_STRATEGY m_quality_layer_alloc_strategy;

    OPJ_BITFIELD m_tp_on : 1;
}
opj_encoding_param_t;

typedef struct opj_decoding_param {

    OPJ_UINT32 m_reduce;

    OPJ_UINT32 m_layer;
}
opj_decoding_param_t;

typedef struct opj_cp {

    OPJ_UINT16 rsiz;

    OPJ_UINT32 tx0;

    OPJ_UINT32 ty0;

    OPJ_UINT32 tdx;

    OPJ_UINT32 tdy;

    OPJ_CHAR *comment;

    OPJ_UINT32 tw;

    OPJ_UINT32 th;

    OPJ_UINT32 ppm_markers_count;

    opj_ppx* ppm_markers;

    OPJ_BYTE *ppm_data;

    OPJ_UINT32 ppm_len;

    OPJ_UINT32 ppm_data_read;

    OPJ_BYTE *ppm_data_current;

    OPJ_BYTE *ppm_buffer;

    OPJ_BYTE *ppm_data_first;

    OPJ_UINT32 ppm_data_size;

    OPJ_INT32 ppm_store;

    OPJ_INT32 ppm_previous;

    opj_tcp_t *tcps;

    union {
        opj_decoding_param_t m_dec;
        opj_encoding_param_t m_enc;
    }
    m_specific_param;

    OPJ_BOOL strict;

#ifdef USE_JPWL

    OPJ_BOOL epc_on;

    OPJ_BOOL epb_on;

    OPJ_BOOL esd_on;

    OPJ_BOOL info_on;

    OPJ_BOOL red_on;

    int hprot_MH;

    int hprot_TPH_tileno[JPWL_MAX_NO_TILESPECS];

    int hprot_TPH[JPWL_MAX_NO_TILESPECS];

    int pprot_tileno[JPWL_MAX_NO_PACKSPECS];

    int pprot_packno[JPWL_MAX_NO_PACKSPECS];

    int pprot[JPWL_MAX_NO_PACKSPECS];

    int sens_size;

    int sens_addr;

    int sens_range;

    int sens_MH;

    int sens_TPH_tileno[JPWL_MAX_NO_TILESPECS];

    int sens_TPH[JPWL_MAX_NO_TILESPECS];

    OPJ_BOOL correct;

    int exp_comps;

    OPJ_UINT32 max_tiles;
#endif

    OPJ_BITFIELD ppm : 1;

    OPJ_BITFIELD m_is_decoder : 1;

    OPJ_BITFIELD allow_different_bit_depth_sign : 1;

} opj_cp_t;

typedef struct opj_j2k_tlm_tile_part_info {

    OPJ_UINT16 m_tile_index;

    OPJ_UINT32 m_length;
} opj_j2k_tlm_tile_part_info_t;

typedef struct opj_j2k_tlm_info {

    OPJ_UINT32 m_entries_count;

    opj_j2k_tlm_tile_part_info_t* m_tile_part_infos;

    OPJ_BOOL m_is_invalid;
} opj_j2k_tlm_info_t;

typedef struct opj_j2k_dec {

    OPJ_UINT32 m_state;

    opj_tcp_t *m_default_tcp;
    OPJ_BYTE  *m_header_data;
    OPJ_UINT32 m_header_data_size;

    OPJ_UINT32 m_sot_length;

    OPJ_UINT32 m_start_tile_x;
    OPJ_UINT32 m_start_tile_y;
    OPJ_UINT32 m_end_tile_x;
    OPJ_UINT32 m_end_tile_y;

    OPJ_INT32 m_tile_ind_to_dec;

    OPJ_OFF_T m_last_sot_read_pos;

    OPJ_BOOL   m_last_tile_part;

    OPJ_UINT32   m_numcomps_to_decode;
    OPJ_UINT32  *m_comps_indices_to_decode;

    opj_j2k_tlm_info_t m_tlm;

    OPJ_UINT32  m_idx_intersecting_tile_parts;

    OPJ_UINT32  m_num_intersecting_tile_parts;

    OPJ_OFF_T*  m_intersecting_tile_parts_offset;

    OPJ_BITFIELD m_can_decode : 1;
    OPJ_BITFIELD m_discard_tiles : 1;
    OPJ_BITFIELD m_skip_data : 1;

    OPJ_BITFIELD m_nb_tile_parts_correction_checked : 1;
    OPJ_BITFIELD m_nb_tile_parts_correction : 1;

} opj_j2k_dec_t;

typedef struct opj_j2k_enc {

    OPJ_UINT32 m_current_poc_tile_part_number;

    OPJ_UINT32 m_current_tile_part_number;

    OPJ_BOOL   m_TLM;

    OPJ_BOOL   m_Ttlmi_is_byte;

    OPJ_OFF_T m_tlm_start;

    OPJ_BYTE * m_tlm_sot_offsets_buffer;

    OPJ_BYTE * m_tlm_sot_offsets_current;

    OPJ_UINT32 m_total_tile_parts;

    OPJ_BYTE * m_encoded_tile_data;

    OPJ_UINT32 m_encoded_tile_size;

    OPJ_BYTE * m_header_tile_data;

    OPJ_UINT32 m_header_tile_data_size;

    OPJ_BOOL   m_PLT;

    OPJ_UINT32 m_reserved_bytes_for_PLT;

    OPJ_UINT32 m_nb_comps;

} opj_j2k_enc_t;

struct opj_tcd;

typedef struct opj_j2k {

    OPJ_BOOL m_is_decoder;

    union {
        opj_j2k_dec_t m_decoder;
        opj_j2k_enc_t m_encoder;
    }
    m_specific_param;

    opj_image_t* m_private_image;

    opj_image_t* m_output_image;

    opj_cp_t m_cp;

    opj_procedure_list_t *  m_procedure_list;

    opj_procedure_list_t *  m_validation_list;

    opj_codestream_index_t *cstr_index;

    OPJ_UINT32 m_current_tile_number;

    struct opj_tcd *    m_tcd;

    opj_thread_pool_t* m_tp;

    OPJ_UINT32 ihdr_w;

    OPJ_UINT32 ihdr_h;

    unsigned int dump_state;
}
opj_j2k_t;

void opj_j2k_setup_decoder(opj_j2k_t *j2k, opj_dparameters_t *parameters);

void opj_j2k_decoder_set_strict_mode(opj_j2k_t *j2k, OPJ_BOOL strict);

OPJ_BOOL opj_j2k_set_threads(opj_j2k_t *j2k, OPJ_UINT32 num_threads);

opj_j2k_t* opj_j2k_create_compress(void);

OPJ_BOOL opj_j2k_setup_encoder(opj_j2k_t *p_j2k,
                               opj_cparameters_t *parameters,
                               opj_image_t *image,
                               opj_event_mgr_t * p_manager);

const char *opj_j2k_convert_progression_order(OPJ_PROG_ORDER prg_order);

OPJ_BOOL opj_j2k_end_decompress(opj_j2k_t *j2k,
                                opj_stream_private_t *p_stream,
                                opj_event_mgr_t * p_manager);

OPJ_BOOL opj_j2k_read_header(opj_stream_private_t *p_stream,
                             opj_j2k_t* p_j2k,
                             opj_image_t** p_image,
                             opj_event_mgr_t* p_manager);

void opj_j2k_destroy(opj_j2k_t *p_j2k);

void j2k_destroy_cstr_index(opj_codestream_index_t *p_cstr_ind);

OPJ_BOOL opj_j2k_decode_tile(opj_j2k_t * p_j2k,
                             OPJ_UINT32 p_tile_index,
                             OPJ_BYTE * p_data,
                             OPJ_UINT32 p_data_size,
                             opj_stream_private_t *p_stream,
                             opj_event_mgr_t * p_manager);

OPJ_BOOL opj_j2k_read_tile_header(opj_j2k_t * p_j2k,
                                  OPJ_UINT32 * p_tile_index,
                                  OPJ_UINT32 * p_data_size,
                                  OPJ_INT32 * p_tile_x0,
                                  OPJ_INT32 * p_tile_y0,
                                  OPJ_INT32 * p_tile_x1,
                                  OPJ_INT32 * p_tile_y1,
                                  OPJ_UINT32 * p_nb_comps,
                                  OPJ_BOOL * p_go_on,
                                  opj_stream_private_t *p_stream,
                                  opj_event_mgr_t * p_manager);

OPJ_BOOL opj_j2k_set_decoded_components(opj_j2k_t *p_j2k,
                                        OPJ_UINT32 numcomps,
                                        const OPJ_UINT32* comps_indices,
                                        opj_event_mgr_t * p_manager);

OPJ_BOOL opj_j2k_set_decode_area(opj_j2k_t *p_j2k,
                                 opj_image_t* p_image,
                                 OPJ_INT32 p_start_x, OPJ_INT32 p_start_y,
                                 OPJ_INT32 p_end_x, OPJ_INT32 p_end_y,
                                 opj_event_mgr_t * p_manager);

opj_j2k_t* opj_j2k_create_decompress(void);

void j2k_dump(opj_j2k_t* p_j2k, OPJ_INT32 flag, FILE* out_stream);

void j2k_dump_image_header(opj_image_t* image, OPJ_BOOL dev_dump_flag,
                           FILE* out_stream);

void j2k_dump_image_comp_header(opj_image_comp_t* comp, OPJ_BOOL dev_dump_flag,
                                FILE* out_stream);

opj_codestream_info_v2_t* j2k_get_cstr_info(opj_j2k_t* p_j2k);

opj_codestream_index_t* j2k_get_cstr_index(opj_j2k_t* p_j2k);

OPJ_BOOL opj_j2k_decode(opj_j2k_t *j2k,
                        opj_stream_private_t *p_stream,
                        opj_image_t *p_image,
                        opj_event_mgr_t *p_manager);

OPJ_BOOL opj_j2k_get_tile(opj_j2k_t *p_j2k,
                          opj_stream_private_t *p_stream,
                          opj_image_t* p_image,
                          opj_event_mgr_t * p_manager,
                          OPJ_UINT32 tile_index);

OPJ_BOOL opj_j2k_set_decoded_resolution_factor(opj_j2k_t *p_j2k,
        OPJ_UINT32 res_factor,
        opj_event_mgr_t * p_manager);

OPJ_BOOL opj_j2k_encoder_set_extra_options(
    opj_j2k_t *p_j2k,
    const char* const* p_options,
    opj_event_mgr_t * p_manager);

OPJ_BOOL opj_j2k_write_tile(opj_j2k_t * p_j2k,
                            OPJ_UINT32 p_tile_index,
                            OPJ_BYTE * p_data,
                            OPJ_UINT32 p_data_size,
                            opj_stream_private_t *p_stream,
                            opj_event_mgr_t * p_manager);

OPJ_BOOL opj_j2k_encode(opj_j2k_t * p_j2k,
                        opj_stream_private_t *cio,
                        opj_event_mgr_t * p_manager);

OPJ_BOOL opj_j2k_start_compress(opj_j2k_t *p_j2k,
                                opj_stream_private_t *p_stream,
                                opj_image_t * p_image,
                                opj_event_mgr_t * p_manager);

OPJ_BOOL opj_j2k_end_compress(opj_j2k_t *p_j2k,
                              opj_stream_private_t *cio,
                              opj_event_mgr_t * p_manager);

OPJ_BOOL opj_j2k_setup_mct_encoding(opj_tcp_t * p_tcp, opj_image_t * p_image);

#endif
