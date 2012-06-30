/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "os_types.h"

#include <stddef.h>
#include <string.h> /* memset() */

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_arith_int.h"
#include "jbig2_arith_iaid.h"
#include "jbig2_huffman.h"
#include "jbig2_generic.h"
#include "jbig2_symbol_dict.h"
#include "jbig2_text.h"


/**
 * jbig2_decode_text_region: decode a text region segment
 *
 * @ctx: jbig2 decoder context
 * @segment: jbig2 segment (header) structure
 * @params: parameters from the text region header
 * @dicts: an array of referenced symbol dictionaries
 * @n_dicts: the number of referenced symbol dictionaries
 * @image: image structure in which to store the decoded region bitmap
 * @data: pointer to text region data to be decoded
 * @size: length of text region data
 *
 * Implements the text region decoding procedure
 * described in section 6.4 of the JBIG2 spec.
 *
 * returns: 0 on success
 **/
int
jbig2_decode_text_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
                             const Jbig2TextRegionParams *params,
                             const Jbig2SymbolDict * const *dicts, const int n_dicts,
                             Jbig2Image *image,
                             const byte *data, const size_t size,
			     Jbig2ArithCx *GR_stats, Jbig2ArithState *as, Jbig2WordStream *ws)
{
    /* relevent bits of 6.4.4 */
    uint32_t NINSTANCES;
    uint32_t ID;
    int32_t STRIPT;
    int32_t FIRSTS;
    int32_t DT;
    int32_t DFS;
    int32_t IDS;
    int32_t CURS;
    int32_t CURT;
    int S,T;
    int x,y;
    bool first_symbol;
    uint32_t index, SBNUMSYMS;
    Jbig2Image *IB = NULL;
    Jbig2HuffmanState *hs = NULL;
    Jbig2HuffmanTable *SBSYMCODES = NULL;
    int code = 0;
    int RI;

    SBNUMSYMS = 0;
    for (index = 0; index < n_dicts; index++) {
        SBNUMSYMS += dicts[index]->n_symbols;
    }
    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
        "symbol list contains %d glyphs in %d dictionaries", SBNUMSYMS, n_dicts);

    if (params->SBHUFF) {
	Jbig2HuffmanTable *runcodes = NULL;
	Jbig2HuffmanParams runcodeparams;
	Jbig2HuffmanLine runcodelengths[35];
	Jbig2HuffmanLine *symcodelengths = NULL;
	Jbig2HuffmanParams symcodeparams;
	int err, len, range, r;

	jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
	  "huffman coded text region");
	hs = jbig2_huffman_new(ctx, ws);
    if (hs == NULL)
    {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
            "failed to allocate storage for text region");
        return -1;
    }

	/* 7.4.3.1.7 - decode symbol ID Huffman table */
	/* this is actually part of the segment header, but it is more
	   convenient to handle it here */

	/* parse and build the runlength code huffman table */
	for (index = 0; index < 35; index++) {
	  runcodelengths[index].PREFLEN = jbig2_huffman_get_bits(hs, 4);
	  runcodelengths[index].RANGELEN = 0;
	  runcodelengths[index].RANGELOW = index;
	  jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
	    "  read runcode%d length %d", index, runcodelengths[index].PREFLEN);
	}
	runcodeparams.HTOOB = 0;
	runcodeparams.lines = runcodelengths;
	runcodeparams.n_lines = 35;
	runcodes = jbig2_build_huffman_table(ctx, &runcodeparams);
	if (runcodes == NULL) {
	  jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
	    "error constructing symbol id runcode table!");
	  code = -1;
      goto cleanup1;
	}

	/* decode the symbol id codelengths using the runlength table */
	symcodelengths = jbig2_new(ctx, Jbig2HuffmanLine, SBNUMSYMS);
	if (symcodelengths == NULL) {
	  jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
	    "memory allocation failure reading symbol ID huffman table!");
	  code = -1;
      goto cleanup1;
	}
	index = 0;
	while (index < SBNUMSYMS) {
	  code = jbig2_huffman_get(hs, runcodes, &err);
	  if (err != 0 || code < 0 || code >= 35) {
	    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
	      "error reading symbol ID huffman table!");
	    code = err ? err : -1;
        goto cleanup1;
	  }

	  if (code < 32) {
	    len = code;
	    range = 1;
	  } else {
	    if (code == 32) {
	      len = symcodelengths[index-1].PREFLEN;
	      if (index < 1) {
		jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
	 	  "error decoding symbol id table: run length with no antecedent!");
	        code = -1;
            goto cleanup1;
	      }
	    } else {
	      len = 0; /* code == 33 or 34 */
	    }
	    if (code == 32) range = jbig2_huffman_get_bits(hs, 2) + 3;
	    else if (code == 33) range = jbig2_huffman_get_bits(hs, 3) + 3;
	    else if (code == 34) range = jbig2_huffman_get_bits(hs, 7) + 11;
	  }
	  jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
	    "  read runcode%d at index %d (length %d range %d)", code, index, len, range);
	  if (index+range > SBNUMSYMS) {
	    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
	      "runlength extends %d entries beyond the end of symbol id table!",
		index+range - SBNUMSYMS);
	    range = SBNUMSYMS - index;
	  }
	  for (r = 0; r < range; r++) {
	    symcodelengths[index+r].PREFLEN = len;
	    symcodelengths[index+r].RANGELEN = 0;
	    symcodelengths[index+r].RANGELOW = index + r;
	  }
	  index += r;
	}

	if (index < SBNUMSYMS) {
	  jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
	    "runlength codes do not cover the available symbol set");
	}
	symcodeparams.HTOOB = 0;
	symcodeparams.lines = symcodelengths;
	symcodeparams.n_lines = SBNUMSYMS;

	/* skip to byte boundary */
	jbig2_huffman_skip(hs);

	/* finally, construct the symbol id huffman table itself */
	SBSYMCODES = jbig2_build_huffman_table(ctx, &symcodeparams);

cleanup1:
	jbig2_free(ctx->allocator, symcodelengths);
	jbig2_release_huffman_table(ctx, runcodes);

	if (SBSYMCODES == NULL) {
	    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "could not construct Symbol ID huffman table!");
        jbig2_huffman_free(ctx, hs);
	    return ((code != 0) ? code : -1);
	}
    }

    /* 6.4.5 (1) */
    jbig2_image_clear(ctx, image, params->SBDEFPIXEL);

    /* 6.4.6 */
    if (params->SBHUFF) {
        STRIPT = jbig2_huffman_get(hs, params->SBHUFFDT, &code);
    } else {
        code = jbig2_arith_int_decode(params->IADT, as, &STRIPT);
    }
    if (code < 0) goto cleanup2;

    /* 6.4.5 (2) */
    STRIPT *= -(params->SBSTRIPS);
    FIRSTS = 0;
    NINSTANCES = 0;

    /* 6.4.5 (3) */
    while (NINSTANCES < params->SBNUMINSTANCES) {
        /* (3b) */
        if (params->SBHUFF) {
            DT = jbig2_huffman_get(hs, params->SBHUFFDT, &code);
        } else {
            code = jbig2_arith_int_decode(params->IADT, as, &DT);
        }
        if (code < 0) goto cleanup2;
        DT *= params->SBSTRIPS;
        STRIPT += DT;

	first_symbol = TRUE;
	/* 6.4.5 (3c) - decode symbols in strip */
	for (;;) {
	    /* (3c.i) */
	    if (first_symbol) {
		/* 6.4.7 */
		if (params->SBHUFF) {
		    DFS = jbig2_huffman_get(hs, params->SBHUFFFS, &code);
		} else {
		    code = jbig2_arith_int_decode(params->IAFS, as, &DFS);
		}
                if (code < 0) goto cleanup2;
		FIRSTS += DFS;
		CURS = FIRSTS;
		first_symbol = FALSE;
	    } else {
                if (NINSTANCES > params->SBNUMINSTANCES) {
                    code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                        "too many NINSTANCES (%d) decoded", NINSTANCES);
                    break;
		}
		/* (3c.ii) / 6.4.8 */
		if (params->SBHUFF) {
		    IDS = jbig2_huffman_get(hs, params->SBHUFFDS, &code);
		} else {
		    code = jbig2_arith_int_decode(params->IADS, as, &IDS);
		}
		if (code) {
                    /* decoded an OOB, reached end of strip */
		    break;
		}
		CURS += IDS + params->SBDSOFFSET;
	    }

	    /* (3c.iii) / 6.4.9 */
	    if (params->SBSTRIPS == 1) {
		CURT = 0;
	    } else if (params->SBHUFF) {
		CURT = jbig2_huffman_get_bits(hs, params->LOGSBSTRIPS);
	    } else {
		code = jbig2_arith_int_decode(params->IAIT, as, &CURT);
                if (code < 0) goto cleanup2;
	    }
	    T = STRIPT + CURT;

	    /* (3b.iv) / 6.4.10 - decode the symbol id */
	    if (params->SBHUFF) {
		ID = jbig2_huffman_get(hs, SBSYMCODES, &code);
	    } else {
		code = jbig2_arith_iaid_decode(params->IAID, as, (int *)&ID);
	    }
            if (code < 0) goto cleanup2;
	    if (ID >= SBNUMSYMS) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                "symbol id out of range! (%d/%d)", ID, SBNUMSYMS);
            goto cleanup2;
	    }

	    /* (3c.v) / 6.4.11 - look up the symbol bitmap IB */
	    {
		uint32_t id = ID;

		index = 0;
		while (id >= dicts[index]->n_symbols)
		    id -= dicts[index++]->n_symbols;
		IB = jbig2_image_clone(ctx, dicts[index]->glyphs[id]);
	    }
	    if (params->SBREFINE) {
	      if (params->SBHUFF) {
		RI = jbig2_huffman_get_bits(hs, 1);
	      } else {
		code = jbig2_arith_int_decode(params->IARI, as, &RI);
        if (code < 0) goto cleanup2;
	      }
	    } else {
		RI = 0;
	    }
	    if (RI) {
		Jbig2RefinementRegionParams rparams;
		Jbig2Image *IBO;
		int32_t RDW, RDH, RDX, RDY;
		Jbig2Image *refimage;
		int BMSIZE = 0;
		int code1 = 0;
		int code2 = 0;
		int code3 = 0;
		int code4 = 0;
		int code5 = 0;

		/* 6.4.11 (1, 2, 3, 4) */
		if (!params->SBHUFF) {
		  code1 = jbig2_arith_int_decode(params->IARDW, as, &RDW);
		  code2 = jbig2_arith_int_decode(params->IARDH, as, &RDH);
		  code3 = jbig2_arith_int_decode(params->IARDX, as, &RDX);
		  code4 = jbig2_arith_int_decode(params->IARDY, as, &RDY);
		} else {
		  RDW = jbig2_huffman_get(hs, params->SBHUFFRDW, &code1);
		  RDH = jbig2_huffman_get(hs, params->SBHUFFRDH, &code2);
		  RDX = jbig2_huffman_get(hs, params->SBHUFFRDX, &code3);
		  RDY = jbig2_huffman_get(hs, params->SBHUFFRDY, &code4);
		  BMSIZE = jbig2_huffman_get(hs, params->SBHUFFRSIZE, &code5);
		  jbig2_huffman_skip(hs);
		}

		if ((code1 < 0) || (code2 < 0) || (code3 < 0) || (code4 < 0) || (code5 < 0))
		{
		    code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
		        "failed to decode data");
		    goto cleanup2;
		}

		/* 6.4.11 (6) */
		IBO = IB;
		refimage = jbig2_image_new(ctx, IBO->width + RDW,
						IBO->height + RDH);
		if (refimage == NULL) {
		  jbig2_image_release(ctx, IBO);
		  if (params->SBHUFF) {
		    jbig2_release_huffman_table(ctx, SBSYMCODES);
		  }
		  return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, 
			segment->number,
			"couldn't allocate reference image");
	        }

		/* Table 12 */
		rparams.GRTEMPLATE = params->SBRTEMPLATE;
		rparams.reference = IBO;
		rparams.DX = (RDW >> 1) + RDX;
		rparams.DY = (RDH >> 1) + RDY;
		rparams.TPGRON = 0;
		memcpy(rparams.grat, params->sbrat, 4);
		jbig2_decode_refinement_region(ctx, segment,
		    &rparams, as, refimage, GR_stats);
		IB = refimage;

		jbig2_image_release(ctx, IBO);

		/* 6.4.11 (7) */
		if (params->SBHUFF) {
		  jbig2_huffman_advance(hs, BMSIZE);
		}

	    }

	    /* (3c.vi) */
	    if ((!params->TRANSPOSED) && (params->REFCORNER > 1)) {
		CURS += IB->width - 1;
	    } else if ((params->TRANSPOSED) && !(params->REFCORNER & 1)) {
		CURS += IB->height - 1;
	    }

	    /* (3c.vii) */
	    S = CURS;

	    /* (3c.viii) */
	    if (!params->TRANSPOSED) {
		switch (params->REFCORNER) {
		case JBIG2_CORNER_TOPLEFT: x = S; y = T; break;
		case JBIG2_CORNER_TOPRIGHT: x = S - IB->width + 1; y = T; break;
		case JBIG2_CORNER_BOTTOMLEFT: x = S; y = T - IB->height + 1; break;
		case JBIG2_CORNER_BOTTOMRIGHT: x = S - IB->width + 1; y = T - IB->height + 1; break;
		}
	    } else { /* TRANSPOSED */
		switch (params->REFCORNER) {
		case JBIG2_CORNER_TOPLEFT: x = T; y = S; break;
		case JBIG2_CORNER_TOPRIGHT: x = T - IB->width + 1; y = S; break;
		case JBIG2_CORNER_BOTTOMLEFT: x = T; y = S - IB->height + 1; break;
		case JBIG2_CORNER_BOTTOMRIGHT: x = T - IB->width + 1; y = S - IB->height + 1; break;
		}
	    }

	    /* (3c.ix) */
#ifdef JBIG2_DEBUG
	    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
			"composing glyph id %d: %dx%d @ (%d,%d) symbol %d/%d",
			ID, IB->width, IB->height, x, y, NINSTANCES + 1,
			params->SBNUMINSTANCES);
#endif
	    code = jbig2_image_compose(ctx, image, IB, x, y, params->SBCOMBOP);
            if (code < 0) goto cleanup2;

	    /* (3c.x) */
	    if ((!params->TRANSPOSED) && (params->REFCORNER < 2)) {
		CURS += IB->width -1 ;
	    } else if ((params->TRANSPOSED) && (params->REFCORNER & 1)) {
		CURS += IB->height - 1;
	    }

	    /* (3c.xi) */
	    NINSTANCES++;

	    jbig2_image_release(ctx, IB);
	}
        /* end strip */
    }
    /* 6.4.5 (4) */

cleanup2:
    if (params->SBHUFF) {
      jbig2_release_huffman_table(ctx, SBSYMCODES);
    }
    jbig2_huffman_free(ctx, hs);

    return code;
}

/**
 * jbig2_text_region: read a text region segment header
 **/
int
jbig2_text_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data)
{
    int offset = 0;
    Jbig2RegionSegmentInfo region_info;
    Jbig2TextRegionParams params;
    Jbig2Image *image = NULL;
    Jbig2SymbolDict **dicts = NULL;
    int n_dicts = 0;
    uint16_t flags = 0;
    uint16_t huffman_flags = 0;
    Jbig2ArithCx *GR_stats = NULL;
    int code = 0;
    Jbig2WordStream *ws = NULL;
    Jbig2ArithState *as = NULL;
    int table_index = 0;
    const Jbig2HuffmanParams *huffman_params = NULL;

    /* 7.4.1 */
    if (segment->data_length < 17)
        goto too_short;
    jbig2_get_region_segment_info(&region_info, segment_data);
    offset += 17;

    /* 7.4.3.1.1 */
    flags = jbig2_get_uint16(segment_data + offset);
    offset += 2;

    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
	"text region header flags 0x%04x", flags);

    /* zero params to ease cleanup later */
    memset(&params, 0, sizeof(Jbig2TextRegionParams));

    params.SBHUFF = flags & 0x0001;
    params.SBREFINE = flags & 0x0002;
    params.LOGSBSTRIPS = (flags & 0x000c) >> 2;
    params.SBSTRIPS = 1 << params.LOGSBSTRIPS;
    params.REFCORNER = (Jbig2RefCorner)((flags & 0x0030) >> 4);
    params.TRANSPOSED = flags & 0x0040;
    params.SBCOMBOP = (Jbig2ComposeOp)((flags & 0x0180) >> 7);
    params.SBDEFPIXEL = flags & 0x0200;
    /* SBDSOFFSET is a signed 5 bit integer */
    params.SBDSOFFSET = (flags & 0x7C00) >> 10;
    if (params.SBDSOFFSET > 0x0f) params.SBDSOFFSET -= 0x20;
    params.SBRTEMPLATE = flags & 0x8000;

    if (params.SBDSOFFSET) {
      jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
	"text region has SBDSOFFSET %d", params.SBDSOFFSET);
    }

    if (params.SBHUFF)	/* Huffman coding */
      {
        /* 7.4.3.1.2 */
        huffman_flags = jbig2_get_uint16(segment_data + offset);
        offset += 2;

        if (huffman_flags & 0x8000)
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
                "reserved bit 15 of text region huffman flags is not zero");
      }
    else	/* arithmetic coding */
      {
        /* 7.4.3.1.3 */
        if ((params.SBREFINE) && !(params.SBRTEMPLATE))
          {
            params.sbrat[0] = segment_data[offset];
            params.sbrat[1] = segment_data[offset + 1];
            params.sbrat[2] = segment_data[offset + 2];
            params.sbrat[3] = segment_data[offset + 3];
            offset += 4;
	  }
      }

    /* 7.4.3.1.4 */
    params.SBNUMINSTANCES = jbig2_get_uint32(segment_data + offset);
    offset += 4;

    if (params.SBHUFF) {
        /* 7.4.3.1.5 - Symbol ID Huffman table */
        /* ...this is handled in the segment body decoder */

        /* 7.4.3.1.6 - Other Huffman table selection */
	switch (huffman_flags & 0x0003) {
	  case 0: /* Table B.6 */
	    params.SBHUFFFS = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_F);
	    break;
	  case 1: /* Table B.7 */
	    params.SBHUFFFS = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_G);
	    break;
	  case 3: /* Custom table from referred segment */
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Custom FS huffman table not found (%d)", table_index);
                goto cleanup1;
            }
            params.SBHUFFFS = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
            break;
	  case 2: /* invalid */
	  default:
	    code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "text region specified invalid FS huffman table");
        goto cleanup1;
	    break;
	}
    if (params.SBHUFFFS == NULL)
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "failed to allocate text region specified FS huffman table");
        goto cleanup1;
    }

	switch ((huffman_flags & 0x000c) >> 2) {
	  case 0: /* Table B.8 */
	    params.SBHUFFDS = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_H);
	    break;
	  case 1: /* Table B.9 */
	    params.SBHUFFDS = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_I);
	    break;
	  case 2: /* Table B.10 */
	    params.SBHUFFDS = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_J);
	    break;
	  case 3: /* Custom table from referred segment */
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Custom DS huffman table not found (%d)", table_index);
                goto cleanup1;
            }
            params.SBHUFFDS = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
            break;
	}
    if (params.SBHUFFDS == NULL)
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "failed to allocate text region specified DS huffman table");
        goto cleanup1;
    }

	switch ((huffman_flags & 0x0030) >> 4) {
	  case 0: /* Table B.11 */
	    params.SBHUFFDT = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_K);
	    break;
	  case 1: /* Table B.12 */
	    params.SBHUFFDT = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_L);
	    break;
	  case 2: /* Table B.13 */
	    params.SBHUFFDT = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_M);
	    break;
	  case 3: /* Custom table from referred segment */
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Custom DT huffman table not found (%d)", table_index);
                goto cleanup1;
            }
            params.SBHUFFDT = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
            break;
	}
    if (params.SBHUFFDT == NULL)
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "failed to allocate text region specified DT huffman table");
        goto cleanup1;
    }

	switch ((huffman_flags & 0x00c0) >> 6) {
	  case 0: /* Table B.14 */
	    params.SBHUFFRDW = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_N);
	    break;
	  case 1: /* Table B.15 */
	    params.SBHUFFRDW = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_O);
	    break;
	  case 3: /* Custom table from referred segment */
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Custom RDW huffman table not found (%d)", table_index);
                goto cleanup1;
            }
            params.SBHUFFRDW = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
	    break;
	  case 2: /* invalid */
	  default:
	    code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "text region specified invalid RDW huffman table");
        goto cleanup1;
	    break;
	}
    if (params.SBHUFFRDW == NULL)
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "failed to allocate text region specified RDW huffman table");
        goto cleanup1;
    }

	switch ((huffman_flags & 0x0300) >> 8) {
	  case 0: /* Table B.14 */
	    params.SBHUFFRDH = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_N);
	    break;
	  case 1: /* Table B.15 */
	    params.SBHUFFRDH = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_O);
	    break;
	  case 3: /* Custom table from referred segment */
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Custom RDH huffman table not found (%d)", table_index);
                goto cleanup1;
            }
            params.SBHUFFRDH = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
	    break;
	  case 2: /* invalid */
	  default:
	    code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "text region specified invalid RDH huffman table");
        goto cleanup1;
	    break;
	}
    if (params.SBHUFFRDH == NULL)
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "failed to allocate text region specified RDH huffman table");
        goto cleanup1;
    }

    switch ((huffman_flags & 0x0c00) >> 10) {
	  case 0: /* Table B.14 */
	    params.SBHUFFRDX = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_N);
	    break;
	  case 1: /* Table B.15 */
	    params.SBHUFFRDX = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_O);
	    break;
	  case 3: /* Custom table from referred segment */
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Custom RDX huffman table not found (%d)", table_index);
                goto cleanup1;
            }
            params.SBHUFFRDX = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
	    break;
	  case 2: /* invalid */
	  default:
	    code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "text region specified invalid RDX huffman table");
        goto cleanup1;
	    break;
	}
    if (params.SBHUFFRDX == NULL)
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "failed to allocate text region specified RDX huffman table");
        goto cleanup1;
    }

	switch ((huffman_flags & 0x3000) >> 12) {
	  case 0: /* Table B.14 */
	    params.SBHUFFRDY = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_N);
	    break;
	  case 1: /* Table B.15 */
	    params.SBHUFFRDY = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_O);
	    break;
	  case 3: /* Custom table from referred segment */
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Custom RDY huffman table not found (%d)", table_index);
                goto cleanup1;
            }
            params.SBHUFFRDY = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
	    break;
	  case 2: /* invalid */
	  default:
	    code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "text region specified invalid RDY huffman table");
        goto cleanup1;
	    break;
	}
    if (params.SBHUFFRDY == NULL)
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "failed to allocate text region specified RDY huffman table");
        goto cleanup1;
    }

	switch ((huffman_flags & 0x4000) >> 14) {
	  case 0: /* Table B.1 */
	    params.SBHUFFRSIZE = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_A);
	    break;
	  case 1: /* Custom table from referred segment */
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Custom RSIZE huffman table not found (%d)", table_index);
                goto cleanup1;
            }
            params.SBHUFFRSIZE = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
	    break;
	}
    if (params.SBHUFFRSIZE == NULL)
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "failed to allocate text region specified RSIZE huffman table");
        goto cleanup1;
    }

        if (huffman_flags & 0x8000) {
	  jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
 	    "text region huffman flags bit 15 is set, contrary to spec");
	}

        /* 7.4.3.1.7 */
        /* For convenience this is done in the body decoder routine */
    }

    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
        "text region: %d x %d @ (%d,%d) %d symbols",
        region_info.width, region_info.height,
        region_info.x, region_info.y, params.SBNUMINSTANCES);

    /* 7.4.3.2 (2) - compose the list of symbol dictionaries */
    n_dicts = jbig2_sd_count_referred(ctx, segment);
    if (n_dicts != 0) {
        dicts = jbig2_sd_list_referred(ctx, segment);
    } else {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "text region refers to no symbol dictionaries!");
        goto cleanup1;
    }
    if (dicts == NULL) {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "unable to retrive symbol dictionaries! previous parsing error?");
        goto cleanup1;
    } else {
	int index;
	if (dicts[0] == NULL) {
        code =jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
            "unable to find first referenced symbol dictionary!");
        goto cleanup1;
	}
	for (index = 1; index < n_dicts; index++)
	    if (dicts[index] == NULL) {
		jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
			"unable to find all referenced symbol dictionaries!");
	    n_dicts = index;
	}
    }

    /* 7.4.3.2 (3) */
    if (!params.SBHUFF && params.SBREFINE) {
	int stats_size = params.SBRTEMPLATE ? 1 << 10 : 1 << 13;
	GR_stats = jbig2_new(ctx, Jbig2ArithCx, stats_size);
    if (GR_stats == NULL)
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "could not allocate GR_stats");
        goto cleanup1;
    }
	memset(GR_stats, 0, stats_size);
    }

    image = jbig2_image_new(ctx, region_info.width, region_info.height);
    if (image == NULL) {
        code =jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "couldn't allocate text region image");
        goto cleanup1;
    }

    ws = jbig2_word_stream_buf_new(ctx, segment_data + offset, segment->data_length - offset);
    if (ws == NULL)
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "couldn't allocate ws in text region image");
        goto cleanup2;
    }

    as = jbig2_arith_new(ctx, ws);
    if (as == NULL)
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "couldn't allocate as in text region image");
        goto cleanup2;
    }

    if (!params.SBHUFF) {
	int SBSYMCODELEN, index;
        int SBNUMSYMS = 0;
	for (index = 0; index < n_dicts; index++) {
	    SBNUMSYMS += dicts[index]->n_symbols;
	}

    params.IADT = jbig2_arith_int_ctx_new(ctx);
    params.IAFS = jbig2_arith_int_ctx_new(ctx);
    params.IADS = jbig2_arith_int_ctx_new(ctx);
    params.IAIT = jbig2_arith_int_ctx_new(ctx);
    if ((params.IADT == NULL) || (params.IAFS == NULL) ||
        (params.IADS == NULL) || (params.IAIT == NULL))
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "couldn't allocate text region image data");
        goto cleanup3;
    }

	/* Table 31 */
	for (SBSYMCODELEN = 0; (1 << SBSYMCODELEN) < SBNUMSYMS; SBSYMCODELEN++);
    params.IAID = jbig2_arith_iaid_ctx_new(ctx, SBSYMCODELEN);
	params.IARI = jbig2_arith_int_ctx_new(ctx);
	params.IARDW = jbig2_arith_int_ctx_new(ctx);
	params.IARDH = jbig2_arith_int_ctx_new(ctx);
	params.IARDX = jbig2_arith_int_ctx_new(ctx);
	params.IARDY = jbig2_arith_int_ctx_new(ctx);
    if ((params.IAID == NULL) || (params.IARI == NULL) ||
        (params.IARDW == NULL) || (params.IARDH == NULL) ||
        (params.IARDX == NULL) || (params.IARDY == NULL))
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "couldn't allocate text region image data");
        goto cleanup4;
    }
    }

    code = jbig2_decode_text_region(ctx, segment, &params,
        (const Jbig2SymbolDict * const *)dicts, n_dicts, image,
        segment_data + offset, segment->data_length - offset,
		GR_stats, as, ws);
    if (code < 0)
    {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "failed to decode text region image data");
        goto cleanup4;
    }

    if ((segment->flags & 63) == 4) {
        /* we have an intermediate region here. save it for later */
        segment->result = jbig2_image_clone(ctx, image);
    } else {
        /* otherwise composite onto the page */
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
            "composing %dx%d decoded text region onto page at (%d, %d)",
            region_info.width, region_info.height, region_info.x, region_info.y);
        jbig2_page_add_result(ctx, &ctx->pages[ctx->current_page], image,
            region_info.x, region_info.y, region_info.op);
    }

cleanup4:
    if (!params.SBHUFF) {
        jbig2_arith_iaid_ctx_free(ctx, params.IAID);
        jbig2_arith_int_ctx_free(ctx, params.IARI);
        jbig2_arith_int_ctx_free(ctx, params.IARDW);
        jbig2_arith_int_ctx_free(ctx, params.IARDH);
        jbig2_arith_int_ctx_free(ctx, params.IARDX);
        jbig2_arith_int_ctx_free(ctx, params.IARDY);
    }

cleanup3:
    if (!params.SBHUFF) {
        jbig2_arith_int_ctx_free(ctx, params.IADT);
        jbig2_arith_int_ctx_free(ctx, params.IAFS);
        jbig2_arith_int_ctx_free(ctx, params.IADS);
        jbig2_arith_int_ctx_free(ctx, params.IAIT);
    }
    jbig2_free(ctx->allocator, as);
    jbig2_word_stream_buf_free(ctx, ws);

cleanup2:
    if (!params.SBHUFF && params.SBREFINE) {
        jbig2_free(ctx->allocator, GR_stats);
    }
    jbig2_image_release(ctx, image);

cleanup1:
    if (params.SBHUFF) {
        jbig2_release_huffman_table(ctx, params.SBHUFFFS);
        jbig2_release_huffman_table(ctx, params.SBHUFFDS);
        jbig2_release_huffman_table(ctx, params.SBHUFFDT);
        jbig2_release_huffman_table(ctx, params.SBHUFFRDX);
        jbig2_release_huffman_table(ctx, params.SBHUFFRDY);
        jbig2_release_huffman_table(ctx, params.SBHUFFRDW);
        jbig2_release_huffman_table(ctx, params.SBHUFFRDH);
        jbig2_release_huffman_table(ctx, params.SBHUFFRSIZE);
    }
    jbig2_free(ctx->allocator, dicts);

    return code;

too_short:
    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
        "Segment too short");
}
