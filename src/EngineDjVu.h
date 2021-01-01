/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void CleanupDjVuEngine();
bool IsDjVuEngineSupportedFileType(Kind kind);
EngineBase* CreateDjVuEngineFromFile(const WCHAR* path);
EngineBase* CreateDjVuEngineFromStream(IStream* stream);
