#ifndef  INDEXBOX_MANAGER_H_
# define INDEXBOX_MANAGER_H_

#include "openjpeg.h"
#include "j2k.h"
#include "jp2.h"

#define JPIP_CIDX 0x63696478
#define JPIP_CPTR 0x63707472
#define JPIP_MANF 0x6d616e66
#define JPIP_FAIX 0x66616978
#define JPIP_MHIX 0x6d686978
#define JPIP_TPIX 0x74706978
#define JPIP_THIX 0x74686978
#define JPIP_PPIX 0x70706978
#define JPIP_PHIX 0x70686978
#define JPIP_FIDX 0x66696478
#define JPIP_FPTR 0x66707472
#define JPIP_PRXY 0x70727879
#define JPIP_IPTR 0x69707472
#define JPIP_PHLD 0x70686c64

int opj_write_tpix(int coff, opj_codestream_info_t cstr_info, int j2klen,
                   opj_stream_private_t *cio,
                   opj_event_mgr_t * p_manager);

int opj_write_thix(int coff, opj_codestream_info_t cstr_info,
                   opj_stream_private_t *cio, opj_event_mgr_t * p_manager);

int opj_write_ppix(int coff, opj_codestream_info_t cstr_info, OPJ_BOOL EPHused,
                   int j2klen, opj_stream_private_t *cio,
                   opj_event_mgr_t * p_manager);

int opj_write_phix(int coff, opj_codestream_info_t cstr_info, OPJ_BOOL EPHused,
                   int j2klen, opj_stream_private_t *cio,
                   opj_event_mgr_t * p_manager);

void opj_write_manf(int second,
                    int v,
                    opj_jp2_box_t *box,
                    opj_stream_private_t *cio,
                    opj_event_mgr_t * p_manager);

int opj_write_mainmhix(int coff, opj_codestream_info_t cstr_info,
                       opj_stream_private_t *cio,
                       opj_event_mgr_t * p_manager);

int opj_write_phixfaix(int coff, int compno, opj_codestream_info_t cstr_info,
                       OPJ_BOOL EPHused, int j2klen, opj_stream_private_t *cio,
                       opj_event_mgr_t * p_manager);

int opj_write_ppixfaix(int coff, int compno, opj_codestream_info_t cstr_info,
                       OPJ_BOOL EPHused, int j2klen, opj_stream_private_t *cio,
                       opj_event_mgr_t * p_manager);

int opj_write_tilemhix(int coff, opj_codestream_info_t cstr_info, int tileno,
                       opj_stream_private_t *cio,
                       opj_event_mgr_t * p_manager);

int opj_write_tpixfaix(int coff, int compno, opj_codestream_info_t cstr_info,
                       int j2klen, opj_stream_private_t *cio,
                       opj_event_mgr_t * p_manager);

#endif
