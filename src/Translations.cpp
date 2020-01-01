/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Log.h"
#include "Translations.h"

#ifdef DEBUG
// define for adding "English RTL" for testing RTL layout
#define ADD_EN_RTL_TEST_LANGUAGE
#endif

// Note: this code is intentionally optimized for (small) size, not speed

namespace trans {

// defined in Trans*_txt.cpp
extern int gLangsCount;
extern int gStringsCount;
extern const char* gLangNames;
extern const char* gLangCodes;
const LANGID* GetLangIds();
bool IsLangRtl(int langIdx);
const char** GetOriginalStrings();

// used locally, gCurrLangCode points into gLangCodes
static const char* gCurrLangCode = nullptr;
static int gCurrLangIdx = 0;

// Note: we don't have access to STRINGS_COUNT and LANGS_COUNT
// hence foo[] => *foo here
// const char *gCurrLangStrings[STRINGS_COUNT];
const char** gCurrLangStrings = nullptr;
// WCHAR ** gLangsTransCache[LANGS_COUNT];
WCHAR*** gLangsTransCache = nullptr;

const char* GetTranslationsForLang(int langIdx);

#ifdef ADD_EN_RTL_TEST_LANGUAGE
#define EN_RTL_CODE "en-rtl"
#define EN_RTL_NAME "English RTL for testing"
#define EN_RTL_IDX gLangsCount
#endif

/* In general, after adding new _TR() strings, one has to re-generate Translations_txt.cpp, but
that also requires uploading new strings to the server, for which one needs access.

To support adding new strings without re-generating Translatiosn_txt.cpp, we have a concept
of missing translations. */

struct MissingTranslation {
    const char* s;
    const WCHAR* translation;
};

// number of missing translations should be small
static MissingTranslation gMissingTranslations[64];
static int gMissingTranslationsCount = 0;

static void FreeMissingTranslations() {
    for (int i = 0; i < gMissingTranslationsCount; i++) {
        str::Free(gMissingTranslations[i].translation);
    }
    gMissingTranslationsCount = 0;
}

static const WCHAR* FindOrAddMissingTranslation(const char* s) {
    for (int i = 0; i < gMissingTranslationsCount; i++) {
        if (s == gMissingTranslations[i].s) {
            return gMissingTranslations[i].translation;
        }
    }
    if (gMissingTranslationsCount >= dimof(gMissingTranslations))
        return L"missing translation";

    gMissingTranslations[gMissingTranslationsCount].s = s;
    const WCHAR* res = strconv::Utf8ToWstr(s);
    gMissingTranslations[gMissingTranslationsCount].translation = res;
    gMissingTranslationsCount++;
    return res;
}

int GetLangsCount() {
#ifdef ADD_EN_RTL_TEST_LANGUAGE
    return gLangsCount + 1;
#else
    return gLangsCount;
#endif
}

const char* GetCurrentLangCode() {
    return gCurrLangCode;
}

static WCHAR** GetTransCacheForLang(int langIdx) {
#ifdef ADD_EN_RTL_TEST_LANGUAGE
    if (langIdx == EN_RTL_IDX)
        langIdx = 0;
#endif
    if (!gLangsTransCache[langIdx])
        gLangsTransCache[langIdx] = AllocArray<WCHAR*>(gStringsCount);
    return gLangsTransCache[langIdx];
}

static void FreeTransCache() {
    for (int langIdx = 0; langIdx < gLangsCount; langIdx++) {
        WCHAR** transCache = gLangsTransCache[langIdx];
        for (int i = 0; transCache && i < gStringsCount; i++) {
            free(transCache[i]);
        }
        free(transCache);
    }
    free(gLangsTransCache);
    free(gCurrLangStrings);
}

static void BuildStringsIndexForLang(int langIdx) {
#ifdef ADD_EN_RTL_TEST_LANGUAGE
    if (0 == gCurrLangIdx || EN_RTL_IDX == gCurrLangIdx) {
#else
    if (0 == gCurrLangIdx) {
#endif
        const char** origStrings = GetOriginalStrings();
        for (int idx = 0; idx < gStringsCount; idx++) {
            gCurrLangStrings[idx] = origStrings[idx];
            CrashIf(!gCurrLangStrings[idx]);
        }
        return;
    }

    const char* s = GetTranslationsForLang(langIdx);
    for (int i = 0; i < gStringsCount; i++) {
        if (0 == *s)
            gCurrLangStrings[i] = nullptr;
        else
            gCurrLangStrings[i] = s;
        // advance to the next string
        while (*s) {
            ++s;
        }
        ++s;
    }
}

void SetCurrentLangByCode(const char* langCode) {
    if (!gCurrLangStrings) {
        gCurrLangStrings = AllocArray<const char*>(gStringsCount);
        gLangsTransCache = AllocArray<WCHAR**>(gLangsCount);
    }

    if (str::Eq(langCode, gCurrLangCode)) {
        return;
    }

    int idx = seqstrings::StrToIdx(gLangCodes, langCode);
#ifdef ADD_EN_RTL_TEST_LANGUAGE
    if (-1 == idx && str::Eq(langCode, EN_RTL_CODE)) {
        idx = EN_RTL_IDX;
    }
#endif
    if (-1 == idx) {
        logf("Unknown lang code: '%s'\n", langCode);
        // set to English
        idx = 0;
    }
    CrashIf(-1 == idx);
    gCurrLangIdx = idx;
    gCurrLangCode = GetLangCodeByIdx(idx);
    BuildStringsIndexForLang(gCurrLangIdx);
}

const char* ValidateLangCode(const char* langCode) {
    int idx = seqstrings::StrToIdx(gLangCodes, langCode);
#ifdef ADD_EN_RTL_TEST_LANGUAGE
    if (-1 == idx && str::Eq(langCode, EN_RTL_CODE)) {
        idx = EN_RTL_IDX;
    }
#endif
    if (-1 == idx) {
        return nullptr;
    }
    return GetLangCodeByIdx(idx);
}

const char* GetLangCodeByIdx(int idx) {
#ifdef ADD_EN_RTL_TEST_LANGUAGE
    if (idx == EN_RTL_IDX)
        return EN_RTL_CODE;
#endif
    return seqstrings::IdxToStr(gLangCodes, idx);
}

const char* GetLangNameByIdx(int idx) {
#ifdef ADD_EN_RTL_TEST_LANGUAGE
    if (idx == EN_RTL_IDX)
        return EN_RTL_NAME;
#endif
    return seqstrings::IdxToStr(gLangNames, idx);
}

bool IsCurrLangRtl() {
#ifdef ADD_EN_RTL_TEST_LANGUAGE
    if (gCurrLangIdx == EN_RTL_IDX)
        return true;
#endif
    return IsLangRtl(gCurrLangIdx);
}

const char* DetectUserLang() {
    const LANGID* langIds = GetLangIds();
    LANGID langId = GetUserDefaultUILanguage();
    // try the exact match
    for (int i = 0; i < gLangsCount; i++) {
        if (langId == langIds[i])
            return GetLangCodeByIdx(i);
    }

    // see if we have a translation in a language that has the same
    // primary id as user's language and neutral sublang
    LANGID userLangIdNeutral = MAKELANGID(PRIMARYLANGID(langId), SUBLANG_NEUTRAL);
    for (int i = 0; i < gLangsCount; i++) {
        if (userLangIdNeutral == langIds[i])
            return GetLangCodeByIdx(i);
    }

    return "en";
}

static int GetEnglishStringIndex(const char* txt) {
    const char** origStrings = GetOriginalStrings();
    for (int idx = 0; idx < gStringsCount; idx++) {
        const char* s = origStrings[idx];
        if (str::Eq(s, txt))
            return idx;
    }
    return -1;
}

const WCHAR* GetTranslation(const char* s) {
    if (nullptr == gCurrLangCode) {
        SetCurrentLangByCode("en");
    }

    int idx = GetEnglishStringIndex(s);
    if (-1 == idx) {
        return FindOrAddMissingTranslation(s);
    }

    const char* trans = gCurrLangStrings[idx];
    // fall back to English if the language doesn't have a translations for this string
    if (!trans) {
        trans = s;
    }

    WCHAR** transCache = GetTransCacheForLang(gCurrLangIdx);
    if (!transCache[idx]) {
        transCache[idx] = strconv::Utf8ToWstr(trans);
    }
    return transCache[idx];
}

void Destroy() {
    if (!gCurrLangCode) {
        // no need for clean-up if translations were never initialized
        return;
    }

    FreeTransCache();
    FreeMissingTranslations();
}

} // namespace trans
