/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/CmdLineParser.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/ScopedWin.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EngineManager.h"

#include "SettingsStructs.h"
#include "Controller.h"
#include "GlobalPrefs.h"

#include "SumatraPDF.h"
#include "TabInfo.h"
#include "ExternalViewers.h"

static WCHAR* GetAcrobatPath() {
    // Try Adobe Acrobat as a fall-back, if the Reader isn't installed
    AutoFreeWstr path(ReadRegStr(HKEY_LOCAL_MACHINE,
                                 L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\AcroRd32.exe", nullptr));
    if (!path)
        path.Set(ReadRegStr(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Acrobat.exe",
                            nullptr));
    if (path && file::Exists(path))
        return path.StealData();
    return nullptr;
}

static WCHAR* GetFoxitPath() {
    AutoFreeWstr path(ReadRegStr(
        HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Foxit Reader", L"DisplayIcon"));
    if (path && file::Exists(path))
        return path.StealData();
    // Registry value for Foxit 5 (and maybe later)
    path.Set(ReadRegStr(HKEY_LOCAL_MACHINE,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Foxit Reader_is1", L"DisplayIcon"));
    if (path && file::Exists(path))
        return path.StealData();
    // Registry value for Foxit 5.5 MSI installer
    path.Set(ReadRegStr(HKEY_LOCAL_MACHINE, L"Software\\Foxit Software\\Foxit Reader", L"InstallPath"));
    if (path)
        path.Set(path::Join(path, L"Foxit Reader.exe"));
    if (path && file::Exists(path))
        return path.StealData();
    return nullptr;
}

static WCHAR* GetPDFXChangePath() {
    AutoFreeWstr path(ReadRegStr(HKEY_LOCAL_MACHINE, L"Software\\Tracker Software\\PDFViewer", L"InstallPath"));
    if (!path)
        path.Set(ReadRegStr(HKEY_CURRENT_USER, L"Software\\Tracker Software\\PDFViewer", L"InstallPath"));
    if (!path)
        return nullptr;
    AutoFreeWstr exePath(path::Join(path, L"PDFXCview.exe"));
    if (file::Exists(exePath))
        return exePath.StealData();
    return nullptr;
}

static WCHAR* GetXPSViewerPath() {
    // the XPS-Viewer seems to always be installed into %WINDIR%\system32
    WCHAR buffer[MAX_PATH];
    UINT res = GetSystemDirectory(buffer, dimof(buffer));
    if (!res || res >= dimof(buffer))
        return nullptr;
    AutoFreeWstr exePath(path::Join(buffer, L"xpsrchvw.exe"));
    if (file::Exists(exePath))
        return exePath.StealData();
#ifndef _WIN64
    // Wow64 redirects access to system32 to syswow64 instead, so we
    // disable file system redirection using the recommended method from
    // http://msdn.microsoft.com/en-us/library/aa384187(v=vs.85).aspx
    if (IsRunningInWow64()) {
        res = GetWindowsDirectory(buffer, dimof(buffer));
        if (!res || res >= dimof(buffer))
            return nullptr;
        exePath.Set(path::Join(buffer, L"Sysnative\\xpsrchvw.exe"));
        if (file::Exists(exePath))
            return exePath.StealData();
    }
#endif
    return nullptr;
}

static WCHAR* GetHtmlHelpPath() {
    // the Html Help viewer seems to be installed either into %WINDIR% or %WINDIR%\system32
    WCHAR buffer[MAX_PATH];
    UINT res = GetWindowsDirectory(buffer, dimof(buffer));
    if (!res || res >= dimof(buffer))
        return nullptr;
    AutoFreeWstr exePath(path::Join(buffer, L"hh.exe"));
    if (file::Exists(exePath))
        return exePath.StealData();
    res = GetSystemDirectory(buffer, dimof(buffer));
    if (!res || res >= dimof(buffer))
        return nullptr;
    exePath.Set(path::Join(buffer, L"hh.exe"));
    if (file::Exists(exePath))
        return exePath.StealData();
    return nullptr;
}

static bool CanViewExternally(TabInfo* tab) {
    if (!HasPermission(Perm_DiskAccess))
        return false;
    // if tab is nullptr, we're queried for the
    // About window with disabled menu items
    if (!tab)
        return true;
    return file::Exists(tab->filePath);
}

bool CouldBePDFDoc(TabInfo* tab) {
    // consider any error state a potential PDF document
    return !tab || !tab->ctrl || tab->GetEngineType() == kindEnginePdf;
}

bool CanViewWithFoxit(TabInfo* tab) {
    // Requirements: a valid filename and a valid path to Foxit
    if (!CouldBePDFDoc(tab) || !CanViewExternally(tab))
        return false;
    AutoFreeWstr path(GetFoxitPath());
    return path != nullptr;
}

bool ViewWithFoxit(TabInfo* tab, const WCHAR* args) {
    if (!tab || !CanViewWithFoxit(tab))
        return false;

    AutoFreeWstr exePath(GetFoxitPath());
    if (!exePath)
        return false;
    if (!args)
        args = L"";

    // Foxit cmd-line format:
    // [PDF filename] [-n <page number>] [-pwd <password>] [-z <zoom>]
    // TODO: Foxit allows passing password and zoom
    AutoFreeWstr params;
    if (tab->ctrl)
        params.Set(str::Format(L"\"%s\" %s -n %d", tab->ctrl->FilePath(), args, tab->ctrl->CurrentPageNo()));
    else
        params.Set(str::Format(L"\"%s\" %s", tab->filePath.get(), args));
    return LaunchFile(exePath, params);
}

bool CanViewWithPDFXChange(TabInfo* tab) {
    // Requirements: a valid filename and a valid path to PDF X-Change
    if (!CouldBePDFDoc(tab) || !CanViewExternally(tab))
        return false;
    AutoFreeWstr path(GetPDFXChangePath());
    return path != nullptr;
}

bool ViewWithPDFXChange(TabInfo* tab, const WCHAR* args) {
    if (!tab || !CanViewWithPDFXChange(tab))
        return false;

    AutoFreeWstr exePath(GetPDFXChangePath());
    if (!exePath)
        return false;
    if (!args)
        args = L"";

    // PDFXChange cmd-line format:
    // [/A "param=value [&param2=value ..."] [PDF filename]
    // /A params: page=<page number>
    AutoFreeWstr params;
    if (tab->ctrl)
        params.Set(str::Format(L"%s /A \"page=%d\" \"%s\"", args, tab->ctrl->CurrentPageNo(), tab->ctrl->FilePath()));
    else
        params.Set(str::Format(L"%s \"%s\"", args, tab->filePath.get()));
    return LaunchFile(exePath, params);
}

bool CanViewWithAcrobat(TabInfo* tab) {
    // Requirements: a valid filename and a valid path to Adobe Reader
    if (!CouldBePDFDoc(tab) || !CanViewExternally(tab))
        return false;
    AutoFreeWstr exePath(GetAcrobatPath());
    return exePath != nullptr;
}

bool ViewWithAcrobat(TabInfo* tab, const WCHAR* args) {
    if (!tab || !CanViewWithAcrobat(tab))
        return false;

    AutoFreeWstr exePath(GetAcrobatPath());
    if (!exePath)
        return false;

    if (!args)
        args = L"";

    AutoFreeWstr params;
    // Command line format for version 6 and later:
    //   /A "page=%d&zoom=%.1f,%d,%d&..." <filename>
    // see http://www.adobe.com/devnet/acrobat/pdfs/pdf_open_parameters.pdf#page=5
    //   /P <filename>
    // see http://www.adobe.com/devnet/acrobat/pdfs/Acrobat_SDK_developer_faq.pdf#page=24
    // TODO: Also set zoom factor and scroll to current position?
    if (tab->ctrl && HIWORD(GetFileVersion(exePath)) >= 6)
        params.Set(str::Format(L"/A \"page=%d\" %s \"%s\"", tab->ctrl->CurrentPageNo(), args, tab->ctrl->FilePath()));
    else
        params.Set(str::Format(L"%s \"%s\"", args, tab->filePath.get()));

    return LaunchFile(exePath, params);
}

bool CanViewWithXPSViewer(TabInfo* tab) {
    // Requirements: a valid filename and a valid path to XPS-Viewer
    if (!tab || !CanViewExternally(tab))
        return false;
    // allow viewing with XPS-Viewer, if either an XPS document is loaded...
    if (tab->ctrl && tab->GetEngineType() != kindEngineXps)
        return false;
    // or a file ending in .xps or .oxps has failed to be loaded
    if (!tab->ctrl && !str::EndsWithI(tab->filePath, L".xps") && !str::EndsWithI(tab->filePath, L".oxps"))
        return false;
    AutoFreeWstr path(GetXPSViewerPath());
    return path != nullptr;
}

bool ViewWithXPSViewer(TabInfo* tab, const WCHAR* args) {
    if (!tab || !CanViewWithXPSViewer(tab))
        return false;

    AutoFreeWstr exePath(GetXPSViewerPath());
    if (!exePath)
        return false;

    if (!args)
        args = L"";

    AutoFreeWstr params;
    if (tab->ctrl)
        params.Set(str::Format(L"%s \"%s\"", args, tab->ctrl->FilePath()));
    else
        params.Set(str::Format(L"%s \"%s\"", args, tab->filePath.get()));
    return LaunchFile(exePath, params);
}

bool CanViewWithHtmlHelp(TabInfo* tab) {
    // Requirements: a valid filename and a valid path to HTML Help
    if (!tab || !CanViewExternally(tab))
        return false;
    // allow viewing with HTML Help, if either an CHM document is loaded...
    if (tab->ctrl && tab->GetEngineType() != kindEngineChm && !tab->AsChm())
        return false;
    // or a file ending in .chm has failed to be loaded
    if (!tab->ctrl && !str::EndsWithI(tab->filePath, L".chm"))
        return false;
    AutoFreeWstr path(GetHtmlHelpPath());
    return path != nullptr;
}

bool ViewWithHtmlHelp(TabInfo* tab, const WCHAR* args) {
    if (!tab || !CanViewWithHtmlHelp(tab))
        return false;

    AutoFreeWstr exePath(GetHtmlHelpPath());
    if (!exePath)
        return false;

    if (!args)
        args = L"";

    AutoFreeWstr params;
    if (tab->ctrl)
        params.Set(str::Format(L"%s \"%s\"", args, tab->ctrl->FilePath()));
    else
        params.Set(str::Format(L"%s \"%s\"", args, tab->filePath.get()));
    return LaunchFile(exePath, params);
}

bool ViewWithExternalViewer(TabInfo* tab, size_t idx) {
    if (!HasPermission(Perm_DiskAccess) || !tab || !file::Exists(tab->filePath))
        return false;
    for (size_t i = 0; i < gGlobalPrefs->externalViewers->size() && i <= idx; i++) {
        ExternalViewer* ev = gGlobalPrefs->externalViewers->at(i);
        // cf. AppendExternalViewersToMenu in Menu.cpp
        if (!ev->commandLine || ev->filter && !str::Eq(ev->filter, L"*") && !path::Match(tab->filePath, ev->filter))
            idx++;
    }
    if (idx >= gGlobalPrefs->externalViewers->size() || !gGlobalPrefs->externalViewers->at(idx)->commandLine)
        return false;

    ExternalViewer* ev = gGlobalPrefs->externalViewers->at(idx);
    WStrVec args;
    ParseCmdLine(ev->commandLine, args, 2);
    if (args.size() == 0 || !file::Exists(args.at(0)))
        return false;

    // if the command line contains %p, it's replaced with the current page number
    // if it contains %1, it's replaced with the file path (else the file path is appended)
    const WCHAR* cmdLine = args.size() > 1 ? args.at(1) : L"\"%1\"";
    AutoFreeWstr params;
    if (str::Find(cmdLine, L"%p")) {
        AutoFreeWstr pageNoStr(str::Format(L"%d", tab->ctrl ? tab->ctrl->CurrentPageNo() : 0));
        params.Set(str::Replace(cmdLine, L"%p", pageNoStr));
        cmdLine = params;
    }
    if (str::Find(cmdLine, L"%1"))
        params.Set(str::Replace(cmdLine, L"%1", tab->filePath));
    else
        params.Set(str::Format(L"%s \"%s\"", cmdLine, tab->filePath.get()));
    return LaunchFile(args.at(0), params);
}

#define DEFINE_GUID_STATIC(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}
DEFINE_GUID_STATIC(CLSID_SendMail, 0x9E56BE60, 0xC50F, 0x11CF, 0x9A, 0x2C, 0x00, 0xA0, 0xC9, 0x0A, 0x90, 0xCE);

bool CanSendAsEmailAttachment(TabInfo* tab) {
    // Requirements: a valid filename and access to SendMail's IDropTarget interface
    if (!CanViewExternally(tab))
        return false;

    ScopedComPtr<IDropTarget> pDropTarget;
    return pDropTarget.Create(CLSID_SendMail);
}

bool SendAsEmailAttachment(TabInfo* tab, HWND hwndParent) {
    if (!tab || !CanSendAsEmailAttachment(tab))
        return false;

    // We use the SendTo drop target provided by SendMail.dll, which should ship with all
    // commonly used Windows versions, instead of MAPISendMail, which doesn't support
    // Unicode paths and might not be set up on systems not having Microsoft Outlook installed.
    ScopedComPtr<IDataObject> pDataObject(GetDataObjectForFile(tab->filePath, hwndParent));
    if (!pDataObject)
        return false;

    ScopedComPtr<IDropTarget> pDropTarget;
    if (!pDropTarget.Create(CLSID_SendMail))
        return false;

    POINTL pt = {0, 0};
    DWORD dwEffect = 0;
    pDropTarget->DragEnter(pDataObject, MK_LBUTTON, pt, &dwEffect);
    HRESULT hr = pDropTarget->Drop(pDataObject, MK_LBUTTON, pt, &dwEffect);
    return SUCCEEDED(hr);
}
