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



#ifndef _JBIG2_METADATA_H
#define _JBIG2_METADATA_H

/* metadata from extension segments */

/* these bits should be moved to jbig2.h for public access */
typedef enum {
    JBIG2_ENCODING_ASCII,
    JBIG2_ENCODING_UCS16
} Jbig2Encoding;

typedef struct _Jbig2Metadata Jbig2Metadata;

Jbig2Metadata *jbig2_metadata_new(Jbig2Ctx *ctx, Jbig2Encoding encoding);
void jbig2_metadata_free(Jbig2Ctx *ctx, Jbig2Metadata *md);
int jbig2_metadata_add(Jbig2Ctx *ctx, Jbig2Metadata *md,
                        const char *key, const int key_length,
                        const char *value, const int value_length);

struct _Jbig2Metadata {
    Jbig2Encoding encoding;
    char **keys, **values;
    int entries, max_entries;
};

/* these bits can go to jbig2_priv.h */
int jbig2_comment_ascii(Jbig2Ctx *ctx, Jbig2Segment *segment,
                                const uint8_t *segment_data);
int jbig2_comment_unicode(Jbig2Ctx *ctx, Jbig2Segment *segment,
                               const uint8_t *segment_data);

#endif /* _JBIG2_METADATA_H */
