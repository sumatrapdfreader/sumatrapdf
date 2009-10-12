#ifndef TRANSLATIONS_H__
#define TRANSLATIONS_H__

bool         Translations_SetCurrentLanguage(const char* lang);
const TCHAR* Translations_GetTranslation(const char* txt);

void Translations_FreeData();

#define _TR(x)  Translations_GetTranslation(x)
#define _TRN(x) x

#endif
