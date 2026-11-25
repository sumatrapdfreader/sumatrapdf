/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/UITask.h"
#include "gui/Dpi.h"
#include "base/File.h"
#include "gui/UIModels.h"
#include "gui/PlatformFont.h"
#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "Annotation.h"
#include "PdfCreator.h"
#include "AnnotationsExport.h"
#include "ImageReader.h"
#include "PngOptimizer.h"
#include "base/Pixmap.h"
#include "SumatraPDF.h"
#include "SumatraConfig.h"
#include "MainWindow.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "Translations.h"
#include "Flags.h"
#include "DisplayModel.h"
#include "Selection.h"
#include "Theme.h"
#include "Notifications.h"
#include "Commands.h"
#include "PdfTools.h"
#include "PdfToolsCommon.h"

Kind kNotifSaveSelectionAsImage = "notifSaveSelectionAsImage";

// a usable page range: Delete Pages cannot take every page
// the value of -p for a tool re-opening srcPath, the current tab's file
char* PdfToolPasswordTemp(MainWindow* win, Str srcPath) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    return CStrTemp(EngineMupdfToolPasswordTemp(tab ? tab->GetEngine() : nullptr, srcPath));
}

// The file a tool that re-opens the PDF from disk reads: srcPath, or a temp copy
// with the unsaved annotations (*tmpPath, deleted by the caller). Empty if saving failed.
TempStr PdfToolInputTemp(EngineBase* engine, Str srcPath, TempStr* tmpPath) {
    *tmpPath = {};
    if (!engine || !EngineHasUnsavedAnnotations(engine)) {
        return str::DupTemp(srcPath);
    }
    TempStr path = GetTempFilePathTemp(StrL("pdftool"));
    if (len(path) == 0 || !EngineMupdfSaveCopy(engine, path)) {
        file::Delete(path);
        return {};
    }
    *tmpPath = path;
    return path;
}

bool PageRangeOk(Str s, int pageCount, bool isExtract, Vec<int>& pages) {
    if (!ParseDeletePages(s, pageCount, pages)) {
        return false;
    }
    return isExtract || len(pages) < pageCount;
}

// "4 of 7 pages" for the pages a range picks; "of 7 pages" while it is not usable
TempStr PageRangeCountTemp(Str s, int pageCount, bool isExtract) {
    Vec<int> pages;
    if (!PageRangeOk(s, pageCount, isExtract, pages)) {
        return fmt(Tr("of %d pages").s, pageCount);
    }
    return PageCountTextTemp(len(pages), pageCount);
}

TempStr PageCountTextTemp(int nPages, int pageCount) {
    return fmt(Tr("%d of %d pages").s, nPages, pageCount);
}

Str PageRangeHint() {
    return StrL("Ranges: 2, 5-7, 13-");
}

// Parse delete page ranges like "1,3-8,13-N" where N means last page.
// Returns a sorted list of unique 1-based page numbers to delete.
// Returns false if the syntax is invalid or any page is out of range.
bool ParseDeletePages(Str s, int pageCount, Vec<int>& pagesToDelete) {
    if (len(s) == 0) {
        return false;
    }
    StrVec parts;
    Split(&parts, s, StrL(","), true);
    if (len(parts) == 0) {
        return false;
    }
    for (int pi = 0; pi < len(parts); pi++) {
        Str part = parts[pi];
        str::TrimWSInPlace(part, str::TrimOpt::Both);
        if (len(part) == 0) {
            return false;
        }
        // check for range "A-B" where A/B can be a number or "N"
        Str startStr, endStr;
        if (str::CutChar(part, '-', &startStr, &endStr)) {
            str::TrimWSInPlace(startStr, str::TrimOpt::Both);
            str::TrimWSInPlace(endStr, str::TrimOpt::Both);
            if (len(startStr) == 0) {
                return false;
            }
            // "8-" means "8-N" (from page 8 to the last page)
            bool endIsEmpty = len(endStr) == 0;
            int start, end;
            if (str::EqI(startStr, StrL("N"))) {
                start = pageCount;
            } else {
                start = !str::IsNull(str::Parse(startStr, "%d%$", &start)) ? start : -1;
            }
            if (endIsEmpty || str::EqI(endStr, StrL("N"))) {
                end = pageCount;
            } else {
                end = !str::IsNull(str::Parse(endStr, "%d%$", &end)) ? end : -1;
            }
            if (start < 1 || start > pageCount || end < 1 || end > pageCount || start > end) {
                return false;
            }
            for (int i = start; i <= end; i++) {
                VecAppend(pagesToDelete, i);
            }
        } else {
            // single page
            int page;
            if (str::EqI(part, StrL("N"))) {
                page = pageCount;
            } else {
                page = !str::IsNull(str::Parse(part, "%d%$", &page)) ? page : -1;
            }
            if (page < 1 || page > pageCount) {
                return false;
            }
            VecAppend(pagesToDelete, page);
        }
    }
    if (len(pagesToDelete) == 0) {
        return false;
    }
    // sort and deduplicate
    VecSort(pagesToDelete, [](const int* a, const int* b) -> int { return *a - *b; });
    int prev = -1;
    Vec<int> unique;
    for (int p : pagesToDelete) {
        if (p != prev) {
            VecAppend(unique, p);
            prev = p;
        }
    }
    pagesToDelete = unique;
    return true;
}

// Build the page range string of pages to KEEP (complement of pagesToDelete).
TempStr BuildKeepPagesRangeTemp(int pageCount, const Vec<int>& pagesToDelete) {
    str::Builder s;
    int delIdx = 0;
    int rangeStart = -1;
    int rangeEnd = -1;
    for (int p = 1; p <= pageCount; p++) {
        bool shouldDelete = (delIdx < len(pagesToDelete) && pagesToDelete[delIdx] == p);
        if (shouldDelete) {
            delIdx++;
            if (rangeStart != -1) {
                if (len(s) > 0) {
                    s.AppendChar(',');
                }
                if (rangeStart == rangeEnd) {
                    s.Append(fmt("%d", rangeStart));
                } else {
                    s.Append(fmt("%d-%d", rangeStart, rangeEnd));
                }
                rangeStart = -1;
            }
        } else {
            if (rangeStart == -1) {
                rangeStart = p;
            }
            rangeEnd = p;
        }
    }
    if (rangeStart != -1) {
        if (len(s) > 0) {
            s.AppendChar(',');
        }
        if (rangeStart == rangeEnd) {
            s.Append(fmt("%d", rangeStart));
        } else {
            s.Append(fmt("%d-%d", rangeStart, rangeEnd));
        }
    }
    return ToStrTemp(s);
}

// Format a sorted list of page numbers as a compact range string (e.g. "1-3,5,7-10").
TempStr FormatPageRangeTemp(const Vec<int>& pages) {
    str::Builder s;
    int i = 0;
    int n = len(pages);
    while (i < n) {
        int start = pages[i];
        int end = start;
        while (i + 1 < n && pages[i + 1] == end + 1) {
            end = pages[++i];
        }
        if (len(s) > 0) {
            s.AppendChar(',');
        }
        if (start == end) {
            s.Append(fmt("%d", start));
        } else {
            s.Append(fmt("%d-%d", start, end));
        }
        i++;
    }
    return ToStrTemp(s);
}

bool KeepOnlyPagesWithAnnotations(EngineBase* engine, Vec<int>& pages) {
    Vec<Annotation*> annotations;
    if (!EngineGetAnnotations(engine, annotations)) {
        return false;
    }

    Vec<int> annotationPages;
    VecReserve(annotationPages, len(annotations));
    for (Annotation* annotation : annotations) {
        VecAppend(annotationPages, PageNo(annotation));
    }
    std::sort(annotationPages.begin(), annotationPages.end());

    Vec<int> filteredPages;
    for (int pageNo : pages) {
        if (std::binary_search(annotationPages.begin(), annotationPages.end(), pageNo)) {
            VecAppend(filteredPages, pageNo);
        }
    }
    pages = filteredPages;
    return true;
}

void ShowPdfDeletePageDialog(MainWindow* win) {
    ShowPdfPageRangeDialog(win, false);
}

void ShowPdfExtractPagesDialog(MainWindow* win) {
    ShowPdfPageRangeDialog(win, true);
}

// Default destination: same path with .pdf extension, made unique if the file
// already exists (e.g. comic.cbz → comic.pdf, or comic.1.pdf if taken).
TempStr DefaultPdfDestPathTemp(Str srcPath) {
    if (len(srcPath) == 0) {
        return {};
    }
    TempStr noExt = path::GetPathNoExtTemp(srcPath);
    if (len(noExt) == 0) {
        noExt = str::DupTemp(srcPath);
    }
    TempStr pdfPath = str::JoinTemp(noExt, StrL(".pdf"));
    return MakeUniqueFilePathTemp(pdfPath);
}

// Extract Pages from XPS, EPUB, ... (IsMupdfNonPdfDoc): the pages of the open
// document as laid out, written to a new PDF (#6509)
bool ExtractPagesToNewPdf(WindowTab* tab, const Vec<int>& pages, Str destPath) {
    if (!IsMupdfNonPdfDoc(tab)) {
        return false;
    }
    // the destination folder may not exist yet (issue #6424)
    dir::CreateForFile(destPath);
    logf("ExtractPagesToNewPdf: %d pages from '%s' to '%s'\n", len(pages), tab->filePath, destPath);
    return EngineMupdfExtractPagesToPdf(tab->GetEngine(), pages, destPath);
}

// Same conversion as File → Convert to PDF (issue #4118).
// a stamp like tmpl on each page, its text format with {n} the page's number,
// counting from start on the first page, and {total} the last number
void PageStamps(const Vec<int>& pages, const PdfStamp& tmpl, Str format, int start, Vec<PdfStamp>& out) {
    int total = start + len(pages) - 1;
    for (int i = 0; i < len(pages); i++) {
        TempStr text = str::ReplaceTemp(format, StrL("{n}"), fmt("%d", start + i));
        text = str::ReplaceTemp(text, StrL("{total}"), fmt("%d", total));
        PdfStamp st = tmpl;
        st.pageNo = pages[i];
        st.text = text;
        VecAppend(out, st);
    }
}

// the Position drop-down's choices, in its order
static const PdfStampPos kPageNumberPositions[] = {
    PdfStampPos::BottomCenter, PdfStampPos::BottomRight, PdfStampPos::BottomLeft,
    PdfStampPos::TopCenter,    PdfStampPos::TopRight,    PdfStampPos::TopLeft,
};

void PageNumberPositionLabels(StrVec& out) {
    for (Str label : {Tr("Bottom center"), Tr("Bottom right"), Tr("Bottom left"), Tr("Top center"), Tr("Top right"),
                      Tr("Top left")}) {
        out.Append(label);
    }
}

void PdfCompressQualityLabels(StrVec& out) {
    for (Str label : {Tr("Same quality"), Tr("Balanced (150 dpi pictures)"), Tr("Smallest (96 dpi pictures)")}) {
        out.Append(label);
    }
}

bool PdfCompressQualityFromName(Str name, PdfCompressQuality& out) {
    if (str::EqI(name, StrL("same"))) {
        out = PdfCompressQuality::Same;
    } else if (str::EqI(name, StrL("balanced"))) {
        out = PdfCompressQuality::Balanced;
    } else if (str::EqI(name, StrL("smallest"))) {
        out = PdfCompressQuality::Smallest;
    } else {
        return false;
    }
    return true;
}

// mutool clean's options for Compress PDF. Color and gray pictures above 1.5x
// the target dpi are scaled down to it and saved as JPEG, kept as they were
// when that isn't smaller. Black and white scans are left alone.
void AppendPdfCompressArgs(Vec<char*>& argv, PdfCompressQuality quality) {
    static const char* lossless[] = {"-gggg", "-e", "100", "-f", "-i", "-t", "-Z"};
    for (const char* a : lossless) {
        VecAppend(argv, (char*)a);
    }
    // the subsample options take threshold,target dpi
    static const char* balanced[] = {"--color-image-subsample-dpi=225,150", "--gray-image-subsample-dpi=225,150",
                                     "--color-image-recompress-method=jpeg:75",
                                     "--gray-image-recompress-method=jpeg:75"};
    static const char* smallest[] = {"--color-image-subsample-dpi=144,96", "--gray-image-subsample-dpi=144,96",
                                     "--color-image-recompress-method=jpeg:60",
                                     "--gray-image-recompress-method=jpeg:60"};
    if (quality == PdfCompressQuality::Same) {
        return;
    }
    // average only halves, so 300 dpi stays 300 for a 150 target
    static const char* bicubic[] = {"--color-image-subsample-method=bicubic", "--gray-image-subsample-method=bicubic"};
    for (const char* a : bicubic) {
        VecAppend(argv, (char*)a);
    }
    const char** extra = quality == PdfCompressQuality::Balanced ? balanced : smallest;
    for (int i = 0; i < dimofi(balanced); i++) {
        VecAppend(argv, (char*)extra[i]);
    }
}

// what Add Watermark (gray, see-through, across the middle) and Add Page
// Numbers (at positionIdx, numbered from 1) put on the pages
void DialogStamps(bool watermark, Str text, const Vec<int>& pages, int positionIdx, Vec<PdfStamp>& out) {
    PdfStamp tmpl;
    if (watermark) {
        tmpl.pos = PdfStampPos::Diagonal;
        tmpl.r = tmpl.g = tmpl.b = kWatermarkGray;
        tmpl.opacity = kWatermarkOpacity;
    } else {
        tmpl.pos = kPageNumberPositions[std::clamp(positionIdx, 0, (int)dimofi(kPageNumberPositions) - 1)];
        tmpl.size = kPageNumberSize;
        tmpl.margin = kPageNumberMargin;
    }
    PageStamps(pages, tmpl, text, 1, out);
}

// a copy of the document (with its unsaved annotations) with the stamps on,
// saved as destPath; the document shown stays as it is
bool StampPdfToFile(EngineBase* engine, const Vec<PdfStamp>& stamps, Str destPath, Str* errOut) {
    EngineBase* copy = engine ? engine->Clone() : nullptr;
    if (!copy) {
        *errOut = StrL("couldn't open a copy of the document");
        return false;
    }
    bool ok = EngineMupdfStampText(copy, stamps, errOut);
    if (ok) {
        dir::CreateForFile(destPath);
        ok = EngineMupdfSaveCopy(copy, destPath);
        if (!ok) {
            *errOut = fmt("couldn't write '%s'", destPath);
        }
    }
    SafeEngineRelease(&copy);
    return ok;
}

bool ConvertImageCollectionToPdf(EngineBase* engine, Str destPath) {
    if (!engine || !engine->isImageCollection || len(destPath) == 0) {
        return false;
    }

    TempStr producer = fmt("SumatraPDF %s", currentVersion);
    PdfCreator::SetProducerName(producer);
    return PdfCreator::SaveImageCollectionAsPdf(destPath, engine);
}

// OFD keeps its text and vector paths; image collections become one image per page
static bool ConvertDocToPdf(EngineBase* engine, Str destPath) {
    if (EngineMupdfIsOfd(engine)) {
        return EngineMupdfSaveAsPdf(engine, destPath);
    }
    return ConvertImageCollectionToPdf(engine, destPath);
}

// Make PDF From Images: the Open dialog's filter, "\1" separated as ng's
Str ImagesToPdfFilter() {
    return StrL(
        "Images\1*.png;*.jpg;*.jpeg;*.gif;*.bmp;*.tif;*.tiff;*.webp;*.heic;*.heif;*.avif;*.jp2;*.jxl\1"
        "All files\1*.*\1");
}

// next to the first image, named after it: scan1.jpg -> scan1.pdf
TempStr ImagesToPdfDestTemp(Str firstImage) {
    return MakeUniqueFilePathTemp(str::JoinTemp(path::GetPathNoExtTemp(firstImage), StrL(".pdf")));
}

// Writes a PDF with a page per image to dest. Empty when it worked, else what
// to tell the user.
TempStr MakePdfFromImagesTemp(const StrVec& paths, Str dest) {
    TempStr producer = fmt("SumatraPDF %s", currentVersion);
    PdfCreator::SetProducerName(producer);
    int bad = 0;
    ImagesPdfResult res = PdfCreator::SaveImagesAsPdf(paths, dest, &bad);
    if (res == ImagesPdfResult::BadImage) {
        return fmt(Tr("Couldn't load image '%s'").s, path::GetBaseNameTemp(paths[bad]));
    }
    if (res == ImagesPdfResult::WriteFailed) {
        return fmt(Tr("Couldn't save '%s'").s, dest);
    }
    return {};
}

// Export Annotations: next to the document, foo.pdf -> foo-annotations.md
TempStr AnnotationsExportDestTemp(Str docPath) {
    TempStr base = str::JoinTemp(path::GetPathNoExtTemp(docPath), StrL("-annotations.md"));
    return MakeUniqueFilePathTemp(base);
}

// Writes the document's annotations (unsaved ones too) to dest: CSV for a .csv
// file, else Markdown. Empty when it worked, else what to tell the user.
TempStr ExportAnnotationsTemp(EngineBase* engine, Str docPath, Str dest) {
    Vec<int> pages;
    for (int i = 1; i <= engine->PageCount(); i++) {
        VecAppend(pages, i);
    }
    Vec<AnnotRow> rows;
    CollectAnnotRows(engine, pages, rows);
    bool csv = str::EndsWithI(dest, StrL(".csv"));
    TempStr data = csv ? AnnotationsCsvTemp(rows) : AnnotationsMarkdownTemp(rows, path::GetBaseNameTemp(docPath));
    if (!file::WriteFile(dest, data)) {
        return fmt(Tr("Couldn't save '%s'").s, dest);
    }
    return {};
}

// 0 if not a usable font size
int EbookPdfFontSizeFromText(Str s) {
    str::TrimWsBoth(s);
    int n = ParseInt(s);
    if (n < 1 || n > kEbookPdfMaxFontSize) {
        return 0;
    }
    return n;
}

// why caj2pdf failed for this file, {} if it didn't
TempStr CajErrorReasonTemp(Str path) {
    u32 code = 0;
    switch (GetCajError(path, &code)) {
        case CajError::None:
            return {};
        case CajError::NoConverter:
            return str::DupTemp(Tr("Opening CAJ files needs the caj2pdf converter"));
        case CajError::DownloadFailed:
            return str::DupTemp(Tr("Downloading the caj2pdf converter failed"));
        case CajError::ConverterDidNotRun:
            return fmt(Tr("The caj2pdf converter failed to run (error 0x%x)").s, code);
        case CajError::ConversionFailed:
            return fmt(Tr("The caj2pdf converter couldn't convert this file (exit code %d)").s, (int)code);
        case CajError::TimedOut:
            return str::DupTemp(Tr("The caj2pdf converter took too long"));
    }
    return {};
}

// Convert EPUB to PDF on the current document without its dialog. Used by
// tests/convert-epub-to-pdf.ts
TempStr ConvertEpubToPdfResultTemp(Str destPath, int pageSizeIdx, int fontSize, int* exitCodeOut) {
    auto finish = [&](int code, TempStr s) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return s;
    };
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    if (!IsEpubDoc(tab)) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-epub")));
    }
    if (pageSizeIdx < 0 || pageSizeIdx >= dimofi(kEbookPdfPageSizes) || fontSize < 1) {
        return finish(1, str::DupTemp(StrL("ERROR bad-args")));
    }
    const EbookPdfPageSize& ps = kEbookPdfPageSizes[pageSizeIdx];
    int n = EngineMupdfEbookToPdf(tab->filePath, destPath, ps.dx, ps.dy, (float)fontSize);
    if (n == 0) {
        return finish(1, str::DupTemp(StrL("ERROR convert-failed")));
    }
    return finish(0, fmt("OK pages=%d", n));
}

TempStr ConvertImageCollectionToPdfResultTemp(Str srcPath, Str destPath, int* exitCodeOut) {
    auto finish = [&](int code, TempStr s) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return s;
    };
    if (len(srcPath) == 0 || len(destPath) == 0) {
        return finish(1, str::DupTemp(StrL("ERROR bad-args")));
    }
    EngineBase* engine = CreateEngineFromFile(srcPath, nullptr, false);
    if (!engine) {
        return finish(1, str::DupTemp(StrL("ERROR engine-create-failed")));
    }
    // as the Convert to PDF dialog
    bool ok = ConvertDocToPdf(engine, destPath);
    SafeEngineRelease(&engine);
    if (!ok) {
        return finish(1, str::DupTemp(StrL("ERROR convert-failed")));
    }
    return finish(0, str::DupTemp(StrL("OK")));
}

TempStr ExtractPdfPagesResultTemp(Str destPath, Str pagesSpec, int annotsOnly, int* exitCodeOut) {
    auto finish = [&](int code, TempStr s) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return s;
    };
    if (len(gWindows) == 0) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-window")));
    }
    MainWindow* win = gWindows[0];
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    EngineBase* engine = tab ? tab->GetEngine() : nullptr;
    if (!engine || len(tab->filePath) == 0) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-engine")));
    }
    if (len(destPath) == 0 || len(pagesSpec) == 0) {
        return finish(1, str::DupTemp(StrL("ERROR bad-args")));
    }
    int pageCount = engine->PageCount();
    Vec<int> parsedPages;
    if (!ParseDeletePages(pagesSpec, pageCount, parsedPages)) {
        return finish(1, str::DupTemp(StrL("ERROR bad-pages")));
    }
    if (IsMupdfNonPdfDoc(tab)) {
        if (!ExtractPagesToNewPdf(tab, parsedPages, destPath)) {
            return finish(1, str::DupTemp(StrL("ERROR extract")));
        }
        return finish(0, str::DupTemp(StrL("OK")));
    }
    if (annotsOnly) {
        if (!KeepOnlyPagesWithAnnotations(engine, parsedPages)) {
            return finish(1, str::DupTemp(StrL("ERROR annots")));
        }
        if (len(parsedPages) == 0) {
            return finish(1, str::DupTemp(StrL("ERROR no-annot-pages")));
        }
    }
    TempStr pageRange = FormatPageRangeTemp(parsedPages);
    Str inputPath = tab->filePath;
    TempStr tmpPath;
    if (EngineHasUnsavedAnnotations(engine)) {
        tmpPath = GetTempFilePathTemp(StrL("extract-pages"));
        if (len(tmpPath) == 0 || !EngineMupdfSaveCopy(engine, tmpPath)) {
            return finish(1, str::DupTemp(StrL("ERROR save-copy")));
        }
        inputPath = tmpPath;
    }
    dir::CreateForFile(destPath);
    char* argv[] = {
        (char*)"clean",     (char*)"-p",         CStrTemp(EngineMupdfToolPasswordTemp(engine, tab->filePath)),
        (char*)"-gggg",     (char*)"-e",         (char*)"100",
        (char*)"-f",        (char*)"-i",         (char*)"-t",
        (char*)"-Z",        CStrTemp(inputPath), CStrTemp(destPath),
        CStrTemp(pageRange)};
    int res = RunPdfToolMain(pdfclean_main, dimofi(argv), argv);
    if (tmpPath) {
        file::Delete(tmpPath);
    }
    if (res != 0) {
        return finish(1, str::DupTemp(StrL("ERROR pdfclean")));
    }
    return finish(0, str::DupTemp(StrL("OK")));
}

constexpr i64 kMaxSaveSelectionPixels = 100LL * 1000 * 1000;

constexpr int kMaxSaveSelectionSide = 16384;

float SaveSelectionZoom(EngineBase* engine, float dpi) {
    float fileDpi = engine->fileDPI;
    if (fileDpi <= 0) {
        fileDpi = 72.0f;
    }
    if (dpi < 1) {
        dpi = (float)kSaveSelectionDefaultDpi;
    }
    return dpi / fileDpi;
}

bool SaveSelectionSizeOk(int w, int h) {
    if (w <= 0 || h <= 0) {
        return false;
    }
    if (w > kMaxSaveSelectionSide || h > kMaxSaveSelectionSide) {
        return false;
    }
    i64 pixels = (i64)w * (i64)h;
    return pixels <= kMaxSaveSelectionPixels;
}

bool EstimateSelectionPx(RectF rect, float zoom, int& w, int& h) {
    w = (int)floorf((rect.dx * zoom) + 0.5f);
    h = (int)floorf((rect.dy * zoom) + 0.5f);
    return w > 0 && h > 0;
}

const ConvertImageFormat kConvertImageFormats[kConvertImageFormatCount] = {
    {StrL("PNG"), StrL(".png")},
    {StrL("JPEG"), StrL(".jpg")},
    {StrL("BMP"), StrL(".bmp")},
    {StrL("GIF"), StrL(".gif")},
};
constexpr int kConvertImageJpegIdx = 1;

// index into kConvertImageFormats for path's extension, -1 if none
static int FindConvertImageFormat(Str path) {
    if (str::EndsWithI(path, StrL(".jpeg"))) {
        return kConvertImageJpegIdx;
    }
    for (int i = 0; i < kConvertImageFormatCount; i++) {
        if (str::EndsWithI(path, kConvertImageFormats[i].ext)) {
            return i;
        }
    }
    return -1;
}

// index into kConvertImageFormats for path, PNG when the extension is unknown
int ConvertImageFormatIdxFromPath(Str path) {
    int idx = FindConvertImageFormat(path);
    return idx < 0 ? 0 : idx;
}

bool IsSupportedConvertImageExt(Str path) {
    return FindConvertImageFormat(path) >= 0;
}

bool PathHasPagePlaceholder(Str path) {
    return str::ContainsI(path, StrL("<N>"));
}

// dest template: if <N> is missing and we'll write more than one file, insert
// it before the extension so pages don't overwrite each other
TempStr EnsurePagePlaceholderTemp(Str path, bool multiPage) {
    if (PathHasPagePlaceholder(path) || !multiPage) {
        return str::DupTemp(path);
    }
    TempStr ext = path::GetExtTemp(path);
    TempStr noExt = path::GetPathNoExtTemp(path);
    if (len(ext) == 0) {
        return str::JoinTemp(noExt, StrL("-<N>.png"));
    }
    return str::JoinTemp(noExt, StrL("-<N>"), ext);
}

TempStr WithDefaultImageExtTemp(Str path) {
    if (IsSupportedConvertImageExt(path)) {
        return str::DupTemp(path);
    }
    TempStr ext = path::GetExtTemp(path);
    if (ext) {
        // has an extension we don't write
        return {};
    }
    return str::JoinTemp(path, StrL(".png"));
}

static TempStr ReplacePagePlaceholderTemp(Str path, int pageNo) {
    TempStr n = fmt("%d", pageNo);
    TempStr s = str::ReplaceTemp(path, StrL("<N>"), n);
    if (str::Contains(s, StrL("<n>"))) {
        s = str::ReplaceTemp(s, StrL("<n>"), n);
    }
    return s;
}

// the template ConvertPagesToImages writes: with an image extension and, for
// more than one page, <N>
static TempStr ImagesTemplateTemp(Str templatePath, int nPages) {
    TempStr withExt = WithDefaultImageExtTemp(templatePath);
    if (len(withExt) == 0) {
        return {};
    }
    return EnsurePagePlaceholderTemp(withExt, nPages > 1);
}

// how many of the files converting pages to templatePath writes already exist
int CountExistingImages(Str templatePath, const Vec<int>& pages) {
    TempStr templ = ImagesTemplateTemp(templatePath, len(pages));
    if (len(templ) == 0) {
        return 0;
    }
    int n = 0;
    for (int pageNo : pages) {
        if (file::Exists(ReplacePagePlaceholderTemp(templ, pageNo))) {
            n++;
        }
    }
    return n;
}

// templatePath with ".<k>" before the extension, the smallest k that
// overwrites none of the files: book-<N>.png => book-<N>.1.png (#6439)
TempStr UniqueImagesTemplateTemp(Str templatePath, const Vec<int>& pages) {
    if (CountExistingImages(templatePath, pages) == 0) {
        return str::DupTemp(templatePath);
    }
    TempStr templ = ImagesTemplateTemp(templatePath, len(pages));
    TempStr noExt = path::GetPathNoExtTemp(templ);
    TempStr ext = path::GetExtTemp(templ);
    for (int k = 1; k < 10000; k++) {
        TempStr candidate = fmt("%s.%d%s", noExt, k, ext);
        if (CountExistingImages(candidate, pages) == 0) {
            return candidate;
        }
    }
    return str::DupTemp(templatePath);
}

TempStr ExistingImagesTextTemp(int nExisting) {
    if (nExisting == 1) {
        return str::DupTemp(Tr("1 file already exists"));
    }
    return fmt(Tr("%d files already exist").s, nExisting);
}

static bool PageFitsDpi(EngineBase* engine, int pageNo, int dpi) {
    float zoom = SaveSelectionZoom(engine, (float)dpi);
    int w = 0;
    int h = 0;
    EstimateSelectionPx(engine->PageMediabox(pageNo), zoom, w, h);
    return SaveSelectionSizeOk(w, h);
}

bool PagesFitDpi(EngineBase* engine, const Vec<int>& pages, int dpi) {
    for (int pageNo : pages) {
        if (!PageFitsDpi(engine, pageNo, dpi)) {
            return false;
        }
    }
    return true;
}

// render each page at dpi and write it to templatePath with <N> replaced by
// the 1-based page number. Returns how many files were written.
int ConvertPagesToImages(EngineBase* engine, int rotation, Str templatePath, const Vec<int>& pages, int dpi,
                         Str* firstPathOwnedOut, const Func1<int>& onProgress) {
    if (firstPathOwnedOut) {
        *firstPathOwnedOut = {};
    }
    if (!engine || len(pages) == 0 || len(templatePath) == 0) {
        return 0;
    }
    TempStr withExt = WithDefaultImageExtTemp(templatePath);
    if (len(withExt) == 0) {
        return 0;
    }
    TempStr templ = EnsurePagePlaceholderTemp(withExt, len(pages) > 1);
    float zoom = SaveSelectionZoom(engine, (float)dpi);
    int nOk = 0;
    StrVec pngs;
    int nDone = 0;
    for (int pageNo : pages) {
        onProgress.Call(nDone++);
        if (!PageFitsDpi(engine, pageNo, dpi)) {
            logf("ConvertPagesToImages: page %d at %d DPI is too large\n", pageNo, dpi);
            continue;
        }
        TempStr dest = ReplacePagePlaceholderTemp(templ, pageNo);
        RenderPageArgs args(pageNo, zoom, rotation);
        Pixmap* px = engine->RenderPage(args);
        if (!px) {
            logf("ConvertPagesToImages: RenderPage failed for page %d\n", pageNo);
            continue;
        }
        px->xres = (float)dpi;
        px->yres = (float)dpi;
        // the destination folder may not exist yet (issue #6424)
        dir::CreateForFile(dest);
        bool ok = SavePixmapAsImageFile(px, dest);
        FreePixmap(px);
        if (!ok) {
            logf("ConvertPagesToImages: save failed for '%s'\n", dest);
            continue;
        }
        if (nOk == 0 && firstPathOwnedOut) {
            *firstPathOwnedOut = str::Dup(dest);
        }
        if (str::EndsWithI(dest, StrL(".png"))) {
            pngs.Append(dest);
        }
        nOk++;
    }
    OptimizePngFilesAsync(pngs);
    return nOk;
}

void CollectAllPages(int pageCount, Vec<int>& pages) {
    VecReset(pages);
    for (int i = 1; i <= pageCount; i++) {
        VecAppend(pages, i);
    }
}

TempStr ConvertPagesToImagesResultTemp(Str templatePath, Str pagesSpec, int dpi, int* exitCodeOut) {
    auto finish = [&](int code, TempStr s) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return s;
    };
    if (len(gWindows) == 0) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-window")));
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-engine")));
    }
    int pageCount = engine->PageCount();
    Vec<int> pages;
    if (str::EqI(pagesSpec, StrL("current"))) {
        int cur = dm->CurrentPageNo();
        if (cur < 1 || cur > pageCount) {
            return finish(1, str::DupTemp(StrL("ERROR bad-current")));
        }
        VecAppend(pages, cur);
    } else if (str::EqI(pagesSpec, StrL("all"))) {
        CollectAllPages(pageCount, pages);
    } else if (!ParseDeletePages(pagesSpec, pageCount, pages)) {
        return finish(1, str::DupTemp(StrL("ERROR bad-pages")));
    }
    if (dpi < 1) {
        dpi = kConvertPdfToImagesDpi;
    }
    Str firstPath;
    int n = ConvertPagesToImages(engine, dm->GetRotation(), templatePath, pages, dpi, &firstPath);
    if (n == 0) {
        str::Free(firstPath);
        return finish(1, str::DupTemp(StrL("ERROR convert-failed")));
    }
    TempStr res = fmt("OK n=%d first=%s", n, firstPath);
    str::Free(firstPath);
    return finish(0, res);
}

Pixmap* RenderSelectionPixmap(EngineBase* engine, int rotation, int pageNo, RectF rect, float dpi) {
    if (!engine || rect.IsEmpty()) {
        return nullptr;
    }
    if (pageNo < 1 || pageNo > engine->PageCount()) {
        return nullptr;
    }
    float zoom = SaveSelectionZoom(engine, dpi);
    int estW = 0;
    int estH = 0;
    if (!EstimateSelectionPx(rect, zoom, estW, estH) || !SaveSelectionSizeOk(estW, estH)) {
        logf("RenderSelectionPixmap: %dx%d at %.0f DPI is too large\n", estW, estH, dpi);
        return nullptr;
    }
    RenderPageArgs args(pageNo, zoom, rotation, &rect, RenderTarget::Export);
    Pixmap* px = engine->RenderPage(args);
    if (!px) {
        logf("RenderSelectionPixmap: RenderPage failed page %d\n", pageNo);
        return nullptr;
    }
    px->xres = dpi;
    px->yres = dpi;
    return PixmapToBgra(px);
}

bool WriteSelectionPixmap(Pixmap* px, Str destPath) {
    if (!px || len(destPath) == 0) {
        return false;
    }
    // the destination folder may not exist yet (issue #6424)
    dir::CreateForFile(destPath);
    bool ok = false;
    if (str::EndsWithI(destPath, StrL(".png"))) {
        // lodepng (no pHYs, so EngineImages displays 1:1) now, zopfli in the
        // background: a 300 dpi page takes it many seconds
        Str png = EncodePngFromPixmap(px);
        ok = len(png) > 0 && file::WriteFile(destPath, png);
        str::Free(png);
        if (!ok) {
            file::Delete(destPath);
        } else {
            OptimizePngFileAsync(destPath);
        }
    } else {
        ok = SavePixmapAsImageFile(px, destPath);
    }
    return ok;
}

static bool SavePageRectAsImage(EngineBase* engine, int rotation, int pageNo, RectF rect, Str destPath, float dpi,
                                int* widthOut, int* heightOut) {
    if (widthOut) {
        *widthOut = 0;
    }
    if (heightOut) {
        *heightOut = 0;
    }
    Pixmap* px = RenderSelectionPixmap(engine, rotation, pageNo, rect, dpi);
    if (!px) {
        return false;
    }
    bool ok = WriteSelectionPixmap(px, destPath);
    if (ok) {
        if (widthOut) {
            *widthOut = px->width;
        }
        if (heightOut) {
            *heightOut = px->height;
        }
    }
    FreePixmap(px);
    return ok;
}

TempStr SaveSelectionAsImageResultTemp(Str destPath, int dpi, int pageNo, int x, int y, int dx, int dy,
                                       int* exitCodeOut) {
    auto finish = [&](int code, TempStr s) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return s;
    };
    if (len(gWindows) == 0) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-window")));
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-engine")));
    }
    if (len(destPath) == 0 || dpi < 1 || dx <= 0 || dy <= 0) {
        return finish(1, str::DupTemp(StrL("ERROR bad-args")));
    }
    TempStr withExt = WithDefaultImageExtTemp(destPath);
    if (len(withExt) == 0) {
        return finish(1, str::DupTemp(StrL("ERROR bad-ext")));
    }
    RectF rect{(float)x, (float)y, (float)dx, (float)dy};
    int w = 0;
    int h = 0;
    if (!SavePageRectAsImage(engine, dm->PageRotation(pageNo), pageNo, rect, withExt, (float)dpi, &w, &h)) {
        return finish(1, str::DupTemp(StrL("ERROR save-failed")));
    }
    return finish(0, fmt("OK w=%d h=%d path=%s", w, h, withExt));
}

bool ExtractTextViaEngine(WindowTab* tab, Str destPath, Str pages) {
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    if (!dm) {
        return false;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        return false;
    }
    int pageCount = engine->PageCount();
    Vec<PageRange> ranges;
    if (!ParsePageRanges(pages, ranges)) {
        return false;
    }
    str::Builder text;
    for (auto& range : ranges) {
        int start = std::max(range.start, 1);
        int end = std::min(range.end, pageCount);
        for (int pageNo = start; pageNo <= end; pageNo++) {
            PageText pt = engine->ExtractPageText(pageNo);
            if (pt.text) {
                text.Append(Str(pt.text.s));
                text.AppendChar('\n');
            }
            FreePageText(&pt);
        }
    }
    return file::WriteFile(destPath, ToStr(text));
}

void SaveSelImgThread(SaveSelImgWork* d) {
    d->ok = WriteSelectionPixmap(d->px, d->destPath);
    FreePixmap(d->px);
    d->px = nullptr;
    uitask::Post(MkFunc0(FinishSaveSelImg, d), "FinishSaveSelImg");
}

// the tools keep getopt state in globals: one runs at a time
static Mutex gPdfToolMutex;

int RunPdfToolMain(PdfToolMain toolMain, int argc, char** argv) {
    ScopedMutex lock(&gPdfToolMutex);
    fz_set_optind(0);
    return toolMain(argc, argv);
}

PdfToolJob* NewPdfToolJob(PdfToolMain toolMain, Str logName, Str destPath, Str failMsg, Str failTitle) {
    auto* job = new PdfToolJob();
    job->toolMain = toolMain;
    job->logName = str::Dup(logName);
    job->destPath = str::Dup(destPath);
    job->failMsg = str::Dup(failMsg);
    job->failTitle = str::Dup(failTitle);
    return job;
}

// tmpPath: the input's temp copy, if any, which the job deletes
void SetPdfToolJobArgs(PdfToolJob* job, char** argv, int argc, Str tmpPath) {
    for (int i = 0; i < argc; i++) {
        job->args.Append(Str(argv[i]));
    }
    if (len(tmpPath) > 0) {
        job->tmpPath = str::Dup(tmpPath);
    }
}

void FreePdfToolJob(PdfToolJob* job) {
    str::Free(job->tmpPath);
    str::Free(job->destPath);
    str::Free(job->logName);
    str::Free(job->failMsg);
    str::Free(job->failTitle);
    delete job;
}

static void PdfToolJobDone(PdfToolJob* job) {
    if (job->res == 0) {
        logf("%s: succeeded\n", job->logName);
    } else {
        logf("%s: failed with %d\n", job->logName, job->res);
    }
    if (job->onFinish.IsValid()) {
        job->onFinish.Call(job);
    } else {
        FinishPdfToolJob(job);
    }
    FreePdfToolJob(job);
}

int PdfToolJob::Run() {
    Vec<char*> argv;
    for (Str s : args) {
        VecAppend(argv, CStrTemp(s));
    }
    return RunPdfToolMain(toolMain, len(argv), argv.els);
}

static void PdfToolJobThread(PdfToolJob* job) {
    job->res = job->Run();
    if (len(job->tmpPath) > 0) {
        file::Delete(job->tmpPath);
    }
    uitask::Post(MkFunc0(PdfToolJobDone, job), "PdfToolJobDone");
}

// a headless caller (tests) needs the result now
int RunPdfToolJobNow(PdfToolJob* job) {
    int res = job->Run();
    if (len(job->tmpPath) > 0) {
        file::Delete(job->tmpPath);
    }
    FreePdfToolJob(job);
    return res;
}

// the job outlives its progress tasks: PdfToolJobDone is posted after them
static void PostPdfToolJobProgress(PdfToolJob* job, int nDone) {
    AtomicIntSet(&job->nDone, nDone);
    uitask::Post(MkFunc0(PdfToolJobProgress, job), "PdfToolJobProgress");
}

static void InitJob(PdfToolJob* job, Str logName, Str destPath, Str failMsg, Str failTitle) {
    job->logName = str::Dup(logName);
    job->destPath = str::Dup(destPath);
    job->failMsg = str::Dup(failMsg);
    job->failTitle = str::Dup(failTitle);
}

// stamps a clone of the document: cloning off the UI thread is safe
// (thumbnails do it) and the reference keeps the engine alive
struct StampJob : PdfToolJob {
    EngineBase* engine = nullptr;
    bool watermark = true;
    Str text; // owned
    Vec<int> pages;
    int posIdx = 0;

    ~StampJob() override {
        SafeEngineRelease(&engine);
        str::Free(text);
    }
    int Run() override {
        Vec<PdfStamp> stamps;
        DialogStamps(watermark, text, pages, posIdx, stamps);
        Str err;
        if (StampPdfToFile(engine, stamps, destPath, &err)) {
            return 0;
        }
        logf("StampJob: failed: %s\n", err);
        str::ReplaceWithCopy(&failMsg, err);
        return 1;
    }
};

PdfToolJob* NewStampJob(EngineBase* engine, bool watermark, Str text, const Vec<int>& pages, int posIdx, Str destPath) {
    auto* job = new StampJob();
    Str title = watermark ? Tr("Add Watermark") : Tr("Add Page Numbers");
    InitJob(job, StrL("PdfStampDoIt"), destPath, {}, title);
    engine->AddRef();
    job->engine = engine;
    job->watermark = watermark;
    job->text = str::Dup(text);
    job->pages = pages;
    job->posIdx = posIdx;
    return job;
}

// render threads already render the document's pages concurrently, so one
// more thread may too; the reference keeps the engine alive
struct ImagesJob : PdfToolJob {
    EngineBase* engine = nullptr;
    int rotation = 0;
    Vec<int> pages;
    int dpi = 0;

    ~ImagesJob() override { SafeEngineRelease(&engine); }
    int Run() override {
        Str firstPath;
        auto onProgress = MkFunc1(PostPdfToolJobProgress, (PdfToolJob*)this);
        int nOk = ConvertPagesToImages(engine, rotation, destPath, pages, dpi, &firstPath, onProgress);
        logf("ImagesJob: wrote %d of %d file(s)\n", nOk, len(pages));
        if (nOk == 0) {
            str::Free(firstPath);
            return 1;
        }
        // the file the dialog shows
        str::Free(destPath);
        destPath = firstPath;
        return 0;
    }
};

PdfToolJob* NewImagesJob(EngineBase* engine, int rotation, Str templatePath, const Vec<int>& pages, int dpi) {
    auto* job = new ImagesJob();
    InitJob(job, StrL("ConvertPdfToImages"), templatePath, Tr("Failed to convert to images."), Tr("Convert to Images"));
    engine->AddRef();
    job->engine = engine;
    job->rotation = rotation;
    job->pages = pages;
    job->dpi = dpi;
    job->nTotal = len(pages);
    job->done = PdfToolDone::ShowDest;
    return job;
}

// lays the book out anew from its file: no engine of the UI's is touched
struct EpubToPdfJob : PdfToolJob {
    Str srcPath; // owned
    float pageDx = 0;
    float pageDy = 0;
    int fontSize = 0;

    ~EpubToPdfJob() override { str::Free(srcPath); }
    int Run() override {
        int nPages = EngineMupdfEbookToPdf(srcPath, destPath, pageDx, pageDy, (float)fontSize);
        logf("ConvertEpubToPdf: wrote %d pages\n", nPages);
        return nPages > 0 ? 0 : 1;
    }
};

PdfToolJob* NewEpubToPdfJob(Str srcPath, Str destPath, const EbookPdfPageSize& ps, int fontSize) {
    auto* job = new EpubToPdfJob();
    InitJob(job, StrL("ConvertEpubToPdf"), destPath, Tr("Failed to save a file"), Tr("Convert EPUB to PDF"));
    job->srcPath = str::Dup(srcPath);
    job->pageDx = ps.dx;
    job->pageDy = ps.dy;
    job->fontSize = fontSize;
    return job;
}

struct CajToPdfJob : PdfToolJob {
    Str srcPath; // owned

    ~CajToPdfJob() override { str::Free(srcPath); }
    int Run() override {
        if (ConvertCajToPdf(srcPath, destPath)) {
            return 0;
        }
        str::ReplaceWithCopy(&failMsg, CajErrorReasonTemp(srcPath));
        return 1;
    }
};

PdfToolJob* NewCajToPdfJob(Str srcPath, Str destPath) {
    auto* job = new CajToPdfJob();
    InitJob(job, StrL("ConvertCajToPdf"), destPath, {}, Tr("Convert CAJ to PDF"));
    job->srcPath = str::Dup(srcPath);
    return job;
}

// converts the open document: its engine already serves concurrent render
// threads (OFD conversion holds docLock); the reference keeps it alive
struct ConvertToPdfJob : PdfToolJob {
    EngineBase* engine = nullptr;

    ~ConvertToPdfJob() override { SafeEngineRelease(&engine); }
    int Run() override { return ConvertDocToPdf(engine, destPath) ? 0 : 1; }
};

PdfToolJob* NewConvertToPdfJob(EngineBase* engine, Str destPath) {
    auto* job = new ConvertToPdfJob();
    InitJob(job, StrL("ConvertToPdf"), destPath, Tr("Failed to save a file"), Tr("Convert to PDF"));
    engine->AddRef();
    job->engine = engine;
    return job;
}

// merges into a temp file, then copies it over destPath, which may be one of
// the sources
struct MergeJob : PdfToolJob {
    StrVec paths;
    StrVec passwords;
    Vec<PdfMergePage> pages;

    int Run() override {
        Vec<PdfMergeSource> srcs;
        for (int i = 0; i < len(paths); i++) {
            VecAppend(srcs, PdfMergeSource{paths[i], passwords[i]});
        }
        TempStr mergedPath = GetTempFilePathTemp(StrL("merge"));
        bool ok = len(mergedPath) > 0 && EngineMupdfMergePdfs(srcs, pages, mergedPath);
        if (ok) {
            Str data = file::ReadFile(mergedPath);
            ok = len(data) > 0 && file::WriteFile(destPath, data);
            str::Free(data);
        }
        if (len(mergedPath) > 0) {
            file::Delete(mergedPath);
        }
        logf("MergeJob: %d pages from %d files to '%s', ok: %d\n", len(pages), len(srcs), destPath, (int)ok);
        return ok ? 0 : 1;
    }
};

// srcCopy: a copy of the first source with its unsaved changes, deleted after
PdfToolJob* NewMergeJob(const Vec<PdfMergeSource>& srcs, const Vec<PdfMergePage>& pages, Str srcCopy, Str destPath) {
    auto* job = new MergeJob();
    InitJob(job, StrL("MergePdf"), destPath, fmt(Tr("Couldn't save '%s'").s, destPath), Tr("Merge PDF"));
    for (const PdfMergeSource& src : srcs) {
        job->paths.Append(src.path);
        job->passwords.Append(src.password);
    }
    if (len(srcCopy) > 0) {
        job->paths.SetAt(0, srcCopy);
        job->tmpPath = str::Dup(srcCopy);
    }
    job->pages = pages;
    job->done = PdfToolDone::ReloadDest;
    return job;
}

// a big PDF takes seconds to rewrite: off the UI thread
void StartPdfToolJob(PdfToolJob* job) {
    RunAsync(MkFunc0(PdfToolJobThread, job), StrL("PdfToolJob"));
}
