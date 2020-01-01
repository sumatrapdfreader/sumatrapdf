/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsEpubEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreateEpubEngineFromFile(const WCHAR* fileName);
EngineBase* CreateEpubEngineFromStream(IStream* stream);

bool IsFb2EngineSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreateFb2EngineFromFile(const WCHAR* fileName);
EngineBase* CreateFb2EngineFromStream(IStream* stream);

bool IsMobiEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreateMobiEngineFromFile(const WCHAR* fileName);
EngineBase* CreateMobiEngineFromStream(IStream* stream);

bool IsPdbEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreatePdbEngineFromFile(const WCHAR* fileName);

bool IsChmEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreateChmEngineFromFile(const WCHAR* fileName);

bool IsHtmlEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreateHtmlEngineFromFile(const WCHAR* fileName);

bool IsTxtEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreateTxtEngineFromFile(const WCHAR* fileName);

void SetDefaultEbookFont(const WCHAR* name, float size);
