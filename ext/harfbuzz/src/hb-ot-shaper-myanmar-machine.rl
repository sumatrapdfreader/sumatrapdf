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

#ifndef HB_OT_SHAPER_MYANMAR_MACHINE_HH
#define HB_OT_SHAPER_MYANMAR_MACHINE_HH

#include "hb.hh"

#include "hb-ot-layout.hh"
#include "hb-ot-shaper-indic.hh"

/* buffer var allocations */
#define myanmar_category() ot_shaper_var_u8_category() /* myanmar_category_t */
#define myanmar_position() ot_shaper_var_u8_auxiliary() /* myanmar_position_t */

using myanmar_category_t = unsigned;
using myanmar_position_t = ot_position_t;

#define M_Cat(Cat) myanmar_syllable_machine_ex_##Cat

enum myanmar_syllable_type_t {
  myanmar_consonant_syllable,
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


# Spec category D is folded into GB; D0 is not implemented by Uniscribe and as such folded into D
# Spec category P is folded into GB

export C    = 1;
export IV   = 2;
export DB   = 3;	# Dot below	     = OT_N
export H    = 4;
export ZWNJ = 5;
export ZWJ  = 6;
export SM    = 8;	# Visarga and Shan tones
export GB   = 10;	# 		     = OT_PLACEHOLDER
export DOTTEDCIRCLE = 11;
export A    = 9;
export Ra   = 15;
export CS   = 18;

export VAbv = 20;
export VBlw = 21;
export VPre = 22;
export VPst = 23;

# 32+ are for Myanmar-specific values
export As   = 32;	# Asat
export MH   = 35;	# Medial Ha
export MR   = 36;	# Medial Ra
export MW   = 37;	# Medial Wa, Shan Wa
export MY   = 38;	# Medial Ya, Mon Na, Mon Ma
export PT   = 39;	# Pwo and other tones
export VS   = 40;	# Variation selectors
export ML   = 41;	# Medial Mon La


j = ZWJ|ZWNJ;			# Joiners
k = (Ra As H);			# Kinzi

c = C|Ra;			# is_consonant

medial_group = MY? As? MR? ((MW MH? ML? | MH ML? | ML) As?)?;
main_vowel_group = (VPre.VS?)* VAbv* VBlw* A* (DB As?)?;
post_vowel_group = VPst MH? ML? As* VAbv* A* (DB As?)?;
pwo_tone_group = PT A* DB? As?;

complex_syllable_tail = As* medial_group main_vowel_group post_vowel_group* pwo_tone_group* SM* j?;
syllable_tail = (H (c|IV).VS?)* (H | complex_syllable_tail);

consonant_syllable =	(k|CS)? (c|IV|GB|DOTTEDCIRCLE).VS? syllable_tail;
broken_cluster =	k? VS? syllable_tail;
other =			any;

main := |*
	consonant_syllable	=> { found_syllable (myanmar_consonant_syllable); };
	j			=> { found_syllable (myanmar_non_myanmar_cluster); };
	broken_cluster		=> { found_syllable (myanmar_broken_cluster); buffer->scratch_flags |= HB_BUFFER_SCRATCH_FLAG_HAS_BROKEN_SYLLABLE; };
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

inline void
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

#endif /* HB_OT_SHAPER_MYANMAR_MACHINE_HH */
