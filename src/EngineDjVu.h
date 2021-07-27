/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void CleanupEngineDjVu();
bool IsEngineDjVuSupportedFileType(Kind kind);
EngineBase* CreateEngineDjVuFromFile(const WCHAR* path);
EngineBase* CreateEngineDjVuFromStream(IStream* stream);
