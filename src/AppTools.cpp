/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "TStrUtil.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "LangMenuDef.h"
#include "translations.h"
#include "AppTools.h"
#include <shlobj.h>

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
        if (*txt == '\r'&& *(txt + 1) == '\n')
            continue;
        if (*txt == '\n' && !*(txt + 1))
            continue;
        return false;
    }

    return true;
}

// extract the next (positive) number from the string *txt
static int ExtractNextNumber(TCHAR **txt)
{
    int val = 0;
    // skip non numeric characters (should only be dots)
    for (; **txt && !ChrIsDigit(**txt); (*txt)++);
    for (; **txt && ChrIsDigit(**txt); (*txt)++)
        val = val * 10 + (**txt - '0');

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
        int v1 = ExtractNextNumber(&txt1);
        int v2 = ExtractNextNumber(&txt2);
        if (v1 != v2)
            return v1 - v2;
    }

    return 0;
}

const char *GuessLanguage()
{
    LANGID langId = GetUserDefaultUILanguage();
    LANGID langIdNoSublang = MAKELANGID(PRIMARYLANGID(langId), SUBLANG_NEUTRAL);
    const char *langName = NULL;

    // Either find the exact primary/sub lang id match, or a neutral sublang if it exists
    // (don't return any sublang for a given language, it might be too different)
    for (int i = 0; i < LANGS_COUNT; i++) {
        if (langId == g_langs[i]._langId)
            return g_langs[i]._langName;

        if (langIdNoSublang == g_langs[i]._langId)
            langName = g_langs[i]._langName;
        // continue searching after finding a match with a neutral sublanguage
    }

    return langName;
}


/* Return the full exe path of my own executable.
   Caller needs to free() the result. */
TCHAR *ExePathGet()
{
    TCHAR buf[MAX_PATH];
    buf[0] = 0;
    GetModuleFileName(NULL, buf, dimof(buf));
    return Path::Normalize(buf);
}

/* Return false if this program has been started from "Program Files" directory
   (which is an indicator that it has been installed) or from the last known
   location of a SumatraPDF installation (HKLM\Software\SumatraPDF\Install_Dir) */
bool IsRunningInPortableMode()
{
    TCHAR programFilesDir[MAX_PATH];
    ScopedMem<TCHAR> exePath(ExePathGet());

    // if we can't get a path, assume we're not running from "Program Files"

    bool ok = ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\") APP_NAME_STR, _T("Install_Dir"), programFilesDir, dimof(programFilesDir));
    if (!ok)
        ok = ReadRegStr(HKEY_CURRENT_USER, _T("Software\\") APP_NAME_STR, _T("Install_Dir"), programFilesDir, dimof(programFilesDir));
    if (ok && exePath) {
        if (!Str::EndsWithI(programFilesDir, _T(".exe"))) {
            ScopedMem<TCHAR> installedPath(Path::Join(programFilesDir, Path::GetBaseName(exePath)));
            if (Path::IsSame(installedPath, exePath))
                return false;
        }
        else if (Path::IsSame(programFilesDir, exePath))
            return false;
    }

    ok = SHGetSpecialFolderPath(NULL, programFilesDir, CSIDL_PROGRAM_FILES, FALSE);
    if (ok && exePath) {
        // check if one of the exePath's parent directories is "Program Files"
        // (or a junction to it)
        TCHAR *baseName;
        while ((baseName = (TCHAR*)Path::GetBaseName(exePath)) > exePath) {
            baseName[-1] = '\0';
            if (Path::IsSame(programFilesDir, exePath))
                return false;
        }
    }

    return true;
}

/* Generate the full path for a filename used by the app in the userdata path. */
/* Caller needs to free() the result. */
TCHAR *AppGenDataFilename(TCHAR *pFilename)
{
    assert(pFilename);
    if (!pFilename) return NULL;

    TCHAR * path = NULL;
    if (IsRunningInPortableMode()) {
        /* Use the same path as the binary */
        TCHAR *exePath = ExePathGet();
        if (exePath) {
            assert(exePath[0]);
            path = Path::GetDir(exePath);
            free(exePath);
        }
    } else {
        /* Use %APPDATA% */
        TCHAR dir[MAX_PATH];
        *dir = '\0';
        SHGetSpecialFolderPath(NULL, dir, CSIDL_APPDATA, TRUE);
        path = Path::Join(dir, APP_NAME_STR);
        if (path)
            _tmkdir(path);
    }
    if (!path)
        return NULL;

    TCHAR *filename = Path::Join(path, pFilename);
    free(path);

    return filename;
}

// Updates the drive letter for a path that could have been on a removable drive,
// if that same path can be found on a different removable drive
void AdjustRemovableDriveLetter(TCHAR *path)
{
    TCHAR szDrive[] = _T("?:\\"), origDrive;
    UINT driveType;
    DWORD driveMask;

    // Don't bother if the file path is still valid
    if (File::Exists(path))
        return;

    // Don't bother for invalid and non-removable drives
    szDrive[0] = toupper(path[0]);
    if (szDrive[0] < 'A' || szDrive[0] > 'Z')
        return;
    driveType = GetDriveType(szDrive);
    if (DRIVE_REMOVABLE != driveType && DRIVE_UNKNOWN != driveType && DRIVE_NO_ROOT_DIR != driveType)
        return;

    // Iterate through all (other) removable drives and try to find the file there
    szDrive[0] = 'A';
    origDrive = path[0];
    for (driveMask = GetLogicalDrives(); driveMask; driveMask >>= 1) {
        if ((driveMask & 1) && szDrive[0] != origDrive && GetDriveType(szDrive) == DRIVE_REMOVABLE) {
            path[0] = szDrive[0];
            if (File::Exists(path))
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

Software\Classes\SumatraPDF\DefaultIcon = $exePath,1
  1 means the second icon resource within the executable
Software\Classes\SumatraPDF\shell\open\command = "$exePath" "%1"
  tells how to call sumatra to open PDF file. %1 is replaced by PDF file path
Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pdf\Progid
  should be SumatraPDF (FoxIt takes it over); only needed for HKEY_CURRENT_USER

HKEY_CLASSES_ROOT\.pdf\OpenWithList
  list of all apps that can be used to open PDF files. We don't touch that.

HKEY_CLASSES_ROOT\.pdf default comes from either HKCU\Software\Classes\.pdf or
HKLM\Software\Classes\.pdf (HKCU has priority over HKLM)

Note: When making changes below, please also adjust
UnregisterFromBeingDefaultViewer() in Installer.cpp.
*/
void DoAssociateExeWithPdfExtension(HKEY hkey)
{
    TCHAR previousPdfHandler[MAX_PATH + 8];
    TCHAR *exePath = ExePathGet();
    if (!exePath)
        return;

    // Remember the previous default app for the Uninstaller
    bool ok = ReadRegStr(hkey, _T("Software\\Classes\\.pdf"), NULL, previousPdfHandler, dimof(previousPdfHandler));
    if (ok && !Str::Eq(previousPdfHandler, APP_NAME_STR)) {
        WriteRegStr(hkey, _T("Software\\Classes\\") APP_NAME_STR, _T("previous.pdf"), previousPdfHandler);
    }

    WriteRegStr(hkey, _T("Software\\Classes\\") APP_NAME_STR, NULL, _TR("PDF Document"));
    TCHAR *icon_path = Str::Join(exePath, _T(",1"));
    WriteRegStr(hkey, _T("Software\\Classes\\") APP_NAME_STR _T("\\DefaultIcon"), NULL, icon_path);
    free(icon_path);

    WriteRegStr(hkey, _T("Software\\Classes\\") APP_NAME_STR _T("\\shell"), NULL, _T("open"));

    ScopedMem<TCHAR> cmdPath(Str::Format(_T("\"%s\" \"%%1\""), exePath)); // "${exePath}" "%1"
    ok = WriteRegStr(hkey, _T("Software\\Classes\\") APP_NAME_STR _T("\\shell\\open\\command"), NULL, cmdPath);

    // also register for printing
    ScopedMem<TCHAR> printPath(Str::Format(_T("\"%s\" -print-to-default \"%%1\""), exePath)); // "${exePath}" -print-to-default "%1"
    WriteRegStr(hkey, _T("Software\\Classes\\") APP_NAME_STR _T("\\shell\\print\\command"), NULL, printPath);

    // also register for printing to specific printer
    ScopedMem<TCHAR> printToPath(Str::Format(_T("\"%s\" -print-to \"%%2\" \"%%1\""), exePath)); // "${exePath}" -print-to "%2" "%1"
    WriteRegStr(hkey, _T("Software\\Classes\\") APP_NAME_STR _T("\\shell\\printto\\command"), NULL, printToPath);

    // Only change the association if we're confident, that we've registered ourselves well enough
    if (ok) {
        WriteRegStr(hkey, _T("Software\\Classes\\.pdf"), NULL, APP_NAME_STR);
        if (hkey == HKEY_CURRENT_USER) {
            WriteRegStr(hkey, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf"), _T("Progid"), APP_NAME_STR);
        }
    }

    free(exePath);
}

// verify that all registry entries that need to be set in order to associate
// Sumatra with .pdf files exist and have the right values
bool IsExeAssociatedWithPdfExtension()
{
    TCHAR tmp[MAX_PATH + 8];
    bool ok;

    // this one doesn't have to exist but if it does, it must be APP_NAME_STR
    ok = ReadRegStr(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf"), _T("Progid"), tmp, dimof(tmp));
    if (ok && !Str::Eq(tmp, APP_NAME_STR))
        return false;

    // HKEY_CLASSES_ROOT\.pdf default key must exist and be equal to APP_NAME_STR
    ok = ReadRegStr(HKEY_CLASSES_ROOT, _T(".pdf"), NULL, tmp, dimof(tmp));
    if (!ok || !Str::Eq(tmp, APP_NAME_STR))
        return false;

    // HKEY_CLASSES_ROOT\SumatraPDF\shell\open default key must be: open
    ok = ReadRegStr(HKEY_CLASSES_ROOT, _T("SumatraPDF\\shell"), NULL, tmp, dimof(tmp));
    if (!ok || !Str::EqI(tmp, _T("open")))
        return false;

    // HKEY_CLASSES_ROOT\SumatraPDF\shell\open\command default key must be: "${exe_path}" "%1"
    ok = ReadRegStr(HKEY_CLASSES_ROOT, _T("SumatraPDF\\shell\\open\\command"), NULL, tmp, dimof(tmp));
    if (!ok)
        return false;

    bool same = false;
    TCHAR *openCommand = tmp;
    TCHAR *exePathReg = tstr_parse_possibly_quoted(&openCommand);
    TCHAR *exePath = ExePathGet();
    if (exePath && exePathReg && _tcsstr(openCommand, _T("\"%1\"")))
        same = Path::IsSame(exePath, exePathReg);
    free(exePath);
    free(exePathReg);

    return same;
}

bool GetAcrobatPath(TCHAR *bufOut, int bufCchSize)
{
    TCHAR path[MAX_PATH];
    // Try Adobe Acrobat as a fall-back, if the Reader isn't installed
    bool found = ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\AcroRd32.exe"), NULL, path, dimof(path)) ||
                 ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Acrobat.exe"), NULL, path, dimof(path));
    if (found)
        found = File::Exists(path);
    if (found && bufOut)
        Str::CopyTo(bufOut, bufCchSize, path);
    return found;
}

bool GetFoxitPath(TCHAR *bufOut, int bufCchSize)
{
    TCHAR path[MAX_PATH];
    bool found = ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Foxit Reader"), _T("DisplayIcon"), path, dimof(path));
    if (found)
        found = File::Exists(path);
    if (found && bufOut)
        Str::CopyTo(bufOut, bufCchSize, path);
    return found;
}

bool GetPDFXChangePath(TCHAR *bufOut, int bufCchSize)
{
    TCHAR path[MAX_PATH];
    bool found = ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\Tracker Software\\PDFViewer"), _T("InstallPath"), path, dimof(path)) ||
                 ReadRegStr(HKEY_CURRENT_USER,  _T("Software\\Tracker Software\\PDFViewer"), _T("InstallPath"), path, dimof(path));
    if (!found)
        return false;
    ScopedMem<TCHAR> exePath(Path::Join(path, _T("PDFXCview.exe")));
    found = File::Exists(exePath);
    if (found && bufOut)
        Str::CopyTo(bufOut, bufCchSize, exePath);
    return found;
}

// List of rules used to detect TeX editors.

// type of path information retrieved from the registy
typedef enum {
    BinaryPath,         // full path to the editor's binary file
    BinaryDir,          // directory containing the editor's binary file
    SiblingPath,        // full path to a sibling file of the editor's binary file    
} EditorPathType;

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
    TCHAR path[MAX_PATH];

    for (int i = 0; i < dimof(editor_rules); i++)
    {
        if (!ReadRegStr(editor_rules[i].RegRoot, editor_rules[i].RegKey, editor_rules[i].RegValue, path, dimof(path)))
            continue;

        TCHAR *exePath;
        if (editor_rules[i].Type == SiblingPath) {
            // remove file part
            ScopedMem<TCHAR> dir(Path::GetDir(path));
            exePath = Path::Join(dir, editor_rules[i].BinaryFilename);
        } else if (editor_rules[i].Type == BinaryDir)
            exePath = Path::Join(path, editor_rules[i].BinaryFilename);
        else // if (editor_rules[i].Type == BinaryPath)
            exePath = Str::Dup(path);

        TCHAR *editorCmd = Str::Format(_T("\"%s\" %s"), exePath, editor_rules[i].InverseSearchArgs);
        free(exePath);

        if (!hwndCombo) {
            // no need to fill a combo box: return immeditately after finding an editor.
            return editorCmd;
        }

        if (!firstEditor)
            firstEditor = Str::Dup(editorCmd);
        ComboBox_AddString(hwndCombo, editorCmd);
        free(editorCmd);

        // skip the remaining rules for this editor
        while (i + 1 < dimof(editor_rules) && Str::Eq(editor_rules[i].Name, editor_rules[i+1].Name))
            i++;
    }

    // Fall back to notepad as a default handler
    if (!firstEditor) {
        firstEditor = Str::Dup(_T("notepad %f"));
        if (hwndCombo)
            ComboBox_AddString(hwndCombo, firstEditor);
    }
    return firstEditor;
}

static HDDEDATA CALLBACK DdeCallback(UINT uType,
    UINT uFmt,
    HCONV hconv,
    HSZ hsz1,
    HSZ hsz2,
    HDDEDATA hdata,
    ULONG_PTR dwData1,
    ULONG_PTR dwData2)
{
    return 0;
}

void DDEExecute(LPCTSTR server, LPCTSTR topic, LPCTSTR command)
{
    DBG_OUT_T("DDEExecute(\"%s\",\"%s\",\"%s\")", server, topic, command);
    unsigned long inst = 0;
    HSZ hszServer = NULL, hszTopic = NULL;
    HCONV hconv = NULL;
    HDDEDATA hddedata = NULL;

    UINT result = DdeInitialize(&inst, &DdeCallback, APPCMD_CLIENTONLY, 0);
    if (result != DMLERR_NO_ERROR) {
        DBG_OUT("DDE communication could not be initiated %d.", result);
        goto exit;
    }
    hszServer = DdeCreateStringHandle(inst, server, CP_WINNEUTRAL);
    if (hszServer == 0) {
        DBG_OUT("DDE communication could not be initiated %u.", DdeGetLastError(inst));
        goto exit;
    }
    hszTopic = DdeCreateStringHandle(inst, topic, CP_WINNEUTRAL);
    if (hszTopic == 0) {
        DBG_OUT("DDE communication could not be initiated %u.", DdeGetLastError(inst));
        goto exit;
    }
    hconv = DdeConnect(inst, hszServer, hszTopic, 0);
    if (hconv == 0) {
        DBG_OUT("DDE communication could not be initiated %u.", DdeGetLastError(inst));
        goto exit;
    }
    hddedata = DdeCreateDataHandle(inst, (BYTE*)command, (DWORD)(Str::Len(command) + 1) * sizeof(TCHAR), 0, 0, CF_T_TEXT, 0);
    if (hddedata == 0) {
        DBG_OUT("DDE communication could not be initiated %u.", DdeGetLastError(inst));
    }
    if (DdeClientTransaction((BYTE*)hddedata, (DWORD)-1, hconv, 0, 0, XTYP_EXECUTE, 10000, 0) == 0) {
        DBG_OUT("DDE transaction failed %u.", DdeGetLastError(inst));
    }
exit:
    DdeFreeDataHandle(hddedata);
    DdeDisconnect(hconv);
    DdeFreeStringHandle(inst, hszTopic);
    DdeFreeStringHandle(inst, hszServer);
    DdeUninitialize(inst);
}


#ifndef USER_DEFAULT_SCREEN_DPI
// the following is only defined if _WIN32_WINNT >= 0x0600 and we use 0x0500
#define USER_DEFAULT_SCREEN_DPI 96
#endif

HFONT Win32_Font_GetSimple(HDC hdc, TCHAR *fontName, int fontSize)
{
    LOGFONT lf = { 0 };

    lf.lfWidth = 0;
    lf.lfHeight = -MulDiv(fontSize, GetDeviceCaps(hdc, LOGPIXELSY), USER_DEFAULT_SCREEN_DPI);
    lf.lfItalic = FALSE;
    lf.lfUnderline = FALSE;
    lf.lfStrikeOut = FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH;    
    Str::CopyTo(lf.lfFaceName, dimof(lf.lfFaceName), fontName);
    lf.lfWeight = FW_DONTCARE;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfEscapement = 0;
    lf.lfOrientation = 0;

    return CreateFontIndirect(&lf);
}

void Win32_Font_Delete(HFONT font)
{
    DeleteObject(font);
}
