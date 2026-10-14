/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by PdfToolsCommon.cpp and each app's PdfTools.cpp ---

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
constexpr int kToolDlgQueryFontPx = 15;
constexpr int kToolDlgTextFontPx = 13;
constexpr int kToolDlgMetaFontPx = 12;
// the meta text, lighter still, while what it describes is not usable
constexpr int kToolDlgFaintShift = 25;
TempStr BuildKeepPagesRangeTemp(int pageCount, const Vec<int>& pagesToDelete);
TempStr FormatPageRangeTemp(const Vec<int>& pages);
bool KeepOnlyPagesWithAnnotations(EngineBase* engine, Vec<int>& pages);
TempStr DefaultPdfDestPathTemp(Str srcPath);
bool ConvertImageCollectionToPdf(EngineBase* engine, Str destPath);
constexpr int kConvertPdfToImagesDpi = 150;
constexpr int kSaveSelectionDefaultDpi = 300;
float SaveSelectionZoom(EngineBase* engine, float dpi);
bool SaveSelectionSizeOk(int w, int h);
bool EstimateSelectionPx(RectF rect, float zoom, int& w, int& h);
int ConvertImageFormatIdxFromPath(Str path);
bool IsSupportedConvertImageExt(Str path);
bool PathHasPagePlaceholder(Str path);
TempStr EnsurePagePlaceholderTemp(Str path, bool multiPage);
TempStr WithDefaultImageExtTemp(Str path);
bool PagesFitDpi(EngineBase* engine, const Vec<int>& pages, int dpi);
int ConvertPagesToImages(EngineBase* engine, int rotation, Str templatePath, const Vec<int>& pages, int dpi,
                         Str* firstPathOwnedOut);
void CollectAllPages(int pageCount, Vec<int>& pages);
Pixmap* RenderSelectionPixmap(EngineBase* engine, int rotation, int pageNo, RectF rect, float dpi);
bool WriteSelectionPixmap(Pixmap* px, Str destPath);

// implemented by each app
bool SavePixmapAsImageFile(Pixmap* px, Str path);
void ShowPdfPageRangeDialog(MainWindow* win, bool isExtract);

bool ExtractTextViaEngine(MainWindow* win, Str destPath, Str pages);

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

// rectangular selection → PNG / JPEG / BMP at a chosen DPI, independent of
// the current zoom (issue #6127)
static Kind kNotifSaveSelectionAsImage = "notifSaveSelectionAsImage";

// entry points of the mupdf command line tools we link in
extern "C" int pdfbake_main(int argc, char** argv);
extern "C" int pdfclean_main(int argc, char** argv);
extern "C" int muconvert_main(int argc, char** argv);
extern "C" void fz_set_optind(int val);
