/* Copyright 2013 Krzysztof Kowalczyk.
   License: BSD */

#include "BaseUtil.h"
#include "Translations2.h"

/*
TODO:
 - special-case english strings. Store them as before in const char * gEnglishStrings[STRINGS_COUNT] = { ... }
   array. That way the compiler should be able to de-duplicate those strings with strings that are in the
   source code inside _TR() macor
 - compress the translations and de-compress them on demand
*/

// Note: this code is optimized for (small) size, not speed
// Let's keep it this way

// from http://msdn.microsoft.com/en-us/library/windows/desktop/dd318693(v=vs.85).aspx
// those definition are not present in 7.0A SDK my VS 2010 uses
#ifndef LANG_CENTRAL_KURDISH
#define LANG_CENTRAL_KURDISH 0x92
#endif

#ifndef SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ
#define SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ 0x01
#endif

namespace trans {

#include "Translations_txt.cpp"

static const char *  gCurrLangCode = NULL;
static int           gCurrLangIdx = 0;

static const char *  gCurrLangStrings[STRINGS_COUNT] = { 0 };
static const WCHAR * gCurrLangTransCache[STRINGS_COUNT] = { 0 };

/* In general, after adding new _TR() strings, one has to re-generate Translations_txt.cpp, but
that also requires uploading new strings to the server, for which one needs accesss.

To support adding new strings without re-generating Translatiosn_txt.cpp, we have a concept
of missing translations. */

struct MissingTranslation {
    const char *s;
    const WCHAR *translation;
};

// number of missing translations should be small
static MissingTranslation  gMissingTranslations[64];
static int                 gMissingTranslationsCount = 0;

static void FreeMissingTranslations()
{
    for (int i=0; i < gMissingTranslationsCount; i++) {
        free((void*)gMissingTranslations[i].translation);
    }
    gMissingTranslationsCount = 0;
}

static const WCHAR *FindOrAddMissingTranslation(const char *s)
{
    for (int i = 0; i < gMissingTranslationsCount; i++) {
        if (s == gMissingTranslations[i].s) {
            return gMissingTranslations[i].translation;
        }
    }
    if (gMissingTranslationsCount >= dimof(gMissingTranslations))
        return L"missing translation";

    gMissingTranslations[gMissingTranslationsCount].s = s;
    const WCHAR *res = str::conv::FromUtf8(s);
    gMissingTranslations[gMissingTranslationsCount].translation = res;
    gMissingTranslationsCount++;
    return res;
}

int GetLangsCount()
{
    return LANGS_COUNT;
}

const char *GetCurrentLangCode()
{
    return gCurrLangCode;
}

static void FreeTransCache()
{
    for (int i = 0; i < STRINGS_COUNT; i++) {
        free((void*)gCurrLangTransCache[i]);
        gCurrLangTransCache[i] = 0;
    }
}

static void BuildStringsIndex(const char *s)
{
    for (int i = 0; i < STRINGS_COUNT; i++) {
        size_t len = str::Len(s);
        if (0 == len)
            gCurrLangStrings[i] = NULL;
        else
            gCurrLangStrings[i] = s;
        s = s + len + 1;
    }
}

void SetCurrentLangByCode(const char *langCode)
{
    if (str::Eq(langCode, gCurrLangCode))
        return;

    int idx = seqstrings::GetStrIdx(gLangCodes, langCode, LANGS_COUNT);
    CrashIf(-1 == idx);
    gCurrLangIdx = idx;
    gCurrLangCode = langCode;
    FreeTransCache();
    BuildStringsIndex(gTranslations[gCurrLangIdx]);
}

const char *ValidateLangCode(const char *langCode)
{
    int idx = seqstrings::GetStrIdx(gLangCodes, langCode, LANGS_COUNT);
    if (-1 == idx)
        return NULL;
    return GetLangCodeByIdx(idx);
}

const char *GetLangCodeByIdx(int idx)
{
    return seqstrings::GetByIdx(gLangCodes, idx);
}

const char *GetLangNameByIdx(int idx)
{
    return seqstrings::GetByIdx(gLangNames, idx);
}

bool IsLangRtlByCode(const char *langCode)
{
    int idx = seqstrings::GetStrIdx(gLangCodes, langCode, LANGS_COUNT);
    for (int i = 0; i < RTL_LANGS_COUNT; i++) {
        if (gRtlLangs[i] == idx)
            return true;
    }
    return false;
}

const char *DetectUserLang()
{
    LANGID langId = GetUserDefaultUILanguage();
    // try the exact match
    for (int i = 0; i < LANGS_COUNT; i++) {
        LANGID tmp = gLangIds[i];
        if (tmp == langId)
            return GetLangCodeByIdx(i);
    }

    LANGID primaryLangId = PRIMARYLANGID(langId);
    // the first match with the same primary lang id
    for (int i = 0; i < LANGS_COUNT; i++) {
        LANGID tmp = PRIMARYLANGID(gLangIds[i]);
        if (tmp == primaryLangId)
            return GetLangCodeByIdx(i);
    }
    return "en";
}

static int GetEnglishStringIndex(const char* txt)
{
    return seqstrings::GetStrIdx(gTranslations_en, txt, STRINGS_COUNT);
}

const WCHAR *GetTranslation(const char *s)
{
    if (NULL == gCurrLangCode)
        SetCurrentLangByCode("en");

    int idx = GetEnglishStringIndex(s);
    if (-1 == idx)
        return FindOrAddMissingTranslation(s);

    const char *trans = gCurrLangStrings[idx];
    // fall back to English if the language doesn't have a translations for this string
    if (!trans)
        trans = s;

    if (!gCurrLangTransCache[idx])
        gCurrLangTransCache[idx] = str::conv::FromUtf8(trans);
    return gCurrLangTransCache[idx];
}

void Destroy()
{
    FreeTransCache();
}

} // namespace trans

