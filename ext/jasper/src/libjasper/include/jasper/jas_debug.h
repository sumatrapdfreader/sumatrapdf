/*
 * Copyright (c) 2001-2002 Michael David Adams.
 * Copyright (c) 2005-2006 artofcode LLC.
 * All rights reserved.
 */

/* __START_OF_JASPER_LICENSE__
 * 
 * JasPer License Version 2.0
 * 
 * Copyright (c) 1999-2000 Image Power, Inc.
 * Copyright (c) 1999-2000 The University of British Columbia
 * Copyright (c) 2001-2003 Michael David Adams
 * 
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person (the
 * "User") obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 * 
 * 1.  The above copyright notices and this permission notice (which
 * includes the disclaimer below) shall be included in all copies or
 * substantial portions of the Software.
 * 
 * 2.  The name of a copyright holder shall not be used to endorse or
 * promote products derived from the Software without specific prior
 * written permission.
 * 
 * THIS DISCLAIMER OF WARRANTY CONSTITUTES AN ESSENTIAL PART OF THIS
 * LICENSE.  NO USE OF THE SOFTWARE IS AUTHORIZED HEREUNDER EXCEPT UNDER
 * THIS DISCLAIMER.  THE SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
 * INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  NO ASSURANCES ARE
 * PROVIDED BY THE COPYRIGHT HOLDERS THAT THE SOFTWARE DOES NOT INFRINGE
 * THE PATENT OR OTHER INTELLECTUAL PROPERTY RIGHTS OF ANY OTHER ENTITY.
 * EACH COPYRIGHT HOLDER DISCLAIMS ANY LIABILITY TO THE USER FOR CLAIMS
 * BROUGHT BY ANY OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL
 * PROPERTY RIGHTS OR OTHERWISE.  AS A CONDITION TO EXERCISING THE RIGHTS
 * GRANTED HEREUNDER, EACH USER HEREBY ASSUMES SOLE RESPONSIBILITY TO SECURE
 * ANY OTHER INTELLECTUAL PROPERTY RIGHTS NEEDED, IF ANY.  THE SOFTWARE
 * IS NOT FAULT-TOLERANT AND IS NOT INTENDED FOR USE IN MISSION-CRITICAL
 * SYSTEMS, SUCH AS THOSE USED IN THE OPERATION OF NUCLEAR FACILITIES,
 * AIRCRAFT NAVIGATION OR COMMUNICATION SYSTEMS, AIR TRAFFIC CONTROL
 * SYSTEMS, DIRECT LIFE SUPPORT MACHINES, OR WEAPONS SYSTEMS, IN WHICH
 * THE FAILURE OF THE SOFTWARE OR SYSTEM COULD LEAD DIRECTLY TO DEATH,
 * PERSONAL INJURY, OR SEVERE PHYSICAL OR ENVIRONMENTAL DAMAGE ("HIGH
 * RISK ACTIVITIES").  THE COPYRIGHT HOLDERS SPECIFICALLY DISCLAIM ANY
 * EXPRESS OR IMPLIED WARRANTY OF FITNESS FOR HIGH RISK ACTIVITIES.
 * 
 * __END_OF_JASPER_LICENSE__
 */

/*
 * Debugging-Related Code
 *
 * $Id$
 */

#ifndef JAS_DEBUG_H
#define JAS_DEBUG_H

/******************************************************************************\
* Includes.
\******************************************************************************/

#include <stdio.h>

#include <jasper/jas_config.h>
#include "jasper/jas_types.h"
#include "jasper/jas_debug.h"

#ifdef __cplusplus
extern "C" {
#endif

/* defined error codes passed to the error callback */
typedef enum
{
	JAS_ERR_STD_ERR_WARNING,
	JAS_ERR_UNEQUAL_PARMS_IN_JAS_CMPXFORMSEQ_APPENDCNVT,
	JAS_ERR_CLR_SPACE_UNKNOWN_IN_ICCTOCLRSPC,
	JAS_ERR_UNSUPPORTED_COLOR_SPACE_IN_JAS_CLRSPC_NUMCHANS,
	JAS_ERR_UNEXPECTED_DATA_IN_JAS_ICCPROF_LOAD,
	JAS_ERR_LEN_NOT_12_IN_JAS_ICCXYZ_INPUT,
	JAS_ERR_INCOMPLETE_STUB_INVOKED_JAS_ICCCURV_COPY,
	JAS_ERR_INCOMPLETE_STUB_INVOKED_JAS_ICCLUT8_COPY,
	JAS_ERR_INCOMPLETE_STUB_INVOKED_JAS_ICCLUT16_COPY,
	JAS_ERR_BAD_PARAM_JAS_ICCPUTSINT,
	JAS_ERR_JAS_IMAGE_DUMP_FAILS,
	JAS_ERR_INVALID_PARAM_GETINT,
	JAS_ERR_INVALID_PARAM_PUTINT,
	JAS_ERR_EOF_ENCOUNTERED_JAS_STREAM_DISPLAY,
	JAS_ERR_INVALID_PARAM_MEM_SEEK,
	JAS_ERR_UNSUPPORTED_BIT_DEPTH_BMP_NUMCPTS,
	JAS_ERR_UNSUPPORTED_CLRSPC_BMP_ENCODE,
	JAS_ERR_UNSUPPORTED_NUMCMPTS_BMP_PUTDATA,
	JAS_ERR_CALL_TO_INVALID_STUB_JP2_GETUINT64,
	JAS_ERR_ICCPROF_SAVE_ERROR_JP2_ENCODE,
	JAS_ERR_STREAM_TELL_FAILURE_JP2_ENCODE,
	JAS_ERR_STREAM_READ_FAILURE_JP2_ENCODE,
	JAS_ERR_UNSUPPORTED_COLOR_SPACE_JP2_ENCODE,
	JAS_ERR_UNSUPPORTED_COLOR_SPACE_CLRSPCTOJP2,
	JAS_ERR_UNSUPPORTED_BITSTREAM_MODE_JPC_BITSTREAM_ALIGN,
	JAS_ERR_FAILED_PPM_MARKER_SEGMENT_CONVERSION,
	JAS_ERR_INVALID_STREAM_DELETE,
	JAS_ERR_BAD_PARAM_JPC_ABSTORELSTEPSIZE,
	JAS_ERR_NULL_DATA_PTR_JPC_ENC_ENCODEMAINHDR,
	JAS_ERR_TILE_CREATION_ERROR_JPC_ENC_ENCODEMAINBODY,
	JAS_ERR_FAILURE_RATEALLOCATE,
	JAS_ERR_STREAM_SEEK_FAILURE_RATEALLOCATE,
	JAS_ERR_UNSUPPORTED_TERMMODE_JPC_MQENC_FLUSH,
	JAS_ERR_MALLOC_FAILURE_JPC_QMFB1D_SPLIT,
	JAS_ERR_MALLOC_FAILURE_JPC_QMFB1D_JOIN,
	JAS_ERR_INCOMPLETE_STUB_INVOKED_JPC_FT_GETANALFILTERS,
	JAS_ERR_INVALID_LEN_PARAM_JPC_FT_GETSYNFILTERS,
	JAS_ERR_INCOMPLETE_STUB_INVOKED_JPC_NS_GETANALFILTERS,
	JAS_ERR_INVALID_LEN_PARAM_JPC_NS_GETSYNFILTERS,
	JAS_ERR_UNSUPPORTED_MODE_JPC_NS_ANALYZE,
	JAS_ERR_UNSUPPORTED_MODE_JPC_NS_SYNTHESIZE,
	JAS_ERR_UNSUPPORTED_PARAM_COMBINATION_JPC_NOMINALGAIN,
	JAS_ERR_UNEXPECTED_EOF_JPC_ENC_ENCCBLK,
	JAS_ERR_INVALID_NODEBANDNO_JPC_TSFBNODE_GETBANDSTREE,
	JAS_ERR_JPC_QMFB1D_GETSYNFILTERS,
	JAS_ERR_INVALID_CLRSPACE_TOJPGCS,
	JAS_ERR_JAS_TVPARSER_NEXT_ERR_MIF_HDR_GET,
	JAS_ERR_INVALID_MAGIC_PARAM_PNM_TYPE,
	JAS_ERR_INVALID_MAGIC_PARAM_PNM_FMT,  
	JAS_ERR_INCOMPLETE_STUB_INVOKED_JAS_ICCTXTDESC_COPY,
	JAS_ERR_INVALID_MAGIC_PARAM_PNM_GETHDR,
	JAS_ERR_FAILURE_TO_ENCODE_BLOCKS_ASSOCIATED_WITH_TILE,
	JAS_ERR_TIMER
} jas_error_t;

/******************************************************************************\
* Macros and functions.
\******************************************************************************/

/* Output debugging information to standard error provided that the debug
  level is set sufficiently high. */
#if defined(DEBUG)
#define	JAS_DBGLOG(n, x) \
	((jas_getdbglevel() >= (n)) ? (jas_eprintf x) : 0)
#else
#define	JAS_DBGLOG(n, x)
#endif

/* Get the library debug level. */
int jas_getdbglevel(void);

/* Set the library debug level. */
int jas_setdbglevel(int dbglevel);

/* Perform formatted output to standard error. */
int jas_eprintf(const char *fmt, ...);

/* Dump memory to a stream. */
int jas_memdump(FILE *out, void *data, size_t len);

/* Report an error out-of-band. */
extern void jas_error( jas_error_t, char *err_str );

/* Set the callback to receive out-of-band error notifications. */
extern void jas_set_error_cb( void(*cb) ( jas_error_t, char *) );

#ifdef __cplusplus
}
#endif

#endif
