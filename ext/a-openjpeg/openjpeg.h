#ifndef OPENJPEG_H
#define OPENJPEG_H

#ifndef INLINE
#if defined(_MSC_VER)
#define INLINE __forceinline
#elif defined(__GNUC__)
#define INLINE __inline__
#elif defined(__MWERKS__)
#define INLINE inline
#else

#define INLINE
#endif
#endif

#ifdef __GNUC__
#define OPJ_DEPRECATED(func) func __attribute__ ((deprecated))
#elif defined(_MSC_VER)
#define OPJ_DEPRECATED(func) __declspec(deprecated) func
#else
#pragma message("WARNING: You need to implement DEPRECATED for this compiler")
#define OPJ_DEPRECATED(func) func
#endif

#if defined(__GNUC__) && __GNUC__ >= 6
#define OPJ_DEPRECATED_STRUCT_MEMBER(memb, msg) __attribute__ ((deprecated(msg))) memb
#else
#define OPJ_DEPRECATED_STRUCT_MEMBER(memb, msg) memb
#endif

#if defined(OPJ_STATIC) || !defined(_WIN32)

#   if !defined(_WIN32) && __GNUC__ >= 4
#       if defined(OPJ_STATIC)
#           define OPJ_API    __attribute__ ((visibility ("hidden")))
#       else
#           define OPJ_API    __attribute__ ((visibility ("default")))
#       endif
#       define OPJ_LOCAL  __attribute__ ((visibility ("hidden")))
#   else
#       define OPJ_API
#       define OPJ_LOCAL
#   endif
#   define OPJ_CALLCONV
#else
#   define OPJ_CALLCONV __stdcall

#   if defined(OPJ_EXPORTS) || defined(DLL_EXPORT)
#       define OPJ_API __declspec(dllexport)
#   else
#       define OPJ_API __declspec(dllimport)
#   endif
#endif

typedef int OPJ_BOOL;
#define OPJ_TRUE 1
#define OPJ_FALSE 0

typedef char          OPJ_CHAR;
typedef float         OPJ_FLOAT32;
typedef double        OPJ_FLOAT64;
typedef unsigned char OPJ_BYTE;

#include <stdint.h>

typedef int8_t   OPJ_INT8;
typedef uint8_t  OPJ_UINT8;
typedef int16_t  OPJ_INT16;
typedef uint16_t OPJ_UINT16;
typedef int32_t  OPJ_INT32;
typedef uint32_t OPJ_UINT32;
typedef int64_t  OPJ_INT64;
typedef uint64_t OPJ_UINT64;

typedef int64_t  OPJ_OFF_T;

#include <stdio.h>
typedef size_t   OPJ_SIZE_T;

#include "opj_config.h"

#define OPJ_ARG_NOT_USED(x) (void)(x)

#define OPJ_PATH_LEN 4096

#define OPJ_J2K_MAXRLVLS 33
#define OPJ_J2K_MAXBANDS (3*OPJ_J2K_MAXRLVLS-2)

#define OPJ_J2K_DEFAULT_NB_SEGS             10
#define OPJ_J2K_STREAM_CHUNK_SIZE           0x100000
#define OPJ_J2K_DEFAULT_HEADER_SIZE         1000
#define OPJ_J2K_MCC_DEFAULT_NB_RECORDS      10
#define OPJ_J2K_MCT_DEFAULT_NB_RECORDS      10

#define JPWL_MAX_NO_TILESPECS   16
#define JPWL_MAX_NO_PACKSPECS   16
#define JPWL_MAX_NO_MARKERS 512
#define JPWL_PRIVATEINDEX_NAME "jpwl_index_privatefilename"
#define JPWL_EXPECTED_COMPONENTS 3
#define JPWL_MAXIMUM_TILES 8192
#define JPWL_MAXIMUM_HAMMING 2
#define JPWL_MAXIMUM_EPB_ROOM 65450

#define OPJ_IMG_INFO        1
#define OPJ_J2K_MH_INFO     2
#define OPJ_J2K_TH_INFO     4
#define OPJ_J2K_TCH_INFO    8
#define OPJ_J2K_MH_IND      16
#define OPJ_J2K_TH_IND      32

#define OPJ_JP2_INFO        128
#define OPJ_JP2_IND         256

#define OPJ_PROFILE_NONE        0x0000
#define OPJ_PROFILE_0           0x0001
#define OPJ_PROFILE_1           0x0002
#define OPJ_PROFILE_PART2       0x8000
#define OPJ_PROFILE_CINEMA_2K   0x0003
#define OPJ_PROFILE_CINEMA_4K   0x0004
#define OPJ_PROFILE_CINEMA_S2K  0x0005
#define OPJ_PROFILE_CINEMA_S4K  0x0006
#define OPJ_PROFILE_CINEMA_LTS  0x0007
#define OPJ_PROFILE_BC_SINGLE   0x0100
#define OPJ_PROFILE_BC_MULTI    0x0200
#define OPJ_PROFILE_BC_MULTI_R  0x0300
#define OPJ_PROFILE_IMF_2K      0x0400
#define OPJ_PROFILE_IMF_4K      0x0500
#define OPJ_PROFILE_IMF_8K      0x0600
#define OPJ_PROFILE_IMF_2K_R    0x0700
#define OPJ_PROFILE_IMF_4K_R    0x0800
#define OPJ_PROFILE_IMF_8K_R    0x0900

#define OPJ_EXTENSION_NONE      0x0000
#define OPJ_EXTENSION_MCT       0x0100

#define OPJ_IS_CINEMA(v)     (((v) >= OPJ_PROFILE_CINEMA_2K)&&((v) <= OPJ_PROFILE_CINEMA_S4K))
#define OPJ_IS_STORAGE(v)    ((v) == OPJ_PROFILE_CINEMA_LTS)
#define OPJ_IS_BROADCAST(v)  (((v) >= OPJ_PROFILE_BC_SINGLE)&&((v) <= ((OPJ_PROFILE_BC_MULTI_R) | (0x000b))))
#define OPJ_IS_IMF(v)        (((v) >= OPJ_PROFILE_IMF_2K)&&((v) <= ((OPJ_PROFILE_IMF_8K_R) | (0x009b))))
#define OPJ_IS_PART2(v)      ((v) & OPJ_PROFILE_PART2)

#define OPJ_GET_IMF_PROFILE(v)   ((v) & 0xff00)
#define OPJ_GET_IMF_MAINLEVEL(v) ((v) & 0xf)
#define OPJ_GET_IMF_SUBLEVEL(v)  (((v) >> 4) & 0xf)

#define OPJ_IMF_MAINLEVEL_MAX    11

#define OPJ_IMF_MAINLEVEL_1_MSAMPLESEC   65
#define OPJ_IMF_MAINLEVEL_2_MSAMPLESEC   130
#define OPJ_IMF_MAINLEVEL_3_MSAMPLESEC   195
#define OPJ_IMF_MAINLEVEL_4_MSAMPLESEC   260
#define OPJ_IMF_MAINLEVEL_5_MSAMPLESEC   520
#define OPJ_IMF_MAINLEVEL_6_MSAMPLESEC   1200
#define OPJ_IMF_MAINLEVEL_7_MSAMPLESEC   2400
#define OPJ_IMF_MAINLEVEL_8_MSAMPLESEC   4800
#define OPJ_IMF_MAINLEVEL_9_MSAMPLESEC   9600
#define OPJ_IMF_MAINLEVEL_10_MSAMPLESEC  19200
#define OPJ_IMF_MAINLEVEL_11_MSAMPLESEC  38400

#define OPJ_IMF_SUBLEVEL_1_MBITSSEC      200
#define OPJ_IMF_SUBLEVEL_2_MBITSSEC      400
#define OPJ_IMF_SUBLEVEL_3_MBITSSEC      800
#define OPJ_IMF_SUBLEVEL_4_MBITSSEC     1600
#define OPJ_IMF_SUBLEVEL_5_MBITSSEC     3200
#define OPJ_IMF_SUBLEVEL_6_MBITSSEC     6400
#define OPJ_IMF_SUBLEVEL_7_MBITSSEC    12800
#define OPJ_IMF_SUBLEVEL_8_MBITSSEC    25600
#define OPJ_IMF_SUBLEVEL_9_MBITSSEC    51200

#define OPJ_CINEMA_24_CS     1302083
#define OPJ_CINEMA_48_CS     651041
#define OPJ_CINEMA_24_COMP   1041666
#define OPJ_CINEMA_48_COMP   520833

typedef enum RSIZ_CAPABILITIES {
    OPJ_STD_RSIZ = 0,
    OPJ_CINEMA2K = 3,
    OPJ_CINEMA4K = 4,
    OPJ_MCT = 0x8100
} OPJ_RSIZ_CAPABILITIES;

typedef enum CINEMA_MODE {
    OPJ_OFF = 0,
    OPJ_CINEMA2K_24 = 1,
    OPJ_CINEMA2K_48 = 2,
    OPJ_CINEMA4K_24 = 3
} OPJ_CINEMA_MODE;

typedef enum PROG_ORDER {
    OPJ_PROG_UNKNOWN = -1,
    OPJ_LRCP = 0,
    OPJ_RLCP = 1,
    OPJ_RPCL = 2,
    OPJ_PCRL = 3,
    OPJ_CPRL = 4
} OPJ_PROG_ORDER;

typedef enum COLOR_SPACE {
    OPJ_CLRSPC_UNKNOWN = -1,
    OPJ_CLRSPC_UNSPECIFIED = 0,
    OPJ_CLRSPC_SRGB = 1,
    OPJ_CLRSPC_GRAY = 2,
    OPJ_CLRSPC_SYCC = 3,
    OPJ_CLRSPC_EYCC = 4,
    OPJ_CLRSPC_CMYK = 5
} OPJ_COLOR_SPACE;

typedef enum CODEC_FORMAT {
    OPJ_CODEC_UNKNOWN = -1,
    OPJ_CODEC_J2K  = 0,
    OPJ_CODEC_JPT  = 1,
    OPJ_CODEC_JP2  = 2,
    OPJ_CODEC_JPP  = 3,
    OPJ_CODEC_JPX  = 4
} OPJ_CODEC_FORMAT;

typedef void (*opj_msg_callback)(const char *msg, void *client_data);

#ifndef OPJ_UINT32_SEMANTICALLY_BUT_INT32
#define OPJ_UINT32_SEMANTICALLY_BUT_INT32 OPJ_INT32
#endif

typedef struct opj_poc {

    OPJ_UINT32 resno0, compno0;

    OPJ_UINT32 layno1, resno1, compno1;

    OPJ_UINT32 layno0, precno0, precno1;

    OPJ_PROG_ORDER prg1, prg;

    OPJ_CHAR progorder[5];

    OPJ_UINT32 tile;

    OPJ_UINT32_SEMANTICALLY_BUT_INT32 tx0, tx1, ty0, ty1;

    OPJ_UINT32 layS, resS, compS, prcS;

    OPJ_UINT32 layE, resE, compE, prcE;

    OPJ_UINT32 txS, txE, tyS, tyE, dx, dy;

    OPJ_UINT32 lay_t, res_t, comp_t, prc_t, tx0_t, ty0_t;
} opj_poc_t;

typedef struct opj_cparameters {

    OPJ_BOOL tile_size_on;

    int cp_tx0;

    int cp_ty0;

    int cp_tdx;

    int cp_tdy;

    int cp_disto_alloc;

    int cp_fixed_alloc;

    int cp_fixed_quality;

    int *cp_matrice;

    char *cp_comment;

    int csty;

    OPJ_PROG_ORDER prog_order;

    opj_poc_t POC[32];

    OPJ_UINT32 numpocs;

    int tcp_numlayers;

    float tcp_rates[100];

    float tcp_distoratio[100];

    int numresolution;

    int cblockw_init;

    int cblockh_init;

    int mode;

    int irreversible;

    int roi_compno;

    int roi_shift;

    int res_spec;

    int prcw_init[OPJ_J2K_MAXRLVLS];

    int prch_init[OPJ_J2K_MAXRLVLS];

    char infile[OPJ_PATH_LEN];

    char outfile[OPJ_PATH_LEN];

    int index_on;

    char index[OPJ_PATH_LEN];

    int image_offset_x0;

    int image_offset_y0;

    int subsampling_dx;

    int subsampling_dy;

    int decod_format;

    int cod_format;

    OPJ_BOOL jpwl_epc_on;

    int jpwl_hprot_MH;

    int jpwl_hprot_TPH_tileno[JPWL_MAX_NO_TILESPECS];

    int jpwl_hprot_TPH[JPWL_MAX_NO_TILESPECS];

    int jpwl_pprot_tileno[JPWL_MAX_NO_PACKSPECS];

    int jpwl_pprot_packno[JPWL_MAX_NO_PACKSPECS];

    int jpwl_pprot[JPWL_MAX_NO_PACKSPECS];

    int jpwl_sens_size;

    int jpwl_sens_addr;

    int jpwl_sens_range;

    int jpwl_sens_MH;

    int jpwl_sens_TPH_tileno[JPWL_MAX_NO_TILESPECS];

    int jpwl_sens_TPH[JPWL_MAX_NO_TILESPECS];

    OPJ_CINEMA_MODE cp_cinema;

    int max_comp_size;

    OPJ_RSIZ_CAPABILITIES cp_rsiz;

    char tp_on;

    char tp_flag;

    char tcp_mct;

    OPJ_BOOL jpip_on;

    void * mct_data;

    int max_cs_size;

    OPJ_UINT16 rsiz;
} opj_cparameters_t;

#define OPJ_DPARAMETERS_IGNORE_PCLR_CMAP_CDEF_FLAG  0x0001
#define OPJ_DPARAMETERS_DUMP_FLAG                   0x0002

typedef struct opj_dparameters {

    OPJ_UINT32 cp_reduce;

    OPJ_UINT32 cp_layer;

    char infile[OPJ_PATH_LEN];

    char outfile[OPJ_PATH_LEN];

    int decod_format;

    int cod_format;

    OPJ_UINT32 DA_x0;

    OPJ_UINT32 DA_x1;

    OPJ_UINT32 DA_y0;

    OPJ_UINT32 DA_y1;

    OPJ_BOOL m_verbose;

    OPJ_UINT32 tile_index;

    OPJ_UINT32 nb_tile_to_decode;

    OPJ_BOOL jpwl_correct;

    int jpwl_exp_comps;

    int jpwl_max_tiles;

    unsigned int flags;

} opj_dparameters_t;

typedef void * opj_codec_t;

#define OPJ_STREAM_READ OPJ_TRUE

#define OPJ_STREAM_WRITE OPJ_FALSE

typedef OPJ_SIZE_T(* opj_stream_read_fn)(void * p_buffer, OPJ_SIZE_T p_nb_bytes,
        void * p_user_data) ;

typedef OPJ_SIZE_T(* opj_stream_write_fn)(void * p_buffer,
        OPJ_SIZE_T p_nb_bytes, void * p_user_data) ;

typedef OPJ_OFF_T(* opj_stream_skip_fn)(OPJ_OFF_T p_nb_bytes,
                                        void * p_user_data) ;

typedef OPJ_BOOL(* opj_stream_seek_fn)(OPJ_OFF_T p_nb_bytes,
                                       void * p_user_data) ;

typedef void (* opj_stream_free_user_data_fn)(void * p_user_data) ;

typedef void * opj_stream_t;

typedef struct opj_image_comp {

    OPJ_UINT32 dx;

    OPJ_UINT32 dy;

    OPJ_UINT32 w;

    OPJ_UINT32 h;

    OPJ_UINT32 x0;

    OPJ_UINT32 y0;

    OPJ_UINT32 prec;

    OPJ_DEPRECATED_STRUCT_MEMBER(OPJ_UINT32 bpp, "Use prec instead");

    OPJ_UINT32 sgnd;

    OPJ_UINT32 resno_decoded;

    OPJ_UINT32 factor;

    OPJ_INT32 *data;

    OPJ_UINT16 alpha;
} opj_image_comp_t;

typedef struct opj_image {

    OPJ_UINT32 x0;

    OPJ_UINT32 y0;

    OPJ_UINT32 x1;

    OPJ_UINT32 y1;

    OPJ_UINT32 numcomps;

    OPJ_COLOR_SPACE color_space;

    opj_image_comp_t *comps;

    OPJ_BYTE *icc_profile_buf;

    OPJ_UINT32 icc_profile_len;
} opj_image_t;

typedef struct opj_image_comptparm {

    OPJ_UINT32 dx;

    OPJ_UINT32 dy;

    OPJ_UINT32 w;

    OPJ_UINT32 h;

    OPJ_UINT32 x0;

    OPJ_UINT32 y0;

    OPJ_UINT32 prec;

    OPJ_DEPRECATED_STRUCT_MEMBER(OPJ_UINT32 bpp, "Use prec instead");

    OPJ_UINT32 sgnd;
} opj_image_cmptparm_t;

typedef struct opj_packet_info {

    OPJ_OFF_T start_pos;

    OPJ_OFF_T end_ph_pos;

    OPJ_OFF_T end_pos;

    double disto;
} opj_packet_info_t;

typedef struct opj_marker_info {

    unsigned short int type;

    OPJ_OFF_T pos;

    int len;
} opj_marker_info_t;

typedef struct opj_tp_info {

    int tp_start_pos;

    int tp_end_header;

    int tp_end_pos;

    int tp_start_pack;

    int tp_numpacks;
} opj_tp_info_t;

typedef struct opj_tile_info {

    double *thresh;

    int tileno;

    int start_pos;

    int end_header;

    int end_pos;

    int pw[33];

    int ph[33];

    int pdx[33];

    int pdy[33];

    opj_packet_info_t *packet;

    int numpix;

    double distotile;

    int marknum;

    opj_marker_info_t *marker;

    int maxmarknum;

    int num_tps;

    opj_tp_info_t *tp;
} opj_tile_info_t;

typedef struct opj_codestream_info {

    double D_max;

    int packno;

    int index_write;

    int image_w;

    int image_h;

    OPJ_PROG_ORDER prog;

    int tile_x;

    int tile_y;

    int tile_Ox;

    int tile_Oy;

    int tw;

    int th;

    int numcomps;

    int numlayers;

    int *numdecompos;

    int marknum;

    opj_marker_info_t *marker;

    int maxmarknum;

    int main_head_start;

    int main_head_end;

    int codestream_size;

    opj_tile_info_t *tile;
} opj_codestream_info_t;

typedef struct opj_tccp_info {

    OPJ_UINT32 compno;

    OPJ_UINT32 csty;

    OPJ_UINT32 numresolutions;

    OPJ_UINT32 cblkw;

    OPJ_UINT32 cblkh;

    OPJ_UINT32 cblksty;

    OPJ_UINT32 qmfbid;

    OPJ_UINT32 qntsty;

    OPJ_UINT32 stepsizes_mant[OPJ_J2K_MAXBANDS];

    OPJ_UINT32 stepsizes_expn[OPJ_J2K_MAXBANDS];

    OPJ_UINT32 numgbits;

    OPJ_INT32 roishift;

    OPJ_UINT32 prcw[OPJ_J2K_MAXRLVLS];

    OPJ_UINT32 prch[OPJ_J2K_MAXRLVLS];
}
opj_tccp_info_t;

typedef struct opj_tile_v2_info {

    int tileno;

    OPJ_UINT32 csty;

    OPJ_PROG_ORDER prg;

    OPJ_UINT32 numlayers;

    OPJ_UINT32 mct;

    opj_tccp_info_t *tccp_info;

} opj_tile_info_v2_t;

typedef struct opj_codestream_info_v2 {

    OPJ_UINT32 tx0;

    OPJ_UINT32 ty0;

    OPJ_UINT32 tdx;

    OPJ_UINT32 tdy;

    OPJ_UINT32 tw;

    OPJ_UINT32 th;

    OPJ_UINT32 nbcomps;

    opj_tile_info_v2_t m_default_tile_info;

    opj_tile_info_v2_t *tile_info;

} opj_codestream_info_v2_t;

typedef struct opj_tp_index {

    OPJ_OFF_T start_pos;

    OPJ_OFF_T end_header;

    OPJ_OFF_T end_pos;

} opj_tp_index_t;

typedef struct opj_tile_index {

    OPJ_UINT32 tileno;

    OPJ_UINT32 nb_tps;

    OPJ_UINT32 current_nb_tps;

    OPJ_UINT32 current_tpsno;

    opj_tp_index_t *tp_index;

    OPJ_UINT32 marknum;

    opj_marker_info_t *marker;

    OPJ_UINT32 maxmarknum;

    OPJ_UINT32 nb_packet;

    opj_packet_info_t *packet_index;

} opj_tile_index_t;

typedef struct opj_codestream_index {

    OPJ_OFF_T main_head_start;

    OPJ_OFF_T main_head_end;

    OPJ_UINT64 codestream_size;

    OPJ_UINT32 marknum;

    opj_marker_info_t *marker;

    OPJ_UINT32 maxmarknum;

    OPJ_UINT32 nb_of_tiles;

    opj_tile_index_t *tile_index;

} opj_codestream_index_t;

typedef struct opj_jp2_metadata {

    OPJ_INT32   not_used;

} opj_jp2_metadata_t;

typedef struct opj_jp2_index {

    OPJ_INT32   not_used;

} opj_jp2_index_t;

#ifdef __cplusplus
extern "C" {
#endif

OPJ_API const char * OPJ_CALLCONV opj_version(void);

OPJ_API opj_image_t* OPJ_CALLCONV opj_image_create(OPJ_UINT32 numcmpts,
        opj_image_cmptparm_t *cmptparms, OPJ_COLOR_SPACE clrspc);

OPJ_API void OPJ_CALLCONV opj_image_destroy(opj_image_t *image);

OPJ_API opj_image_t* OPJ_CALLCONV opj_image_tile_create(OPJ_UINT32 numcmpts,
        opj_image_cmptparm_t *cmptparms, OPJ_COLOR_SPACE clrspc);

OPJ_API void* OPJ_CALLCONV opj_image_data_alloc(OPJ_SIZE_T size);

OPJ_API void OPJ_CALLCONV opj_image_data_free(void* ptr);

OPJ_API opj_stream_t* OPJ_CALLCONV opj_stream_default_create(
    OPJ_BOOL p_is_input);

OPJ_API opj_stream_t* OPJ_CALLCONV opj_stream_create(OPJ_SIZE_T p_buffer_size,
        OPJ_BOOL p_is_input);

OPJ_API void OPJ_CALLCONV opj_stream_destroy(opj_stream_t* p_stream);

OPJ_API void OPJ_CALLCONV opj_stream_set_read_function(opj_stream_t* p_stream,
        opj_stream_read_fn p_function);

OPJ_API void OPJ_CALLCONV opj_stream_set_write_function(opj_stream_t* p_stream,
        opj_stream_write_fn p_function);

OPJ_API void OPJ_CALLCONV opj_stream_set_skip_function(opj_stream_t* p_stream,
        opj_stream_skip_fn p_function);

OPJ_API void OPJ_CALLCONV opj_stream_set_seek_function(opj_stream_t* p_stream,
        opj_stream_seek_fn p_function);

OPJ_API void OPJ_CALLCONV opj_stream_set_user_data(opj_stream_t* p_stream,
        void * p_data, opj_stream_free_user_data_fn p_function);

OPJ_API void OPJ_CALLCONV opj_stream_set_user_data_length(
    opj_stream_t* p_stream, OPJ_UINT64 data_length);

OPJ_API opj_stream_t* OPJ_CALLCONV opj_stream_create_default_file_stream(
    const char *fname, OPJ_BOOL p_is_read_stream);

OPJ_API opj_stream_t* OPJ_CALLCONV opj_stream_create_file_stream(
    const char *fname,
    OPJ_SIZE_T p_buffer_size,
    OPJ_BOOL p_is_read_stream);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_set_info_handler(opj_codec_t * p_codec,
        opj_msg_callback p_callback,
        void * p_user_data);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_set_warning_handler(opj_codec_t * p_codec,
        opj_msg_callback p_callback,
        void * p_user_data);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_set_error_handler(opj_codec_t * p_codec,
        opj_msg_callback p_callback,
        void * p_user_data);

OPJ_API opj_codec_t* OPJ_CALLCONV opj_create_decompress(
    OPJ_CODEC_FORMAT format);

OPJ_API void OPJ_CALLCONV opj_destroy_codec(opj_codec_t * p_codec);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_end_decompress(opj_codec_t *p_codec,
        opj_stream_t *p_stream);

OPJ_API void OPJ_CALLCONV opj_set_default_decoder_parameters(
    opj_dparameters_t *parameters);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_setup_decoder(opj_codec_t *p_codec,
        opj_dparameters_t *parameters);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_decoder_set_strict_mode(opj_codec_t *p_codec,
        OPJ_BOOL strict);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_codec_set_threads(opj_codec_t *p_codec,
        int num_threads);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_read_header(opj_stream_t *p_stream,
        opj_codec_t *p_codec,
        opj_image_t **p_image);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_set_decoded_components(opj_codec_t *p_codec,
        OPJ_UINT32 numcomps,
        const OPJ_UINT32* comps_indices,
        OPJ_BOOL apply_color_transforms);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_set_decode_area(opj_codec_t *p_codec,
        opj_image_t* p_image,
        OPJ_INT32 p_start_x, OPJ_INT32 p_start_y,
        OPJ_INT32 p_end_x, OPJ_INT32 p_end_y);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_decode(opj_codec_t *p_decompressor,
        opj_stream_t *p_stream,
        opj_image_t *p_image);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_get_decoded_tile(opj_codec_t *p_codec,
        opj_stream_t *p_stream,
        opj_image_t *p_image,
        OPJ_UINT32 tile_index);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_set_decoded_resolution_factor(
    opj_codec_t *p_codec, OPJ_UINT32 res_factor);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_write_tile(opj_codec_t *p_codec,
        OPJ_UINT32 p_tile_index,
        OPJ_BYTE * p_data,
        OPJ_UINT32 p_data_size,
        opj_stream_t *p_stream);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_read_tile_header(opj_codec_t *p_codec,
        opj_stream_t * p_stream,
        OPJ_UINT32 * p_tile_index,
        OPJ_UINT32 * p_data_size,
        OPJ_INT32 * p_tile_x0, OPJ_INT32 * p_tile_y0,
        OPJ_INT32 * p_tile_x1, OPJ_INT32 * p_tile_y1,
        OPJ_UINT32 * p_nb_comps,
        OPJ_BOOL * p_should_go_on);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_decode_tile_data(opj_codec_t *p_codec,
        OPJ_UINT32 p_tile_index,
        OPJ_BYTE * p_data,
        OPJ_UINT32 p_data_size,
        opj_stream_t *p_stream);

OPJ_API opj_codec_t* OPJ_CALLCONV opj_create_compress(OPJ_CODEC_FORMAT format);

OPJ_API void OPJ_CALLCONV opj_set_default_encoder_parameters(
    opj_cparameters_t *parameters);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_setup_encoder(opj_codec_t *p_codec,
        opj_cparameters_t *parameters,
        opj_image_t *image);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_encoder_set_extra_options(
    opj_codec_t *p_codec,
    const char* const* p_options);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_start_compress(opj_codec_t *p_codec,
        opj_image_t * p_image,
        opj_stream_t *p_stream);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_end_compress(opj_codec_t *p_codec,
        opj_stream_t *p_stream);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_encode(opj_codec_t *p_codec,
        opj_stream_t *p_stream);

OPJ_API void OPJ_CALLCONV opj_destroy_cstr_info(opj_codestream_info_v2_t
        **cstr_info);

OPJ_API void OPJ_CALLCONV opj_dump_codec(opj_codec_t *p_codec,
        OPJ_INT32 info_flag,
        FILE* output_stream);

OPJ_API opj_codestream_info_v2_t* OPJ_CALLCONV opj_get_cstr_info(
    opj_codec_t *p_codec);

OPJ_API opj_codestream_index_t * OPJ_CALLCONV opj_get_cstr_index(
    opj_codec_t *p_codec);

OPJ_API void OPJ_CALLCONV opj_destroy_cstr_index(opj_codestream_index_t
        **p_cstr_index);

OPJ_API opj_jp2_metadata_t* OPJ_CALLCONV opj_get_jp2_metadata(
    opj_codec_t *p_codec);

OPJ_API opj_jp2_index_t* OPJ_CALLCONV opj_get_jp2_index(opj_codec_t *p_codec);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_set_MCT(opj_cparameters_t *parameters,
        OPJ_FLOAT32 * pEncodingMatrix,
        OPJ_INT32 * p_dc_shift,
        OPJ_UINT32 pNbComp);

OPJ_API OPJ_BOOL OPJ_CALLCONV opj_has_thread_support(void);

OPJ_API int OPJ_CALLCONV opj_get_num_cpus(void);

#ifdef __cplusplus
}
#endif

#endif
