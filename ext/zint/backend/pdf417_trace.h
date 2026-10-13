/* pdf417_trace.h - Trace routines for optimal PDF417 optimization algorithm */
/*
    libzint - the open source barcode library
    Copyright (C) 2022-2023 Robin Stuart <rstuart114@gmail.com>

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

#ifndef Z_PDF417_TRACE_H
#define Z_PDF417_TRACE_H

#ifndef PDF_TRACE
#define PDF_TRACE_Edges(px, s, l, p, v)
#define PDF_TRACE_AddEdge(s, l, es, p, v, t, e) do { (void)(s); (void)(l); } while (0)
#define PDF_TRACE_NotAddEdge(s, l, es, p, v, t, e) do { (void)(s); (void)(l); } while (0)
#else

static int PDF_TRACE_getPreviousMode(struct pdf_edge *edges, struct pdf_edge *edge) {
    struct pdf_edge *previous = PDF_PREVIOUS(edges, edge);
    return previous == NULL ? PDF_ALP : previous->mode;
}

static void PDF_TRACE_EdgeToString(char *buf, const unsigned char *source, const int length, struct pdf_edge *edges,
            struct pdf_edge *edge) {
    int previousMode = PDF_TRACE_getPreviousMode(edges, edge);
    (void)length;
    if (buf) {
        sprintf(buf, "%d_%c %c(%d,%d) %d(%d,%d,%d) -> %d_%c",
            edge->from, pdf_smodes[previousMode], pdf_smodes[edge->mode], source[edge->from], edge->len, edge->size
            + edge->unit_size, edge->units, edge->unit_size, edge->size, edge->from + 1, pdf_smodes[edge->mode]);
    } else {
        printf("%d_%c %c(%d,%d) %d(%d,%d,%d) -> %d_%c",
            edge->from, pdf_smodes[previousMode], pdf_smodes[edge->mode], source[edge->from], edge->len, edge->size
            + edge->unit_size, edge->units, edge->unit_size, edge->size, edge->from + 1, pdf_smodes[edge->mode]);
    }
}

static void PDF_TRACE_Path(const unsigned char *source, const int length, struct pdf_edge *edges,
            struct pdf_edge *edge, char *result, const int result_size) {
    PDF_TRACE_EdgeToString(result, source, length, edges, edge);
    struct pdf_edge *current = PDF_PREVIOUS(edges, edge);
    while (current) {
        char s[256];
        char *pos;
        int len;
        PDF_TRACE_EdgeToString(s, source, length, edges, current);
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
        current = PDF_PREVIOUS(edges, current);
    }
    puts(result);
}

static void PDF_TRACE_Edges(const char *prefix, const unsigned char *source, const int length,
            struct pdf_edge *edges, const int vertexIndex) {
    int i, j, e_i;
    char result[1024 * 2];
    if (vertexIndex) {
        printf(prefix, vertexIndex);
    } else {
        fputs(prefix, stdout);
    }
    for (i = vertexIndex; i <= length; i++) {
        e_i = i * PDF_NUM_MODES;
        for (j = 0; j < PDF_NUM_MODES; j++) {
            if (edges[e_i + j].mode) {
                fputs(" **** ", stdout);
                PDF_TRACE_Path(source, length, edges, edges + e_i + j, result, (int) ARRAY_SIZE(result));
            }
        }
    }
}

static void PDF_TRACE_AddEdge(const unsigned char *source, const int length, struct pdf_edge *edges,
            struct pdf_edge *previous, const int vertexIndex, const int t_table, struct pdf_edge *edge) {
    const int new_size = edge->size + edge->unit_size;
    const int v_ij = vertexIndex * PDF_NUM_MODES + edge->mode - 1;
    const int v_size = edges[v_ij].size + edges[v_ij].unit_size;

    (void)source; (void)length;

    printf("add mode %c, t_table 0x%X, previous %d, from %d, len %d, v_ij %d, %d(%d,%d,%d) > %d(%d,%d,%d)\n",
            pdf_smodes[edge->mode], t_table, previous ? (int) (previous - edges) : 0, edge->from, edge->len, v_ij,
            v_size, edges[v_ij].units, edges[v_ij].unit_size, edges[v_ij].size,
            new_size, edge->units, edge->unit_size, edge->size);
}

static void PDF_TRACE_NotAddEdge(const unsigned char *source, const int length, struct pdf_edge *edges,
            struct pdf_edge *previous, const int vertexIndex, const int t_table, struct pdf_edge *edge) {
    const int new_size = edge->size + edge->unit_size;
    const int v_ij = vertexIndex * PDF_NUM_MODES + edge->mode - 1;
    const int v_size = edges[v_ij].size + edges[v_ij].unit_size;

    (void)source; (void)length;

    printf("NOT mode %c, t_table %d, previous %d, from %d, len %d, v_ij %d, %d(%d,%d,%d) <= %d(%d,%d,%d)\n",
            pdf_smodes[edge->mode], t_table, previous ? (int) (previous - edges) : 0, edge->from, edge->len, v_ij,
            v_size, edges[v_ij].units, edges[v_ij].unit_size, edges[v_ij].size,
            new_size, edge->units, edge->unit_size, edge->size);
}

#endif /* PDF_TRACE */
/* vim: set ts=4 sw=4 et : */
#endif /* Z_PDF417_TRACE_H */
