/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the POSIX half of Print.cpp. Pages are rendered through mupdf and written
// with PdfCreator. Interactive printing saves that PDF; command-line printing
// can send it to the platform spooler. What the win32 path does with a DEVMODE
// has no counterpart yet and is recorded in docs/port-progress.md.

#include "base/Base.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "gui/WasmBridge.h"

#if !OS_WASM
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "AppSettings.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "DisplayModel.h"
#include "PngOptimizer.h"
#include "PdfCreator.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Selection.h"
#include "WindowTab.h"
#include "SumatraDialogs.h"
#include "Translations.h"
#include "Print.h"

#include "SumatraLog.h"

#if !OS_WASM
extern char** environ;
#endif

// the resolution a page is rasterized at before it goes into the PDF
constexpr int kPrintToPdfDpi = 150;
static constexpr char kDefaultPrinterToken[] = "\037default-printer";

struct PosixPageRange {
    int from;
    int to;
};

// appends stdout and stderr while waiting for a short-lived command
static bool AppendCommandOutput(str::Builder& out, char* const argv[]) {
#if OS_WASM
    (void)out;
    (void)argv;
    return false;
#else
    int fds[2]{};
    if (pipe(fds) != 0) {
        return false;
    }
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, fds[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, fds[0]);
    posix_spawn_file_actions_addclose(&actions, fds[1]);
    pid_t pid = 0;
    int err = posix_spawnp(&pid, argv[0], &actions, nullptr, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);
    if (err != 0) {
        close(fds[0]);
        return false;
    }
    char buf[4096];
    for (;;) {
        ssize_t n = read(fds[0], buf, dimof(buf));
        if (n > 0) {
            out.Append(Str(buf, (int)n));
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        break;
    }
    close(fds[0]);
    int status = 0;
    do {
        err = waitpid(pid, &status, 0) < 0 ? errno : 0;
    } while (err == EINTR);
    return err == 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

void GetPrintersInfo(str::Builder& out) {
#if OS_WASM
    out.Append(StrL("No printers: this build prints to a PDF file.\n"));
#else
    char* argv[] = {(char*)"lpstat", (char*)"-d", (char*)"-p", nullptr};
    if (!AppendCommandOutput(out, argv) && len(out) == 0) {
        out.Append(StrL("Unable to run lpstat.\n"));
    } else if (len(out) == 0) {
        out.Append(StrL("No printers found.\n"));
    }
#endif
}

// bounds all selection fragments on a page, as the Windows print path does
static RectF SelectionBounds(const Vec<SelectionOnPage>& selection, int pageNo) {
    RectF bounds;
    for (const SelectionOnPage& sel : selection) {
        if (sel.pageNo == pageNo) {
            bounds = bounds.Union(sel.rect);
        }
    }
    return bounds;
}

// renders every page, or the selected part of its pages, into a new PDF
static bool PrintEngineToPdf(EngineBase* engine, Str destPath, const Vec<SelectionOnPage>* selection = nullptr,
                             int rotation = 0, const Vec<int>* pages = nullptr, float dpi = kPrintToPdfDpi,
                             int orientation = 0) {
    EnsureFullLayout(engine);
    PdfCreator c;
    float zoom = dpi / engine->fileDPI;
    int nPages = engine->PageCount();
    int nAdded = 0;
    int nToPrint = pages ? len(*pages) : nPages;
    for (int i = 0; i < nToPrint; i++) {
        int pageNo = pages ? (*pages)[i] : i + 1;
        RectF bounds;
        RectF* boundsPtr = nullptr;
        if (selection) {
            bounds = SelectionBounds(*selection, pageNo);
            if (bounds.IsEmpty()) {
                continue;
            }
            boundsPtr = &bounds;
        }
        int pageRotation = rotation;
        if (orientation != 0) {
            RectF box = engine->PageMediabox(pageNo);
            bool isLandscape = box.dx > box.dy;
            bool wantLandscape = orientation > 0;
            if (isLandscape != wantLandscape) {
                pageRotation = (pageRotation + 90) % 360;
            }
        }
        RenderPageArgs args(pageNo, zoom, pageRotation, boundsPtr, RenderTarget::Print);
        Pixmap* bmp = engine->RenderPage(args);
        if (!bmp) {
            logf("PrintEngineToPdf: failed to render page %d\n", pageNo);
            return false;
        }
        Str png = EncodeAndOptimizePngFromPixmap(bmp);
        FreePixmap(bmp);
        if (len(png) == 0) {
            logf("PrintEngineToPdf: failed to encode page %d\n", pageNo);
            return false;
        }
        bool ok = c.AddPageFromImageData(png, dpi);
        str::Free(png);
        if (!ok) {
            logf("PrintEngineToPdf: failed to add page %d\n", pageNo);
            return false;
        }
        nAdded++;
    }
    if (nAdded == 0) {
        return false;
    }
    c.CopyProperties(engine);
    return c.SaveToFile(destPath);
}

// `output=<path>` of -print-settings, the only token this path understands
static TempStr OutputPathFromSettingsTemp(Str settings) {
    if (len(settings) == 0) {
        return {};
    }
    StrVec list;
    Split(&list, settings, StrL(","), true);
    for (Str s : list) {
        if (str::TrimPrefixI(s, StrL("output="))) {
            return str::DupTemp(s);
        }
    }
    return {};
}

// parses the page-selection subset of the original -print-settings syntax
static void ApplyRenderSettings(Str settings, int pageCount, Vec<int>& pages, float& dpi, int& rotation,
                                int& orientation) {
    Vec<PosixPageRange> ranges;
    int parity = 0;
    StrVec list;
    Split(&list, settings, StrL(","), true);
    for (Str s : list) {
        int from = 0;
        int to = 0;
        if (str::EqI(s, StrL("last"))) {
            VecAppend(ranges, PosixPageRange{pageCount, pageCount});
        } else if (!str::IsNull(str::Parse(s, "%d-%d%$", &from, &to))) {
            if (from < 0) {
                from = pageCount + from + 1;
            }
            if (to < 0) {
                to = pageCount + to + 1;
            }
            VecAppend(ranges, PosixPageRange{limitValue(from, 1, pageCount), limitValue(to, 1, pageCount)});
        } else if (!str::IsNull(str::Parse(s, "%d%$", &from))) {
            if (from < 0) {
                from = pageCount + from + 1;
            }
            from = limitValue(from, 1, pageCount);
            VecAppend(ranges, PosixPageRange{from, from});
        } else if (str::EqI(s, StrL("even"))) {
            parity = 2;
        } else if (str::EqI(s, StrL("odd"))) {
            parity = 1;
        } else if (str::EqI(s, StrL("portrait"))) {
            orientation = -1;
        } else if (str::EqI(s, StrL("landscape"))) {
            orientation = 1;
        } else if (str::TrimPrefixI(s, StrL("rotate="))) {
            int deg = 0;
            if (!str::IsNull(str::Parse(s, "%d%$", &deg))) {
                deg = ((deg % 360) + 360) % 360;
                if (deg == 90 || deg == 180 || deg == 270) {
                    rotation = deg;
                }
            }
        } else if (str::TrimPrefixI(s, StrL("dpi="))) {
            float value = 0;
            if (!str::IsNull(str::Parse(s, "%f%$", &value)) && value > 0) {
                dpi = value;
            }
        }
    }
    if (len(ranges) == 0) {
        VecAppend(ranges, PosixPageRange{1, pageCount});
    }
    for (const PosixPageRange& range : ranges) {
        int dir = range.from > range.to ? -1 : 1;
        for (int pageNo = range.from; pageNo != range.to + dir; pageNo += dir) {
            if (parity && pageNo % 2 != parity) {
                continue;
            }
            VecAppend(pages, pageNo);
        }
    }
}

static void AppendLpOption(StrVec& args, Str value) {
    args.Append(StrL("-o"));
    args.Append(value);
}

// maps the driver-level subset of -print-settings to standard CUPS options
static void AppendLpSettings(StrVec& args, Str settings) {
    StrVec list;
    Split(&list, settings, StrL(","), true);
    for (Str s : list) {
        int copies = 0;
        if (!str::IsNull(str::Parse(s, "%dx%$", &copies))) {
            copies = limitValue(copies, 1, 9999);
            args.Append(StrL("-n"));
            args.Append(fmt("%d", copies));
        } else if (str::EqI(s, StrL("simplex"))) {
            AppendLpOption(args, StrL("sides=one-sided"));
        } else if (str::EqI(s, StrL("duplex")) || str::EqI(s, StrL("duplexlong"))) {
            AppendLpOption(args, StrL("sides=two-sided-long-edge"));
        } else if (str::EqI(s, StrL("duplexshort"))) {
            AppendLpOption(args, StrL("sides=two-sided-short-edge"));
        } else if (str::EqI(s, StrL("color"))) {
            AppendLpOption(args, StrL("print-color-mode=color"));
        } else if (str::EqI(s, StrL("monochrome"))) {
            AppendLpOption(args, StrL("print-color-mode=monochrome"));
        } else if (str::EqI(s, StrL("collate"))) {
            AppendLpOption(args, StrL("Collate=True"));
        } else if (str::EqI(s, StrL("nocollate"))) {
            AppendLpOption(args, StrL("Collate=False"));
        } else if (str::EqI(s, StrL("fit"))) {
            AppendLpOption(args, StrL("fit-to-page"));
        } else if (str::EqI(s, StrL("shrink"))) {
            AppendLpOption(args, StrL("print-scaling=auto-fit"));
        } else if (str::EqI(s, StrL("noscale"))) {
            AppendLpOption(args, StrL("scaling=100"));
        } else if (str::EqI(s, StrL("center"))) {
            AppendLpOption(args, StrL("position=center"));
        } else if (str::TrimPrefixI(s, StrL("paper="))) {
            AppendLpOption(args, fmt("media=%s", s));
        } else if (str::TrimPrefixI(s, StrL("bin="))) {
            AppendLpOption(args, fmt("InputSlot=%s", s));
        } else if (str::TrimPrefixI(s, StrL("docname="))) {
            args.Append(StrL("-t"));
            args.Append(s);
        }
    }
}

// submits a generated PDF synchronously so the temporary file can be removed
static bool SendPdfToPrinter(Str path, Str printerName, Str settings) {
#if OS_WASM
    (void)path;
    (void)printerName;
    (void)settings;
    return false;
#else
    bool useDefault = str::Eq(printerName, Str(kDefaultPrinterToken));
    StrVec args;
    args.Append(StrL("lp"));
    if (!useDefault) {
        args.Append(StrL("-d"));
        args.Append(printerName);
    }
    AppendLpSettings(args, settings);
    args.Append(path);
    auto** argv = AllocArrayTemp<char*>(len(args) + 1);
    for (int i = 0; i < len(args); i++) {
        argv[i] = CStrTemp(args[i]);
    }
    argv[len(args)] = nullptr;

    pid_t pid = 0;
    int err = posix_spawnp(&pid, argv[0], nullptr, nullptr, argv, environ);
    if (err != 0) {
        logf("SendPdfToPrinter: posix_spawnp failed with %d\n", err);
        return false;
    }
    int status = 0;
    do {
        err = waitpid(pid, &status, 0) < 0 ? errno : 0;
    } while (err == EINTR);
    if (err != 0) {
        logf("SendPdfToPrinter: waitpid failed with %d\n", err);
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

PrintResult PrintFile2(EngineBase* engine, Str printerName, bool displayErrors, Str settings) {
    if (!HasPermission(Perm::PrinterAccess)) {
        return PrintResult::NoPermission;
    }
    if (!engine) {
        logf("PrintFile2: engine is null\n");
        return PrintResult::CannotLoadFile;
    }
#ifndef DISABLE_DOCUMENT_RESTRICTIONS
    if (!engine->AllowsPrinting()) {
        logf("PrintFile2: printing not allowed by the document\n");
        return PrintResult::PrintingNotAllowed;
    }
#endif
    (void)displayErrors;
    TempStr destPath = OutputPathFromSettingsTemp(settings);
    bool sendToPrinter = !OS_WASM && len(destPath) == 0 && len(printerName) > 0;
    if (sendToPrinter) {
        destPath = GetTempFilePathTemp(StrL("sumatra-print-"));
    } else if (len(destPath) == 0) {
        Str filePath = engine->FilePath();
        if (len(filePath) == 0) {
            return PrintResult::PrintFailed;
        }
        destPath = fmt("%s.print.pdf", filePath);
    }
    logf("PrintFile2: printing '%s' to '%s'\n", engine->FilePath(), destPath);
    Vec<int> pages;
    float dpi = kPrintToPdfDpi;
    int rotation = 0;
    int orientation = 0;
    ApplyRenderSettings(settings, engine->PageCount(), pages, dpi, rotation, orientation);
    if (!PrintEngineToPdf(engine, destPath, nullptr, rotation, &pages, dpi, orientation)) {
        if (sendToPrinter) {
            file::Delete(destPath);
        }
        return PrintResult::PrintFailed;
    }
    if (sendToPrinter) {
        bool ok = SendPdfToPrinter(destPath, printerName, settings);
        file::Delete(destPath);
        return ok ? PrintResult::Ok : PrintResult::PrintFailed;
    }
    return PrintResult::Ok;
}

PrintResult PrintFile(Str fileName, Str printerName, bool displayErrors, Str settings) {
    logf("PrintFile: file: '%s', printer: '%s'\n", fileName, printerName);
    fileName = path::NormalizeTemp(fileName);
    EngineBase* engine = CreateEngineFromFile(fileName, nullptr, true);
    if (!engine) {
        logf("PrintFile: couldn't open '%s'\n", fileName);
        return PrintResult::CannotLoadFile;
    }
    PrintResult res = PrintFile2(engine, printerName, displayErrors, settings);
    SafeEngineRelease(&engine);
    return res;
}

struct PrintToPdfCtx {
    MainWindow* win;
    EngineBase* engine;
    Vec<SelectionOnPage>* selection;
    int rotation;
};

static void OnPrintPathPicked(PrintToPdfCtx* ctx, SavePathArgs* args) {
    MainWindow* win = ctx->win;
    EngineBase* engine = ctx->engine;
    Vec<SelectionOnPage>* selection = ctx->selection;
    int rotation = ctx->rotation;
    delete ctx;
    bool canceled = len(args->path) == 0;
    bool ok = !canceled && PrintEngineToPdf(engine, args->path, selection, rotation);
    logf("PrintCurrentFile: wrote '%s', ok=%d\n", args->path, (int)ok);
    SafeEngineRelease(&engine);
    delete selection;
    if (canceled || !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (!ok) {
        MessageBoxWarning(win, Tr("Couldn't initialize printer"), Tr("Printing problem."));
    }
}

void PrintCurrentFile(MainWindow* win, bool waitForCompletion, bool selectionByDefault) {
    if (!HasPermission(Perm::PrinterAccess) || !win->IsDocLoaded()) {
        return;
    }
    if (win->AsChm()) {
        win->AsChm()->PrintCurrentPage(true);
        return;
    }
    if (win->AsMarkdown()) {
        win->AsMarkdown()->PrintCurrentPage(true);
        return;
    }
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return;
    }
#ifndef DISABLE_DOCUMENT_RESTRICTIONS
    if (!engine->AllowsPrinting()) {
        return;
    }
#endif
    TempStr destPath = fmt("%s.print.pdf", engine->FilePath());
    Vec<SelectionOnPage>* selection = nullptr;
    if (selectionByDefault && win->CurrentTab()->selectionOnPage) {
        selection = new Vec<SelectionOnPage>();
        for (const SelectionOnPage& sel : *win->CurrentTab()->selectionOnPage) {
            VecAppend(*selection, sel);
        }
    }
    int rotation = dm->GetRotation();
    if (waitForCompletion) {
        // -print-dialog -exit-when-done has nobody to answer the dialog
        PrintEngineToPdf(engine, destPath, selection, rotation);
        delete selection;
        return;
    }
#if OS_WASM
    TempStr printPath = GetTempFilePathTemp(StrL("sumatra-print-"));
    bool ok = PrintEngineToPdf(engine, printPath, selection, rotation) && WasmPrintPdf(printPath);
    file::Delete(printPath);
    delete selection;
    if (!ok) {
        MessageBoxWarning(win, Tr("Couldn't initialize printer"), Tr("Printing problem."));
    }
    return;
#endif
    engine->AddRef();
    auto* ctx = new PrintToPdfCtx{win, engine, selection, rotation};
    auto* args = new SavePathArgs();
    args->win = win;
    args->title = str::Dup(Tr("Print"));
    args->initialPath = str::Dup(destPath);
    args->defExt = str::Dup(StrL(".pdf"));
    args->onDone = MkFunc1(OnPrintPathPicked, ctx);
    ShowSavePathDialog(args);
}

void AbortPrinting(MainWindow*) {
    // nothing runs on a thread here: the PDF is written inline
}
