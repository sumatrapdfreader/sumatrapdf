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

bool IsAppThemed();
bool WindowsVerVistaOrGreater();

void SeeLastError(DWORD err=0);
bool ReadRegStr(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, const TCHAR *buffer, DWORD bufLen);
bool WriteRegStr(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, const TCHAR *value);
bool WriteRegDWORD(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, DWORD value);

void EnableNx();
void RedirectIOToConsole();
TCHAR *ResolveLnk(TCHAR * path);
IDataObject* GetDataObjectForFile(LPCTSTR pszPath, HWND hwnd=NULL);
DWORD GetFileVersion(TCHAR *path);

inline bool IsKeyPressed(int key)
{
    return GetKeyState(key) & 0x8000 ? true : false;
}
inline bool IsShiftPressed() { return IsKeyPressed(VK_SHIFT); }
inline bool IsAltPressed() { return IsKeyPressed(VK_MENU); }
inline bool IsCtrlPressed() { return IsKeyPressed(VK_CONTROL); }

namespace Win {
namespace Menu {

inline void Check(HMENU m, UINT id, bool check)
{
    CheckMenuItem(m, id, MF_BYCOMMAND | (check ? MF_CHECKED : MF_UNCHECKED));
}

inline void Enable(HMENU m, UINT id, bool enable)
{
    EnableMenuItem(m, id, MF_BYCOMMAND | (enable ? MF_ENABLED : MF_GRAYED));
}

} // namespace Menu
} // namespace Win

#endif
