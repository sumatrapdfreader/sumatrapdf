/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "ShortcutParse.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

static bool AccelShowsAs(BYTE virt, WORD key, Str expected) {
    ACCEL a{};
    a.fVirt = virt;
    a.key = key;
    TempStr s = AppendAccelKeyToMenuStringTemp(StrL(""), a);
    if (len(s) < 1 || s.s[0] != '\t') {
        return false;
    }
    return str::Eq(Str(s.s + 1, len(s) - 1), expected);
}

bool ShortcutParse_UnitTestShiftedPunct() {
    auto prevLang = gShortcutLangCode;
    gShortcutLangCode = nullptr;

    utassert(AccelShowsAs(FSHIFT | FVIRTKEY, VK_OEM_2, StrL("?")));
    utassert(AccelShowsAs(FVIRTKEY, VK_OEM_2, StrL("/")));
    utassert(AccelShowsAs(FCONTROL | FSHIFT | FVIRTKEY, VK_OEM_2, StrL("Ctrl + ?")));
    utassert(AccelShowsAs(FSHIFT | FVIRTKEY, VK_OEM_COMMA, StrL("<")));
    utassert(AccelShowsAs(FSHIFT | FVIRTKEY, VK_OEM_PERIOD, StrL(">")));
    utassert(AccelShowsAs(FSHIFT | FVIRTKEY, VK_OEM_4, StrL("{")));
    utassert(AccelShowsAs(FSHIFT | FVIRTKEY, VK_OEM_6, StrL("}")));
    utassert(AccelShowsAs(FSHIFT | FVIRTKEY, VK_OEM_5, StrL("|")));
    utassert(AccelShowsAs(FSHIFT | FVIRTKEY, VK_OEM_1, StrL(":")));
    utassert(AccelShowsAs(FSHIFT | FVIRTKEY, VK_OEM_7, StrL("\"")));
    utassert(AccelShowsAs(FSHIFT | FVIRTKEY, VK_OEM_3, StrL("~")));
    utassert(AccelShowsAs(FSHIFT | FVIRTKEY, 'A', StrL("Shift + A")));
    // VK codes that equal a punctuation char ('\'' is VK_RIGHT) aren't punctuation
    utassert(AccelShowsAs(FCONTROL | FSHIFT | FVIRTKEY, VK_RIGHT, StrL("Ctrl + Shift + Right")));
    utassert(AccelShowsAs(FSHIFT | FVIRTKEY, VK_DELETE, StrL("Shift + Del")));
    utassert(AccelShowsAs(FSHIFT | FVIRTKEY, VK_SNAPSHOT, StrL("Shift + PrtSc")));

    ACCEL a{};
    utassert(ParseShortcutString(StrL("<"), a));
    utassert(a.key == VK_OEM_COMMA && a.fVirt == (FSHIFT | FVIRTKEY));
    a = {};
    utassert(ParseShortcutString(StrL("Ctrl + \""), a));
    utassert(a.key == VK_OEM_7 && a.fVirt == (FCONTROL | FSHIFT | FVIRTKEY));

    gShortcutLangCode = prevLang;
    return true;
}
