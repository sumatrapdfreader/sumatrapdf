/* deflate_rle.c -- compress data using RLE strategy of deflation algorithm
 *
 * Copyright (C) 1995-2013 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "deflate.h"
#include "deflate_p.h"
#include "functable.h"

/* ===========================================================================
 * For Z_RLE, simply look for runs of bytes, generate matches only of distance
 * one.  Do not maintain a hash table.  (It will be regenerated if this run of
 * deflate switches away from Z_RLE.)
 */
Z_INTERNAL block_state deflate_rle(deflate_state *s, int flush) {
    int bflush = 0;                 /* set if current block must be flushed */
    unsigned int prev;              /* byte at distance one to match */
    unsigned char *scan, *strend;   /* scan goes up to strend for length of run */
    uint32_t match_len = 0;

    for (;;) {
        /* Make sure that we always have enough lookahead, except
         * at the end of the input file. We need STD_MAX_MATCH bytes
         * for the longest run, plus one for the unrolled loop.
         */
        if (s->lookahead <= STD_MAX_MATCH) {
            fill_window(s);
            if (s->lookahead <= STD_MAX_MATCH && flush == Z_NO_FLUSH)
                return need_more;
            if (s->lookahead == 0)
                break; /* flush the current block */
        }

        /* See how many times the previous byte repeats */
        if (s->lookahead >= STD_MIN_MATCH && s->strstart > 0) {
            scan = s->window + s->strstart - 1;
            prev = *scan;
            if (prev == *++scan && prev == *++scan && prev == *++scan) {
                strend = s->window + s->strstart + STD_MAX_MATCH;
                do {
                } while (prev == *++scan && prev == *++scan &&
                         prev == *++scan && prev == *++scan &&
                         prev == *++scan && prev == *++scan &&
                         prev == *++scan && prev == *++scan &&
                         scan < strend);
                match_len = STD_MAX_MATCH - (unsigned int)(strend - scan);
                match_len = MIN(match_len, s->lookahead);
            }
            Assert(scan <= s->window + s->window_size - 1, "wild scan");
        }

        /* Emit match if have run of STD_MIN_MATCH or longer, else emit literal */
        if (match_len >= STD_MIN_MATCH) {
            check_match(s, s->strstart, s->strstart - 1, match_len);

            bflush = zng_tr_tally_dist(s, 1, match_len - STD_MIN_MATCH);

            s->lookahead -= match_len;
            s->strstart += match_len;
            match_len = 0;
        } else {
            /* No match, output a literal byte */
            bflush = zng_tr_tally_lit(s, s->window[s->strstart]);
            s->lookahead--;
            s->strstart++;
        }
        if (bflush)
            FLUSH_BLOCK(s, 0);
    }
    s->insert = 0;
    if (flush == Z_FINISH) {
        FLUSH_BLOCK(s, 1);
        return finish_done;
    }
    if (s->sym_next)
        FLUSH_BLOCK(s, 0);
    return block_done;
}
