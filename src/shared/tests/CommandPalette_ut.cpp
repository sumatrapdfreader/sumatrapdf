/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "FilterUtil.h"
#include "Commands.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

static void PaletteSwitchLabel_UnitTests() {
    struct {
        const char* label;
        const char* prefix;
        const char* expected;
    } cases[] = {
        {"& Thumbnails", "&", "Thumbnails"},
        {"& 缩略图", "&", "缩略图"},
        {"缩略图", "&", "缩略图"},
        {"&缩略图", "&", "缩略图"},
        {"縮圖", "&", "縮圖"},
        {"Vignettes (&)", "&", "Vignettes (&)"},
        {"@ 标签页", "@", "标签页"},
        {"= Settings", "=", "Settings"},
        {"", "&", ""},
        {"&", "&", ""},
    };
    for (auto& c : cases) {
        utassert(str::Eq(PaletteSwitchLabel(Str(c.label), Str(c.prefix)), Str(c.expected)));
    }
}

// the command palette's filter over real command names: every word, any case
static bool PaletteMatches(int cmdId, Str query) {
    StrVec words;
    SplitFilterToWords(query, words);
    return FilterMatches(GetCommandDescription(cmdId), words);
}

// the prefixes docs/md/Command-Palette.md documents, and what they select
static void PalettePrefixes_UnitTests() {
    struct {
        const char* query;
        PaletteMode mode;
        const char* rest;
    } cases[] = {
        {"", PaletteMode::Commands, ""},
        {"zoom", PaletteMode::Commands, "zoom"},
        {">zoom", PaletteMode::Commands, "zoom"},
        {"#doc", PaletteMode::FileHistory, "doc"},
        {"@tab", PaletteMode::Tabs, "tab"},
        {":all", PaletteMode::Everything, "all"},
        {"%chapter", PaletteMode::Toc, "chapter"},
        {"$fav", PaletteMode::Favorites, "fav"},
        {"*annot", PaletteMode::Annotations, "annot"},
        {"=ZoomIncrement = 25", PaletteMode::Settings, "ZoomIncrement = 25"},
        {"&", PaletteMode::Thumbnails, ""},
    };
    for (auto& c : cases) {
        Str rest(c.query);
        PaletteMode mode = ParsePaletteMode(rest);
        utassert(mode == c.mode);
        utassert(str::Eq(rest, Str(c.rest)));
    }
    // the prefix is only a prefix: a '#' inside the query is part of the text
    Str rest = StrL("a#b");
    utassert(ParsePaletteMode(rest) == PaletteMode::Commands);
    utassert(str::Eq(rest, StrL("a#b")));
}

void CommandPaletteModel_UnitTests() {
    PaletteSwitchLabel_UnitTests();
    PalettePrefixes_UnitTests();
    utassert(PaletteMatches(CmdRotateRight, StrL("rotate right")));
    utassert(!PaletteMatches(CmdRotateLeft, StrL("rotate right")));
    utassert(PaletteMatches(CmdZoomFitWidth, StrL("FIT width")));
    utassert(!PaletteMatches(CmdOpenFile, StrL("missing")));
    utassert(PaletteMatches(CmdOpenFile, {}));

    // ignore diacritics
    StrVec words;
    SplitFilterToWords(StrL("lacz CAFE"), words);
    utassert(FilterMatches(StrL("\xc5\x81\xc4\x85\x63\x7a caf\xc3\xa9"), words)); // Łącz café
    int matchLen = 0;
    utassert(FilterIndexOf(StrL("x\xc5\x81\xc4\x85\x63\x7a"), StrL("lacz"), &matchLen) == 1);
    utassert(matchLen == 6);
    utassert(FilterIndexOf(StrL("cafe\xcc\x81!"), StrL("caf\xc3\xa9"), &matchLen) == 0); // decomposed é
    utassert(matchLen == 6);
    utassert(FilterIndexOf(StrL("abc"), StrL("\xcc\x81"), &matchLen) < 0);
}
