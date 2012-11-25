/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Translations_h
#define Translations_h

namespace trans {

const char *  GuessLanguage();
const char *  ValidateLanguageCode(const char *code);
bool          SetCurrentLanguage(const char *code);
const WCHAR * GetTranslation(const char *txt);

int           GetLanguageIndex(const char *code);
const char *  GetLanguageCode(int langIdx);
WCHAR *       GetLanguageName(int langIdx);
bool          IsLanguageRtL(int langIdx);

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
