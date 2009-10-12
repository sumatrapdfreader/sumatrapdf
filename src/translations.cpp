#include "base_util.h"
#include "translations.h"
#include "translations_txt.h"
#include "tstr_util.h"

/*
This code relies on the following variables that must be defined in a 
separate file (translations_txt.h and translations_txt.c).
The idea is that those files are automatically generated 
by a script from translations file.

// number of languages we support
int g_transLangsCount;

// array of language names so that g_transLangs[i] is a name of
// language. i is 0..g_transLangsCount-1
const char **g_transLangs;

// total number of translated strings
int g_transTranslationsCount;

// array of UTF-8 encoded translated strings. 
// it has g_transLangsCount * g_translationsCount elements
// (for simplicity). Translation i for language n is at position
// (n * g_transTranslationsCount) + i
const char **g_transTranslations;
*/

// numeric index of the current language. 0 ... g_transLangsCount-1
static int currLangIdx = 0;
static TCHAR **g_translations = NULL;  // cached translations

bool Translations_SetCurrentLanguage(const char* lang)
{
    for (int i=0; i < g_transLangsCount; i++) {
        if (str_eq(lang, g_transLangs[i])) {
            currLangIdx = i;
            return true;
        }
    }
    return false;
}

static int cmpCharPtrs(const void *a, const void *b)
{
    return strcmp(*(char **)a, *(char **)b);
}

static const char* Translations_GetTranslationAndIndex(const char* txt, int& idx)
{
    assert(currLangIdx < g_transLangsCount);
    const char **res = (const char **)bsearch(&txt, &g_transTranslations, g_transTranslationsCount, sizeof(g_transTranslations[0]), cmpCharPtrs);
    assert(res);
    if (!res) {
        // bad - didn't find a translation
        idx = -1;
        return txt;
    }

    idx = res - g_transTranslations;
    const char *translation = g_transTranslations[(currLangIdx * g_transTranslationsCount) + idx];
    return translation ? translation : txt;
}

// Return a properly encoded version of a translation for 'txt'.
// Memory for the string needs to be allocated and is cached in g_translations
// array. That way the client doesn't have to worry about the lifetime of the string.
// All allocated strings can be freed with Translations_FreeData(), which should be
// done at program exit so that we're guaranteed no-one is using the data
const TCHAR* Translations_GetTranslation(const char* txt)
{
    if (!g_translations) {
        g_translations = (TCHAR**)zmalloc(sizeof(TCHAR*) * g_transTranslationsCount * g_transLangsCount);
        if (!g_translations)
            return NULL;
    }
    int idx;
    txt = Translations_GetTranslationAndIndex(txt, idx);
    if (!txt || (-1 == idx)) return NULL;
    int transIdx = (currLangIdx * g_transTranslationsCount) + idx;
    TCHAR *trans = g_translations[transIdx];
    if (!trans)
        trans = g_translations[transIdx] = utf8_to_tstr(txt);
    return (const TCHAR*)trans;
}

// Call at program exit to free all memory related to traslations functionality.
void Translations_FreeData()
{
    if (!g_translations)
        return;
    for (int i=0; i < (g_transTranslationsCount * g_transLangsCount); i++) {
        free(g_translations[i]);
    }
    free(g_translations);
    g_translations = NULL;
}

