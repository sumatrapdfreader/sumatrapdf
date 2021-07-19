/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Log.h"
#include "Translations.h"

#ifdef DEBUG
// define for adding "English RTL" for testing RTL layout
#define ADD_EN_RTL_TEST_LANGUAGE
#endif

namespace trans {

// defined in Trans*_txt.cpp
extern int gLangsCount;
extern int gStringsCount;
extern const char* gLangNames;
extern const char* gLangCodes;
extern const LANGID* GetLangIds();
extern bool IsLangRtl(int langIdx);
extern const char** GetOriginalStrings();
} // namespace trans

// set to 1 to use new translation code
#define USE_OLD_TRANS 1

#if USE_OLD_TRANS

// Note: this code is intentionally optimized for (small) size, not speed

namespace trans {

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
    if (gMissingTranslationsCount >= dimof(gMissingTranslations)) {
        return L"missing translation";
    }

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
    if (!gLangsTransCache[langIdx]) {
        gLangsTransCache[langIdx] = AllocArray<WCHAR*>(gStringsCount);
    }
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
        if (0 == *s) {
            gCurrLangStrings[i] = nullptr;
        } else {
            gCurrLangStrings[i] = s;
        }
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
        if (langId == langIds[i]) {
            return GetLangCodeByIdx(i);
        }
    }

    // see if we have a translation in a language that has the same
    // primary id as user's language and neutral sublang
    LANGID userLangIdNeutral = MAKELANGID(PRIMARYLANGID(langId), SUBLANG_NEUTRAL);
    for (int i = 0; i < gLangsCount; i++) {
        if (userLangIdNeutral == langIds[i]) {
            return GetLangCodeByIdx(i);
        }
    }

    return "en";
}

static int GetEnglishStringIndex(const char* txt) {
    const char** origStrings = GetOriginalStrings();
    for (int idx = 0; idx < gStringsCount; idx++) {
        const char* s = origStrings[idx];
        if (str::Eq(s, txt)) {
            return idx;
        }
    }
    return -1;
}

const char* GetTranslationATemp(const char* s) {
    if (nullptr == gCurrLangCode) {
        SetCurrentLangByCode("en");
    }

    int idx = GetEnglishStringIndex(s);
    if (-1 == idx) {
        return s;
    }

    const char* trans = gCurrLangStrings[idx];
    // fall back to English if the language doesn't have a translations for this string
    if (!trans) {
        trans = s;
    }
    return trans;
}

const WCHAR* GetTranslationTemp(const char* s) {
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

const WCHAR* _TR(const char* s) {
    return trans::GetTranslationTemp(s);
}

const char* _TRA(const char* s) {
    return trans::GetTranslationATemp(s);
}

#else

#include "utils/WinUtil.h"

namespace trans {

// used locally, gCurrLangCode points into gLangCodes
static const char* gCurrLangCode = nullptr;

struct StringsParser {
    std::string_view orig{};
    std::string_view rest{};
    char* currOrig{nullptr};
};

std::string_view SkipPast(std::string_view sv, char c) {
    const char* s = sv.data();
    const char* e = s + sv.size();
    while (s < e) {
        if (*s == c) {
            s++;
            size_t len = (e - s);
            return {s, len};
        }
        s++;
    }
    return {};
}

char* GetLineUnescaped(char* s, char* e) {
    char* w = s;
    while (s < e) {
        if (*s == '\n') {
            *s = 0;
            return s + 1;
        }
        if (*s != '\\') {
            *w++ = *s++;
        } else {
            // TODO: implement me
            //&&(s + 1 < e) && (*s)
        }
    }
    return nullptr;
}

void SkipPast(StringsParser& p, char c) {
    p.rest = SkipPast(p.rest, c);
}

bool ParseOrigString(StringsParser& p) {
    // TODO: write me
    // store parsed string in p.orig
    std::string_view sv = p.rest;
    if (sv.empty()) {
        // TODO: need to detect a valid eof
        return false;
    }
    // starts with ':'
    char* s = (char*)sv.data();
    char* e = s + sv.size();
    if (*s != ':') {
        return false;
    }
    s++;
    p.currOrig = s;
    while (s < e) {
        // TODO; write me
    }
    return false;
}

bool ParseTranslations(StringsParser& p) {
    // TODO: write me
    // parses $lang: $translation\n
    return false;
}

void ParseTranslationsTxt(std::string_view s) {
    StringsParser p{};
    p.orig = s;
    p.rest = s;
    // skip AppTranslator: SumatraPDF
    SkipPast(p, '\n');
    // skip 1b35f5aeed9fc7aacdd4ed60c3a3b489943bc7d7
    SkipPast(p, '\n');
}

void ParseTranslationsFromResources() {
    std::span<u8> d = LoadDataResource(2);
    if (d.empty()) {
        return;
    }
    ParseTranslationsTxt({(const char*)d.data(), d.size()});
    free(d.data());
}

int GetLangsCount() {
    return gLangsCount;
}

const char* GetCurrentLangCode() {
    return gCurrLangCode;
}

// translation info about a single string
// we set str/trans once by parsing translations.txt file
// after the user changes the language
struct Translation {
    // english string from translations.txt file
    // we lazily match it to origStr
    char* str{nullptr};
    // translation of str/origStr in gCurrLangCode
    WCHAR* trans{nullptr};
};

static Translation* gTranslations{nullptr};
static int gCurrLangIdx{0};

static void FreeTranslations() {
    if (!gTranslations) {
        return;
    }
    for (int i = 0; i < gLangsCount; i++) {
        str::Free(gTranslations[i].str);
        str::Free(gTranslations[i].trans);
    }
}

void BuildTranslationsForLang(int langIdx, const char* langCode) {
    if (langIdx == 0) {
        // if english, do nothing
        return;
    }
    // TODO: write me
}

void SetCurrentLangByCode(const char* langCode) {
    if (str::Eq(langCode, gCurrLangCode)) {
        return;
    }
    FreeTranslations();

    int idx = seqstrings::StrToIdx(gLangCodes, langCode);
    if (-1 == idx) {
        logf("SetCurrentLangByCode: unknown lang code: '%s'\n", langCode);
        // set to English
        idx = 0;
    }
    CrashIf(-1 == idx);
    gCurrLangIdx = idx;
    gCurrLangCode = GetLangCodeByIdx(idx);
    BuildTranslationsForLang(gCurrLangIdx, gCurrLangCode);
}

const char* ValidateLangCode(const char* langCode) {
    int idx = seqstrings::StrToIdx(gLangCodes, langCode);
    if (-1 == idx) {
        return nullptr;
    }
    return GetLangCodeByIdx(idx);
}

const WCHAR* GetTranslationTemp(const char* s) {
    return ToWstrTemp(s);
}

const char* GetTranslationATemp(const char* s) {
    return s;
}

const char* GetLangCodeByIdx(int idx) {
    return seqstrings::IdxToStr(gLangCodes, idx);
}

const char* GetLangNameByIdx(int idx) {
    return seqstrings::IdxToStr(gLangNames, idx);
}

bool IsCurrLangRtl() {
    return IsLangRtl(gCurrLangIdx);
}

const char* DetectUserLang() {
    const LANGID* langIds = GetLangIds();
    LANGID langId = GetUserDefaultUILanguage();
    // try the exact match
    for (int i = 0; i < gLangsCount; i++) {
        if (langId == langIds[i]) {
            return GetLangCodeByIdx(i);
        }
    }

    // see if we have a translation in a language that has the same
    // primary id as user's language and neutral sublang
    LANGID userLangIdNeutral = MAKELANGID(PRIMARYLANGID(langId), SUBLANG_NEUTRAL);
    for (int i = 0; i < gLangsCount; i++) {
        if (userLangIdNeutral == langIds[i]) {
            return GetLangCodeByIdx(i);
        }
    }

    return "en";
}

void Destroy() {
    if (!gCurrLangCode) {
        // no need for clean-up if translations were never initialized
        return;
    }
    FreeTranslations();
    free((void*)gTranslations);
    gTranslations = nullptr;
}

} // namespace trans

const WCHAR* _TR(const char* s) {
    return trans::GetTranslationTemp(s);
}

const char* _TRA(const char* s) {
    return trans::GetTranslationATemp(s);
}

#endif
