/* Copyright 2015 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

package org.brotli.dec;

import java.nio.ByteBuffer;

/**
 * Transformations on dictionary words.
 *
 * Transform descriptor is a triplet: {prefix, operator, suffix}.
 * "prefix" and "suffix" are short strings inserted before and after transformed dictionary word.
 * "operator" is applied to dictionary word itself.
 *
 * Some operators has "built-in" parameters, i.e. parameter is defined by operator ordinal. Other
 * operators have "external" parameters, supplied via additional table encoded in shared dictionary.
 *
 * Operators:
 *  - IDENTITY (0): dictionary word is inserted "as is"
 *  - OMIT_LAST_N (1 - 9): last N octets of dictionary word are not inserted; N == ordinal
 *  - OMIT_FIRST_M (12-20): first M octets of dictionary word are not inserted; M == ordinal - 11
 *  - UPPERCASE_FIRST (10): first "scalar" is XOR'ed with number 32
 *  - UPPERCASE_ALL (11): all "scalars" are XOR'ed with number 32
 *  - SHIFT_FIRST (21): first "scalar" is shifted by number form parameter table
 *  - SHIFT_ALL (22): all "scalar" is shifted by number form parameter table
 *
 * Here "scalar" is a variable length character coding similar to UTF-8 encoding.
 * UPPERCASE_XXX / SHIFT_XXX operators were designed to change the case of UTF-8 encoded characters.
 * While UPPERCASE_XXX works well only on ASCII charset, SHIFT is much more generic and could be
 * used for most (all?) alphabets.
 */
final class Transform {

  static final class Transforms {
    final int numTransforms;
    final int[] triplets;
    final byte[] prefixSuffixStorage;
    final int[] prefixSuffixHeads;
    final short[] params;

    Transforms(int numTransforms, int prefixSuffixLen, int prefixSuffixCount) {
      this.numTransforms = numTransforms;
      this.triplets = new int[numTransforms * 3];
      this.params = new short[numTransforms];
      this.prefixSuffixStorage = new byte[prefixSuffixLen];
      this.prefixSuffixHeads = new int[prefixSuffixCount + 1];
    }
  }

  static final int NUM_RFC_TRANSFORMS = 121;
  static final Transforms RFC_TRANSFORMS = new Transforms(NUM_RFC_TRANSFORMS, 167, 50);

  private static final int OMIT_FIRST_LAST_LIMIT = 9;

  private static final int IDENTITY = 0;
  private static final int OMIT_LAST_BASE = IDENTITY + 1 - 1;  // there is no OMIT_LAST_0.
  private static final int UPPERCASE_FIRST = OMIT_LAST_BASE + OMIT_FIRST_LAST_LIMIT + 1;
  private static final int UPPERCASE_ALL = UPPERCASE_FIRST + 1;
  private static final int OMIT_FIRST_BASE = UPPERCASE_ALL + 1 - 1;  // there is no OMIT_FIRST_0.
  private static final int SHIFT_FIRST = OMIT_FIRST_BASE + OMIT_FIRST_LAST_LIMIT + 1;
  private static final int SHIFT_ALL = SHIFT_FIRST + 1;

  // Bundle of 0-terminated strings.
  private static final String PREFIX_SUFFIX_SRC = "# #s #, #e #.# the #.com/#\u00C2\u00A0# of # and"
      + " # in # to #\"#\">#\n#]# for # a # that #. # with #'# from # by #. The # on # as # is #ing"
      + " #\n\t#:#ed #(# at #ly #=\"# of the #. This #,# not #er #al #='#ful #ive #less #est #ize #"
      + "ous #";
  private static final String TRANSFORMS_SRC = "     !! ! ,  *!  &!  \" !  ) *   * -  ! # !  #!*!  "
      + "+  ,$ !  -  %  .  / #   0  1 .  \"   2  3!*   4%  ! # /   5  6  7  8 0  1 &   $   9 +   : "
      + " ;  < '  !=  >  ?! 4  @ 4  2  &   A *# (   B  C& ) %  ) !*# *-% A +! *.  D! %'  & E *6  F "
      + " G% ! *A *%  H! D  I!+!  J!+   K +- *4! A  L!*4  M  N +6  O!*% +.! K *G  P +%(  ! G *D +D "
      + " Q +# *K!*G!+D!+# +G +A +4!+% +K!+4!*D!+K!*K";

  private static void unpackTransforms(byte[] prefixSuffix,
      int[] prefixSuffixHeads, int[] transforms, String prefixSuffixSrc, String transformsSrc) {
    final int n = prefixSuffixSrc.length();
    int index = 1;
    int j = 0;
    for (int i = 0; i < n; ++i) {
      final int c = (int) prefixSuffixSrc.charAt(i);
      if (c == 35) { // == #
        prefixSuffixHeads[index++] = j;
      } else {
        prefixSuffix[j++] = (byte) c;
      }
    }

    for (int i = 0; i < NUM_RFC_TRANSFORMS * 3; ++i) {
      transforms[i] = (int) transformsSrc.charAt(i) - 32;
    }
  }

  static {
    unpackTransforms(RFC_TRANSFORMS.prefixSuffixStorage, RFC_TRANSFORMS.prefixSuffixHeads,
        RFC_TRANSFORMS.triplets, PREFIX_SUFFIX_SRC, TRANSFORMS_SRC);
  }

  static int transformDictionaryWord(byte[] dst, int dstOffset, ByteBuffer src, int srcOffset,
      int wordLen, Transforms transforms, int transformIndex) {
    int offset = dstOffset;
    final int[] triplets = transforms.triplets;
    final byte[] prefixSuffixStorage = transforms.prefixSuffixStorage;
    final int[] prefixSuffixHeads = transforms.prefixSuffixHeads;
    final int transformOffset = 3 * transformIndex;
    final int prefixIdx = triplets[transformOffset];
    final int transformType = triplets[transformOffset + 1];
    final int suffixIdx = triplets[transformOffset + 2];
    int prefix = prefixSuffixHeads[prefixIdx];
    final int prefixEnd = prefixSuffixHeads[prefixIdx + 1];
    int suffix = prefixSuffixHeads[suffixIdx];
    final int suffixEnd = prefixSuffixHeads[suffixIdx + 1];

    int omitFirst = transformType - OMIT_FIRST_BASE;
    int omitLast = transformType - OMIT_LAST_BASE;
    if (omitFirst < 1 || omitFirst > OMIT_FIRST_LAST_LIMIT) {
      omitFirst = 0;
    }
    if (omitLast < 1 || omitLast > OMIT_FIRST_LAST_LIMIT) {
      omitLast = 0;
    }

    // Copy prefix.
    while (prefix != prefixEnd) {
      dst[offset++] = prefixSuffixStorage[prefix++];
    }

    int len = wordLen;
    // Copy trimmed word.
    if (omitFirst > len) {
      omitFirst = len;
    }
    int dictOffset = srcOffset + omitFirst;
    len -= omitFirst;
    len -= omitLast;
    int i = len;
    while (i > 0) {
      dst[offset++] = src.get(dictOffset++);
      i--;
    }

    // Ferment.
    if (transformType == UPPERCASE_FIRST || transformType == UPPERCASE_ALL) {
      int uppercaseOffset = offset - len;
      if (transformType == UPPERCASE_FIRST) {
        len = 1;
      }
      while (len > 0) {
        final int c0 = (int) dst[uppercaseOffset] & 0xFF;
        if (c0 < 0xC0) {
          if (c0 >= 97 && c0 <= 122) { // in [a..z] range
            dst[uppercaseOffset] = (byte) ((int) dst[uppercaseOffset] ^ 32);
          }
          uppercaseOffset += 1;
          len -= 1;
        } else if (c0 < 0xE0) {
          dst[uppercaseOffset + 1] = (byte) ((int) dst[uppercaseOffset + 1] ^ 32);
          uppercaseOffset += 2;
          len -= 2;
        } else {
          dst[uppercaseOffset + 2] = (byte) ((int) dst[uppercaseOffset + 2] ^ 5);
          uppercaseOffset += 3;
          len -= 3;
        }
      }
    } else if (transformType == SHIFT_FIRST || transformType == SHIFT_ALL) {
      int shiftOffset = offset - len;
      final int param = (int) transforms.params[transformIndex];
      /* Limited sign extension: scalar < (1 << 24). */
      int scalar = (param & 0x7FFF) + (0x1000000 - (param & 0x8000));
      while (len > 0) {
        int step = 1;
        final int c0 = (int) dst[shiftOffset] & 0xFF;
        if (c0 < 0x80) {
          /* 1-byte rune / 0sssssss / 7 bit scalar (ASCII). */
          scalar += c0;
          dst[shiftOffset] = (byte) (scalar & 0x7F);
        } else if (c0 < 0xC0) {
          /* Continuation / 10AAAAAA. */
        } else if (c0 < 0xE0) {
          /* 2-byte rune / 110sssss AAssssss / 11 bit scalar. */
          if (len >= 2) {
            final int c1 = (int) dst[shiftOffset + 1];
            scalar += (c1 & 0x3F) | ((c0 & 0x1F) << 6);
            dst[shiftOffset] = (byte) (0xC0 | ((scalar >> 6) & 0x1F));
            dst[shiftOffset + 1] = (byte) ((c1 & 0xC0) | (scalar & 0x3F));
            step = 2;
          } else {
            step = len;
          }
        } else if (c0 < 0xF0) {
          /* 3-byte rune / 1110ssss AAssssss BBssssss / 16 bit scalar. */
          if (len >= 3) {
            final int c1 = (int) dst[shiftOffset + 1];
            final int c2 = (int) dst[shiftOffset + 2];
            scalar += (c2 & 0x3F) | ((c1 & 0x3F) << 6) | ((c0 & 0x0F) << 12);
            dst[shiftOffset] = (byte) (0xE0 | ((scalar >> 12) & 0x0F));
            dst[shiftOffset + 1] = (byte) ((c1 & 0xC0) | ((scalar >> 6) & 0x3F));
            dst[shiftOffset + 2] = (byte) ((c2 & 0xC0) | (scalar & 0x3F));
            step = 3;
          } else {
            step = len;
          }
        } else if (c0 < 0xF8) {
          /* 4-byte rune / 11110sss AAssssss BBssssss CCssssss / 21 bit scalar. */
          if (len >= 4) {
            final int c1 = (int) dst[shiftOffset + 1];
            final int c2 = (int) dst[shiftOffset + 2];
            final int c3 = (int) dst[shiftOffset + 3];
            scalar += (c3 & 0x3F) | ((c2 & 0x3F) << 6) | ((c1 & 0x3F) << 12) | ((c0 & 0x07) << 18);
            dst[shiftOffset] = (byte) (0xF0 | ((scalar >> 18) & 0x07));
            dst[shiftOffset + 1] = (byte) ((c1 & 0xC0) | ((scalar >> 12) & 0x3F));
            dst[shiftOffset + 2] = (byte) ((c2 & 0xC0) | ((scalar >> 6) & 0x3F));
            dst[shiftOffset + 3] = (byte) ((c3 & 0xC0) | (scalar & 0x3F));
            step = 4;
          } else {
            step = len;
          }
        }
        shiftOffset += step;
        len -= step;
        if (transformType == SHIFT_FIRST) {
          len = 0;
        }
      }
    }

    // Copy suffix.
    while (suffix != suffixEnd) {
      dst[offset++] = prefixSuffixStorage[suffix++];
    }

    return offset - dstOffset;
  }
}
