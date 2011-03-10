/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Translations_h
#define Translations_h

bool         Translations_SetCurrentLanguage(const char* lang);
const TCHAR* Translations_GetTranslation(const char* txt);

void Translations_FreeData();

#define _TR(x)  Translations_GetTranslation(x)
#define _TRN(x) x

#endif
