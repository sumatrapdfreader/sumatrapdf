/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsEpubEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateEpubEngineFromFile(const WCHAR* fileName);
BaseEngine* CreateEpubEngineFromStream(IStream* stream);

bool IsFb2EngineSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateFb2EngineFromFile(const WCHAR* fileName);
BaseEngine* CreateFb2EngineFromStream(IStream* stream);

bool IsMobiEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateMobiEngineFromFile(const WCHAR* fileName);
BaseEngine* CreateMobiEngineFromStream(IStream* stream);

bool IsPdbEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreatePdbEngineFromFile(const WCHAR* fileName);

bool IsChmEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateChmEngineFromFile(const WCHAR* fileName);

bool IsHtmlEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateHtmlEngineFromFile(const WCHAR* fileName);

bool IsTxtEngineSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateTxtEngineFromFile(const WCHAR* fileName);

void SetDefaultEbookFont(const WCHAR* name, float size);
