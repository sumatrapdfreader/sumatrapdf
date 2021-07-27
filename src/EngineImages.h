/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsEngineImageSupportedFileType(Kind);
EngineBase* CreateEngineImageFromFile(const WCHAR* fileName);
EngineBase* CreateEngineImageFromStream(IStream* stream);

bool IsEngineImageDirSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreateEngineImageDirFromFile(const WCHAR* fileName);

bool IsEngineCbxSupportedFileType(Kind kind);
EngineBase* CreateEngineCbxFromFile(const WCHAR* path);
EngineBase* CreateEngineCbxFromStream(IStream* stream);
