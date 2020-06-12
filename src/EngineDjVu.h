/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void CleanupDjVuEngine();
bool IsDjVuEngineSupportedFileType(Kind kind);
EngineBase* CreateDjVuEngineFromFile(const WCHAR* fileName);
EngineBase* CreateDjVuEngineFromStream(IStream* stream);
