/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. 
   Take all the code you want, we'll just write more.
*/
#ifndef WINUTIL_HPP__
#define WINUTIL_HPP__

#include <windows.h>
#include <CommCtrl.h>

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

class ComScope {
public:
    ComScope() { CoInitialize(NULL); }
    ~ComScope() { CoUninitialize(); }
};

class MillisecondTimer {
    LARGE_INTEGER   start;
    LARGE_INTEGER   end;
public:
    void Start() { QueryPerformanceCounter(&start); }
    void Stop() { QueryPerformanceCounter(&end); }

    double GetTimeInMs()
    {
        LARGE_INTEGER   freq;
        QueryPerformanceFrequency(&freq);
        double timeInSecs = (double)(end.QuadPart-start.QuadPart)/(double)freq.QuadPart;
        return timeInSecs * 1000.0;
    }
};

static inline void InitAllCommonControls()
{
    INITCOMMONCONTROLSEX cex = {0};
    cex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    cex.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_USEREX_CLASSES | ICC_COOL_CLASSES ;
    InitCommonControlsEx(&cex);
}

static inline void FillWndClassEx(WNDCLASSEX &wcex, HINSTANCE hInstance) 
{
    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = NULL;
    wcex.lpszMenuName   = NULL;
    wcex.hIconSm        = 0;
}

static inline int RectDx(RECT *r)
{
    return r->right - r->left;
}

static inline int RectDy(RECT *r)
{
    return r->bottom - r->top;
}

static inline void SetFont(HWND hwnd, HFONT font)
{
    SendMessage(hwnd, WM_SETFONT, WPARAM(font), TRUE);
}

static inline void SetCheckboxState(HWND hwnd, int checked)
{
    SendMessage(hwnd, BM_SETCHECK, checked, 0L);
}

static inline BOOL GetCheckboxState(HWND hwnd)
{
    return (BOOL)SendMessage(hwnd, BM_GETCHECK, 0, 0L);
}

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
