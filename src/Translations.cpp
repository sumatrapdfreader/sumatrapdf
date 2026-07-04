/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "base/LzmaSimpleArchive.h"

#include "SumatraConfig.h"

#include "Translations.h"
#include "resource.h"

#include "base/Log.h"

namespace trans {

// defined in Trans*_txt.cpp
extern int gLangsCount;
extern SeqStrings gLangNames;
extern SeqStrings gLangCodes;
extern const LANGID* GetLangIds();
extern bool IsLangRtl(int langIdx);
} // namespace trans

namespace trans {

// used locally, gCurrLangCode is a view into gLangCodes
static Str gCurrLangCode;
static int gCurrLangIdx = 0;
// for each translation: english string followed by a translation
static StrVec* gTranslationCache = nullptr;

static TempStr UnescapeTemp(Str sOrig) {
    TempStr s = str::DupTemp(sOrig);
    char* unescaped = s.s;
    char* dst = s.s;
    char* src = s.s;
    char c, c2;
    while (*src) {
        c = *src++;
        if (c != '\\') {
            *dst++ = c;
            continue;
        }
        c2 = *src;
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
        src++;
    }
    *dst = 0;
    return unescaped;
}

static void FreeTranslations() {
    delete gTranslationCache;
    gTranslationCache = nullptr;
}

static void ParseTranslationsTxt(Str d, Str langCode) {
    TempStr langCodePref = str::JoinTemp(langCode, StrL(":"));
    int nLangCode = langCodePref.len;

    StrVec lines;
    Split(&lines, d, "\n", true);
    int nStrings = 0;
    for (Str l : lines) {
        if (l && l.s[0] == ':') {
            nStrings++;
        }
    }
    int nLines = len(lines);
    logf("ParseTranslationsTxt: %d lines, nStrings: %d\n", nLines, nStrings);

    delete gTranslationCache;
    gTranslationCache = new StrVec();
    auto c = gTranslationCache;
    int nUntranslated = 0;

    Str orig;
    Str trans;
    int i = 2; // skip first 2 header lines
    while (i < nLines) {
        Str origLine = lines[i];
        ReportDebugIf(!origLine || origLine.s[0] != ':');
        orig = Str(origLine.s + 1, origLine.len - 1);
        i++;
        trans = {};
        while (i < nLines && lines[i] && lines[i].s[0] != ':') {
            if (!trans) {
                Str line = lines[i];
                if (str::StartsWith(line, langCodePref)) {
                    trans = Str(line.s + nLangCode, line.len - nLangCode);
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
    ReportDebugIf(len(*c) != nStrings * 2);
    if (nUntranslated > 0 && !str::Eq(langCodePref, StrL("en:"))) {
        logf("Untranslated strings: %d for lang '%s'\n", nUntranslated, langCodePref);
    }
}

// don't free the result
Str GetTranslation(Str s) {
    if (gCurrLangIdx == 0) {
        // 0 is english, no translation needed
        return s;
    }
    auto c = gTranslationCache;
    if (!c) {
        // translations failed to load (e.g. corrupted resource data)
        return s;
    }
    int n = len(*c);
    ReportDebugIf(n % 2 != 0);
    n = n / 2;
    int sLen = len(s);
    for (int i = 0; i < n; i++) {
        int idx = i * 2;
        Str s2 = c->At(idx);
        if (s2.len == sLen && str::Eq(s, s2)) {
            Str tr = c->At(idx + 1);
            if (!tr) {
                logf("Didn't find translation for '%s'\n", s);
                return s;
            }
            // special case of "Change Language"
            // if we accidentally change language, we want be able to
            // change it back so add ("Change Language") to translation
            if (str::ContainsI(s, StrL("Change Language")) && !str::ContainsI(tr, StrL("Change Language"))) {
                tr = str::JoinTemp(tr, StrL(" (Change Language)"));
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

Str GetCurrentLangCode() {
    return gCurrLangCode;
}

// when we can't load translations, reset the language to english so that
// GetTranslation() doesn't try to use a gTranslationCache that was never built
static void FallbackToEnglish() {
    gCurrLangIdx = 0;
    gCurrLangCode = GetLangCodeByIdxTemp(0);
}

void SetCurrentLangByCode(Str langCode) {
    if (str::Eq(langCode, gCurrLangCode)) {
        return;
    }

    int idx = SeqStrIndex(gLangCodes, langCode);
    if (idx < 0) {
        logf("SetCurrentLangByCode: unknown lang code: '%s'\n", langCode);
        // set to English
        idx = 0;
    }
    ReportDebugIf(-1 == idx);
    gCurrLangIdx = idx;
    gCurrLangCode = GetLangCodeByIdxTemp(idx);
    if (idx == 0 && !gIsDebugBuild) {
        // perf: in release builds we skip parsing translations for english
        // in debug we want to execute this code to catch errors
        return;
    }
    LoadedDataResource ldr;
    bool lok = LockDataResource(IDR_TRANSLATIONS, &ldr);
    if (!lok) {
        logf("SetCurrentLangByCode: LockDataResource(IDR_TRANSLATIONS) failed\n");
        FallbackToEnglish();
        return;
    }
    lzma::SimpleArchive archive;
    lok = lzma::ParseSimpleArchive(ldr.data, ldr.dataSize, &archive);
    if (!lok) {
        logf("SetCurrentLangByCode: ParseSimpleArchive failed\n");
        FallbackToEnglish();
        return;
    }
    int fileIdx = lzma::GetIdxFromName(&archive, "translations-good.txt");
    if (fileIdx < 0) {
        logf("SetCurrentLangByCode: translations-good.txt not found in archive\n");
        FallbackToEnglish();
        return;
    }
    u8* data = lzma::GetFileDataByIdx(&archive, fileIdx, nullptr);
    if (!data) {
        logf("SetCurrentLangByCode: GetFileDataByIdx failed\n");
        FallbackToEnglish();
        return;
    }
    int dataSize = (int)(archive.files[fileIdx].uncompressedSize);
    Str d = Str((char*)(data), (int)(dataSize));
    ParseTranslationsTxt(d, langCode);
    free(data);
}

Str ValidateLangCode(Str langCode) {
    if (!langCode) return Str();
    int idx = SeqStrIndex(gLangCodes, langCode);
    if (idx < 0) {
        return nullptr;
    }
    return GetLangCodeByIdxTemp(idx);
}

TempStr GetLangCodeByIdxTemp(int idx) {
    return SeqStrByIndex(gLangCodes, idx);
}

TempStr GetLangNameByIdxTemp(int idx) {
    return SeqStrByIndex(gLangNames, idx);
}

bool IsCurrLangRtl() {
    return IsLangRtl(gCurrLangIdx);
}

Str DetectUserLang() {
    const LANGID* langIds = GetLangIds();
    LANGID langId = GetUserDefaultUILanguage();
    // try the exact match
    for (int i = 0; i < gLangsCount; i++) {
        if (langId == langIds[i]) {
            return GetLangCodeByIdxTemp(i);
        }
    }

    // see if we have a translation in a language that has the same
    // primary id as user's language and neutral sublang
    LANGID userLangIdNeutral = MAKELANGID(PRIMARYLANGID(langId), SUBLANG_NEUTRAL);
    for (int i = 0; i < gLangsCount; i++) {
        if (userLangIdNeutral == langIds[i]) {
            return GetLangCodeByIdxTemp(i);
        }
    }

    return StrL("en");
}

void Destroy() {
    FreeTranslations();
}

} // namespace trans

Str _TRA(Str s) {
    return trans::GetTranslation(s);
}

TempWStr _TRW(Str s) {
    return ToWStrTemp(trans::GetTranslation(s));
}
