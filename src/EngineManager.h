/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace EngineManager {

bool IsSupportedFile(const WCHAR* filePath, bool sniff = false, bool enableEngineEbooks = true);
EngineBase* CreateEngine(const WCHAR* filePath, PasswordUI* pwdUI = nullptr, bool enableChmEngine = true,
                         bool enableEngineEbooks = true);
} // namespace EngineManager
