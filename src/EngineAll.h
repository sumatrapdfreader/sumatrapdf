/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct Annotation;
enum class AnnotationType;
struct PasswordUI;
struct FileArgs;
struct AnnotCreateArgs;

struct XfaFieldHit;
enum class XfaFieldKind;

/* EngineDjVu.cpp */
void CleanupEngineDjVu();
bool IsEngineDjVuSupportedFileType(Kind kind);
EngineBase* CreateEngineDjVuFromFile(const char* path);
EngineBase* CreateEngineDjVuFromStream(IStream* stream);

/* EngineEbook.cpp */
EngineBase* CreateEngineEpubFromFile(const char* fileName);
EngineBase* CreateEngineEpubFromStream(IStream* stream);
EngineBase* CreateEngineFb2FromFile(const char* fileName);
EngineBase* CreateEngineFb2FromStream(IStream* stream);
EngineBase* CreateEngineMobiFromFile(const char* fileName);
EngineBase* CreateEngineMobiFromStream(IStream* stream);
EngineBase* CreateEnginePdbFromFile(const char* fileName);
EngineBase* CreateEngineChmFromFile(const char* fileName);
EngineBase* CreateEngineHtmlFromFile(const char* fileName);
EngineBase* CreateEngineTxtFromFile(const char* fileName);

void SetDefaultEbookFont(const char* name, float size);
void EngineEbookCleanup();

/* EngineImages.cpp */

bool IsEngineImageSupportedFileType(Kind);
EngineBase* CreateEngineImageFromFile(const char* fileName);
EngineBase* CreateEngineImageFromStream(IStream* stream);

bool IsEngineImageDirSupportedFile(const char* fileName, bool sniff = false);
EngineBase* CreateEngineImageDirFromFile(const char* fileName);

bool IsEngineCbxSupportedFileType(Kind kind);
EngineBase* CreateEngineCbxFromFile(const char* path, PasswordUI* pwdUI = nullptr, Kind hintKind = nullptr,
                                    const char* realPath = nullptr);
EngineBase* CreateEngineCbxFromStream(IStream* stream);

bool IsEngineImages(EngineBase*);
void EngineImagesGetImageProperties(EngineBase*, int pageNo, StrVec& keyValOut);

/* EngineMupdf.cpp */

using ShowErrorCb = Func1<const char*>;

bool IsEngineMupdfSupportedFileType(Kind);
EngineBase* CreateEngineMupdfFromFile(const char* path, Kind kind, int displayDPI, PasswordUI* pwdUI = nullptr);
EngineBase* CreateEngineMupdfFromStream(IStream* stream, const char* nameHint, PasswordUI* pwdUI = nullptr);
EngineBase* CreateEngineMupdfFromData(const ByteSlice& data, const char* nameHint, PasswordUI* pwdUI);
ByteSlice LoadEmbeddedPDFFile(const char* path);
const char* ParseEmbeddedStreamNumber(const char* path, int* streamNoOut);
TempStr GetEmbeddedFileNameTemp(const char* path);
Annotation* EngineMupdfCreateAnnotation(EngineBase*, int pageNo, PointF pos, AnnotCreateArgs* args);
void EngineMupdfGetAnnotations(EngineBase*, Vec<Annotation*>&);
bool EngineMupdfHasUnsavedAnnotations(EngineBase*);
bool EngineMupdfSupportsAnnotations(EngineBase*);
bool EngineMupdfIsEncrypted(EngineBase* engine);
const char* EngineMupdfGetPassword(EngineBase* engine);
bool EngineMupdfSaveUpdated(EngineBase* engine, const char* path, const ShowErrorCb& showErrorFunc);
Annotation* EngineMupdfGetAnnotationAtPos(EngineBase*, int pageNo, PointF pos, Annotation*);
Annotation* EngineMupdfGetWidgetAtPos(EngineBase*, int pageNo, PointF pos);
Annotation* EngineMupdfGetAdjacentWidget(EngineBase*, Annotation* cur, bool forward);
bool EngineMupdfIsHybridXfa(EngineBase* engine);
XfaFieldHit EngineMupdfGetXfaFieldAtPos(EngineBase* engine, int pageNo, PointF pos);
XfaFieldHit EngineMupdfGetAdjacentXfaField(EngineBase* engine, const XfaFieldHit& cur, bool forward);
bool EngineMupdfSetXfaFieldContent(EngineBase* engine, const char* fieldName, const char* value);
bool EngineMupdfSelectXfaRadio(EngineBase* engine, int pageNo, const char* fieldName, RectF bounds);
bool EngineMupdfToggleXfaCheckbox(EngineBase* engine, const char* fieldName);
TempStr EngineMupdfGetXfaFieldContentTemp(EngineBase* engine, const char* fieldName);
int EngineMupdfGetXfaFieldChoiceCount(EngineBase* engine, const char* fieldName);
TempStr EngineMupdfGetXfaFieldChoiceOptionTemp(EngineBase* engine, const char* fieldName, int index);
void EngineMupdfMarkXfaPageModified(EngineBase* engine, int pageNo);
// disable mupdf's JavaScript engine for PDFs loaded after this call
void EngineMupdfSetDisableJavaScript(bool disable);
// allow PDFs to load images from an external sibling file (#3731), for PDFs
// loaded after this call; set from gGlobalPrefs->allowExternalImages
void EngineMupdfSetAllowExternalImages(bool allow);
ByteSlice EngineMupdfLoadAttachment(EngineBase*, int attachmentNo);
ByteSlice EngineMupdfLoadAnnotAttachment(EngineBase*, int objNum);
TempStr EngineMupdfGetPdfInfo(const char* path);
TempStr EngineMupdfGetPdfOutline(const char* path);

/* EnginePs.cpp */

bool IsEnginePsAvailable();
bool IsEnginePsSupportedFileType(Kind);
EngineBase* CreateEnginePsFromFile(const char* fileName);

/* EngineCreate.cpp */

bool IsSupportedFileType(Kind kind, bool enableEngineEbooks);

EngineBase* CreateEngineFromFile(const char* filePath, PasswordUI* pwdUI, bool enableChmEngine);

bool EngineSupportsAnnotations(EngineBase*);
bool EngineGetAnnotations(EngineBase*, Vec<Annotation*>&);
bool EngineHasUnsavedAnnotations(EngineBase*);
Annotation* EngineGetAnnotationAtPos(EngineBase*, int pageNo, PointF pos, Annotation*);
Annotation* EngineGetWidgetAtPos(EngineBase*, int pageNo, PointF pos);
bool EngineIsHybridXfa(EngineBase* engine);
XfaFieldHit EngineGetXfaFieldAtPos(EngineBase* engine, int pageNo, PointF pos);
XfaFieldHit EngineGetAdjacentXfaField(EngineBase* engine, const XfaFieldHit& cur, bool forward);
bool EngineSetXfaFieldContent(EngineBase* engine, const char* fieldName, const char* value);
bool EngineSelectXfaRadio(EngineBase* engine, int pageNo, const char* fieldName, RectF bounds);
bool EngineToggleXfaCheckbox(EngineBase* engine, const char* fieldName);
TempStr EngineGetXfaFieldContentTemp(EngineBase* engine, const char* fieldName);
int EngineGetXfaFieldChoiceCount(EngineBase* engine, const char* fieldName);
TempStr EngineGetXfaFieldChoiceOptionTemp(EngineBase* engine, const char* fieldName, int index);
void EngineMarkXfaPageModified(EngineBase* engine, int pageNo);
