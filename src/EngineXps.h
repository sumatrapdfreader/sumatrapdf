/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsEngineXpsSupportedFileType(Kind kind);
bool IsXpsDirectory(const WCHAR* path);
EngineBase* CreateEngineXpsFromFile(const WCHAR* fileName);
EngineBase* CreateEngineXpsFromStream(IStream* stream);
