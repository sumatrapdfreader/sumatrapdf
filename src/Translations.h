/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace trans {

int GetLangsCount();

const char* GetCurrentLangCode();

void SetCurrentLangByCode(const char* langCode);
const char* ValidateLangCode(const char* langCode);

const WCHAR* GetTranslation(const char* s);
const char* GetLangCodeByIdx(int idx);
const char* GetLangNameByIdx(int idx);
bool IsCurrLangRtl();
const char* DetectUserLang();
void Destroy();

} // namespace trans

// _TR() marks strings that need to be translated
const WCHAR* _TR(const char* s);
#define _TR_TODO(quote) L##quote
#define _TR_TODON(quote) quote

// _TRN() marks strings that need to be translated but are used in a context
// that doesn't allow calling Trans::GetTranslation() (e.g. when used as part
// of a struct). This allows the translation manager script to see the string
// but they'll need additional code that does Trans::GetTranslation() on them
#define _TRN(x) (x)
