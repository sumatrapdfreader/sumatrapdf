/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. 
   Take all the code you want, we'll just write more.
*/
#ifndef WINUTIL_HPP__
#define WINUTIL_HPP__

class WinLibrary {
public:
    WinLibrary(const TCHAR *libName) {
        _hlib = _LoadSystemLibrary(libName);
    }
    ~WinLibrary() { FreeLibrary(_hlib); }
    FARPROC GetProcAddr(const char *procName) {
        if (!_hlib) return NULL;
        return GetProcAddress(_hlib, procName);
    }
private:
    HMODULE _hlib;
    HMODULE _LoadSystemLibrary(const TCHAR *libName);
};

class WinProcess {
public:
    static WinProcess* Create(const char *cmd, char *args="");
    
private:
    WinProcess(PROCESS_INFORMATION *);  // we don't want just anyone to make us
    PROCESS_INFORMATION m_processInfo;
};

bool IsAppThemed();
bool WindowsVerVistaOrGreater();
bool WindowsVer2000OrGreater();

void SeeLastError(DWORD err=0);
bool ReadRegStr(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, const TCHAR *buffer, DWORD bufLen);
bool WriteRegStr(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, const TCHAR *value);
bool WriteRegDWORD(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, DWORD value);

void DynSetProcessDPIAware();
void EnableNx();
void RedirectIOToConsole();
TCHAR *ResolveLnk(TCHAR * path);

#endif
