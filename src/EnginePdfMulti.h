/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

bool IsEnginePdfMultiSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateEnginePdfMultiFromFile(const WCHAR* fileName, PasswordUI* pwdUI = nullptr);
