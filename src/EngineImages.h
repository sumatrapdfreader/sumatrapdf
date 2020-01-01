/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsImageEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreateImageEngineFromFile(const WCHAR* fileName);
EngineBase* CreateImageEngineFromStream(IStream* stream);

bool IsImageDirEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreateImageDirEngineFromFile(const WCHAR* fileName);

bool IsCbxEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreateCbxEngineFromFile(const WCHAR* fileName);
EngineBase* CreateCbxEngineFromStream(IStream* stream);
