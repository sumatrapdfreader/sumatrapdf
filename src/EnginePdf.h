/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace EnginePdf {

bool IsSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateFromFile(const WCHAR* fileName, PasswordUI* pwdUI = nullptr);
BaseEngine* CreateFromStream(IStream* stream, PasswordUI* pwdUI = nullptr);

} // namespace EnginePdf
