/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// utils
#include "BaseUtil.h"
#include "WinDynCalls.h"
#include "CmdLineParser.h"
#include "DbgHelpDyn.h"
#include "FileUtil.h"
#include "WinUtil.h"
// ui
#include "AppTools.h"
#include "Translations.h"
#include "Version.h"

#define REG_PATH_UNINST L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" APP_NAME_STR

/* Returns true, if a Registry entry indicates that this executable has been
   created by an installer (and should be updated through an installer) */
bool HasBeenInstalled()
{
    std::unique_ptr<WCHAR> installedPath(ReadRegStr(HKEY_CURRENT_USER, REG_PATH_UNINST, L"InstallLocation"));
    // cf. GetInstallationDir() in installer\Installer.cpp
    if (!installedPath) {
        installedPath.reset(ReadRegStr(HKEY_LOCAL_MACHINE, REG_PATH_UNINST, L"InstallLocation"));
    }

    if (!installedPath) {
        return false;
    }

    std::unique_ptr<WCHAR> exePath(GetExePath());
    if (!exePath) {
        return false;
    }

    if (!str::EndsWithI(installedPath.get(), L".exe")) {
        installedPath.reset(path::Join(installedPath.get(), path::GetBaseName(exePath.get())));
    }
    return path::IsSame(installedPath.get(), exePath.get());
}

/* Return false if this program has been started from "Program Files" directory
   (which is an indicator that it has been installed) or from the last known
   location of a SumatraPDF installation: */
bool IsRunningInPortableMode()
{
    // cache the result so that it will be consistent during the lifetime of the process
    static int sCacheIsPortable = -1; // -1 == uninitialized, 0 == installed, 1 == portable
    if (sCacheIsPortable != -1) {
        return sCacheIsPortable != 0;
    }
    sCacheIsPortable = 1;

    if (HasBeenInstalled()) {
        sCacheIsPortable = 0;
        return false;
    }

    AutoFreeW exePath(GetExePath());
    AutoFreeW programFilesDir(GetSpecialFolder(CSIDL_PROGRAM_FILES));
    // if we can't get a path, assume we're not running from "Program Files"
    if (!exePath || !programFilesDir) {
        return true;
    }

    // check if one of the exePath's parent directories is "Program Files"
    // (or a junction to it)
    WCHAR *baseName;
    while ((baseName = (WCHAR*)path::GetBaseName(exePath)) > exePath) {
        baseName[-1] = '\0';
        if (path::IsSame(programFilesDir, exePath)) {
            sCacheIsPortable = 0;
            return false;
        }
    }

    return true;
}

static AutoFreeW gAppDataPath;

void SetAppDataPath(const WCHAR *path)
{
    gAppDataPath.Set(path::Normalize(path));
}

/* Generate the full path for a filename used by the app in the userdata path. */
/* Caller needs to free() the result. */
WCHAR *AppGenDataFilename(const WCHAR *fileName)
{
    if (!fileName)
        return nullptr;

    if (gAppDataPath && dir::Exists(gAppDataPath)) {
        return path::Join(gAppDataPath, fileName);
    }

    if (IsRunningInPortableMode()) {
        /* Use the same path as the binary */
        return path::GetAppPath(fileName);
    }

    /* Use %APPDATA% */
    AutoFreeW path(GetSpecialFolder(CSIDL_APPDATA, true));
    CrashIf(!path);
    if (!path)
        return nullptr;
    path.Set(path::Join(path, APP_NAME_STR));
    if (!path)
        return nullptr;
    bool ok = dir::Create(path);
    if (!ok)
        return nullptr;
    return path::Join(path, fileName);
}

/*
Structure of registry entries for associating Sumatra with PDF files.

The following paths exist under both HKEY_LOCAL_MACHINE and HKEY_CURRENT_USER.
HKCU has precedence over HKLM.

Software\Classes\.pdf default key is name of reg entry describing the app
  handling opening PDF files. In our case it's SumatraPDF
Software\Classes\.pdf\OpenWithProgids
  should contain SumatraPDF so that it's easier for the user to later
  restore SumatraPDF to become the default app through Windows Explorer,
  cf. http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx

Software\Classes\SumatraPDF\DefaultIcon = $exePath,1
  1 means the second icon resource within the executable
Software\Classes\SumatraPDF\shell\open\command = "$exePath" "%1"
  tells how to call sumatra to open PDF file. %1 is replaced by PDF file path

Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pdf\Progid
  should be SumatraPDF (FoxIt takes it over); only needed for HKEY_CURRENT_USER
  TODO: No other app seems to set this one, and only UserChoice seems to make
        a difference - is this still required for Windows XP?

Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pdf\Application
  should be SumatraPDF.exe; only needed for HKEY_CURRENT_USER
  Windows XP seems to use this instead of:

Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pdf\UserChoice\Progid
  should be SumatraPDF as well (also only needed for HKEY_CURRENT_USER);
  this key is used for remembering a user's choice with Explorer's Open With dialog
  and can't be written to - so we delete it instead!

HKEY_CLASSES_ROOT\.pdf\OpenWithList
  list of all apps that can be used to open PDF files. We don't touch that.

HKEY_CLASSES_ROOT\.pdf default comes from either HKCU\Software\Classes\.pdf or
HKLM\Software\Classes\.pdf (HKCU has priority over HKLM)

Note: When making changes below, please also adjust WriteExtendedFileExtensionInfo(),
UnregisterFromBeingDefaultViewer() and RemoveOwnRegistryKeys() in Installer.cpp.

*/
#define REG_CLASSES_APP     L"Software\\Classes\\" APP_NAME_STR
#define REG_CLASSES_PDF     L"Software\\Classes\\.pdf"

#define REG_WIN_CURR        L"Software\\Microsoft\\Windows\\CurrentVersion"
#define REG_EXPLORER_PDF_EXT REG_WIN_CURR L"\\Explorer\\FileExts\\.pdf"

void DoAssociateExeWithPdfExtension(HKEY hkey)
{
    AutoFreeW exePath(GetExePath());
    if (!exePath)
        return;

    AutoFreeW prevHandler(nullptr);
    // Remember the previous default app for the Uninstaller
    prevHandler.Set(ReadRegStr(hkey, REG_CLASSES_PDF, nullptr));
    if (prevHandler && !str::Eq(prevHandler, APP_NAME_STR))
        WriteRegStr(hkey, REG_CLASSES_APP, L"previous.pdf", prevHandler);

    WriteRegStr(hkey, REG_CLASSES_APP, nullptr, _TR("PDF Document"));
    WCHAR *icon_path = str::Join(exePath, L",1");
    WriteRegStr(hkey, REG_CLASSES_APP L"\\DefaultIcon", nullptr, icon_path);
    free(icon_path);

    WriteRegStr(hkey, REG_CLASSES_APP L"\\shell", nullptr, L"open");

    AutoFreeW cmdPath(str::Format(L"\"%s\" \"%%1\" %%*", exePath.Get())); // "${exePath}" "%1" %*
    bool ok = WriteRegStr(hkey, REG_CLASSES_APP L"\\shell\\open\\command", nullptr, cmdPath);

    // also register for printing
    cmdPath.Set(str::Format(L"\"%s\" -print-to-default \"%%1\"", exePath.Get())); // "${exePath}" -print-to-default "%1"
    WriteRegStr(hkey, REG_CLASSES_APP L"\\shell\\print\\command", nullptr, cmdPath);

    // also register for printing to specific printer
    cmdPath.Set(str::Format(L"\"%s\" -print-to \"%%2\" \"%%1\"", exePath.Get())); // "${exePath}" -print-to "%2" "%1"
    WriteRegStr(hkey, REG_CLASSES_APP L"\\shell\\printto\\command", nullptr, cmdPath);

    // Only change the association if we're confident, that we've registered ourselves well enough
    if (!ok)
        return;

    WriteRegStr(hkey, REG_CLASSES_PDF, nullptr, APP_NAME_STR);
    // TODO: also add SumatraPDF to the Open With lists for the other supported extensions?
    WriteRegStr(hkey, REG_CLASSES_PDF L"\\OpenWithProgids", APP_NAME_STR, L"");
    if (hkey == HKEY_CURRENT_USER) {
        WriteRegStr(hkey, REG_EXPLORER_PDF_EXT, L"Progid", APP_NAME_STR);
        CrashIf(hkey == 0); // to appease prefast
        SHDeleteValue(hkey, REG_EXPLORER_PDF_EXT, L"Application");
        DeleteRegKey(hkey, REG_EXPLORER_PDF_EXT L"\\UserChoice", true);
    }
}

// verify that all registry entries that need to be set in order to associate
// Sumatra with .pdf files exist and have the right values
bool IsExeAssociatedWithPdfExtension()
{
    // this one doesn't have to exist but if it does, it must be APP_NAME_STR
    AutoFreeW tmp(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, L"Progid"));
    if (tmp && !str::Eq(tmp, APP_NAME_STR))
        return false;

    // this one doesn't have to exist but if it does, it must be APP_NAME_STR.exe
    tmp.Set(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, L"Application"));
    if (tmp && !str::EqI(tmp, APP_NAME_STR L".exe"))
        return false;

    // this one doesn't have to exist but if it does, it must be APP_NAME_STR
    tmp.Set(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT L"\\UserChoice", L"Progid"));
    if (tmp && !str::Eq(tmp, APP_NAME_STR))
        return false;

    // HKEY_CLASSES_ROOT\.pdf default key must exist and be equal to APP_NAME_STR
    tmp.Set(ReadRegStr(HKEY_CLASSES_ROOT, L".pdf", nullptr));
    if (!str::Eq(tmp, APP_NAME_STR))
        return false;

    // HKEY_CLASSES_ROOT\SumatraPDF\shell\open default key must be: open
    tmp.Set(ReadRegStr(HKEY_CLASSES_ROOT, APP_NAME_STR L"\\shell", nullptr));
    if (!str::EqI(tmp, L"open"))
        return false;

    // HKEY_CLASSES_ROOT\SumatraPDF\shell\open\command default key must be: "${exe_path}" "%1"
    tmp.Set(ReadRegStr(HKEY_CLASSES_ROOT, APP_NAME_STR L"\\shell\\open\\command", nullptr));
    if (!tmp)
        return false;

    WStrVec argList;
    ParseCmdLine(tmp, argList);
    AutoFreeW exePath(GetExePath());
    if (!exePath || !argList.Contains(L"%1") || !str::Find(tmp, L"\"%1\""))
        return false;

    return path::IsSame(exePath, argList.At(0));
}

// List of rules used to detect TeX editors.

// type of path information retrieved from the registy
enum EditorPathType {
    BinaryPath,         // full path to the editor's binary file
    BinaryDir,          // directory containing the editor's binary file
    SiblingPath,        // full path to a sibling file of the editor's binary file
};

static struct {
    const WCHAR *  BinaryFilename;      // Editor's binary file name
    const WCHAR *  InverseSearchArgs;   // Parameters to be passed to the editor;
                                        // use placeholder '%f' for path to source file and '%l' for line number.
    EditorPathType Type;                // Type of the path information obtained from the registry
    HKEY           RegRoot;             // Root of the regkey
    const WCHAR *  RegKey;              // Registry key path
    const WCHAR *  RegValue;            // Registry value name
} editor_rules[] = {
    L"WinEdt.exe",      L"\"[Open(|%f|);SelPar(%l,8)]\"",   BinaryPath,
                        HKEY_LOCAL_MACHINE, REG_WIN_CURR L"\\App Paths\\WinEdt.exe", nullptr,
    L"WinEdt.exe",      L"\"[Open(|%f|);SelPar(%l,8)]\"",   BinaryDir,
                        HKEY_CURRENT_USER,  L"Software\\WinEdt", L"Install Root",
    L"notepad++.exe",   L"-n%l \"%f\"",                     BinaryPath,
                        HKEY_LOCAL_MACHINE, REG_WIN_CURR L"\\App Paths\\notepad++.exe", nullptr,
    L"notepad++.exe",   L"-n%l \"%f\"",                     BinaryDir,
                        HKEY_LOCAL_MACHINE, L"Software\\Notepad++", nullptr,
    L"notepad++.exe",   L"-n%l \"%f\"",                     BinaryPath,
                        HKEY_LOCAL_MACHINE, REG_WIN_CURR L"\\Uninstall\\Notepad++", L"DisplayIcon",
    L"TeXnicCenter.exe",L"/ddecmd \"[goto('%f', '%l')]\"",  BinaryDir,
                        HKEY_LOCAL_MACHINE, L"Software\\ToolsCenter\\TeXnicCenterNT", L"AppPath",
    L"TeXnicCenter.exe",L"/ddecmd \"[goto('%f', '%l')]\"",  BinaryDir,
                        HKEY_LOCAL_MACHINE, REG_WIN_CURR L"\\Uninstall\\TeXnicCenter_is1", L"InstallLocation",
    L"TeXnicCenter.exe",L"/ddecmd \"[goto('%f', '%l')]\"",  BinaryDir,
                        HKEY_LOCAL_MACHINE, REG_WIN_CURR L"\\Uninstall\\TeXnicCenter Alpha_is1", L"InstallLocation",
    L"TEXCNTR.exe",     L"/ddecmd \"[goto('%f', '%l')]\"",  BinaryDir,
                        HKEY_LOCAL_MACHINE, L"Software\\ToolsCenter\\TeXnicCenter", L"AppPath",
    L"TEXCNTR.exe",     L"/ddecmd \"[goto('%f', '%l')]\"",  BinaryDir,
                        HKEY_LOCAL_MACHINE, REG_WIN_CURR L"\\Uninstall\\TeXnicCenter_is1", L"InstallLocation",
    L"WinShell.exe",    L"-c \"%f\" -l %l",                 BinaryDir,
                        HKEY_LOCAL_MACHINE, REG_WIN_CURR L"\\Uninstall\\WinShell_is1", L"InstallLocation",
    L"gvim.exe",        L"\"%f\" +%l",                      BinaryPath,
                        HKEY_LOCAL_MACHINE, L"Software\\Vim\\Gvim", L"path",
    // TODO: add this rule only if the latex-suite for ViM is installed (http://vim-latex.sourceforge.net/documentation/latex-suite.txt)
    L"gvim.exe",        L"-c \":RemoteOpen +%l %f\"",       BinaryPath,
                        HKEY_LOCAL_MACHINE, L"Software\\Vim\\Gvim", L"path",
    L"texmaker.exe",    L"\"%f\" -line %l",                 SiblingPath,
                        HKEY_LOCAL_MACHINE, REG_WIN_CURR L"\\Uninstall\\Texmaker", L"UninstallString",
    L"TeXworks.exe",    L"-p=%l \"%f\"",                    BinaryDir,
                        HKEY_LOCAL_MACHINE, REG_WIN_CURR L"\\Uninstall\\{41DA4817-4D2A-4D83-AD02-6A2D95DC8DCB}_is1", L"InstallLocation",
    // TODO: find a way to detect where emacs is installed
    // L"emacsclientw.exe",L"+%l \"%f\"", BinaryPath, HKEY_LOCAL_MACHINE, L"???", L"???",
};

// Detect TeX editors installed on the system and construct the
// corresponding inverse search commands.
//
// Parameters:
//      hwndCombo   -- (optional) handle to a combo list that will be filled with the list of possible inverse search commands.
// Returns:
//      the inverse search command of the first detected editor (the caller needs to free() the result).
WCHAR *AutoDetectInverseSearchCommands(HWND hwndCombo)
{
    WCHAR *firstEditor = nullptr;
    WStrList foundExes;

    for (int i = 0; i < dimof(editor_rules); i++) {
        AutoFreeW path(ReadRegStr(editor_rules[i].RegRoot, editor_rules[i].RegKey, editor_rules[i].RegValue));
        if (!path)
            continue;

        AutoFreeW exePath;
        if (editor_rules[i].Type == SiblingPath) {
            // remove file part
            AutoFreeW dir(path::GetDir(path));
            exePath.Set(path::Join(dir, editor_rules[i].BinaryFilename));
        }
        else if (editor_rules[i].Type == BinaryDir)
            exePath.Set(path::Join(path, editor_rules[i].BinaryFilename));
        else // if (editor_rules[i].Type == BinaryPath)
            exePath.Set(path.StealData());
        // don't show duplicate entries
        if (foundExes.FindI(exePath) != -1)
            continue;
        // don't show inexistent paths (and don't try again for them)
        if (!file::Exists(exePath)) {
            foundExes.Append(exePath.StealData());
            continue;
        }

        AutoFreeW editorCmd(str::Format(L"\"%s\" %s", exePath.Get(), editor_rules[i].InverseSearchArgs));

        if (!hwndCombo) {
            // no need to fill a combo box: return immeditately after finding an editor.
            return editorCmd.StealData();
        }

        ComboBox_AddString(hwndCombo, editorCmd);
        if (!firstEditor)
            firstEditor = editorCmd.StealData();
        foundExes.Append(exePath.StealData());
    }

    // Fall back to notepad as a default handler
    if (!firstEditor) {
        firstEditor = str::Dup(L"notepad %f");
        if (hwndCombo)
            ComboBox_AddString(hwndCombo, firstEditor);
    }
    return firstEditor;
}

#define UWM_DELAYED_SET_FOCUS (WM_APP + 1)
#define UWM_DELAYED_CTRL_BACK (WM_APP + 2)

// selects all text in an edit box if it's selected either
// through a keyboard shortcut or a non-selecting mouse click
// (or responds to Ctrl+Backspace as nowadays expected)
bool ExtendedEditWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNUSED(lParam);

    static bool delayFocus = false;

    switch (message) {
    case WM_LBUTTONDOWN:
        delayFocus = GetFocus() != hwnd;
        return true;

    case WM_LBUTTONUP:
        if (delayFocus) {
            DWORD sel = Edit_GetSel(hwnd);
            if (LOWORD(sel) == HIWORD(sel))
                PostMessage(hwnd, UWM_DELAYED_SET_FOCUS, 0, 0);
            delayFocus = false;
        }
        return true;

    case WM_KILLFOCUS:
        return false; // for easier debugging (make setting a breakpoint possible)

    case WM_SETFOCUS:
        if (!delayFocus)
            PostMessage(hwnd, UWM_DELAYED_SET_FOCUS, 0, 0);
        return true;

    case UWM_DELAYED_SET_FOCUS:
        Edit_SelectAll(hwnd);
        return true;

    case WM_KEYDOWN:
        if (VK_BACK != wParam || !IsCtrlPressed() || IsShiftPressed())
            return false;
        PostMessage(hwnd, UWM_DELAYED_CTRL_BACK, 0, 0);
        return true;

    case UWM_DELAYED_CTRL_BACK:
        {
            AutoFreeW text(win::GetText(hwnd));
            int selStart = LOWORD(Edit_GetSel(hwnd)), selEnd = selStart;
            // remove the rectangle produced by Ctrl+Backspace
            if (selStart > 0 && text[selStart - 1] == '\x7F') {
                memmove(text + selStart - 1, text + selStart, str::Len(text + selStart - 1) * sizeof(WCHAR));
                win::SetText(hwnd, text);
                selStart = selEnd = selStart - 1;
            }
            // remove the previous word (and any spacing after it)
            for (; selStart > 0 && str::IsWs(text[selStart - 1]); selStart--);
            for (; selStart > 0 && !str::IsWs(text[selStart - 1]); selStart--);
            Edit_SetSel(hwnd, selStart, selEnd);
            SendMessage(hwnd, WM_CLEAR, 0, 0);
        }
        return true;

    default:
        return false;
    }
}

/* Default size for the window, happens to be american A4 size (I think) */
#define DEF_PAGE_RATIO          (612.0/792.0)

#define MIN_WIN_DX 50
#define MIN_WIN_DY 50

void EnsureAreaVisibility(RectI& r)
{
    // adjust to the work-area of the current monitor (not necessarily the primary one)
    RectI work = GetWorkAreaRect(r);

    // make sure that the window is neither too small nor bigger than the monitor
    if (r.dx < MIN_WIN_DX || r.dx > work.dx)
        r.dx = std::min((int)((double)work.dy * DEF_PAGE_RATIO), work.dx);
    if (r.dy < MIN_WIN_DY || r.dy > work.dy)
        r.dy = work.dy;

    // check whether the lower half of the window's title bar is
    // inside a visible working area
    int captionDy = GetSystemMetrics(SM_CYCAPTION);
    RectI halfCaption(r.x, r.y + captionDy / 2, r.dx, captionDy / 2);
    if (halfCaption.Intersect(work).IsEmpty())
        r = RectI(work.TL(), r.Size());
}

RectI GetDefaultWindowPos()
{
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    RectI work = RectI::FromRECT(workArea);

    RectI r = work;
    r.dx = std::min((int)((double)r.dy * DEF_PAGE_RATIO), work.dx);
    r.x = (work.dx - r.dx) / 2;

    return r;
}

void SaveCallstackLogs()
{
    char *s = dbghelp::GetCallstacks();
    if (!s)
        return;
    AutoFreeW filePath(AppGenDataFilename(L"callstacks.txt"));
    file::WriteAll(filePath.Get(), s, str::Len(s));
    free(s);
}
