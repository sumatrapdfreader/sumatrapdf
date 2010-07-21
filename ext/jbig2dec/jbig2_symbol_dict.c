/*
    jbig2dec

    Copyright (C) 2001-2005 Artifex Software, Inc.

    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.

    For further licensing information refer to http://artifex.com/ or
    contact Artifex Software, Inc., 7 Mt. Lassen Drive - Suite A-134,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* symbol dictionary segment decode and support */

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
#include "jbig2_mmr.h"
#include "jbig2_symbol_dict.h"
#include "jbig2_text.h"

#if defined(OUTPUT_PBM) || defined(DUMP_SYMDICT)
#include <stdio.h>
#include "jbig2_image.h"
#endif

/* Table 13 */
typedef struct {
  bool SDHUFF;
  bool SDREFAGG;
  int32_t SDNUMINSYMS;
  Jbig2SymbolDict *SDINSYMS;
  uint32_t SDNUMNEWSYMS;
  uint32_t SDNUMEXSYMS;
  Jbig2HuffmanTable *SDHUFFDH;
  Jbig2HuffmanTable *SDHUFFDW;
  Jbig2HuffmanTable *SDHUFFBMSIZE;
  Jbig2HuffmanTable *SDHUFFAGGINST;
  int SDTEMPLATE;
  int8_t sdat[8];
  bool SDRTEMPLATE;
  int8_t sdrat[4];
} Jbig2SymbolDictParams;


/* Utility routines */

#ifdef DUMP_SYMDICT
void
jbig2_dump_symbol_dict(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
    Jbig2SymbolDict *dict = (Jbig2SymbolDict *)segment->result;
    int index;
    char filename[24];

    if (dict == NULL) return;
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
        "dumping symbol dict as %d individual png files\n", dict->n_symbols);
    for (index = 0; index < dict->n_symbols; index++) {
        snprintf(filename, sizeof(filename), "symbol_%02d-%04d.png",
		segment->number, index);
	jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
	  "dumping symbol %d/%d as '%s'", index, dict->n_symbols, filename);
#ifdef HAVE_LIBPNG
        jbig2_image_write_png_file(dict->glyphs[index], filename);
#else
        jbig2_image_write_pbm_file(dict->glyphs[index], filename);
#endif
    }
}
#endif /* DUMP_SYMDICT */

/* return a new empty symbol dict */
Jbig2SymbolDict *
jbig2_sd_new(Jbig2Ctx *ctx, int n_symbols)
{
   Jbig2SymbolDict *new = NULL;

   new = jbig2_new(ctx, Jbig2SymbolDict, 1);
   if (new != NULL) {
     new->glyphs = jbig2_new(ctx, Jbig2Image*, n_symbols);
     new->n_symbols = n_symbols;
   } else {
     return NULL;
   }

   if (new->glyphs != NULL) {
     memset(new->glyphs, 0, n_symbols*sizeof(Jbig2Image*));
   } else {
     jbig2_free(ctx->allocator, new);
     return NULL;
   }

   return new;
}

/* release the memory associated with a symbol dict */
void
jbig2_sd_release(Jbig2Ctx *ctx, Jbig2SymbolDict *dict)
{
   int i;

   if (dict == NULL) return;
   for (i = 0; i < dict->n_symbols; i++)
     if (dict->glyphs[i]) jbig2_image_release(ctx, dict->glyphs[i]);
   jbig2_free(ctx->allocator, dict->glyphs);
   jbig2_free(ctx->allocator, dict);
}

/* get a particular glyph by index */
Jbig2Image *
jbig2_sd_glyph(Jbig2SymbolDict *dict, unsigned int id)
{
   if (dict == NULL) return NULL;
   return dict->glyphs[id];
}

/* count the number of dictionary segments referred to by the given segment */
int
jbig2_sd_count_referred(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
    int index;
    Jbig2Segment *rsegment;
    int n_dicts = 0;

    for (index = 0; index < segment->referred_to_segment_count; index++) {
        rsegment = jbig2_find_segment(ctx, segment->referred_to_segments[index]);
        if (rsegment && ((rsegment->flags & 63) == 0)) n_dicts++;
    }

    return (n_dicts);
}

/* return an array of pointers to symbol dictionaries referred to by the given segment */
Jbig2SymbolDict **
jbig2_sd_list_referred(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
    int index;
    Jbig2Segment *rsegment;
    Jbig2SymbolDict **dicts;
    int n_dicts = jbig2_sd_count_referred(ctx, segment);
    int dindex = 0;

    dicts = jbig2_new(ctx, Jbig2SymbolDict*, n_dicts);
    for (index = 0; index < segment->referred_to_segment_count; index++) {
        rsegment = jbig2_find_segment(ctx, segment->referred_to_segments[index]);
        if (rsegment && ((rsegment->flags & 63) == 0)) {
            /* add this referred to symbol dictionary */
            dicts[dindex++] = (Jbig2SymbolDict *)rsegment->result;
        }
    }

    if (dindex != n_dicts) {
        /* should never happen */
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "counted %d symbol dictionaries but build a list with %d.\n",
            n_dicts, dindex);
    }

    return (dicts);
}

/* generate a new symbol dictionary by concatenating a list of
   existing dictionaries */
Jbig2SymbolDict *
jbig2_sd_cat(Jbig2Ctx *ctx, int n_dicts, Jbig2SymbolDict **dicts)
{
  int i,j,k, symbols;
  Jbig2SymbolDict *new = NULL;

  /* count the imported symbols and allocate a new array */
  symbols = 0;
  for(i = 0; i < n_dicts; i++)
    symbols += dicts[i]->n_symbols;

  /* fill a new array with cloned glyph pointers */
  new = jbig2_sd_new(ctx, symbols);
  if (new != NULL) {
    k = 0;
    for (i = 0; i < n_dicts; i++)
      for (j = 0; j < dicts[i]->n_symbols; j++)
        new->glyphs[k++] = jbig2_image_clone(ctx, dicts[i]->glyphs[j]);
  }

  return new;
}


/* Decoding routines */

/* 6.5 */
static Jbig2SymbolDict *
jbig2_decode_symbol_dict(Jbig2Ctx *ctx,
			 Jbig2Segment *segment,
			 const Jbig2SymbolDictParams *params,
			 const byte *data, size_t size,
			 Jbig2ArithCx *GB_stats,
			 Jbig2ArithCx *GR_stats)
{
  Jbig2SymbolDict *SDNEWSYMS;
  Jbig2SymbolDict *SDEXSYMS;
  int32_t HCHEIGHT;
  uint32_t NSYMSDECODED;
  int32_t SYMWIDTH, TOTWIDTH;
  uint32_t HCFIRSTSYM;
  uint32_t *SDNEWSYMWIDTHS = NULL;
  int SBSYMCODELEN = 0;
  Jbig2WordStream *ws = NULL;
  Jbig2HuffmanState *hs = NULL;
  Jbig2HuffmanTable *SDHUFFRDX = NULL;
  Jbig2ArithState *as = NULL;
  Jbig2ArithIntCtx *IADH = NULL;
  Jbig2ArithIntCtx *IADW = NULL;
  Jbig2ArithIntCtx *IAEX = NULL;
  Jbig2ArithIntCtx *IAAI = NULL;
  Jbig2ArithIaidCtx *IAID = NULL;
  Jbig2ArithIntCtx *IARDX = NULL;
  Jbig2ArithIntCtx *IARDY = NULL;
  int code = 0;
  Jbig2SymbolDict **refagg_dicts;
  int n_refagg_dicts = 1;

  Jbig2TextRegionParams *tparams = NULL;

  /* 6.5.5 (3) */
  HCHEIGHT = 0;
  NSYMSDECODED = 0;

  ws = jbig2_word_stream_buf_new(ctx, data, size);

  if (!params->SDHUFF) {
      as = jbig2_arith_new(ctx, ws);
      IADH = jbig2_arith_int_ctx_new(ctx);
      IADW = jbig2_arith_int_ctx_new(ctx);
      IAEX = jbig2_arith_int_ctx_new(ctx);
      IAAI = jbig2_arith_int_ctx_new(ctx);
      if (params->SDREFAGG) {
	  int tmp = params->SDINSYMS->n_symbols + params->SDNUMNEWSYMS;
	  for (SBSYMCODELEN = 0; (1 << SBSYMCODELEN) < tmp; SBSYMCODELEN++);
	  IAID = jbig2_arith_iaid_ctx_new(ctx, SBSYMCODELEN);
	  IARDX = jbig2_arith_int_ctx_new(ctx);
	  IARDY = jbig2_arith_int_ctx_new(ctx);
      }
  } else {
      jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
	"huffman coded symbol dictionary");
      hs = jbig2_huffman_new(ctx, ws);
      SDHUFFRDX = jbig2_build_huffman_table(ctx,
				&jbig2_huffman_params_O);
      if (!params->SDREFAGG) {
	  SDNEWSYMWIDTHS = jbig2_new(ctx, uint32_t, params->SDNUMNEWSYMS);
	  if (SDNEWSYMWIDTHS == NULL) {
	    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
		"could not allocate storage for symbol widths");
	    return NULL;
	  }
      }
  }

  SDNEWSYMS = jbig2_sd_new(ctx, params->SDNUMNEWSYMS);

  /* 6.5.5 (4a) */
  while (NSYMSDECODED < params->SDNUMNEWSYMS) {
      int32_t HCDH, DW;

      /* 6.5.6 */
      if (params->SDHUFF) {
	  HCDH = jbig2_huffman_get(hs, params->SDHUFFDH, &code);
      } else {
	  code = jbig2_arith_int_decode(IADH, as, &HCDH);
      }

      if (code != 0) {
	jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
	  "error or OOB decoding height class delta (%d)\n", code);
      }

      /* 6.5.5 (4b) */
      HCHEIGHT = HCHEIGHT + HCDH;
      SYMWIDTH = 0;
      TOTWIDTH = 0;
      HCFIRSTSYM = NSYMSDECODED;

      if (HCHEIGHT < 0) {
	  /* todo: mem cleanup */
	  code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
			   "Invalid HCHEIGHT value");
          return NULL;
      }
#ifdef JBIG2_DEBUG
      jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
        "HCHEIGHT = %d", HCHEIGHT);
#endif
      jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
        "decoding height class %d with %d syms decoded", HCHEIGHT, NSYMSDECODED);

      for (;;) {
	  /* check for broken symbol table */
 	  if (NSYMSDECODED > params->SDNUMNEWSYMS)
	    {
	      /* todo: mem cleanup? */
	      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
	        "No OOB signalling end of height class %d", HCHEIGHT);
	      break;
	    }
	  /* 6.5.7 */
	  if (params->SDHUFF) {
	      DW = jbig2_huffman_get(hs, params->SDHUFFDW, &code);
	  } else {
	      code = jbig2_arith_int_decode(IADW, as, &DW);
	  }

	  /* 6.5.5 (4c.i) */
	  if (code == 1) {
	    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
	    " OOB signals end of height class %d", HCHEIGHT);
	    break;
	  }
	  SYMWIDTH = SYMWIDTH + DW;
	  TOTWIDTH = TOTWIDTH + SYMWIDTH;
	  if (SYMWIDTH < 0) {
	      /* todo: mem cleanup */
              code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                "Invalid SYMWIDTH value (%d) at symbol %d", SYMWIDTH, NSYMSDECODED+1);
              return NULL;
          }
#ifdef JBIG2_DEBUG
	  jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
            "SYMWIDTH = %d TOTWIDTH = %d", SYMWIDTH, TOTWIDTH);
#endif
	  /* 6.5.5 (4c.ii) */
	  if (!params->SDHUFF || params->SDREFAGG) {
#ifdef JBIG2_DEBUG
		jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
		  "SDHUFF = %d; SDREFAGG = %d", params->SDHUFF, params->SDREFAGG);
#endif
	      /* 6.5.8 */
	      if (!params->SDREFAGG) {
		  Jbig2GenericRegionParams region_params;
		  int sdat_bytes;
		  Jbig2Image *image;

		  /* Table 16 */
		  region_params.MMR = 0;
		  region_params.GBTEMPLATE = params->SDTEMPLATE;
		  region_params.TPGDON = 0;
		  region_params.USESKIP = 0;
		  sdat_bytes = params->SDTEMPLATE == 0 ? 8 : 2;
		  memcpy(region_params.gbat, params->sdat, sdat_bytes);

		  image = jbig2_image_new(ctx, SYMWIDTH, HCHEIGHT);

		  code = jbig2_decode_generic_region(ctx, segment, &region_params,
						     as, image, GB_stats);
		  /* todo: handle errors */

                  SDNEWSYMS->glyphs[NSYMSDECODED] = image;

	      } else {
                  /* 6.5.8.2 refinement/aggregate symbol */
                  uint32_t REFAGGNINST;

		  if (params->SDHUFF) {
		      REFAGGNINST = jbig2_huffman_get(hs, params->SDHUFFAGGINST, &code);
		  } else {
		      code = jbig2_arith_int_decode(IAAI, as, (int32_t*)&REFAGGNINST);
		  }
		  if (code || (int32_t)REFAGGNINST <= 0) {
		      code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
			"invalid number of symbols or OOB in aggregate glyph");
		      return NULL;
		  }

		  jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
		    "aggregate symbol coding (%d instances)", REFAGGNINST);

		  if (REFAGGNINST > 1) {
		      Jbig2Image *image;
		      int i;

		      if (tparams == NULL)
		      {
			  /* First time through, we need to initialise the */
			  /* various tables for Huffman or adaptive encoding */
			  /* as well as the text region parameters structure */
			  refagg_dicts = jbig2_new(ctx, Jbig2SymbolDict*, n_refagg_dicts);
		          if (refagg_dicts == NULL) {
			      code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
			       "Out of memory allocating dictionary array");
		              return NULL;
		          }
		          refagg_dicts[0] = jbig2_sd_new(ctx, params->SDNUMINSYMS + params->SDNUMNEWSYMS);
		          if (refagg_dicts[0] == NULL) {
			      code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
			       "Out of memory allocating symbol dictionary");
		              jbig2_free(ctx->allocator, refagg_dicts);
		              return NULL;
		          }
		          refagg_dicts[0]->n_symbols = params->SDNUMINSYMS + params->SDNUMNEWSYMS;
		          for (i=0;i < params->SDNUMINSYMS;i++)
		          {
			      refagg_dicts[0]->glyphs[i] = jbig2_image_clone(ctx, params->SDINSYMS->glyphs[i]);
		          }

			  tparams = jbig2_new(ctx, Jbig2TextRegionParams, 1);
			  if (tparams == NULL) {
			      code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
			      "Out of memory creating text region params");
			      jbig2_sd_release(ctx, refagg_dicts[0]);
			      jbig2_free(ctx->allocator, refagg_dicts);
			      return NULL;
			  }
    		          if (!params->SDHUFF) {
			      /* Values from Table 17, section 6.5.8.2 (2) */
			      tparams->IADT = jbig2_arith_int_ctx_new(ctx);
			      tparams->IAFS = jbig2_arith_int_ctx_new(ctx);
			      tparams->IADS = jbig2_arith_int_ctx_new(ctx);
			      tparams->IAIT = jbig2_arith_int_ctx_new(ctx);
			      /* Table 31 */
			      for (SBSYMCODELEN = 0; (1 << SBSYMCODELEN) <
				  (int)(params->SDNUMINSYMS + params->SDNUMNEWSYMS); SBSYMCODELEN++);
			      tparams->IAID = jbig2_arith_iaid_ctx_new(ctx, SBSYMCODELEN);
			      tparams->IARI = jbig2_arith_int_ctx_new(ctx);
			      tparams->IARDW = jbig2_arith_int_ctx_new(ctx);
			      tparams->IARDH = jbig2_arith_int_ctx_new(ctx);
			      tparams->IARDX = jbig2_arith_int_ctx_new(ctx);
			      tparams->IARDY = jbig2_arith_int_ctx_new(ctx);
			  } else {
			      tparams->SBHUFFFS = jbig2_build_huffman_table(ctx,
				&jbig2_huffman_params_F);   /* Table B.6 */
			      tparams->SBHUFFDS = jbig2_build_huffman_table(ctx,
				&jbig2_huffman_params_H);  /* Table B.8 */
			      tparams->SBHUFFDT = jbig2_build_huffman_table(ctx,
				&jbig2_huffman_params_K);  /* Table B.11 */
			      tparams->SBHUFFRDW = jbig2_build_huffman_table(ctx,
				&jbig2_huffman_params_O); /* Table B.15 */
			      tparams->SBHUFFRDH = jbig2_build_huffman_table(ctx,
				&jbig2_huffman_params_O); /* Table B.15 */
			      tparams->SBHUFFRDX = jbig2_build_huffman_table(ctx,
				&jbig2_huffman_params_O); /* Table B.15 */
			      tparams->SBHUFFRDY = jbig2_build_huffman_table(ctx,
				&jbig2_huffman_params_O); /* Table B.15 */
			  }
			  tparams->SBHUFF = params->SDHUFF;
			  tparams->SBREFINE = 1;
			  tparams->SBSTRIPS = 1;
			  tparams->SBDEFPIXEL = 0;
			  tparams->SBCOMBOP = JBIG2_COMPOSE_OR;
			  tparams->TRANSPOSED = 0;
			  tparams->REFCORNER = JBIG2_CORNER_TOPLEFT;
			  tparams->SBDSOFFSET = 0;
			  tparams->SBRTEMPLATE = params->SDRTEMPLATE;
		      }
		      tparams->SBNUMINSTANCES = REFAGGNINST;

      		      image = jbig2_image_new(ctx, SYMWIDTH, HCHEIGHT);
		      if (image == NULL) {
			  code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
			   "Out of memory creating symbol image");
		          jbig2_free(ctx->allocator, tparams);
			  jbig2_sd_release(ctx, refagg_dicts[0]);
		          jbig2_free(ctx->allocator, refagg_dicts);
		          return NULL;
		      }

		      /* multiple symbols are handled as a text region */
		      jbig2_decode_text_region(ctx, segment, tparams, (const Jbig2SymbolDict * const *)refagg_dicts,
			  n_refagg_dicts, image, data, size, GR_stats, as, (Jbig2WordStream *)NULL);

		      SDNEWSYMS->glyphs[NSYMSDECODED] = image;
		      refagg_dicts[0]->glyphs[params->SDNUMINSYMS + NSYMSDECODED] = jbig2_image_clone(ctx, SDNEWSYMS->glyphs[NSYMSDECODED]);
		  } else {
		      /* 6.5.8.2.2 */
		      /* bool SBHUFF = params->SDHUFF; */
		      Jbig2RefinementRegionParams rparams;
		      Jbig2Image *image;
		      uint32_t ID;
		      int32_t RDX, RDY;
		      int ninsyms = params->SDINSYMS->n_symbols;

		      if (params->SDHUFF) {
			  ID = jbig2_huffman_get_bits(hs, SBSYMCODELEN);
			  RDX = jbig2_huffman_get(hs, SDHUFFRDX, &code);
			  RDY = jbig2_huffman_get(hs, SDHUFFRDX, &code);
		      } else {
			  code = jbig2_arith_iaid_decode(IAID, as, (int32_t*)&ID);
		          code = jbig2_arith_int_decode(IARDX, as, &RDX);
		          code = jbig2_arith_int_decode(IARDY, as, &RDY);
		      }

		      if (ID >= ninsyms+NSYMSDECODED) {
			code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
			  "refinement references unknown symbol %d", ID);
			return NULL;
		      }

		      jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
			"symbol is a refinement of id %d with the refinement applied at (%d,%d)",
			ID, RDX, RDY);

		      image = jbig2_image_new(ctx, SYMWIDTH, HCHEIGHT);

		      /* Table 18 */
		      rparams.GRTEMPLATE = params->SDRTEMPLATE;
		      rparams.reference = (ID < ninsyms) ?
					params->SDINSYMS->glyphs[ID] :
					SDNEWSYMS->glyphs[ID-ninsyms];
		      rparams.DX = RDX;
		      rparams.DY = RDY;
		      rparams.TPGRON = 0;
		      memcpy(rparams.grat, params->sdrat, 4);
		      jbig2_decode_refinement_region(ctx, segment,
		          &rparams, as, image, GR_stats);

		      SDNEWSYMS->glyphs[NSYMSDECODED] = image;

		  }
               }

#ifdef OUTPUT_PBM
		  {
		    char name[64];
		    FILE *out;
		    snprintf(name, 64, "sd.%04d.%04d.pbm",
		             segment->number, NSYMSDECODED);
		    out = fopen(name, "wb");
                    jbig2_image_write_pbm(SDNEWSYMS->glyphs[NSYMSDECODED], out);
		    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
			"writing out glyph as '%s' ...", name);
		    fclose(out);
		  }
#endif

	  }

	  /* 6.5.5 (4c.iii) */
	  if (params->SDHUFF && !params->SDREFAGG) {
	    SDNEWSYMWIDTHS[NSYMSDECODED] = SYMWIDTH;
	  }

	  jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
            "decoded symbol %d of %d (%dx%d)",
		NSYMSDECODED, params->SDNUMNEWSYMS,
		SYMWIDTH, HCHEIGHT);

	  /* 6.5.5 (4c.iv) */
	  NSYMSDECODED = NSYMSDECODED + 1;

      } /* end height class decode loop */

      /* 6.5.5 (4d) */
      if (params->SDHUFF && !params->SDREFAGG) {
	/* 6.5.9 */
	Jbig2Image *image;
	int BMSIZE = jbig2_huffman_get(hs, params->SDHUFFBMSIZE, &code);
	int j, x;

	if (code || (BMSIZE < 0)) {
	  jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
	    "error decoding size of collective bitmap!");
	  /* todo: memory cleanup */
	  return NULL;
	}

	/* skip any bits before the next byte boundary */
	jbig2_huffman_skip(hs);

	image = jbig2_image_new(ctx, TOTWIDTH, HCHEIGHT);
	if (image == NULL) {
	  jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
	    "could not allocate collective bitmap image!");
	  /* todo: memory cleanup */
	  return NULL;
	}

	if (BMSIZE == 0) {
	  /* if BMSIZE == 0 bitmap is uncompressed */
	  const byte *src = data + jbig2_huffman_offset(hs);
	  const int stride = (image->width >> 3) +
		((image->width & 7) ? 1 : 0);
	  byte *dst = image->data;

	  BMSIZE = image->height * stride;
	  jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
	    "reading %dx%d uncompressed bitmap"
	    " for %d symbols (%d bytes)",
	    image->width, image->height, NSYMSDECODED - HCFIRSTSYM, BMSIZE);

	  for (j = 0; j < image->height; j++) {
	    memcpy(dst, src, stride);
	    dst += image->stride;
	    src += stride;
	  }
	} else {
	  Jbig2GenericRegionParams rparams;

	  jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
	    "reading %dx%d collective bitmap for %d symbols (%d bytes)",
	    image->width, image->height, NSYMSDECODED - HCFIRSTSYM, BMSIZE);

	  rparams.MMR = 1;
	  code = jbig2_decode_generic_mmr(ctx, segment, &rparams,
	    data + jbig2_huffman_offset(hs), BMSIZE, image);
	  if (code) {
	    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
	      "error decoding MMR bitmap image!");
	    /* todo: memory cleanup */
	    return NULL;
	  }
	}

	/* advance past the data we've just read */
	jbig2_huffman_advance(hs, BMSIZE);

	/* copy the collective bitmap into the symbol dictionary */
	x = 0;
	for (j = HCFIRSTSYM; j < NSYMSDECODED; j++) {
	  Jbig2Image *glyph;
	  glyph = jbig2_image_new(ctx, SDNEWSYMWIDTHS[j], HCHEIGHT);
	  jbig2_image_compose(ctx, glyph, image,
		-x, 0, JBIG2_COMPOSE_REPLACE);
	  x += SDNEWSYMWIDTHS[j];
	  SDNEWSYMS->glyphs[j] = glyph;
	}
	jbig2_image_release(ctx, image);
      }

  } /* end of symbol decode loop */

  if (tparams != NULL)
  {
      if (!params->SDHUFF)
      {
          jbig2_arith_int_ctx_free(ctx, tparams->IADT);
          jbig2_arith_int_ctx_free(ctx, tparams->IAFS);
          jbig2_arith_int_ctx_free(ctx, tparams->IADS);
          jbig2_arith_int_ctx_free(ctx, tparams->IAIT);
          jbig2_arith_iaid_ctx_free(ctx, tparams->IAID);
          jbig2_arith_int_ctx_free(ctx, tparams->IARI);
          jbig2_arith_int_ctx_free(ctx, tparams->IARDW);
          jbig2_arith_int_ctx_free(ctx, tparams->IARDH);
          jbig2_arith_int_ctx_free(ctx, tparams->IARDX);
          jbig2_arith_int_ctx_free(ctx, tparams->IARDY);
      }
      else
      {
          jbig2_release_huffman_table(ctx, tparams->SBHUFFFS);
          jbig2_release_huffman_table(ctx, tparams->SBHUFFDS);
          jbig2_release_huffman_table(ctx, tparams->SBHUFFDT);
          jbig2_release_huffman_table(ctx, tparams->SBHUFFRDX);
          jbig2_release_huffman_table(ctx, tparams->SBHUFFRDY);
          jbig2_release_huffman_table(ctx, tparams->SBHUFFRDW);
          jbig2_release_huffman_table(ctx, tparams->SBHUFFRDH);
      }
      jbig2_free(ctx->allocator, tparams);
      tparams = NULL;
      jbig2_sd_release(ctx, refagg_dicts[0]);
      jbig2_free(ctx->allocator, refagg_dicts);
  }

  jbig2_free(ctx->allocator, GB_stats);

  /* 6.5.10 */
  SDEXSYMS = jbig2_sd_new(ctx, params->SDNUMEXSYMS);
  {
    int i = 0;
    int j = 0;
    int k, m, exflag = 0;
    int32_t exrunlength;

    if (params->SDINSYMS != NULL)
      m = params->SDINSYMS->n_symbols;
    else
      m = 0;
    while (j < params->SDNUMEXSYMS) {
      if (params->SDHUFF)
      	/* FIXME: implement reading from huff table B.1 */
        exrunlength = exflag ? params->SDNUMEXSYMS : 0;
      else
        code = jbig2_arith_int_decode(IAEX, as, &exrunlength);
      if (exflag && exrunlength > params->SDNUMEXSYMS - j) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
          "runlength too large in export symbol table (%d > %d - %d)\n",
          exrunlength, params->SDNUMEXSYMS, j);
        jbig2_sd_release(ctx, SDEXSYMS);
        /* skip to the cleanup code and return SDEXSYMS = NULL */
        SDEXSYMS = NULL;
        break;
      }
      for(k = 0; k < exrunlength; k++) {
        if (exflag) {
          SDEXSYMS->glyphs[j++] = (i < m) ?
            jbig2_image_clone(ctx, params->SDINSYMS->glyphs[i]) :
            jbig2_image_clone(ctx, SDNEWSYMS->glyphs[i-m]);
        }
          i++;
        }
        exflag = !exflag;
    }
  }

  jbig2_sd_release(ctx, SDNEWSYMS);

  if (!params->SDHUFF) {
      jbig2_arith_int_ctx_free(ctx, IADH);
      jbig2_arith_int_ctx_free(ctx, IADW);
      jbig2_arith_int_ctx_free(ctx, IAEX);
      jbig2_arith_int_ctx_free(ctx, IAAI);
      if (params->SDREFAGG) {
        jbig2_arith_iaid_ctx_free(ctx, IAID);
        jbig2_arith_int_ctx_free(ctx, IARDX);
        jbig2_arith_int_ctx_free(ctx, IARDY);
      }
      jbig2_free(ctx->allocator, as);
  } else {
      if (params->SDREFAGG) {
	jbig2_free(ctx->allocator, SDNEWSYMWIDTHS);
      }
      jbig2_release_huffman_table(ctx, SDHUFFRDX);
      jbig2_free(ctx->allocator, hs);
  }

  jbig2_word_stream_buf_free(ctx, ws);

  return SDEXSYMS;
}

/* 7.4.2 */
int
jbig2_symbol_dictionary(Jbig2Ctx *ctx, Jbig2Segment *segment,
			const byte *segment_data)
{
  Jbig2SymbolDictParams params;
  uint16_t flags;
  int sdat_bytes;
  int offset;
  Jbig2ArithCx *GB_stats = NULL;
  Jbig2ArithCx *GR_stats = NULL;
  int table_index = 0;
  const Jbig2HuffmanParams *huffman_params;

  if (segment->data_length < 10)
    goto too_short;

  /* 7.4.2.1.1 */
  flags = jbig2_get_int16(segment_data);
  params.SDHUFF = flags & 1;
  params.SDREFAGG = (flags >> 1) & 1;
  params.SDTEMPLATE = (flags >> 10) & 3;
  params.SDRTEMPLATE = (flags >> 12) & 1;

  params.SDHUFFDH = NULL;
  params.SDHUFFDW = NULL;
  params.SDHUFFBMSIZE = NULL;
  params.SDHUFFAGGINST = NULL;

  if (params.SDHUFF) {
    switch ((flags & 0x000c) >> 2) {
      case 0: /* Table B.4 */
	params.SDHUFFDH = jbig2_build_huffman_table(ctx,
		                       &jbig2_huffman_params_D);
	break;
      case 1: /* Table B.5 */
	params.SDHUFFDH = jbig2_build_huffman_table(ctx,
		                       &jbig2_huffman_params_E);
	break;
      case 3: /* Custom table from referred segment */
        huffman_params = jbig2_find_table(ctx, segment, table_index);
        if (huffman_params == NULL) {
	return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                "Custom DH huffman table not found (%d)", table_index);
        }
        params.SDHUFFDH = jbig2_build_huffman_table(ctx, huffman_params);
        ++table_index;
        break;
        /* FIXME: this function leaks memory when error happens.
           i.e. not calling jbig2_release_huffman_table() */
      case 2:
      default:
	return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
	    "symbol dictionary specified invalid huffman table");
	break;
    }
    switch ((flags & 0x0030) >> 4) {
      case 0: /* Table B.2 */
	params.SDHUFFDW = jbig2_build_huffman_table(ctx,
		                       &jbig2_huffman_params_B);
	break;
      case 1: /* Table B.3 */
	params.SDHUFFDW = jbig2_build_huffman_table(ctx,
		                       &jbig2_huffman_params_C);
	break;
      case 3: /* Custom table from referred segment */
        huffman_params = jbig2_find_table(ctx, segment, table_index);
        if (huffman_params == NULL) {
	return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                "Custom DW huffman table not found (%d)", table_index);
        }
        params.SDHUFFDW = jbig2_build_huffman_table(ctx, huffman_params);
        ++table_index;
        break;
      case 2:
      default:
	return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
	    "symbol dictionary specified invalid huffman table");
	break;
    }
    if (flags & 0x0040) {
        /* Custom table from referred segment */
        huffman_params = jbig2_find_table(ctx, segment, table_index);
        if (huffman_params == NULL) {
	return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                "Custom BMSIZE huffman table not found (%d)", table_index);
        }
        params.SDHUFFBMSIZE = jbig2_build_huffman_table(ctx, huffman_params);
        ++table_index;
    } else {
	/* Table B.1 */
	params.SDHUFFBMSIZE = jbig2_build_huffman_table(ctx,
					&jbig2_huffman_params_A);
    }
    if (flags & 0x0080) {
        /* Custom table from referred segment */
        huffman_params = jbig2_find_table(ctx, segment, table_index);
        if (huffman_params == NULL) {
	return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                "Custom REFAGG huffman table not found (%d)", table_index);
        }
        params.SDHUFFAGGINST = jbig2_build_huffman_table(ctx, huffman_params);
        ++table_index;
    } else {
	/* Table B.1 */
	params.SDHUFFAGGINST = jbig2_build_huffman_table(ctx,
					&jbig2_huffman_params_A);
    }
  }

  /* FIXME: there are quite a few of these conditions to check */
  /* maybe #ifdef CONFORMANCE and a separate routine */
  if (!params.SDHUFF) {
    if (flags & 0x000c) {
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
		  "SDHUFF is zero, but contrary to spec SDHUFFDH is not.");
    }
    if (flags & 0x0030) {
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
		  "SDHUFF is zero, but contrary to spec SDHUFFDW is not.");
    }
  }

  if (flags & 0x0080) {
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "bitmap coding context is used (NYI) symbol data likely to be garbage!");
  }

  /* 7.4.2.1.2 */
  sdat_bytes = params.SDHUFF ? 0 : params.SDTEMPLATE == 0 ? 8 : 2;
  memcpy(params.sdat, segment_data + 2, sdat_bytes);
  offset = 2 + sdat_bytes;

  /* 7.4.2.1.3 */
  if (params.SDREFAGG && !params.SDRTEMPLATE) {
      if (offset + 4 > segment->data_length)
	goto too_short;
      memcpy(params.sdrat, segment_data + offset, 4);
      offset += 4;
  } else {
      /* sdrat is meaningless if SDRTEMPLATE is 1, but set a value
         to avoid confusion if anybody looks */
      memset(params.sdrat, 0, 4);
  }

  if (offset + 8 > segment->data_length)
    goto too_short;

  /* 7.4.2.1.4 */
  params.SDNUMEXSYMS = jbig2_get_int32(segment_data + offset);
  /* 7.4.2.1.5 */
  params.SDNUMNEWSYMS = jbig2_get_int32(segment_data + offset + 4);
  offset += 8;

  jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
	      "symbol dictionary, flags=%04x, %d exported syms, %d new syms",
	      flags, params.SDNUMEXSYMS, params.SDNUMNEWSYMS);

  /* 7.4.2.2 (2) */
  {
    int n_dicts = jbig2_sd_count_referred(ctx, segment);
    Jbig2SymbolDict **dicts = NULL;

    params.SDINSYMS = NULL;
    if (n_dicts > 0) {
      dicts = jbig2_sd_list_referred(ctx, segment);
      params.SDINSYMS = jbig2_sd_cat(ctx, n_dicts, dicts);
    }
    if (params.SDINSYMS != NULL) {
      params.SDNUMINSYMS = params.SDINSYMS->n_symbols;
    } else {
     params.SDNUMINSYMS = 0;
    }
  }

  /* 7.4.2.2 (4) */
  if (!params.SDHUFF) {
      int stats_size = params.SDTEMPLATE == 0 ? 65536 :
	params.SDTEMPLATE == 1 ? 8192 : 1024;
      GB_stats = jbig2_new(ctx, Jbig2ArithCx, stats_size);
      memset(GB_stats, 0, stats_size);
      if (params.SDREFAGG) {
	stats_size = params.SDRTEMPLATE ? 1 << 10 : 1 << 13;
	GR_stats = jbig2_new(ctx, Jbig2ArithCx, stats_size);
	memset(GR_stats, 0, stats_size);
      }
  }

  if (flags & 0x0100) {
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "segment marks bitmap coding context as retained (NYI)");
  }

  segment->result = (void *)jbig2_decode_symbol_dict(ctx, segment,
				  &params,
				  segment_data + offset,
				  segment->data_length - offset,
				  GB_stats, GR_stats);
#ifdef DUMP_SYMDICT
  if (segment->result) jbig2_dump_symbol_dict(ctx, segment);
#endif

  if (params.SDHUFF) {
      jbig2_release_huffman_table(ctx, params.SDHUFFDH);
      jbig2_release_huffman_table(ctx, params.SDHUFFDW);
      jbig2_release_huffman_table(ctx, params.SDHUFFBMSIZE);
      jbig2_release_huffman_table(ctx, params.SDHUFFAGGINST);
  }

  /* todo: retain or free GB_stats, GR_stats */

  return (segment->result != NULL) ? 0 : -1;

 too_short:
  return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
		     "Segment too short");
}
