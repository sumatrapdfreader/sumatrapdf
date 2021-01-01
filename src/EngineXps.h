/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsXpsEngineSupportedFileType(Kind kind);
bool IsXpsDirectory(const WCHAR* path);
EngineBase* CreateXpsEngineFromFile(const WCHAR* fileName);
EngineBase* CreateXpsEngineFromStream(IStream* stream);
