/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace EngineManager {

bool IsSupportedFile(const WCHAR* filePath, bool sniff = false, bool enableEbookEngines = true);
BaseEngine* CreateEngine(const WCHAR* filePath, PasswordUI* pwdUI = nullptr, bool enableChmEngine = true,
                         bool enableEbookEngines = true);
} // namespace EngineManager
