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
} // namespace trans

constexpr u16 kIdxMissing = 0xffff;

namespace trans {

// translation info about a single string
// we set str/trans once by parsing translations.txt file
// after the user changes the language
struct Translation {
    // index in allStrings;
    u16 idxStr = 0;
    // translation of str/origStr in gCurrLangCode
    // index in allTranslations
    u16 idxTrans = 0;
};

struct TranslationCache {
    // english string from translations.txt file
    // we lazily match it to origStr
    StrVec allStrings;
    StrVec allTranslations;
    Translation* translations = nullptr;
    int nTranslations = 0;
    int nUntranslated = 0;
};

// used locally, gCurrLangCode points into gLangCodes
static const char* gCurrLangCode = nullptr;
static int gCurrLangIdx = 0;
static TranslationCache* gTranslationCache = nullptr;

static TempStr UnescapeTemp(char* sOrig) {
    char* s = str::DupTemp(sOrig);
    char* unescaped = s;
    char* dst = s;
    char c, c2;
    while (*s) {
        c = *s++;
        if (c != '\\') {
            *dst++ = c;
            continue;
        }
        c2 = *s;
        switch (c2) {
            case '\\':
                *dst++ = '\\';
                break;
            case 't':
                *dst++ = '\t';
                break;
            case 'n':
                *dst++ = '\n';
                break;
            case 'r':
                *dst++ = '\r';
                break;
            default:
                *dst++ = c;
                break;
        }
        s++;
    }
    *dst = 0;
    return unescaped;
}

static void FreeTranslations() {
    if (!gTranslationCache) {
        return;
    }
    delete gTranslationCache;
    gTranslationCache = nullptr;
}

static void ParseTranslationsTxt(const StrSpan& d, const char* langCode) {
    langCode = str::JoinTemp(langCode, ":");
    int nLangCode = str::Len(langCode);

    // parse into lines
    char* s = d.CStr();
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
        ReportIf(*orig != ':');
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
        int idxStr = c->allStrings.Size();
        // when this fires, we'll have to bump strIdx form u16 to u32
        ReportIf(idxStr > 64 * 1024);
        translation.idxStr = (u16)idxStr;
        TempStr unescaped = UnescapeTemp(orig);
        c->allStrings.Append(unescaped);
        if (!trans) {
            translation.idxTrans = kIdxMissing;
            continue;
        }
        int idxTrans = c->allTranslations.Size();
        ReportIf(idxTrans > 64 * 1024);
        translation.idxTrans = (u16)idxTrans;
        unescaped = UnescapeTemp(trans);
        c->allTranslations.Append(unescaped);
    }
    ReportIf(nTrans != c->nTranslations);
    if (c->nUntranslated > 0 && !str::Eq(langCode, "en:")) {
        logf("Untranslated strings: %d for lang '%s'\n", c->nUntranslated, langCode);
    }
}

static Translation* FindTranslation(const char* s) {
    ReportIf(!s);
    ReportIf(!gTranslationCache);
    auto c = gTranslationCache;
    for (int i = 0; i < c->nTranslations; i++) {
        Translation& trans = c->translations[i];
        int idx = (int)trans.idxStr;
        char* s2 = c->allStrings.At(idx);
        if (str::Eq(s, s2)) {
            return &trans;
        }
    }
    return nullptr;
}

// don't free
const char* GetTranslation(const char* s) {
    if (gCurrLangIdx == 0) {
        // 0 is english, no translation needed
        return s;
    }
    Translation* trans = FindTranslation(s);
    // we don't have a translation for this string
    u32 idx = trans ? trans->idxTrans : kIdxMissing;
    if (idx == kIdxMissing) {
        logf("Didn't find translation for '%s'\n", s);
        return s;
    }
    return gTranslationCache->allTranslations.At((int)idx);
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
    ReportIf(-1 == idx);
    gCurrLangIdx = idx;
    gCurrLangCode = GetLangCodeByIdx(idx);

    StrSpan d = LoadDataResource(2);
    ReportIf(d.IsEmpty());
    ParseTranslationsTxt(d, langCode);
    str::Free(d.CStr());
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

const char* _TRA(const char* s) {
    return trans::GetTranslation(s);
}
