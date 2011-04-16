/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "translations.h"
#include "translations_txt.h"
#include "StrUtil.h"

namespace Trans {

/*
This code relies on the following variables that must be defined in a 
separate file (translations_txt.h and translations_txt.cpp).
The idea is that those files are automatically generated 
by a script from translations file.

// array of language names so that g_transLangs[i] is a name of
// language. i is in 0 .. LANGS_COUNT-1
const char *g_transLangs[LANGS_COUNT];

// array of UTF-8 encoded translated strings. 
// it has LANGS_COUNT * STRINGS_COUNT elements
// (for simplicity). Translation i for language n is at position
// (n * STRINGS_COUNT) + i
const char *g_transTranslations[LANGS_COUNT * STRINGS_COUNT];
*/

static int FreeData();

// numeric index of the current language. 0 ... LANGS_COUNT-1
static int g_currLangIdx = 0;
static const TCHAR **g_translations = NULL;  // cached translations

bool SetCurrentLanguage(const char *lang)
{
    for (size_t i = 0; i < dimof(g_transLangs); i++) {
        if (Str::Eq(lang, g_transLangs[i])) {
            g_currLangIdx = i;
            return true;
        }
    }
    return false;
}

static int cmpCharPtrs(const void *a, const void *b)
{
    return strcmp(*(char **)a, *(char **)b);
}

static int GetTranslationIndex(const char* txt)
{
    assert(g_currLangIdx < LANGS_COUNT);
    const char **res = (const char **)bsearch(&txt, &g_transTranslations, STRINGS_COUNT, sizeof(g_transTranslations[0]), cmpCharPtrs);
    assert(res);
    if (!res) {
        // bad - didn't find a translation
        return -1;
    }

    return (int)(res - g_transTranslations);
}

// Return a properly encoded version of a translation for 'txt'.
// Memory for the string needs to be allocated and is cached in g_translations
// array. That way the client doesn't have to worry about the lifetime of the string.
// All allocated strings can be freed with Trans::FreeData(), which should be
// done at program exit so that we're guaranteed no-one is using the data
const TCHAR *GetTranslation(const char *txt)
{
    if (!g_translations) {
        assert(dimof(g_transTranslations) == STRINGS_COUNT * LANGS_COUNT);
        g_translations = SAZA(const TCHAR *, dimof(g_transTranslations));
        if (!g_translations)
            return _T("Missing translation!?");
        _onexit(FreeData);
    }

    int idx = GetTranslationIndex(txt);
    assert(0 <= idx && idx < STRINGS_COUNT);
    if (-1 == idx)
        return _T("Missing translation!?");

    int transIdx = (g_currLangIdx * STRINGS_COUNT) + idx;
    // fall back to the English string, if a translation is missing
    if (!g_transTranslations[transIdx])
        transIdx = idx;

    if (!g_translations[transIdx])
        g_translations[transIdx] = Str::Conv::FromUtf8(g_transTranslations[transIdx]);
    return g_translations[transIdx];
}

// Call at program exit to free all memory related to translations functionality.
static int FreeData()
{
    if (!g_translations)
        return 0;
    for (size_t i = 0; i < dimof(g_transTranslations); i++)
        free((void *)g_translations[i]);
    free((void *)g_translations);
    g_translations = NULL;
    return 0;
}

}
