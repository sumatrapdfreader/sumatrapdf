/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

EngineBase* CreateEpubEngineFromFile(const WCHAR* fileName);
EngineBase* CreateEpubEngineFromStream(IStream* stream);
EngineBase* CreateFb2EngineFromFile(const WCHAR* fileName);
EngineBase* CreateFb2EngineFromStream(IStream* stream);
EngineBase* CreateMobiEngineFromFile(const WCHAR* fileName);
EngineBase* CreateMobiEngineFromStream(IStream* stream);
EngineBase* CreatePdbEngineFromFile(const WCHAR* fileName);
EngineBase* CreateChmEngineFromFile(const WCHAR* fileName);
EngineBase* CreateHtmlEngineFromFile(const WCHAR* fileName);
EngineBase* CreateTxtEngineFromFile(const WCHAR* fileName);

void SetDefaultEbookFont(const WCHAR* name, float size);
void EngineEbookCleanup();
