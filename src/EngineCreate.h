/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsSupportedFileType(Kind kind, bool enableEngineEbooks);

EngineBase* CreateEngine(const WCHAR* filePath, PasswordUI* pwdUI = nullptr, bool enableChmEngine = true,
                         bool enableEngineEbooks = true);
