/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// minimal code for making the installer translatable
// TODO: replace with ..\Translations.cpp?

void SelectTranslation();
const WCHAR *Translate(const WCHAR *s);
bool IsLanguageRtL();

#define _TR(x) Translate(TEXT(x))
