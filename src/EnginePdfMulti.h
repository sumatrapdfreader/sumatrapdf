/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

bool IsEnginePdfMultiSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreateEnginePdfMultiFromFile(const WCHAR* fileName, PasswordUI* pwdUI = nullptr);
