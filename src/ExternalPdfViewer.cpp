/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "WinUtil.h"

#include "ExternalPdfViewer.h"

#include "EngineManager.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"

static TCHAR *GetAcrobatPath()
{
    // Try Adobe Acrobat as a fall-back, if the Reader isn't installed
    ScopedMem<TCHAR> path(ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\AcroRd32.exe"), NULL));
    if (!path)
        path.Set(ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Acrobat.exe"), NULL));
    if (path && file::Exists(path))
        return path.StealData();
    return NULL;
}

static TCHAR *GetFoxitPath()
{
    ScopedMem<TCHAR> path(ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Foxit Reader"), _T("DisplayIcon")));
    if (path && file::Exists(path))
        return path.StealData();
    // Registry value for Foxit 5 (and maybe later)
    path.Set(ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Foxit Reader_is1"), _T("DisplayIcon")));
    if (path && file::Exists(path))
        return path.StealData();
    return NULL;
}

static TCHAR *GetPDFXChangePath()
{
    ScopedMem<TCHAR> path(ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\Tracker Software\\PDFViewer"), _T("InstallPath")));
    if (!path)
        path.Set(ReadRegStr(HKEY_CURRENT_USER,  _T("Software\\Tracker Software\\PDFViewer"), _T("InstallPath")));
    if (!path)
        return false;
    ScopedMem<TCHAR> exePath(path::Join(path, _T("PDFXCview.exe")));
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
    if (!CanViewExternally(win) || win && !win->IsPdf())
        return false;
    ScopedMem<TCHAR> path(GetFoxitPath());
    return path != NULL;
}

bool ViewWithFoxit(WindowInfo *win, TCHAR *args)
{
    if (!CanViewWithFoxit(win))
        return false;

    ScopedMem<TCHAR> exePath(GetFoxitPath());
    if (!exePath)
        return false;
    if (!args)
        args = _T("");

    // Foxit cmd-line format:
    // [PDF filename] [-n <page number>] [-pwd <password>] [-z <zoom>]
    // TODO: Foxit allows passing password and zoom
    ScopedMem<TCHAR> params(str::Format(_T("%s \"%s\" -n %d"), args, win->loadedFilePath, win->dm->CurrentPageNo()));
    return LaunchFile(exePath, params);
}

bool CanViewWithPDFXChange(WindowInfo *win)
{
    // Requirements: a valid filename and a valid path to PDF X-Change
    if (!CanViewExternally(win) || win && !win->IsPdf())
        return false;
    ScopedMem<TCHAR> path(GetPDFXChangePath());
    return path != NULL;
}

bool ViewWithPDFXChange(WindowInfo *win, TCHAR *args)
{
    if (!CanViewWithPDFXChange(win))
        return false;

    ScopedMem<TCHAR> exePath(GetPDFXChangePath());
    if (!exePath)
        return false;
    if (!args)
        args = _T("");

    // PDFXChange cmd-line format:
    // [/A "param=value [&param2=value ..."] [PDF filename] 
    // /A params: page=<page number>
    ScopedMem<TCHAR> params(str::Format(_T("%s /A \"page=%d\" \"%s\""), args, win->dm->CurrentPageNo(), win->loadedFilePath));
    return LaunchFile(exePath, params);
}

bool CanViewWithAcrobat(WindowInfo *win)
{
    // Requirements: a valid filename and a valid path to Adobe Reader
    if (!CanViewExternally(win) || win && !win->IsPdf())
        return false;
    ScopedMem<TCHAR> exePath(GetAcrobatPath());
    return exePath != NULL;
}

bool ViewWithAcrobat(WindowInfo *win, TCHAR *args)
{
    if (!CanViewWithAcrobat(win))
        return false;

    ScopedMem<TCHAR> exePath(GetAcrobatPath());
    if (!exePath)
        return false;

    if (!args)
        args = _T("");

    ScopedMem<TCHAR> params(NULL);
    // Command line format for version 6 and later:
    //   /A "page=%d&zoom=%.1f,%d,%d&..." <filename>
    // see http://www.adobe.com/devnet/acrobat/pdfs/pdf_open_parameters.pdf
    //   /P <filename>
    // see http://www.adobe.com/devnet/acrobat/pdfs/Acrobat_SDK_developer_faq.pdf#page=24
    // TODO: Also set zoom factor and scroll to current position?
    if (win->dm && HIWORD(GetFileVersion(exePath)) >= 6)
        params.Set(str::Format(_T("/A \"page=%d\" %s \"%s\""), win->dm->CurrentPageNo(), args, win->dm->FileName()));
    else
        params.Set(str::Format(_T("%s \"%s\""), args, win->loadedFilePath));

    return LaunchFile(exePath, params);
}
