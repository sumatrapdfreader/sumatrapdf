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

namespace trans {

// defined in Trans*_txt.cpp
extern int              gLangsCount;
extern int              gStringsCount;
extern const char *     gLangNames;
extern const char *     gLangCodes;
extern const LANGID *   gLangIds;
extern int              gRtlLangsCount;
extern const char **    gTranslations;
extern const char *     gTranslations_en;
extern const char **    gCurrLangStrings;
extern const WCHAR **   gCurrLangTransCache;
const int *             GetRtlLangs();
const LANGID *          GetLangIds();

static const char *     gCurrLangCode = NULL;
static int              gCurrLangIdx = 0;

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
    return gLangsCount;
}

const char *GetCurrentLangCode()
{
    return gCurrLangCode;
}

static void FreeTransCache()
{
    for (int i = 0; i < gStringsCount; i++) {
        free((void*)gCurrLangTransCache[i]);
        gCurrLangTransCache[i] = 0;
    }
}

static void BuildStringsIndex(const char *s)
{
    for (int i = 0; i < gStringsCount; i++) {
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

    int idx = seqstrings::GetStrIdx(gLangCodes, langCode, gLangsCount);
    CrashIf(-1 == idx);
    gCurrLangIdx = idx;
    gCurrLangCode = langCode;
    FreeTransCache();
    BuildStringsIndex(gTranslations[gCurrLangIdx]);
}

const char *ValidateLangCode(const char *langCode)
{
    int idx = seqstrings::GetStrIdx(gLangCodes, langCode, gLangsCount);
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
    const int *rtlLangs = GetRtlLangs();
    int idx = seqstrings::GetStrIdx(gLangCodes, langCode, gLangsCount);
    for (int i = 0; i < gRtlLangsCount; i++) {
        if (rtlLangs[i] == idx)
            return true;
    }
    return false;
}

const char *DetectUserLang()
{
    const LANGID *langIds = GetLangIds();
    LANGID langId = GetUserDefaultUILanguage();
    // try the exact match
    for (int i = 0; i < gLangsCount; i++) {
        LANGID tmp = langIds[i];
        if (tmp == langId)
            return GetLangCodeByIdx(i);
    }

    LANGID primaryLangId = PRIMARYLANGID(langId);
    // the first match with the same primary lang id
    for (int i = 0; i < gLangsCount; i++) {
        LANGID tmp = PRIMARYLANGID(langIds[i]);
        if (tmp == primaryLangId)
            return GetLangCodeByIdx(i);
    }
    return "en";
}

static int GetEnglishStringIndex(const char* txt)
{
    return seqstrings::GetStrIdx(gTranslations_en, txt, gStringsCount);
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

