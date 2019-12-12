/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsDjVuEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateDjVuEngineFromFile(const WCHAR* fileName);
BaseEngine* CreateDjVuEngineFromStream(IStream* stream);
