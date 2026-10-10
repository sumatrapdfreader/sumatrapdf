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
#include "ImageReader.h"
#include "PngOptimizer.h"
#include "base/Pixmap.h"
#include "SumatraPDF.h"
#include "SumatraConfig.h"
#include "MainWindow.h"
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

extern "C" int pdfbake_main(int argc, char** argv);
extern "C" int pdfclean_main(int argc, char** argv);
extern "C" int muconvert_main(int argc, char** argv);
extern "C" void fz_set_optind(int val);

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
    for (Annotation* annotation : annotations) {
        int pageNo = PageNo(annotation);
        if (!VecContains(annotationPages, pageNo)) {
            VecAppend(annotationPages, pageNo);
        }
    }

    Vec<int> filteredPages;
    for (int pageNo : pages) {
        if (VecContains(annotationPages, pageNo)) {
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

// Same conversion as File → Convert to PDF (issue #4118).
bool ConvertImageCollectionToPdf(EngineBase* engine, Str destPath) {
    if (!engine || !engine->isImageCollection || len(destPath) == 0) {
        return false;
    }

    TempStr producer = fmt("SumatraPDF %s", currentVersion);
    PdfCreator::SetProducerName(producer);
    // Formats PDF cannot re-wrap (WebP, JXL, HEIC, AVIF, TGA, …): decode via
    // the same codecs we use for viewing, then PNG + zopfli before embed.
    auto toOptimizedPng = [](Str data) -> Str {
        Pixmap* px = PixmapFromData(data);
        if (!px) {
            logf("ConvertToPdf: decode-to-pixmap failed (%d bytes)\n", len(data));
            return {};
        }
        Str png = EncodeAndOptimizePngFromPixmap(px);
        FreePixmap(px);
        return png;
    };
    return PdfCreator::SaveImageCollectionAsPdf(destPath, engine, toOptimizedPng);
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
    bool ok = ConvertImageCollectionToPdf(engine, destPath);
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
    char* argv[] = {(char*)"clean",      (char*)"-gggg",     (char*)"-e",        (char*)"100",
                    (char*)"-f",         (char*)"-i",        (char*)"-t",        (char*)"-Z",
                    CStrTemp(inputPath), CStrTemp(destPath), CStrTemp(pageRange)};
    fz_set_optind(0);
    int res = pdfclean_main(11, argv);
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

int ConvertImageFormatIdxFromPath(Str path) {
    if (str::EndsWithI(path, StrL(".jpg")) || str::EndsWithI(path, StrL(".jpeg"))) {
        return 1;
    }
    if (str::EndsWithI(path, StrL(".bmp"))) {
        return 2;
    }
    return 0; // PNG
}

bool IsSupportedConvertImageExt(Str path) {
    return str::EndsWithI(path, StrL(".png")) || str::EndsWithI(path, StrL(".jpg")) ||
           str::EndsWithI(path, StrL(".jpeg")) || str::EndsWithI(path, StrL(".bmp"));
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
                         Str* firstPathOwnedOut) {
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
    for (int pageNo : pages) {
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
    if (!SavePageRectAsImage(engine, dm->GetRotation(), pageNo, rect, withExt, (float)dpi, &w, &h)) {
        return finish(1, str::DupTemp(StrL("ERROR save-failed")));
    }
    return finish(0, fmt("OK w=%d h=%d path=%s", w, h, withExt));
}
