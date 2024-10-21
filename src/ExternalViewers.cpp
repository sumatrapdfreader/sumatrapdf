/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/ScopedWin.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "WindowTab.h"
#include "ExternalViewers.h"
#include "Commands.h"
#include "Translations.h"

#include "utils/Log.h"

struct ExternalViewerInfo {
    const char* name; // shown to the user
    int cmdId;
    const char* exts; // valid extensions
    const char* exePartialPath;
    const char* launchArgs;
    Kind engineKind;
    // set by DetectExternalViewers()
    const char* exeFullPath; // if found, full path to the executable
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
        nullptr,
    },
    {
        "Directory Opus",
        CmdOpenWithDirectoryOpus,
        "*",
        R"(GPSoftware\Directory Opus\dopus.exe)",
        R"("%d")",
        nullptr,
        nullptr,
    },
    {
        "Total Commander",
        CmdOpenWithTotalCommander,
        "*",
        R"(totalcmd\TOTALCMD64.EXE)",
        R"("%d")",
        nullptr,
        nullptr,
    },
    {
        "Double Commander",
        CmdOpenWithDoubleCommander,
        "*",
        R"(Double Commander\doublecmd.exe)",
        R"(--no-splash --client "%d")",
        nullptr,
        nullptr,
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
        nullptr
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
        nullptr
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
        nullptr
    },
    {
        "Foxit PhantomPDF",
        CmdOpenWithFoxItPhantom,
        ".pdf",
        R"(Foxit Software\Foxit PhantomPDF\FoxitPhantomPDF.exe)",
        R"("%1" /A page=%p)",
        kindEngineMupdf,
        nullptr
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
        nullptr
    },
    {
        "Pdf & Djvu Bookmarker",
        CmdOpenWithPdfDjvuBookmarker,
        ".pdf;.djvu",
        R"(Pdf & Djvu Bookmarker\PdfDjvuBookmarker.exe)",
        nullptr,
        nullptr,
        nullptr
    },
    {
        "XPS Viewer",
        CmdOpenWithXpsViewer,
        ".xps;.oxps",
        "xpsrchvw.exe",
        nullptr,
        kindEngineMupdf,
        nullptr
    },
    {
        "HTML Help",
        CmdOpenWithHtmlHelp,
        ".chm",
        "hh.exe",
        nullptr,
        kindEngineChm,
        nullptr
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
    return info && info->exeFullPath != nullptr;
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
        str::FreePtr(&info.exeFullPath);
    }
}

static char* GetAcrobatPathTemp() {
    // Try Adobe Acrobat as a fall-back, if the Reader isn't installed
    const char* keyName = R"(Software\Microsoft\Windows\CurrentVersion\App Paths\AcroRd32.exe)";
    char* path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, nullptr);
    if (!path) {
        keyName = R"(Software\Microsoft\Windows\CurrentVersion\App Paths\Acrobat.exe)";
        path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, nullptr);
    }
    if (path && file::Exists(path)) {
        return path;
    }
    return nullptr;
}

static char* GetFoxitPathTemp() {
    const char* keyName = R"(Software\Microsoft\Windows\CurrentVersion\Uninstall\Foxit Reader)";
    char* path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, "DisplayIcon");
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
        path = path::JoinTemp(path, "Foxit Reader.exe");
    }
    if (path && file::Exists(path)) {
        return path;
    }
    // Registry value for Foxit PDF Reader 12.1.3.15356 (The last version with Add Bookmark function without bugs in
    // single-key accelerator)
    keyName = R"(SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\FoxitPDFReader.exe)";
    path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, "Path");
    if (path) {
        path = path::JoinTemp(path, "FoxitPDFReader.exe");
    }
    if (path && file::Exists(path)) {
        return path;
    }
    return nullptr;
}

static char* GetPDFXChangePathTemp() {
    const char* keyName = R"(Software\Tracker Software\PDFViewer)";
    char* path = ReadRegStrTemp(HKEY_LOCAL_MACHINE, keyName, "InstallPath");
    if (!path) {
        keyName = R"(Software\Tracker Software\PDFViewer)";
        path = ReadRegStrTemp(HKEY_CURRENT_USER, keyName, "InstallPath");
    }
    if (!path) {
        return nullptr;
    }
    char* exePath = path::JoinTemp(path, "PDFXCview.exe");
    if (file::Exists(exePath)) {
        return exePath;
    }
    return nullptr;
}

static void SetKnownExternalViewerExePath(int cmdId, const char* exePath) {
    if (!exePath) {
        return;
    }
    ExternalViewerInfo* info = FindKnownExternalViewerInfoByCmdId(cmdId);
    if (info && info->exeFullPath == nullptr) {
        info->exeFullPath = str::Dup(exePath);
    }
}

static bool DetectExternalViewer(ExternalViewerInfo* ev) {
    const char* partialPath = ev->exePartialPath;
    if (!partialPath || !*partialPath) {
        return false;
    }

    static int const csidls[] = {CSIDL_PROGRAM_FILES, CSIDL_PROGRAM_FILESX86, CSIDL_WINDOWS, CSIDL_SYSTEM};
    for (int csidl : csidls) {
        TempStr dir = GetSpecialFolderTemp(csidl);
        TempStr path = path::JoinTemp(dir, partialPath);
        if (file::Exists(path)) {
            ev->exeFullPath = str::Dup(path);
            const char* args = ev->launchArgs;
            if (!args) {
                args = "";
            }
            logf("DetectExternalViewer: cmd %d, '%s' %s\n", ev->cmdId, ev->exeFullPath, args);
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

    const char* exePath = GetAcrobatPathTemp();
    SetKnownExternalViewerExePath(CmdOpenWithAcrobat, exePath);

    exePath = GetFoxitPathTemp();
    SetKnownExternalViewerExePath(CmdOpenWithFoxIt, exePath);

    exePath = GetPDFXChangePathTemp();
    SetKnownExternalViewerExePath(CmdOpenWithPdfXchange, exePath);
}

static bool filterMatchesEverything(const char* ext) {
    return str::IsEmptyOrWhiteSpace(ext) || str::EqIS(ext, "*");
}

bool CanViewWithKnownExternalViewer(WindowTab* tab, int cmdId) {
    if (!tab || !CanViewExternally(tab)) {
        return false;
    }
    ExternalViewerInfo* ev = FindKnownExternalViewerInfoByCmdId(cmdId);
    if (!ev || ev->exeFullPath == nullptr) {
        // logfa("CanViewWithKnownExternalViewer cmd: %d, !ev || ev->exeFullPath == nullptr\n", cmd);
        return false;
    }
    // must match file extension

    if (!filterMatchesEverything(ev->exts)) {
        const char* filePath = tab->filePath;
        char* ext = path::GetExtTemp(filePath);
        const char* pos = str::FindI(ev->exts, ext);
        if (!pos) {
            // logfa("CanViewWithKnownExternalViewer cmd: %d, !pos\n", cmd);
            return false;
        }
    }
    Kind engineKind = tab->GetEngineType();
    if (engineKind != nullptr) {
        if (ev->engineKind != nullptr) {
            if (ev->engineKind != engineKind) {
                logfa("CanViewWithKnownExternalViewer cmd: %d, ev->engineKind '%s' != engineKind '%s'\n", cmdId,
                      ev->engineKind, engineKind);
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
static TempStr FormatParamTemp(char* arg, WindowTab* tab) {
    if (str::Find(arg, "%p")) {
        int pageNo = tab->ctrl ? tab->ctrl->CurrentPageNo() : 0;
        TempStr pageNoStr = str::FormatTemp("%d", pageNo);
        arg = str::ReplaceTemp(arg, "%p", pageNoStr);
    }
    const char* path = tab->filePath;
    if (str::Find(arg, "%d")) {
        TempStr dir = path::GetDirTemp(path);
        arg = str::ReplaceTemp(arg, "%d", dir);
    }
    if (str::Find(arg, "%1")) {
        // TODO: if %1 is un-quoted, we should quote it but it's complicated because
        // it could be part of a pattern like %1.Page%p.txt
        // (as in https://github.com/sumatrapdfreader/sumatrapdf/issues/3868)
        arg = str::ReplaceTemp(arg, "%1", path);
    }
    return (char*)arg;
}

static TempStr GetDocumentPathQuoted(WindowTab* tab) {
    auto path = tab->filePath;
    return str::JoinTemp("\"", path, "\"");
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
    if (ev->exeFullPath == nullptr) {
        return false;
    }
    char* origArgs = (char*)ev->launchArgs;
    TempStr args;
    if (origArgs) {
        args = FormatParamTemp(origArgs, tab);
    } else {
        args = GetDocumentPathQuoted(tab);
    }
    return LaunchFileShell(ev->exeFullPath, args);
}

bool PathMatchFilter(const char* path, const char* filter) {
    if (filterMatchesEverything(filter)) {
        return true;
    }
    bool matches = path::Match(path, filter);
    return matches;
}

// TODO: find a better file for this?
bool RunWithExe(WindowTab* tab, const char* cmdLine, const char* filter) {
    const char* path = tab->filePath;
    if (!PathMatchFilter(path, filter)) {
        return false;
    }

    StrVec args;
    ParseCmdLine(cmdLine, args);
    int nArgs = args.Size();
    if (nArgs == 0) {
        return false;
    }
    const char* exePath = args.At(0);
    // TODO: this should be in ViewWithCustomExternalViewer()
    if (!file::Exists(exePath)) {
        TempStr msg = str::FormatTemp(
            "External viewer executable not found: %s. Fix ExternalViewers in advanced settings.", exePath);
        auto caption = _TRA("Error");
        MsgBox(nullptr, msg, caption, MB_OK | MB_ICONERROR);
        return false;
    }
    StrVec argsQuoted;
    if (nArgs == 1) {
        return LaunchFileShell(exePath, path);
    }
    for (int i = 1; i < nArgs; i++) {
        char* s = args.At(i);
        TempStr param = FormatParamTemp(s, tab);
        TempStr paramQuoted = QuoteCmdLineArgTemp(param);
        argsQuoted.Append(paramQuoted);
    }
    TempStr params = JoinTemp(&argsQuoted, " ");
    return LaunchFileShell(exePath, params);
}

#define DEFINE_GUID_STATIC(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}
DEFINE_GUID_STATIC(CLSID_SendMail, 0x9E56BE60, 0xC50F, 0x11CF, 0x9A, 0x2C, 0x00, 0xA0, 0xC9, 0x0A, 0x90, 0xCE);

bool CanSendAsEmailAttachment(WindowTab* tab) {
    // Requirements: a valid filename and access to SendMail's IDropTarget interface
    if (!CanViewExternally(tab)) {
        return false;
    }

    ScopedComPtr<IDropTarget> pDropTarget;
    return pDropTarget.Create(CLSID_SendMail);
}

// TODO: maybe use
// https://stackoverflow.com/questions/47639267/win32-c-sending-email-in-windows-10-by-invoking-default-mail-client
bool SendAsEmailAttachment(WindowTab* tab, HWND hwndParent) {
    if (!tab || !CanSendAsEmailAttachment(tab)) {
        return false;
    }

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
}
