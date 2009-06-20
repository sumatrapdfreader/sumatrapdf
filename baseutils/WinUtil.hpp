/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. 
   Take all the code you want, we'll just write more.
*/
#ifndef WINUTIL_HPP__
#define WINUTIL_HPP__

class WinLibrary {
public:
    WinLibrary(const char *libName) {
        _hlib = LoadLibraryA(libName);
    }
    ~WinLibrary() { if (_hlib) FreeLibrary(_hlib); }
    FARPROC GetProcAddr(const char *procName) {
        if (!_hlib) return NULL;
        return GetProcAddress(_hlib, procName);
    }
    HMODULE _hlib;
};

class WinProcess {
public:
    static WinProcess* Create(const char *cmd, char *args="");
    
private:
    WinProcess(PROCESS_INFORMATION *);  // we don't want just anyone to make us
    PROCESS_INFORMATION m_processInfo;
};

bool IsAppThemed(void);
bool WindowsVerVistaOrGreater();
bool WindowsVer2000OrGreater();

#endif
