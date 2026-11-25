/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by PdfToolsCommon.cpp and each app's PdfTools.cpp ---

char* PdfToolPasswordTemp(MainWindow* win, Str srcPath);
TempStr PdfToolInputTemp(EngineBase* engine, Str srcPath, TempStr* tmpPath);
bool ParseDeletePages(Str s, int pageCount, Vec<int>& pagesToDelete);
bool PageRangeOk(Str s, int pageCount, bool isExtract, Vec<int>& pages);
TempStr PageRangeCountTemp(Str s, int pageCount, bool isExtract);
TempStr PageCountTextTemp(int nPages, int pageCount);
Str PageRangeHint();

// the tool dialogs' layout, in dips: the lines the user types in, then the
// files, then the footer, hairlines between them, as the palette draws
constexpr int kToolDlgMinDx = 520;
constexpr int kToolDlgPadDx = 16;
constexpr int kToolDlgLabelDx = 64;
constexpr int kToolDlgGapDx = 10;
constexpr int kToolDlgLineDy = 44;
constexpr int kToolDlgRowDy = 30;
constexpr int kToolDlgFilesPadDy = 8;
constexpr int kToolDlgFootPadDy = 10;
constexpr int kToolDlgComboDx = 84;
// Compress PDF's Pictures drop-down: its labels are longer
constexpr int kCompressQualityDx = 220;
constexpr int kToolDlgQueryFontPx = 15;
constexpr int kToolDlgTextFontPx = 13;
constexpr int kToolDlgMetaFontPx = 12;
// the meta text, lighter still, while what it describes is not usable
constexpr int kToolDlgFaintShift = 25;
TempStr BuildKeepPagesRangeTemp(int pageCount, const Vec<int>& pagesToDelete);
TempStr FormatPageRangeTemp(const Vec<int>& pages);
bool KeepOnlyPagesWithAnnotations(EngineBase* engine, Vec<int>& pages);
TempStr DefaultPdfDestPathTemp(Str srcPath);
bool ExtractPagesToNewPdf(WindowTab* tab, const Vec<int>& pages, Str destPath);
bool ConvertImageCollectionToPdf(EngineBase* engine, Str destPath);
Str ImagesToPdfFilter();
TempStr ImagesToPdfDestTemp(Str firstImage);
TempStr MakePdfFromImagesTemp(const StrVec& paths, Str dest);
TempStr AnnotationsExportDestTemp(Str docPath);
TempStr ExportAnnotationsTemp(EngineBase* engine, Str docPath, Str dest);

// Add Watermark / Add Page Numbers (and `cli watermark`, `cli number`)
struct PdfStamp;
constexpr float kWatermarkOpacity = 0.25f;
constexpr u8 kWatermarkGray = 0x80;
constexpr float kPageNumberSize = 10;
constexpr float kPageNumberMargin = 30;
void PageStamps(const Vec<int>& pages, const PdfStamp& tmpl, Str format, int start, Vec<PdfStamp>& out);
// the Position drop-down of Add Page Numbers, and the stamps the dialogs make
void PageNumberPositionLabels(StrVec& out);
void DialogStamps(bool watermark, Str text, const Vec<int>& pages, int positionIdx, Vec<PdfStamp>& out);
bool StampPdfToFile(EngineBase* engine, const Vec<PdfStamp>& stamps, Str destPath, Str* errOut);
// Compress PDF's Pictures drop-down (and `cli compress --quality`): Same keeps
// the pictures, Balanced and Smallest scale them down and save them as JPEG
enum class PdfCompressQuality {
    Same,
    Balanced,
    Smallest,
};
void PdfCompressQualityLabels(StrVec& out);
bool PdfCompressQualityFromName(Str name, PdfCompressQuality& out);
void AppendPdfCompressArgs(Vec<char*>& argv, PdfCompressQuality quality);
constexpr int kConvertPdfToImagesDpi = 150;
constexpr int kSaveSelectionDefaultDpi = 300;
float SaveSelectionZoom(EngineBase* engine, float dpi);
bool SaveSelectionSizeOk(int w, int h);
bool EstimateSelectionPx(RectF rect, float zoom, int& w, int& h);
// PNG / JPEG / BMP / GIF, same core formats as the Save Image dialog
struct ConvertImageFormat {
    Str label;
    Str ext; // including the dot, e.g. ".png"
};
constexpr int kConvertImageFormatCount = 4;
extern const ConvertImageFormat kConvertImageFormats[kConvertImageFormatCount];
int ConvertImageFormatIdxFromPath(Str path);
bool IsSupportedConvertImageExt(Str path);
bool PathHasPagePlaceholder(Str path);
TempStr EnsurePagePlaceholderTemp(Str path, bool multiPage);
TempStr WithDefaultImageExtTemp(Str path);
int CountExistingImages(Str templatePath, const Vec<int>& pages);
TempStr UniqueImagesTemplateTemp(Str templatePath, const Vec<int>& pages);
TempStr ExistingImagesTextTemp(int nExisting);
bool PagesFitDpi(EngineBase* engine, const Vec<int>& pages, int dpi);
int ConvertPagesToImages(EngineBase* engine, int rotation, Str templatePath, const Vec<int>& pages, int dpi,
                         Str* firstPathOwnedOut, const Func1<int>& onProgress = {});
void CollectAllPages(int pageCount, Vec<int>& pages);
Pixmap* RenderSelectionPixmap(EngineBase* engine, int rotation, int pageNo, RectF rect, float dpi);
bool WriteSelectionPixmap(Pixmap* px, Str destPath);

// implemented by each app
bool SavePixmapAsImageFile(Pixmap* px, Str path);
void ShowPdfPageRangeDialog(MainWindow* win, bool isExtract);

bool ExtractTextViaEngine(WindowTab* tab, Str destPath, Str pages);

struct SaveSelImgWork {
    Pixmap* px = nullptr;
    Str destPath;
    // orig drops its notification by canvas
    HWND hwndCanvas = nullptr;
    MainWindow* win = nullptr;
    bool ok = false;
};
void SaveSelImgThread(SaveSelImgWork* d);
// implemented by each app
void FinishSaveSelImg(SaveSelImgWork* d);

constexpr int kMaxImageDpi = 9600;

static const int kImageDpiChoices[] = {72, 96, 150, 300, 600, 1200};

// Convert EPUB to PDF: the page sizes (points) and font sizes it offers.
// A5 and 11 pt are mupdf's ebook layout defaults
struct EbookPdfPageSize {
    Str label;
    float dx;
    float dy;
};
static const EbookPdfPageSize kEbookPdfPageSizes[] = {
    {StrL("A5"), 420, 595},
    {StrL("A4"), 595, 842},
    {StrL("A6"), 298, 420},
    {StrL("Letter"), 612, 792},
};
static const int kEbookPdfFontSizes[] = {8, 9, 10, 11, 12, 14, 16, 18, 20, 24};
constexpr int kEbookPdfDefaultFontSize = 11;
constexpr int kEbookPdfMaxFontSize = 72;
int EbookPdfFontSizeFromText(Str s);

// rectangular selection → PNG / JPEG / BMP at a chosen DPI, independent of
// the current zoom (issue #6127)
extern Kind kNotifSaveSelectionAsImage;

// entry points of the mupdf command line tools we link in
extern "C" int pdfbake_main(int argc, char** argv);
extern "C" int pdfclean_main(int argc, char** argv);
extern "C" int muconvert_main(int argc, char** argv);
extern "C" void fz_set_optind(int val);

using PdfToolMain = int (*)(int, char**);
int RunPdfToolMain(PdfToolMain toolMain, int argc, char** argv);

// what a dialog does once its tool succeeded
enum class PdfToolDone {
    LoadDest,
    ShowDest,
    // reloads destPath where it's open, else loads it (Merge)
    ReloadDest,
};

// how a dialog reports its tool's failure
enum class PdfToolFail {
    MessageBox,
    Notification,
};

// a mutool command (or, overriding Run(), other work) a tool dialog runs on a thread
struct PdfToolJob {
    virtual ~PdfToolJob() = default;
    virtual int Run();
    // pages done and to do, for a dialog that shows progress
    AtomicInt nDone = 0;
    int nTotal = 0;
    // called instead of FinishPdfToolJob, for an owner that isn't a tool dialog
    Func1<PdfToolJob*> onFinish;
    PdfToolMain toolMain = nullptr;
    StrVec args;
    Str tmpPath;   // owned; a copy of the input, deleted after the run
    Str destPath;  // owned
    Str logName;   // owned
    Str failMsg;   // owned
    Str failTitle; // owned
    PdfToolDone done = PdfToolDone::LoadDest;
    PdfToolFail fail = PdfToolFail::MessageBox;
    int res = -1;
    // the dialog; the app clears it when the dialog goes away first
    void* owner = nullptr;
};
PdfToolJob* NewPdfToolJob(PdfToolMain toolMain, Str logName, Str destPath, Str failMsg, Str failTitle);
void SetPdfToolJobArgs(PdfToolJob* job, char** argv, int argc, Str tmpPath);
void StartPdfToolJob(PdfToolJob* job);
void FreePdfToolJob(PdfToolJob* job);
int RunPdfToolJobNow(PdfToolJob* job);
PdfToolJob* NewStampJob(EngineBase* engine, bool watermark, Str text, const Vec<int>& pages, int posIdx, Str destPath);
PdfToolJob* NewImagesJob(EngineBase* engine, int rotation, Str templatePath, const Vec<int>& pages, int dpi);
PdfToolJob* NewEpubToPdfJob(Str srcPath, Str destPath, const EbookPdfPageSize& ps, int fontSize);
PdfToolJob* NewCajToPdfJob(Str srcPath, Str destPath);
PdfToolJob* NewConvertToPdfJob(EngineBase* engine, Str destPath);
struct PdfMergeSource;
struct PdfMergePage;
PdfToolJob* NewMergeJob(const Vec<PdfMergeSource>& srcs, const Vec<PdfMergePage>& pages, Str srcCopy, Str destPath);
// implemented by each app
void FinishPdfToolJob(PdfToolJob* job);
void PdfToolJobProgress(PdfToolJob* job);
