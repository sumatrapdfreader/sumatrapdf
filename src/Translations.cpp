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
    // index points inside allStrings;
    u16 strIdx{0};
    // translation of str/origStr in gCurrLangCode
    // index points inside allTranslations
    u16 transIdx{0};
};

struct TranslationCache {
    // english string from translations.txt file
    // we lazily match it to origStr
    str::Str allStrings;
    str::Str allTranslations;
    // TODO: maybe also cache str::WStr allTranslationsW
    // we currently return a temp converted string but there's a chance their
    // lifetime could survive temp allocator lifetime
    Translation* translations{nullptr};
    int nTranslations{0};
    int nUntranslated{0};
};

// used locally, gCurrLangCode points into gLangCodes
static const char* gCurrLangCode = nullptr;
static int gCurrLangIdx{0};
static TranslationCache* gTranslationCache{nullptr};

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
    if (!gTranslationCache) {
        return;
    }
    delete gTranslationCache;
    gTranslationCache = nullptr;
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
    gTranslationCache = new TranslationCache();
    // make index of first string to be 1 so that 0 can be
    // "missing" value in transIdx
    gTranslationCache->allTranslations.AppendChar(' ');
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
        size_t idx = c->allStrings.size();
        // when this fires, we'll have to bump strIdx form u16 to u32
        CrashIf(idx > 64 * 1024);
        translation.strIdx = (u16)idx;
        UnescapeStringIntoStr(orig, c->allStrings);
        if (trans) {
            idx = c->allTranslations.size();
            CrashIf(idx > 64 * 1024);
            translation.transIdx = (u16)idx;
            UnescapeStringIntoStr(trans, c->allTranslations);
        }
    }
    CrashIf(nTrans != c->nTranslations);
    if (c->nUntranslated > 0) {
        logf("Untranslated strings: %d for lang '%s'\n", c->nUntranslated, langCode);
    }
}

const char* GetTranslationATemp(const char* s) {
    CrashIf(!s);
    if (!gTranslationCache) {
        return s;
    }
    auto c = gTranslationCache;
    for (int i = 0; i < c->nTranslations; i++) {
        Translation& trans = c->translations[i];
        size_t idx = trans.strIdx;
        const char* s2 = c->allStrings.LendData() + idx;
        if (str::Eq(s, s2)) {
            if (trans.transIdx == 0) {
                return s;
            }
            idx = trans.transIdx;
            s2 = c->allTranslations.LendData() + idx;
            return s2;
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
