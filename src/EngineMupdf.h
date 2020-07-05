/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsMupdfEngineSupportedFileType(Kind);
EngineBase* CreateEngineMupdfFromFile(const WCHAR* path);
EngineBase* CreateEngineMupdfFromStream(IStream* stream);
