/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Translations_h
#define Translations_h

namespace Trans {

bool         SetCurrentLanguage(const char* lang);
const TCHAR* GetTranslation(const char* txt);

void FreeData();

}

#define _TR(x)  Trans::GetTranslation(x)
#define _TRN(x) (x)
#if defined(DEBUG) || defined(SVN_PRE_RELEASE_VER)
// use the following macros to mark translatable strings that
// translators should not yet translate, as the strings might
// still change due to the experimental nature of a feature
#define _TB_TR(x)   Trans::GetTranslation(x)
#define _TB_TRN(x)  (x)
#endif

#endif
