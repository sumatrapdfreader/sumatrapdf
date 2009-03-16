#ifndef TRANSLATIONS_H__
#define TRANSLATIONS_H__

//bool Translations_FromData(const char* langs, const char* data, size_t data_len);
bool         Translations_SetCurrentLanguage(const char* lang);
const char*  Translations_GetTranslationA(const char* txt);
const WCHAR* Translations_GetTranslationW(const char* txt);

void Translations_FreeData();

#define _TRA(x) Translations_GetTranslationA(x)
#define _TRN(x) x
#define _TRW(x) Translations_GetTranslationW(x)
#define _TRWN(x) x

#ifdef UNICODE
#define Translations_GetTranslation Translations_GetTranslationW
#define _TR(x) Translatations_GetTranslationW(x)
#else
#define Translations_GetTranslation Translations_GetTranslationA
#define _TR(x) Translations_GetTranslationA(x)
#endif

#endif
