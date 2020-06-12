/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsPdfEngineSupportedFileType(Kind);
EngineBase* CreateEnginePdfFromFile(const WCHAR* path, PasswordUI* pwdUI = nullptr);
EngineBase* CreateEnginePdfFromStream(IStream* stream, PasswordUI* pwdUI = nullptr);

bool EnginePdfSaveUpdated(EngineBase*, std::string_view path);
std::string_view LoadEmbeddedPDFFile(const WCHAR* path);
const WCHAR* ParseEmbeddedStreamNumber(const WCHAR* path, int* streamNoOut);
