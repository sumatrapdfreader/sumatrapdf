#include "base_util.h"
#include "translations.h"
#include "translations_txt.h"
#include "str_util.h"
#include "utf_util.h"

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

// array of translated strings. 
// it has g_transLangsCount * g_translationsCount elements
// (for simplicity). Translation i for language n is at position
// (n * g_transTranslationsCount) + i
const char **g_transTranslations;
*/

// numeric index of the current language. 0 ... g_transLangsCount-1
static int currLangIdx = 0;

/* 'data'/'data_len' is a text describing all texts we translate.
   It builds data structures need for quick lookup of translations
   as well as a list of available languages.
   It returns a list of available languages.
   The list is not valid after a call to Translations_FreeData.
   The function must be called before any other function in this module.
   It can be called multiple times. This is to make debugging of translations
   easier by allowing re-loading translation file at runtime. 
   */
bool Translations_FromData(const char* langs, const char* data, size_t data_len)
{
    assert(0); // TODO: implement me
    return false;
}

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

const char* Translations_GetTranslationA(const char* txt)
{
    // perf shortcut: don't bother translating if we use default lanuage
    if (0 == currLangIdx)
        return txt;
    for (int i=0; i < g_transTranslationsCount; i++) {
        // TODO: translations are sorted so can use binary search
        const char *tmp =  g_transTranslations[i];
        int cmp_res = strcmp(txt, tmp);
        if (0 == cmp_res) {
            tmp = g_transTranslations[(currLangIdx * g_transTranslationsCount) + i];
            if (NULL == tmp)
                return txt;
            return tmp;
        } else if (cmp_res < 0) {
            assert(0);  // bad - didn't find a translation
            return txt;
        }
    }
    assert(0); // bad - didn't find a translation
    return txt;
}

static WCHAR* lastTxtCached = NULL;

// TODO: this is not thread-safe. lastTxtCached should be per-thread
const WCHAR* Translations_GetTranslationW(const char* txt)
{
    txt = Translations_GetTranslationA(txt);
    if (!txt) return NULL;
    if (lastTxtCached)
        free(lastTxtCached);
    lastTxtCached = utf8_to_utf16(txt);
    return (const WCHAR*)lastTxtCached;
}

void Translations_FreeData()
{
    // TODO: will be more when we implement Translations_FromData
    free(lastTxtCached);
    lastTxtCached = NULL;
}

