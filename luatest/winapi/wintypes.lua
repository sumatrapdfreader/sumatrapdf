--ffi/wintypes: basic windows types and macros from multiple headers.
--we don't define time_t as it's 64 in windows but 32bit in mingw: use explicit types.
local ffi = require'ffi'
require'ctypes'

ffi.cdef[[
typedef size_t          rsize_t;
typedef unsigned short  wctype_t;
typedef int             errno_t;
typedef long            __time32_t;
typedef __int64         __time64_t;

typedef unsigned long   ULONG;
typedef ULONG           *PULONG;
typedef unsigned short  USHORT;
typedef USHORT          *PUSHORT;
typedef unsigned char   UCHAR;
typedef UCHAR           *PUCHAR;
typedef char            *PSZ;
typedef unsigned long   DWORD;
typedef int             BOOL;
typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef float           FLOAT;
typedef FLOAT           *PFLOAT;
typedef BOOL            *PBOOL;
typedef BOOL            *LPBOOL;
typedef BYTE            *PBYTE;
typedef BYTE            *LPBYTE;
typedef int             *PINT;
typedef int             *LPINT;
typedef WORD            *PWORD;
typedef WORD            *LPWORD;
typedef long            *LPLONG;
typedef DWORD           *PDWORD;
typedef DWORD           *LPDWORD;
typedef void            *LPVOID;
typedef const void      *LPCVOID;
typedef int             INT;
typedef unsigned int    UINT;
typedef unsigned int    *PUINT;
typedef unsigned long   POINTER_64_INT;
typedef signed char     INT8, *PINT8;
typedef signed short    INT16, *PINT16;
typedef signed int      INT32, *PINT32;
typedef signed __int64  INT64, *PINT64;
typedef unsigned char   UINT8, *PUINT8;
typedef unsigned short  UINT16, *PUINT16;
typedef unsigned int    UINT32, *PUINT32;
typedef unsigned __int64 UINT64, *PUINT64;
typedef signed int      LONG32, *PLONG32;
typedef unsigned int    ULONG32, *PULONG32;
typedef unsigned int    DWORD32, *PDWORD32;
typedef int             INT_PTR, *PINT_PTR;
typedef unsigned int    UINT_PTR, *PUINT_PTR;
typedef long            LONG_PTR, *PLONG_PTR;
typedef unsigned long   ULONG_PTR, *PULONG_PTR;
typedef unsigned short  UHALF_PTR, *PUHALF_PTR;
typedef short           HALF_PTR, *PHALF_PTR;
typedef long            SHANDLE_PTR;
typedef unsigned long   HANDLE_PTR;
typedef ULONG_PTR       SIZE_T, *PSIZE_T;
typedef LONG_PTR        SSIZE_T, *PSSIZE_T;
typedef ULONG_PTR       DWORD_PTR, *PDWORD_PTR;
typedef __int64         LONG64, *PLONG64;
typedef unsigned __int64 ULONG64, *PULONG64;
typedef unsigned __int64 DWORD64, *PDWORD64;
typedef void            *PVOID;
typedef void * __ptr64  PVOID64;
typedef char            CHAR;
typedef short           SHORT;
typedef long            LONG;
typedef int             INT;
typedef wchar_t         WCHAR;
typedef WCHAR           *PWCHAR, *LPWCH, *PWCH;
typedef const WCHAR     *LPCWCH, *PCWCH;
typedef WCHAR           *NWPSTR, *LPWSTR, *PWSTR;
typedef PWSTR           *PZPWSTR;
typedef const PWSTR     *PCZPWSTR;
typedef WCHAR           *LPUWSTR, *PUWSTR;
typedef const WCHAR     *LPCWSTR, *PCWSTR;
typedef PCWSTR          *PZPCWSTR;
typedef const WCHAR     *LPCUWSTR, *PCUWSTR;
typedef CHAR            *PCHAR, *LPCH, *PCH;
typedef const CHAR      *LPCCH, *PCCH;
typedef CHAR            *NPSTR, *LPSTR, *PSTR;
typedef PSTR            *PZPSTR;
typedef const PSTR      *PCZPSTR;
typedef const CHAR      *LPCSTR, *PCSTR;
typedef PCSTR           *PZPCSTR;
typedef char            TCHAR, *PTCHAR;
typedef unsigned char   TBYTE, *PTBYTE;
typedef LPCH            LPTCH, PTCH;
typedef LPSTR           PTSTR, LPTSTR, PUTSTR, LPUTSTR;
typedef LPCSTR          PCTSTR, LPCTSTR, PCUTSTR, LPCUTSTR;
typedef SHORT           *PSHORT;
typedef LONG            *PLONG;
typedef void            *HANDLE;
typedef HANDLE          *PHANDLE;
typedef BYTE            FCHAR;
typedef WORD            FSHORT;
typedef DWORD           FLONG;
typedef long            HRESULT;
typedef char            CCHAR;
typedef DWORD           LCID;
typedef PDWORD          PLCID;
typedef WORD            LANGID;
typedef __int64         LONGLONG;
typedef unsigned __int64 ULONGLONG;
typedef LONGLONG        *PLONGLONG;
typedef ULONGLONG       *PULONGLONG;
typedef LONGLONG        USN;

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG HighPart;
    };
    struct {
        DWORD LowPart;
        LONG HighPart;
    } u;
    LONGLONG QuadPart;
} LARGE_INTEGER;
typedef LARGE_INTEGER *PLARGE_INTEGER;
typedef union _ULARGE_INTEGER {
    struct {
        DWORD LowPart;
        DWORD HighPart;
    };
    struct {
        DWORD LowPart;
        DWORD HighPart;
    } u;
    ULONGLONG QuadPart;
} ULARGE_INTEGER;
typedef ULARGE_INTEGER *PULARGE_INTEGER;
typedef struct _LUID {
    DWORD LowPart;
    LONG HighPart;
} LUID, *PLUID;
typedef ULONGLONG  DWORDLONG;
typedef DWORDLONG *PDWORDLONG;
typedef BYTE  BOOLEAN;
typedef BOOLEAN *PBOOLEAN;

typedef int HFILE;

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;
]]
