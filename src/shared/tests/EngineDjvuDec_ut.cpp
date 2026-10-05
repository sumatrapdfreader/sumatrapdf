/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/GuessFileType.h"

extern "C" {
#include "djvu.h"
}

#include "gui/UIModels.h"
#include "EngineBase.h"
#include "EngineAll.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

static djvu_text_zone MakeZone(djvu_zone_type type, const char* text, djvu_text_zone* children, int nchildren) {
    djvu_text_zone z{};
    z.type = type;
    z.x = 0;
    z.y = 0;
    z.w = 40;
    z.h = 10;
    z.text = (char*)text;
    z.children = children;
    z.nchildren = nchildren;
    return z;
}

void EngineDjvuDec_UnitTests() {
    // a line of two words: a space after each word, one rect per codepoint
    djvu_text_zone words[2] = {MakeZone(DJVU_ZONE_WORD, "ab", nullptr, 0), MakeZone(DJVU_ZONE_WORD, "cd", nullptr, 0)};
    djvu_text_zone line = MakeZone(DJVU_ZONE_LINE, "ab cd", words, 2);
    PageText pt = DjvuZonesToPageText(&line, 1.f);
    utassert(str::Eq(pt.text, StrL("ab cd ")));
    utassert(pt.nCodepoints == 6);
    FreePageText(&pt);

    // zone text is cut out of the page text by byte offsets, so a multi-byte
    // character can be split across two character zones; the page text then
    // has one codepoint where the zones have two, and every rect after it
    // was off by one (crash 2026-09-21-13-59-98ef)
    djvu_text_zone chars[2] = {MakeZone(DJVU_ZONE_CHAR, "\xD0", nullptr, 0),
                               MakeZone(DJVU_ZONE_CHAR, "\x92", nullptr, 0)};
    chars[1].x = 10;
    djvu_text_zone words2[2] = {MakeZone(DJVU_ZONE_WORD, "\xD0\x92", chars, 2),
                                MakeZone(DJVU_ZONE_WORD, "z", nullptr, 0)};
    words2[1].x = 30;
    djvu_text_zone line2 = MakeZone(DJVU_ZONE_LINE, "\xD0\x92 z", words2, 2);
    pt = DjvuZonesToPageText(&line2, 1.f);
    utassert(str::Eq(pt.text, StrL("\xD0\x92 z ")));
    utassert(pt.nCodepoints == 4);
    utassert(pt.coords[0].x == 0);
    utassert(pt.coords[2].x == 30);
    FreePageText(&pt);
}
