/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsEngineMupdfSupportedFileType(Kind);
EngineBase* CreateEngineMupdfFromFile(const WCHAR* path, PasswordUI* pwdUI = nullptr);
EngineBase* CreateEngineMupdfFromStream(IStream* stream, PasswordUI* pwdUI = nullptr);

std::span<u8> LoadEmbeddedPDFFile(const WCHAR* path);
const WCHAR* ParseEmbeddedStreamNumber(const WCHAR* path, int* streamNoOut);
Annotation* EngineMupdfCreateAnnotation(EngineBase*, AnnotationType type, int pageNo, PointF pos);
int EngineMupdfGetAnnotations(EngineBase*, Vec<Annotation*>*);
bool EngineMupdfHasUnsavedAnnotations(EngineBase*);
bool EngineMupdfSupportsAnnotations(EngineBase*);
bool EngineMupdfSaveUpdated(EngineBase* engine, std::string_view path,
                            std::function<void(std::string_view)> showErrorFunc);
Annotation* EngineMupdfGetAnnotationAtPos(EngineBase*, int pageNo, PointF pos, AnnotationType* allowedAnnots);
