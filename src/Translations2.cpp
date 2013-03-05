/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "Translations2.h"

/*
TODO:
 - compress the translations and de-compress them on demand
 - speed up GetEnglishStringIndex() with binary search
 - verify that MissingTranslations still work
*/

// Note: this code is optimized for (small) size, not speed
// Let's keep it this way

namespace trans {

// defined in Trans*_txt.cpp
extern int              gLangsCount;
extern int              gStringsCount;
extern const char *     gLangNames;
extern const char *     gLangCodes;
const LANGID *          GetLangIds();
bool                    IsLangRtl(int langIdx);
const char *            GetTranslationsForLang(int langIdx);
const char **           GetOriginalStrings();

static const char *     gCurrLangCode = NULL;
static int              gCurrLangIdx = 0;

const char **           gCurrLangStrings = NULL;
WCHAR ***               gLangsTransCache = NULL;

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

static WCHAR **GetTransCacheForLang(int langIdx)
{
    if (!gLangsTransCache[langIdx])
        gLangsTransCache[langIdx] = AllocArray<WCHAR *>(gStringsCount);
    return gLangsTransCache[langIdx];
}

static void FreeTransCache()
{
    for (int langIdx = 0; langIdx < gLangsCount; langIdx++) {
        WCHAR **transCache = gLangsTransCache[langIdx];
        for (int i = 0; transCache && i < gStringsCount; i++) {
            free(transCache[i]);
        }
        free(transCache);
    }
    free(gLangsTransCache);
    gLangsTransCache = NULL;
    // also free gCurrLangStrings here so that an accidental call to
    // SetCurrentLangByCode after FreeTransCache doesn't crash
    free(gCurrLangStrings);
    gCurrLangStrings = NULL;
}

static void BuildStringsIndexForLang(int langIdx)
{
    if (0 == gCurrLangIdx) {
        const char **origStrings = GetOriginalStrings();
        for (int idx = 0; idx < gStringsCount; idx++) {
            gCurrLangStrings[idx] = origStrings[idx];
            CrashIf(!gCurrLangStrings[idx]);
        }
        return;
    }

    const char *s = GetTranslationsForLang(langIdx);
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
    if (!gCurrLangStrings) {
        gCurrLangStrings = AllocArray<const char *>(gStringsCount);
        gLangsTransCache = AllocArray<WCHAR **>(gLangsCount);
    }

    if (str::Eq(langCode, gCurrLangCode))
        return;

    int idx = seqstrings::GetStrIdx(gLangCodes, langCode, gLangsCount);
    CrashIf(-1 == idx);
    gCurrLangIdx = idx;
    gCurrLangCode = langCode;
    BuildStringsIndexForLang(gCurrLangIdx);
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

bool IsCurrLangRtl()
{
    return IsLangRtl(gCurrLangIdx);
}

const char *DetectUserLang()
{
    const LANGID *langIds = GetLangIds();
    LANGID langId = GetUserDefaultUILanguage();
    // try the exact match
    for (int i = 0; i < gLangsCount; i++) {
        if (langId == langIds[i])
            return GetLangCodeByIdx(i);
    }

    // see if we have a translation in a language that has the same
    // primary id as user's language and neutral sublang
    LANGID userLangIdNeutral = MAKELANGID(PRIMARYLANGID(langId), SUBLANG_NEUTRAL);
    for (int i = 0; i < gLangsCount; i++) {
        if (userLangIdNeutral == langIds[i])
            return GetLangCodeByIdx(i);
    }

    return "en";
}

static int GetEnglishStringIndex(const char* txt)
{
    const char **origStrings = GetOriginalStrings();
    for (int idx = 0; idx < gStringsCount; idx++) {
        const char *s = origStrings[idx];
        if (str::Eq(s, txt))
            return idx;
    }
    return -1;
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

    WCHAR **transCache = GetTransCacheForLang(gCurrLangIdx);
    if (!transCache[idx])
        transCache[idx] = str::conv::FromUtf8(trans);
    return transCache[idx];
}

void Destroy()
{
    FreeTransCache();
    FreeMissingTranslations();
}

} // namespace trans
