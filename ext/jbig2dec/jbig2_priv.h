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


/* library internals */

typedef uint8_t byte;

#define bool int

#ifdef __cplusplus
#define template template_C
#define new new_C
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef enum {
  JBIG2_FILE_HEADER,
  JBIG2_FILE_SEQUENTIAL_HEADER,
  JBIG2_FILE_SEQUENTIAL_BODY,
  JBIG2_FILE_RANDOM_HEADERS,
  JBIG2_FILE_RANDOM_BODIES,
  JBIG2_FILE_EOF
} Jbig2FileState;

struct _Jbig2Ctx {
  Jbig2Allocator *allocator;
  Jbig2Options options;
  const Jbig2Ctx *global_ctx;
  Jbig2ErrorCallback error_callback;
  void *error_callback_data;

  byte *buf;
  size_t buf_size;
  unsigned int buf_rd_ix;
  unsigned int buf_wr_ix;

  Jbig2FileState state;

  uint8_t file_header_flags;
  uint32_t n_pages;

  int n_segments_max;
  Jbig2Segment **segments;
  int n_segments;	/* index of last segment header parsed */
  int segment_index;    /* index of last segment body parsed */

  /* list of decoded pages, including the one in progress,
     currently stored as a contiguous, 0-indexed array. */
  int current_page;
  int max_page_index;
  Jbig2Page *pages;
};

uint32_t
jbig2_get_uint32(const byte *bptr);

int32_t
jbig2_get_int32 (const byte *buf);

uint16_t
jbig2_get_uint16(const byte *bptr);

int16_t
jbig2_get_int16 (const byte *buf);

/* dynamic memory management */
void *
jbig2_alloc (Jbig2Allocator *allocator, size_t size, size_t num);

void
jbig2_free (Jbig2Allocator *allocator, void *p);

void *
jbig2_realloc (Jbig2Allocator *allocator, void *p, size_t size, size_t num);

#define jbig2_new(ctx, t, size) ((t *)jbig2_alloc(ctx->allocator, size, sizeof(t)))

#define jbig2_renew(ctx, p, t, size) ((t *)jbig2_realloc(ctx->allocator, (p), size, sizeof(t)))

int
jbig2_error (Jbig2Ctx *ctx, Jbig2Severity severity, int32_t seg_idx,
	     const char *fmt, ...);

/* the page structure handles decoded page
   results. it's allocated by a 'page info'
   segement and marked complete by an 'end of page'
   segment.
*/
typedef enum {
    JBIG2_PAGE_FREE,
    JBIG2_PAGE_NEW,
    JBIG2_PAGE_COMPLETE,
    JBIG2_PAGE_RETURNED,
    JBIG2_PAGE_RELEASED
} Jbig2PageState;

struct _Jbig2Page {
    Jbig2PageState state;
    uint32_t number;
    uint32_t height, width;	/* in pixels */
    uint32_t x_resolution,
             y_resolution;	/* in pixels per meter */
    uint16_t stripe_size;
    bool striped;
    int end_row;
    uint8_t flags;
    Jbig2Image *image;
};

int jbig2_page_info (Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data);
int jbig2_end_of_stripe(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data);
int jbig2_end_of_page(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data);
int jbig2_extension_segment(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data);

typedef enum {
    JBIG2_COMPOSE_OR = 0,
    JBIG2_COMPOSE_AND = 1,
    JBIG2_COMPOSE_XOR = 2,
    JBIG2_COMPOSE_XNOR = 3,
    JBIG2_COMPOSE_REPLACE = 4
} Jbig2ComposeOp;

int jbig2_image_compose(Jbig2Ctx *ctx, Jbig2Image *dst, Jbig2Image *src, int x, int y, Jbig2ComposeOp op);
int jbig2_page_add_result(Jbig2Ctx *ctx, Jbig2Page *page, Jbig2Image *src, int x, int y, Jbig2ComposeOp op);

/* region segment info */

typedef struct {
  int32_t width;
  int32_t height;
  int32_t x;
  int32_t y;
  Jbig2ComposeOp op;
  uint8_t flags;
} Jbig2RegionSegmentInfo;

void jbig2_get_region_segment_info(Jbig2RegionSegmentInfo *info, const uint8_t *segment_data);
int jbig2_text_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data);

/* 7.4 */
int jbig2_immediate_generic_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
			       const uint8_t *segment_data);
int jbig2_refinement_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
                               const byte *segment_data);

int jbig2_pattern_dictionary(Jbig2Ctx *ctx, Jbig2Segment *segment,
                               const byte *segment_data);
int jbig2_halftone_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
                               const byte *segment_data);


/* The word stream design is a compromise between simplicity and
   trying to amortize the number of method calls. Each ::get_next_word
   invocation pulls 4 bytes from the stream, packed big-endian into a
   32 bit word. The offset argument is provided as a convenience. It
   begins at 0 and increments by 4 for each successive invocation. */
typedef struct _Jbig2WordStream Jbig2WordStream;

struct _Jbig2WordStream {
  uint32_t (*get_next_word) (Jbig2WordStream *self, int offset);
};

Jbig2WordStream *
jbig2_word_stream_buf_new(Jbig2Ctx *ctx, const byte *data, size_t size);

void
jbig2_word_stream_buf_free(Jbig2Ctx *ctx, Jbig2WordStream *ws);
