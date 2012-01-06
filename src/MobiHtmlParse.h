/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MobiHtmlParse_h
#define MobiHtmlParse_h

#include <stdint.h>

uint8_t *MobiHtmlToDisplay(uint8_t *s, size_t sLen, size_t& lenOut);

#endif
