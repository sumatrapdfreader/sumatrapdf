/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "ExternalPdfViewer.h"

#include "AppPrefs.h"
#include "CmdLineParser.h"
#include "FileUtil.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "WinUtil.h"

static WCHAR *GetAcrobatPath()
{
    // Try Adobe Acrobat as a fall-back, if the Reader isn't installed
    ScopedMem<WCHAR> path(ReadRegStr(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\AcroRd32.exe", NULL));
    if (!path)
        path.Set(ReadRegStr(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Acrobat.exe", NULL));
    if (path && file::Exists(path))
        return path.StealData();
    return NULL;
}

static WCHAR *GetFoxitPath()
{
    ScopedMem<WCHAR> path(ReadRegStr(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Foxit Reader", L"DisplayIcon"));
    if (path && file::Exists(path))
        return path.StealData();
    // Registry value for Foxit 5 (and maybe later)
    path.Set(ReadRegStr(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Foxit Reader_is1", L"DisplayIcon"));
    if (path && file::Exists(path))
        return path.StealData();
    // Registry value for Foxit 5.5 MSI installer
    path.Set(ReadRegStr(HKEY_LOCAL_MACHINE, L"Software\\Foxit Software\\Foxit Reader", L"InstallPath"));
    if (path)
        path.Set(path::Join(path, L"Foxit Reader.exe"));
    if (path && file::Exists(path))
        return path.StealData();
    return NULL;
}

static WCHAR *GetPDFXChangePath()
{
    ScopedMem<WCHAR> path(ReadRegStr(HKEY_LOCAL_MACHINE, L"Software\\Tracker Software\\PDFViewer", L"InstallPath"));
    if (!path)
        path.Set(ReadRegStr(HKEY_CURRENT_USER, L"Software\\Tracker Software\\PDFViewer", L"InstallPath"));
    if (!path)
        return false;
    ScopedMem<WCHAR> exePath(path::Join(path, L"PDFXCview.exe"));
    if (file::Exists(exePath))
        return exePath.StealData();
    return NULL;
}

static WCHAR *GetXPSViewerPath()
{
    // the XPS-Viewer seems to always be installed into %WINDIR%\system32
    WCHAR buffer[MAX_PATH];
    UINT res = GetSystemDirectory(buffer, dimof(buffer));
    if (!res || res >= dimof(buffer))
        return NULL;
    ScopedMem<WCHAR> exePath(path::Join(buffer, L"xpsrchvw.exe"));
    if (file::Exists(exePath))
        return exePath.StealData();
#ifndef _WIN64
    // Wow64 redirects access to system32 to syswow64 instead, so we
    // disable file system redirection using the recommended method from
    // http://msdn.microsoft.com/en-us/library/aa384187(v=vs.85).aspx
    if (IsRunningInWow64()) {
        res = GetWindowsDirectory(buffer, dimof(buffer));
        if (!res || res >= dimof(buffer))
            return NULL;
        exePath.Set(path::Join(buffer, L"Sysnative\\xpsrchvw.exe"));
        if (file::Exists(exePath))
            return exePath.StealData();
    }
#endif
    return NULL;
}

static WCHAR *GetHtmlHelpPath()
{
    // the Html Help viewer seems to be installed either into %WINDIR% or %WINDIR%\system32
    WCHAR buffer[MAX_PATH];
    UINT res = GetWindowsDirectory(buffer, dimof(buffer));
    if (!res || res >= dimof(buffer))
        return NULL;
    ScopedMem<WCHAR> exePath(path::Join(buffer, L"hh.exe"));
    if (file::Exists(exePath))
        return exePath.StealData();
    res = GetSystemDirectory(buffer, dimof(buffer));
    if (!res || res >= dimof(buffer))
        return NULL;
    exePath.Set(path::Join(buffer, L"hh.exe"));
    if (file::Exists(exePath))
        return exePath.StealData();
    return NULL;
}

bool CanViewExternally(WindowInfo *win)
{
    if (!HasPermission(Perm_DiskAccess))
        return false;
    if (!win || win->IsAboutWindow())
        return true;
    return file::Exists(win->loadedFilePath);
}

bool CanViewWithFoxit(WindowInfo *win)
{
    // Requirements: a valid filename and a valid path to Foxit
    if (win && win->IsNotPdf() || !CanViewExternally(win))
        return false;
    ScopedMem<WCHAR> path(GetFoxitPath());
    return path != NULL;
}

bool ViewWithFoxit(WindowInfo *win, WCHAR *args)
{
    if (!CanViewWithFoxit(win))
        return false;

    ScopedMem<WCHAR> exePath(GetFoxitPath());
    if (!exePath)
        return false;
    if (!args)
        args = L"";

    // Foxit cmd-line format:
    // [PDF filename] [-n <page number>] [-pwd <password>] [-z <zoom>]
    // TODO: Foxit allows passing password and zoom
    ScopedMem<WCHAR> params;
    if (win->IsDocLoaded())
        params.Set(str::Format(L"\"%s\" %s -n %d", win->dm->FilePath(), args, win->dm->CurrentPageNo()));
    else
        params.Set(str::Format(L"\"%s\" %s", win->loadedFilePath, args));
    return LaunchFile(exePath, params);
}

bool CanViewWithPDFXChange(WindowInfo *win)
{
    // Requirements: a valid filename and a valid path to PDF X-Change
    if (win && win->IsNotPdf() || !CanViewExternally(win))
        return false;
    ScopedMem<WCHAR> path(GetPDFXChangePath());
    return path != NULL;
}

bool ViewWithPDFXChange(WindowInfo *win, WCHAR *args)
{
    if (!CanViewWithPDFXChange(win))
        return false;

    ScopedMem<WCHAR> exePath(GetPDFXChangePath());
    if (!exePath)
        return false;
    if (!args)
        args = L"";

    // PDFXChange cmd-line format:
    // [/A "param=value [&param2=value ..."] [PDF filename]
    // /A params: page=<page number>
    ScopedMem<WCHAR> params;
    if (win->IsDocLoaded())
        params.Set(str::Format(L"%s /A \"page=%d\" \"%s\"", args, win->dm->CurrentPageNo(), win->dm->FilePath()));
    else
        params.Set(str::Format(L"%s \"%s\"", args, win->loadedFilePath));
    return LaunchFile(exePath, params);
}

bool CanViewWithAcrobat(WindowInfo *win)
{
    // Requirements: a valid filename and a valid path to Adobe Reader
    if (win && win->IsNotPdf() || !CanViewExternally(win))
        return false;
    ScopedMem<WCHAR> exePath(GetAcrobatPath());
    return exePath != NULL;
}

bool ViewWithAcrobat(WindowInfo *win, WCHAR *args)
{
    if (!CanViewWithAcrobat(win))
        return false;

    ScopedMem<WCHAR> exePath(GetAcrobatPath());
    if (!exePath)
        return false;

    if (!args)
        args = L"";

    ScopedMem<WCHAR> params;
    // Command line format for version 6 and later:
    //   /A "page=%d&zoom=%.1f,%d,%d&..." <filename>
    // see http://www.adobe.com/devnet/acrobat/pdfs/pdf_open_parameters.pdf#page=5
    //   /P <filename>
    // see http://www.adobe.com/devnet/acrobat/pdfs/Acrobat_SDK_developer_faq.pdf#page=24
    // TODO: Also set zoom factor and scroll to current position?
    if (win->IsDocLoaded() && HIWORD(GetFileVersion(exePath)) >= 6)
        params.Set(str::Format(L"/A \"page=%d\" %s \"%s\"", win->dm->CurrentPageNo(), args, win->dm->FilePath()));
    else
        params.Set(str::Format(L"%s \"%s\"", args, win->loadedFilePath));

    return LaunchFile(exePath, params);
}

bool CanViewWithXPSViewer(WindowInfo *win)
{
    // Requirements: a valid filename and a valid path to XPS-Viewer
    if (!win || win->IsAboutWindow() || !CanViewExternally(win))
        return false;
    // allow viewing with XPS-Viewer, if either an XPS document is loaded...
    if (win->IsDocLoaded() && win->dm->engineType != Engine_XPS)
        return false;
    // or a file ending in .xps or .oxps has failed to be loaded
    if (!win->IsDocLoaded() && !str::EndsWithI(win->loadedFilePath, L".xps") && !str::EndsWithI(win->loadedFilePath, L".oxps"))
        return false;
    ScopedMem<WCHAR> path(GetXPSViewerPath());
    return path != NULL;
}

bool ViewWithXPSViewer(WindowInfo *win, WCHAR *args)
{
    if (!CanViewWithXPSViewer(win))
        return false;

    ScopedMem<WCHAR> exePath(GetXPSViewerPath());
    if (!exePath)
        return false;

    if (!args)
        args = L"";

    ScopedMem<WCHAR> params;
    if (win->IsDocLoaded())
        params.Set(str::Format(L"%s \"%s\"", args, win->dm->FilePath()));
    else
        params.Set(str::Format(L"%s \"%s\"", args, win->loadedFilePath));
    return LaunchFile(exePath, params);
}

bool CanViewWithHtmlHelp(WindowInfo *win)
{
    // Requirements: a valid filename and a valid path to HTML Help
    if (!win || win->IsAboutWindow() || !CanViewExternally(win))
        return false;
    // allow viewing with HTML Help, if either an CHM document is loaded...
    if (win->IsDocLoaded() && win->dm->engineType != Engine_Chm && win->dm->engineType != Engine_Chm2)
        return false;
    // or a file ending in .chm has failed to be loaded
    if (!win->IsDocLoaded() && !str::EndsWithI(win->loadedFilePath, L".chm"))
        return false;
    ScopedMem<WCHAR> path(GetHtmlHelpPath());
    return path != NULL;
}

bool ViewWithHtmlHelp(WindowInfo *win, WCHAR *args)
{
    if (!CanViewWithHtmlHelp(win))
        return false;

    ScopedMem<WCHAR> exePath(GetHtmlHelpPath());
    if (!exePath)
        return false;

    if (!args)
        args = L"";

    ScopedMem<WCHAR> params;
    if (win->IsDocLoaded())
        params.Set(str::Format(L"%s \"%s\"", args, win->dm->FilePath()));
    else
        params.Set(str::Format(L"%s \"%s\"", args, win->loadedFilePath));
    return LaunchFile(exePath, params);
}

bool ViewWithExternalViewer(size_t idx, const WCHAR *filePath, int pageNo)
{
    if (!HasPermission(Perm_DiskAccess) || !file::Exists(filePath))
        return false;
    for (size_t i = 0; i < gGlobalPrefs->externalViewers->Count() && i <= idx; i++) {
        ExternalViewer *ev = gGlobalPrefs->externalViewers->At(i);
        if (ev->filter && !str::Eq(ev->filter, L"*") && !(filePath && path::Match(filePath, ev->filter)))
            idx++;
    }
    if (idx >= gGlobalPrefs->externalViewers->Count() || !gGlobalPrefs->externalViewers->At(idx)->commandLine)
        return false;

    ExternalViewer *ev = gGlobalPrefs->externalViewers->At(idx);
    WStrVec args;
    ParseCmdLine(ev->commandLine, args, 2);
    if (args.Count() == 0 || !file::Exists(args.At(0)))
        return false;

    // if the command line contains %p, it's replaced with the current page number
    // if it contains %1, it's replaced with the file path (else the file path is appended)
    const WCHAR *cmdLine = args.Count() > 1 ? args.At(1) : L"\"%1\"";
    ScopedMem<WCHAR> pageNoStr(str::Format(L"%d", pageNo));
    ScopedMem<WCHAR> params(str::Replace(cmdLine, L"%p", pageNoStr));
    if (str::Find(params, L"%1"))
        params.Set(str::Replace(params, L"%1", filePath));
    else
        params.Set(str::Format(L"%s \"%s\"", params, filePath));
    return LaunchFile(args.At(0), params);
}
