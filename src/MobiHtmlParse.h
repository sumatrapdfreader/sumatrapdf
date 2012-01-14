/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MobiHtmlParse_h
#define MobiHtmlParse_h

#include <stdint.h>
#include <Vec.h>

#define IS_END_TAG_MASK  0x01
#define HAS_ATTR_MASK    0x02

Vec<uint8_t> *MobiHtmlToDisplay(uint8_t *s, size_t sLen, Vec<uint8_t> *html);

#endif
