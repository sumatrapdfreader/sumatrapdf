/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: BSD */

#ifndef Translations_h
#define Translations_h

typedef struct {
    const char *            code;
    const char *            fullName;
    LANGID                  langId;
    bool                    isRTL;
    const char *            translations;

    uint16_t *              translationsOffsets;
    const WCHAR **          translationsCache;
} LangDef;

namespace trans {

LangDef *     GetCurrentLang();
void          SetCurrentLang(LangDef *lang);

int           GetLangsCount();
LangDef *     GetLang(int i);

LangDef *     GetLangByCode(const char *code);

LangDef *     GuessLang();

const WCHAR * GetTranslation(const char *txt);

void          Destroy();
}

// _TR() marks strings that need to be translated
#define _TR(x)  trans::GetTranslation(x)

// _TRN() marks strings that need to be translated but are used in a context
// that doesn't allow calling Trans::GetTranslation() (e.g. when used as part
// of a struct). This allows the translation manager script to see the string
// but they'll need additional code that does Trans::GetTranslation() on them
#define _TRN(x) (x)

#endif
