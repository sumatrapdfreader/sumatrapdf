#ifndef  CIDX_MANAGER_H_
# define CIDX_MANAGER_H_

#include "openjpeg.h"

int opj_write_cidx(int offset, opj_stream_private_t *cio,
                   opj_codestream_info_t cstr_info, int j2klen,
                   opj_event_mgr_t * p_manager);

OPJ_BOOL opj_check_EPHuse(int coff, opj_marker_info_t *markers, int marknum,
                          opj_stream_private_t *cio,
                          opj_event_mgr_t * p_manager);

#endif
