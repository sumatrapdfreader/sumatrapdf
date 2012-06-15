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

#include <stddef.h> /* size_t */

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_huffman.h"
#include "jbig2_symbol_dict.h"
#include "jbig2_metadata.h"
#include "jbig2_arith.h"
#include "jbig2_halftone.h"

Jbig2Segment *
jbig2_parse_segment_header (Jbig2Ctx *ctx, uint8_t *buf, size_t buf_size,
			    size_t *p_header_size)
{
  Jbig2Segment *result;
  uint8_t rtscarf;
  uint32_t rtscarf_long;
  uint32_t *referred_to_segments;
  int referred_to_segment_count;
  int referred_to_segment_size;
  int pa_size;
  int offset;

  /* minimum possible size of a jbig2 segment header */
  if (buf_size < 11)
    return NULL;

  result = jbig2_new(ctx, Jbig2Segment, 1);
  if (result == NULL)
  {
      jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
          "failed to allocate segment in jbig2_parse_segment_header");
      return result;
  }

  /* 7.2.2 */
  result->number = jbig2_get_uint32(buf);

  /* 7.2.3 */
  result->flags = buf[4];

  /* 7.2.4 referred-to segments */
  rtscarf = buf[5];
  if ((rtscarf & 0xe0) == 0xe0)
    {
      rtscarf_long = jbig2_get_uint32(buf + 5);
      referred_to_segment_count = rtscarf_long & 0x1fffffff;
      offset = 5 + 4 + (referred_to_segment_count + 1) / 8;
    }
  else
    {
      referred_to_segment_count = (rtscarf >> 5);
      offset = 5 + 1;
    }
  result->referred_to_segment_count = referred_to_segment_count;

  /* we now have enough information to compute the full header length */
  referred_to_segment_size = result->number <= 256 ? 1:
        result->number <= 65536 ? 2 : 4;  /* 7.2.5 */
  pa_size = result->flags & 0x40 ? 4 : 1; /* 7.2.6 */
  if (offset + referred_to_segment_count*referred_to_segment_size + pa_size + 4 > buf_size)
    {
      jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, result->number,
        "jbig2_parse_segment_header() called with insufficient data", -1);
      jbig2_free (ctx->allocator, result);
      return NULL;
    }

  /* 7.2.5 */
  if (referred_to_segment_count)
    {
      int i;

      referred_to_segments = jbig2_new(ctx, uint32_t,
        referred_to_segment_count * referred_to_segment_size);
      if (referred_to_segments == NULL)
      {
          jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
              "could not allocate referred_to_segments "
              "in jbig2_parse_segment_header");
          return NULL;
      }

      for (i = 0; i < referred_to_segment_count; i++) {
        referred_to_segments[i] =
          (referred_to_segment_size == 1) ? buf[offset] :
          (referred_to_segment_size == 2) ? jbig2_get_uint16(buf+offset) :
            jbig2_get_uint32(buf + offset);
        offset += referred_to_segment_size;
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, result->number,
            "segment %d refers to segment %d",
            result->number, referred_to_segments[i]);
      }
      result->referred_to_segments = referred_to_segments;
    }
  else /* no referred-to segments */
    {
      result->referred_to_segments = NULL;
    }

  /* 7.2.6 */
  if (result->flags & 0x40) {
	result->page_association = jbig2_get_uint32(buf + offset);
	offset += 4;
  } else {
	result->page_association = buf[offset++];
  }
  jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, result->number,
  	"segment %d is associated with page %d",
  	result->number, result->page_association);

  /* 7.2.7 */
  result->data_length = jbig2_get_uint32(buf + offset);
  *p_header_size = offset + 4;

  /* no body parsing results yet */
  result->result = NULL;

  return result;
}

void
jbig2_free_segment (Jbig2Ctx *ctx, Jbig2Segment *segment)
{
  if (segment->referred_to_segments != NULL) {
    jbig2_free(ctx->allocator, segment->referred_to_segments);
  }
  /* todo: we need either some separate fields or
     a more complex result object rather than this
     brittle special casing */
    switch (segment->flags & 63) {
	case 0:  /* symbol dictionary */
	  if (segment->result != NULL)
	    jbig2_sd_release(ctx, (Jbig2SymbolDict*)segment->result);
	  break;
	case 4:  /* intermediate text region */
	case 40: /* intermediate refinement region */
	  if (segment->result != NULL)
	    jbig2_image_release(ctx, (Jbig2Image*)segment->result);
	  break;
	case 16: /* pattern dictionary */
      if (segment->result != NULL)
        jbig2_hd_release(ctx, (Jbig2PatternDict*)segment->result);
      break;
	case 53: /* user-supplied huffman table */
	  if (segment->result != NULL)
		jbig2_table_free(ctx, (Jbig2HuffmanParams*)segment->result);
	  break;
	case 62:
	  if (segment->result != NULL)
	    jbig2_metadata_free(ctx, (Jbig2Metadata*)segment->result);
	  break;
	default:
	  /* anything else is probably an undefined pointer */
	  break;
    }
  jbig2_free (ctx->allocator, segment);
}

/* find a segment by number */
Jbig2Segment *
jbig2_find_segment(Jbig2Ctx *ctx, uint32_t number)
{
    int index, index_max = ctx->segment_index - 1;
    const Jbig2Ctx *global_ctx = ctx->global_ctx;

    /* FIXME: binary search would be better */
    for (index = index_max; index >= 0; index--)
        if (ctx->segments[index]->number == number)
            return (ctx->segments[index]);

    if (global_ctx)
	for (index = global_ctx->segment_index - 1; index >= 0; index--)
	    if (global_ctx->segments[index]->number == number)
		return (global_ctx->segments[index]);

    /* didn't find a match */
    return NULL;
}

/* parse the generic portion of a region segment data header */
void
jbig2_get_region_segment_info(Jbig2RegionSegmentInfo *info,
			      const uint8_t *segment_data)
{
  /* 7.4.1 */
  info->width = jbig2_get_int32(segment_data);
  info->height = jbig2_get_int32(segment_data + 4);
  info->x = jbig2_get_int32(segment_data + 8);
  info->y = jbig2_get_int32(segment_data + 12);
  info->flags = segment_data[16];
  info->op = (Jbig2ComposeOp)(info->flags & 0x7);
}

/* dispatch code for extension segment parsing */
int jbig2_parse_extension_segment(Jbig2Ctx *ctx, Jbig2Segment *segment,
                            const uint8_t *segment_data)
{
    uint32_t type = jbig2_get_uint32(segment_data);
    bool reserved = type & 0x20000000;
    /* bool dependent = type & 0x40000000; (NYI) */
    bool necessary = type & 0x80000000;

    if (necessary && !reserved) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
            "extension segment is marked 'necessary' but "
            "not 'reservered' contrary to spec");
    }

    switch (type) {
        case 0x20000000: return jbig2_comment_ascii(ctx, segment, segment_data);
        case 0x20000002: return jbig2_comment_unicode(ctx, segment, segment_data);
        default:
            if (necessary) {
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "unhandled necessary extension segment type 0x%08x", type);
            } else {
                return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
                    "unhandled extension segment");
            }
    }

    return 0;
}

/* general segment parsing dispatch */
int jbig2_parse_segment (Jbig2Ctx *ctx, Jbig2Segment *segment,
			 const uint8_t *segment_data)
{
  jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
	      "Segment %d, flags=%x, type=%d, data_length=%d",
	      segment->number, segment->flags, segment->flags & 63,
	      segment->data_length);
  switch (segment->flags & 63)
    {
    case 0:
      return jbig2_symbol_dictionary(ctx, segment, segment_data);
    case 4: /* intermediate text region */
    case 6: /* immediate text region */
    case 7: /* immediate lossless text region */
      return jbig2_text_region(ctx, segment, segment_data);
    case 16:
      return jbig2_pattern_dictionary(ctx, segment, segment_data);
    case 20: /* intermediate halftone region */
    case 22: /* immediate halftone region */
    case 23: /* immediate lossless halftone region */
      return jbig2_halftone_region(ctx, segment, segment_data);
    case 36:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled segment type 'intermediate generic region'");
    case 38: /* immediate generic region */
    case 39: /* immediate lossless generic region */
      return jbig2_immediate_generic_region(ctx, segment, segment_data);
    case 40: /* intermediate generic refinement region */
    case 42: /* immediate generic refinement region */
    case 43: /* immediate lossless generic refinement region */
      return jbig2_refinement_region(ctx, segment, segment_data);
    case 48:
      return jbig2_page_info(ctx, segment, segment_data);
    case 49:
      return jbig2_end_of_page(ctx, segment, segment_data);
    case 50:
      return jbig2_end_of_stripe(ctx, segment, segment_data);
    case 51:
      ctx->state = JBIG2_FILE_EOF;
      return jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
        "end of file");
    case 52:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled segment type 'profile'");
    case 53: /* user-supplied huffman table */
      return jbig2_table(ctx, segment, segment_data);
    case 62:
      return jbig2_parse_extension_segment(ctx, segment, segment_data);
    default:
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
          "unknown segment type %d", segment->flags & 63);
    }
  return 0;
}
