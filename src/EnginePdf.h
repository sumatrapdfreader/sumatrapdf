/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsPdfEngineSupportedFileType(Kind);
EngineBase* CreateEnginePdfFromFile(const WCHAR* path, PasswordUI* pwdUI = nullptr);
EngineBase* CreateEnginePdfFromStream(IStream* stream, PasswordUI* pwdUI = nullptr);

bool EnginePdfSaveUpdated(EngineBase* engine, std::string_view path);
std::span<u8> LoadEmbeddedPDFFile(const WCHAR* path);
const WCHAR* ParseEmbeddedStreamNumber(const WCHAR* path, int* streamNoOut);
Annotation* EnginePdfCreateAnnotation(EngineBase* engine, AnnotationType type, int pageNo, PointD pos);
