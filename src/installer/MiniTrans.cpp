/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"

namespace trans {

#include "Translations_txt.cpp"

}

static int gTranslationIdx = 0;

void SelectTranslation()
{
    LANGID langId = GetUserDefaultUILanguage();
    int idx = trans::GetLanguageIndex(langId);
    if (-1 == idx) {
        // try a neutral language if the specific sublanguage isn't available
        langId = MAKELANGID(PRIMARYLANGID(langId), SUBLANG_NEUTRAL);
        idx = trans::GetLanguageIndex(langId);
    }
    if (-1 != idx) {
        gTranslationIdx = idx;
    }
}

static int cmpWcharPtrs(const void *a, const void *b)
{
    return wcscmp(*(const WCHAR **)a, *(const WCHAR **)b);
}

const WCHAR *Translate(const WCHAR *s)
{
    const WCHAR **res = (const WCHAR **)bsearch(&s, trans::gTranslations, trans::gTranslationsCount, sizeof(s), cmpWcharPtrs);
    int idx = gTranslationIdx + res - trans::gTranslations;
    return res && trans::gTranslations[idx] ? trans::gTranslations[idx] : s;
}

// TODO: check this value in installer/uninstaller code
bool IsLanguageRtL()
{
    return trans::IsLanguageRtL(gTranslationIdx);
}
