/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum class FileType : u8;
struct Annotation;
enum class AnnotationType;
struct PasswordUI;
struct FileArgs;
struct AnnotCreateArgs;
struct PropValue;

/* EngineDjvuDec.cpp: DjVu engine built on ext/djvudec */
bool IsEngineDjVuSupportedFileType(FileType kind);
EngineBase* CreateEngineDjvuDecFromFile(Str path);
EngineBase* CreateEngineDjvuDecFromData(Str data);
extern bool gMemoryMapLargeFiles;

/* EngineEbook.cpp */
EngineBase* CreateEngineEpubFromFile(Str fileName);
EngineBase* CreateEngineEpubFromData(Str data);
EngineBase* CreateEngineFb2FromFile(Str fileName);
EngineBase* CreateEngineFb2FromData(Str data);
EngineBase* CreateEngineMobiFromFile(Str fileName);
EngineBase* CreateEngineMobiFromData(Str data);
EngineBase* CreateEnginePdbFromFile(Str fileName);
EngineBase* CreateEngineChmFromFile(Str fileName);
EngineBase* CreateEngineHtmlFromFile(Str fileName);
EngineBase* CreateEngineTxtFromFile(Str fileName);

void SetDefaultEbookFont(Str name, float size);
void EngineEbookCleanup();

/* EngineImages.cpp */

bool IsEngineImageSupportedFileType(FileType);
EngineBase* CreateEngineImageFromFile(Str fileName);
EngineBase* CreateEngineImageFromData(Str data);

bool IsEngineImageDirSupportedFile(Str fileName, bool sniff = false);
EngineBase* CreateEngineImageDirFromFile(Str fileName);

bool IsEngineCbxSupportedFileType(FileType kind);
EngineBase* CreateEngineCbxFromFile(Str path, PasswordUI* pwdUI = nullptr, FileType hintType = FileType::Unknown,
                                    Str realPath = {});
EngineBase* CreateEngineCbxFromData(Str data);

bool IsEngineImages(EngineBase*);
void EngineImagesGetImageProperties(EngineBase*, int pageNo, Vec<PropValue>& propsOut);

/* EngineMupdf.cpp */

using ShowErrorCb = Func1<Str>;

bool IsEngineMupdfSupportedFileType(FileType);
EngineBase* CreateEngineMupdfFromFile(Str path, FileType kind, int displayDPI, PasswordUI* pwdUI = nullptr);
EngineBase* CreateEngineMupdfFromData(Str data, Str nameHint, PasswordUI* pwdUI);
Str LoadEmbeddedPDFFile(Str path);
TempStr ParseEmbeddedStreamNumber(Str path, int* streamNoOut);
Annotation* EngineMupdfCreateAnnotation(EngineBase*, int pageNo, PointF pos, AnnotCreateArgs* args);
void EngineMupdfGetAnnotations(EngineBase*, Vec<Annotation*>&);
bool EngineMupdfHasUnsavedAnnotations(EngineBase*);
bool EngineMupdfSupportsAnnotations(EngineBase*);
bool EngineMupdfIsEncrypted(EngineBase* engine);
Str EngineMupdfGetPassword(EngineBase* engine);
bool EngineMupdfSaveUpdated(EngineBase* engine, Str path, const ShowErrorCb& showErrorFunc);
Annotation* EngineMupdfGetAnnotationAtPos(EngineBase*, int pageNo, PointF pos, Annotation*);
Annotation* EngineMupdfGetWidgetAtPos(EngineBase*, int pageNo, PointF pos);
Annotation* EngineMupdfGetAdjacentWidget(EngineBase*, Annotation* cur, bool forward);
// disable mupdf's JavaScript engine for PDFs loaded after this call
void EngineMupdfSetDisableJavaScript(bool disable);
// allow PDFs to load images from an external sibling file (#3731), for PDFs
// loaded after this call; set from gGlobalPrefs->allowExternalImages
void EngineMupdfSetAllowExternalImages(bool allow);
// toggle CAD/engineering-drawing line enhancement for this document
void EngineMupdfToggleCadEnhance(EngineBase* engine);
// drop cached dark-mode analyses/images (call when dark-mode options change)
void EngineMupdfInvalidateDarkMode(EngineBase* engine);
// PDF documents support the object-level smart dark renderer
bool EngineSupportsSmartDarkMode(EngineBase* engine);
Str EngineMupdfLoadAttachment(EngineBase*, int attachmentNo);
Str EngineMupdfLoadAnnotAttachment(EngineBase*, int objNum);
TempStr EngineMupdfGetPdfInfo(Str path);
TempStr EngineMupdfGetPdfOutline(Str path);

/* EnginePs.cpp */

bool IsEnginePsAvailable();
bool IsEnginePsSupportedFileType(FileType);
EngineBase* CreateEnginePsFromFile(Str fileName);

/* EngineCreate.cpp */

bool IsSupportedFileType(FileType kind, bool enableEngineEbooks);

EngineBase* CreateEngineFromFile(Str filePath, PasswordUI* pwdUI, bool enableChmEngine);

bool EngineSupportsAnnotations(EngineBase*);
bool EngineGetAnnotations(EngineBase*, Vec<Annotation*>&);
bool EngineHasUnsavedAnnotations(EngineBase*);
Annotation* EngineGetAnnotationAtPos(EngineBase*, int pageNo, PointF pos, Annotation*);
Annotation* EngineGetWidgetAtPos(EngineBase*, int pageNo, PointF pos);
