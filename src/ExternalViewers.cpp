/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "SumatraPDF.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "Commands.h"
#include "Translations.h"

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
        "Explorer",
        CmdOpenWithExplorer,
        "*",
        "explorer.exe",
        R"(/select,"%1")",
        nullptr,
        Str{},
    },
    {
        "Directory Opus",
        CmdOpenWithDirectoryOpus,
        "*",
        R"(GPSoftware\Directory Opus\dopus.exe)",
        R"("%d")",
        nullptr,
        Str{},
    },
    {
        "Total Commander",
        CmdOpenWithTotalCommander,
        "*",
        R"(totalcmd\TOTALCMD64.EXE)",
        R"("%d")",
        nullptr,
        Str{},
    },
    {
        "Double Commander",
        CmdOpenWithDoubleCommander,
        "*",
        R"(Double Commander\doublecmd.exe)",
        R"(--no-splash --client "%d")",
        nullptr,
        Str{},
    },
    {
        "Acrobat Reader",
        CmdOpenWithAcrobat,
        ".pdf",
        R"(Adobe\Acrobat Reader DC\Reader\AcroRd32.exe)",
        // Command line format for version 6 and later:
        //   /A "page=%d&zoom=%.1f,%d,%d&..." <filename>
        // see http://www.adobe.com/devnet/acrobat/pdfs/pdf_open_parameters.pdf#page=5
        //   /P <filename>
        // see http://www.adobe.com/devnet/acrobat/pdfs/Acrobat_SDK_developer_faq.pdf#page=24
        // TODO: Also set zoom factor and scroll to current position?
        R"(/A page=%p "%1")",
        kindEngineMupdf,
        Str{}
    },
    {
        "Acrobat Reader",
        CmdOpenWithAcrobat,
        ".pdf",
        R"(Adobe\Acrobat DC\Acrobat\Acrobat.exe)",
        // Command line format for version 6 and later:
        //   /A "page=%d&zoom=%.1f,%d,%d&..." <filename>
        // see http://www.adobe.com/devnet/acrobat/pdfs/pdf_open_parameters.pdf#page=5
        //   /P <filename>
        // see http://www.adobe.com/devnet/acrobat/pdfs/Acrobat_SDK_developer_faq.pdf#page=24
        // TODO: Also set zoom factor and scroll to current position?
        R"(/A page=%p "%1")",
        kindEngineMupdf,
        Str{}
    },
    {
        "Foxit Reader",
        CmdOpenWithFoxIt,
        ".pdf",
        R"(Foxit Software\Foxit Reader\FoxitReader.exe)",
        // Foxit cmd-line format:
        // [PDF filename] [-n <page number>] [-pwd <password>] [-z <zoom>]
        // TODO: Foxit allows passing password and zoom
        R"("%1" /A page=%p)",
        kindEngineMupdf,
        Str{}
    },
    {
        "Foxit PhantomPDF",
        CmdOpenWithFoxItPhantom,
        ".pdf",
        R"(Foxit Software\Foxit PhantomPDF\FoxitPhantomPDF.exe)",
        R"("%1" /A page=%p)",
        kindEngineMupdf,
        Str{}
    },
    {
        "PDF-XChange Editor",
        CmdOpenWithPdfXchange,
        ".pdf",
        R"(Tracker Software\PDF Editor\PDFXEdit.exe)",
        // PDFXChange cmd-line format:
        // [/A "param=value [&param2=value ..."] [PDF filename]
        // /A params: page=<page number>
        R"(/A page=%p "%1")",
        kindEngineMupdf,
        Str{}
    },
    {
        "Pdf & Djvu Bookmarker",
        CmdOpenWithPdfDjvuBookmarker,
        ".pdf;.djvu",
        R"(Pdf & Djvu Bookmarker\PdfDjvuBookmarker.exe)",
        Str{},
        nullptr,
        Str{}
    },
    {
        "XPS Viewer",
        CmdOpenWithXpsViewer,
        ".xps;.oxps",
        "xpsrchvw.exe",
        Str{},
        kindEngineMupdf,
        Str{}
    },
    {
        "HTML Help",
        CmdOpenWithHtmlHelp,
        ".chm",
        "hh.exe",
        Str{},
        kindEngineChm,
        Str{}
    }
};
// clang-format on

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

static TempStr GetAcrobatPathTemp() {
    // Try Adobe Acrobat as a fall-back, if the Reader isn't installed
    Str keyName = R"(Software\Microsoft\Windows\CurrentVersion\App Paths\AcroRd32.exe)";
    TempStr path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, nullptr);
    if (!path) {
        keyName = R"(Software\Microsoft\Windows\CurrentVersion\App Paths\Acrobat.exe)";
        path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, nullptr);
    }
    if (path && file::Exists(path)) {
        return path;
    }
    return {};
}

static TempStr GetFoxitPathTemp() {
    Str keyName = R"(Software\Microsoft\Windows\CurrentVersion\Uninstall\Foxit Reader)";
    TempStr path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, "DisplayIcon");
    if (path && file::Exists(path)) {
        return path;
    }
    // Registry value for Foxit 5 (and maybe later)
    keyName = R"(Software\Microsoft\Windows\CurrentVersion\Uninstall\Foxit Reader_is1)";
    path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, "DisplayIcon");
    if (path && file::Exists(path)) {
        return path;
    }
    // Registry value for Foxit 5.5 MSI installer
    keyName = R"(Software\Foxit Software\Foxit Reader)";
    path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, "InstallPath");
    if (path) {
        path = path::JoinTemp(path, StrL("Foxit Reader.exe"));
    }
    if (path && file::Exists(path)) {
        return path;
    }
    // Registry value for Foxit PDF Reader 12.1.3.15356 (The last version with Add Bookmark function without bugs in
    // single-key accelerator)
    keyName = R"(SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\FoxitPDFReader.exe)";
    path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, "Path");
    if (path) {
        path = path::JoinTemp(path, StrL("FoxitPDFReader.exe"));
    }
    if (path && file::Exists(path)) {
        return path;
    }
    return {};
}

static TempStr GetPDFXChangePathTemp() {
    Str keyName = R"(Software\Tracker Software\PDFViewer)";
    TempStr path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, "InstallPath");
    if (!path) {
        keyName = R"(Software\Tracker Software\PDFViewer)";
        path = ReadRegStrTemp(HKEY_CURRENT_USER, keyName, "InstallPath");
    }
    if (!path) {
        return {};
    }
    TempStr exePath = path::JoinTemp(path, StrL("PDFXCview.exe"));
    if (file::Exists(exePath)) {
        return exePath;
    }
    return {};
}

static void SetKnownExternalViewerExePath(int cmdId, Str exePath) {
    if (!exePath) {
        return;
    }
    ExternalViewerInfo* info = FindKnownExternalViewerInfoByCmdId(cmdId);
    if (info && !info->exeFullPath) {
        info->exeFullPath = str::Dup(exePath);
    }
}

static bool DetectExternalViewer(ExternalViewerInfo* ev) {
    if (!ev->exePartialPath) {
        return false;
    }

    static int const csidls[] = {CSIDL_PROGRAM_FILES, CSIDL_PROGRAM_FILESX86, CSIDL_WINDOWS, CSIDL_SYSTEM};
    for (int csidl : csidls) {
        TempStr dir = GetSpecialFolderTemp(csidl);
        TempStr path = path::JoinTemp(dir, ev->exePartialPath);
        if (file::Exists(path)) {
            ev->exeFullPath = str::Dup(path);
            // logf("DetectExternalViewer: cmd %d, '%s' %s\n", ev->cmdId, ev->exeFullPath, ev->launchArgs);
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

    for (ExternalViewerInfo& i : gExternalViewers) {
        if (DetectExternalViewer(&i)) {
            gExternalViewersCount++;
        }
    }

    TempStr exePath = GetAcrobatPathTemp();
    SetKnownExternalViewerExePath(CmdOpenWithAcrobat, exePath);

    exePath = GetFoxitPathTemp();
    SetKnownExternalViewerExePath(CmdOpenWithFoxIt, exePath);

    exePath = GetPDFXChangePathTemp();
    SetKnownExternalViewerExePath(CmdOpenWithPdfXchange, exePath);
}

static bool filterMatchesEverything(Str ext) {
    return str::IsEmptyOrWhiteSpace(ext) || str::EqIS(ext, StrL("*"));
}

bool CanViewWithKnownExternalViewer(WindowTab* tab, int cmdId) {
    if (!tab || !CanViewExternally(tab)) {
        return false;
    }
    ExternalViewerInfo* ev = FindKnownExternalViewerInfoByCmdId(cmdId);
    if (!ev || !ev->exeFullPath) {
        // logfa("CanViewWithKnownExternalViewer cmd: %d, !ev || ev->exeFullPath == nullptr\n", cmd);
        return false;
    }
    // must match file extension

    if (!filterMatchesEverything(ev->exts)) {
        TempStr ext = path::GetExtTemp(tab->filePath);
        if (!str::ContainsI(ev->exts, ext)) {
            // logfa("CanViewWithKnownExternalViewer cmd: %d, !pos\n", cmd);
            return false;
        }
    }
    Kind engineKind = tab->GetEngineType();
    if (engineKind != nullptr) {
        if (ev->engineKind != nullptr) {
            if (ev->engineKind != engineKind) {
                logfa("CanViewWithKnownExternalViewer cmd: %d, ev->engineKind '%s' != engineKind '%s'\n", cmdId,
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

// substitutions in cmdLine:
//  %1 : file path (else the file path is appended)
//  %d : directory in which file is
//  %p : current page number
//  %% : a literal '%' (so e.g. "%%d" reaches the external program as "%d",
//       useful for tools like `mutool draw -o page-%d.png` -- see #5583)
// any other "%x" sequence is passed through unchanged.
// Note: substituted values (path, dir) are inserted literally and not
// re-scanned, so a '%' inside a file path can't trigger another substitution.
static TempStr FormatParamTemp(Str arg, WindowTab* tab) {
    Str path = tab->filePath ? tab->filePath : StrL("");

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
        logfa("ViewWithKnownExternalViewer cmd: %d\n", cmdId);
        // with command palette can send un-enforcable command so not ReportIf
        ReportDebugIf(!canView);
        return false;
    }
    ExternalViewerInfo* ev = FindKnownExternalViewerInfoByCmdId(cmdId);
    if (!ev->exeFullPath) {
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
    bool matches = path::Match(path, filter);
    return matches;
}

// TODO: find a better file for this?
// extract the executable (first token) from cmdLine, honoring a leading quote,
// and set *restOut to the remaining command line (after the exe and any spaces)
static TempStr ExtractExePathTemp(Str cmdLine, Str* restOut) {
    Str s = cmdLine;
    str::SkipChar(s, ' ');
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
    str::SkipChar(s, ' ');
    *restOut = s;
    return ToStrTemp(exe);
}

bool RunWithExe(WindowTab* tab, Str cmdLine, Str filter) {
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
        auto caption = _TRA("Error");
        MsgBox(nullptr, msg, caption, MB_OK | MB_ICONERROR);
        return false;
    }
    if (str::IsEmptyOrWhiteSpace(rest)) {
        // no arguments given: pass the document path as the only argument
        return LaunchFileShell(exePath, tab->filePath);
    }
    TempStr params = FormatParamTemp(rest, tab);
    return LaunchFileShell(exePath, params);
}

#define DEFINE_GUID_STATIC(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}
DEFINE_GUID_STATIC(CLSID_SendMail, 0x9E56BE60, 0xC50F, 0x11CF, 0x9A, 0x2C, 0x00, 0xA0, 0xC9, 0x0A, 0x90, 0xCE);

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
    if (IsRunningOnWine()) {
        cached = 0;
        return false;
    }
    bool ok = IsMapiSendMailAvailable();
    cached = ok ? 1 : 0;
    return ok;
}

bool CanSendAsEmailAttachment(WindowTab* tab) {
    // Requirements: a valid filename and a working MAPISendMailW (what we use to send).
    if (!CanViewExternally(tab)) {
        return false;
    }
    return IsEmailAttachmentSendAvailable();
}

// Use MAPISendMailW to send email with attachment.
// Works with Outlook, Thunderbird and other MAPI-registered email clients.
bool SendAsEmailAttachmentWithMapi(HWND hwndParent, Str filePath) {
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

bool SendAsEmailAttachment(WindowTab* tab, HWND hwndParent) {
    if (!tab || !CanSendAsEmailAttachment(tab)) {
        return false;
    }

    if (SendAsEmailAttachmentWithMapi(tab->win->hwndFrame, tab->filePath)) {
        return true;
    }
    // if there's no e-mail client associated, they both show the same message box
    // which will be confusing so I'll just hope that mapi works
#if 0
    // We use the SendTo drop target provided by SendMail.dll, which should ship with all
    // commonly used Windows versions, instead of MAPISendMail, which doesn't support
    // Unicode paths and might not be set up on systems not having Microsoft Outlook installed.
    ScopedComPtr<IDataObject> pDataObject(GetDataObjectForFile(tab->filePath, hwndParent));
    if (!pDataObject) {
        return false;
    }

    ScopedComPtr<IDropTarget> pDropTarget;
    if (!pDropTarget.Create(CLSID_SendMail)) {
        return false;
    }

    POINTL pt = {0, 0};
    DWORD dwEffect = 0;
    pDropTarget->DragEnter(pDataObject, MK_LBUTTON, pt, &dwEffect);
    HRESULT hr = pDropTarget->Drop(pDataObject, MK_LBUTTON, pt, &dwEffect);
    return SUCCEEDED(hr);
#else
    return false;
#endif
}
