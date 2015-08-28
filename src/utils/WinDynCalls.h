/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
License: Simplified BSD (see COPYING.BSD) */

/*
A centrialized location for all APIs that we need to load dynamically.
The convention is: for a function like SetProcessDEPPolicy(), we define
a  function pointer DynSetProcessDEPPolicy() (with a signature matching SetProcessDEPPolicy()).

You can test if a function is available with if (DynSetProcessDEPPolicy).

The intent is to standardize how we do it.
*/

// as an exception, we include system headers needed for the calls that we dynamically load
#include <dwmapi.h>
#include <vssym32.h>
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>
#include <OleAcc.h>

// kernel32.dll
#ifndef PROCESS_DEP_ENABLE
#define PROCESS_DEP_ENABLE 0x1
#define PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION 0x2
#endif
typedef BOOL(WINAPI *Sig_SetProcessDEPPolicy)(DWORD dwFlags);
typedef BOOL(WINAPI *Sig_IsWow64Process)(HANDLE, PBOOL);
typedef BOOL(WINAPI *Sig_SetDllDirectoryW)(LPCWSTR);
typedef void(WINAPI *Sig_RtlCaptureContext)(PCONTEXT);
typedef HANDLE(WINAPI *Sig_CreateFileTransactedW)(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile, HANDLE hTransaction, PUSHORT pusMiniVersion, PVOID pExtendedParameter);
typedef BOOL(WINAPI * Sig_DeleteFileTransactedW)(LPCWSTR lpFileName, HANDLE hTransaction);


#define KERNEL32_API_LIST(V) \
    V(SetProcessDEPPolicy) \
    V(IsWow64Process) \
    V(SetDllDirectoryW) \
    V(RtlCaptureContext) \
    V(CreateFileTransactedW) \
    V(DeleteFileTransactedW)



// ntdll.dll
#define PROCESS_EXECUTE_FLAGS 0x22
#define MEM_EXECUTE_OPTION_DISABLE 0x1
#define MEM_EXECUTE_OPTION_ENABLE 0x2
#define MEM_EXECUTE_OPTION_PERMANENT 0x8
#define MEM_EXECUTE_OPTION_DISABLE_ATL 0x4

/* enable "NX" execution prevention for XP, 2003
* cf. http://www.uninformed.org/?v=2&a=4 */
typedef HRESULT(WINAPI *Sig_NtSetInformationProcess)(HANDLE ProcessHandle, UINT ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength);

#define NTDLL_API_LIST(V) \
    V(NtSetInformationProcess)



// uxtheme.dll
// for win SDKs that don't have this
#ifndef WM_THEMECHANGED
#define WM_THEMECHANGED 0x031A
#endif
#ifndef WM_DWMCOMPOSITIONCHANGED
#define WM_DWMCOMPOSITIONCHANGED 0x031E
#endif
#ifndef SM_CXPADDEDBORDER
#define SM_CXPADDEDBORDER 92
#endif
#ifndef WM_DWMCOLORIZATIONCOLORCHANGED
#define WM_DWMCOLORIZATIONCOLORCHANGED 0x0320
#endif

typedef BOOL(WINAPI *Sig_IsAppThemed)();
typedef HTHEME(WINAPI *Sig_OpenThemeData)(HWND hwnd, LPCWSTR pszClassList);
typedef HRESULT(WINAPI *Sig_CloseThemeData)(HTHEME hTheme);
typedef HRESULT(WINAPI *Sig_DrawThemeBackground)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, LPCRECT pClipRect);
typedef BOOL(WINAPI *Sig_IsThemeActive)(void);
typedef BOOL(WINAPI *Sig_IsThemeBackgroundPartiallyTransparent)(HTHEME hTheme, int iPartId, int iStateId);
typedef HRESULT(WINAPI *Sig_GetThemeColor)(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF *pColor);

#define UXTHEME_API_LIST(V) \
    V(IsAppThemed) \
    V(OpenThemeData) \
    V(CloseThemeData) \
    V(DrawThemeBackground) \
    V(IsThemeActive) \
    V(IsThemeBackgroundPartiallyTransparent) \
    V(GetThemeColor)


/// dwmapi.dll
typedef HRESULT(WINAPI *Sig_DwmIsCompositionEnabled)(BOOL *pfEnabled);
typedef HRESULT(WINAPI *Sig_DwmExtendFrameIntoClientArea)(HWND hwnd, const MARGINS *pMarInset);
typedef BOOL(WINAPI *Sig_DwmDefWindowProc)(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *plResult);
typedef HRESULT(WINAPI *Sig_DwmGetWindowAttribute)(HWND hwnd, DWORD dwAttribute, void *pvAttribute, DWORD cbAttribute);

#define DWMAPI_API_LIST(V) \
    V(DwmIsCompositionEnabled) \
    V(DwmExtendFrameIntoClientArea) \
    V(DwmDefWindowProc) \
    V(DwmGetWindowAttribute)


// normaliz.dll
typedef int(WINAPI *Sig_NormalizeString)(int, LPCWSTR, int, LPWSTR, int);

#define NORMALIZ_API_LIST(V) \
    V(NormalizeString)


// ktmw32.dll
typedef HANDLE(WINAPI * Sig_CreateTransaction)(LPSECURITY_ATTRIBUTES lpTransactionAttributes, LPGUID UOW, DWORD CreateOptions, DWORD IsolationLevel, DWORD IsolationFlags, DWORD Timeout, LPWSTR Description);
typedef BOOL(WINAPI * Sig_CommitTransaction)(HANDLE TransactionHandle);
typedef BOOL(WINAPI * Sig_RollbackTransaction)(HANDLE TransactionHandle);

#define KTMW32_API_LIST(V) \
    V(CreateTransaction) \
    V(CommitTransaction) \
    V(RollbackTransaction)


// uiautomationcore.dll, not available under Win2000
typedef LRESULT(WINAPI *Sig_UiaReturnRawElementProvider)(HWND hwnd, WPARAM wParam, LPARAM lParam, IRawElementProviderSimple *el);
typedef HRESULT(WINAPI *Sig_UiaHostProviderFromHwnd)(HWND hwnd, IRawElementProviderSimple ** pProvider);
typedef HRESULT(WINAPI *Sig_UiaRaiseAutomationEvent)(IRawElementProviderSimple * pProvider, EVENTID id);
typedef HRESULT(WINAPI *Sig_UiaRaiseStructureChangedEvent)(IRawElementProviderSimple * pProvider, StructureChangeType structureChangeType, int * pRuntimeId, int cRuntimeIdLen);
typedef HRESULT(WINAPI *Sig_UiaGetReservedNotSupportedValue)(IUnknown **punkNotSupportedValue);

#define UIA_API_LIST(V) \
    V(UiaReturnRawElementProvider) \
    V(UiaHostProviderFromHwnd) \
    V(UiaRaiseAutomationEvent) \
    V(UiaRaiseStructureChangedEvent) \
    V(UiaGetReservedNotSupportedValue)



// user32.dll

#ifndef _QWORD_DEFINED
#define _QWORD_DEFINED
typedef unsigned __int64 QWORD, *LPQWORD;
#endif

#ifndef MAKEQWORD
#define MAKEQWORD(a, b)     ((QWORD)(((QWORD)((DWORD)(a))) << 32 | ((DWORD)(b))))
#define LODWORD(l)          ((DWORD)((l) & 0xFFFFFFFF))
#define HIDWORD(l)          ((DWORD)(((QWORD)(l) >> 32) & 0xFFFFFFFF))
#endif

// Define the Gesture structures here because they
// are not available in all versions of Windows
// These defines can be found in WinUser.h
#ifndef GF_BEGIN  // needs WINVER >= 0x0601

DECLARE_HANDLE(HGESTUREINFO);

/*
* Gesture flags - GESTUREINFO.dwFlags
*/
#define GF_BEGIN                        0x00000001
#define GF_INERTIA                      0x00000002
#define GF_END                          0x00000004

/*
* Gesture configuration structure
*   - Used in SetGestureConfig and GetGestureConfig
*   - Note that any setting not included in either GESTURECONFIG.dwWant or
*     GESTURECONFIG.dwBlock will use the parent window's preferences or
*     system defaults.
*/
typedef struct tagGESTURECONFIG {
    DWORD dwID;                     // gesture ID
    DWORD dwWant;                   // settings related to gesture ID that are to be turned on
    DWORD dwBlock;                  // settings related to gesture ID that are to be turned off
} GESTURECONFIG, *PGESTURECONFIG;

/*
* Gesture information structure
*   - Pass the HGESTUREINFO received in the WM_GESTURE message lParam into the
*     GetGestureInfo function to retrieve this information.
*   - If cbExtraArgs is non-zero, pass the HGESTUREINFO received in the WM_GESTURE
*     message lParam into the GetGestureExtraArgs function to retrieve extended
*     argument information.
*/
typedef struct tagGESTUREINFO {
    UINT cbSize;                    // size, in bytes, of this structure (including variable length Args field)
    DWORD dwFlags;                  // see GF_* flags
    DWORD dwID;                     // gesture ID, see GID_* defines
    HWND hwndTarget;                // handle to window targeted by this gesture
    POINTS ptsLocation;             // current location of this gesture
    DWORD dwInstanceID;             // internally used
    DWORD dwSequenceID;             // internally used
    ULONGLONG ullArguments;         // arguments for gestures whose arguments fit in 8 BYTES
    UINT cbExtraArgs;               // size, in bytes, of extra arguments, if any, that accompany this gesture
} GESTUREINFO, *PGESTUREINFO;
typedef GESTUREINFO const * PCGESTUREINFO;

/*
* Gesture argument helpers
*   - Angle should be a double in the range of -2pi to +2pi
*   - Argument should be an unsigned 16-bit value
*/
#define GID_ROTATE_ANGLE_TO_ARGUMENT(_arg_)     ((USHORT)((((_arg_) + 2.0 * M_PI) / (4.0 * M_PI)) * 65535.0))
#define GID_ROTATE_ANGLE_FROM_ARGUMENT(_arg_)   ((((double)(_arg_) / 65535.0) * 4.0 * M_PI) - 2.0 * M_PI)

/*
* Gesture configuration flags
*/
#define GC_ALLGESTURES                              0x00000001
#define GC_ZOOM                                     0x00000001
#define GC_PAN                                      0x00000001
#define GC_PAN_WITH_SINGLE_FINGER_VERTICALLY        0x00000002
#define GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY      0x00000004
#define GC_PAN_WITH_GUTTER                          0x00000008
#define GC_PAN_WITH_INERTIA                         0x00000010
#define GC_ROTATE                                   0x00000001
#define GC_TWOFINGERTAP                             0x00000001
#define GC_PRESSANDTAP                              0x00000001

/*
* Gesture IDs
*/
#define GID_BEGIN                       1
#define GID_END                         2
#define GID_ZOOM                        3
#define GID_PAN                         4
#define GID_ROTATE                      5
#define GID_TWOFINGERTAP                6
#define GID_PRESSANDTAP                 7

// Window events
#define WM_GESTURE                         0x0119

#endif // HGESTUREINFO

typedef BOOL(WINAPI * Sig_GetGestureInfo)(HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo);
typedef BOOL(WINAPI * Sig_CloseGestureInfoHandle)(HGESTUREINFO hGestureInfo);
typedef BOOL(WINAPI * Sig_SetGestureConfig)(HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize);

#define USER32_API_LIST(V) \
    V(GetGestureInfo) \
    V(CloseGestureInfoHandle) \
    V(SetGestureConfig)



#define API_DECLARATION(name) \
extern Sig_##name Dyn##name;

KERNEL32_API_LIST(API_DECLARATION)
NTDLL_API_LIST(API_DECLARATION)
UXTHEME_API_LIST(API_DECLARATION)
DWMAPI_API_LIST(API_DECLARATION)
NORMALIZ_API_LIST(API_DECLARATION)
KTMW32_API_LIST(API_DECLARATION)
USER32_API_LIST(API_DECLARATION)
UIA_API_LIST(API_DECLARATION)

#undef API_DECLARATION

HMODULE SafeLoadLibrary(const WCHAR *dllName);
void InitDynCalls();


// convenience wrappers
namespace theme {

bool IsAppThemed();
HTHEME OpenThemeData(HWND hwnd, LPCWSTR pszClassList);
HRESULT CloseThemeData(HTHEME hTheme);
HRESULT DrawThemeBackground(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, LPCRECT pClipRect);
BOOL IsThemeActive();
BOOL IsThemeBackgroundPartiallyTransparent(HTHEME hTheme, int iPartId, int iStateId);
HRESULT GetThemeColor(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF *pColor);

};

namespace dwm {

BOOL IsCompositionEnabled();
HRESULT ExtendFrameIntoClientArea(HWND hwnd, const MARGINS *pMarInset);
BOOL DefWindowProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *plResult);
HRESULT GetWindowAttribute(HWND hwnd, DWORD dwAttribute, void *pvAttribute, DWORD cbAttribute);

};

// Touch Gesture API, only available in Windows 7
namespace touch {

bool SupportsGestures();

BOOL GetGestureInfo(HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo);
BOOL CloseGestureInfoHandle(HGESTUREINFO hGestureInfo);
BOOL SetGestureConfig(HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize);

}

namespace uia {

LRESULT ReturnRawElementProvider(HWND hwnd, WPARAM wParam, LPARAM lParam, IRawElementProviderSimple *);
HRESULT HostProviderFromHwnd(HWND hwnd, IRawElementProviderSimple ** pProvider);
HRESULT RaiseAutomationEvent(IRawElementProviderSimple * pProvider, EVENTID id);
HRESULT RaiseStructureChangedEvent(IRawElementProviderSimple * pProvider, StructureChangeType structureChangeType, int * pRuntimeId, int cRuntimeIdLen);
HRESULT GetReservedNotSupportedValue(IUnknown **punkNotSupportedValue);

};


