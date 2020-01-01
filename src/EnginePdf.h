/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsEnginePdfSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreateEnginePdfFromFile(const WCHAR* fileName, PasswordUI* pwdUI = nullptr);
EngineBase* CreateEnginePdfFromStream(IStream* stream, PasswordUI* pwdUI = nullptr);
