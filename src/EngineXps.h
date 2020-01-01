/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsXpsEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreateXpsEngineFromFile(const WCHAR* fileName);
EngineBase* CreateXpsEngineFromStream(IStream* stream);
