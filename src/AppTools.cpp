/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "translations.h"
#include "AppTools.h"
#include "CmdLineParser.h"
#include "Version.h"
#include <shlobj.h>
#include <shlwapi.h>

// the only valid chars are 0-9, . and newlines.
// a valid version has to match the regex /^\d+(\.\d+)*(\r?\n)?$/
// Return false if it contains anything else.
bool IsValidProgramVersion(char *txt)
{
    if (!ChrIsDigit(*txt))
        return false;

    for (; *txt; txt++) {
        if (ChrIsDigit(*txt))
            continue;
        if (*txt == '.' && ChrIsDigit(*(txt + 1)))
            continue;
        if (*txt == '\r' && *(txt + 1) == '\n')
            continue;
        if (*txt == '\n' && !*(txt + 1))
            continue;
        return false;
    }

    return true;
}

// extract the next (positive) number from the string *txt
static unsigned int ExtractNextNumber(TCHAR **txt)
{
    unsigned int val = 0;
    const TCHAR *next = str::Parse(*txt, _T("%u%?."), &val);
    *txt = next ? (TCHAR *)next : *txt + str::Len(*txt);
    return val;
}

// compare two version string. Return 0 if they are the same,
// > 0 if the first is greater than the second and < 0 otherwise.
// e.g. 
//   0.9.3.900 is greater than 0.9.3
//   1.09.300 is greater than 1.09.3 which is greater than 1.9.1
//   1.2.0 is the same as 1.2
int CompareVersion(TCHAR *txt1, TCHAR *txt2)
{
    while (*txt1 || *txt2) {
        unsigned int v1 = ExtractNextNumber(&txt1);
        unsigned int v2 = ExtractNextNumber(&txt2);
        if (v1 != v2)
            return v1 - v2;
    }

    return 0;
}

/* Return false if this program has been started from "Program Files" directory
   (which is an indicator that it has been installed) or from the last known
   location of a SumatraPDF installation (HKLM\Software\SumatraPDF\Install_Dir) */
bool IsRunningInPortableMode()
{
    // cache the result so that it will be consistent during the lifetime of the process
    static int sCacheIsPortable = -1; // -1 == uninitialized, 0 == installed, 1 == portable
    if (sCacheIsPortable != -1)
        return sCacheIsPortable != 0;
    sCacheIsPortable = 1;

    ScopedMem<TCHAR> exePath(GetExePath());
    if (!exePath)
        return true;

    // if we can't get a path, assume we're not running from "Program Files"
    ScopedMem<TCHAR> installedPath(NULL);
    installedPath.Set(ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\") APP_NAME_STR, _T("Install_Dir")));
    if (!installedPath)
        installedPath.Set(ReadRegStr(HKEY_CURRENT_USER, _T("Software\\") APP_NAME_STR, _T("Install_Dir")));
    if (installedPath) {
        if (!str::EndsWithI(installedPath.Get(), _T(".exe")))
            installedPath.Set(path::Join(installedPath.Get(), path::GetBaseName(exePath)));
        if (path::IsSame(installedPath, exePath)) {
            sCacheIsPortable = 0;
            return false;
        }
    }

    TCHAR programFilesDir[MAX_PATH];
    programFilesDir[0] = '\0';
    BOOL ok = SHGetSpecialFolderPath(NULL, programFilesDir, CSIDL_PROGRAM_FILES, FALSE);
    if (!ok)
        return true;

    // check if one of the exePath's parent directories is "Program Files"
    // (or a junction to it)
    TCHAR *baseName;
    while ((baseName = (TCHAR*)path::GetBaseName(exePath)) > exePath) {
        baseName[-1] = '\0';
        if (path::IsSame(programFilesDir, exePath)) {
            sCacheIsPortable = 0;
            return false;
        }
    }

    return true;
}

/* Generate the full path for a filename used by the app in the userdata path. */
/* Caller needs to free() the result. */
TCHAR *AppGenDataFilename(TCHAR *pFilename)
{
    ScopedMem<TCHAR> path;
    if (IsRunningInPortableMode()) {
        /* Use the same path as the binary */
        ScopedMem<TCHAR> exePath(GetExePath());
        if (exePath) {
            assert(exePath[0]);
            path.Set(path::GetDir(exePath));
        }
    } else {
        /* Use %APPDATA% */
        TCHAR dir[MAX_PATH];
        dir[0] = '\0';
        BOOL ok = SHGetSpecialFolderPath(NULL, dir, CSIDL_APPDATA, TRUE);
        if (ok) {
            path.Set(path::Join(dir, APP_NAME_STR));
            if (path && !dir::Create(path))
                path.Set(NULL);
        }
    }

    assert(path && pFilename);
    if (!path || !pFilename)
        return NULL;

    return path::Join(path, pFilename);
}

// Updates the drive letter for a path that could have been on a removable drive,
// if that same path can be found on a different removable drive
void AdjustRemovableDriveLetter(TCHAR *path)
{
    // Don't bother if the file path is still valid
    if (file::Exists(path))
        return;
    // Don't bother for files on non-removable drives
    if (!path::IsOnRemovableDrive(path))
        return;

    // Iterate through all (other) removable drives and try to find the file there
    TCHAR szDrive[] = _T("A:\\");
    TCHAR origDrive = path[0];
    for (DWORD driveMask = GetLogicalDrives(); driveMask; driveMask >>= 1) {
        if ((driveMask & 1) && szDrive[0] != origDrive && path::IsOnRemovableDrive(szDrive)) {
            path[0] = szDrive[0];
            if (file::Exists(path))
                return;
        }
        szDrive[0]++;
    }
    path[0] = origDrive;
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
#define REG_CLASSES_APP     _T("Software\\Classes\\") APP_NAME_STR
#define REG_CLASSES_PDF     _T("Software\\Classes\\.pdf")

#define REG_EXPLORER_PDF_EXT _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf")

void DoAssociateExeWithPdfExtension(HKEY hkey)
{
    ScopedMem<TCHAR> exePath(GetExePath());
    if (!exePath)
        return;

    ScopedMem<TCHAR> prevHandler(NULL);
    // Remember the previous default app for the Uninstaller
    prevHandler.Set(ReadRegStr(hkey, REG_CLASSES_PDF, NULL));
    if (prevHandler && !str::Eq(prevHandler, APP_NAME_STR))
        WriteRegStr(hkey, REG_CLASSES_APP, _T("previous.pdf"), prevHandler);

    WriteRegStr(hkey, REG_CLASSES_APP, NULL, _TR("PDF Document"));
    TCHAR *icon_path = str::Join(exePath, _T(",1"));
    WriteRegStr(hkey, REG_CLASSES_APP _T("\\DefaultIcon"), NULL, icon_path);
    free(icon_path);

    WriteRegStr(hkey, REG_CLASSES_APP _T("\\shell"), NULL, _T("open"));

    ScopedMem<TCHAR> cmdPath(str::Format(_T("\"%s\" \"%%1\""), exePath)); // "${exePath}" "%1"
    bool ok = WriteRegStr(hkey, REG_CLASSES_APP _T("\\shell\\open\\command"), NULL, cmdPath);

    // also register for printing
    cmdPath.Set(str::Format(_T("\"%s\" -print-to-default \"%%1\""), exePath)); // "${exePath}" -print-to-default "%1"
    WriteRegStr(hkey, REG_CLASSES_APP _T("\\shell\\print\\command"), NULL, cmdPath);

    // also register for printing to specific printer
    cmdPath.Set(str::Format(_T("\"%s\" -print-to \"%%2\" \"%%1\""), exePath)); // "${exePath}" -print-to "%2" "%1"
    WriteRegStr(hkey, REG_CLASSES_APP _T("\\shell\\printto\\command"), NULL, cmdPath);

    // Only change the association if we're confident, that we've registered ourselves well enough
    if (ok) {
        WriteRegStr(hkey, REG_CLASSES_PDF, NULL, APP_NAME_STR);
        // TODO: also add SumatraPDF to the Open With lists for the other supported extensions?
        WriteRegStr(hkey, REG_CLASSES_PDF _T("\\OpenWithProgids"), APP_NAME_STR, _T(""));
        if (hkey == HKEY_CURRENT_USER) {
            WriteRegStr(hkey, REG_EXPLORER_PDF_EXT, _T("Progid"), APP_NAME_STR);
            SHDeleteValue(hkey, REG_EXPLORER_PDF_EXT, _T("Application"));
            DeleteRegKey(hkey, REG_EXPLORER_PDF_EXT _T("\\UserChoice"), true);
        }
    }
}

// verify that all registry entries that need to be set in order to associate
// Sumatra with .pdf files exist and have the right values
bool IsExeAssociatedWithPdfExtension()
{
    // this one doesn't have to exist but if it does, it must be APP_NAME_STR
    ScopedMem<TCHAR> tmp(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, _T("Progid")));
    if (tmp && !str::Eq(tmp, APP_NAME_STR))
        return false;

    // this one doesn't have to exist but if it does, it must be APP_NAME_STR.exe
    tmp.Set(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, _T("Application")));
    if (tmp && !str::EqI(tmp, APP_NAME_STR _T(".exe")))
        return false;

    // this one doesn't have to exist but if it does, it must be APP_NAME_STR
    tmp.Set(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT _T("\\UserChoice"), _T("Progid")));
    if (tmp && !str::Eq(tmp, APP_NAME_STR))
        return false;

    // HKEY_CLASSES_ROOT\.pdf default key must exist and be equal to APP_NAME_STR
    tmp.Set(ReadRegStr(HKEY_CLASSES_ROOT, _T(".pdf"), NULL));
    if (!str::Eq(tmp, APP_NAME_STR))
        return false;

    // HKEY_CLASSES_ROOT\SumatraPDF\shell\open default key must be: open
    tmp.Set(ReadRegStr(HKEY_CLASSES_ROOT, APP_NAME_STR _T("\\shell"), NULL));
    if (!str::EqI(tmp, _T("open")))
        return false;

    // HKEY_CLASSES_ROOT\SumatraPDF\shell\open\command default key must be: "${exe_path}" "%1"
    tmp.Set(ReadRegStr(HKEY_CLASSES_ROOT, APP_NAME_STR _T("\\shell\\open\\command"), NULL));
    if (!tmp)
        return false;

    CmdLineParser argList(tmp);
    ScopedMem<TCHAR> exePath(GetExePath());
    if (!exePath || !argList.Find(_T("%1")))
        return false;

    return path::IsSame(exePath, argList.At(0));
}

// caller needs to free() the result
TCHAR *ExtractFilenameFromURL(const TCHAR *url)
{
    ScopedMem<TCHAR> urlName(str::Dup(url));
    // try to extract the file name from the URL (last path component before query or hash)
    str::TransChars(urlName, _T("/?#"), _T("\\\0\0"));
    urlName.Set(str::Dup(path::GetBaseName(urlName)));
    // unescape hex-escapes (these are usually UTF-8)
    if (str::FindChar(urlName, '%')) {
        ScopedMem<char> utf8Name(str::conv::ToUtf8(urlName));
        char *src = utf8Name, *dst = utf8Name;
        while (*src) {
            if (*src == '%' && isxdigit(*(src + 1)) && isxdigit(*(src + 2))) {
                char hex[3] = { *(src + 1), *(src + 2), '\0' };
                *dst++ = (char)strtoul(hex, NULL, 16);
                src += 3;
            }
            else
                *dst++ = *src++;
        }
        *dst = '\0';
        urlName.Set(str::conv::FromUtf8(utf8Name));
    }
    if (str::IsEmpty(urlName.Get()))
        return NULL;
    return urlName.StealData();
}

// files are considered untrusted, if they're either loaded from a
// non-file URL in plugin mode, or if they're marked as being from
// an untrusted zone (e.g. by the browser that's downloaded them)
bool IsUntrustedFile(const TCHAR *filePath, const TCHAR *fileURL)
{
    ScopedMem<TCHAR> protocol;
    if (fileURL && str::Parse(fileURL, _T("%S:"), &protocol))
        if (str::Len(protocol) > 1 && !str::EqI(protocol, _T("file")))
            return true;

    if (file::GetZoneIdentifier(filePath) >= URLZONE_INTERNET)
        return true;

    // check all parents of embedded files and ADSs as well
    ScopedMem<TCHAR> path(str::Dup(filePath));
    while (str::Len(path) > 2 && str::FindChar(path + 2, ':')) {
        *_tcsrchr(path, ':') = '\0';
        if (file::GetZoneIdentifier(path) >= URLZONE_INTERNET)
            return true;
    }

    return false;
}

// List of rules used to detect TeX editors.

// type of path information retrieved from the registy
enum EditorPathType {
    BinaryPath,         // full path to the editor's binary file
    BinaryDir,          // directory containing the editor's binary file
    SiblingPath,        // full path to a sibling file of the editor's binary file    
};

static struct {
    PTSTR          Name;                // Editor name
    EditorPathType Type;                // Type of the path information obtained from the registry
    HKEY           RegRoot;             // Root of the regkey
    PTSTR          RegKey;              // Registry key path
    PTSTR          RegValue;            // Registry value name
    PTSTR          BinaryFilename;      // Editor's binary file name
    PTSTR          InverseSearchArgs;   // Parameters to be passed to the editor;
                                        // use placeholder '%f' for path to source file and '%l' for line number.
} editor_rules[] = {
    _T("WinEdt"),             BinaryPath, HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\WinEdt.exe"), NULL,
                              _T("WinEdt.exe"), _T("\"[Open(|%f|);SelPar(%l,8)]\""),

    _T("WinEdt"),             BinaryDir, HKEY_CURRENT_USER, _T("Software\\WinEdt"), _T("Install Root"),
                              _T("WinEdt.exe"), _T("\"[Open(|%f|);SelPar(%l,8)]\""),

    _T("Notepad++"),          BinaryPath, HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\notepad++.exe"), NULL,
                              _T("WinEdt.exe"), _T("-n%l \"%f\""),

    _T("Notepad++"),          BinaryDir, HKEY_LOCAL_MACHINE, _T("Software\\Notepad++"), NULL,
                              _T("notepad++.exe"), _T("-n%l \"%f\""),

    _T("Notepad++"),          BinaryPath, HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Notepad++"), _T("DisplayIcon"),
                              _T("notepad++.exe"), _T("-n%l \"%f\""),

    _T("TeXnicCenter Alpha"), BinaryDir, HKEY_LOCAL_MACHINE, _T("Software\\ToolsCenter\\TeXnicCenterNT"), _T("AppPath"),
                              _T("TeXnicCenter.exe"), _T("/ddecmd \"[goto('%f', '%l')]\""),

    _T("TeXnicCenter Alpha"), BinaryDir, HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TeXnicCenter Alpha_is1"), _T("InstallLocation"),
                              _T("TeXnicCenter.exe"), _T("/ddecmd \"[goto('%f', '%l')]\""),

    _T("TeXnicCenter"),       BinaryDir, HKEY_LOCAL_MACHINE, _T("Software\\ToolsCenter\\TeXnicCenter"), _T("AppPath"),
                              _T("TEXCNTR.exe"), _T("/ddecmd \"[goto('%f', '%l')]\""),

    _T("TeXnicCenter"),       BinaryDir, HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TeXnicCenter_is1"), _T("InstallLocation"),
                              _T("TEXCNTR.exe"), _T("/ddecmd \"[goto('%f', '%l')]\""),

    _T("WinShell"),           BinaryDir, HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WinShell_is1"), _T("InstallLocation"),
                              _T("WinShell.exe"), _T("-c \"%f\" -l %l"),

    _T("Gvim"),               BinaryPath, HKEY_LOCAL_MACHINE, _T("Software\\Vim\\Gvim"), _T("path"),
                              _T("gvim.exe"), _T("\"%f\" +%l"),

    // TODO: add this rule only if the latex-suite for ViM is installed (http://vim-latex.sourceforge.net/documentation/latex-suite.txt)
    _T("Gvim+latex-suite"),   BinaryPath, HKEY_LOCAL_MACHINE, _T("Software\\Vim\\Gvim"), _T("path"),
                             _T("gvim.exe"), _T("-c \":RemoteOpen +%l %f\""),

    _T("Texmaker"),           SiblingPath, HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Texmaker"), _T("UninstallString"),
                              _T("texmaker.exe"), _T("\"%f\" -line %l"),

    _T("TeXworks"),           BinaryDir, HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{41DA4817-4D2A-4D83-AD02-6A2D95DC8DCB}_is1"), _T("InstallLocation"),
                              _T("TeXworks.exe"), _T("-p=%l  \"%f\""),

    // TODO: find a way to detect where emacs is installed
    //_T("ntEmacs"),            BinaryPath, HKEY_LOCAL_MACHINE, _T("???"), _T("???"),
    //                          _T("emacsclientw.exe"), _T("+%l \"%f\""),
};

// Detect TeX editors installed on the system and construct the
// corresponding inverse search commands.
//
// Parameters:
//      hwndCombo   -- (optional) handle to a combo list that will be filled with the list of possible inverse search commands.
// Returns:
//      the inverse search command of the first detected editor (the caller needs to free() the result).
LPTSTR AutoDetectInverseSearchCommands(HWND hwndCombo)
{
    LPTSTR firstEditor = NULL;
    ScopedMem<TCHAR> path(NULL);

    for (int i = 0; i < dimof(editor_rules); i++)
    {
        path.Set(ReadRegStr(editor_rules[i].RegRoot, editor_rules[i].RegKey, editor_rules[i].RegValue));
        if (!path)
            continue;

        TCHAR *exePath;
        if (editor_rules[i].Type == SiblingPath) {
            // remove file part
            ScopedMem<TCHAR> dir(path::GetDir(path));
            exePath = path::Join(dir, editor_rules[i].BinaryFilename);
        } else if (editor_rules[i].Type == BinaryDir)
            exePath = path::Join(path, editor_rules[i].BinaryFilename);
        else // if (editor_rules[i].Type == BinaryPath)
            exePath = str::Dup(path);

        TCHAR *editorCmd = str::Format(_T("\"%s\" %s"), exePath, editor_rules[i].InverseSearchArgs);
        free(exePath);

        if (!hwndCombo) {
            // no need to fill a combo box: return immeditately after finding an editor.
            return editorCmd;
        }

        if (!firstEditor)
            firstEditor = str::Dup(editorCmd);
        ComboBox_AddString(hwndCombo, editorCmd);
        free(editorCmd);

        // skip the remaining rules for this editor
        while (i + 1 < dimof(editor_rules) && str::Eq(editor_rules[i].Name, editor_rules[i+1].Name))
            i++;
    }

    // Fall back to notepad as a default handler
    if (!firstEditor) {
        firstEditor = str::Dup(_T("notepad %f"));
        if (hwndCombo)
            ComboBox_AddString(hwndCombo, firstEditor);
    }
    return firstEditor;
}

static HDDEDATA CALLBACK DdeCallback(UINT uType, UINT uFmt, HCONV hconv, HSZ hsz1,
    HSZ hsz2, HDDEDATA hdata, ULONG_PTR dwData1, ULONG_PTR dwData2)
{
    return 0;
}

void DDEExecute(LPCTSTR server, LPCTSTR topic, LPCTSTR command)
{
    DBG_OUT("DDEExecute(\"%s\",\"%s\",\"%s\")\n", server, topic, command);
    unsigned long inst = 0;
    HSZ hszServer = NULL, hszTopic = NULL;
    HCONV hconv = NULL;
    HDDEDATA hddedata = NULL;

    UINT result = DdeInitialize(&inst, &DdeCallback, APPCMD_CLIENTONLY, 0);
    if (result != DMLERR_NO_ERROR) {
        DBG_OUT("DDE communication could not be initiated %u.\n", result);
        goto exit;
    }
    hszServer = DdeCreateStringHandle(inst, server, CP_WINNEUTRAL);
    if (!hszServer) {
        DBG_OUT("DDE communication could not be initiated %u.\n", DdeGetLastError(inst));
        goto exit;
    }
    hszTopic = DdeCreateStringHandle(inst, topic, CP_WINNEUTRAL);
    if (!hszTopic) {
        DBG_OUT("DDE communication could not be initiated %u.\n", DdeGetLastError(inst));
        goto exit;
    }
    hconv = DdeConnect(inst, hszServer, hszTopic, 0);
    if (!hconv) {
        DBG_OUT("DDE communication could not be initiated %u\n.", DdeGetLastError(inst));
        goto exit;
    }
    hddedata = DdeCreateDataHandle(inst, (BYTE*)command, (DWORD)(str::Len(command) + 1) * sizeof(TCHAR), 0, 0, CF_T_TEXT, 0);
    if (!hddedata) {
        DBG_OUT("DDE communication could not be initiated %u.\n", DdeGetLastError(inst));
        goto exit;
    }
    HDDEDATA answer = DdeClientTransaction((BYTE*)hddedata, (DWORD)-1, hconv, 0, 0, XTYP_EXECUTE, 10000, 0);
    if (answer)
        DdeFreeDataHandle(answer);
    else
        DBG_OUT("DDE transaction failed %u.\n", DdeGetLastError(inst));

exit:
    DdeFreeDataHandle(hddedata);
    DdeDisconnect(hconv);
    DdeFreeStringHandle(inst, hszTopic);
    DdeFreeStringHandle(inst, hszServer);
    DdeUninitialize(inst);
}


#define UWM_DELAYED_SET_FOCUS (WM_APP + 1)
#define UWM_DELAYED_CTRL_BACK (WM_APP + 2)

// selects all text in an edit box if it's selected either
// through a keyboard shortcut or a non-selecting mouse click
// (or responds to Ctrl+Backspace as nowadays expected)
bool ExtendedEditWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
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
            ScopedMem<TCHAR> text(win::GetText(hwnd));
            int selStart = LOWORD(Edit_GetSel(hwnd)), selEnd = selStart;
            // remove the rectangle produced by Ctrl+Backspace
            if (selStart > 0 && text[selStart - 1] == '\x7F') {
                memmove(text + selStart - 1, text + selStart, str::Len(text + selStart - 1) * sizeof(WCHAR));
                win::SetText(hwnd, text);
                selStart = selEnd = selStart - 1;
            }
            // remove the previous word (and any spacing after it)
            for (; selStart > 0 && _istspace(text[selStart - 1]); selStart--);
            for (; selStart > 0 && !_istspace(text[selStart - 1]); selStart--);
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
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfo(MonitorFromRect(&r.ToRECT(), MONITOR_DEFAULTTONEAREST), &mi))
        SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);

    RectI work = RectI::FromRECT(mi.rcWork);
    // make sure that the window is neither too small nor bigger than the monitor
    if (r.dx < MIN_WIN_DX || r.dx > work.dx)
        r.dx = (int)min(work.dy * DEF_PAGE_RATIO, work.dx);
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
    r.dx = (int)min(r.dy * DEF_PAGE_RATIO, work.dx);
    r.x = (work.dx - r.dx) / 2;

    return r;
}
