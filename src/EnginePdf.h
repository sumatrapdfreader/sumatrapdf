/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsPdfEngineSupportedFileType(Kind);
EngineBase* CreateEnginePdfFromFile(const WCHAR* path, PasswordUI* pwdUI = nullptr);
EngineBase* CreateEnginePdfFromStream(IStream* stream, PasswordUI* pwdUI = nullptr);

std::span<u8> LoadEmbeddedPDFFile(const WCHAR* path);
const WCHAR* ParseEmbeddedStreamNumber(const WCHAR* path, int* streamNoOut);
Annotation* EnginePdfCreateAnnotation(EngineBase* engine, AnnotationType type, int pageNo, PointF pos);
int EnginePdfGetAnnotations(EngineBase*, Vec<Annotation*>*);
bool EnginePdfHasUnsavedAnnotations(EngineBase* engine);
bool EnginePdfSaveUpdated(EngineBase* engine, std::string_view path);
Annotation* EnginePdfGetAnnotationAtPos(EngineBase* engine, int pageNo, PointF pos, AnnotationType* allowedAnnots);
