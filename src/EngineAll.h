/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct Annotation;
enum class AnnotationType;
struct PasswordUI;
struct FileArgs;
struct AnnotCreateArgs;
struct IStream;
struct PropValue;

/* EngineDjvuDec.cpp: DjVu engine built on ext/djvudec */
bool IsEngineDjVuSupportedFileType(Kind kind);
EngineBase* CreateEngineDjvuDecFromFile(Str path);
EngineBase* CreateEngineDjvuDecFromStream(IStream* stream);

/* EngineEbook.cpp */
EngineBase* CreateEngineEpubFromFile(Str fileName);
EngineBase* CreateEngineEpubFromStream(IStream* stream);
EngineBase* CreateEngineFb2FromFile(Str fileName);
EngineBase* CreateEngineFb2FromStream(IStream* stream);
EngineBase* CreateEngineMobiFromFile(Str fileName);
EngineBase* CreateEngineMobiFromStream(IStream* stream);
EngineBase* CreateEnginePdbFromFile(Str fileName);
EngineBase* CreateEngineChmFromFile(Str fileName);
EngineBase* CreateEngineHtmlFromFile(Str fileName);
EngineBase* CreateEngineTxtFromFile(Str fileName);

void SetDefaultEbookFont(Str name, float size);
void EngineEbookCleanup();

/* EngineImages.cpp */

bool IsEngineImageSupportedFileType(Kind);
EngineBase* CreateEngineImageFromFile(Str fileName);
EngineBase* CreateEngineImageFromStream(IStream* stream);

bool IsEngineImageDirSupportedFile(Str fileName, bool sniff = false);
EngineBase* CreateEngineImageDirFromFile(Str fileName);

bool IsEngineCbxSupportedFileType(Kind kind);
EngineBase* CreateEngineCbxFromFile(Str path, PasswordUI* pwdUI = nullptr, Kind hintKind = nullptr, Str realPath = {});
EngineBase* CreateEngineCbxFromStream(IStream* stream);

bool IsEngineImages(EngineBase*);
void EngineImagesGetImageProperties(EngineBase*, int pageNo, Vec<PropValue>& propsOut);

/* EngineMupdf.cpp */

using ShowErrorCb = Func1<Str>;

bool IsEngineMupdfSupportedFileType(Kind);
EngineBase* CreateEngineMupdfFromFile(Str path, Kind kind, int displayDPI, PasswordUI* pwdUI = nullptr);
EngineBase* CreateEngineMupdfFromStream(IStream* stream, Str nameHint, PasswordUI* pwdUI = nullptr);
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
Str EngineMupdfLoadAttachment(EngineBase*, int attachmentNo);
Str EngineMupdfLoadAnnotAttachment(EngineBase*, int objNum);
TempStr EngineMupdfGetPdfInfo(Str path);
TempStr EngineMupdfGetPdfOutline(Str path);

/* EnginePs.cpp */

bool IsEnginePsAvailable();
bool IsEnginePsSupportedFileType(Kind);
EngineBase* CreateEnginePsFromFile(Str fileName);

/* EngineCreate.cpp */

bool IsSupportedFileType(Kind kind, bool enableEngineEbooks);

EngineBase* CreateEngineFromFile(Str filePath, PasswordUI* pwdUI, bool enableChmEngine);

bool EngineSupportsAnnotations(EngineBase*);
bool EngineGetAnnotations(EngineBase*, Vec<Annotation*>&);
bool EngineHasUnsavedAnnotations(EngineBase*);
Annotation* EngineGetAnnotationAtPos(EngineBase*, int pageNo, PointF pos, Annotation*);
Annotation* EngineGetWidgetAtPos(EngineBase*, int pageNo, PointF pos);
