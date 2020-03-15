#ifndef _SWELL_WINDOWS_H_
#define _SWELL_WINDOWS_H_

#ifdef _WIN32
#include <windows.h>
#else
#include "swell.h"


#define BIF_RETURNONLYFSDIRS   0x0001
#define BIF_DONTGOBELOWDOMAIN  0x0002
#define BIF_STATUSTEXT         0x0004
#define BIF_RETURNFSANCESTORS  0x0008
#define BIF_EDITBOX            0x0010
#define BIF_VALIDATE           0x0020

#define BIF_BROWSEFORCOMPUTER  0x1000
#define BIF_BROWSEFORPRINTER   0x2000
#define BIF_BROWSEINCLUDEFILES 0x4000

#define BFFM_INITIALIZED        1
#define BFFM_SELCHANGED         2
typedef int (CALLBACK* BFFCALLBACK)(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData);

typedef struct __ITEMIDLIST ITEMIDLIST;
typedef const ITEMIDLIST *LPCITEMIDLIST;

typedef struct _browseinfoA {
    HWND        hwndOwner;
    LPCITEMIDLIST pidlRoot;
    LPSTR        pszDisplayName;
    LPCSTR       lpszTitle;
    UINT         ulFlags;
    BFFCALLBACK  lpfn;
    LPARAM      lParam;
    int          iImage;
} BROWSEINFO, *PBROWSEINFO, *LPBROWSEINFO;

#ifdef __cplusplus
class IMalloc
{
  public:
    void Release() { delete this; }
    void Free(void *p) { free(p); }    
};
#define SHGetMalloc(x) { *(x) = new IMalloc; }
#define SHGetPathFromIDList(src,dest) { if (src) {lstrcpyn(dest,(char *)src,MAX_PATH); } else *dest=0; }
#endif

#define BFFM_SETSELECTION      (WM_USER + 102)

#define Shell_NotifyIcon(a,b) (0)
#define PostQuitMessage(x) { /* todo: mac quit message*/ }
#define SetClassLong(a,b,c) (0)
#define LoadIcon(a,b) ((HICON)0)
#define IsIconic(x) (0)
#define IsWindowEnabled(x) (1)
#define TrackPopupMenuEx(a,b,c,d,e,f) TrackPopupMenu(a,b,c,d,0,e,NULL)
#endif

typedef UINT (CALLBACK *LPOFNHOOKPROC) (HWND, UINT, WPARAM, LPARAM);

typedef struct tagOFNA {
   DWORD        lStructSize;
   HWND         hwndOwner;
   HINSTANCE    hInstance;
   LPCSTR       lpstrFilter;
   LPSTR        lpstrCustomFilter;
   DWORD        nMaxCustFilter;
   DWORD        nFilterIndex;
   LPSTR        lpstrFile;
   DWORD        nMaxFile;
   LPSTR        lpstrFileTitle;
   DWORD        nMaxFileTitle;
   LPCSTR       lpstrInitialDir;
   LPCSTR       lpstrTitle;
   DWORD        Flags;
   WORD         nFileOffset;
   WORD         nFileExtension;
   LPCSTR       lpstrDefExt;
   LPARAM       lCustData;
   LPOFNHOOKPROC lpfnHook;
   LPCSTR       lpTemplateName;
} OPENFILENAME, *LPOPENFILENAME;


#define OFN_READONLY                 0x00000001
#define OFN_OVERWRITEPROMPT          0x00000002
#define OFN_HIDEREADONLY             0x00000004
#define OFN_NOCHANGEDIR              0x00000008
#define OFN_SHOWHELP                 0x00000010
#define OFN_ENABLEHOOK               0x00000020
#define OFN_ENABLETEMPLATE           0x00000040
#define OFN_ENABLETEMPLATEHANDLE     0x00000080
#define OFN_NOVALIDATE               0x00000100
#define OFN_ALLOWMULTISELECT         0x00000200
#define OFN_EXTENSIONDIFFERENT       0x00000400
#define OFN_PATHMUSTEXIST            0x00000800
#define OFN_FILEMUSTEXIST            0x00001000
#define OFN_CREATEPROMPT             0x00002000
#define OFN_SHAREAWARE               0x00004000
#define OFN_NOREADONLYRETURN         0x00008000
#define OFN_NOTESTFILECREATE         0x00010000
#define OFN_NONETWORKBUTTON          0x00020000
#define OFN_NOLONGNAMES              0x00040000
#define OFN_EXPLORER                 0x00080000
#define OFN_NODEREFERENCELINKS       0x00100000
#define OFN_LONGNAMES                0x00200000
#define OFN_ENABLEINCLUDENOTIFY      0x00400000
#define OFN_ENABLESIZING             0x00800000


#define MB_ICONHAND                 0x00000010L
#define MB_ICONQUESTION             0x00000020L
#define MB_ICONEXCLAMATION          0x00000030L
#define MB_ICONASTERISK             0x00000040L

typedef struct _SHELLEXECUTEINFOA
{
        DWORD cbSize;
        ULONG fMask;
        HWND hwnd;
        LPCSTR   lpVerb;
        LPCSTR   lpFile;
        LPCSTR   lpParameters;
        LPCSTR   lpDirectory;
        int nShow;
        HINSTANCE hInstApp;
} SHELLEXECUTEINFO,  *LPSHELLEXECUTEINFO;


#define InitCommonControls() { }
#define CoInitialize(x) { }

/*
#define IsDialogMessage(wnd,a) (0)
#define TranslateMessage(a) { }
#define DispatchMessage(a) { }
*/

#define INVALID_HANDLE_VALUE ((HANDLE)((unsigned int)-1))

#define RemoveDirectory(x) (!rmdir(x))

#define CharNext(x) ((x)+1)
#define CharPrev(base,x) ( (x)>(base)?(x)-1:(base)) 
#define isspace(x) ((x) == ' ' || (x) == '\t' || (x) == '\r' || (x) == '\n')

#define lstrcpyA strcpy
#define lstrcpynA lstrcpyn

#endif
