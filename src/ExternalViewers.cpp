/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/ScopedWin.h"

#include "wingui/TreeModel.h"
#include "DisplayMode.h"
#include "Controller.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "TabInfo.h"
#include "ExternalViewers.h"
#include "Commands.h"

struct ExternalViewerInfo {
    const char* name; // shown to the user
    int cmd;
    const char* exts; // valid extensions
    const char* exePartialPath;
    const char* launchArgs;
    Kind engineKind;
    // set by DetectExternalViewers()
    const WCHAR* exeFullPath; // if found, full path to the executable
};

// kindEngineChm

static int gExternalViewersCount{0};

// clang-format off
static ExternalViewerInfo gExternalViewers[] = {
    // it's no longer installed by default in win 10
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
    },
};
// clang-format on

static ExternalViewerInfo* FindExternalViewerInfoByCmd(int cmd) {
    int n = dimof(gExternalViewers);
    for (int i = 0; i < n; i++) {
        ExternalViewerInfo* info = &gExternalViewers[i];
        if (info->cmd == cmd) {
            return info;
        }
    }
    CrashMe();
    return nullptr;
}

static bool CanViewExternally(TabInfo* tab) {
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
    const WCHAR* partialPath = ToWstrTemp(ev->exePartialPath);
    if (!partialPath || !*partialPath) {
        return false;
    }

    {
        TempWstr dir = GetSpecialFolderTemp(CSIDL_PROGRAM_FILES);
        WCHAR* path = path::Join(dir, partialPath);
        if (file::Exists(path)) {
            ev->exeFullPath = path;
            return true;
        }
        str::Free(path);
    }
    {
        TempWstr dir = GetSpecialFolderTemp(CSIDL_PROGRAM_FILESX86);
        WCHAR* path = path::Join(dir, partialPath);
        if (file::Exists(path)) {
            ev->exeFullPath = path;
            return true;
        }
        str::Free(path);
    }
    {
        TempWstr dir = GetSpecialFolderTemp(CSIDL_WINDOWS);
        WCHAR* path = path::Join(dir, partialPath);
        if (file::Exists(path)) {
            ev->exeFullPath = path;
            return true;
        }
        str::Free(path);
    }
    {
        TempWstr dir = GetSpecialFolderTemp(CSIDL_SYSTEM);
        WCHAR* path = path::Join(dir, partialPath);
        if (file::Exists(path)) {
            ev->exeFullPath = path;
            return true;
        }
        str::Free(path);
    }

    return false;
}

void FreeExternalViewers() {
    int n = dimof(gExternalViewers);
    for (int i = 0; i < n; i++) {
        ExternalViewerInfo* info = &gExternalViewers[i];
        str::FreePtr(&info->exeFullPath);
    }
}

static WCHAR* GetAcrobatPath() {
    // Try Adobe Acrobat as a fall-back, if the Reader isn't installed
    AutoFreeWstr path =
        ReadRegStr(HKEY_LOCAL_MACHINE, LR"(Software\Microsoft\Windows\CurrentVersion\App Paths\AcroRd32.exe)", nullptr);
    if (!path) {
        path.Set(ReadRegStr(HKEY_LOCAL_MACHINE, LR"(Software\Microsoft\Windows\CurrentVersion\App Paths\Acrobat.exe)",
                            nullptr));
    }
    if (path && file::Exists(path)) {
        return path.StealData();
    }
    return nullptr;
}

static WCHAR* GetFoxitPath() {
    AutoFreeWstr path = ReadRegStr(
        HKEY_LOCAL_MACHINE, LR"(Software\Microsoft\Windows\CurrentVersion\Uninstall\Foxit Reader)", L"DisplayIcon");
    if (path && file::Exists(path)) {
        return path.StealData();
    }
    // Registry value for Foxit 5 (and maybe later)
    path.Set(ReadRegStr(HKEY_LOCAL_MACHINE, LR"(Software\Microsoft\Windows\CurrentVersion\Uninstall\Foxit Reader_is1)",
                        L"DisplayIcon"));
    if (path && file::Exists(path)) {
        return path.StealData();
    }
    // Registry value for Foxit 5.5 MSI installer
    path.Set(ReadRegStr(HKEY_LOCAL_MACHINE, LR"(Software\Foxit Software\Foxit Reader)", L"InstallPath"));
    if (path) {
        path.Set(path::Join(path, L"Foxit Reader.exe"));
    }
    if (path && file::Exists(path)) {
        return path.StealData();
    }
    return nullptr;
}

static WCHAR* GetPDFXChangePath() {
    AutoFreeWstr path = ReadRegStr(HKEY_LOCAL_MACHINE, LR"(Software\Tracker Software\PDFViewer)", L"InstallPath");
    if (!path) {
        path.Set(ReadRegStr(HKEY_CURRENT_USER, LR"(Software\Tracker Software\PDFViewer)", L"InstallPath"));
    }
    if (!path) {
        return nullptr;
    }
    AutoFreeWstr exePath(path::Join(path, L"PDFXCview.exe"));
    if (file::Exists(exePath)) {
        return exePath.StealData();
    }
    return nullptr;
}

void DetectExternalViewers() {
    CrashIf(gExternalViewersCount > 0); // only call once

    ExternalViewerInfo* info{nullptr};
    int n = dimof(gExternalViewers);
    for (int i = 0; i < n; i++) {
        info = &gExternalViewers[i];
        bool didDetect = DetectExternalViewer(info);
        if (didDetect) {
            gExternalViewersCount++;
        }
    }

    info = FindExternalViewerInfoByCmd(CmdOpenWithAcrobat);
    if (!info->exeFullPath) {
        info->exeFullPath = GetAcrobatPath();
    }

    info = FindExternalViewerInfoByCmd(CmdOpenWithFoxIt);
    if (!info->exeFullPath) {
        info->exeFullPath = GetFoxitPath();
    }

    info = FindExternalViewerInfoByCmd(CmdOpenWithPdfXchange);
    if (!info->exeFullPath) {
        info->exeFullPath = GetPDFXChangePath();
    }
}

bool CanViewWithKnownExternalViewer(TabInfo* tab, int cmd) {
    if (!tab || !CanViewExternally(tab)) {
        return false;
    }
    ExternalViewerInfo* ev = FindExternalViewerInfoByCmd(cmd);
    if (!ev || ev->exeFullPath == nullptr) {
        return false;
    }
    // must match file extension
    const WCHAR* filePath = tab->filePath.Get();
    const WCHAR* ext = path::GetExtTemp(filePath);
    WCHAR* exts = ToWstrTemp(ev->exts);
    const WCHAR* pos = str::FindI(exts, ext);
    if (!pos) {
        return false;
    }
    Kind engineKind = tab->GetEngineType();
    if (engineKind != nullptr) {
        if (ev->engineKind != nullptr) {
            if (ev->engineKind != engineKind) {
                return false;
            }
        }
    }
    return true;
}

bool CouldBePDFDoc(TabInfo* tab) {
    // consider any error state a potential PDF document
    return !tab || !tab->ctrl || tab->GetEngineType() == kindEngineMupdf;
}

static WCHAR* FormatParams(const WCHAR* cmdLine, TabInfo* tab) {
    // if the command line contains %p, it's replaced with the current page number
    // if it contains %1, it's replaced with the file path (else the file path is appended)
    AutoFreeWstr params;
    if (cmdLine == nullptr) {
        cmdLine = LR"("%1")";
    }
    if (str::Find(cmdLine, L"%p")) {
        AutoFreeWstr pageNoStr(str::Format(L"%d", tab->ctrl ? tab->ctrl->CurrentPageNo() : 0));
        params.Set(str::Replace(cmdLine, L"%p", pageNoStr));
        cmdLine = params;
    }
    if (str::Find(cmdLine, LR"("%1")")) {
        // "%1", is alrady quoted so no need to add quotes
        params.Set(str::Replace(cmdLine, L"%1", tab->filePath));
    } else if (str::Find(cmdLine, LR"(%1)")) {
        // %1, not quoted, need to add
        auto s = str::JoinTemp(L"\"", tab->filePath.Get(), L"\"").Get();
        params.Set(str::Replace(cmdLine, L"%1", s));
    } else {
        params.Set(str::Format(LR"(%s "%s")", cmdLine, tab->filePath.Get()));
    }
    return params.StealData();
}

bool ViewWithKnownExternalViewer(TabInfo* tab, int cmd) {
    bool canView = CanViewWithKnownExternalViewer(tab, cmd);
    CrashIf(!canView);
    ExternalViewerInfo* ev = FindExternalViewerInfoByCmd(cmd);
    if (!canView || ev->exeFullPath == nullptr) {
        return false;
    }
    AutoFreeWstr params = FormatParams(ToWstrTemp(ev->launchArgs), tab);
    return LaunchFile(ev->exeFullPath, params);
}

bool PathMatchFilter(const WCHAR* path, char* filter) {
    // no filter means matches everything
    if (str::IsEmpty(filter)) {
        return true;
    }
    if (str::Eq(filter, "*")) {
        return true;
    }
    auto s = ToWstrTemp(filter);
    bool matches = path::Match(path, s.Get());
    return matches;
}

bool ViewWithExternalViewer(TabInfo* tab, size_t idx) {
    if (!HasPermission(Perm::DiskAccess) || !tab || !file::Exists(tab->filePath)) {
        return false;
    }

    auto& viewers = gGlobalPrefs->externalViewers;
    ExternalViewer* ev = nullptr;
    for (size_t i = 0; i < viewers->size() && i <= idx; i++) {
        ev = viewers->at(i);
        // see AppendExternalViewersToMenu in Menu.cpp
        if (!ev->commandLine || !PathMatchFilter(tab->filePath, ev->filter)) {
            idx++;
        }
    }
    if (idx >= viewers->size() || !viewers->at(idx)->commandLine) {
        return false;
    }

    CmdLineArgsIter args(ToWstrTemp(ev->commandLine));
    if (args.nArgs == 0) {
        return false;
    }
    const WCHAR* exePath = args.at(0);
    if (!file::Exists(exePath)) {
        return false;
    }
    const WCHAR* cmdLine = args.ParamsTemp();
    AutoFreeWstr params = FormatParams(cmdLine, tab);
    return LaunchFile(exePath, params);
}

#define DEFINE_GUID_STATIC(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}
DEFINE_GUID_STATIC(CLSID_SendMail, 0x9E56BE60, 0xC50F, 0x11CF, 0x9A, 0x2C, 0x00, 0xA0, 0xC9, 0x0A, 0x90, 0xCE);

bool CanSendAsEmailAttachment(TabInfo* tab) {
    // Requirements: a valid filename and access to SendMail's IDropTarget interface
    if (!CanViewExternally(tab)) {
        return false;
    }

    ScopedComPtr<IDropTarget> pDropTarget;
    return pDropTarget.Create(CLSID_SendMail);
}

bool SendAsEmailAttachment(TabInfo* tab, HWND hwndParent) {
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
