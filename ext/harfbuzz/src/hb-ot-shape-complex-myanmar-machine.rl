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

#ifndef HB_OT_SHAPE_COMPLEX_MYANMAR_MACHINE_HH
#define HB_OT_SHAPE_COMPLEX_MYANMAR_MACHINE_HH

#include "hb.hh"

enum myanmar_syllable_type_t {
  myanmar_consonant_syllable,
  myanmar_punctuation_cluster,
  myanmar_broken_cluster,
  myanmar_non_myanmar_cluster,
};

%%{
  machine myanmar_syllable_machine;
  alphtype unsigned char;
  write exports;
  write data;
}%%

%%{

export A    = 10;
export As   = 18;
export C    = 1;
export D    = 32;
export D0   = 20;
export DB   = 3;
export GB   = 11;
export H    = 4;
export IV   = 2;
export MH   = 21;
export ML   = 33;
export MR   = 22;
export MW   = 23;
export MY   = 24;
export PT   = 25;
export V    = 8;
export VAbv = 26;
export VBlw = 27;
export VPre = 28;
export VPst = 29;
export VS   = 30;
export ZWJ  = 6;
export ZWNJ = 5;
export Ra   = 16;
export P    = 31;
export CS   = 19;

j = ZWJ|ZWNJ;			# Joiners
k = (Ra As H);			# Kinzi

c = C|Ra;			# is_consonant

medial_group = MY? As? MR? ((MW MH? ML? | MH ML? | ML) As?)?;
main_vowel_group = (VPre.VS?)* VAbv* VBlw* A* (DB As?)?;
post_vowel_group = VPst MH? ML? As* VAbv* A* (DB As?)?;
pwo_tone_group = PT A* DB? As?;

complex_syllable_tail = As* medial_group main_vowel_group post_vowel_group* pwo_tone_group* V* j?;
syllable_tail = (H (c|IV).VS?)* (H | complex_syllable_tail);

consonant_syllable =	(k|CS)? (c|IV|D|GB).VS? syllable_tail;
punctuation_cluster =	P V;
broken_cluster =	k? VS? syllable_tail;
other =			any;

main := |*
	consonant_syllable	=> { found_syllable (myanmar_consonant_syllable); };
	j			=> { found_syllable (myanmar_non_myanmar_cluster); };
	punctuation_cluster	=> { found_syllable (myanmar_punctuation_cluster); };
	broken_cluster		=> { found_syllable (myanmar_broken_cluster); };
	other			=> { found_syllable (myanmar_non_myanmar_cluster); };
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
find_syllables_myanmar (hb_buffer_t *buffer)
{
  unsigned int p, pe, eof, ts, te, act HB_UNUSED;
  int cs;
  hb_glyph_info_t *info = buffer->info;
  %%{
    write init;
    getkey info[p].myanmar_category();
  }%%

  p = 0;
  pe = eof = buffer->len;

  unsigned int syllable_serial = 1;
  %%{
    write exec;
  }%%
}

#undef found_syllable

#endif /* HB_OT_SHAPE_COMPLEX_MYANMAR_MACHINE_HH */
