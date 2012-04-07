/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "Translations.h"
#include "StrUtil.h"

namespace Trans {

/*
This code relies on the following variables that must be defined in
a separate file Translations_txt.cpp which is automatically generated
from translation files by scripts\update_translations.py.

// array of UTF-8 encoded translated strings.
// it has LANGS_COUNT * STRINGS_COUNT elements
// (for simplicity). Translation i for language n is at position
// (n * STRINGS_COUNT) + i
const char *gTranslations[LANGS_COUNT * STRINGS_COUNT];

// array of language codes, names and IDs
// used for determining the human readable name of a language
// and for guessing the OS's current language settings
LangDef gLangData[LANGS_COUNT];
*/

#include "Translations_txt.cpp"

// numeric index of the current language. 0 ... LANGS_COUNT-1
static int gCurrLangIdx = 0;
static const TCHAR **gTranslationCache = NULL;  // cached translations

const char *GuessLanguage()
{
    LANGID langId = GetUserDefaultUILanguage();
    LANGID langIdNoSublang = MAKELANGID(PRIMARYLANGID(langId), SUBLANG_NEUTRAL);
    const char *langCode = NULL;

    // Either find the exact primary/sub lang id match, or a neutral sublang if it exists
    // (don't return any sublang for a given language, it might be too different)
    for (int i = 0; i < LANGS_COUNT; i++) {
        if (langId == gLangData[i].id)
            return gLangData[i].code;

        if (langIdNoSublang == gLangData[i].id)
            langCode = gLangData[i].code;
        // continue searching after finding a match with a neutral sublanguage
    }

    return langCode;
}

inline bool IsValidLangIdx(int idx)
{
    return (idx >= 0) && (idx < LANGS_COUNT);
}

static int GetLangIndexFromCode(const char *code)
{
    for (int i = 0; i < LANGS_COUNT; i++)
        if (str::Eq(code, gLangData[i].code))
            return i;
    return -1;
}

// checks whether the language code is known and returns
// a static pointer to the same code if so (else NULL)
const char *ValidateLanguageCode(const char *code)
{
    int index = GetLangIndexFromCode(code);
    if (IsValidLangIdx(index))
        return gLangData[index].code;
    return NULL;
}

bool SetCurrentLanguage(const char *code)
{
    int newIdx = GetLangIndexFromCode(code);
    if (newIdx != -1)
        gCurrLangIdx = newIdx;
    return newIdx != -1;
}

static int cmpCharPtrs(const void *a, const void *b)
{
    return strcmp(*(char **)a, *(char **)b);
}

static int GetTranslationIndex(const char* txt)
{
    assert(gCurrLangIdx < LANGS_COUNT);
    const char **res = (const char **)bsearch(&txt, &gTranslations, STRINGS_COUNT, sizeof(gTranslations[0]), cmpCharPtrs);
    assert(res);
    if (!res) {
        // didn't find a translation
        return -1;
    }

    return (int)(res - gTranslations);
}

// Call at program exit to free all memory related to translations functionality.
static void __cdecl FreeData()
{
    if (!gTranslationCache)
        return;
    for (size_t i = 0; i < dimof(gTranslations); i++)
        free((void *)gTranslationCache[i]);
    free((void *)gTranslationCache);
    gTranslationCache = NULL;
}

// Return a properly encoded version of a translation for 'txt'.
// Memory for the string needs to be allocated and is cached in gTranslationCache
// array. That way the client doesn't have to worry about the lifetime of the string.
// All allocated strings can be freed with Trans::FreeData(), which should be
// done at program exit so that we're guaranteed no-one is using the data
const TCHAR *GetTranslation(const char *txt)
{
    if (!gTranslationCache) {
        assert(dimof(gTranslations) == STRINGS_COUNT * LANGS_COUNT);
        gTranslationCache = SAZA(const TCHAR *, dimof(gTranslations));
        if (!gTranslationCache)
            return _T("Missing translation!?");
        atexit(FreeData);
    }

    int idx = GetTranslationIndex(txt);
    assert((idx >= 0) && (idx < STRINGS_COUNT));
    if (-1 == idx)
        return _T("Missing translation!?");

    int transIdx = (gCurrLangIdx * STRINGS_COUNT) + idx;
    // fall back to the English string, if a translation is missing
    if (!gTranslations[transIdx])
        transIdx = idx;

    if (!gTranslationCache[transIdx])
        gTranslationCache[transIdx] = str::conv::FromUtf8(gTranslations[transIdx]);
    return gTranslationCache[transIdx];
}

// returns an arbitrary index for a given language code
// which can be used for calling GetLanguageCode and
// GetLanguageName (this index isn't guaranteed to remain
// stable after a restart, use the language code when saving
// the current language settings instead)
int GetLanguageIndex(const char *code)
{
    for (int i = 0; i < LANGS_COUNT; i++) {
        const char *langCode = gLangData[i].code;
        if (str::Eq(code, langCode))
            return i;
    }
    return -1;
}

// caller MUST NOT free the result
const char *GetLanguageCode(int langIdx)
{
    if (!IsValidLangIdx(langIdx))
        return NULL;
    return gLangData[langIdx].code;
}

// caller needs to free() the result
TCHAR *GetLanguageName(int langIdx)
{
    assert(IsValidLangIdx(langIdx));
    if (!IsValidLangIdx(langIdx))
        return NULL;
    return str::conv::FromUtf8(gLangData[langIdx].fullName);
}

bool IsLanguageRtL(int langIdx)
{
    assert(IsValidLangIdx(langIdx));
    if (!IsValidLangIdx(langIdx))
        return false;
    return gLangData[langIdx].isRTL;
}

}
