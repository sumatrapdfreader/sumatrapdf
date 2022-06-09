/*
 * Copyright © 2011,2012  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_SHAPE_COMPLEX_KHMER_MACHINE_HH
#define HB_OT_SHAPE_COMPLEX_KHMER_MACHINE_HH

#include "hb.hh"

enum khmer_syllable_type_t {
  khmer_consonant_syllable,
  khmer_broken_cluster,
  khmer_non_khmer_cluster,
};

%%{
  machine khmer_syllable_machine;
  alphtype unsigned char;
  write exports;
  write data;
}%%

%%{

export C    = 1;
export V    = 2;
export ZWNJ = 5;
export ZWJ  = 6;
export PLACEHOLDER = 11;
export DOTTEDCIRCLE = 12;
export Coeng= 14;
export Ra   = 16;
export Robatic = 20;
export Xgroup  = 21;
export Ygroup  = 22;
export VAbv = 26;
export VBlw = 27;
export VPre = 28;
export VPst = 29;

c = (C | Ra | V);
cn = c.((ZWJ|ZWNJ)?.Robatic)?;
joiner = (ZWJ | ZWNJ);
xgroup = (joiner*.Xgroup)*;
ygroup = Ygroup*;

# This grammar was experimentally extracted from what Uniscribe allows.

matra_group = VPre? xgroup VBlw? xgroup (joiner?.VAbv)? xgroup VPst?;
syllable_tail = xgroup matra_group xgroup (Coeng.c)? ygroup;


broken_cluster =	(Coeng.cn)* (Coeng | syllable_tail);
consonant_syllable =	(cn|PLACEHOLDER|DOTTEDCIRCLE) broken_cluster;
other =			any;

main := |*
	consonant_syllable	=> { found_syllable (khmer_consonant_syllable); };
	broken_cluster		=> { found_syllable (khmer_broken_cluster); };
	other			=> { found_syllable (khmer_non_khmer_cluster); };
*|;


}%%

#define found_syllable(syllable_type) \
  HB_STMT_START { \
    if (0) fprintf (stderr, "syllable %d..%d %s\n", ts, te, #syllable_type); \
    for (unsigned int i = ts; i < te; i++) \
      info[i].syllable() = (syllable_serial << 4) | syllable_type; \
    syllable_serial++; \
    if (unlikely (syllable_serial == 16)) syllable_serial = 1; \
  } HB_STMT_END

static void
find_syllables_khmer (hb_buffer_t *buffer)
{
  unsigned int p, pe, eof, ts, te, act HB_UNUSED;
  int cs;
  hb_glyph_info_t *info = buffer->info;
  %%{
    write init;
    getkey info[p].khmer_category();
  }%%

  p = 0;
  pe = eof = buffer->len;

  unsigned int syllable_serial = 1;
  %%{
    write exec;
  }%%
}

#undef found_syllable

#endif /* HB_OT_SHAPE_COMPLEX_KHMER_MACHINE_HH */
