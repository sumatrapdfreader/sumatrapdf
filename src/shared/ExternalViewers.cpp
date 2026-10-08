/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/Launch.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "SumatraPDF.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "Commands.h"
#include "Translations.h"
#ifdef SUMATRA_NG
#include "SumatraDialogs.h"
#include "gui/WasmBridge.h"
#endif
#include "ExternalViewers.h"

struct ExternalViewerInfo {
    Str name; // shown to the user
    int cmdId;
    Str exts; // valid extensions
    Str exePartialPath;
    Str launchArgs;
    Kind engineKind;
    // set by DetectExternalViewers()
    Str exeFullPath; // if found, full path to the executable (heap-owned)
};

// kindEngineChm

static int gExternalViewersCount = 0;

// clang-format off
static ExternalViewerInfo gExternalViewers[] = {
    {
        StrL("Explorer"),
        CmdOpenWithExplorer,
        StrL("*"),
        StrL("explorer.exe"),
        StrL(R"(/select,"%1")"),
        {},
        Str{},
    },
    {
        StrL("Directory Opus"),
        CmdOpenWithDirectoryOpus,
        StrL("*"),
        StrL(R"(GPSoftware\Directory Opus\dopus.exe)"),
        StrL(R"("%d")"),
        {},
        Str{},
    },
    {
        StrL("Total Commander"),
        CmdOpenWithTotalCommander,
        StrL("*"),
        StrL(R"(totalcmd\TOTALCMD64.EXE)"),
        StrL(R"("%d")"),
        {},
        Str{},
    },
    {
        StrL("Double Commander"),
        CmdOpenWithDoubleCommander,
        StrL("*"),
        StrL(R"(Double Commander\doublecmd.exe)"),
        StrL(R"(--no-splash --client "%d")"),
        {},
        Str{},
    },
    {
        StrL("Acrobat Reader"),
        CmdOpenWithAcrobat,
        StrL(".pdf"),
        StrL(R"(Adobe\Acrobat Reader DC\Reader\AcroRd32.exe)"),
        // Command line format for version 6 and later:
        //   /A "page=%d&zoom=%.1f,%d,%d&..." <filename>
        // see http://www.adobe.com/devnet/acrobat/pdfs/pdf_open_parameters.pdf#page=5
        // zoom=%z,%x,%y : percentage zoom and upper-left view (user-space coords)
        StrL(R"(/A "page=%p&zoom=%z,%x,%y" "%1")"),
        kindEngineMupdf,
        Str{}
    },
    {
        StrL("Acrobat Reader"),
        CmdOpenWithAcrobat,
        StrL(".pdf"),
        StrL(R"(Adobe\Acrobat DC\Acrobat\Acrobat.exe)"),
        // Command line format for version 6 and later:
        //   /A "page=%d&zoom=%.1f,%d,%d&..." <filename>
        // see http://www.adobe.com/devnet/acrobat/pdfs/pdf_open_parameters.pdf#page=5
        StrL(R"(/A "page=%p&zoom=%z,%x,%y" "%1")"),
        kindEngineMupdf,
        Str{}
    },
    {
        StrL("Foxit Reader"),
        CmdOpenWithFoxit,
        StrL(".pdf"),
        StrL(R"(Foxit Software\Foxit Reader\FoxitReader.exe)"),
        // Foxit: filename [-n page] [-z zoom]
        StrL(R"("%1" /A page=%p -z %z)"),
        kindEngineMupdf,
        Str{}
    },
    {
        StrL("Foxit PhantomPDF"),
        CmdOpenWithFoxitPhantom,
        StrL(".pdf"),
        StrL(R"(Foxit Software\Foxit PhantomPDF\FoxitPhantomPDF.exe)"),
        StrL(R"("%1" /A page=%p -z %z)"),
        kindEngineMupdf,
        Str{}
    },
    {
        StrL("PDF-XChange Editor"),
        CmdOpenWithPdfXchange,
        StrL(".pdf"),
        StrL(R"(Tracker Software\PDF Editor\PDFXEdit.exe)"),
        // PDFXChange cmd-line format:
        // [/A "param=value [&param2=value ..."] [PDF filename]
        StrL(R"(/A "page=%p&zoom=%z" "%1")"),
        kindEngineMupdf,
        Str{}
    },
    {
        StrL("Pdf & Djvu Bookmarker"),
        CmdOpenWithPdfDjvuBookmarker,
        StrL(".pdf;.djvu"),
        StrL(R"(Pdf & Djvu Bookmarker\PdfDjvuBookmarker.exe)"),
        Str{},
        {},
        Str{}
    },
    {
        StrL("XPS Viewer"),
        CmdOpenWithXpsViewer,
        StrL(".xps;.oxps"),
        StrL("xpsrchvw.exe"),
        Str{},
        kindEngineMupdf,
        Str{}
    },
    {
        StrL("HTML Help"),
        CmdOpenWithHtmlHelp,
        StrL(".chm"),
        StrL("hh.exe"),
        Str{},
        kindEngineChm,
        Str{}
    }
};
// clang-format on

// clang-format off
const int gOpenWithKnownExternalViewerCmds[] = {
    CmdOpenWithExplorer,
    CmdOpenWithDirectoryOpus,
    CmdOpenWithTotalCommander,
    CmdOpenWithDoubleCommander,
    CmdOpenWithAcrobat,
    CmdOpenWithFoxit,
    CmdOpenWithFoxitPhantom,
    CmdOpenWithPdfXchange,
    CmdOpenWithXpsViewer,
    CmdOpenWithHtmlHelp,
    CmdOpenWithPdfDjvuBookmarker,
    0,
};
// clang-format on

bool IsOpenWithKnownExternalViewerCmd(int cmdId) {
    for (int i = 0; gOpenWithKnownExternalViewerCmds[i]; i++) {
        if (gOpenWithKnownExternalViewerCmds[i] == cmdId) {
            return true;
        }
    }
    return false;
}

// a Shortcuts / toolbar entry is a clone with its own id, so it's the command
// it stands for that decides
bool IsOpenWithKnownExternalViewerCmd(CustomCommand* cmd) {
    return cmd && IsOpenWithKnownExternalViewerCmd(cmd->origId);
}

static ExternalViewerInfo* FindKnownExternalViewerInfoByCmdId(int cmdId) {
    for (ExternalViewerInfo& ev : gExternalViewers) {
        if (ev.cmdId == cmdId) {
            return &ev;
        }
    }
    return nullptr;
}

bool HasKnownExternalViewerForCmd(int cmdId) {
    ExternalViewerInfo* info = FindKnownExternalViewerInfoByCmdId(cmdId);
    return info && info->exeFullPath;
}

static bool CanViewExternally(WindowTab* tab) {
    if (!CanAccessDisk()) {
        return false;
    }
    // if tab is nullptr, we're queried for the
    // About window with disabled menu items
    if (!tab) {
        return true;
    }
    return file::Exists(tab->filePath);
}

void FreeExternalViewers() {
    for (ExternalViewerInfo& info : gExternalViewers) {
        str::Free(info.exeFullPath);
        info.exeFullPath = {};
    }
}

#if OS_WIN
static TempStr GetAcrobatPathTemp() {
    // Try Adobe Acrobat as a fall-back, if the Reader isn't installed
    Str keyName = StrL(R"(Software\Microsoft\Windows\CurrentVersion\App Paths\AcroRd32.exe)");
    TempStr path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, {});
    if (len(path) == 0) {
        keyName = StrL(R"(Software\Microsoft\Windows\CurrentVersion\App Paths\Acrobat.exe)");
        path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, {});
    }
    if (path && file::Exists(path)) {
        return path;
    }
    return {};
}

static TempStr GetFoxitPathTemp() {
    Str keyName = StrL(R"(Software\Microsoft\Windows\CurrentVersion\Uninstall\Foxit Reader)");
    TempStr path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, StrL("DisplayIcon"));
    if (path && file::Exists(path)) {
        return path;
    }
    // Registry value for Foxit 5 (and maybe later)
    keyName = StrL(R"(Software\Microsoft\Windows\CurrentVersion\Uninstall\Foxit Reader_is1)");
    path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, StrL("DisplayIcon"));
    if (path && file::Exists(path)) {
        return path;
    }
    // Registry value for Foxit 5.5 MSI installer
    keyName = StrL(R"(Software\Foxit Software\Foxit Reader)");
    path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, StrL("InstallPath"));
    if (path) {
        path = path::JoinTemp(path, StrL("Foxit Reader.exe"));
    }
    if (path && file::Exists(path)) {
        return path;
    }
    // Registry value for Foxit PDF Reader 12.1.3.15356 (The last version with Add Bookmark function without bugs in
    // single-key accelerator)
    keyName = StrL(R"(SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\FoxitPDFReader.exe)");
    path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, StrL("Path"));
    if (path) {
        path = path::JoinTemp(path, StrL("FoxitPDFReader.exe"));
    }
    if (path && file::Exists(path)) {
        return path;
    }
    return {};
}

static TempStr GetAppPathExeTemp(Str exeName) {
    TempStr keyName = fmt(R"(Software\Microsoft\Windows\CurrentVersion\App Paths\%s)", exeName);
    TempStr path = ReadRegStr2Temp(keyName, {});
    if (path && file::Exists(path)) {
        return path;
    }
    return {};
}

static TempStr GetRegisteredOpenExeTemp(Str progId) {
    TempStr keyName = fmt(R"(%s\shell\open\command)", progId);
    TempStr command = ReadRegStrTemp(HKEY_CLASSES_ROOT, keyName, {});
    if (len(command) == 0) {
        return {};
    }
    StrNode* args = ParseCmdLine(command);
    defer {
        FreeStrNode(nullptr, args);
    };
    if (!args || !file::Exists(args->s)) {
        return {};
    }
    return str::DupTemp(args->s);
}

static TempStr FindPDFXChangeInProgramDirsTemp(const StrVec& programDirs) {
    Str partialPaths[] = {
        StrL(R"(PDF-XChange\PDF Editor\PXCEditor.exe)"),
        StrL(R"(Tracker Software\PDF Editor\PXCEditor.exe)"),
        StrL(R"(PDF-XChange\PDF Editor\PDFXEdit.exe)"),
        StrL(R"(Tracker Software\PDF Editor\PDFXEdit.exe)"),
    };
    for (Str partialPath : partialPaths) {
        for (Str dir : programDirs) {
            TempStr exePath = path::JoinTemp(dir, partialPath);
            if (file::Exists(exePath)) {
                return exePath;
            }
        }
    }
    return {};
}

static TempStr GetPDFXChangePathTemp() {
    // V11 renamed both the vendor directory and the executable. Prefer paths
    // registered by the installer, then cover clean and upgraded installations.
    TempStr exePath = GetAppPathExeTemp(StrL("PXCEditor.exe"));
    if (len(exePath) == 0) {
        exePath = GetAppPathExeTemp(StrL("PDFXEdit.exe"));
    }
    if (len(exePath) == 0) {
        exePath = GetRegisteredOpenExeTemp(StrL("PXCEditor.PDF"));
    }
    if (len(exePath) == 0) {
        exePath = GetRegisteredOpenExeTemp(StrL("PDFXEdit.PDF"));
    }
    if (exePath) {
        return exePath;
    }

    StrVec programDirs;
    int csidls[] = {CSIDL_PROGRAM_FILES, CSIDL_PROGRAM_FILESX86};
    for (int csidl : csidls) {
        TempStr dir = GetSpecialFolderTemp(csidl);
        if (dir) {
            programDirs.Append(dir);
        }
    }
    exePath = FindPDFXChangeInProgramDirsTemp(programDirs);
    if (exePath) {
        return exePath;
    }

    // Legacy PDF-XChange Viewer registry entry.
    Str keyName = StrL(R"(Software\Tracker Software\PDFViewer)");
    TempStr path = ReadRegStr2Temp(keyName, StrL("InstallPath"));
    if (len(path) == 0) {
        return {};
    }
    exePath = path::JoinTemp(path, StrL("PDFXCview.exe"));
    if (file::Exists(exePath)) {
        return exePath;
    }
    return {};
}

#if IS_DEBUG
bool ExternalViewers_UnitTestPDFXChangePaths() {
    TempStr testDir = GetTempFilePathTemp(StrL("issue-5941"));
    if (len(testDir) == 0 || !file::Delete(testDir) || !dir::Create(testDir)) {
        return false;
    }
    defer {
        dir::RemoveAll(testDir);
    };

    StrVec programDirs;
    programDirs.Append(testDir);
    Str partialPaths[] = {
        StrL(R"(PDF-XChange\PDF Editor\PXCEditor.exe)"),
        StrL(R"(Tracker Software\PDF Editor\PXCEditor.exe)"),
        StrL(R"(Tracker Software\PDF Editor\PDFXEdit.exe)"),
    };
    for (Str partialPath : partialPaths) {
        TempStr expected = path::JoinTemp(testDir, partialPath);
        if (!dir::CreateForFile(expected) || !file::WriteFile(expected, StrL("test"))) {
            return false;
        }
        TempStr found = FindPDFXChangeInProgramDirsTemp(programDirs);
        if (len(found) == 0 || !path::IsSame(found, expected) || !file::Delete(expected)) {
            return false;
        }
    }
    return true;
}
#endif

static void SetKnownExternalViewerExePath(int cmdId, Str exePath) {
    if (len(exePath) == 0) {
        return;
    }
    ExternalViewerInfo* info = FindKnownExternalViewerInfoByCmdId(cmdId);
    if (info && len(info->exeFullPath) == 0) {
        info->exeFullPath = str::Dup(exePath);
    }
}

static bool DetectExternalViewer(ExternalViewerInfo* ev) {
    if (ev->exeFullPath) {
        return true;
    }
    if (len(ev->exePartialPath) == 0) {
        return false;
    }

    static int const csidls[] = {CSIDL_PROGRAM_FILES, CSIDL_PROGRAM_FILESX86, CSIDL_WINDOWS, CSIDL_SYSTEM};
    for (int csidl : csidls) {
        TempStr dir = GetSpecialFolderTemp(csidl);
        TempStr path = path::JoinTemp(dir, ev->exePartialPath);
        if (file::Exists(path)) {
            ev->exeFullPath = str::Dup(path);
            return true;
        }
    }
    return false;
}

void DetectExternalViewers() {
    ReportIf(gExternalViewersCount > 0); // only call once

    if (!CanAccessDisk()) {
        return;
    }

    TempStr exePath = GetPDFXChangePathTemp();
    SetKnownExternalViewerExePath(CmdOpenWithPdfXchange, exePath);

    for (ExternalViewerInfo& i : gExternalViewers) {
        if (DetectExternalViewer(&i)) {
            gExternalViewersCount++;
        }
    }

    exePath = GetAcrobatPathTemp();
    SetKnownExternalViewerExePath(CmdOpenWithAcrobat, exePath);

    exePath = GetFoxitPathTemp();
    SetKnownExternalViewerExePath(CmdOpenWithFoxit, exePath);
}
#else
void DetectExternalViewers() {
    (void)gExternalViewersCount;
}
#endif

static bool filterMatchesEverything(Str ext) {
    return str::IsEmptyOrWhiteSpace(ext) || str::EqIS(ext, StrL("*"));
}

bool CanViewWithKnownExternalViewer(WindowTab* tab, int cmdId) {
    if (!tab || !CanViewExternally(tab)) {
        return false;
    }
    ExternalViewerInfo* ev = FindKnownExternalViewerInfoByCmdId(cmdId);
    if (!ev || len(ev->exeFullPath) == 0) {
        return false;
    }
    // must match file extension

    if (!filterMatchesEverything(ev->exts)) {
        TempStr ext = path::GetExtTemp(tab->filePath);
        if (!str::ContainsI(ev->exts, ext)) {
            return false;
        }
    }
    Kind engineKind = tab->GetEngineType();
    if (engineKind != nullptr) {
        if (ev->engineKind != nullptr) {
            if (ev->engineKind != engineKind) {
                logf("CanViewWithKnownExternalViewer cmd: %d, ev->engineKind '%s' != engineKind '%s'\n", cmdId,
                     Str(ev->engineKind), Str(engineKind));
                return false;
            }
        }
    }
    return true;
}

bool CouldBePDFDoc(WindowTab* tab) {
    // consider any error state a potential PDF document
    return !tab || !tab->ctrl || tab->GetEngineType() == kindEngineMupdf;
}

bool IsPdfDoc(WindowTab* tab) {
    if (!tab || !tab->ctrl) {
        return true;
    }
    if (tab->GetEngineType() != kindEngineMupdf) {
        return false;
    }
    DisplayModel* dm = tab->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    return !engine || EngineMupdfIsPdf(engine);
}

// substitutions in cmdLine:
//  %1 : file path (else the file path is appended)
//  %d : directory in which file is
//  %p : current page number
//  %z : zoom as percentage (100 = 100%; fit modes → 100)
//  %x : horizontal scroll position on the page (user-space; 0 if not scrolled)
//  %y : vertical scroll position on the page (user-space; 0 if not scrolled)
//  %% : a literal '%' (so e.g. "%%d" reaches the external program as "%d",
//       useful for tools like `mutool draw -o page-%d.png` -- see #5583)
// any other "%x" sequence is passed through unchanged.
// Note: substituted values (path, dir) are inserted literally and not
// re-scanned, so a '%' inside a file path can't trigger another substitution.
static TempStr FormatParamTemp(Str arg, WindowTab* tab) {
    Str path = tab->filePath ? tab->filePath : StrL("");

    // Zoom/scroll for Adobe-style open parameters (page=%p&zoom=%z,%x,%y).
    float zoomPct = 100.f;
    int scrollX = 0;
    int scrollY = 0;
    if (tab->ctrl) {
        float zv = tab->ctrl->GetZoomVirtual(true);
        if (zv > 0) {
            zoomPct = zv;
        }
        DisplayModel* dm = tab->AsFixed();
        if (dm) {
            ScrollState ss = dm->GetScrollState();
            if (ss.x >= 0) {
                scrollX = (int)ss.x;
            }
            if (ss.y >= 0) {
                scrollY = (int)ss.y;
            }
        }
    }

    str::Builder out;
    for (int i = 0; i < arg.len; i++) {
        if (arg.s[i] != '%') {
            out.AppendChar(arg.s[i]);
            continue;
        }
        if (i + 1 >= arg.len) {
            out.AppendChar('%');
            break;
        }
        switch (arg.s[i + 1]) {
            case '%':
                out.AppendChar('%'); // %% -> literal %
                i++;
                break;
            case '1':
                // TODO: if %1 is un-quoted, we should quote it but it's complicated because
                // it could be part of a pattern like %1.Page%p.txt
                // (as in https://github.com/sumatrapdfreader/sumatrapdf/issues/3868)
                out.Append(path);
                i++;
                break;
            case 'd':
                out.Append(path::GetDirTemp(path));
                i++;
                break;
            case 'p':
                out.Append(fmt("%d", tab->ctrl ? tab->ctrl->CurrentPageNo() : 0));
                i++;
                break;
            case 'z':
                out.Append(fmt("%.1f", zoomPct));
                i++;
                break;
            case 'x':
                out.Append(fmt("%d", scrollX));
                i++;
                break;
            case 'y':
                out.Append(fmt("%d", scrollY));
                i++;
                break;
            default:
                // unknown (or trailing) '%': leave it literal and keep scanning
                out.AppendChar('%');
                break;
        }
    }
    return ToStrTemp(out);
}

static TempStr GetDocumentPathQuoted(WindowTab* tab) {
    auto path = tab->filePath;
    return str::JoinTemp(StrL("\""), path, StrL("\""));
}

bool ViewWithKnownExternalViewer(WindowTab* tab, int cmdId) {
    bool canView = CanViewWithKnownExternalViewer(tab, cmdId);
    if (!canView) {
        logf("ViewWithKnownExternalViewer cmd: %d\n", cmdId);
        // with command palette can send un-enforcable command so not ReportIf
        ReportDebugIf(!canView);
        return false;
    }
    ExternalViewerInfo* ev = FindKnownExternalViewerInfoByCmdId(cmdId);
    if (len(ev->exeFullPath) == 0) {
        return false;
    }
    TempStr args;
    if (ev->launchArgs) {
        args = FormatParamTemp(ev->launchArgs, tab);
    } else {
        args = GetDocumentPathQuoted(tab);
    }
    return LaunchFileShell(ev->exeFullPath, args);
}

bool PathMatchFilter(Str path, Str filter) {
    if (filterMatchesEverything(filter)) {
        return true;
    }
    return path::Match(path, filter);
}

// TODO: find a better file for this?
// extract the executable (first token) from cmdLine, honoring a leading quote,
// and set *restOut to the remaining command line (after the exe and any spaces)
static TempStr ExtractExePathTemp(Str cmdLine, Str* restOut) {
    Str s = cmdLine;
    str::TrimChar(s, ' ');
    str::Builder exe;
    if (len(s) > 0 && s.s[0] == '"') {
        s = Str(s.s + 1, s.len - 1);
        int i = 0;
        for (; i < s.len && s.s[i] != '"'; i++) {
            exe.AppendChar(s.s[i]);
        }
        s = Str(s.s + (i < s.len ? i + 1 : i), s.len - (i < s.len ? i + 1 : i));
    } else {
        int i = 0;
        for (; i < s.len && s.s[i] != ' '; i++) {
            exe.AppendChar(s.s[i]);
        }
        s = Str(s.s + i, s.len - i);
    }
    str::TrimChar(s, ' ');
    *restOut = s;
    return ToStrTemp(exe);
}

bool RunWithExe(WindowTab* tab, Str cmdLine, Str filter) {
    if (!tab) {
        return false;
    }
    if (!PathMatchFilter(tab->filePath, filter)) {
        return false;
    }
    if (str::IsEmptyOrWhiteSpace(cmdLine)) {
        return false;
    }
    // Split into the exe (first token) and the rest of the command line, then do
    // the %1/%p/%d/%% substitution on the rest WITHOUT re-tokenizing and
    // re-quoting it, so the user's own quoting is preserved (issue #5695).
    // Re-quoting moved/mangled quotes and broke e.g. cmd.exe command lines.
    // This matches how the known external viewers are launched (FormatParamTemp
    // on the raw args). The user is responsible for quoting arguments that
    // contain spaces, e.g. "%1".
    Str rest;
    TempStr exePath = ExtractExePathTemp(cmdLine, &rest);
    if (len(exePath) == 0) {
        return false;
    }
    // TODO: this should be in ViewWithCustomExternalViewer()
    if (!file::Exists(exePath)) {
        TempStr msg =
            fmt("External viewer executable not found: %s. Fix ExternalViewers in advanced settings.", exePath);
        auto caption = Tr("Error");
#ifdef SUMATRA_NG
        MsgBox(tab ? tab->win : nullptr, msg, caption, MbOk | MbIconError);
#else
        MsgBox(nullptr, msg, caption, MB_OK | MB_ICONERROR);
#endif
        return false;
    }
    if (str::IsEmptyOrWhiteSpace(rest)) {
        // no arguments given: pass the document path as the only argument
        return LaunchFileShell(exePath, tab->filePath);
    }
    TempStr params = FormatParamTemp(rest, tab);
    return LaunchFileShell(exePath, params);
}

// --- send as e-mail attachment (step 14a) -----------------------------------

#if OS_WIN

static bool IsMapiSendMailAvailable() {
    HMODULE hMapi = LoadLibraryW(L"mapi32.dll");
    if (!hMapi) {
        return false;
    }
    bool ok = GetProcAddress(hMapi, "MAPISendMailW") != nullptr;
    FreeLibrary(hMapi);
    return ok;
}

static bool IsEmailAttachmentSendAvailable() {
    static int cached = -1;
    if (cached >= 0) {
        return cached != 0;
    }
    // note: don't gate on IsRunningOnWine() here - Wine implements MAPISendMailW
    // (routing to the configured mailer), and the Wine heuristic has
    // false-positives that would wrongly hide the feature for real users
    bool ok = IsMapiSendMailAvailable();
    cached = ok ? 1 : 0;
    return ok;
}

// Use MAPISendMailW to send email with attachment.
// Works with Outlook, Thunderbird and other MAPI-registered email clients.
static bool SendAsEmailAttachmentWithMapi(HWND hwndParent, Str filePath) {
    HMODULE hMapi = LoadLibraryW(L"mapi32.dll");
    if (!hMapi) {
        return false;
    }

    // MapiFileDescW and MapiMessageW structs matching Windows SDK definitions
    struct MapiFileDescW {
        ULONG ulReserved;
        ULONG flFlags;
        ULONG nPosition;
        PWSTR lpszPathName;
        PWSTR lpszFileName;
        PVOID lpFileType;
    };

    struct MapiMessageW {
        ULONG ulReserved;
        PWSTR lpszSubject;
        PWSTR lpszNoteText;
        PWSTR lpszMessageType;
        PWSTR lpszDateReceived;
        PWSTR lpszConversationID;
        ULONG flFlags;
        PVOID lpOriginator;
        ULONG nRecipCount;
        PVOID lpRecips;
        ULONG nFileCount;
        MapiFileDescW* lpFiles;
    };

    using MAPISendMailWFn = ULONG(WINAPI*)(ULONG_PTR, ULONG_PTR, MapiMessageW*, ULONG, ULONG);
    auto fnSendMailW = (MAPISendMailWFn)GetProcAddress(hMapi, "MAPISendMailW");
    if (!fnSendMailW) {
        FreeLibrary(hMapi);
        return false;
    }

    WCHAR* filePathW = CWStrTemp(filePath);
    TempStr fileName = path::GetBaseNameTemp(filePath);
    WCHAR* fileNameW = CWStrTemp(fileName);

    MapiFileDescW fileDesc{};
    fileDesc.nPosition = (ULONG)-1;
    fileDesc.lpszPathName = filePathW;
    fileDesc.lpszFileName = fileNameW;

    MapiMessageW msg{};
    msg.nFileCount = 1;
    msg.lpFiles = &fileDesc;

    constexpr ULONG kMapiDialog = 0x8;
    constexpr ULONG kMapiLogonUI = 0x1;
    ULONG result = fnSendMailW(0, (ULONG_PTR)hwndParent, &msg, kMapiDialog | kMapiLogonUI, 0);

    FreeLibrary(hMapi);
    // SUCCESS_SUCCESS = 0, MAPI_E_USER_ABORT = 1
    return result <= 1;
}

#else

// ng: no MAPI off Windows. Linux's xdg-email supports attachments; other
// desktops get a mailto naming the file so the user can attach it.
static bool IsEmailAttachmentSendAvailable() {
    return true;
}

#if OS_LINUX
static Str XdgEmailPath() {
    static const Str paths[] = {StrL("/usr/bin/xdg-email"), StrL("/bin/xdg-email")};
    for (Str path : paths) {
        if (file::Exists(path)) {
            return path;
        }
    }
    return {};
}

static bool SendAsEmailAttachmentWithXdg(Str filePath) {
    Str exe = XdgEmailPath();
    if (len(exe) == 0) {
        return false;
    }
    TempStr name = path::GetBaseNameTemp(filePath);
    TempStr params = fmt("--attach \"%s\" --subject \"%s\"", filePath, name);
    return LaunchFileShell(exe, params);
}
#endif

#if OS_DARWIN
static bool SendAsEmailAttachmentWithMail(Str filePath) {
    TempStr name = path::GetBaseNameTemp(filePath);
    StrVec args;
    args.Append(StrL("/usr/bin/osascript"));
    args.Append(StrL("-e"));
    args.Append(StrL("on run argv"));
    args.Append(StrL("-e"));
    args.Append(StrL("set attachmentFile to POSIX file (item 1 of argv)"));
    args.Append(StrL("-e"));
    args.Append(StrL("tell application \"Mail\""));
    args.Append(StrL("-e"));
    args.Append(
        StrL("set newMessage to make new outgoing message with properties {subject:(item 2 of argv), visible:true}"));
    args.Append(StrL("-e"));
    args.Append(StrL("tell content of newMessage"));
    args.Append(StrL("-e"));
    args.Append(StrL("make new attachment with properties {file name:attachmentFile} at after last paragraph"));
    args.Append(StrL("-e"));
    args.Append(StrL("end tell"));
    args.Append(StrL("-e"));
    args.Append(StrL("activate"));
    args.Append(StrL("-e"));
    args.Append(StrL("end tell"));
    args.Append(StrL("-e"));
    args.Append(StrL("end run"));
    args.Append(StrL("--"));
    args.Append(filePath);
    args.Append(name);
    return LaunchFileShellArgs(args);
}
#endif

static bool SendAsEmailAttachmentWithMailto(Str filePath) {
    TempStr name = path::GetBaseNameTemp(filePath);
    TempStr url = fmt("mailto:?subject=%s&body=%s", name, filePath);
    return LaunchBrowser(url);
}

#endif

bool CanSendAsEmailAttachment(WindowTab* tab) {
    // Requirements: a valid filename and a working way to send it
    if (!CanViewExternally(tab)) {
        return false;
    }
    return IsEmailAttachmentSendAvailable();
}

bool SendAsEmailAttachment(WindowTab* tab) {
    if (!tab || !CanSendAsEmailAttachment(tab)) {
        return false;
    }
#if OS_WIN
    return SendAsEmailAttachmentWithMapi(MainWindowHwnd(tab->win), tab->filePath);
#else
#if OS_LINUX
    if (SendAsEmailAttachmentWithXdg(tab->filePath)) {
        return true;
    }
#elif OS_DARWIN
    if (SendAsEmailAttachmentWithMail(tab->filePath)) {
        return true;
    }
#elif OS_WASM
    if (WasmShareFile(tab->filePath)) {
        return true;
    }
#endif
    return SendAsEmailAttachmentWithMailto(tab->filePath);
#endif
}
