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

#include "utils/Log.h"

struct ExternalViewerInfo {
    const char* name; // shown to the user
    int cmd;
    const char* exts; // valid extensions
    const char* exePartialPath;
    const char* launchArgs;
    Kind engineKind;
    // set by DetectExternalViewers()
    const char* exeFullPath; // if found, full path to the executable
};

// kindEngineChm

static int gExternalViewersCount = 0;

//
// clang-format off
static ExternalViewerInfo gExternalViewers[] = {
    {
        "Explorer",
        CmdOpenWithExplorer,
        "*",
        "explorer.exe",
        R"("%d")",
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

static ExternalViewerInfo* FindExternalViewerInfoByCmd(int cmd) {
    ExternalViewerInfo* info = nullptr;
    int n = dimof(gExternalViewers);
    for (int i = 0; i < n; i++) {
        info = &gExternalViewers[i];
        if (info->cmd == cmd) {
            return info;
        }
    }
    return nullptr;
}

bool HasExternalViewerForCmd(int cmd) {
    auto* v = FindExternalViewerInfoByCmd(cmd);
    if (v == nullptr) {
        return false;
    }
    return v->exeFullPath != nullptr;
}

static bool CanViewExternally(WindowTab* tab) {
    if (!HasPermission(Perm::DiskAccess)) {
        return false;
    }
    // if tab is nullptr, we're queried for the
    // About window with disabled menu items
    if (!tab) {
        return true;
    }
    return file::Exists(tab->filePath);
}

static bool DetectExternalViewer(ExternalViewerInfo* ev) {
    const char* partialPath = ev->exePartialPath;
    if (!partialPath || !*partialPath) {
        return false;
    }

    static int const csidls[] = {CSIDL_PROGRAM_FILES, CSIDL_PROGRAM_FILESX86, CSIDL_WINDOWS, CSIDL_SYSTEM};
    for (int csidl : csidls) {
        char* dir = GetSpecialFolderTemp(csidl);
        char* path = path::JoinTemp(dir, partialPath);
        if (file::Exists(path)) {
            ev->exeFullPath = str::Dup(path);
            return true;
        }
    }
    return false;
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

void DetectExternalViewers() {
    CrashIf(gExternalViewersCount > 0); // only call once

    ExternalViewerInfo* info = nullptr;
    for (ExternalViewerInfo& i : gExternalViewers) {
        info = &i;
        bool didDetect = DetectExternalViewer(info);
        if (didDetect) {
            gExternalViewersCount++;
        }
    }

    info = FindExternalViewerInfoByCmd(CmdOpenWithAcrobat);
    if (!info->exeFullPath) {
        info->exeFullPath = str::Dup(GetAcrobatPathTemp());
    }

    info = FindExternalViewerInfoByCmd(CmdOpenWithFoxIt);
    if (!info->exeFullPath) {
        info->exeFullPath = str::Dup(GetFoxitPathTemp());
    }

    info = FindExternalViewerInfoByCmd(CmdOpenWithPdfXchange);
    if (!info->exeFullPath) {
        info->exeFullPath = str::Dup(GetPDFXChangePathTemp());
    }
}

static bool filterMatchesEverything(const char* ext) {
    return str::IsEmpty(ext) || str::Eq(ext, "*");
}

bool CanViewWithKnownExternalViewer(WindowTab* tab, int cmd) {
    if (!tab || !CanViewExternally(tab)) {
        return false;
    }
    ExternalViewerInfo* ev = FindExternalViewerInfoByCmd(cmd);
    if (!ev || ev->exeFullPath == nullptr) {
        // logfa("CanViewWithKnownExternalViewer cmd: %d, !ev || ev->exeFullPath == nullptr\n", cmd);
        return false;
    }
    // must match file extension

    if (!filterMatchesEverything(ev->exts)) {
        const char* filePath = tab->filePath.Get();
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
                logfa("CanViewWithKnownExternalViewer cmd: %d, ev->engineKind '%s' != engineKind '%s'\n", cmd,
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
static TempStr FormatParamsTemp(const char* cmdLine, WindowTab* tab) {
    if (cmdLine == nullptr) {
        cmdLine = R"("%1")";
    }
    if (str::Find(cmdLine, "%p")) {
        int pageNo = tab->ctrl ? tab->ctrl->CurrentPageNo() : 0;
        TempStr pageNoStr = str::FormatTemp("%d", pageNo);
        cmdLine = str::ReplaceTemp(cmdLine, "%p", pageNoStr);
    }
    bool appendPath = true;
    char* path = tab->filePath;
    if (str::Find(cmdLine, "%d")) {
        TempStr dir = path::GetDirTemp(path);
        cmdLine = str::ReplaceTemp(cmdLine, "%d", dir);
        appendPath = false;
    }
    if (str::Find(cmdLine, R"("%1")")) {
        // "%1", is alrady quoted so no need to add quotes
        cmdLine = str::ReplaceTemp(cmdLine, "%1", path);
    } else if (str::Find(cmdLine, R"(%1)")) {
        // %1, not quoted, need to add
        char* s = str::JoinTemp("\"", path, "\"");
        cmdLine = str::ReplaceTemp(cmdLine, "%1", s);
    } else if (appendPath) {
        cmdLine = str::FormatTemp(R"(%s "%s")", cmdLine, path);
    }
    return (char*)cmdLine;
}

bool ViewWithKnownExternalViewer(WindowTab* tab, int cmd) {
    bool canView = CanViewWithKnownExternalViewer(tab, cmd);
    if (!canView) {
        // TODO: with command palette can send un-enforcable command
        logfa("ViewWithKnownExternalViewer cmd: %d\n", cmd);
        ReportIf(!canView);
        return false;
    }
    ExternalViewerInfo* ev = FindExternalViewerInfoByCmd(cmd);
    if (ev->exeFullPath == nullptr) {
        return false;
    }
    TempStr params = FormatParamsTemp(ev->launchArgs, tab);
    return LaunchFile(ev->exeFullPath, params);
}

bool PathMatchFilter(const char* path, char* filter) {
    if (filterMatchesEverything(filter)) {
        return true;
    }
    bool matches = path::Match(path, filter);
    return matches;
}

bool ViewWithExternalViewer(WindowTab* tab, size_t idx) {
    if (!HasPermission(Perm::DiskAccess) || !tab || !file::Exists(tab->filePath)) {
        return false;
    }

    auto& viewers = gGlobalPrefs->externalViewers;
    ExternalViewer* ev = nullptr;
    size_t n = viewers->size();
    for (size_t i = 0; i < n && i <= idx; i++) {
        ev = viewers->at(i);
        // see AppendExternalViewersToMenu in Menu.cpp
        char* path = tab->filePath;
        if (!ev->commandLine || !PathMatchFilter(path, ev->filter)) {
            idx++;
        }
    }
    if (idx >= n) {
        return false;
    }
    ev = viewers->at(idx);
    if (!ev || !ev->commandLine) {
        return false;
    }

    CmdLineArgsIter args(ToWStrTemp(ev->commandLine));
    if (args.nArgs == 0) {
        return false;
    }
    const char* exePath = args.at(0);
    if (!file::Exists(exePath)) {
        return false;
    }
    char* cmdLine = args.ParamsTemp();
    TempStr params = FormatParamsTemp(cmdLine, tab);
    return LaunchFile(exePath, params);
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
