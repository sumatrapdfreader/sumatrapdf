/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum class FileType : u8;
struct Annotation;
enum class AnnotationType;
struct PasswordUI;
struct FileArgs;
struct AnnotCreateArgs;
struct PropValue;

bool IsEngineDjVuSupportedFileType(FileType kind);
EngineBase* CreateEngineDjvuDecFromFile(Str path);
EngineBase* CreateEngineDjvuDecFromData(Str data);
extern bool gMemoryMapLargeFiles;

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
void SetDefaultChmFont(Str name);
// Reject characters that would break out of a quoted CSS font-family value.
inline bool IsSafeEbookFontName(Str name) {
    if (!name) {
        return false;
    }
    for (int i = 0; i < len(name); i++) {
        unsigned char c = (unsigned char)name.s[i];
        if (c < 32 || c == '"' || c == '\'' || c == ';' || c == '{' || c == '}' || c == '\\') {
            return false;
        }
    }
    return true;
}
// Drop one matching pair of wrapping ' or " quotes, then check IsSafeEbookFontName.
inline Str EbookFontNameFromSetting(Str name) {
    int n = len(name);
    if (n >= 2) {
        char q = name.s[0];
        if ((q == '"' || q == '\'') && name.s[n - 1] == q) {
            name = Str(name.s + 1, n - 2);
        }
    }
    if (!IsSafeEbookFontName(name)) {
        return {};
    }
    return name;
}
void EngineEbookCleanup();

// the ebook font this reflowable document asked for and we couldn't load, so
// the text came out in the default font; null if that didn't happen (#4600)
Str EngineEbookFontUnavailable(EngineBase* engine);

// the user CSS generated from ebook settings (font family + line height), so
// the Ebook Settings dialog can show exactly what the engine will apply
TempStr EbookGeneratedCssTemp(Str fontName, const Vec<float>* margin, float lineSpacing, int displayDPI);

/* EngineImages.cpp */

// how many leading bytes of an image file are enough, in practice, to parse the
// image size out of its header (JPEG SOF can sit past EXIF / ICC segments)
constexpr int kImageSizeFromDataPartialSize = 64 * 1024;

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
bool EngineImagesGetPageFileInfo(EngineBase*, int pageNo, TempStr* nameOut, i64* sizeOut);
// Non-owning raw image bytes for a page (valid until engine is destroyed). Empty if
// not an image-collection engine or the page has no extractable file data.
Str EngineImagesGetImageData(EngineBase*, int pageNo);

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
bool EngineMupdfIsPdf(EngineBase* engine);
bool EngineMupdfIsEncrypted(EngineBase* engine);
Str EngineMupdfGetPassword(EngineBase* engine);
bool EngineMupdfSaveUpdated(EngineBase* engine, Str path, const ShowErrorCb& showErrorFunc);
Annotation* EngineMupdfGetAnnotationAtPos(EngineBase*, int pageNo, PointF pos, Annotation*);
Annotation* EngineMupdfGetWidgetAtPos(EngineBase*, int pageNo, PointF pos);
Annotation* EngineMupdfGetAdjacentWidget(EngineBase*, Annotation* cur, bool forward);
void EngineMupdfSetDisableJavaScript(bool disable);
void EngineMupdfSetEbookLayoutAspect(float dyOverDx);
void EngineMupdfSetAllowExternalImages(bool allow);
void EngineMupdfToggleCadEnhance(EngineBase* engine);
bool EngineMupdfCadEnhanceActive(EngineBase* engine);
void EngineMupdfInvalidateDarkMode(EngineBase* engine);
bool EngineSupportsSmartDarkMode(EngineBase* engine);
Str EngineMupdfLoadAttachment(EngineBase*, int attachmentNo);
Str EngineMupdfLoadAnnotAttachment(EngineBase*, int objNum);
TempStr EngineMupdfGetPdfInfo(Str path);
TempStr EngineMupdfGetPdfOutline(Str path);

bool IsEnginePsAvailable();
bool IsEnginePsSupportedFileType(FileType);
EngineBase* CreateEnginePsFromFile(Str fileName);

bool IsSupportedFileType(FileType kind, bool enableEngineEbooks);

EngineBase* CreateEngineFromFile(Str filePath, PasswordUI* pwdUI, bool enableChmEngine);

bool EngineSupportsAnnotations(EngineBase*);
bool EngineGetAnnotations(EngineBase*, Vec<Annotation*>&);
bool EngineHasUnsavedAnnotations(EngineBase*);
Annotation* EngineGetAnnotationAtPos(EngineBase*, int pageNo, PointF pos, Annotation*);
Annotation* EngineGetWidgetAtPos(EngineBase*, int pageNo, PointF pos);
