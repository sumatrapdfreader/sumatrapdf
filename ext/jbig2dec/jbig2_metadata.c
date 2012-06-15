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

#include <stdlib.h>
#include <string.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_metadata.h"

/* metadata key,value list object */
Jbig2Metadata *jbig2_metadata_new(Jbig2Ctx *ctx, Jbig2Encoding encoding)
{
    Jbig2Metadata *md = jbig2_new(ctx, Jbig2Metadata, 1);

    if (md != NULL) {
        md->encoding = encoding;
        md->entries = 0;
        md->max_entries = 4;
        md->keys = jbig2_new(ctx, char*, md->max_entries);
        md->values = jbig2_new(ctx, char*, md->max_entries);
        if (md->keys == NULL || md->values == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
                "failed to allocate storage for metadata keys/values");
            jbig2_metadata_free(ctx, md);
            md = NULL;
        }
    } else {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
            "failed to allocate storage for metadata");
    }
    return md;
}

void jbig2_metadata_free(Jbig2Ctx *ctx, Jbig2Metadata *md)
{
    int i;

    if (md->keys) {
      /* assume we own the pointers */
      for (i = 0; i < md->entries; i++)
        jbig2_free(ctx->allocator, md->keys[i]);
      jbig2_free(ctx->allocator, md->keys);
    }
    if (md->values) {
      for (i = 0; i < md->entries; i++)
        jbig2_free(ctx->allocator, md->values[i]);
      jbig2_free(ctx->allocator, md->values);
    }
    jbig2_free(ctx->allocator, md);
}

static char *jbig2_strndup(Jbig2Ctx *ctx, const char *c, const int len)
{
    char *s = jbig2_new(ctx, char, len);
    if (s == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
            "unable to duplicate comment string");
    } else {
        memcpy(s, c, len);
    }
    return s;
}

int jbig2_metadata_add(Jbig2Ctx *ctx, Jbig2Metadata *md,
                        const char *key, const int key_length,
                        const char *value, const int value_length)
{
    char **keys, **values;

    /* grow the array if necessary */
    if (md->entries == md->max_entries) {
        md->max_entries <<= 1;
        keys = jbig2_renew(ctx, md->keys, char*, md->max_entries);
        values = jbig2_renew(ctx, md->values, char*, md->max_entries);
        if (keys == NULL || values == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
                "unable to resize metadata structure");
            return -1;
        }
        md->keys = keys;
        md->values = values;
    }

    /* copy the passed key,value pair */
    md->keys[md->entries] = jbig2_strndup(ctx, key, key_length);
    md->values[md->entries] = jbig2_strndup(ctx, value, value_length);
    md->entries++;

    return 0;
}


/* decode an ascii comment segment 7.4.15.1 */
int jbig2_comment_ascii(Jbig2Ctx *ctx, Jbig2Segment *segment,
                               const uint8_t *segment_data)
{
    char *s = (char *)(segment_data + 4);
    char *end = (char *)(segment_data + segment->data_length);
    Jbig2Metadata *comment;
    char *key, *value;
    int key_length, value_length;

    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
        "ASCII comment data");

    comment = jbig2_metadata_new(ctx, JBIG2_ENCODING_ASCII);
    if (comment == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
            "unable to allocate comment structure");
        return -1;
    }
    /* loop over the segment data pulling out the key,value pairs */
    while(*s && s < end) {
        key_length = strlen(s) + 1;
        key = s; s += key_length;
        if (s >= end) goto too_short;
        value_length = strlen(s) + 1;
        value = s; s += value_length;
        if (s >= end) goto too_short;
        jbig2_metadata_add(ctx, comment, key, key_length, value, value_length);
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
            "'%s'\t'%s'", key, value);
    }

    /* TODO: associate with ctx, page, or referred-to segment(s) */
    segment->result = comment;

    return 0;

too_short:
    jbig2_metadata_free(ctx, comment);
    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unexpected end of comment segment");
}

/* decode a UCS-16 comment segement 7.4.15.2 */
int jbig2_comment_unicode(Jbig2Ctx *ctx, Jbig2Segment *segment,
                               const uint8_t *segment_data)
{
    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled unicode comment segment");
}
