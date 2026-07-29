#ifndef OPJ_CODEC_H
#define OPJ_CODEC_H

typedef struct opj_codec_private {

    union {

        struct opj_decompression {

            OPJ_BOOL(*opj_read_header)(struct opj_stream_private * cio,
                                       void * p_codec,
                                       opj_image_t **p_image,
                                       struct opj_event_mgr * p_manager);

            OPJ_BOOL(*opj_decode)(void * p_codec,
                                  struct opj_stream_private * p_cio,
                                  opj_image_t * p_image,
                                  struct opj_event_mgr * p_manager);

            OPJ_BOOL(*opj_read_tile_header)(void * p_codec,
                                            OPJ_UINT32 * p_tile_index,
                                            OPJ_UINT32 * p_data_size,
                                            OPJ_INT32 * p_tile_x0,
                                            OPJ_INT32 * p_tile_y0,
                                            OPJ_INT32 * p_tile_x1,
                                            OPJ_INT32 * p_tile_y1,
                                            OPJ_UINT32 * p_nb_comps,
                                            OPJ_BOOL * p_should_go_on,
                                            struct opj_stream_private * p_cio,
                                            struct opj_event_mgr * p_manager);

            OPJ_BOOL(*opj_decode_tile_data)(void * p_codec,
                                            OPJ_UINT32 p_tile_index,
                                            OPJ_BYTE * p_data,
                                            OPJ_UINT32 p_data_size,
                                            struct opj_stream_private * p_cio,
                                            struct opj_event_mgr * p_manager);

            OPJ_BOOL(* opj_end_decompress)(void *p_codec,
                                           struct opj_stream_private * cio,
                                           struct opj_event_mgr * p_manager);

            void (*opj_destroy)(void * p_codec);

            void (*opj_setup_decoder)(void * p_codec, opj_dparameters_t * p_param);

            void (*opj_decoder_set_strict_mode)(void * p_codec, OPJ_BOOL strict);

            OPJ_BOOL(*opj_set_decode_area)(void * p_codec,
                                           opj_image_t * p_image,
                                           OPJ_INT32 p_start_x,
                                           OPJ_INT32 p_end_x,
                                           OPJ_INT32 p_start_y,
                                           OPJ_INT32 p_end_y,
                                           struct opj_event_mgr * p_manager);

            OPJ_BOOL(*opj_get_decoded_tile)(void *p_codec,
                                            opj_stream_private_t * p_cio,
                                            opj_image_t *p_image,
                                            struct opj_event_mgr * p_manager,
                                            OPJ_UINT32 tile_index);

            OPJ_BOOL(*opj_set_decoded_resolution_factor)(void * p_codec,
                    OPJ_UINT32 res_factor,
                    opj_event_mgr_t * p_manager);

            OPJ_BOOL(*opj_set_decoded_components)(void * p_codec,
                                                  OPJ_UINT32 num_comps,
                                                  const OPJ_UINT32* comps_indices,
                                                  opj_event_mgr_t * p_manager);
        } m_decompression;

        struct opj_compression {
            OPJ_BOOL(* opj_start_compress)(void *p_codec,
                                           struct opj_stream_private * cio,
                                           struct opj_image * p_image,
                                           struct opj_event_mgr * p_manager);

            OPJ_BOOL(* opj_encode)(void * p_codec,
                                   struct opj_stream_private *p_cio,
                                   struct opj_event_mgr * p_manager);

            OPJ_BOOL(* opj_write_tile)(void * p_codec,
                                       OPJ_UINT32 p_tile_index,
                                       OPJ_BYTE * p_data,
                                       OPJ_UINT32 p_data_size,
                                       struct opj_stream_private * p_cio,
                                       struct opj_event_mgr * p_manager);

            OPJ_BOOL(* opj_end_compress)(void * p_codec,
                                         struct opj_stream_private * p_cio,
                                         struct opj_event_mgr * p_manager);

            void (* opj_destroy)(void * p_codec);

            OPJ_BOOL(* opj_setup_encoder)(void * p_codec,
                                          opj_cparameters_t * p_param,
                                          struct opj_image * p_image,
                                          struct opj_event_mgr * p_manager);

            OPJ_BOOL(* opj_encoder_set_extra_options)(void * p_codec,
                    const char* const* p_options,
                    struct opj_event_mgr * p_manager);

        } m_compression;
    } m_codec_data;

    void * m_codec;

    opj_event_mgr_t m_event_mgr;

    OPJ_BOOL is_decompressor;
    void (*opj_dump_codec)(void * p_codec, OPJ_INT32 info_flag,
                           FILE* output_stream);
    opj_codestream_info_v2_t* (*opj_get_codec_info)(void* p_codec);
    opj_codestream_index_t* (*opj_get_codec_index)(void* p_codec);

    OPJ_BOOL(*opj_set_threads)(void * p_codec, OPJ_UINT32 num_threads);
}
opj_codec_private_t;

#endif
