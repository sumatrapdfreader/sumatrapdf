/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "SumatraConfig.h"

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

namespace trans {

// used locally, gCurrLangCode points into gLangCodes
static const char* gCurrLangCode = nullptr;
static int gCurrLangIdx = 0;
// for each translation: english string followed by a translation
static StrVec* gTranslationCache = nullptr;

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
    delete gTranslationCache;
    gTranslationCache = nullptr;
}

static void ParseTranslationsTxt(const StrSpan& d, const char* langCode) {
    langCode = str::JoinTemp(langCode, ":");
    int nLangCode = str::Len(langCode);

    StrVec lines;
    Split(&lines, d.CStr(), "\n", true);
    int nStrings = 0;
    for (char* l : lines) {
        if (l[0] == ':') {
            nStrings++;
        }
    }
    int nLines = lines.Size();
    logf("ParseTranslationsTxt: %d lines, nStrings: %d\n", nLines, nStrings);

    delete gTranslationCache;
    gTranslationCache = new StrVec();
    auto c = gTranslationCache;
    int nUntranslated = 0;

    char* orig;
    char* trans;
    char* line;
    int i = 2; // skip first 2 header lines
    while (i < nLines) {
        orig = lines[i];
        ReportDebugIf(*orig != ':');
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
            nUntranslated++;
        }
        TempStr unescaped = UnescapeTemp(orig);
        c->Append(unescaped);
        if (!trans) {
            c->Append(nullptr);
            continue;
        }
        unescaped = UnescapeTemp(trans);
        c->Append(unescaped);
    }
    int nTrans = c->Size();
    ReportDebugIf(nTrans != nStrings * 2);
    if (nUntranslated > 0 && !str::Eq(langCode, "en:")) {
        logf("Untranslated strings: %d for lang '%s'\n", nUntranslated, langCode);
    }
}

// don't free
const char* GetTranslation(const char* s) {
    if (gCurrLangIdx == 0) {
        // 0 is english, no translation needed
        return s;
    }
    auto c = gTranslationCache;
    int n = c->Size();
    ReportDebugIf(n % 2 != 0);
    n = n / 2;
    int sLen = str::Leni(s);
    for (int i = 0; i < n; i++) {
        int idx = i * 2;
        StrSpan s2 = c->AtSpan(idx);
        if (s2.Len() == sLen && str::Eq(s, s2.CStr())) {
            auto tr = c->At(idx + 1);
            if (!tr) {
                logf("Didn't find translation for '%s'\n", s);
                return s;
            }
            return tr;
        }
    }
    ReportDebugIf(true);
    return s;
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
    ReportDebugIf(-1 == idx);
    gCurrLangIdx = idx;
    gCurrLangCode = GetLangCodeByIdx(idx);
    if (idx == 0 && !gIsDebugBuild) {
        // perf: in release builds we skip parsing translations for english
        // in debug we want to execute this code to catch errors
        return;
    }
    StrSpan d = LoadDataResource(2);
    ParseTranslationsTxt(d, langCode);
    str::Free(d);
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
