/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsImageEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateImageEngineFromFile(const WCHAR* fileName);
BaseEngine* CreateImageEngineFromStream(IStream* stream);

bool IsImageDirEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateImageDirEngineFromFile(const WCHAR* fileName);

bool IsCbxEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateCbxEngineFromFile(const WCHAR* fileName);
BaseEngine* CreateCbxEngineFromStream(IStream* stream);
