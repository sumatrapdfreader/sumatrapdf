/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Dict.h"
#include "base/LzmaSimpleArchive.h"
#include "EmbeddedResources.h"

#include "SumatraConfig.h"

#include "Translations.h"

namespace trans {

extern int FindLangIdx(Str code);
#if OS_WIN
extern LANGID GetLangIdByIdx(int idx);
#endif
extern bool IsLangRtl(int langIdx);

// A view into the shared language table.
static Str gCurrLangCode;
static int gCurrLangIdx = 0;
// for each translation: english string followed by a translation
static StrVec* gTranslationCache = nullptr;
// english string => its index in gTranslationCache
static dict::StrToInt* gTranslationIdx = nullptr;

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
        if (!c2) {
            *dst++ = c;
            break;
        }
        src++;
        switch (c2) {
            case '\\':
            case '"':
            case '\'':
                *dst++ = c2;
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
            case 'x': {
                // "\xC2\xB7" is the UTF-8 of U+00B7, as in the Tr() literal
                int hi = str::HexDigitVal(src[0]);
                int lo = hi < 0 ? -1 : str::HexDigitVal(src[1]);
                if (lo < 0) {
                    *dst++ = c;
                    *dst++ = c2;
                    break;
                }
                *dst++ = (char)(hi * 16 + lo);
                src += 2;
                break;
            }
            default:
                *dst++ = c;
                *dst++ = c2;
                break;
        }
    }
    *dst = 0;
    s.len = (int)(dst - s.s);
    return s;
}

static void FreeTranslations() {
    delete gTranslationCache;
    gTranslationCache = nullptr;
    delete gTranslationIdx;
    gTranslationIdx = nullptr;
}

static void ParseTranslationsTxt(Str d, Str langCode) {
    TempStr langCodePref = str::JoinTemp(langCode, StrL(":"));

    StrVec lines;
    Split(&lines, d, StrL("\n"), true);
    int nStrings = 0;
    for (Str l : lines) {
        if (l && l.s[0] == ':') {
            nStrings++;
        }
    }
    int nLines = len(lines);
    logf("ParseTranslationsTxt: %d lines, nStrings: %d\n", nLines, nStrings);

    FreeTranslations();
    gTranslationCache = new StrVec();
    gTranslationIdx = new dict::StrToInt(nStrings * 2);
    auto* c = gTranslationCache;
    int nUntranslated = 0;

    Str orig;
    Str trans;
    int i = 2; // skip first 2 header lines
    while (i < nLines) {
        Str origLine = lines[i];
        ReportDebugIf(len(origLine) == 0 || origLine.s[0] != ':');
        orig = Str(origLine.s + 1, origLine.len - 1);
        i++;
        trans = {};
        while (i < nLines && lines[i] && lines[i].s[0] != ':') {
            if (len(trans) == 0) {
                Str line = lines[i];
                if (str::TrimPrefix(line, langCodePref)) {
                    trans = line;
                }
            }
            i++;
        }
        if (len(trans) == 0) {
            nUntranslated++;
        }
        TempStr unescaped = UnescapeTemp(orig);
        // keeps the first of duplicates, like the linear scan did
        gTranslationIdx->Insert(unescaped, len(*c));
        c->Append(unescaped);
        if (len(trans) == 0) {
            c->Append({});
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
    auto* c = gTranslationCache;
    if (!c) {
        // translations failed to load (e.g. corrupted resource data)
        return s;
    }
    ReportDebugIf(len(*c) % 2 != 0);
    int idx = -1;
    if (!gTranslationIdx || !gTranslationIdx->Get(s, &idx)) {
        ReportDebugIf(true);
        return s;
    }
    Str tr = c->At(idx + 1);
    if (len(tr) == 0) {
        logf("Didn't find translation for '%s'\n", s);
        return s;
    }
    // special case of "Change Language"
    // if we accidentally change language, we want be able to
    // change it back so add ("Change Language") to translation
    if (str::ContainsI(s, StrL("Change Language")) && !str::ContainsI(tr, StrL("Change Language"))) {
        // "Sprache wechseln..." => "Sprache wechseln (Change Language)..."
        // Cache in stable storage (not TempStr) so callers can hold the Str.
        static Str gChangeLangSrc[2];
        static Str gChangeLangDst[2];
        Str dots = StrL("...");
        int slot = str::EndsWith(tr, dots) ? 1 : 0;
        if (!str::Eq(gChangeLangSrc[slot], tr)) {
            TempStr full;
            if (slot == 1) {
                Str base = Str(tr.s, len(tr) - len(dots));
                full = str::JoinTemp(base, StrL(" (Change Language)"), dots);
            } else {
                full = str::JoinTemp(tr, StrL(" (Change Language)"));
            }
            str::ReplaceWithCopy(&gChangeLangSrc[slot], tr);
            str::ReplaceWithCopy(&gChangeLangDst[slot], full);
        }
        tr = gChangeLangDst[slot];
    }
    return tr;
}

Str GetCurrentLangCode() {
    return gCurrLangCode;
}

// when we can't load translations, reset the language to english so that
// GetTranslation() doesn't try to use a gTranslationCache that was never built
static void FallbackToEnglish() {
    gCurrLangIdx = 0;
    gCurrLangCode = GetLangCodeByIdx(0);
}

void SetCurrentLangByCode(Str langCode) {
    if (str::Eq(langCode, gCurrLangCode)) {
        return;
    }

    int idx = FindLangIdx(langCode);
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
    int dataSize = 0;
    u8* data = GetEmbeddedFileData(StrL("translations.txt"), &dataSize);
    if (!data) {
        logf("SetCurrentLangByCode: translations.txt not found in IDR_EMBEDDED_PAK\n");
        FallbackToEnglish();
        return;
    }
    logf("SetCurrentLangByCode: translations.txt uncompressed=%d\n", dataSize);
    // empty file is expected when TRANS_UPLOAD_SECRET was missing at build time
    if (dataSize <= 0) {
        logf("SetCurrentLangByCode: translations.txt is empty (no translations available)\n");
        free(data);
        FallbackToEnglish();
        return;
    }
    Str d = Str((char*)data, dataSize);
    // whitespace-only / header-only with no strings: treat as empty
    bool hasString = false;
    for (int i = 0; i < d.len; i++) {
        if (d.s[i] == ':' && (i == 0 || d.s[i - 1] == '\n')) {
            hasString = true;
            break;
        }
    }
    if (!hasString) {
        logf("SetCurrentLangByCode: translations.txt has no strings (no translations available)\n");
        free(data);
        FallbackToEnglish();
        return;
    }
    ParseTranslationsTxt(d, gCurrLangCode);
    free(data);
}

Str ValidateLangCode(Str langCode) {
    if (len(langCode) == 0) return {};
    int idx = FindLangIdx(langCode);
    if (idx < 0) {
        return {};
    }
    return GetLangCodeByIdx(idx);
}

bool IsCurrLangRtl() {
    return IsLangRtl(gCurrLangIdx);
}

// BCP 47 / ISO 639 tags whose apptranslator code differs; most codes match
// the tag already ("de", "fr"). Longest tags first within a language.
static const char* gLocaleAliases =
    "zh-hans\0cn\0zh-cn\0cn\0zh-sg\0cn\0zh\0cn\0"
    "zh-hant\0tw\0zh-tw\0tw\0zh-hk\0tw\0zh-mo\0tw\0"
    "ko\0kr\0cs\0cz\0da\0dk\0pt-br\0br\0be\0by\0vi\0vn\0ms\0my\0hy\0am\0nb\0no\0"
    "sr-cyrl\0sr-rs\0sr-latn\0sp-rs\0sr\0sr-rs\0ca-valencia\0ca-xv\0fy\0fy-nl\0";

static int FindLangIdxByTag(Str tag) {
    int i = SeqStrIndex(gLocaleAliases, tag);
    if (i >= 0 && i % 2 == 0) {
        return FindLangIdx(SeqStrByIndex(gLocaleAliases, i + 1));
    }
    return FindLangIdx(tag);
}

// "zh-Hans-CN", "de_DE.UTF-8" or "pt_BR" to our language code, empty if we
// have no translation: the tag loses its last part until something matches
Str LangCodeFromLocale(Str locale) {
    int n = 0;
    while (n < len(locale) && locale.s[n] != '.' && locale.s[n] != '@') {
        n++;
    }
    TempStr tag = str::DupTemp(Str(locale.s, n));
    str::ToLowerInPlace(tag);
    str::TransCharsInPlace(tag, StrL("_"), StrL("-"));
    while (len(tag) > 0) {
        int idx = FindLangIdxByTag(tag);
        if (idx >= 0) {
            return GetLangCodeByIdx(idx);
        }
        int dash = str::LastIndexOfChar(tag, '-');
        if (dash <= 0) {
            break;
        }
        tag = Str(tag.s, dash);
    }
    return {};
}

#if !OS_WIN
// ng: the win32 half below maps a LANGID to our language code. Here the OS
// (macOS: the preferred languages list; a GUI app launched from Finder has no
// $LANG) or $LC_ALL / $LC_MESSAGES / $LANG name the language.
Str DetectUserLang() {
    StrVec locales;
    ::GetSystemPreferredLocales(locales);
    const char* envNames[] = {"LC_ALL", "LC_MESSAGES", "LANG"};
    for (const char* name : envNames) {
        const char* v = getenv(name);
        if (v && *v) {
            locales.Append(Str((char*)v));
        }
    }
    for (Str locale : locales) {
        Str code = LangCodeFromLocale(locale);
        if (len(code) > 0) {
            return code;
        }
    }
    return StrL("en");
}
#else
Str DetectUserLang() {
    LANGID langId = GetUserDefaultUILanguage();
    // try the exact match
    for (int i = 0; i < GetLangsCount(); i++) {
        if (langId == GetLangIdByIdx(i)) {
            return GetLangCodeByIdx(i);
        }
    }

    // see if we have a translation in a language that has the same
    // primary id as user's language and neutral sublang
    LANGID userLangIdNeutral = MAKELANGID(PRIMARYLANGID(langId), SUBLANG_NEUTRAL);
    for (int i = 0; i < GetLangsCount(); i++) {
        if (userLangIdNeutral == GetLangIdByIdx(i)) {
            return GetLangCodeByIdx(i);
        }
    }

    return StrL("en");
}
#endif

void Destroy() {
    FreeTranslations();
}

} // namespace trans

#if IS_DEBUG
// translations.txt keeps C escapes; unescaped they must equal the compiled Tr() literal
bool Translations_UnitTestLocale() {
    struct {
        Str locale;
        Str want;
    } tests[] = {
        {StrL("zh_CN.UTF-8"), StrL("cn")},
        {StrL("zh-Hans-CN"), StrL("cn")},
        {StrL("zh-Hant-TW"), StrL("tw")},
        {StrL("zh"), StrL("cn")},
        {StrL("ko_KR"), StrL("kr")},
        {StrL("pt_BR"), StrL("br")},
        {StrL("pt_PT"), StrL("pt")},
        {StrL("de_DE.UTF-8@euro"), StrL("de")},
        {StrL("en-US"), StrL("en")},
        {StrL("sr-Latn-RS"), StrL("sp-rs")},
        {StrL("ca-ES-valencia"), StrL("ca")},
        {StrL("C"), {}},
        {StrL("xx-YY"), {}},
        {StrL(""), {}},
    };
    for (auto& t : tests) {
        Str got = trans::LangCodeFromLocale(t.locale);
        if (!str::Eq(got, t.want)) {
            logf("Translations_UnitTestLocale: '%s' => '%s', want '%s'\n", t.locale, got, t.want);
            return false;
        }
    }
    return true;
}

bool Translations_UnitTestUnescape() {
    struct {
        Str escaped;
        Str want;
    } tests[] = {
        {StrL("Font \\\"%s\\\" not found"), StrL("Font \"%s\" not found")},
        {StrL("Paused \\xC2\\xB7 %s"), StrL("Paused \xC2\xB7 %s")},
        {StrL("a\\nb\\tc\\\\d\\re"), StrL("a\nb\tc\\d\re")},
        {StrL("it\\'s"), StrL("it's")},
    };
    for (auto& t : tests) {
        TempStr got = trans::UnescapeTemp(t.escaped);
        // GetTranslation() matches on len first
        if (len(got) != len(t.want) || !str::Eq(got, t.want)) {
            logf("Translations_UnitTestUnescape: '%s' => '%s', want '%s'\n", t.escaped, got, t.want);
            return false;
        }
    }
    return true;
}

bool Translations_UnitTestLookup() {
    using namespace trans;
    StrVec* savedCache = gTranslationCache;
    dict::StrToInt* savedIdx = gTranslationIdx;
    int savedLangIdx = gCurrLangIdx;
    gTranslationCache = nullptr;
    gTranslationIdx = nullptr;

    Str d = StrL(
        "header\nheader\n:Open\nde:Offnen\n:Close\nfr:Fermer\n:Change Language\nde:Sprache\n:Change "
        "Language...\nde:Sprache...\n:Open\nde:Dup\n");
    ParseTranslationsTxt(d, StrL("de"));
    gCurrLangIdx = FindLangIdx(StrL("de"));

    bool ok = str::Eq(GetTranslation(StrL("Open")), StrL("Offnen"));
    ok = ok && str::Eq(GetTranslation(StrL("Close")), StrL("Close"));
    ok = ok && str::Eq(GetTranslation(StrL("Change Language")), StrL("Sprache (Change Language)"));
    ok = ok && str::Eq(GetTranslation(StrL("Change Language...")), StrL("Sprache (Change Language)..."));
    // a substring of a key must not match
    ok = ok && str::Eq(GetTranslation(Str("Openx", 4)), StrL("Offnen"));

    FreeTranslations();
    gTranslationCache = savedCache;
    gTranslationIdx = savedIdx;
    gCurrLangIdx = savedLangIdx;
    return ok;
}
#endif

Str Tr(Str s) {
    return trans::GetTranslation(s);
}
