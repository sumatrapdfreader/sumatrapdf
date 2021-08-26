/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct Annotation;
enum class AnnotationType;
struct PasswordUI;

/* EngineDjVu.cpp */
void CleanupEngineDjVu();
bool IsEngineDjVuSupportedFileType(Kind kind);
EngineBase* CreateEngineDjVuFromFile(const WCHAR* path);
EngineBase* CreateEngineDjVuFromStream(IStream* stream);

/* EngineEbook.cpp */
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

/* EngineImages.cpp */

bool IsEngineImageSupportedFileType(Kind);
EngineBase* CreateEngineImageFromFile(const WCHAR* fileName);
EngineBase* CreateEngineImageFromStream(IStream* stream);

bool IsEngineImageDirSupportedFile(const WCHAR* fileName, bool sniff = false);
EngineBase* CreateEngineImageDirFromFile(const WCHAR* fileName);

bool IsEngineCbxSupportedFileType(Kind kind);
EngineBase* CreateEngineCbxFromFile(const WCHAR* path);
EngineBase* CreateEngineCbxFromStream(IStream* stream);

/* EngineMulti.cpp */

bool IsEngineMultiSupportedFileType(Kind);
EngineBase* CreateEngineMultiFromFiles(std::string_view dir, VecStr& files);
EngineBase* CreateEngineMultiFromDirectory(const WCHAR* dirW);
TocItem* CreateWrapperItem(EngineBase* engine);

/* EngineMupdf.cpp */

bool IsEngineMupdfSupportedFileType(Kind);
EngineBase* CreateEngineMupdfFromFile(const WCHAR* path, int displayDPI, PasswordUI* pwdUI = nullptr);
EngineBase* CreateEngineMupdfFromStream(IStream* stream, const char* nameHint, PasswordUI* pwdUI = nullptr);

ByteSlice LoadEmbeddedPDFFile(const WCHAR* path);
const WCHAR* ParseEmbeddedStreamNumber(const WCHAR* path, int* streamNoOut);
Annotation* EngineMupdfCreateAnnotation(EngineBase*, AnnotationType type, int pageNo, PointF pos);
int EngineMupdfGetAnnotations(EngineBase*, Vec<Annotation*>*);
bool EngineMupdfHasUnsavedAnnotations(EngineBase*);
bool EngineMupdfSupportsAnnotations(EngineBase*);
bool EngineMupdfSaveUpdated(EngineBase* engine, std::string_view path,
                            std::function<void(std::string_view)> showErrorFunc);
Annotation* EngineMupdfGetAnnotationAtPos(EngineBase*, int pageNo, PointF pos, AnnotationType* allowedAnnots);

/* EnginePs.cpp */

bool IsEnginePsAvailable();
bool IsEnginePsSupportedFileType(Kind);
EngineBase* CreateEnginePsFromFile(const WCHAR* fileName);

/* EngineCreate.cpp */

bool IsSupportedFileType(Kind kind, bool enableEngineEbooks);

EngineBase* CreateEngine(const WCHAR* filePath, PasswordUI* pwdUI, bool enableChmEngine);

bool EngineSupportsAnnotations(EngineBase*);
bool EngineGetAnnotations(EngineBase*, Vec<Annotation*>*);
bool EngineHasUnsavedAnnotations(EngineBase*);
Annotation* EngineGetAnnotationAtPos(EngineBase*, int pageNo, PointF pos, AnnotationType* allowedAnnots);
