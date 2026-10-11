/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#if OS_WIN
// Minimal redeclaration of the shell's IVirtualDesktopManager (Windows 10 1607+),
// to tell whether a window is on the user's current virtual desktop. We use a
// distinct name (and don't include <shobjidl.h>) to avoid clashing with the SDK
// declaration; the vtable layout matches so COM calls dispatch correctly. Lets
// reusing an existing instance avoid yanking focus to another desktop (#5630).
struct ISumatraVirtualDesktopManager : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE IsWindowOnCurrentVirtualDesktop(HWND topLevelWindow, BOOL* onCurrentDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWindowDesktopId(HWND topLevelWindow, GUID* desktopId) = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveWindowToDesktop(HWND topLevelWindow, REFGUID desktopId) = 0;
};
ISumatraVirtualDesktopManager* CreateVirtualDesktopManager();
bool IsWindowOnCurrentDesktop(ISumatraVirtualDesktopManager* vdm, HWND hwnd);
TempStr GetWindowDesktopIdTemp(ISumatraVirtualDesktopManager* vdm, HWND hwnd);
bool MoveWindowToVirtualDesktop(ISumatraVirtualDesktopManager* vdm, HWND hwnd, Str desktopId);
#endif
