/*
    jbig2dec

    Copyright (C) 2002-2008 Artifex Software, Inc.

    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.

    For further licensing information refer to http://artifex.com/ or
    contact Artifex Software, Inc., 7 Mt. Lassen Drive - Suite A-134,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.
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
    Jbig2Image *IB;
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
	Jbig2HuffmanTable *runcodes;
	Jbig2HuffmanParams runcodeparams;
	Jbig2HuffmanLine runcodelengths[35];
	Jbig2HuffmanLine *symcodelengths;
	Jbig2HuffmanParams symcodeparams;
	int code, err, len, range, r;

	jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
	  "huffman coded text region");
	hs = jbig2_huffman_new(ctx, ws);

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
	  return -1;
	}

	/* decode the symbol id codelengths using the runlength table */
	symcodelengths = jbig2_new(ctx, Jbig2HuffmanLine, SBNUMSYMS);
	if (symcodelengths == NULL) {
	  jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
	    "memory allocation failure reading symbol ID huffman table!");
	  return -1;
	}
	index = 0;
	while (index < SBNUMSYMS) {
	  code = jbig2_huffman_get(hs, runcodes, &err);
	  if (err != 0 || code < 0 || code >= 35) {
	    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
	      "error reading symbol ID huffman table!");
	    return err ? err : -1;
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
	        /* todo: memory cleanup */
	        return -1;
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

	jbig2_free(ctx->allocator, symcodelengths);
	jbig2_release_huffman_table(ctx, runcodes);

	if (SBSYMCODES == NULL) {
	    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
		"could not construct Symbol ID huffman table!");
	    return -1;
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
		FIRSTS += DFS;
		CURS = FIRSTS;
		first_symbol = FALSE;

	    } else {
		/* (3c.ii) / 6.4.8 */
		if (params->SBHUFF) {
		    IDS = jbig2_huffman_get(hs, params->SBHUFFDS, &code);
		} else {
		    code = jbig2_arith_int_decode(params->IADS, as, &IDS);
		}
		if (code) {
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
	    }
	    T = STRIPT + CURT;

	    /* (3b.iv) / 6.4.10 - decode the symbol id */
	    if (params->SBHUFF) {
		ID = jbig2_huffman_get(hs, SBSYMCODES, &code);
	    } else {
		code = jbig2_arith_iaid_decode(params->IAID, as, (int *)&ID);
	    }
	    if (ID >= SBNUMSYMS) {
		return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "symbol id out of range! (%d/%d)", ID, SBNUMSYMS);
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

		/* 6.4.11 (1, 2, 3, 4) */
		if (!params->SBHUFF) {
		  code = jbig2_arith_int_decode(params->IARDW, as, &RDW);
		  code = jbig2_arith_int_decode(params->IARDH, as, &RDH);
		  code = jbig2_arith_int_decode(params->IARDX, as, &RDX);
		  code = jbig2_arith_int_decode(params->IARDY, as, &RDY);
		} else {
		  RDW = jbig2_huffman_get(hs, params->SBHUFFRDW, &code);
		  RDH = jbig2_huffman_get(hs, params->SBHUFFRDH, &code);
		  RDX = jbig2_huffman_get(hs, params->SBHUFFRDX, &code);
		  RDY = jbig2_huffman_get(hs, params->SBHUFFRDY, &code);
		  BMSIZE = jbig2_huffman_get(hs, params->SBHUFFRSIZE, &code);
		  jbig2_huffman_skip(hs);
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
	    jbig2_image_compose(ctx, image, IB, x, y, params->SBCOMBOP);

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

    if (params->SBHUFF) {
      jbig2_release_huffman_table(ctx, SBSYMCODES);
    }

    return 0;
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
    Jbig2Image *image;
    Jbig2SymbolDict **dicts;
    int n_dicts;
    uint16_t flags;
    uint16_t huffman_flags = 0;
    Jbig2ArithCx *GR_stats = NULL;
    int code = 0;
    Jbig2WordStream *ws = NULL;
    Jbig2ArithState *as = NULL;
    int table_index = 0;
    const Jbig2HuffmanParams *huffman_params;

    /* 7.4.1 */
    if (segment->data_length < 17)
        goto too_short;
    jbig2_get_region_segment_info(&region_info, segment_data);
    offset += 17;

    /* 7.4.3.1.1 */
    flags = jbig2_get_int16(segment_data + offset);
    offset += 2;

    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
	"text region header flags 0x%04x", flags);

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
        huffman_flags = jbig2_get_int16(segment_data + offset);
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
	  } else {
	    /* zero these for the sake of later debug messages */
	    memset(params.sbrat, 0, sizeof(params.sbrat));
	  }
      }

    /* 7.4.3.1.4 */
    params.SBNUMINSTANCES = jbig2_get_int32(segment_data + offset);
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
	    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Custom FS huffman table not found (%d)", table_index);
            }
            params.SBHUFFFS = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
            /* FIXME: this function leaks memory when error happens.
               i.e. not calling jbig2_release_huffman_table() */
	    break;
	  case 2: /* invalid */
	  default:
	    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
		"text region specified invalid FS huffman table");
	    break;
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
	    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Custom DS huffman table not found (%d)", table_index);
            }
            params.SBHUFFDS = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
	    break;
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
	    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Custom DT huffman table not found (%d)", table_index);
            }
            params.SBHUFFDT = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
	    break;
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
	    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Custom RDW huffman table not found (%d)", table_index);
            }
            params.SBHUFFRDW = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
	    break;
	  case 2: /* invalid */
	  default:
	    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
		"text region specified invalid RDW huffman table");
	    break;
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
	    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Custom RDH huffman table not found (%d)", table_index);
            }
            params.SBHUFFRDH = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
	    break;
	  case 2: /* invalid */
	  default:
	    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
		"text region specified invalid RDH huffman table");
	    break;
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
	    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Custom RDX huffman table not found (%d)", table_index);
            }
            params.SBHUFFRDX = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
	    break;
	  case 2: /* invalid */
	  default:
	    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
		"text region specified invalid RDX huffman table");
	    break;
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
	    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Custom RDY huffman table not found (%d)", table_index);
            }
            params.SBHUFFRDY = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
	    break;
	  case 2: /* invalid */
	  default:
	    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
		"text region specified invalid RDY huffman table");
	    break;
	}
	switch ((huffman_flags & 0x4000) >> 14) {
	  case 0: /* Table B.1 */
	    params.SBHUFFRSIZE = jbig2_build_huffman_table(ctx,
			&jbig2_huffman_params_A);
	    break;
	  case 1: /* Custom table from referred segment */
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
	    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Custom RSIZE huffman table not found (%d)", table_index);
            }
            params.SBHUFFRSIZE = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
	    break;
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
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                "text region refers to no symbol dictionaries!");
    }
    if (dicts == NULL) {
	return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
		"unable to retrive symbol dictionaries!"
		" previous parsing error?");
    } else {
	int index;
	if (dicts[0] == NULL) {
	    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING,
			segment->number,
                        "unable to find first referenced symbol dictionary!");
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
	memset(GR_stats, 0, stats_size);
    }

    image = jbig2_image_new(ctx, region_info.width, region_info.height);
    if (image == NULL) {
      if (!params.SBHUFF && params.SBREFINE) {
	jbig2_free(ctx->allocator, GR_stats);
      } else if (params.SBHUFF) {
	jbig2_release_huffman_table(ctx, params.SBHUFFFS);
	jbig2_release_huffman_table(ctx, params.SBHUFFDS);
	jbig2_release_huffman_table(ctx, params.SBHUFFDT);
	jbig2_release_huffman_table(ctx, params.SBHUFFRDX);
	jbig2_release_huffman_table(ctx, params.SBHUFFRDY);
	jbig2_release_huffman_table(ctx, params.SBHUFFRDW);
	jbig2_release_huffman_table(ctx, params.SBHUFFRDH);
	jbig2_release_huffman_table(ctx, params.SBHUFFRSIZE);
      }
      return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
		"couldn't allocate text region image");
    }

    ws = jbig2_word_stream_buf_new(ctx, segment_data + offset, segment->data_length - offset);
    if (!params.SBHUFF) {
	int SBSYMCODELEN, index;
        int SBNUMSYMS = 0;
	for (index = 0; index < n_dicts; index++) {
	    SBNUMSYMS += dicts[index]->n_symbols;
	}

	as = jbig2_arith_new(ctx, ws);

        params.IADT = jbig2_arith_int_ctx_new(ctx);
        params.IAFS = jbig2_arith_int_ctx_new(ctx);
        params.IADS = jbig2_arith_int_ctx_new(ctx);
        params.IAIT = jbig2_arith_int_ctx_new(ctx);
	/* Table 31 */
	for (SBSYMCODELEN = 0; (1 << SBSYMCODELEN) < SBNUMSYMS; SBSYMCODELEN++);
        params.IAID = jbig2_arith_iaid_ctx_new(ctx, SBSYMCODELEN);
	params.IARI = jbig2_arith_int_ctx_new(ctx);
	params.IARDW = jbig2_arith_int_ctx_new(ctx);
	params.IARDH = jbig2_arith_int_ctx_new(ctx);
	params.IARDX = jbig2_arith_int_ctx_new(ctx);
	params.IARDY = jbig2_arith_int_ctx_new(ctx);
    }

    code = jbig2_decode_text_region(ctx, segment, &params,
                (const Jbig2SymbolDict * const *)dicts, n_dicts, image,
                segment_data + offset, segment->data_length - offset,
		GR_stats, as, as ? NULL : ws);

    if (!params.SBHUFF && params.SBREFINE) {
	jbig2_free(ctx->allocator, GR_stats);
    }

    if (params.SBHUFF) {
      jbig2_release_huffman_table(ctx, params.SBHUFFFS);
      jbig2_release_huffman_table(ctx, params.SBHUFFDS);
      jbig2_release_huffman_table(ctx, params.SBHUFFDT);
      jbig2_release_huffman_table(ctx, params.SBHUFFRDX);
      jbig2_release_huffman_table(ctx, params.SBHUFFRDY);
      jbig2_release_huffman_table(ctx, params.SBHUFFRDW);
      jbig2_release_huffman_table(ctx, params.SBHUFFRDH);
      jbig2_release_huffman_table(ctx, params.SBHUFFRSIZE);
      jbig2_word_stream_buf_free(ctx, ws);
    }
    else {
	jbig2_arith_int_ctx_free(ctx, params.IADT);
	jbig2_arith_int_ctx_free(ctx, params.IAFS);
	jbig2_arith_int_ctx_free(ctx, params.IADS);
	jbig2_arith_int_ctx_free(ctx, params.IAIT);
	jbig2_arith_iaid_ctx_free(ctx, params.IAID);
	jbig2_arith_int_ctx_free(ctx, params.IARI);
	jbig2_arith_int_ctx_free(ctx, params.IARDW);
	jbig2_arith_int_ctx_free(ctx, params.IARDH);
	jbig2_arith_int_ctx_free(ctx, params.IARDX);
	jbig2_arith_int_ctx_free(ctx, params.IARDY);
	jbig2_free(ctx->allocator, as);
	jbig2_word_stream_buf_free(ctx, ws);
    }

    jbig2_free(ctx->allocator, dicts);

    /* todo: check errors */

    if ((segment->flags & 63) == 4) {
        /* we have an intermediate region here. save it for later */
        segment->result = image;
    } else {
        /* otherwise composite onto the page */
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
            "composing %dx%d decoded text region onto page at (%d, %d)",
            region_info.width, region_info.height, region_info.x, region_info.y);
	jbig2_page_add_result(ctx, &ctx->pages[ctx->current_page], image,
			      region_info.x, region_info.y, region_info.op);
        jbig2_image_release(ctx, image);
    }

    /* success */
    return 0;

    too_short:
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Segment too short");
}
