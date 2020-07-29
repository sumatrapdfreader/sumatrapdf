#ifndef _WDL_WIN32_UTF8_H_
#define _WDL_WIN32_UTF8_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && !defined(WDL_NO_SUPPORT_UTF8)

#ifndef WDL_WIN32_UTF8_IMPL 
#define WDL_WIN32_UTF8_IMPL
#define WDL_WIN32_UTF8_IMPL_NOTSTATIC
#endif

#include <windows.h>
#include <sys/stat.h>
#include <stdio.h>

WDL_WIN32_UTF8_IMPL BOOL SetWindowTextUTF8(HWND hwnd, LPCTSTR str);
WDL_WIN32_UTF8_IMPL BOOL SetDlgItemTextUTF8(HWND hDlg, int nIDDlgItem, LPCTSTR lpString);
WDL_WIN32_UTF8_IMPL int GetWindowTextUTF8(HWND hWnd, LPTSTR lpString, int nMaxCount);
WDL_WIN32_UTF8_IMPL UINT GetDlgItemTextUTF8(HWND hDlg, int nIDDlgItem, LPTSTR lpString, int nMaxCount);
WDL_WIN32_UTF8_IMPL int MessageBoxUTF8(HWND hwnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT fl);

WDL_WIN32_UTF8_IMPL BOOL CreateDirectoryUTF8(LPCTSTR path, LPSECURITY_ATTRIBUTES attr);
WDL_WIN32_UTF8_IMPL BOOL DeleteFileUTF8(LPCTSTR path);
WDL_WIN32_UTF8_IMPL BOOL MoveFileUTF8(LPCTSTR existfn, LPCTSTR newfn);
WDL_WIN32_UTF8_IMPL BOOL CopyFileUTF8(LPCTSTR existfn, LPCTSTR newfn, BOOL fie);
WDL_WIN32_UTF8_IMPL DWORD GetCurrentDirectoryUTF8(DWORD nBufferLength, LPTSTR lpBuffer);
WDL_WIN32_UTF8_IMPL BOOL SetCurrentDirectoryUTF8(LPCTSTR path);
WDL_WIN32_UTF8_IMPL BOOL RemoveDirectoryUTF8(LPCTSTR path);
WDL_WIN32_UTF8_IMPL HINSTANCE LoadLibraryUTF8(LPCTSTR path);

WDL_WIN32_UTF8_IMPL HANDLE CreateFileUTF8(LPCTSTR lpFileName,DWORD dwDesiredAccess,DWORD dwShareMode,LPSECURITY_ATTRIBUTES lpSecurityAttributes,DWORD dwCreationDisposition,DWORD dwFlagsAndAttributes,HANDLE hTemplateFile);

WDL_WIN32_UTF8_IMPL UINT DragQueryFileUTF8(HDROP hDrop, UINT idx, char *buf, UINT bufsz);

WDL_WIN32_UTF8_IMPL int DrawTextUTF8(HDC hdc, LPCTSTR str, int nc, LPRECT lpRect, UINT format);

WDL_WIN32_UTF8_IMPL BOOL GetOpenFileNameUTF8(LPOPENFILENAME lpofn);
WDL_WIN32_UTF8_IMPL BOOL GetSaveFileNameUTF8(LPOPENFILENAME lpofn);

#if _MSC_VER > 1700 && defined(_WIN64)
WDL_WIN32_UTF8_IMPL BOOL SHGetPathFromIDListUTF8(const struct _ITEMIDLIST __unaligned *pidl, LPSTR pszPath, int pszPathLen);
#else
WDL_WIN32_UTF8_IMPL BOOL SHGetPathFromIDListUTF8(const struct _ITEMIDLIST *pidl, LPSTR pszPath, int pszPathLen);
#endif

WDL_WIN32_UTF8_IMPL BOOL SHGetSpecialFolderPathUTF8(HWND hwndOwner, LPTSTR lpszPath, int pszPathLen, int csidl, BOOL create);

WDL_WIN32_UTF8_IMPL struct _ITEMIDLIST *SHBrowseForFolderUTF8(struct _browseinfoA *browseInfoA);
WDL_WIN32_UTF8_IMPL int WDL_UTF8_SendBFFM_SETSEL(HWND hwnd, const char *str); // sends BFFM_SETSELECTIONA or BFFM_SETSELECTIONW

WDL_WIN32_UTF8_IMPL HINSTANCE ShellExecuteUTF8(HWND hwnd, LPCTSTR lpOp, LPCTSTR lpFile, LPCTSTR lpParm, LPCTSTR lpDir, INT nShowCmd);

WDL_WIN32_UTF8_IMPL BOOL GetUserNameUTF8(LPTSTR lpString, LPDWORD nMaxCount);
WDL_WIN32_UTF8_IMPL BOOL GetComputerNameUTF8(LPTSTR lpString, LPDWORD nMaxCount);

WDL_WIN32_UTF8_IMPL BOOL InsertMenuUTF8(HMENU hMenu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCTSTR str);
WDL_WIN32_UTF8_IMPL BOOL InsertMenuItemUTF8( HMENU hMenu,UINT uItem, BOOL fByPosition, LPMENUITEMINFO lpmii);
WDL_WIN32_UTF8_IMPL BOOL SetMenuItemInfoUTF8(HMENU hMenu, UINT uItem, BOOL fByPosition,LPMENUITEMINFO lpmii);
WDL_WIN32_UTF8_IMPL BOOL GetMenuItemInfoUTF8(HMENU hMenu, UINT uItem,BOOL fByPosition,LPMENUITEMINFO lpmii);
   
WDL_WIN32_UTF8_IMPL int statUTF8(const char *filename, struct stat *buffer);
WDL_WIN32_UTF8_IMPL FILE *fopenUTF8(const char *filename, const char *mode);

WDL_WIN32_UTF8_IMPL int GetKeyNameTextUTF8(LONG lParam, LPTSTR lpString, int nMaxCount);


WDL_WIN32_UTF8_IMPL WCHAR *WDL_UTF8ToWC(const char *buf, BOOL doublenull, int minsize, DWORD *sizeout); 

WDL_WIN32_UTF8_IMPL BOOL WDL_HasUTF8(const char *_str);

WDL_WIN32_UTF8_IMPL void WDL_UTF8_HookComboBox(HWND h);
WDL_WIN32_UTF8_IMPL void WDL_UTF8_HookListView(HWND h);
WDL_WIN32_UTF8_IMPL void WDL_UTF8_HookListBox(HWND h);
WDL_WIN32_UTF8_IMPL void WDL_UTF8_HookTreeView(HWND h);
WDL_WIN32_UTF8_IMPL void WDL_UTF8_HookTabCtrl(HWND h);

WDL_WIN32_UTF8_IMPL LPSTR GetCommandParametersUTF8();
WDL_WIN32_UTF8_IMPL void WDL_UTF8_ListViewConvertDispInfoToW(void *di); //NMLVDISPINFO 

WDL_WIN32_UTF8_IMPL UINT GetPrivateProfileIntUTF8(LPCTSTR appStr, LPCTSTR keyStr, INT def, LPCTSTR fnStr);
WDL_WIN32_UTF8_IMPL DWORD GetPrivateProfileStringUTF8(LPCTSTR appStr, LPCTSTR keyStr, LPCTSTR defStr, LPTSTR retStr, DWORD nSize, LPCTSTR fnStr);
WDL_WIN32_UTF8_IMPL BOOL WritePrivateProfileStringUTF8(LPCTSTR appStr, LPCTSTR keyStr, LPCTSTR str, LPCTSTR fnStr);
WDL_WIN32_UTF8_IMPL BOOL GetPrivateProfileStructUTF8(LPCTSTR appStr, LPCTSTR keyStr, LPVOID pStruct, UINT uSize, LPCTSTR fnStr);
WDL_WIN32_UTF8_IMPL BOOL WritePrivateProfileStructUTF8(LPCTSTR appStr, LPCTSTR keyStr, LPVOID pStruct, UINT uSize, LPCTSTR fnStr);

WDL_WIN32_UTF8_IMPL DWORD GetModuleFileNameUTF8(HMODULE hModule, LPTSTR fnStr, DWORD nSize);

WDL_WIN32_UTF8_IMPL BOOL CreateProcessUTF8( LPCTSTR lpApplicationName, LPTSTR lpCommandLine,
  LPSECURITY_ATTRIBUTES lpProcessAttributes,
  LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles,
  DWORD dwCreationFlags,
  LPVOID lpEnvironment,
  LPCTSTR lpCurrentDirectory,
  LPSTARTUPINFO lpStartupInfo,
  LPPROCESS_INFORMATION lpProcessInformation);

#ifdef SetWindowText
#undef SetWindowText
#endif
#define SetWindowText SetWindowTextUTF8

#ifdef SetDlgItemText
#undef SetDlgItemText
#endif
#define SetDlgItemText SetDlgItemTextUTF8


#ifdef GetWindowText
#undef GetWindowText
#endif
#define GetWindowText GetWindowTextUTF8

#ifdef GetDlgItemText
#undef GetDlgItemText
#endif
#define GetDlgItemText GetDlgItemTextUTF8

#ifdef MessageBox
#undef MessageBox
#endif
#define MessageBox MessageBoxUTF8

#ifdef DragQueryFile
#undef DragQueryFile
#endif
#define DragQueryFile DragQueryFileUTF8

#ifdef GetOpenFileName
#undef GetOpenFileName
#endif
#define GetOpenFileName GetOpenFileNameUTF8

#ifdef GetSaveFileName
#undef GetSaveFileName
#endif
#define GetSaveFileName GetSaveFileNameUTF8

#ifdef ShellExecute
#undef ShellExecute
#endif
#define ShellExecute ShellExecuteUTF8

#ifdef GetUserName
#undef GetUserName
#endif
#define GetUserName GetUserNameUTF8

#ifdef GetComputerName
#undef GetComputerName
#endif
#define GetComputerName GetComputerNameUTF8

#ifdef CreateDirectory
#undef CreateDirectory
#endif
#define CreateDirectory CreateDirectoryUTF8

#ifdef DeleteFile
#undef DeleteFile
#endif
#define DeleteFile DeleteFileUTF8

#ifdef MoveFile
#undef MoveFile
#endif
#define MoveFile MoveFileUTF8

#ifdef CopyFile
#undef CopyFile
#endif
#define CopyFile CopyFileUTF8

#ifdef GetCurrentDirectory
#undef GetCurrentDirectory
#endif
#define GetCurrentDirectory GetCurrentDirectoryUTF8

#ifdef SetCurrentDirectory
#undef SetCurrentDirectory
#endif
#define SetCurrentDirectory SetCurrentDirectoryUTF8


#ifdef RemoveDirectory
#undef RemoveDirectory
#endif
#define RemoveDirectory RemoveDirectoryUTF8


#ifdef CreateFile
#undef CreateFile
#endif
#define CreateFile CreateFileUTF8


#ifdef InsertMenu
#undef InsertMenu
#endif
#define InsertMenu InsertMenuUTF8

#ifdef InsertMenuItem
#undef InsertMenuItem
#endif
#define InsertMenuItem InsertMenuItemUTF8

#ifdef SetMenuItemInfo
#undef SetMenuItemInfo
#endif
#define SetMenuItemInfo SetMenuItemInfoUTF8

#ifdef GetMenuItemInfo
#undef GetMenuItemInfo
#endif
#define GetMenuItemInfo GetMenuItemInfoUTF8

#ifdef LoadLibrary
#undef LoadLibrary
#endif
#define LoadLibrary LoadLibraryUTF8

#ifdef GetPrivateProfileInt
#undef GetPrivateProfileInt
#endif
#define GetPrivateProfileInt GetPrivateProfileIntUTF8

#ifdef GetPrivateProfileString
#undef GetPrivateProfileString
#endif
#define GetPrivateProfileString GetPrivateProfileStringUTF8

#ifdef WritePrivateProfileString
#undef WritePrivateProfileString
#endif
#define WritePrivateProfileString WritePrivateProfileStringUTF8

#ifdef WritePrivateProfileStruct
#undef WritePrivateProfileStruct
#endif
#define WritePrivateProfileStruct WritePrivateProfileStructUTF8

#ifdef GetPrivateProfileStruct
#undef GetPrivateProfileStruct
#endif
#define GetPrivateProfileStruct GetPrivateProfileStructUTF8


#ifdef GetModuleFileName
#undef GetModuleFileName
#endif
#define GetModuleFileName GetModuleFileNameUTF8

#ifdef CreateProcess
#undef CreateProcess
#endif
#define CreateProcess CreateProcessUTF8

#ifdef fopen
#undef fopen
#endif
#define fopen fopenUTF8

#ifdef stat
#undef stat
#endif
#define stat(fn,s) statUTF8(fn,s)
typedef char wdl_utf8_chk_stat_types_assert_failed[sizeof(struct stat) == sizeof(struct _stat) ? 1 : -1];

#else

#if defined(WDL_CHECK_FOR_NON_UTF8_FOPEN) && defined(fopen)
  #undef fopen
#endif

// compat defines for when UTF disabled
#define DrawTextUTF8 DrawText
#define statUTF8 stat
#define fopenUTF8 fopen
#define WDL_UTF8_HookComboBox(x)
#define WDL_UTF8_HookListView(x)
#define WDL_UTF8_HookListBox(x)
#define WDL_UTF8_HookTreeView(x)
#define WDL_UTF8_HookTabCtrl(x)
#define WDL_UTF8_ListViewConvertDispInfoToW(x)

#endif

#ifdef __cplusplus
};
#endif

#endif
