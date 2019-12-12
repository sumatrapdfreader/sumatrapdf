/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsEnginePdfSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateEnginePdfFromFile(const WCHAR* fileName, PasswordUI* pwdUI = nullptr);
BaseEngine* CreateEnginePdfFromStream(IStream* stream, PasswordUI* pwdUI = nullptr);
