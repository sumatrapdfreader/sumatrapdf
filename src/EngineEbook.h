/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

EngineBase* CreateEngineEpubFromFile(const WCHAR* fileName);
EngineBase* CreateEngineEpubFromStream(IStream* stream);
EngineBase* CreateEngineFb2FromFile(const WCHAR* fileName);
EngineBase* CreateEngineFb2FromStream(IStream* stream);
EngineBase* CreateEngineMobiFromFile(const WCHAR* fileName);
EngineBase* CreateEngineMobiFromStream(IStream* stream);
EngineBase* CreateEnginePdbFromFile(const WCHAR* fileName);
EngineBase* CreateEngineChmFromFile(const WCHAR* fileName);
EngineBase* CreateEngineHtmlFromFile(const WCHAR* fileName);
EngineBase* CreateEngineTxtFromFile(const WCHAR* fileName);

void SetDefaultEbookFont(const WCHAR* name, float size);
void EngineEbookCleanup();
