/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsXpsEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateXpsEngineFromFile(const WCHAR* fileName);
BaseEngine* CreateXpsEngineFromStream(IStream* stream);
