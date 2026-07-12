#ifndef OPJ_TCD_H
#define OPJ_TCD_H

typedef struct opj_tcd_pass {
    OPJ_UINT32 rate;
    OPJ_FLOAT64 distortiondec;
    OPJ_UINT32 len;
    OPJ_BITFIELD term : 1;
} opj_tcd_pass_t;

typedef struct opj_tcd_layer {
    OPJ_UINT32 numpasses;
    OPJ_UINT32 len;
    OPJ_FLOAT64 disto;
    OPJ_BYTE *data;
} opj_tcd_layer_t;

typedef struct opj_tcd_cblk_enc {
    OPJ_BYTE* data;
    opj_tcd_layer_t* layers;
    opj_tcd_pass_t* passes;
    OPJ_INT32 x0, y0, x1,
              y1;
    OPJ_UINT32 numbps;
    OPJ_UINT32 numlenbits;
    OPJ_UINT32 data_size;
    OPJ_UINT32
    numpasses;
    OPJ_UINT32 numpassesinlayers;
    OPJ_UINT32 totalpasses;
} opj_tcd_cblk_enc_t;

typedef struct opj_tcd_seg_data_chunk {

    OPJ_BYTE * data;
    OPJ_UINT32 len;
} opj_tcd_seg_data_chunk_t;

typedef struct opj_tcd_seg {
    OPJ_UINT32 len;

    OPJ_UINT32 numpasses;

    OPJ_UINT32 real_num_passes;

    OPJ_UINT32 maxpasses;

    OPJ_UINT32 numnewpasses;

    OPJ_UINT32 newlen;
} opj_tcd_seg_t;

typedef struct opj_tcd_cblk_dec {
    opj_tcd_seg_t* segs;
    opj_tcd_seg_data_chunk_t* chunks;

    OPJ_INT32 x0, y0, x1, y1;

    OPJ_UINT32 Mb;

    OPJ_UINT32 numbps;

    OPJ_UINT32 numlenbits;

    OPJ_UINT32 numnewpasses;

    OPJ_UINT32 numsegs;

    OPJ_UINT32 real_num_segs;
    OPJ_UINT32 m_current_max_segs;
    OPJ_UINT32 numchunks;
    OPJ_UINT32 numchunksalloc;

    OPJ_INT32* decoded_data;
    OPJ_BOOL corrupted;
} opj_tcd_cblk_dec_t;

typedef struct opj_tcd_precinct {

    OPJ_INT32 x0, y0, x1, y1;
    OPJ_UINT32 cw, ch;
    union {
        opj_tcd_cblk_enc_t* enc;
        opj_tcd_cblk_dec_t* dec;
        void*               blocks;
    } cblks;
    OPJ_UINT32 block_size;
    opj_tgt_tree_t *incltree;
    opj_tgt_tree_t *imsbtree;
} opj_tcd_precinct_t;

typedef struct opj_tcd_band {

    OPJ_INT32 x0, y0, x1, y1;

    OPJ_UINT32 bandno;

    opj_tcd_precinct_t *precincts;

    OPJ_UINT32 precincts_data_size;
    OPJ_INT32 numbps;
    OPJ_FLOAT32 stepsize;
} opj_tcd_band_t;

typedef struct opj_tcd_resolution {

    OPJ_INT32 x0, y0, x1, y1;

    OPJ_UINT32 pw, ph;

    OPJ_UINT32 numbands;

    opj_tcd_band_t bands[3];

    OPJ_UINT32 win_x0;
    OPJ_UINT32 win_y0;
    OPJ_UINT32 win_x1;
    OPJ_UINT32 win_y1;
} opj_tcd_resolution_t;

typedef struct opj_tcd_tilecomp {

    OPJ_INT32 x0, y0, x1, y1;

    OPJ_UINT32 compno;

    OPJ_UINT32 numresolutions;

    OPJ_UINT32 minimum_num_resolutions;

    opj_tcd_resolution_t *resolutions;

    OPJ_UINT32 resolutions_size;

    OPJ_INT32 *data;

    OPJ_BOOL  ownsData;

    size_t data_size_needed;

    size_t data_size;

    OPJ_INT32 *data_win;

    OPJ_UINT32 win_x0;
    OPJ_UINT32 win_y0;
    OPJ_UINT32 win_x1;
    OPJ_UINT32 win_y1;

    OPJ_SIZE_T numpix;
} opj_tcd_tilecomp_t;

typedef struct opj_tcd_tile {

    OPJ_INT32 x0, y0, x1, y1;
    OPJ_UINT32 numcomps;
    opj_tcd_tilecomp_t *comps;
    OPJ_SIZE_T numpix;
    OPJ_FLOAT64 distotile;
    OPJ_FLOAT64 distolayer[100];
    OPJ_UINT32 packno;
} opj_tcd_tile_t;

typedef struct opj_tcd_image {
    opj_tcd_tile_t *tiles;
}
opj_tcd_image_t;

typedef struct opj_tcd {

    OPJ_INT32 tp_pos;

    OPJ_UINT32 tp_num;

    OPJ_UINT32 cur_tp_num;

    OPJ_UINT32 cur_totnum_tp;

    OPJ_UINT32 cur_pino;

    opj_tcd_image_t *tcd_image;

    opj_image_t *image;

    opj_cp_t *cp;

    opj_tcp_t *tcp;

    OPJ_UINT32 tcd_tileno;

    OPJ_BITFIELD m_is_decoder : 1;

    opj_thread_pool_t* thread_pool;

    OPJ_UINT32 win_x0;
    OPJ_UINT32 win_y0;
    OPJ_UINT32 win_x1;
    OPJ_UINT32 win_y1;

    OPJ_BOOL   whole_tile_decoding;

    OPJ_BOOL* used_component;
} opj_tcd_t;

typedef struct opj_tcd_marker_info {

    OPJ_BOOL    need_PLT;

    OPJ_UINT32  packet_count;

    OPJ_UINT32* p_packet_size;
} opj_tcd_marker_info_t;

opj_tcd_t* opj_tcd_create(OPJ_BOOL p_is_decoder);

void opj_tcd_destroy(opj_tcd_t *tcd);

opj_tcd_marker_info_t* opj_tcd_marker_info_create(OPJ_BOOL need_PLT);

void opj_tcd_marker_info_destroy(opj_tcd_marker_info_t *p_tcd_marker_info);

OPJ_BOOL opj_tcd_init(opj_tcd_t *p_tcd,
                      opj_image_t * p_image,
                      opj_cp_t * p_cp,
                      opj_thread_pool_t* p_tp);

OPJ_BOOL opj_tcd_init_decode_tile(opj_tcd_t *p_tcd, OPJ_UINT32 p_tile_no,
                                  opj_event_mgr_t* p_manager);

OPJ_UINT32 opj_tcd_get_decoded_tile_size(opj_tcd_t *p_tcd,
        OPJ_BOOL take_into_account_partial_decoding);

OPJ_BOOL opj_tcd_encode_tile(opj_tcd_t *p_tcd,
                             OPJ_UINT32 p_tile_no,
                             OPJ_BYTE *p_dest,
                             OPJ_UINT32 * p_data_written,
                             OPJ_UINT32 p_len,
                             struct opj_codestream_info *p_cstr_info,
                             opj_tcd_marker_info_t* p_marker_info,
                             opj_event_mgr_t *p_manager);

OPJ_BOOL opj_tcd_decode_tile(opj_tcd_t *tcd,
                             OPJ_UINT32 win_x0,
                             OPJ_UINT32 win_y0,
                             OPJ_UINT32 win_x1,
                             OPJ_UINT32 win_y1,
                             OPJ_UINT32 numcomps_to_decode,
                             const OPJ_UINT32 *comps_indices,
                             OPJ_BYTE *src,
                             OPJ_UINT32 len,
                             OPJ_UINT32 tileno,
                             opj_codestream_index_t *cstr_info,
                             opj_event_mgr_t *manager);

OPJ_BOOL opj_tcd_update_tile_data(opj_tcd_t *p_tcd,
                                  OPJ_BYTE * p_dest,
                                  OPJ_UINT32 p_dest_length);

OPJ_SIZE_T opj_tcd_get_encoder_input_buffer_size(opj_tcd_t *p_tcd);

OPJ_BOOL opj_tcd_init_encode_tile(opj_tcd_t *p_tcd,
                                  OPJ_UINT32 p_tile_no, opj_event_mgr_t* p_manager);

OPJ_BOOL opj_tcd_copy_tile_data(opj_tcd_t *p_tcd,
                                OPJ_BYTE * p_src,
                                OPJ_SIZE_T p_src_length);

OPJ_BOOL opj_alloc_tile_component_data(opj_tcd_tilecomp_t *l_tilec);

OPJ_BOOL opj_tcd_is_band_empty(opj_tcd_band_t* band);

void opj_tcd_reinit_segment(opj_tcd_seg_t* seg);

OPJ_BOOL opj_tcd_is_subband_area_of_interest(opj_tcd_t *tcd,
        OPJ_UINT32 compno,
        OPJ_UINT32 resno,
        OPJ_UINT32 bandno,
        OPJ_UINT32 x0,
        OPJ_UINT32 y0,
        OPJ_UINT32 x1,
        OPJ_UINT32 y1);

#endif
