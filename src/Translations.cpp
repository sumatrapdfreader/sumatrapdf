/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "Translations.h"

#include "utils/Log.h"

namespace trans {

// defined in Trans*_txt.cpp
extern int gLangsCount;
extern const char* gLangNames;
extern const char* gLangCodes;
extern const LANGID* GetLangIds();
extern bool IsLangRtl(int langIdx);
extern const char** GetOriginalStrings();
} // namespace trans

namespace trans {

// translation info about a single string
// we set str/trans once by parsing translations.txt file
// after the user changes the language
struct Translation {
    // index points inside allStrings;
    u16 idxStr = 0;
    // translation of str/origStr in gCurrLangCode
    // index points inside allTranslations
    u16 idxTrans = 0;
    // translation in WCHAR*, points inside allTranslationsW
    u16 idxTransW = 0;
};

struct TranslationCache {
    // english string from translations.txt file
    // we lazily match it to origStr
    str::Str allStrings;
    str::Str allTranslations;
    str::WStr allTranslationsW;
    // TODO: maybe also cache str::WStr allTranslationsW
    // we currently return a temp converted string but there's a chance their
    // lifetime could survive temp allocator lifetime
    Translation* translations = nullptr;
    int nTranslations = 0;
    int nUntranslated = 0;
};

// used locally, gCurrLangCode points into gLangCodes
static const char* gCurrLangCode = nullptr;
static int gCurrLangIdx = 0;
static TranslationCache* gTranslationCache = nullptr;

static void UnescapeStringIntoStr(char* s, str::Str& str) {
    for (; *s; s++) {
        if (*s == '\\') {
            char c = s[1];
            switch (c) {
                case 't':
                    str.AppendChar('\t');
                    break;
                case 'n':
                    str.AppendChar('\n');
                    break;
                case 'r':
                    str.AppendChar('\r');
                    break;
                default:
                    str.AppendChar(c);
                    break;
            }
            s++;
        } else {
            str.AppendChar(*s);
        }
    }
    str.AppendChar(0);
}

static void FreeTranslations() {
    if (!gTranslationCache) {
        return;
    }
    delete gTranslationCache;
    gTranslationCache = nullptr;
}

static void ParseTranslationsTxt(const ByteSlice& d, const char* langCode) {
    langCode = str::JoinTemp(langCode, ":");
    int nLangCode = str::Len(langCode);

    // parse into lines
    char* s = (char*)d.data();
    StrVec lines;
    Split(lines, s, "\n", true);
    int nStrings = 0;
    for (char* l : lines) {
        if (l[0] == ':') {
            nStrings++;
        }
    }
    int nLines = lines.Size();
    logf("ParseTranslationsTxt: %d lines, nStrings: %d\n", nLines, nStrings);

    FreeTranslations();
    gTranslationCache = new TranslationCache();
    // make index of first string to be 1 so that 0 can be
    // "missing" value in transIdx
    gTranslationCache->allStrings.AppendChar(' ');
    gTranslationCache->allTranslations.AppendChar(' ');
    gTranslationCache->allTranslationsW.AppendChar(' ');
    auto c = gTranslationCache;
    c->nTranslations = nStrings;
    c->translations = AllocArray<Translation>(c->nTranslations);
    c->nUntranslated = 0;

    char* orig;
    char* trans;
    char* line;
    int i = 2; // skip first 2 header lines
    int nTrans = 0;
    while (i < nLines) {
        orig = lines[i];
        CrashIf(*orig != ':');
        orig += 1; // skip the ':' at the beginning
        i++;
        trans = nullptr;
        while (i < nLines && lines[i][0] != ':') {
            if (!trans) {
                line = lines[i];
                if (str::StartsWith(line, langCode)) {
                    trans = line + nLangCode;
                }
            }
            i++;
        }
        if (!trans) {
            c->nUntranslated++;
        }
        Translation& translation = c->translations[nTrans++];
        size_t idxStr = c->allStrings.size();
        // when this fires, we'll have to bump strIdx form u16 to u32
        CrashIf(idxStr > 64 * 1024);
        translation.idxStr = (u16)idxStr;
        UnescapeStringIntoStr(orig, c->allStrings);
        char* toConvert = c->allStrings.LendData() + idxStr; // after insertion because could re-allocate
        if (trans) {
            size_t idxTrans = c->allTranslations.size();
            CrashIf(idxTrans > 64 * 1024);
            translation.idxTrans = (u16)idxTrans;
            UnescapeStringIntoStr(trans, c->allTranslations);
            toConvert = c->allTranslations.LendData() + idxTrans; // after insertion because could re-allocate
        }
        // if we don't have a translation, we cache WCHAR* version from original string
        auto ws = ToWStrTemp(toConvert);
        size_t idxTransW = c->allTranslationsW.size();
        CrashIf(idxTransW > 64 * 1024);
        translation.idxTransW = (u16)idxTransW;
        size_t wsLen = str::Len(ws);
        c->allTranslationsW.Append(ws, wsLen + 1);
    }
    CrashIf(nTrans != c->nTranslations);
    if (c->nUntranslated > 0 && !str::Eq(langCode, "en:")) {
        logf("Untranslated strings: %d for lang '%s'\n", c->nUntranslated, langCode);
    }
}

static Translation* FindTranslation(const char* s) {
    CrashIf(!s);
    CrashIf(!gTranslationCache);
    auto c = gTranslationCache;
    for (int i = 0; i < c->nTranslations; i++) {
        Translation& trans = c->translations[i];
        size_t idx = trans.idxStr;
        const char* s2 = c->allStrings.LendData() + idx;
        if (str::Eq(s, s2)) {
            return &trans;
        }
    }
    return nullptr;
}

// don't free
const char* GetTranslationA(const char* s) {
    if (gCurrLangIdx == 0) {
        return s;
    }
    Translation* trans = FindTranslation(s);
    // we don't have a translation for this string
    if (!trans || trans->idxTrans == 0) {
        logf("Didn't find translation for '%s'\n", s);
        return s;
    }
    auto idx = trans->idxTrans;
    return gTranslationCache->allTranslations.LendData() + idx;
}

// don't free
const WCHAR* GetTranslation(const char* s) {
    Translation* trans = FindTranslation(s);
    // we don't have a translation for this string
    if (!trans || trans->idxTransW == 0) {
        logf("GetTranslation: didn't find translation for '%s'\n", s);
        // shouldn't happen
        // ReportIf(true);
        // it's a mem leak but the contract is that those strings
        // are not freed and survive long enough they can't be temp strings
        return ToWstr(s);
    }
    auto idx = trans->idxTransW;
    return gTranslationCache->allTranslationsW.LendData() + idx;
}

int GetLangsCount() {
    return gLangsCount;
}

const char* GetCurrentLangCode() {
    return gCurrLangCode;
}

void SetCurrentLangByCode(const char* langCode) {
    if (str::Eq(langCode, gCurrLangCode)) {
        return;
    }

    int idx = seqstrings::StrToIdx(gLangCodes, langCode);
    if (idx < 0) {
        logf("SetCurrentLangByCode: unknown lang code: '%s'\n", langCode);
        // set to English
        idx = 0;
    }
    CrashIf(-1 == idx);
    gCurrLangIdx = idx;
    gCurrLangCode = GetLangCodeByIdx(idx);

    ByteSlice d = LoadDataResource(2);
    CrashIf(d.empty());
    ParseTranslationsTxt(d, langCode);
    free(d.data());
}

const char* ValidateLangCode(const char* langCode) {
    int idx = seqstrings::StrToIdx(gLangCodes, langCode);
    if (idx < 0) {
        return nullptr;
    }
    return GetLangCodeByIdx(idx);
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
    FreeTranslations();
}

} // namespace trans

const WCHAR* _TR(const char* s) {
    return trans::GetTranslation(s);
}

const char* _TRA(const char* s) {
    return trans::GetTranslationA(s);
}
