/*
 * The copyright in this software is being made available under the 2-clauses 
 * BSD License, included below. This software may be subject to other third 
 * party and contributor rights, including patent rights, and no such rights
 * are granted under this license.
 *
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2001-2003, David Janssens
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2003-2007, Francois-Olivier Devaux 
 * Copyright (c) 2003-2014, Antonin Descampe
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * Copyright (c) 2008, Jerome Fimes, Communications & Systemes <jerome.fimes@c-s.fr>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MQC_INL_H
#define __MQC_INL_H
/**
FIXME DOC
@param mqc MQC handle
@return
*/
static INLINE OPJ_INT32 opj_mqc_mpsexchange(opj_mqc_t *const mqc) {
	OPJ_INT32 d;
	if (mqc->a < (*mqc->curctx)->qeval) {
		d = (OPJ_INT32)(1 - (*mqc->curctx)->mps);
		*mqc->curctx = (*mqc->curctx)->nlps;
	} else {
		d = (OPJ_INT32)(*mqc->curctx)->mps;
		*mqc->curctx = (*mqc->curctx)->nmps;
	}

	return d;
}

/**
FIXME DOC
@param mqc MQC handle
@return
*/
static INLINE OPJ_INT32 opj_mqc_lpsexchange(opj_mqc_t *const mqc) {
	OPJ_INT32 d;
	if (mqc->a < (*mqc->curctx)->qeval) {
		mqc->a = (*mqc->curctx)->qeval;
		d = (OPJ_INT32)(*mqc->curctx)->mps;
		*mqc->curctx = (*mqc->curctx)->nmps;
	} else {
		mqc->a = (*mqc->curctx)->qeval;
		d = (OPJ_INT32)(1 - (*mqc->curctx)->mps);
		*mqc->curctx = (*mqc->curctx)->nlps;
	}

	return d;
}

/**
Input a byte
@param mqc MQC handle
*/
#ifdef MQC_PERF_OPT
static INLINE void opj_mqc_bytein(opj_mqc_t *const mqc) {
	unsigned int i = *((unsigned int *) mqc->bp);
	mqc->c += i & 0xffff00;
	mqc->ct = i & 0x0f;
	mqc->bp += (i >> 2) & 0x04;
}
#else
static INLINE void opj_mqc_bytein(opj_mqc_t *const mqc) {
	if (mqc->bp != mqc->end) {
		OPJ_UINT32 c;
		if (mqc->bp + 1 != mqc->end) {
			c = *(mqc->bp + 1);
		} else {
			c = 0xff;
		}
		if (*mqc->bp == 0xff) {
			if (c > 0x8f) {
				mqc->c += 0xff00;
				mqc->ct = 8;
			} else {
				mqc->bp++;
				mqc->c += c << 9;
				mqc->ct = 7;
			}
		} else {
			mqc->bp++;
			mqc->c += c << 8;
			mqc->ct = 8;
		}
	} else {
		mqc->c += 0xff00;
		mqc->ct = 8;
	}
}
#endif

/**
Renormalize mqc->a and mqc->c while decoding
@param mqc MQC handle
*/
static INLINE void opj_mqc_renormd(opj_mqc_t *const mqc) {
	do {
		if (mqc->ct == 0) {
			opj_mqc_bytein(mqc);
		}
		mqc->a <<= 1;
		mqc->c <<= 1;
		mqc->ct--;
	} while (mqc->a < 0x8000);
}

/**
Decode a symbol
@param mqc MQC handle
@return Returns the decoded symbol (0 or 1)
*/
static INLINE OPJ_INT32 opj_mqc_decode(opj_mqc_t *const mqc) {
	OPJ_INT32 d;
	mqc->a -= (*mqc->curctx)->qeval;
	if ((mqc->c >> 16) < (*mqc->curctx)->qeval) {
		d = opj_mqc_lpsexchange(mqc);
		opj_mqc_renormd(mqc);
	} else {
		mqc->c -= (*mqc->curctx)->qeval << 16;
		if ((mqc->a & 0x8000) == 0) {
			d = opj_mqc_mpsexchange(mqc);
			opj_mqc_renormd(mqc);
		} else {
			d = (OPJ_INT32)(*mqc->curctx)->mps;
		}
	}

	return d;
}

#endif /* __MQC_INL_H */
