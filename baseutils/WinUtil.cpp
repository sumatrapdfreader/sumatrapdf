/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. 
   Take all the code you want, we'll just write more.
*/

#include "base_util.h"
#include "WinUtil.hpp"
#include "tstr_util.h"
#include <shlwapi.h>
#include <shlobj.h>
#include <io.h>
#include <fcntl.h>

#define DONT_INHERIT_HANDLES FALSE

// Return true if application is themed. Wrapper around IsAppThemed() in uxtheme.dll
// that is compatible with earlier windows versions.
bool IsAppThemed() {
    WinLibrary lib(_T("uxtheme.dll"));
    FARPROC pIsAppThemed = lib.GetProcAddr("IsAppThemed");
    if (!pIsAppThemed) 
        return false;
    if (pIsAppThemed())
        return true;
    return false;
}

// Loads a DLL explicitly from the system's library collection
HMODULE WinLibrary::_LoadSystemLibrary(const TCHAR *libName) {
    TCHAR dllPath[MAX_PATH];
    GetSystemDirectory(dllPath, dimof(dllPath));
    PathAppend(dllPath, libName);
    return LoadLibrary(dllPath);
}

// Given name of the command to exececute 'cmd', and its arguments 'args'
// return WinProcess object that makes it easier to handle the process
// Returns NULL if failed to create the process. Caller can use GetLastError()
// for detailed error information.
WinProcess * WinProcess::Create(const TCHAR *cmd, TCHAR *args)
{
    UINT                res;
    HANDLE              stdOut = INVALID_HANDLE_VALUE;
    HANDLE              stdErr = INVALID_HANDLE_VALUE;
    STARTUPINFO         siStartupInfo;
    PROCESS_INFORMATION piProcessInfo;
    SECURITY_ATTRIBUTES sa;

    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = 0;
    sa.bInheritHandle = 1;

    memzero(&siStartupInfo, sizeof(siStartupInfo));
    memzero(&piProcessInfo, sizeof(piProcessInfo));
    siStartupInfo.cb = sizeof(siStartupInfo);

    TCHAR stdoutTempName[MAX_PATH] = {0};
    TCHAR stderrTempName[MAX_PATH] = {0};
    TCHAR *stdoutTempNameCopy = NULL;
    TCHAR *stderrTempNameCopy = NULL;

    TCHAR buf[MAX_PATH] = {0};
    DWORD len = GetTempPath(sizeof(buf), buf);
    assert(len < sizeof(buf));
    // create temporary files for capturing stdout and stderr or the command
    res = GetTempFileName(buf, _T("stdout"), 0, stdoutTempName);
    if (0 == res)
        goto Error;

    res = GetTempFileName(buf, _T("stderr"), 0, stderrTempName);
    if (0 == res)
        goto Error;

    stdoutTempNameCopy = tstr_dup(stdoutTempName);
    stderrTempNameCopy = tstr_dup(stderrTempName);

    stdOut = CreateFile(stdoutTempNameCopy,
        GENERIC_READ|GENERIC_WRITE,
        FILE_SHARE_WRITE|FILE_SHARE_READ,
        &sa, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, 0);
    if (INVALID_HANDLE_VALUE == stdOut)
        goto Error;

    stdErr = CreateFile(stderrTempNameCopy,
        GENERIC_READ|GENERIC_WRITE,
        FILE_SHARE_WRITE|FILE_SHARE_READ,
        &sa, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, 0);
    if (INVALID_HANDLE_VALUE == stdErr)
        goto Error;

    siStartupInfo.hStdOutput = stdOut;
    siStartupInfo.hStdError = stdErr;

    BOOL ok = CreateProcess(cmd, args, NULL, NULL, DONT_INHERIT_HANDLES,
        CREATE_DEFAULT_ERROR_MODE, NULL /*env*/, NULL /*curr dir*/,
        &siStartupInfo, &piProcessInfo);

    if (!ok)
        goto Error;

    // TODO: pass stdoutTempNameCopy and stderrTempNameCopy so upon
    // WinProcess destruction the files can be deleted and their memory freed
    WinProcess *wp = new WinProcess(&piProcessInfo);
    return wp;

Error:
    CloseHandle(stdOut);
    CloseHandle(stdErr);

    if (stdoutTempName[0]) {
        // TODO: delete stdoutTempName
    }
    if (stderrTempName[0]) {
        // TODO: delete stderrTempName
    }
    free(stdoutTempNameCopy);
    free(stderrTempNameCopy);
    return NULL;
}

WinProcess::WinProcess(PROCESS_INFORMATION *pi)
{
    memcpy(&m_processInfo, pi, sizeof(PROCESS_INFORMATION));
}

static int WindowsVerMajor()
{
    DWORD version = GetVersion();
    return (int)(version & 0xFF);
}

static int WindowsVerMinor()
{
    DWORD version = GetVersion();
    return (int)((version & 0xFF00) >> 8);    
}

bool WindowsVer2000OrGreater()
{
    if (WindowsVerMajor() >= 5)
        return true;
    return false;
}

bool WindowsVerVistaOrGreater()
{
    if (WindowsVerMajor() >= 6)
        return true;
    return false;
}

void SeeLastError(DWORD err) {
    char *msgBuf = NULL;
    if (err == 0)
        err = GetLastError();
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&msgBuf, 0, NULL);
    if (!msgBuf) return;
    DBG_OUT("SeeLastError(): %s\n", msgBuf);
    LocalFree(msgBuf);
}

bool ReadRegStr(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, const TCHAR *buffer, DWORD bufLen)
{
    LONG res = SHGetValue(keySub, keyName, valName, NULL, (VOID *)buffer, &bufLen);
    if (ERROR_SUCCESS != res && ERROR_FILE_NOT_FOUND != res)
        SeeLastError(res);
    return ERROR_SUCCESS == res;
}

bool WriteRegStr(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, const TCHAR *value)
{
    LONG res = SHSetValue(keySub, keyName, valName, REG_SZ, (const VOID *)value, (tstr_len(value) + 1) * sizeof(TCHAR));
    if (ERROR_SUCCESS != res)
        SeeLastError(res);
    return ERROR_SUCCESS == res;
}

bool WriteRegDWORD(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, DWORD value)
{
    LONG res = SHSetValue(keySub, keyName, valName, REG_DWORD, (const VOID *)&value, sizeof(DWORD));
    if (ERROR_SUCCESS != res)
        SeeLastError(res);
    return ERROR_SUCCESS == res;
}

#define PROCESS_EXECUTE_FLAGS 0x22

/*
 * enable "NX" execution prevention for XP, 2003
 * cf. http://www.uninformed.org/?v=2&a=4
 */
typedef HRESULT (WINAPI *_NtSetInformationProcess)(
   HANDLE  ProcessHandle,
   UINT    ProcessInformationClass,
   PVOID   ProcessInformation,
   ULONG   ProcessInformationLength
   );

void EnableNx(void)
{
    WinLibrary lib(_T("ntdll.dll"));
    _NtSetInformationProcess ntsip;
    DWORD dep_mode = 13; /* ENABLE | DISABLE_ATL | PERMANENT */

    ntsip = (_NtSetInformationProcess)lib.GetProcAddr("NtSetInformationProcess");
    if (ntsip)
        ntsip(GetCurrentProcess(), PROCESS_EXECUTE_FLAGS, &dep_mode, sizeof(dep_mode));
}

// Code from http://www.halcyon.com/~ast/dload/guicon.htm
void RedirectIOToConsole(void)
{
    CONSOLE_SCREEN_BUFFER_INFO coninfo;
    int hConHandle;

    // allocate a console for this app
    AllocConsole();

    // set the screen buffer to be big enough to let us scroll text
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
    coninfo.dwSize.Y = 500;
    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

    // redirect unbuffered STDOUT to the console
    hConHandle = _open_osfhandle((long)GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);
    *stdout = *(FILE *)_fdopen(hConHandle, "w");
    setvbuf(stdout, NULL, _IONBF, 0);

    // redirect unbuffered STDERR to the console
    hConHandle = _open_osfhandle((long)GetStdHandle(STD_ERROR_HANDLE), _O_TEXT);
    *stderr = *(FILE *)_fdopen(hConHandle, "w");
    setvbuf(stderr, NULL, _IONBF, 0);

    // redirect unbuffered STDIN to the console
    hConHandle = _open_osfhandle((long)GetStdHandle(STD_INPUT_HANDLE), _O_TEXT);
    *stdin = *(FILE *)_fdopen(hConHandle, "r");
    setvbuf(stdin, NULL, _IONBF, 0);
}

TCHAR *ResolveLnk(TCHAR * path)
{
    IShellLink *lnk = NULL;
    IPersistFile *file = NULL;
    TCHAR *resolvedPath = NULL;

    LPCOLESTR olePath = tstr_to_wstr(path);
    if (!olePath)
        return NULL;

    HRESULT hRes = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                    IID_IShellLink, (LPVOID *)&lnk);
    if (FAILED(hRes))
        goto Exit;

    hRes = lnk->QueryInterface(IID_IPersistFile, (LPVOID *)&file);
    if (FAILED(hRes))
        goto Exit;

    hRes = file->Load(olePath, STGM_READ);
    if (FAILED(hRes))
        goto Exit;

    hRes = lnk->Resolve(NULL, SLR_UPDATE);
    if (FAILED(hRes))
        goto Exit;

    TCHAR newPath[MAX_PATH];
    hRes = lnk->GetPath(newPath, MAX_PATH, NULL, 0);
    if (FAILED(hRes))
        goto Exit;

    resolvedPath = tstr_dup(newPath);

Exit:
    if (file)
        file->Release();
    if (lnk)
        lnk->Release();
    free((void *)olePath);

    return resolvedPath;
}

/* adapted from http://blogs.msdn.com/oldnewthing/archive/2004/09/20/231739.aspx */
IDataObject* GetDataObjectForFile(LPCTSTR pszPath, HWND hwnd)
{
    IDataObject* pDataObject = NULL;
    IShellFolder *pDesktopFolder;
    HRESULT hr = SHGetDesktopFolder(&pDesktopFolder);
    if (FAILED(hr))
        return NULL;

    LPWSTR lpWPath = tstr_to_wstr(pszPath);
    LPITEMIDLIST pidl;
    hr = pDesktopFolder->ParseDisplayName(NULL, NULL, lpWPath, NULL, &pidl, NULL);
    if (SUCCEEDED(hr)) {
        IShellFolder *pShellFolder;
        LPCITEMIDLIST pidlChild;
        hr = SHBindToParent(pidl, IID_IShellFolder, (void**)&pShellFolder, &pidlChild);
        if (SUCCEEDED(hr)) {
            pShellFolder->GetUIObjectOf(hwnd, 1, &pidlChild, IID_IDataObject, NULL, (void **)&pDataObject);
            pShellFolder->Release();
        }
        CoTaskMemFree(pidl);
    }
    pDesktopFolder->Release();

    free(lpWPath);
    return pDataObject;
}

// The result value contains major and minor version in the high resp. the low WORD
DWORD GetFileVersion(TCHAR *path)
{
    DWORD fileVersion = 0;
    DWORD handle;
    DWORD size = GetFileVersionInfoSize(path, &handle);
    LPVOID versionInfo = malloc(size);

    if (GetFileVersionInfo(path, handle, size, versionInfo)) {
        VS_FIXEDFILEINFO *fileInfo;
        UINT len;
        if (VerQueryValue(versionInfo, _T("\\"), (LPVOID *)&fileInfo, &len))
            fileVersion = fileInfo->dwFileVersionMS;
    }

    free(versionInfo);
    return fileVersion;
}
