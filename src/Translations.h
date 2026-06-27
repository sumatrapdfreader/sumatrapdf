/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace trans {

int GetLangsCount();

Str GetCurrentLangCode();

void SetCurrentLangByCode(Str langCode);
Str ValidateLangCode(Str langCode);

Str GetTranslation(Str s);
Str GetLangCodeByIdx(int idx);
Str GetLangNameByIdx(int idx);
bool IsCurrLangRtl();
Str DetectUserLang();
void Destroy();

} // namespace trans

Str _TRA(Str s);
TempWStr _TRW(Str s);

// _TRN() marks strings that need to be translated but are used in a context
// that doesn't allow calling Trans::GetTranslationTemp() (e.g. when used as part
// of a struct). This allows the translation manager script to see the string
// but they'll need additional code that does Trans::GetTranslationTemp() on them
#define _TRN(x) (x)
