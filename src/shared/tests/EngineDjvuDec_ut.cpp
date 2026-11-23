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

#if IS_DEBUG
bool EngineDjvuDec_UnitTestRender();
#endif

static void AppendBe(str::Builder& sb, u32 v, int nBytes) {
    for (int i = nBytes - 1; i >= 0; i--) {
        sb.AppendChar((char)((v >> (i * 8)) & 0xff));
    }
}

// single page DjVu whose TXTa holds a chain of zones, each the only child of the previous
static void BuildNestedZonesDjvu(str::Builder& sb, int depth) {
    str::Builder txt;
    AppendBe(txt, 0, 3); // text length
    txt.AppendChar(1);   // version
    for (int i = 0; i < depth; i++) {
        txt.AppendChar(DJVU_ZONE_PAGE);
        for (int j = 0; j < 5; j++) {
            AppendBe(txt, 0x8000, 2); // x, y, w, h, text offset (biased 0)
        }
        AppendBe(txt, 0, 3);                     // text length
        AppendBe(txt, i + 1 < depth ? 1 : 0, 3); // children
    }
    sb.Append(StrL("AT&TFORM"));
    AppendBe(sb, 4 + 8 + len(txt), 4);
    sb.Append(StrL("DJVUTXTa"));
    AppendBe(sb, len(txt), 4);
    sb.Append(ToStr(txt));
}

static bool NestedZonesParse(int depth) {
    str::Builder data;
    BuildNestedZonesDjvu(data, depth);
    djvu_ctx* ctx = djvu_ctx_new(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    Str d = ToStr(data);
    djvu_doc* doc = djvu_doc_open(ctx, (const u8*)d.s, len(d));
    utassert(doc);
    djvu_page_text_zones* zones = djvu_page_text_get_zones(doc, 0);
    bool ok = zones && zones->root;
    djvu_text_zones_destroy(ctx, zones);
    djvu_doc_close(doc);
    djvu_ctx_free(ctx);
    return ok;
}

void EngineDjvuDec_UnitTests() {
#if IS_DEBUG
    utassert(EngineDjvuDec_UnitTestRender());
#endif
    // deeply nested text zones are rejected instead of overflowing the stack
    utassert(NestedZonesParse(3));
    utassert(!NestedZonesParse(200000));

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
