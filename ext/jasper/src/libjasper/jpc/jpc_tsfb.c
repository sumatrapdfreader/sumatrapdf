/*
 * Copyright (c) 1999-2000 Image Power, Inc. and the University of
 *   British Columbia.
 * Copyright (c) 2001-2003 Michael David Adams.
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
 * Tree-Structured Filter Bank (TSFB) Library
 *
 * $Id$
 */

/******************************************************************************\
* Includes.
\******************************************************************************/

#include <assert.h>

#include "jasper/jas_malloc.h"
#include "jasper/jas_seq.h"
#include "jasper/jas_debug.h"

#include "jpc_tsfb.h"
#include "jpc_cod.h"
#include "jpc_cs.h"
#include "jpc_util.h"

/******************************************************************************\
*
\******************************************************************************/

#define	bandnotovind(tsfbnode, x)	((x) / (tsfbnode)->numhchans)
#define	bandnotohind(tsfbnode, x)	((x) % (tsfbnode)->numhchans)

static jpc_tsfb_t *jpc_tsfb_create(void);
static jpc_tsfbnode_t *jpc_tsfbnode_create(void);
static void jpc_tsfbnode_destroy(jpc_tsfbnode_t *node);
static void jpc_tsfbnode_synthesize(jpc_tsfbnode_t *node, int flags, jas_seq2d_t *x);
static void jpc_tsfbnode_analyze(jpc_tsfbnode_t *node, int flags, jas_seq2d_t *x);
static void qmfb2d_getbands(jpc_qmfb1d_t *hqmfb, jpc_qmfb1d_t *vqmfb,
  uint_fast32_t xstart, uint_fast32_t ystart, uint_fast32_t xend,
  uint_fast32_t yend, int maxbands, int *numbandsptr, jpc_tsfbnodeband_t *bands);
static void jpc_tsfbnode_getbandstree(jpc_tsfbnode_t *node, uint_fast32_t posxstart,
  uint_fast32_t posystart, uint_fast32_t xstart, uint_fast32_t ystart,
  uint_fast32_t xend, uint_fast32_t yend, jpc_tsfb_band_t **bands);
static int jpc_tsfbnode_findchild(jpc_tsfbnode_t *parnode, jpc_tsfbnode_t *cldnode);
static int jpc_tsfbnode_getequivfilters(jpc_tsfbnode_t *tsfbnode, int cldind,
  int width, int height, jas_seq_t **vfilter, jas_seq_t **hfilter);

/******************************************************************************\
*
\******************************************************************************/

jpc_tsfb_t *jpc_tsfb_wavelet(jpc_qmfb1d_t *hqmfb, jpc_qmfb1d_t *vqmfb, int numdlvls)
{
	jpc_tsfb_t *tsfb;
	int dlvlno;
	jpc_tsfbnode_t *curnode;
	jpc_tsfbnode_t *prevnode;
	int childno;
	if (!(tsfb = jpc_tsfb_create())) {
		return 0;
	}
	prevnode = 0;
	for (dlvlno = 0; dlvlno < numdlvls; ++dlvlno) {
		if (!(curnode = jpc_tsfbnode_create())) {
			jpc_tsfb_destroy(tsfb);
			return 0;
		}
		if (prevnode) {
			prevnode->children[0] = curnode;
			++prevnode->numchildren;
			curnode->parent = prevnode;
		} else {
			tsfb->root = curnode;
			curnode->parent = 0;
		}
		if (hqmfb) {
			curnode->numhchans = jpc_qmfb1d_getnumchans(hqmfb);
			if (!(curnode->hqmfb = jpc_qmfb1d_copy(hqmfb))) {
				jpc_tsfb_destroy(tsfb);
				return 0;
			}
		} else {
			curnode->hqmfb = 0;
			curnode->numhchans = 1;
		}
		if (vqmfb) {
			curnode->numvchans = jpc_qmfb1d_getnumchans(vqmfb);
			if (!(curnode->vqmfb = jpc_qmfb1d_copy(vqmfb))) {
				jpc_tsfb_destroy(tsfb);
				return 0;
			}
		} else {
			curnode->vqmfb = 0;
			curnode->numvchans = 1;
		}
		curnode->maxchildren = curnode->numhchans * curnode->numvchans;
		for (childno = 0; childno < curnode->maxchildren;
		  ++childno) {
			curnode->children[childno] = 0;
		}
		prevnode = curnode;
	}
	return tsfb;
}

static jpc_tsfb_t *jpc_tsfb_create()
{
	jpc_tsfb_t *tsfb;
	if (!(tsfb = jas_malloc(sizeof(jpc_tsfb_t)))) {
		return 0;
	}
	tsfb->root = 0;
	return tsfb;
}

void jpc_tsfb_destroy(jpc_tsfb_t *tsfb)
{
	if (tsfb->root) {
		jpc_tsfbnode_destroy(tsfb->root);
	}
	jas_free(tsfb);
}

/******************************************************************************\
*
\******************************************************************************/

void jpc_tsfb_analyze(jpc_tsfb_t *tsfb, int flags, jas_seq2d_t *x)
{
	if (tsfb->root) {
		jpc_tsfbnode_analyze(tsfb->root, flags, x);
	}
}

static void jpc_tsfbnode_analyze(jpc_tsfbnode_t *node, int flags, jas_seq2d_t *x)
{
	jpc_tsfbnodeband_t nodebands[JPC_TSFB_MAXBANDSPERNODE];
	int numbands;
	jas_seq2d_t *y;
	int bandno;
	jpc_tsfbnodeband_t *band;

	if (node->vqmfb) {
		jpc_qmfb1d_analyze(node->vqmfb, flags | JPC_QMFB1D_VERT, x);
	}
	if (node->hqmfb) {
		jpc_qmfb1d_analyze(node->hqmfb, flags, x);
	}
	if (node->numchildren > 0) {
		qmfb2d_getbands(node->hqmfb, node->vqmfb, jas_seq2d_xstart(x),
		  jas_seq2d_ystart(x), jas_seq2d_xend(x), jas_seq2d_yend(x),
		  JPC_TSFB_MAXBANDSPERNODE, &numbands, nodebands);
		y = jas_seq2d_create(0, 0, 0, 0);
		assert(y);
		for (bandno = 0, band = nodebands; bandno < numbands; ++bandno, ++band) {
			if (node->children[bandno]) {
				if (band->xstart != band->xend && band->ystart != band->yend) {
					jas_seq2d_bindsub(y, x, band->locxstart, band->locystart,
					  band->locxend, band->locyend);
					jas_seq2d_setshift(y, band->xstart, band->ystart);
					jpc_tsfbnode_analyze(node->children[bandno], flags, y);
				}
			}
		}
		jas_matrix_destroy(y);
	}
}

void jpc_tsfb_synthesize(jpc_tsfb_t *tsfb, int flags, jas_seq2d_t *x)
{
	if (tsfb->root) {
		jpc_tsfbnode_synthesize(tsfb->root, flags, x);
	}
}


static void jpc_tsfbnode_synthesize(jpc_tsfbnode_t *node, int flags, jas_seq2d_t *x)
{
	jpc_tsfbnodeband_t nodebands[JPC_TSFB_MAXBANDSPERNODE];
	int numbands;
	jas_seq2d_t *y;
	int bandno;
	jpc_tsfbnodeband_t *band;

	if (node->numchildren > 0) 
	{
		jas_matrix_t	org;

		qmfb2d_getbands(	node->hqmfb, node->vqmfb, 
							jas_seq2d_xstart(x),
							jas_seq2d_ystart(x), 
							jas_seq2d_xend(x), 
							jas_seq2d_yend(x),
							JPC_TSFB_MAXBANDSPERNODE, 
							&numbands, 
							nodebands
						);

		y = &org;
		memset( y, 0, sizeof(org));

		for (bandno = 0, band = nodebands; bandno < numbands; ++bandno, ++band) 
		{
			if (node->children[bandno]) 
			{
				if (band->xstart != band->xend && band->ystart != band->yend) 
				{
					jas_seq2d_bindsub(	y,
										x, 
										band->locxstart, 
										band->locystart,
										band->locxend, 
										band->locyend
									);

					jas_seq2d_setshift(	y, 
										band->xstart, 
										band->ystart
									);
					jpc_tsfbnode_synthesize(	node->children[bandno], 
												flags, 
												y
											);
				}
			}
		}
	}

	if (node->hqmfb) 
	{
		(*(node->hqmfb)->ops->synthesize)(node->hqmfb, flags, x);
	}
	if (node->vqmfb) 
	{
		(*(node->vqmfb)->ops->synthesize)(node->vqmfb, flags | JPC_QMFB1D_VERT, x);
	}
}


/*
static void jpc_tsfbnode_synthesize(jpc_tsfbnode_t *node, int flags, jas_seq2d_t *x)
{
	jpc_tsfbnodeband_t nodebands[JPC_TSFB_MAXBANDSPERNODE];
	int numbands;
	jas_seq2d_t *y;
	int bandno;
	jpc_tsfbnodeband_t *band;

	if (node->numchildren > 0) {
		qmfb2d_getbands(node->hqmfb, node->vqmfb, jas_seq2d_xstart(x),
		  jas_seq2d_ystart(x), jas_seq2d_xend(x), jas_seq2d_yend(x),
		  JPC_TSFB_MAXBANDSPERNODE, &numbands, nodebands);
		y = jas_seq2d_create(0, 0, 0, 0);
		for (bandno = 0, band = nodebands; bandno < numbands; ++bandno, ++band) {
			if (node->children[bandno]) {
				if (band->xstart != band->xend && band->ystart != band->yend) {
					jas_seq2d_bindsub(y, x, band->locxstart, band->locystart,
					  band->locxend, band->locyend);
					jas_seq2d_setshift(y, band->xstart, band->ystart);
					jpc_tsfbnode_synthesize(node->children[bandno], flags, y);
				}
			}
		}
		jas_seq2d_destroy(y);
	}
	if (node->hqmfb) {
		jpc_qmfb1d_synthesize(node->hqmfb, flags, x);
	}
	if (node->vqmfb) {
		jpc_qmfb1d_synthesize(node->vqmfb, flags | JPC_QMFB1D_VERT, x);
	}
}

*/


/******************************************************************************\
*
\******************************************************************************/


int jpc_tsfb_getbands(jpc_tsfb_t *tsfb, uint_fast32_t xstart, uint_fast32_t ystart,
  uint_fast32_t xend, uint_fast32_t yend, jpc_tsfb_band_t *bands)
{
	jpc_tsfb_band_t *savbands;
	savbands = bands;
	if (!tsfb->root) {
		bands[0].xstart = xstart;
		bands[0].ystart = ystart;
		bands[0].xend = xend;
		bands[0].yend = yend;
		bands[0].locxstart = xstart;
		bands[0].locystart = ystart;
		bands[0].locxend = xend;
		bands[0].locyend = yend;
		bands[0].orient = JPC_TSFB_LL;
		bands[0].synenergywt = JPC_FIX_ONE;
		++bands;
	} else {
		jpc_tsfbnode_getbandstree(tsfb->root, xstart, ystart,
		  xstart, ystart, xend, yend, &bands);
	}
	return bands - savbands;
}

static void jpc_tsfbnode_getbandstree(jpc_tsfbnode_t *node, uint_fast32_t posxstart,
  uint_fast32_t posystart, uint_fast32_t xstart, uint_fast32_t ystart,
  uint_fast32_t xend, uint_fast32_t yend, jpc_tsfb_band_t **bands)
{
	jpc_tsfbnodeband_t nodebands[JPC_TSFB_MAXBANDSPERNODE];
	jpc_tsfbnodeband_t *nodeband;
	int nodebandno;
	int numnodebands;
	jpc_tsfb_band_t *band;
	jas_seq_t *hfilter;
	jas_seq_t *vfilter;

	qmfb2d_getbands(node->hqmfb, node->vqmfb, xstart, ystart, xend, yend,
	  JPC_TSFB_MAXBANDSPERNODE, &numnodebands, nodebands);
	if (node->numchildren > 0) {
		for (nodebandno = 0, nodeband = nodebands;
		  nodebandno < numnodebands; ++nodebandno, ++nodeband) {
			if (node->children[nodebandno]) {
				jpc_tsfbnode_getbandstree(node->children[
				  nodebandno], posxstart +
				  nodeband->locxstart - xstart, posystart +
				  nodeband->locystart - ystart, nodeband->xstart,
				  nodeband->ystart, nodeband->xend,
				  nodeband->yend, bands);

			}
		}
	}
assert(numnodebands == 4 || numnodebands == 3);
	for (nodebandno = 0, nodeband = nodebands; nodebandno < numnodebands;
	  ++nodebandno, ++nodeband) {
		if (!node->children[nodebandno]) {
			band = *bands;
			band->xstart = nodeband->xstart;
			band->ystart = nodeband->ystart;
			band->xend = nodeband->xend;
			band->yend = nodeband->yend;
			band->locxstart = posxstart + nodeband->locxstart -
			  xstart;
			band->locystart = posystart + nodeband->locystart -
			  ystart;
			band->locxend = band->locxstart + band->xend -
			  band->xstart;
			band->locyend = band->locystart + band->yend -
			  band->ystart;
			if (numnodebands == 4) {
				switch (nodebandno) {
				case 0:
					band->orient = JPC_TSFB_LL;
					break;
				case 1:
					band->orient = JPC_TSFB_HL;
					break;
				case 2:
					band->orient = JPC_TSFB_LH;
					break;
				case 3:
					band->orient = JPC_TSFB_HH;
					break;
				default:
					jas_error(	JAS_ERR_INVALID_NODEBANDNO_JPC_TSFBNODE_GETBANDSTREE,
								"JAS_ERR_INVALID_NODEBANDNO_JPC_TSFBNODE_GETBANDSTREE"
							);
					return;
					break;
				}
			} else {
				switch (nodebandno) {
				case 0:
					band->orient = JPC_TSFB_HL;
					break;
				case 1:
					band->orient = JPC_TSFB_LH;
					break;
				case 2:
					band->orient = JPC_TSFB_HH;
					break;
				default:
					jas_error(	JAS_ERR_INVALID_NODEBANDNO_JPC_TSFBNODE_GETBANDSTREE,
								"JAS_ERR_INVALID_NODEBANDNO_JPC_TSFBNODE_GETBANDSTREE"
							);
					return;
					break;
				}
			}
			jpc_tsfbnode_getequivfilters(node, nodebandno, band->xend - band->xstart, band->yend - band->ystart, &hfilter, &vfilter);
			band->synenergywt = jpc_fix_mul(jpc_seq_norm(hfilter),
			  jpc_seq_norm(vfilter));
			jas_seq_destroy(hfilter);
			jas_seq_destroy(vfilter);
			++(*bands);
		}
	}
}

/******************************************************************************\
*
\******************************************************************************/

static jpc_tsfbnode_t *jpc_tsfbnode_create()
{
	jpc_tsfbnode_t *node;
	if (!(node = jas_malloc(sizeof(jpc_tsfbnode_t)))) {
		return 0;
	}
	node->numhchans = 0;
	node->numvchans = 0;
	node->numchildren = 0;
	node->maxchildren = 0;
	node->hqmfb = 0;
	node->vqmfb = 0;
	node->parent = 0;
	return node;
}

static void jpc_tsfbnode_destroy(jpc_tsfbnode_t *node)
{
	jpc_tsfbnode_t **child;
	int childno;
	for (childno = 0, child = node->children; childno < node->maxchildren;
	  ++childno, ++child) {
		if (*child) {
			jpc_tsfbnode_destroy(*child);
		}
	}
	if (node->hqmfb) {
		jpc_qmfb1d_destroy(node->hqmfb);
	}
	if (node->vqmfb) {
		jpc_qmfb1d_destroy(node->vqmfb);
	}
	jas_free(node);
}








static void qmfb2d_getbands(jpc_qmfb1d_t *hqmfb, jpc_qmfb1d_t *vqmfb,
  uint_fast32_t xstart, uint_fast32_t ystart, uint_fast32_t xend,
  uint_fast32_t yend, int maxbands, int *numbandsptr, jpc_tsfbnodeband_t *bands)
{
	jpc_qmfb1dband_t hbands[JPC_QMFB1D_MAXCHANS];
	jpc_qmfb1dband_t vbands[JPC_QMFB1D_MAXCHANS];
	int numhbands;
	int numvbands;
	int numbands;
	int bandno;
	int hbandno;
	int vbandno;
	jpc_tsfbnodeband_t *band;

	if (hqmfb) {
		jpc_qmfb1d_getbands(hqmfb, 0, xstart, ystart, xend, yend,
		  JPC_QMFB1D_MAXCHANS, &numhbands, hbands);
	} else {
		numhbands = 1;
		hbands[0].start = xstart;
		hbands[0].end = xend;
		hbands[0].locstart = xstart;
		hbands[0].locend = xend;
	}
	if (vqmfb) {
		jpc_qmfb1d_getbands(vqmfb, JPC_QMFB1D_VERT, xstart, ystart, xend,
		  yend, JPC_QMFB1D_MAXCHANS, &numvbands, vbands);
	} else {
		numvbands = 1;
		vbands[0].start = ystart;
		vbands[0].end = yend;
		vbands[0].locstart = ystart;
		vbands[0].locend = yend;
	}
	numbands = numhbands * numvbands;
	assert(numbands <= maxbands);
	*numbandsptr = numbands;
	for (bandno = 0, band = bands; bandno < numbands; ++bandno, ++band) {
		hbandno = bandno % numhbands;
		vbandno = bandno / numhbands;
		band->xstart = hbands[hbandno].start;
		band->ystart = vbands[vbandno].start;
		band->xend = hbands[hbandno].end;
		band->yend = vbands[vbandno].end;
		band->locxstart = hbands[hbandno].locstart;
		band->locystart = vbands[vbandno].locstart;
		band->locxend = hbands[hbandno].locend;
		band->locyend = vbands[vbandno].locend;
		assert(band->xstart <= band->xend &&
		  band->ystart <= band->yend);
		if (band->xstart == band->xend) {
			band->yend = band->ystart;
			band->locyend = band->locystart;
		} else if (band->ystart == band->yend) {
			band->xend = band->xstart;
			band->locxend = band->locxstart;
		}
	}
}

static int jpc_tsfbnode_getequivfilters(jpc_tsfbnode_t *tsfbnode, int cldind,
  int width, int height, jas_seq_t **hfilter, jas_seq_t **vfilter)
{
	jas_seq_t *hseq;
	jas_seq_t *vseq;
	jpc_tsfbnode_t *node;
	jas_seq2d_t *hfilters[JPC_QMFB1D_MAXCHANS];
	jas_seq2d_t *vfilters[JPC_QMFB1D_MAXCHANS];
	int numhchans;
	int numvchans;
	jas_seq_t *tmpseq;

	hseq = 0;
	vseq = 0;

	if (!(hseq = jas_seq_create(0, 1))) {
		goto error;
	}
	jas_seq_set(hseq, 0, jpc_inttofix(1));
	if (!(vseq = jas_seq_create(0, 1))) {
		goto error;
	}
	jas_seq_set(vseq, 0, jpc_inttofix(1));

	node = tsfbnode;
	while (node) {
		if (node->hqmfb) {
			numhchans = jpc_qmfb1d_getnumchans(node->hqmfb);
			if (jpc_qmfb1d_getsynfilters(node->hqmfb, width, hfilters)) {
					jas_error(	JAS_ERR_JPC_QMFB1D_GETSYNFILTERS,
								"JAS_ERR_JPC_QMFB1D_GETSYNFILTERS"

							);
				goto error;
			}
			if (!(tmpseq = jpc_seq_upsample(hseq, numhchans))) {
				goto error;
			}
			jas_seq_destroy(hseq);
			hseq = tmpseq;
			if (!(tmpseq = jpc_seq_conv(hseq, hfilters[bandnotohind(node, cldind)]))) {
				goto error;
			}
			jas_seq_destroy(hfilters[0]);
			jas_seq_destroy(hfilters[1]);
			jas_seq_destroy(hseq);
			hseq = tmpseq;
		}
		if (node->vqmfb) {
			numvchans = jpc_qmfb1d_getnumchans(node->vqmfb);
			if (jpc_qmfb1d_getsynfilters(node->vqmfb, height, vfilters)) 
			{
				goto error;
			}
			if (!(tmpseq = jpc_seq_upsample(vseq, numvchans))) {
				goto error;
			}
			jas_seq_destroy(vseq);
			vseq = tmpseq;
			if (!(tmpseq = jpc_seq_conv(vseq, vfilters[bandnotovind(node, cldind)]))) {
				goto error;
			}
			jas_seq_destroy(vfilters[0]);
			jas_seq_destroy(vfilters[1]);
			jas_seq_destroy(vseq);
			vseq = tmpseq;
		}
		if (node->parent) {
			cldind = jpc_tsfbnode_findchild(node->parent, node);
		}
		node = node->parent;
	}

	*hfilter = hseq;
	*vfilter = vseq;

	return 0;

error:
	if (hseq) {
		jas_seq_destroy(hseq);
	}
	if (vseq) {
		jas_seq_destroy(vseq);
	}
	return -1;

}

static int jpc_tsfbnode_findchild(jpc_tsfbnode_t *parnode, jpc_tsfbnode_t *cldnode)
{
	int i;

	for (i = 0; i < parnode->maxchildren; i++) {
		if (parnode->children[i] == cldnode)
			return i;
	}
	assert(0);
	return -1;
}

jpc_tsfb_t *jpc_cod_gettsfb(int qmfbid, int numlevels)
{
	jpc_tsfb_t *tsfb;

	switch (qmfbid) {
	case JPC_COX_RFT:
		qmfbid = JPC_QMFB1D_FT;
		break;
	case JPC_COX_INS:
		qmfbid = JPC_QMFB1D_NS;
		break;
	default:
		assert(0);
		qmfbid = 10;
		break;
	}

{
	jpc_qmfb1d_t *hqmfb;
	hqmfb = jpc_qmfb1d_make(qmfbid);
	assert(hqmfb);
	tsfb = jpc_tsfb_wavelet(hqmfb, hqmfb, numlevels);
	assert(tsfb);
	jpc_qmfb1d_destroy(hqmfb);
}

	return tsfb;
}
