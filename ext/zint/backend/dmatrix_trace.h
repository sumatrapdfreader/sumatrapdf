/* dmatrix_trace.h - Trace routines for DM_COMPRESSION algorithm */
/*
    libzint - the open source barcode library
    Copyright (C) 2021-2022 Robin Stuart <rstuart114@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the project nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
 */
/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef Z_DMATRIX_TRACE_H
#define Z_DMATRIX_TRACE_H

#ifndef DM_TRACE
#define DM_TRACE_Edges(px, s, l, p, v)
#define DM_TRACE_AddEdge(s, l, es, p, v, e)
#define DM_TRACE_NotAddEdge(s, l, es, p, v, ij, e)
#else

static int DM_TRACE_getPreviousMode(struct dm_edge *edges, struct dm_edge *edge) {
    struct dm_edge *previous = DM_PREVIOUS(edges, edge);
    return previous == NULL ? DM_ASCII : previous->endMode;
}

static void DM_TRACE_VertexToString(const unsigned char *source, const int length, const int position,
            struct dm_edge *edge) {
    if (position >= length) {
        printf("end mode %s", dm_smodes[edge->mode]);
    } else {
        printf("char '%c' at %d mode %s", source[position], position, dm_smodes[edge->mode]);
    }
}

static void DM_TRACE_EdgeToString(char *buf, const unsigned char *source, const int length, struct dm_edge *edges,
            struct dm_edge *edge) {
    int previousMode = DM_TRACE_getPreviousMode(edges, edge);
    (void)length;
    if (buf) {
        sprintf(buf, "%d_%s %s(%.*s) (%d) --> %d_%s",
            edge->from, dm_smodes[previousMode], dm_smodes[edge->mode], edge->len, source + edge->from, edge->size,
            edge->from + edge->len, dm_smodes[edge->mode]);
    } else {
        printf("%d_%s %s(%.*s) (%d) --> %d_%s",
            edge->from, dm_smodes[previousMode], dm_smodes[edge->mode], edge->len, source + edge->from, edge->size,
            edge->from + edge->len, dm_smodes[edge->mode]);
    }
}

static void DM_TRACE_Path(const unsigned char *source, const int length, struct dm_edge *edges,
            struct dm_edge *edge, char *result, const int result_size) {
    DM_TRACE_EdgeToString(result, source, length, edges, edge);
    struct dm_edge *current = DM_PREVIOUS(edges, edge);
    while (current) {
        char s[256];
        char *pos;
        int len;
        DM_TRACE_EdgeToString(s, source, length, edges, current);
        pos = strrchr(s, ' ');
        assert(pos);
        len = strlen(result);
        if ((pos - s) + 1 + len + 1 >= result_size) {
            result[result_size - 4] = '\0';
            strcat(result, "...");
            break;
        }
        memmove(result + (pos - s) + 1, result, len + 1);
        memcpy(result, s, (pos - s) + 1);
        current = DM_PREVIOUS(edges, current);
    }
    puts(result);
}

static void DM_TRACE_Edges(const char *prefix, const unsigned char *source, const int length,
            struct dm_edge *edges, const int vertexIndex) {
    int i, j, e_i;
    char result[1024 * 2];
    if (vertexIndex) {
        printf(prefix, vertexIndex);
    } else {
        fputs(prefix, stdout);
    }
    for (i = vertexIndex; i <= length; i++) {
        e_i = i * DM_NUM_MODES;
        for (j = 0; j < DM_NUM_MODES; j++) {
            if (edges[e_i + j].mode) {
                fputs("DEBUG ", stdout);
                DM_TRACE_Path(source, length, edges, edges + e_i + j, result, (int) ARRAY_SIZE(result));
            }
        }
    }
}

static void DM_TRACE_AddEdge(const unsigned char *source, const int length, struct dm_edge *edges,
            struct dm_edge *previous, const int vertexIndex, struct dm_edge *edge) {
    if (previous == NULL) {
        fputs("DEBUG add ", stdout);
        DM_TRACE_EdgeToString(NULL, source, length, edges, edge);
        printf(" from %d to %d size %d\n", edge->from, vertexIndex, edge->size);
    } else {
        fputs("DEBUG add ", stdout);
        DM_TRACE_EdgeToString(NULL, source, length, edges, edge);
        fputs(" from ", stdout);
        DM_TRACE_VertexToString(source, length, previous->from, previous);
        fputs(" to ", stdout);
        DM_TRACE_VertexToString(source, length, vertexIndex, edge);
        printf(" size %d\n", edge->size);
    }
}

static void DM_TRACE_NotAddEdge(const unsigned char *source, const int length, struct dm_edge *edges,
            struct dm_edge *previous, const int vertexIndex, const int e_ij, struct dm_edge *edge) {
    if (previous == NULL) {
        fputs("DEBUG not add ", stdout);
        DM_TRACE_EdgeToString(NULL, source, length, edges, edge);
        printf(" from %d to %d size %d since ", edge->from, vertexIndex, edge->size);
        DM_TRACE_EdgeToString(NULL, source, length, edges, edges + e_ij);
        printf(" < size %d\n", edges[e_ij].size);
    } else {
        fputs("DEBUG not add ", stdout);
        DM_TRACE_EdgeToString(NULL, source, length, edges, edge);
        fputs(" from ", stdout);
        DM_TRACE_VertexToString(source, length, previous->from, previous);
        fputs(" to ", stdout);
        DM_TRACE_VertexToString(source, length, vertexIndex, edge);
        printf(" size %d since ", edge->size);
        DM_TRACE_EdgeToString(NULL, source, length, edges, edges + e_ij);
        printf(" < size %d\n", edges[e_ij].size);
    }
}

#endif /* DM_TRACE */
/* vim: set ts=4 sw=4 et : */
#endif /* Z_DMATRIX_TRACE_H */
