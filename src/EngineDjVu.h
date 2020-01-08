/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void CleanupDjVuEngine();
bool IsDjVuEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreateDjVuEngineFromFile(const WCHAR* fileName);
EngineBase* CreateDjVuEngineFromStream(IStream* stream);
