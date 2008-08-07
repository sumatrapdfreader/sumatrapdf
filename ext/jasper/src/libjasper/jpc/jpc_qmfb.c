/*
 * Copyright (c) 1999-2000 Image Power, Inc. and the University of
 *   British Columbia.
 * Copyright (c) 2001-2003 Michael David Adams.
 * Copyright (c) 2005-2006 artofcode LLC.
 *
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
 * Quadrature Mirror-Image Filter Bank (QMFB) Library
 *
 * $Id$
 */

/******************************************************************************\
* Includes.
\******************************************************************************/

#include <assert.h>

#include "jasper/jas_fix.h"
#include "jasper/jas_malloc.h"
#include "jasper/jas_math.h"
#include "jasper/jas_debug.h"

#include "jpc_qmfb.h"
#include "jpc_tsfb.h"
#include "jpc_math.h"

#ifndef USE_ASM_WIN32
# if defined(_WIN32) && !defined(_WIN64)
#  define USE_ASM_WIN32
# endif
#endif

/******************************************************************************\
*
\******************************************************************************/

static jpc_qmfb1d_t *jpc_qmfb1d_create(void);

static int jpc_ft_getnumchans(jpc_qmfb1d_t *qmfb);
static int jpc_ft_getanalfilters(jpc_qmfb1d_t *qmfb, int len, jas_seq2d_t **filters);
static int jpc_ft_getsynfilters(jpc_qmfb1d_t *qmfb, int len, jas_seq2d_t **filters);
static void jpc_ft_analyze(jpc_qmfb1d_t *qmfb, int flags, jas_seq2d_t *x);
static void jpc_ft_synthesize(jpc_qmfb1d_t *qmfb, int flags, jas_seq2d_t *x);

static int jpc_ns_getnumchans(jpc_qmfb1d_t *qmfb);
static int jpc_ns_getanalfilters(jpc_qmfb1d_t *qmfb, int len, jas_seq2d_t **filters);
static int jpc_ns_getsynfilters(jpc_qmfb1d_t *qmfb, int len, jas_seq2d_t **filters);
static void jpc_ns_analyze(jpc_qmfb1d_t *qmfb, int flags, jas_seq2d_t *x);


static void jpc_ns_synthesize(jpc_qmfb1d_t *qmfb, int flags, jas_seq2d_t *x);

#ifdef USE_ASM_WIN32
static void jpc_win32_ns_synthesize( jpc_qmfb1d_t *qmfb, int flags, jas_seq2d_t *x);
#endif

/******************************************************************************\
*
\******************************************************************************/

jpc_qmfb1dops_t jpc_ft_ops = {
	jpc_ft_getnumchans,
	jpc_ft_getanalfilters,
	jpc_ft_getsynfilters,
	jpc_ft_analyze,
	jpc_ft_synthesize
};

jpc_qmfb1dops_t jpc_ns_ops = {
	jpc_ns_getnumchans,
	jpc_ns_getanalfilters,
	jpc_ns_getsynfilters,
	jpc_ns_analyze,
#ifdef USE_ASM_WIN32
	jpc_win32_ns_synthesize
#else
	jpc_ns_synthesize
#endif
};

/******************************************************************************\
*
\******************************************************************************/

static void jpc_qmfb1d_setup(jpc_fix_t *startptr, int startind, int endind,
  int intrastep, jpc_fix_t **lstartptr, int *lstartind, int *lendind,
  jpc_fix_t **hstartptr, int *hstartind, int *hendind)
{
	*lstartind = JPC_CEILDIVPOW2(startind, 1);
	*lendind = JPC_CEILDIVPOW2(endind, 1);
	*hstartind = JPC_FLOORDIVPOW2(startind, 1);
	*hendind = JPC_FLOORDIVPOW2(endind, 1);
	*lstartptr = startptr;
	*hstartptr = &startptr[(*lendind - *lstartind) * intrastep];
}

static void jpc_qmfb1d_split(jpc_fix_t *startptr, int startind, int endind,
  register int step, jpc_fix_t *lstartptr, int lstartind, int lendind,
  jpc_fix_t *hstartptr, int hstartind, int hendind)
{
	int bufsize = JPC_CEILDIVPOW2(endind - startind, 2);
#if !defined(HAVE_VLA)
#define QMFB_SPLITBUFSIZE 4096
	jpc_fix_t splitbuf[QMFB_SPLITBUFSIZE];
#else
	jpc_fix_t splitbuf[bufsize];
#endif
	jpc_fix_t *buf = splitbuf;
	int llen;
	int hlen;
	int twostep;
	jpc_fix_t *tmpptr;
	register jpc_fix_t *ptr;
	register jpc_fix_t *hptr;
	register jpc_fix_t *lptr;
	register int n;
	int state;

	twostep = step << 1;
	llen = lendind - lstartind;
	hlen = hendind - hstartind;

#if !defined(HAVE_VLA)
	/* Get a buffer. */
	if (bufsize > QMFB_SPLITBUFSIZE) {
		if (!(buf = jas_malloc(bufsize * sizeof(jpc_fix_t)))) {
			/* We have no choice but to commit suicide in this case. */
			jas_error(	JAS_ERR_MALLOC_FAILURE_JPC_QMFB1D_SPLIT,
						"JAS_ERR_MALLOC_FAILURE_JPC_QMFB1D_SPLIT"
					);
			return;
		}
	}
#endif

	if (hstartind < lstartind) {
		/* The first sample in the input signal is to appear
		  in the highpass subband signal. */
		/* Copy the appropriate samples into the lowpass subband
		  signal, saving any samples destined for the highpass subband
		  signal as they are overwritten. */
		tmpptr = buf;
		ptr = &startptr[step];
		lptr = lstartptr;
		n = llen;
		state = 1;
		while (n-- > 0) {
			if (state) {
				*tmpptr = *lptr;
				++tmpptr;
			}
			*lptr = *ptr;
			ptr += twostep;
			lptr += step;
			state ^= 1;
		}
		/* Copy the appropriate samples into the highpass subband
		  signal. */
		/* Handle the nonoverwritten samples. */
		hptr = &hstartptr[(hlen - 1) * step];
		ptr = &startptr[(((llen + hlen - 1) >> 1) << 1) * step];
		n = hlen - (tmpptr - buf);
		while (n-- > 0) {
			*hptr = *ptr;
			hptr -= step;
			ptr -= twostep;
		}
		/* Handle the overwritten samples. */
		n = tmpptr - buf;
		while (n-- > 0) {
			--tmpptr;
			*hptr = *tmpptr;
			hptr -= step;
		}
	} else {
		/* The first sample in the input signal is to appear
		  in the lowpass subband signal. */
		/* Copy the appropriate samples into the lowpass subband
		  signal, saving any samples for the highpass subband
		  signal as they are overwritten. */
		state = 0;
		ptr = startptr;
		lptr = lstartptr;
		tmpptr = buf;
		n = llen;
		while (n-- > 0) {
			if (state) {
				*tmpptr = *lptr;
				++tmpptr;
			}
			*lptr = *ptr;
			ptr += twostep;
			lptr += step;
			state ^= 1;
		}
		/* Copy the appropriate samples into the highpass subband
		  signal. */
		/* Handle the nonoverwritten samples. */
		ptr = &startptr[((((llen + hlen) >> 1) << 1) - 1) * step];
		hptr = &hstartptr[(hlen - 1) * step];
		n = hlen - (tmpptr - buf);
		while (n-- > 0) {
			*hptr = *ptr;
			ptr -= twostep;
			hptr -= step;
		}
		/* Handle the overwritten samples. */
		n = tmpptr - buf;
		while (n-- > 0) {
			--tmpptr;
			*hptr = *tmpptr;
			hptr -= step;
		}
	}

#if !defined(HAVE_VLA)
	/* If the split buffer was allocated on the heap, free this memory. */
	if (buf != splitbuf) {
		jas_free(buf);
	}
#endif
}

static void jpc_qmfb1d_join(jpc_fix_t *startptr, int startind, int endind,
  register int step, jpc_fix_t *lstartptr, int lstartind, int lendind,
  jpc_fix_t *hstartptr, int hstartind, int hendind)
{
	int bufsize = JPC_CEILDIVPOW2(endind - startind, 2);
#if !defined(HAVE_VLA)
#define	QMFB_JOINBUFSIZE	4096
	jpc_fix_t joinbuf[QMFB_JOINBUFSIZE];
#else
	jpc_fix_t joinbuf[bufsize];
#endif
	jpc_fix_t *buf = joinbuf;
	int llen;
	int hlen;
	int twostep;
	jpc_fix_t *tmpptr;
	register jpc_fix_t *ptr;
	register jpc_fix_t *hptr;
	register jpc_fix_t *lptr;
	register int n;
	int state;

#if !defined(HAVE_VLA)
	/* Allocate memory for the join buffer from the heap. */
	if (bufsize > QMFB_JOINBUFSIZE) {
		if (!(buf = jas_malloc(bufsize * sizeof(jpc_fix_t)))) {
			/* We have no choice but to commit suicide. */
			jas_error(	JAS_ERR_MALLOC_FAILURE_JPC_QMFB1D_JOIN,
						"JAS_ERR_MALLOC_FAILURE_JPC_QMFB1D_JOIN"
					);
			return;
		}
	}
#endif

	twostep = step << 1;
	llen = lendind - lstartind;
	hlen = hendind - hstartind;

	if (hstartind < lstartind) {
		/* The first sample in the highpass subband signal is to
		  appear first in the output signal. */
		/* Copy the appropriate samples into the first phase of the
		  output signal. */
		tmpptr = buf;
		hptr = hstartptr;
		ptr = startptr;
		n = (llen + 1) >> 1;
		while (n-- > 0) {
			*tmpptr = *ptr;
			*ptr = *hptr;
			++tmpptr;
			ptr += twostep;
			hptr += step;
		}
		n = hlen - ((llen + 1) >> 1);
		while (n-- > 0) {
			*ptr = *hptr;
			ptr += twostep;
			hptr += step;
		}
		/* Copy the appropriate samples into the second phase of
		  the output signal. */
		ptr -= (lendind > hendind) ? (step) : (step + twostep);
		state = !((llen - 1) & 1);
		lptr = &lstartptr[(llen - 1) * step];
		n = llen;
		while (n-- > 0) {
			if (state) {
				--tmpptr;
				*ptr = *tmpptr;
			} else {
				*ptr = *lptr;
			}
			lptr -= step;
			ptr -= twostep;
			state ^= 1;
		}
	} else {
		/* The first sample in the lowpass subband signal is to
		  appear first in the output signal. */
		/* Copy the appropriate samples into the first phase of the
		  output signal (corresponding to even indexed samples). */
		lptr = &lstartptr[(llen - 1) * step];
		ptr = &startptr[((llen - 1) << 1) * step];
		n = llen >> 1;
		tmpptr = buf;
		while (n-- > 0) {
			*tmpptr = *ptr;
			*ptr = *lptr;
			++tmpptr;
			ptr -= twostep;
			lptr -= step;
		}
		n = llen - (llen >> 1);
		while (n-- > 0) {
			*ptr = *lptr;
			ptr -= twostep;
			lptr -= step;
		}
		/* Copy the appropriate samples into the second phase of
		  the output signal (corresponding to odd indexed
		  samples). */
		ptr = &startptr[step];
		hptr = hstartptr;
		state = !(llen & 1);
		n = hlen;
		while (n-- > 0) {
			if (state) {
				--tmpptr;
				*ptr = *tmpptr;
			} else {
				*ptr = *hptr;
			}
			hptr += step;
			ptr += twostep;
			state ^= 1;
		}
	}

#if !defined(HAVE_VLA)
	/* If the join buffer was allocated on the heap, free this memory. */
	if (buf != joinbuf) {
		jas_free(buf);
	}
#endif
}

/******************************************************************************\
* Code for 5/3 transform.
\******************************************************************************/

static int jpc_ft_getnumchans(jpc_qmfb1d_t *qmfb)
{
	/* Avoid compiler warnings about unused parameters. */
	qmfb = 0;

	return 2;
}

static int jpc_ft_getanalfilters(jpc_qmfb1d_t *qmfb, int len, jas_seq2d_t **filters)
{
	/* Avoid compiler warnings about unused parameters. */
	qmfb = 0;
	len = 0;
	filters = 0;

	jas_error(	JAS_ERR_INCOMPLETE_STUB_INVOKED_JPC_FT_GETANALFILTERS,
				"JAS_ERR_INCOMPLETE_STUB_INVOKED_JPC_FT_GETANALFILTERS"
			);
	return -1;
}

static int jpc_ft_getsynfilters(jpc_qmfb1d_t *qmfb, int len, jas_seq2d_t **filters)
{
	jas_seq_t *lf;
	jas_seq_t *hf;

	/* Avoid compiler warnings about unused parameters. */
	qmfb = 0;

	lf = 0;
	hf = 0;

	if (len > 1 || (!len)) {
		if (!(lf = jas_seq_create(-1, 2))) {
			goto error;
		}
		jas_seq_set(lf, -1, jpc_dbltofix(0.5));
		jas_seq_set(lf, 0, jpc_dbltofix(1.0));
		jas_seq_set(lf, 1, jpc_dbltofix(0.5));
		if (!(hf = jas_seq_create(-1, 4))) {
			goto error;
		}
		jas_seq_set(hf, -1, jpc_dbltofix(-0.125));
		jas_seq_set(hf, 0, jpc_dbltofix(-0.25));
		jas_seq_set(hf, 1, jpc_dbltofix(0.75));
		jas_seq_set(hf, 2, jpc_dbltofix(-0.25));
		jas_seq_set(hf, 3, jpc_dbltofix(-0.125));
	} else if (len == 1) {
		if (!(lf = jas_seq_create(0, 1))) {
			goto error;
		}
		jas_seq_set(lf, 0, jpc_dbltofix(1.0));
		if (!(hf = jas_seq_create(0, 1))) {
			goto error;
		}
		jas_seq_set(hf, 0, jpc_dbltofix(2.0));
	} else {
		jas_error(	JAS_ERR_INVALID_LEN_PARAM_JPC_FT_GETSYNFILTERS,
					"JAS_ERR_INVALID_LEN_PARAM_JPC_FT_GETSYNFILTERS"
				);
		goto error;
	}

	filters[0] = lf;
	filters[1] = hf;

	return 0;

error:
	if (lf) {
		jas_seq_destroy(lf);
	}
	if (hf) {
		jas_seq_destroy(hf);
	}
	return -1;
}

#define	NFT_LIFT0(lstartptr, lstartind, lendind, hstartptr, hstartind, hendind, step, pluseq) \
{ \
	register jpc_fix_t *lptr = (lstartptr); \
	register jpc_fix_t *hptr = (hstartptr); \
	register int n = (hendind) - (hstartind); \
	if ((hstartind) < (lstartind)) { \
		pluseq(*hptr, *lptr); \
		hptr += (step); \
		--n; \
	} \
	if ((hendind) >= (lendind)) { \
		--n; \
	} \
	while (n-- > 0) { \
		pluseq(*hptr, jpc_fix_asr(jpc_fix_add(*lptr, lptr[(step)]), 1)); \
		hptr += (step); \
		lptr += (step); \
	} \
	if ((hendind) >= (lendind)) { \
		pluseq(*hptr, *lptr); \
	} \
}

#define	NFT_LIFT1(lstartptr, lstartind, lendind, hstartptr, hstartind, hendind, step, pluseq) \
{ \
	register jpc_fix_t *lptr = (lstartptr); \
	register jpc_fix_t *hptr = (hstartptr); \
	register int n = (lendind) - (lstartind); \
	if ((hstartind) >= (lstartind)) { \
		pluseq(*lptr, *hptr); \
		lptr += (step); \
		--n; \
	} \
	if ((lendind) > (hendind)) { \
		--n; \
	} \
	while (n-- > 0) { \
		pluseq(*lptr, jpc_fix_asr(jpc_fix_add(*hptr, hptr[(step)]), 2)); \
		lptr += (step); \
		hptr += (step); \
	} \
	if ((lendind) > (hendind)) { \
		pluseq(*lptr, *hptr); \
	} \
}

#define	RFT_LIFT0(lstartptr, lstartind, lendind, hstartptr, hstartind, hendind, step, pmeqop) \
{ \
	register jpc_fix_t *lptr = (lstartptr); \
	register jpc_fix_t *hptr = (hstartptr); \
	register int n = (hendind) - (hstartind); \
	if ((hstartind) < (lstartind)) { \
		*hptr pmeqop *lptr; \
		hptr += (step); \
		--n; \
	} \
	if ((hendind) >= (lendind)) { \
		--n; \
	} \
	while (n-- > 0) { \
		*hptr pmeqop (*lptr + lptr[(step)]) >> 1; \
		hptr += (step); \
		lptr += (step); \
	} \
	if ((hendind) >= (lendind)) { \
		*hptr pmeqop *lptr; \
	} \
}

#define	RFT_LIFT1(lstartptr, lstartind, lendind, hstartptr, hstartind, hendind, step, pmeqop) \
{ \
	register jpc_fix_t *lptr = (lstartptr); \
	register jpc_fix_t *hptr = (hstartptr); \
	register int n = (lendind) - (lstartind); \
	if ((hstartind) >= (lstartind)) { \
		*lptr pmeqop ((*hptr << 1) + 2) >> 2; \
		lptr += (step); \
		--n; \
	} \
	if ((lendind) > (hendind)) { \
		--n; \
	} \
	while (n-- > 0) { \
		*lptr pmeqop ((*hptr + hptr[(step)]) + 2) >> 2; \
		lptr += (step); \
		hptr += (step); \
	} \
	if ((lendind) > (hendind)) { \
		*lptr pmeqop ((*hptr << 1) + 2) >> 2; \
	} \
}

static void jpc_ft_analyze(jpc_qmfb1d_t *qmfb, int flags, jas_seq2d_t *x)
{
	jpc_fix_t *startptr;
	int startind;
	int endind;
	jpc_fix_t *  lstartptr;
	int   lstartind;
	int   lendind;
	jpc_fix_t *  hstartptr;
	int   hstartind;
	int   hendind;
	int interstep;
	int intrastep;
	int numseq;

	/* Avoid compiler warnings about unused parameters. */
	qmfb = 0;

	if (flags & JPC_QMFB1D_VERT) {
		interstep = 1;
		intrastep = jas_seq2d_rowstep(x);
		numseq = jas_seq2d_width(x);
		startind = jas_seq2d_ystart(x);
		endind = jas_seq2d_yend(x);
	} else {
		interstep = jas_seq2d_rowstep(x);
		intrastep = 1;
		numseq = jas_seq2d_height(x);
		startind = jas_seq2d_xstart(x);
		endind = jas_seq2d_xend(x);
	}

	assert(startind < endind);

	startptr = jas_seq2d_getref(x, jas_seq2d_xstart(x), jas_seq2d_ystart(x));
	if (flags & JPC_QMFB1D_RITIMODE) {
		while (numseq-- > 0) {
			jpc_qmfb1d_setup(startptr, startind, endind, intrastep,
			  &lstartptr, &lstartind, &lendind, &hstartptr,
			  &hstartind, &hendind);
			if (endind - startind > 1) {
				jpc_qmfb1d_split(startptr, startind, endind,
				  intrastep, lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind);
				RFT_LIFT0(lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind, intrastep, -=);
				RFT_LIFT1(lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind, intrastep, +=);
			} else {
				if (lstartind == lendind) {
					*startptr <<= 1;
				}
			}
			startptr += interstep;
		}
	} else {
		while (numseq-- > 0) {
			jpc_qmfb1d_setup(startptr, startind, endind, intrastep,
			  &lstartptr, &lstartind, &lendind, &hstartptr,
			  &hstartind, &hendind);
			if (endind - startind > 1) {
				jpc_qmfb1d_split(startptr, startind, endind,
				  intrastep, lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind);
				NFT_LIFT0(lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind, intrastep,
				  jpc_fix_minuseq);
				NFT_LIFT1(lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind, intrastep,
				  jpc_fix_pluseq);
			} else {
				if (lstartind == lendind) {
					*startptr = jpc_fix_asl(*startptr, 1);
				}
			}
			startptr += interstep;
		}
	}
}

static void jpc_ft_synthesize(jpc_qmfb1d_t *qmfb, int flags, jas_seq2d_t *x)
{
	jpc_fix_t *startptr;
	int startind;
	int endind;
	jpc_fix_t *lstartptr;
	int lstartind;
	int lendind;
	jpc_fix_t *hstartptr;
	int hstartind;
	int hendind;
	int interstep;
	int intrastep;
	int numseq;

	/* Avoid compiler warnings about unused parameters. */
	qmfb = 0;

	if (flags & JPC_QMFB1D_VERT) {
		interstep = 1;
		intrastep = jas_seq2d_rowstep(x);
		numseq = jas_seq2d_width(x);
		startind = jas_seq2d_ystart(x);
		endind = jas_seq2d_yend(x);
	} else {
		interstep = jas_seq2d_rowstep(x);
		intrastep = 1;
		numseq = jas_seq2d_height(x);
		startind = jas_seq2d_xstart(x);
		endind = jas_seq2d_xend(x);
	}

	assert(startind < endind);

	startptr = jas_seq2d_getref(x, jas_seq2d_xstart(x), jas_seq2d_ystart(x));
	if (flags & JPC_QMFB1D_RITIMODE) {
		while (numseq-- > 0) {
			jpc_qmfb1d_setup(startptr, startind, endind, intrastep,
			  &lstartptr, &lstartind, &lendind, &hstartptr,
			  &hstartind, &hendind);
			if (endind - startind > 1) {
				RFT_LIFT1(lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind, intrastep, -=);
				RFT_LIFT0(lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind, intrastep, +=);
				jpc_qmfb1d_join(startptr, startind, endind,
				  intrastep, lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind);
			} else {
				if (lstartind == lendind) {
					*startptr >>= 1;
				}
			}
			startptr += interstep;
		}
	} else {
		while (numseq-- > 0) {
			jpc_qmfb1d_setup(startptr, startind, endind, intrastep,
			  &lstartptr, &lstartind, &lendind, &hstartptr,
			  &hstartind, &hendind);
			if (endind - startind > 1) {
				NFT_LIFT1(lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind, intrastep,
				  jpc_fix_minuseq);
				NFT_LIFT0(lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind, intrastep,
				  jpc_fix_pluseq);
				jpc_qmfb1d_join(startptr, startind, endind,
				  intrastep, lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind);
			} else {
				if (lstartind == lendind) {
					*startptr = jpc_fix_asr(*startptr, 1);
				}
			}
			startptr += interstep;
		}
	}
}

/******************************************************************************\
* Code for 9/7 transform.
\******************************************************************************/

static int jpc_ns_getnumchans(jpc_qmfb1d_t *qmfb)
{
	/* Avoid compiler warnings about unused parameters. */
	qmfb = 0;

	return 2;
}

static int jpc_ns_getanalfilters(jpc_qmfb1d_t *qmfb, int len, jas_seq2d_t **filters)
{
	/* Avoid compiler warnings about unused parameters. */
	qmfb = 0;
	len = 0;
	filters = 0;

	jas_error(	JAS_ERR_INCOMPLETE_STUB_INVOKED_JPC_NS_GETANALFILTERS,
				"JAS_ERR_INCOMPLETE_STUB_INVOKED_JPC_NS_GETANALFILTERS"
			);
	return -1;
}

static int jpc_ns_getsynfilters(jpc_qmfb1d_t *qmfb, int len, jas_seq2d_t **filters)
{
	jas_seq_t *lf;
	jas_seq_t *hf;

	/* Avoid compiler warnings about unused parameters. */
	qmfb = 0;

	lf = 0;
	hf = 0;

	if (len > 1 || (!len)) {
		if (!(lf = jas_seq_create(-3, 4))) {
			goto error;
		}
		jas_seq_set(lf, -3, jpc_dbltofix(-0.09127176311424948));
		jas_seq_set(lf, -2, jpc_dbltofix(-0.05754352622849957));
		jas_seq_set(lf, -1, jpc_dbltofix(0.5912717631142470));
		jas_seq_set(lf, 0, jpc_dbltofix(1.115087052456994));
		jas_seq_set(lf, 1, jpc_dbltofix(0.5912717631142470));
		jas_seq_set(lf, 2, jpc_dbltofix(-0.05754352622849957));
		jas_seq_set(lf, 3, jpc_dbltofix(-0.09127176311424948));
		if (!(hf = jas_seq_create(-3, 6))) {
			goto error;
		}
		jas_seq_set(hf, -3, jpc_dbltofix(-0.02674875741080976 * 2.0));
		jas_seq_set(hf, -2, jpc_dbltofix(-0.01686411844287495 * 2.0));
		jas_seq_set(hf, -1, jpc_dbltofix(0.07822326652898785 * 2.0));
		jas_seq_set(hf, 0, jpc_dbltofix(0.2668641184428723 * 2.0));
		jas_seq_set(hf, 1, jpc_dbltofix(-0.6029490182363579 * 2.0));
		jas_seq_set(hf, 2, jpc_dbltofix(0.2668641184428723 * 2.0));
		jas_seq_set(hf, 3, jpc_dbltofix(0.07822326652898785 * 2.0));
		jas_seq_set(hf, 4, jpc_dbltofix(-0.01686411844287495 * 2.0));
		jas_seq_set(hf, 5, jpc_dbltofix(-0.02674875741080976 * 2.0));
	} else if (len == 1) {
		if (!(lf = jas_seq_create(0, 1))) {
			goto error;
		}
		jas_seq_set(lf, 0, jpc_dbltofix(1.0));
		if (!(hf = jas_seq_create(0, 1))) {
			goto error;
		}
		jas_seq_set(hf, 0, jpc_dbltofix(2.0));
	} else {
		jas_error(	JAS_ERR_INVALID_LEN_PARAM_JPC_NS_GETSYNFILTERS,
					"JAS_ERR_INVALID_LEN_PARAM_JPC_NS_GETSYNFILTERS"
				);
		goto error;
	}

	filters[0] = lf;
	filters[1] = hf;

	return 0;

error:
	if (lf) {
		jas_seq_destroy(lf);
	}
	if (hf) {
		jas_seq_destroy(hf);
	}
	return -1;
}

#define	NNS_LIFT0(lstartptr, lstartind, lendind, hstartptr, hstartind, hendind, step, alpha) \
{ \
	register jpc_fix_t *lptr = (lstartptr); \
	register jpc_fix_t *hptr = (hstartptr); \
	register int n = (hendind) - (hstartind); \
	jpc_fix_t twoalpha = jpc_fix_mulbyint(alpha, 2); \
	if ((hstartind) < (lstartind)) { \
		jpc_fix_pluseq(*hptr, jpc_fix_mul(*lptr, (twoalpha))); \
		hptr += (step); \
		--n; \
	} \
	if ((hendind) >= (lendind)) { \
		--n; \
	} \
	while (n-- > 0) { \
		jpc_fix_pluseq(*hptr, jpc_fix_mul(jpc_fix_add(*lptr, lptr[(step)]), (alpha))); \
		hptr += (step); \
		lptr += (step); \
	} \
	if ((hendind) >= (lendind)) { \
		jpc_fix_pluseq(*hptr, jpc_fix_mul(*lptr, (twoalpha))); \
	} \
}

#define	NNS_LIFT1(lstartptr, lstartind, lendind, hstartptr, hstartind, hendind, step, alpha) \
{ \
	register jpc_fix_t *lptr = (lstartptr); \
	register jpc_fix_t *hptr = (hstartptr); \
	register int n = (lendind) - (lstartind); \
	int twoalpha = jpc_fix_mulbyint(alpha, 2); \
	if ((hstartind) >= (lstartind)) { \
		jpc_fix_pluseq(*lptr, jpc_fix_mul(*hptr, (twoalpha))); \
		lptr += (step); \
		--n; \
	} \
	if ((lendind) > (hendind)) { \
		--n; \
	} \
	while (n-- > 0) { \
		jpc_fix_pluseq(*lptr, jpc_fix_mul(jpc_fix_add(*hptr, hptr[(step)]), (alpha))); \
		lptr += (step); \
		hptr += (step); \
	} \
	if ((lendind) > (hendind)) { \
		jpc_fix_pluseq(*lptr, jpc_fix_mul(*hptr, (twoalpha))); \
	} \
}

#define	NNS_SCALE(startptr, startind, endind, step, alpha) \
{ \
	register jpc_fix_t *ptr = (startptr); \
	register int n = (endind) - (startind); \
	while (n-- > 0) { \
		jpc_fix_muleq(*ptr, alpha); \
		ptr += (step); \
	} \
}

static void jpc_ns_analyze(jpc_qmfb1d_t *qmfb, int flags, jas_seq2d_t *x)
{
	jpc_fix_t *startptr;
	int startind;
	int endind;
	jpc_fix_t *lstartptr;
	int lstartind;
	int lendind;
	jpc_fix_t *hstartptr;
	int hstartind;
	int hendind;
	int interstep;
	int intrastep;
	int numseq;

	/* Avoid compiler warnings about unused parameters. */
	qmfb = 0;

	if (flags & JPC_QMFB1D_VERT) {
		interstep = 1;
		intrastep = jas_seq2d_rowstep(x);
		numseq = jas_seq2d_width(x);
		startind = jas_seq2d_ystart(x);
		endind = jas_seq2d_yend(x);
	} else {
		interstep = jas_seq2d_rowstep(x);
		intrastep = 1;
		numseq = jas_seq2d_height(x);
		startind = jas_seq2d_xstart(x);
		endind = jas_seq2d_xend(x);
	}

	assert(startind < endind);

	startptr = jas_seq2d_getref(x, jas_seq2d_xstart(x), jas_seq2d_ystart(x));
	if (!(flags & JPC_QMFB1D_RITIMODE)) {
		while (numseq-- > 0) {
			jpc_qmfb1d_setup(startptr, startind, endind, intrastep,
			  &lstartptr, &lstartind, &lendind, &hstartptr,
			  &hstartind, &hendind);
			if (endind - startind > 1) {
				jpc_qmfb1d_split(startptr, startind, endind,
				  intrastep, lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind);
				NNS_LIFT0(lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind, intrastep,
				  jpc_dbltofix(-1.586134342));
				NNS_LIFT1(lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind, intrastep,
				  jpc_dbltofix(-0.052980118));
				NNS_LIFT0(lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind, intrastep,
				  jpc_dbltofix(0.882911075));
				NNS_LIFT1(lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind, intrastep,
				  jpc_dbltofix(0.443506852));
				NNS_SCALE(lstartptr, lstartind, lendind,
				  intrastep, jpc_dbltofix(1.0/1.23017410558578));
				NNS_SCALE(hstartptr, hstartind, hendind,
				  intrastep, jpc_dbltofix(1.0/1.62578613134411));
			} else {
#if 0
				if (lstartind == lendind) {
					*startptr = jpc_fix_asl(*startptr, 1);
				}
#endif
			}
			startptr += interstep;
		}
	} else {
		/* The reversible integer-to-integer mode is not supported
		  for this transform. */
		jas_error(	JAS_ERR_UNSUPPORTED_MODE_JPC_NS_ANALYZE,
					"JAS_ERR_UNSUPPORTED_MODE_JPC_NS_ANALYZE"
				);
	}
}


#ifdef USE_ASM_WIN32

#define	DBL_FIX_A	(0x0000275d)
#define	DBL_FIX_B	(0x00003406)
#define	DBL_FIX_C	(0xfffff1cf)
#define	DBL_FIX_D	(0xffffe3c0)
#define	DBL_FIX_E	(0x000001b2)
#define	DBL_FIX_F	(0x000032c1)

#define	twoalpha_C	(0xffffe39e)
#define	twoalpha_D	(0xffffc780)
#define	twoalpha_E	(0x00000364)
#define	twoalpha_F	(0x00006582)

#endif




void jpc_ns_synthesize(jpc_qmfb1d_t *qmfb, int flags, jas_seq2d_t *x)
{
	jpc_fix_t *startptr;
	int startind;
	int endind;
	jpc_fix_t *lstartptr;
	int lstartind;
	int lendind;
	jpc_fix_t *hstartptr;
	int hstartind;
	int hendind;
	int interstep;
	int intrastep;
	int numseq;

	/* Avoid compiler warnings about unused parameters. */
	qmfb = 0;

	if (flags & JPC_QMFB1D_VERT) {
		interstep = 1;
		intrastep = jas_seq2d_rowstep(x);
		numseq = jas_seq2d_width(x);
		startind = jas_seq2d_ystart(x);
		endind = jas_seq2d_yend(x);
	} else {
		interstep = jas_seq2d_rowstep(x);
		intrastep = 1;
		numseq = jas_seq2d_height(x);
		startind = jas_seq2d_xstart(x);
		endind = jas_seq2d_xend(x);
	}

	assert(startind < endind);

	startptr = jas_seq2d_getref(x, jas_seq2d_xstart(x), jas_seq2d_ystart(x));
	if (!(flags & JPC_QMFB1D_RITIMODE)) {
		while (numseq-- > 0) {
			jpc_qmfb1d_setup(startptr, startind, endind, intrastep,
			  &lstartptr, &lstartind, &lendind, &hstartptr,
			  &hstartind, &hendind);
			if (endind - startind > 1) {
				NNS_SCALE(lstartptr, lstartind, lendind,
				  intrastep, jpc_dbltofix(1.23017410558578));
				NNS_SCALE(hstartptr, hstartind, hendind,
				  intrastep, jpc_dbltofix(1.62578613134411));
				NNS_LIFT1(lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind, intrastep,
				  jpc_dbltofix(-0.443506852));
				NNS_LIFT0(lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind, intrastep,
				  jpc_dbltofix(-0.882911075));
				NNS_LIFT1(lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind, intrastep,
				  jpc_dbltofix(0.052980118));
				NNS_LIFT0(lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind, intrastep,
				  jpc_dbltofix(1.586134342));
				jpc_qmfb1d_join(startptr, startind, endind,
				  intrastep, lstartptr, lstartind, lendind,
				  hstartptr, hstartind, hendind);
			} else {
#if 0
				if (lstartind == lendind) {
					*startptr = jpc_fix_asr(*startptr, 1);
				}
#endif
			}
			startptr += interstep;
		}
	} else {
		/* The reversible integer-to-integer mode is not supported
		  for this transform. */
		jas_error(	JAS_ERR_UNSUPPORTED_MODE_JPC_NS_SYNTHESIZE,
					"JAS_ERR_UNSUPPORTED_MODE_JPC_NS_SYNTHESIZE"
				);
	}
}



#ifdef USE_ASM_WIN32


#define	USE_LF_ASM

void jpc_win32_ns_synthesize(	jpc_qmfb1d_t *qmfb, 
								int flags, 
								jas_seq2d_t *x
							)
{
	jpc_fix_t *startptr;
	int startind;
	int endind;
	jpc_fix_t *lstartptr;
	int lstartind;
	int lendind;
	jpc_fix_t *hstartptr;
	int hstartind;
	int hendind;
	int interstep;
	int intrastep;
	int numseq;

	/* Avoid compiler warnings about unused parameters. */
	qmfb = 0;

	if (flags & JPC_QMFB1D_VERT) 
	{
		interstep = 1;
		intrastep = jas_seq2d_rowstep(x);
		numseq = jas_seq2d_width(x);
		startind = jas_seq2d_ystart(x);
		endind = jas_seq2d_yend(x);
	} 
	else 
	{
		interstep = jas_seq2d_rowstep(x);
		intrastep = 1;
		numseq = jas_seq2d_height(x);
		startind = jas_seq2d_xstart(x);
		endind = jas_seq2d_xend(x);
	}

	assert(startind < endind);

	startptr = jas_seq2d_getref(x, jas_seq2d_xstart(x), jas_seq2d_ystart(x));

	if (!(flags & JPC_QMFB1D_RITIMODE)) 
	{
		while (numseq-- > 0) 
		{
			jpc_qmfb1d_setup(	startptr, 
								startind, 
								endind, 
								intrastep,
								&lstartptr, 
								&lstartind, 
								&lendind, 
								&hstartptr,
								&hstartind, 
								&hendind
							);

			if (endind - startind > 1) 
			{
#if !defined(USE_LF_ASM)
				NNS_SCALE(	lstartptr, 
							lstartind, 
							lendind,
							intrastep, 
							DBL_FIX_A
						);
#else

	__asm	mov	esi,	lstartptr
	__asm	mov	eax,	lstartind
	__asm	mov ebx,	lendind
	__asm	sub ebx,	eax
	__asm	mov			ecx, intrastep
	__asm	shl			ecx, 2

scale_lp0:	;
	__asm	test	ebx, ebx
	__asm	je		skip_scale0

	__asm	mov		eax, [esi]
	__asm	test	eax, eax
	__asm	je		skip_mul0
	__asm	mov		edx, DBL_FIX_A
	__asm	imul	edx
	__asm	shrd	eax, edx, JPC_FIX_FRACBITS
	__asm	mov		[esi], eax
skip_mul0:	;
	__asm	add		esi, ecx
	__asm	sub		ebx, 1
	__asm	jmp		scale_lp0
skip_scale0:	;

#endif

#if !defined(USE_LF_ASM)
				NNS_SCALE(	hstartptr, 
							hstartind, 
							hendind,
							intrastep, 
							DBL_FIX_B
						);
#else

	__asm	mov	esi,	hstartptr
	__asm	mov	eax,	hstartind
	__asm	mov ebx,	hendind
	__asm	sub ebx,	eax

scale_lp1:	;
	__asm	test	ebx, ebx
	__asm	je		skip_scale1

	__asm	mov		eax, [esi]
	__asm	test	eax, eax
	__asm	je		skip_mul1
	__asm	mov		edx, DBL_FIX_B
	__asm	imul	edx
	__asm	shrd	eax, edx, JPC_FIX_FRACBITS
	__asm	mov		[esi], eax
skip_mul1:	;
	__asm	add		esi, ecx
	__asm	sub		ebx, 1
	__asm	jmp		scale_lp1
skip_scale1:	;

#endif

#if !defined(USE_LF_ASM)
				RA_NNS_LIFT1(	lstartptr, 
								lstartind, 
								lendind,
								hstartptr, 
								hstartind, 
								hendind, 
								intrastep,
								DBL_FIX_C,
								twoalpha_C
							);
#else

	__asm	mov	esi,	lstartptr
	__asm	mov	edi,	hstartptr
	__asm	mov	eax,	lstartind
	__asm	mov ebx,	lendind
	__asm	sub ebx,	eax

	__asm	mov	eax,	hstartind
	__asm	cmp	eax,	lstartind
	__asm	jl			no_1C
	
	__asm	mov			eax, [edi]
	__asm	test		eax, eax
	__asm	je			skip_slow1C
	__asm	mov			edx, twoalpha_C
	__asm	imul		edx
	__asm	shrd		eax, edx, JPC_FIX_FRACBITS
	__asm	add			dword ptr[esi], eax
skip_slow1C:

	__asm	add			esi, ecx
	__asm	sub			ebx, 1
no_1C:
	__asm	mov	eax,	lendind
	__asm	cmp	eax,	hendind
	__asm	jle			lpC
	__asm	sub			ebx, 1

lpC:
	__asm	test	ebx, ebx
	__asm	jle		done_lpC
lpaC:
	__asm	mov		eax, dword ptr[edi]
	__asm	sub		ebx, 1
	__asm	add		eax, dword ptr[edi + ecx ]
	__asm	test	eax, eax
	__asm	je		skip_slowC

	__asm	mov		edx, DBL_FIX_C
	__asm	imul	edx
	__asm	shrd	eax, edx, JPC_FIX_FRACBITS
	__asm	add		dword ptr[esi], eax

skip_slowC:
	__asm	add		esi, ecx
	__asm	add		edi, ecx
	__asm	test	ebx, ebx
	__asm	jg		lpaC
done_lpC:					;

	__asm	mov	eax,	lendind
	__asm	cmp	eax,	hendind
	__asm	jle			no_3C
	__asm	mov			eax, dword ptr[edi]
	__asm	test		eax, eax
	__asm	je			no_3C
	__asm	mov			edx, dword ptr[twoalpha_C]
	__asm	imul		edx
	__asm	shrd		eax, edx, JPC_FIX_FRACBITS
	__asm	add			dword ptr[esi],eax
no_3C:		;

#endif


#if !defined(USE_LF_ASM)
				NNS_LIFT0(	lstartptr, 
							lstartind, 
							lendind,
							hstartptr, 
							hstartind, 
							hendind, 
							intrastep,
							DBL_FIX_D
						);
#else

	__asm	mov	esi,	lstartptr
	__asm	mov	edi,	hstartptr
	__asm	mov	eax,	hstartind
	__asm	mov ebx,	hendind
	__asm	sub ebx,	eax

	__asm	mov	eax,	hstartind
	__asm	cmp	eax,	lstartind
	__asm	jge			no_lift0a
	
	__asm	mov			eax, [edi]
	__asm	test		eax, eax
	__asm	je			skip_slow_lift0
	__asm	mov			edx, twoalpha_D
	__asm	imul		edx
	__asm	shrd		eax, edx, JPC_FIX_FRACBITS
	__asm	add			dword ptr[esi], eax
skip_slow_lift0:

	__asm	add			esi, ecx
	__asm	sub			ebx, 1
no_lift0a:

	__asm	mov	eax,	hendind
	__asm	cmp	eax,	lendind
	__asm	jl			lpa_lift0
	__asm	dec			ebx

lpa_lift0:	;
	__asm	test	ebx, ebx
	__asm	jle		done_lpa_lift0
lpb_lift0:
	__asm	mov		eax, dword ptr[esi]
	__asm	sub		ebx, 1
	__asm	add		eax, dword ptr[esi + ecx ]
	__asm	test	eax, eax
	__asm	je		skip_slowa_lift0

	__asm	mov		edx, DBL_FIX_D
	__asm	imul	edx
	__asm	shrd	eax, edx, JPC_FIX_FRACBITS
	__asm	add		dword ptr[edi], eax

skip_slowa_lift0:
	__asm	add		esi, ecx
	__asm	add		edi, ecx
	__asm	test	ebx, ebx
	__asm	jg		lpb_lift0
done_lpa_lift0:	;

	__asm	mov	eax,	hendind
	__asm	cmp	eax,	lendind
	__asm	jl			no_3b_lift0
	__asm	mov			eax, dword ptr[esi]
	__asm	test		eax, eax
	__asm	je			no_3b_lift0
	__asm	mov			edx, twoalpha_D
	__asm	imul		edx
	__asm	shrd		eax, edx, JPC_FIX_FRACBITS
	__asm	add			dword ptr[edi],eax
no_3b_lift0:		;


#endif

#if !defined(USE_LF_ASM)
				NNS_LIFT1(	lstartptr, 
							lstartind, 
							lendind,
							hstartptr, 
							hstartind, 
							hendind, 
							intrastep,
							DBL_FIX_E
						);
#else

	__asm	mov	esi,	lstartptr
	__asm	mov	edi,	hstartptr
	__asm	mov	eax,	lstartind
	__asm	mov ebx,	lendind
	__asm	sub ebx,	eax

	__asm	mov	eax,	hstartind
	__asm	cmp	eax,	lstartind
	__asm	jl			no_1a
	
	__asm	mov			eax, [edi]
	__asm	test		eax, eax
	__asm	je			skip_slow1
	__asm	mov			edx, twoalpha_E
	__asm	imul		edx
	__asm	shrd		eax, edx, JPC_FIX_FRACBITS
	__asm	add			dword ptr[esi], eax
skip_slow1:

	__asm	add			esi, ecx
	__asm	sub			ebx, 1
no_1a:
	__asm	mov	eax,	lendind
	__asm	cmp	eax,	hendind
	__asm	jle			lpa
	__asm	sub			ebx, 1

lpa:
	__asm	test	ebx, ebx
	__asm	jle		done_lpa
lpaa:
	__asm	mov		eax, dword ptr[edi]
	__asm	sub		ebx, 1
	__asm	add		eax, dword ptr[edi + ecx ]
	__asm	test	eax, eax
	__asm	je		skip_slow

	__asm	mov		edx, DBL_FIX_E
	__asm	imul	edx
	__asm	shrd	eax, edx, JPC_FIX_FRACBITS
	__asm	add		dword ptr[esi], eax

skip_slow:
	__asm	add		esi, ecx
	__asm	add		edi, ecx
	__asm	test	ebx, ebx
	__asm	jg		lpaa
done_lpa:							;

	__asm	mov	eax,	lendind
	__asm	cmp	eax,	hendind
	__asm	jle			no_3a
	__asm	mov			eax, dword ptr[edi]
	__asm	test		eax, eax
	__asm	je			no_3a
	__asm	mov			edx, dword ptr[twoalpha_E]
	__asm	imul		edx
	__asm	shrd		eax, edx, JPC_FIX_FRACBITS
	__asm	add			dword ptr[esi],eax
no_3a:		;
#endif



#if !defined(USE_LF_ASM)
				NNS_LIFT0(	lstartptr, 
							lstartind, 
	  						lendind,
							hstartptr, 
							hstartind, 
							hendind, 
							intrastep,
							DBL_FIX_F
						);
#else

	__asm	mov	esi,	lstartptr
	__asm	mov	edi,	hstartptr
	__asm	mov	eax,	hstartind
	__asm	mov ebx,	hendind
	__asm	sub ebx,	eax

	__asm	mov	eax,	hstartind
	__asm	cmp	eax,	lstartind
	__asm	jge			no_1d
	
	__asm	mov			eax, [edi]
	__asm	test		eax, eax
	__asm	je			skip_slow4
	__asm	mov			edx, twoalpha_F
	__asm	imul		edx
	__asm	shrd		eax, edx, JPC_FIX_FRACBITS
	__asm	add			dword ptr[esi], eax
skip_slow4:

	__asm	add			esi, ecx
	__asm	sub			ebx, 1
no_1d:

	__asm	mov	eax,	hendind
	__asm	cmp	eax,	lendind
	__asm	jl			lpd
	__asm	dec			ebx

lpd:	;
	__asm	test	ebx, ebx
	__asm	jle		done_lpd
lpad:
	__asm	mov		eax, dword ptr[esi]
	__asm	sub		ebx, 1
	__asm	add		eax, dword ptr[esi + ecx ]
	__asm	test	eax, eax
	__asm	je		skip_slowd

	__asm	mov		edx, DBL_FIX_F
	__asm	imul	edx
	__asm	shrd	eax, edx, JPC_FIX_FRACBITS
	__asm	add		dword ptr[edi], eax

skip_slowd:
	__asm	add		esi, ecx
	__asm	add		edi, ecx
	__asm	test	ebx, ebx
	__asm	jg		lpad
done_lpd:	;

	__asm	mov	eax,	hendind
	__asm	cmp	eax,	lendind
	__asm	jl			no_3d
	__asm	mov			eax, dword ptr[esi]
	__asm	test		eax, eax
	__asm	je			no_3d
	__asm	mov			edx, twoalpha_F
	__asm	imul		edx
	__asm	shrd		eax, edx, JPC_FIX_FRACBITS
	__asm	add			dword ptr[edi],eax
no_3d:		;
#endif


				jpc_qmfb1d_join(	startptr, 
									startind, 
									endind,
									intrastep, 
									lstartptr, 
									lstartind, 
									lendind,
									hstartptr, 
									hstartind, 
									hendind
								);
			} else 
			{
#if !defined(USE_LF_ASM)
				if (lstartind == lendind) {
					*startptr = jpc_fix_asr(*startptr, 1);
				}
#endif
			}
			startptr += interstep;
		}
	} else {
		/* The reversible integer-to-integer mode is not supported
		  for this transform. */
		jas_error(	JAS_ERR_UNSUPPORTED_MODE_JPC_NS_SYNTHESIZE,
					"JAS_ERR_UNSUPPORTED_MODE_JPC_NS_SYNTHESIZE"
				);
	}
}

#endif


/******************************************************************************\
*
\******************************************************************************/

jpc_qmfb1d_t *jpc_qmfb1d_make(int qmfbid)
{
	jpc_qmfb1d_t *qmfb;
	if (!(qmfb = jpc_qmfb1d_create())) {
		return 0;
	}
	switch (qmfbid) {
	case JPC_QMFB1D_FT:
		qmfb->ops = &jpc_ft_ops;
		break;
	case JPC_QMFB1D_NS:
		qmfb->ops = &jpc_ns_ops;
		break;
	default:
		jpc_qmfb1d_destroy(qmfb);
		return 0;
		break;
	}
	return qmfb;
}

static jpc_qmfb1d_t *jpc_qmfb1d_create()
{
	jpc_qmfb1d_t *qmfb;
	if (!(qmfb = jas_malloc(sizeof(jpc_qmfb1d_t)))) {
		return 0;
	}
	qmfb->ops = 0;
	return qmfb;
}

jpc_qmfb1d_t *jpc_qmfb1d_copy(jpc_qmfb1d_t *qmfb)
{
	jpc_qmfb1d_t *newqmfb;

	if (!(newqmfb = jpc_qmfb1d_create())) {
		return 0;
	}
	newqmfb->ops = qmfb->ops;
	return newqmfb;
}

void jpc_qmfb1d_destroy(jpc_qmfb1d_t *qmfb)
{
	jas_free(qmfb);
}

/******************************************************************************\
*
\******************************************************************************/

void jpc_qmfb1d_getbands(jpc_qmfb1d_t *qmfb, int flags, uint_fast32_t xstart,
  uint_fast32_t ystart, uint_fast32_t xend, uint_fast32_t yend, int maxbands,
  int *numbandsptr, jpc_qmfb1dband_t *bands)
{
	int start;
	int end;

	assert(maxbands >= 2);

	if (flags & JPC_QMFB1D_VERT) {
		start = ystart;
		end = yend;
	} else {
		start = xstart;
		end = xend;
	}
/*	assert(jpc_qmfb1d_getnumchans(qmfb) == 2);	*/
	assert(start <= end);
	bands[0].start = JPC_CEILDIVPOW2(start, 1);
	bands[0].end = JPC_CEILDIVPOW2(end, 1);
	bands[0].locstart = start;
	bands[0].locend = start + bands[0].end - bands[0].start;
	bands[1].start = JPC_FLOORDIVPOW2(start, 1);
	bands[1].end = JPC_FLOORDIVPOW2(end, 1);
	bands[1].locstart = bands[0].locend;
	bands[1].locend = bands[1].locstart + bands[1].end - bands[1].start;
	assert(bands[1].locend == end);
	*numbandsptr = 2;
}

/******************************************************************************\
*
\******************************************************************************/

int jpc_qmfb1d_getnumchans(jpc_qmfb1d_t *qmfb)
{
	return (*qmfb->ops->getnumchans)(qmfb);
}

int jpc_qmfb1d_getanalfilters(jpc_qmfb1d_t *qmfb, int len, jas_seq2d_t **filters)
{
	return (*qmfb->ops->getanalfilters)(qmfb, len, filters);
}

int jpc_qmfb1d_getsynfilters(jpc_qmfb1d_t *qmfb, int len, jas_seq2d_t **filters)
{
	return (*qmfb->ops->getsynfilters)(qmfb, len, filters);
}

void jpc_qmfb1d_analyze(jpc_qmfb1d_t *qmfb, int flags, jas_seq2d_t *x)
{
	(*qmfb->ops->analyze)(qmfb, flags, x);
}

void jpc_qmfb1d_synthesize(jpc_qmfb1d_t *qmfb, int flags, jas_seq2d_t *x)
{
	(*qmfb->ops->synthesize)(qmfb, flags, x);
}
