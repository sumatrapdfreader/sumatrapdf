/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Translations_h
#define Translations_h

namespace Trans {

bool         SetCurrentLanguage(const char* lang);
const TCHAR* GetTranslation(const char* txt);

void FreeData();

}

// _TR() marks strings that need to be translated
#define _TR(x)  Trans::GetTranslation(x)

// _TRN() marks strings that need to be translated but are used in a context
// that doesn't allow calling Trans::GetTranslation() (e.g. when used as part
// of a struct). This allows the translation manager script to see the string
// but they'll need additional code that does Trans::GetTranslation() on them
#define _TRN(x) (x)

// use the following macros to mark translatable strings that
// translators should not yet translate, as the strings might
// still change due to the experimental nature of a feature
// (make sure not to have any of these in release builds!)
#define _TB_TR(x)   Trans::GetTranslation(x)
#define _TB_TRN(x)  (x)

#endif
