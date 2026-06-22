/*
 * Copyright © 2015  Mozilla Foundation.
 * Copyright © 2015  Google, Inc.
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
 * Mozilla Author(s): Jonathan Kew
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_SHAPER_USE_MACHINE_HH
#define HB_OT_SHAPER_USE_MACHINE_HH

#include "hb.hh"

#include "hb-ot-shaper-syllabic.hh"

/* buffer var allocations */
#define use_category() ot_shaper_var_u8_category()

#define USE(Cat) use_syllable_machine_ex_##Cat

enum use_syllable_type_t {
  use_virama_terminated_cluster,
  use_sakot_terminated_cluster,
  use_standard_cluster,
  use_number_joiner_terminated_cluster,
  use_numeral_cluster,
  use_symbol_cluster,
  use_hieroglyph_cluster,
  use_broken_cluster,
  use_non_cluster,
};

%%{
  machine use_syllable_machine;
  alphtype unsigned char;
  write exports;
  write data;
}%%

%%{

# Categories used in the Universal Shaping Engine spec:
# https://docs.microsoft.com/en-us/typography/script-development/use

export O	= 0; # OTHER

export B	= 1; # BASE
export N	= 4; # BASE_NUM
export GB	= 5; # BASE_OTHER
export CGJ	= 6; # CGJ
export SUB	= 11; # CONS_SUB
export H	= 12; # HALANT

export HN	= 13; # HALANT_NUM
export ZWNJ	= 14; # Zero width non-joiner
export WJ	= 16; # Word joiner
export R	= 18; # REPHA
export CS	= 43; # CONS_WITH_STACKER
export IS	= 44; # INVISIBLE_STACKER
export Sk	= 48; # SAKOT
export G	= 49; # HIEROGLYPH
export J	= 50; # HIEROGLYPH_JOINER
export SB	= 51; # HIEROGLYPH_SEGMENT_BEGIN
export SE	= 52; # HIEROGLYPH_SEGMENT_END
export HVM	= 53; # HALANT_OR_VOWEL_MODIFIER
export HM	= 54; # HIEROGLYPH_MOD
export HR	= 55; # HIEROGLYPH_MIRROR
export RK	= 56; # REORDERING_KILLER

export FAbv	= 24; # CONS_FINAL_ABOVE
export FBlw	= 25; # CONS_FINAL_BELOW
export FPst	= 26; # CONS_FINAL_POST
export MAbv	= 27; # CONS_MED_ABOVE
export MBlw	= 28; # CONS_MED_BELOW
export MPst	= 29; # CONS_MED_POST
export MPre	= 30; # CONS_MED_PRE
export CMAbv	= 31; # CONS_MOD_ABOVE
export CMBlw	= 32; # CONS_MOD_BELOW
export VAbv	= 33; # VOWEL_ABOVE / VOWEL_ABOVE_BELOW / VOWEL_ABOVE_BELOW_POST / VOWEL_ABOVE_POST
export VBlw	= 34; # VOWEL_BELOW / VOWEL_BELOW_POST
export VPst	= 35; # VOWEL_POST	UIPC = Right
export VPre	= 22; # VOWEL_PRE / VOWEL_PRE_ABOVE / VOWEL_PRE_ABOVE_POST / VOWEL_PRE_POST
export VMAbv	= 37; # VOWEL_MOD_ABOVE
export VMBlw	= 38; # VOWEL_MOD_BELOW
export VMPst	= 39; # VOWEL_MOD_POST
export VMPre	= 23; # VOWEL_MOD_PRE
export SMAbv	= 41; # SYM_MOD_ABOVE
export SMBlw	= 42; # SYM_MOD_BELOW
export FMAbv	= 45; # CONS_FINAL_MOD	UIPC = Top
export FMBlw	= 46; # CONS_FINAL_MOD	UIPC = Bottom
export FMPst	= 47; # CONS_FINAL_MOD	UIPC = Not_Applicable


h = H | HVM | IS | Sk;

consonant_modifiers = CMAbv* CMBlw* ((h B | SUB) CMAbv* CMBlw*)*;
medial_consonants = MPre? MAbv? MBlw? MPst?;
dependent_vowels = VPre* VAbv* VBlw* VPst* | H;
vowel_modifiers = HVM? VMPre* VMAbv* VMBlw* VMPst*;
final_consonants = FAbv* FBlw* FPst*;
final_modifiers = FMAbv* FMBlw* | FMPst?;

complex_syllable_start = (R | CS)? (B | GB);
complex_syllable_middle =
	consonant_modifiers
	medial_consonants
	dependent_vowels
	vowel_modifiers
	(Sk B)*
;
complex_syllable_tail =
	complex_syllable_middle
	final_consonants
	final_modifiers
;
number_joiner_terminated_cluster_tail = (HN N)* HN;
numeral_cluster_tail = (HN N)+;
symbol_cluster_tail = SMAbv+ SMBlw* | SMBlw+;

virama_terminated_cluster_tail =
	consonant_modifiers
	(IS | RK)
;
virama_terminated_cluster =
	complex_syllable_start
	virama_terminated_cluster_tail
;
sakot_terminated_cluster_tail =
	complex_syllable_middle
	Sk
;
sakot_terminated_cluster =
	complex_syllable_start
	sakot_terminated_cluster_tail
;
standard_cluster =
	complex_syllable_start
	complex_syllable_tail
;
tail = complex_syllable_tail | sakot_terminated_cluster_tail | symbol_cluster_tail | virama_terminated_cluster_tail;
broken_cluster =
	R?
	(tail | number_joiner_terminated_cluster_tail | numeral_cluster_tail)
;

number_joiner_terminated_cluster = N number_joiner_terminated_cluster_tail;
numeral_cluster = N numeral_cluster_tail?;
symbol_cluster = (O | GB | SB) tail?;
hieroglyph_cluster = SB* G HR? HM? SE* (J SB* (G HR? HM? SE*)?)*;
other = any;

main := |*
	virama_terminated_cluster ZWNJ?		=> { found_syllable (use_virama_terminated_cluster); };
	sakot_terminated_cluster ZWNJ?		=> { found_syllable (use_sakot_terminated_cluster); };
	standard_cluster ZWNJ?			=> { found_syllable (use_standard_cluster); };
	number_joiner_terminated_cluster ZWNJ?	=> { found_syllable (use_number_joiner_terminated_cluster); };
	numeral_cluster ZWNJ?			=> { found_syllable (use_numeral_cluster); };
	symbol_cluster ZWNJ?			=> { found_syllable (use_symbol_cluster); };
	hieroglyph_cluster ZWNJ?		=> { found_syllable (use_hieroglyph_cluster); };
	FMPst					=> { found_syllable (use_non_cluster); };
	broken_cluster ZWNJ?			=> { found_syllable (use_broken_cluster); buffer->scratch_flags |= HB_BUFFER_SCRATCH_FLAG_HAS_BROKEN_SYLLABLE; };
	other					=> { found_syllable (use_non_cluster); };
*|;


}%%

#define found_syllable(syllable_type) \
  HB_STMT_START { \
    if (0) fprintf (stderr, "syllable %u..%u %s\n", (*ts).second.first, (*te).second.first, #syllable_type); \
    for (unsigned i = (*ts).second.first; i < (*te).second.first; ++i) \
      info[i].syllable() = (syllable_serial << 4) | syllable_type; \
    syllable_serial++; \
    if (syllable_serial == 16) syllable_serial = 1; \
  } HB_STMT_END


template <typename Iter>
struct machine_index_t :
  hb_iter_with_fallback_t<machine_index_t<Iter>,
			  typename Iter::item_t>
{
  machine_index_t (const Iter& it) : it (it) {}
  machine_index_t (const machine_index_t& o) : hb_iter_with_fallback_t<machine_index_t<Iter>,
								       typename Iter::item_t> (),
					       it (o.it), is_null (o.is_null) {}

  static constexpr bool is_random_access_iterator = Iter::is_random_access_iterator;
  static constexpr bool is_sorted_iterator = Iter::is_sorted_iterator;

  typename Iter::item_t __item__ () const { return *it; }
  typename Iter::item_t __item_at__ (unsigned i) const { return it[i]; }
  unsigned __len__ () const { return it.len (); }
  void __next__ () { ++it; }
  void __forward__ (unsigned n) { it += n; }
  void __prev__ () { --it; }
  void __rewind__ (unsigned n) { it -= n; }

  void operator = (unsigned n)
  {
    assert (n == 0);
    is_null = true;
  }
  explicit operator bool () { return !is_null; }

  void operator = (const machine_index_t& o)
  {
    is_null = o.is_null;
    unsigned index = (*it).first;
    unsigned n = (*o.it).first;
    if (index < n) it += n - index; else if (index > n) it -= index - n;
  }
  bool operator == (const machine_index_t& o) const
  { return is_null ? o.is_null : !o.is_null && (*it).first == (*o.it).first; }
  bool operator != (const machine_index_t& o) const { return !(*this == o); }

  private:
  Iter it;
  bool is_null = false;
};
struct
{
  template <typename Iter,
	    hb_requires (hb_is_iterable (Iter))>
  machine_index_t<hb_iter_type<Iter>>
  operator () (Iter&& it) const
  { return machine_index_t<hb_iter_type<Iter>> (hb_iter (it)); }
}
HB_FUNCOBJ (machine_index);



static bool
not_ccs_default_ignorable (const hb_glyph_info_t &i)
{ return i.use_category() != USE(CGJ); }

static inline void
find_syllables_use (hb_buffer_t *buffer)
{
  hb_glyph_info_t *info = buffer->info;
  auto p =
    + hb_iter (info, buffer->len)
    | hb_enumerate
    | hb_filter ([] (const hb_glyph_info_t &i) { return not_ccs_default_ignorable (i); },
		 hb_second)
    | hb_filter ([&] (const hb_pair_t<unsigned, const hb_glyph_info_t &> p)
		 {
		   if (p.second.use_category() == USE(ZWNJ))
		     for (unsigned i = p.first + 1; i < buffer->len; ++i)
		       if (not_ccs_default_ignorable (info[i]))
			 return !_hb_glyph_info_is_unicode_mark (&info[i]);
		   return true;
		 })
    | hb_enumerate
    | machine_index
    ;
  auto pe = p + p.len ();
  auto eof = +pe;
  auto ts = +p;
  auto te = +p;
  unsigned int act HB_UNUSED;
  int cs;
  %%{
    write init;
    getkey (*p).second.second.use_category();
  }%%

  unsigned int syllable_serial = 1;
  %%{
    write exec;
  }%%
}

#undef found_syllable

#endif /* HB_OT_SHAPER_USE_MACHINE_HH */
