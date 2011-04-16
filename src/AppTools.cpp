/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "translations_txt.h"
#include "translations.h"
#include "AppTools.h"
#include "CmdLineParser.h"
#include "Version.h"
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
    const TCHAR *next = Str::Parse(*txt, _T("%u%?."), &val);
    *txt = next ? (TCHAR *)next : *txt + Str::Len(*txt);
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
TCHAR *GetExePath()
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
    ScopedMem<TCHAR> exePath(GetExePath());
    if (NULL == exePath.Get())
        return true;

    // if we can't get a path, assume we're not running from "Program Files"
    ScopedMem<TCHAR> installedPath(NULL);
    installedPath.Set(ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\") APP_NAME_STR, _T("Install_Dir")));
    if (NULL == installedPath.Get())
        installedPath.Set(ReadRegStr(HKEY_CURRENT_USER, _T("Software\\") APP_NAME_STR, _T("Install_Dir")));
    if (NULL != installedPath.Get()) {
        if (!Str::EndsWithI(installedPath.Get(), _T(".exe"))) {
            installedPath.Set(Path::Join(installedPath.Get(), Path::GetBaseName(exePath)));
        }
        if (Path::IsSame(installedPath, exePath))
            return false;
    }

    TCHAR programFilesDir[MAX_PATH] = {0};
    BOOL ok = SHGetSpecialFolderPath(NULL, programFilesDir, CSIDL_PROGRAM_FILES, FALSE);
    if (FALSE == ok)
        return true;

    // check if one of the exePath's parent directories is "Program Files"
    // (or a junction to it)
    TCHAR *baseName;
    while ((baseName = (TCHAR*)Path::GetBaseName(exePath)) > exePath) {
        baseName[-1] = '\0';
        if (Path::IsSame(programFilesDir, exePath))
            return false;
    }

    return true;
}

TCHAR *AppGenDataDir()
{
    /* Use %APPDATA% */
    TCHAR dir[MAX_PATH] = {0};
    SHGetSpecialFolderPath(NULL, dir, CSIDL_APPDATA, TRUE);
    TCHAR *path = Path::Join(dir, APP_NAME_STR);
    if (!path)
        return NULL;
    if (!Dir::Create(path)) {
        free(path);
        return NULL;
    }
    return path;
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
        TCHAR *exePath = GetExePath();
        if (exePath) {
            assert(exePath[0]);
            path = Path::GetDir(exePath);
            free(exePath);
        }
    } else {
        path = AppGenDataDir();
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
Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pdf\UserChoice\Progid
  should be SumatraPDF as well (also only needed for HKEY_CURRENT_USER);
  this key is used for remembering a user's choice with Explorer's Open With dialog
  and can't be written to - so we delete it instead!

HKEY_CLASSES_ROOT\.pdf\OpenWithList
  list of all apps that can be used to open PDF files. We don't touch that.

HKEY_CLASSES_ROOT\.pdf default comes from either HKCU\Software\Classes\.pdf or
HKLM\Software\Classes\.pdf (HKCU has priority over HKLM)

Note: When making changes below, please also adjust
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
    if (prevHandler && !Str::Eq(prevHandler, APP_NAME_STR)) {
        WriteRegStr(hkey, REG_CLASSES_APP, _T("previous.pdf"), prevHandler);
    }

    WriteRegStr(hkey, REG_CLASSES_APP, NULL, _TR("PDF Document"));
    TCHAR *icon_path = Str::Join(exePath, _T(",1"));
    WriteRegStr(hkey, REG_CLASSES_APP _T("\\DefaultIcon"), NULL, icon_path);
    free(icon_path);

    WriteRegStr(hkey, REG_CLASSES_APP _T("\\shell"), NULL, _T("open"));

    ScopedMem<TCHAR> cmdPath(Str::Format(_T("\"%s\" \"%%1\""), exePath)); // "${exePath}" "%1"
    bool ok = WriteRegStr(hkey, REG_CLASSES_APP _T("\\shell\\open\\command"), NULL, cmdPath);

    // also register for printing
    ScopedMem<TCHAR> printPath(Str::Format(_T("\"%s\" -print-to-default \"%%1\""), exePath)); // "${exePath}" -print-to-default "%1"
    WriteRegStr(hkey, REG_CLASSES_APP _T("\\shell\\print\\command"), NULL, printPath);

    // also register for printing to specific printer
    ScopedMem<TCHAR> printToPath(Str::Format(_T("\"%s\" -print-to \"%%2\" \"%%1\""), exePath)); // "${exePath}" -print-to "%2" "%1"
    WriteRegStr(hkey, REG_CLASSES_APP _T("\\shell\\printto\\command"), NULL, printToPath);

    // Only change the association if we're confident, that we've registered ourselves well enough
    if (ok) {
        WriteRegStr(hkey, REG_CLASSES_PDF, NULL, APP_NAME_STR);
        if (hkey == HKEY_CURRENT_USER) {
            WriteRegStr(hkey, REG_EXPLORER_PDF_EXT, _T("Progid"), APP_NAME_STR);
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
    if (tmp && !Str::Eq(tmp, APP_NAME_STR))
        return false;

    // this one doesn't have to exist but if it does, it must be APP_NAME_STR
    tmp.Set(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT _T("\\UserChoice"), _T("Progid")));
    if (tmp && !Str::Eq(tmp, APP_NAME_STR))
        return false;

    // HKEY_CLASSES_ROOT\.pdf default key must exist and be equal to APP_NAME_STR
    tmp.Set(ReadRegStr(HKEY_CLASSES_ROOT, _T(".pdf"), NULL));
    if (!Str::Eq(tmp, APP_NAME_STR))
        return false;

    // HKEY_CLASSES_ROOT\SumatraPDF\shell\open default key must be: open
    tmp.Set(ReadRegStr(HKEY_CLASSES_ROOT, _T("SumatraPDF\\shell"), NULL));
    if (!Str::EqI(tmp, _T("open")))
        return false;

    // HKEY_CLASSES_ROOT\SumatraPDF\shell\open\command default key must be: "${exe_path}" "%1"
    tmp.Set(ReadRegStr(HKEY_CLASSES_ROOT, _T("SumatraPDF\\shell\\open\\command"), NULL));
    if (!tmp)
        return false;

    CmdLineParser argList(tmp);
    ScopedMem<TCHAR> exePath(GetExePath());
    if (!exePath || !argList.Find(_T("%1")))
        return false;

    return Path::IsSame(exePath, argList[0]);
}

TCHAR *GetAcrobatPath()
{
    // Try Adobe Acrobat as a fall-back, if the Reader isn't installed
    ScopedMem<TCHAR> path(ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\AcroRd32.exe"), NULL));
    if (!path)
        path.Set(ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Acrobat.exe"), NULL));
    if (path && File::Exists(path))
        return path.StealData();
    return NULL;
}

TCHAR *GetFoxitPath()
{
    ScopedMem<TCHAR> path(ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Foxit Reader"), _T("DisplayIcon")));
    if (path && File::Exists(path))
        return path.StealData();
    return NULL;
}

TCHAR *GetPDFXChangePath()
{
    ScopedMem<TCHAR> path(ReadRegStr(HKEY_LOCAL_MACHINE, _T("Software\\Tracker Software\\PDFViewer"), _T("InstallPath")));
    if (!path)
        path.Set(ReadRegStr(HKEY_CURRENT_USER,  _T("Software\\Tracker Software\\PDFViewer"), _T("InstallPath")));
    if (!path)
        return false;
    ScopedMem<TCHAR> exePath(Path::Join(path, _T("PDFXCview.exe")));
    if (File::Exists(exePath))
        return exePath.StealData();
    return NULL;
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
    ScopedMem<TCHAR> path(NULL);

    for (int i = 0; i < dimof(editor_rules); i++)
    {
        path.Set(ReadRegStr(editor_rules[i].RegRoot, editor_rules[i].RegKey, editor_rules[i].RegValue));
        if (!path)
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
    hddedata = DdeCreateDataHandle(inst, (BYTE*)command, (DWORD)(Str::Len(command) + 1) * sizeof(TCHAR), 0, 0, CF_T_TEXT, 0);
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
