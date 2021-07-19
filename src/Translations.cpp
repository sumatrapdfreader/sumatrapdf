/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/Log.h"
#include "Translations.h"

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
    // english string from translations.txt file
    // we lazily match it to origStr
    char* str{nullptr};
    // translation of str/origStr in gCurrLangCode
    char* trans{nullptr};
};

// used locally, gCurrLangCode points into gLangCodes
static const char* gCurrLangCode = nullptr;
static int gCurrLangIdx{0};
static int gTranslationsCount{0};
static Translation* gTranslations{nullptr};
static int gUntranslatedCount{0};

static char* UnescapeString(char* s) {
    str::Str str;
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
    return str.StealData();
}

static int FindChar(char* s, int sLen, char c) {
    int i = 0;
    char c2;
    while (i < sLen) {
        c2 = *s++;
        if (c == c2) {
            return i;
        }
        i++;
    }
    return sLen;
}

static void FreeTranslations() {
    if (!gTranslations) {
        return;
    }
    for (int i = 0; i < gTranslationsCount; i++) {
        str::FreePtr(&gTranslations[i].str);
        str::FreePtr(&gTranslations[i].trans);
    }
    free(gTranslations);
    gTranslations = nullptr;
    gTranslationsCount = 0;
    gUntranslatedCount = 0;
}

static void ParseTranslationsTxt(std::string_view sv, const char* langCode) {
    langCode = str::JoinTemp(langCode, ":");
    int nLangCode = str::Len(langCode);

    // parse into lines
    char* s = (char*)sv.data();
    int sLen = (int)sv.size();
    Vec<char*> lines;
    int n;
    int nStrings = 0;
    while (sLen > 0) {
        n = FindChar(s, sLen, '\n');
        if (n < 2) {
            break;
        }
        s[n] = 0;
        lines.Append(s);
        if (s[0] == ':') {
            nStrings++;
        }
        n++; // skip '\n'
        s += n;
        sLen -= n;
        CrashIf(sLen < 0);
    }
    int nLines = lines.isize();
    logf("%d lines, nStrings: %d\n", nLines, nStrings);

    FreeTranslations();
    gTranslationsCount = nStrings;
    gTranslations = AllocArray<Translation>(gTranslationsCount);
    gUntranslatedCount = 0;

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
            gUntranslatedCount++;
        }
        Translation& translation = gTranslations[nTrans++];
        translation.str = UnescapeString(orig);
        translation.trans = nullptr;
        if (trans) {
            translation.trans = UnescapeString(trans);
        }
    }
    CrashIf(nTrans != gTranslationsCount);
    if (gUntranslatedCount > 0) {
        logf("Untranslated strings: %d for lang '%s'\n", gUntranslatedCount, langCode);
    }
}

const char* GetTranslationATemp(const char* s) {
    CrashIf(!s);
    if (!gTranslations) {
        return s;
    }
    for (int i = 0; i < gTranslationsCount; i++) {
        Translation& trans = gTranslations[i];
        if (str::Eq(s, trans.str)) {
            if (!trans.trans) {
                return s;
            }
            return (const char*)trans.trans;
        }
    }
    logf("Didn't find translation for '%s'\n", s);
    // CrashIf(true);
    return s;
}

const WCHAR* GetTranslationTemp(const char* s) {
    auto trans = GetTranslationATemp(s);
    return ToWstrTemp(trans);
}

int GetLangsCount() {
    return gLangsCount;
}

const char* GetCurrentLangCode() {
    return gCurrLangCode;
}

static void BuildTranslationsForLang(int langIdx, const char* langCode) {
    if (langIdx == 0) {
        // if english, do nothing
        FreeTranslations();
        return;
    }
    std::span<u8> d = LoadDataResource(2);
    if (d.empty()) {
        return;
    }
    std::string_view sv{(const char*)d.data(), d.size()};
    ParseTranslationsTxt(sv, langCode);
    free(d.data());
}

void SetCurrentLangByCode(const char* langCode) {
    if (str::Eq(langCode, gCurrLangCode)) {
        return;
    }

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
    return trans::GetTranslationTemp(s);
}

const char* _TRA(const char* s) {
    return trans::GetTranslationATemp(s);
}
