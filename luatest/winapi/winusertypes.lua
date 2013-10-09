--ffi/winusertypes: winuser types we keep separate for use by external libs that don't want to include winapi.
--note: SIZE has w and h in addition to cx and cy and these are the ones used.
--note: RECT has x1, y1, x2, y2 in addition to left, right, top, bottom and these are the ones used.
local ffi = require'ffi'
require'winapi.wintypes'

ffi.cdef[[
typedef UINT_PTR            WPARAM;
typedef LONG_PTR            LPARAM;
typedef LONG_PTR            LRESULT;
struct HWND__ { int unused; }; typedef struct HWND__ *HWND;
struct HHOOK__ { int unused; }; typedef struct HHOOK__ *HHOOK;
typedef WORD                ATOM;
typedef HANDLE              *SPHANDLE;
typedef HANDLE              *LPHANDLE;
typedef HANDLE              HGLOBAL;
typedef HANDLE              HLOCAL;
typedef HANDLE              GLOBALHANDLE;
typedef HANDLE              LOCALHANDLE;
typedef int ( *FARPROC)();
typedef int ( *NEARPROC)();
typedef int (*PROC)();
typedef void *HGDIOBJ;

typedef LONG (__stdcall* WNDPROC)(HWND, UINT, WPARAM, LONG);

struct HACCEL__ { int unused; }; typedef struct HACCEL__ *HACCEL;
struct HBITMAP__ { int unused; }; typedef struct HBITMAP__ *HBITMAP;
struct HBRUSH__ { int unused; }; typedef struct HBRUSH__ *HBRUSH;
struct HCOLORSPACE__ { int unused; }; typedef struct HCOLORSPACE__ *HCOLORSPACE;
struct HDC__ { int unused; }; typedef struct HDC__ *HDC;
struct HGLRC__ { int unused; }; typedef struct HGLRC__ *HGLRC;
struct HDESK__ { int unused; }; typedef struct HDESK__ *HDESK;
struct HENHMETAFILE__ { int unused; }; typedef struct HENHMETAFILE__ *HENHMETAFILE;
struct HFONT__ { int unused; }; typedef struct HFONT__ *HFONT;
struct HICON__ { int unused; }; typedef struct HICON__ *HICON;
struct HMENU__ { int unused; }; typedef struct HMENU__ *HMENU;
struct HMETAFILE__ { int unused; }; typedef struct HMETAFILE__ *HMETAFILE;
struct HPALETTE__ { int unused; }; typedef struct HPALETTE__ *HPALETTE;
struct HPEN__ { int unused; }; typedef struct HPEN__ *HPEN;
struct HRGN__ { int unused; }; typedef struct HRGN__ *HRGN;
struct HRSRC__ { int unused; }; typedef struct HRSRC__ *HRSRC;
struct HSPRITE__ { int unused; }; typedef struct HSPRITE__ *HSPRITE;
struct HSTR__ { int unused; }; typedef struct HSTR__ *HSTR;
struct HTASK__ { int unused; }; typedef struct HTASK__ *HTASK;
struct HWINSTA__ { int unused; }; typedef struct HWINSTA__ *HWINSTA;
struct HKL__ { int unused; }; typedef struct HKL__ *HKL;
struct HWINEVENTHOOK__ { int unused; }; typedef struct HWINEVENTHOOK__ *HWINEVENTHOOK;
struct HMONITOR__ { int unused; }; typedef struct HMONITOR__ *HMONITOR;
struct HUMPD__ { int unused; }; typedef struct HUMPD__ *HUMPD;
typedef HICON HCURSOR;
typedef DWORD   COLORREF;
typedef DWORD   *LPCOLORREF;

struct HINSTANCE__ { int unused; }; typedef struct HINSTANCE__ *HINSTANCE;
typedef HINSTANCE HMODULE;

typedef struct tagRECT
{
union{
  struct{
      LONG    left;
      LONG    top;
      LONG    right;
      LONG    bottom;
  };
  struct{
      LONG    x1;
      LONG    y1;
      LONG    x2;
      LONG    y2;
  };
  struct{
      LONG    x;
      LONG    y;
  };
};
} RECT, *PRECT,  *NPRECT,  *LPRECT;
typedef const RECT * LPCRECT;
typedef RECT RECTL, *PRECTL, *LPRECTL;
typedef const RECTL * LPCRECTL;

typedef struct tagPOINT
{
    LONG  x;
    LONG  y;
} POINT, *PPOINT, *NPPOINT, *LPPOINT;

typedef struct _POINTL
{
    LONG  x;
    LONG  y;
} POINTL, *PPOINTL;

typedef struct tagSIZE
{
union {
    struct {
        LONG w;
        LONG h;
    };
    struct {
        LONG cx;
        LONG cy;
    };
};
} SIZE, *PSIZE, *LPSIZE;

typedef SIZE               SIZEL;
typedef SIZE               *PSIZEL, *LPSIZEL;

typedef struct tagPOINTS
{
    SHORT   x;
    SHORT   y;
} POINTS, *PPOINTS, *LPPOINTS;
]]
