/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsXpsEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
bool IsXpsDirectory(const WCHAR* path);
EngineBase* CreateXpsEngineFromFile(const WCHAR* fileName);
EngineBase* CreateXpsEngineFromStream(IStream* stream);
