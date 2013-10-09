--proc/keyboard: keyboard input handling and keyboard layouts.
setfenv(1, require'winapi')
require'winapi.winusertypes'

ffi.cdef[[
UINT  GetKBCodePage(void);

SHORT GetKeyState(int nVirtKey);
SHORT GetAsyncKeyState(int vKey);

BOOL  GetKeyboardState(PBYTE lpKeyState);
BOOL  SetKeyboardState(LPBYTE lpKeyState);

int   GetKeyNameTextW(LONG lParam, LPWSTR lpString, int cchSize);
int   GetKeyboardType( int nTypeFlag);

int   ToAscii(UINT uVirtKey, UINT uScanCode, const BYTE *lpKeyState, LPWORD lpChar, UINT uFlags);
int   ToUnicode(UINT wVirtKey, UINT wScanCode, const BYTE *lpKeyState, LPWSTR pwszBuff, int cchBuff, UINT wFlags);

DWORD OemKeyScan(WORD wOemChar);
SHORT VkKeyScanW(WCHAR ch);
void  keybd_event(BYTE bVk, BYTE bScan, DWORD dwFlags, ULONG_PTR dwExtraInfo);

typedef struct tagMOUSEINPUT {
    LONG    dx;
    LONG    dy;
    DWORD   mouseData;
    DWORD   dwFlags;
    DWORD   time;
    ULONG_PTR dwExtraInfo;
} MOUSEINPUT, *PMOUSEINPUT, * LPMOUSEINPUT;

typedef struct tagKEYBDINPUT {
    WORD    wVk;
    WORD    wScan;
    DWORD   dwFlags;
    DWORD   time;
    ULONG_PTR dwExtraInfo;
} KEYBDINPUT, *PKEYBDINPUT, * LPKEYBDINPUT;

typedef struct tagHARDWAREINPUT {
    DWORD   uMsg;
    WORD    wParamL;
    WORD    wParamH;
} HARDWAREINPUT, *PHARDWAREINPUT, * LPHARDWAREINPUT;

typedef struct tagINPUT {
    DWORD   type;
    union
    {
        MOUSEINPUT      mi;
        KEYBDINPUT      ki;
        HARDWAREINPUT   hi;
    };
} INPUT, *PINPUT, * LPINPUT;

UINT SendInput(UINT cInputs, LPINPUT pInputs, int cbSize);

typedef struct tagLASTINPUTINFO {
    UINT cbSize;
    DWORD dwTime;
} LASTINPUTINFO, * PLASTINPUTINFO;

BOOL GetLastInputInfo(PLASTINPUTINFO plii);

UINT MapVirtualKeyW(UINT uCode, UINT uMapType);


// keyboard layouts

HKL  LoadKeyboardLayoutW(LPCWSTR pwszKLID, UINT Flags);
HKL  ActivateKeyboardLayout(HKL hkl, UINT Flags);
BOOL UnloadKeyboardLayout(HKL hkl);
BOOL GetKeyboardLayoutNameW(LPWSTR pwszKLID);
int  GetKeyboardLayoutList(int nBuff, HKL  *lpList);
HKL  GetKeyboardLayout(DWORD idThread);

int   ToAsciiEx(UINT uVirtKey, UINT uScanCode, const BYTE *lpKeyState, LPWORD lpChar, UINT uFlags, HKL dwhkl);
int   ToUnicodeEx(UINT wVirtKey, UINT wScanCode, const BYTE *lpKeyState,
						LPWSTR pwszBuff, int cchBuff, UINT wFlags, HKL dwhkl);
SHORT VkKeyScanExW(WCHAR ch, HKL dwhkl);
UINT  MapVirtualKeyExW(UINT uCode, UINT uMapType, HKL dwhkl);

]]
