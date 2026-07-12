#ifndef OPJ_JP2_H
#define OPJ_JP2_H

#define     JP2_JP   0x6a502020
#define     JP2_FTYP 0x66747970
#define     JP2_JP2H 0x6a703268
#define     JP2_IHDR 0x69686472
#define     JP2_COLR 0x636f6c72
#define     JP2_JP2C 0x6a703263
#define     JP2_URL  0x75726c20
#define     JP2_PCLR 0x70636c72
#define     JP2_CMAP 0x636d6170
#define     JP2_CDEF 0x63646566
#define     JP2_DTBL 0x6474626c
#define     JP2_BPCC 0x62706363
#define     JP2_JP2  0x6a703220

typedef enum {
    JP2_STATE_NONE            = 0x0,
    JP2_STATE_SIGNATURE       = 0x1,
    JP2_STATE_FILE_TYPE       = 0x2,
    JP2_STATE_HEADER          = 0x4,
    JP2_STATE_CODESTREAM      = 0x8,
    JP2_STATE_END_CODESTREAM  = 0x10,
    JP2_STATE_UNKNOWN         = 0x7fffffff
}
JP2_STATE;

typedef enum {
    JP2_IMG_STATE_NONE        = 0x0,
    JP2_IMG_STATE_UNKNOWN     = 0x7fffffff
}
JP2_IMG_STATE;

typedef struct opj_jp2_cdef_info {
    OPJ_UINT16 cn, typ, asoc;
} opj_jp2_cdef_info_t;

typedef struct opj_jp2_cdef {
    opj_jp2_cdef_info_t *info;
    OPJ_UINT16 n;
} opj_jp2_cdef_t;

typedef struct opj_jp2_cmap_comp {
    OPJ_UINT16 cmp;
    OPJ_BYTE mtyp, pcol;
} opj_jp2_cmap_comp_t;

typedef struct opj_jp2_pclr {
    OPJ_UINT32 *entries;
    OPJ_BYTE *channel_sign;
    OPJ_BYTE *channel_size;
    opj_jp2_cmap_comp_t *cmap;
    OPJ_UINT16 nr_entries;
    OPJ_BYTE nr_channels;
} opj_jp2_pclr_t;

typedef struct opj_jp2_color {
    OPJ_BYTE *icc_profile_buf;
    OPJ_UINT32 icc_profile_len;

    opj_jp2_cdef_t *jp2_cdef;
    opj_jp2_pclr_t *jp2_pclr;
    OPJ_BYTE jp2_has_colr;
} opj_jp2_color_t;

typedef struct opj_jp2_comps {
    OPJ_UINT32 depth;
    OPJ_UINT32 sgnd;
    OPJ_UINT32 bpcc;
} opj_jp2_comps_t;

typedef struct opj_jp2 {

    opj_j2k_t *j2k;

    struct opj_procedure_list * m_validation_list;

    struct opj_procedure_list * m_procedure_list;

    OPJ_UINT32 w;

    OPJ_UINT32 h;

    OPJ_UINT32 numcomps;
    OPJ_UINT32 bpc;
    OPJ_UINT32 C;
    OPJ_UINT32 UnkC;
    OPJ_UINT32 IPR;
    OPJ_UINT32 meth;
    OPJ_UINT32 approx;
    OPJ_UINT32 enumcs;
    OPJ_UINT32 precedence;
    OPJ_UINT32 brand;
    OPJ_UINT32 minversion;
    OPJ_UINT32 numcl;
    OPJ_UINT32 *cl;
    opj_jp2_comps_t *comps;

    OPJ_OFF_T j2k_codestream_offset;
    OPJ_OFF_T jpip_iptr_offset;
    OPJ_BOOL jpip_on;
    OPJ_UINT32 jp2_state;
    OPJ_UINT32 jp2_img_state;

    opj_jp2_color_t color;

    OPJ_BOOL ignore_pclr_cmap_cdef;
    OPJ_BYTE has_jp2h;
    OPJ_BYTE has_ihdr;
}
opj_jp2_t;

typedef struct opj_jp2_box {
    OPJ_UINT32 length;
    OPJ_UINT32 type;
    OPJ_INT32 init_pos;
} opj_jp2_box_t;

typedef struct opj_jp2_header_handler {

    OPJ_UINT32 id;

    OPJ_BOOL(*handler)(opj_jp2_t *jp2,
                       OPJ_BYTE *p_header_data,
                       OPJ_UINT32 p_header_size,
                       opj_event_mgr_t * p_manager);
}
opj_jp2_header_handler_t;

typedef struct opj_jp2_img_header_writer_handler {

    OPJ_BYTE*   (*handler)(opj_jp2_t *jp2, OPJ_UINT32 * p_data_size);

    OPJ_BYTE*   m_data;

    OPJ_UINT32  m_size;
}
opj_jp2_img_header_writer_handler_t;

void opj_jp2_setup_decoder(opj_jp2_t *jp2, opj_dparameters_t *parameters);

void opj_jp2_decoder_set_strict_mode(opj_jp2_t *jp2, OPJ_BOOL strict);

OPJ_BOOL opj_jp2_set_threads(opj_jp2_t *jp2, OPJ_UINT32 num_threads);

OPJ_BOOL opj_jp2_decode(opj_jp2_t *jp2,
                        opj_stream_private_t *p_stream,
                        opj_image_t* p_image,
                        opj_event_mgr_t * p_manager);

OPJ_BOOL opj_jp2_setup_encoder(opj_jp2_t *jp2,
                               opj_cparameters_t *parameters,
                               opj_image_t *image,
                               opj_event_mgr_t * p_manager);

OPJ_BOOL opj_jp2_encode(opj_jp2_t *jp2,
                        opj_stream_private_t *stream,
                        opj_event_mgr_t * p_manager);

OPJ_BOOL opj_jp2_start_compress(opj_jp2_t *jp2,
                                opj_stream_private_t *stream,
                                opj_image_t * p_image,
                                opj_event_mgr_t * p_manager);

OPJ_BOOL opj_jp2_end_compress(opj_jp2_t *jp2,
                              opj_stream_private_t *cio,
                              opj_event_mgr_t * p_manager);

OPJ_BOOL opj_jp2_end_decompress(opj_jp2_t *jp2,
                                opj_stream_private_t *cio,
                                opj_event_mgr_t * p_manager);

OPJ_BOOL opj_jp2_read_header(opj_stream_private_t *p_stream,
                             opj_jp2_t *jp2,
                             opj_image_t ** p_image,
                             opj_event_mgr_t * p_manager);

OPJ_BOOL opj_jp2_set_decoded_components(opj_jp2_t *jp2,
                                        OPJ_UINT32 numcomps,
                                        const OPJ_UINT32* comps_indices,
                                        opj_event_mgr_t * p_manager);

OPJ_BOOL opj_jp2_read_tile_header(opj_jp2_t * p_jp2,
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

OPJ_BOOL opj_jp2_write_tile(opj_jp2_t *p_jp2,
                            OPJ_UINT32 p_tile_index,
                            OPJ_BYTE * p_data,
                            OPJ_UINT32 p_data_size,
                            opj_stream_private_t *p_stream,
                            opj_event_mgr_t * p_manager);

OPJ_BOOL opj_jp2_decode_tile(opj_jp2_t * p_jp2,
                             OPJ_UINT32 p_tile_index,
                             OPJ_BYTE * p_data,
                             OPJ_UINT32 p_data_size,
                             opj_stream_private_t *p_stream,
                             opj_event_mgr_t * p_manager);

opj_jp2_t* opj_jp2_create(OPJ_BOOL p_is_decoder);

void opj_jp2_destroy(opj_jp2_t *jp2);

OPJ_BOOL opj_jp2_set_decode_area(opj_jp2_t *p_jp2,
                                 opj_image_t* p_image,
                                 OPJ_INT32 p_start_x, OPJ_INT32 p_start_y,
                                 OPJ_INT32 p_end_x, OPJ_INT32 p_end_y,
                                 opj_event_mgr_t * p_manager);

OPJ_BOOL opj_jp2_get_tile(opj_jp2_t *p_jp2,
                          opj_stream_private_t *p_stream,
                          opj_image_t* p_image,
                          opj_event_mgr_t * p_manager,
                          OPJ_UINT32 tile_index);

OPJ_BOOL opj_jp2_set_decoded_resolution_factor(opj_jp2_t *p_jp2,
        OPJ_UINT32 res_factor,
        opj_event_mgr_t * p_manager);

OPJ_BOOL opj_jp2_encoder_set_extra_options(
    opj_jp2_t *p_jp2,
    const char* const* p_options,
    opj_event_mgr_t * p_manager);

void jp2_dump(opj_jp2_t* p_jp2, OPJ_INT32 flag, FILE* out_stream);

opj_codestream_info_v2_t* jp2_get_cstr_info(opj_jp2_t* p_jp2);

opj_codestream_index_t* jp2_get_cstr_index(opj_jp2_t* p_jp2);

#endif
