#if defined(_WIN32) && !defined(WDL_WIN32_UTF8_NO_UI_IMPL)
#include <shlobj.h>
#include <commctrl.h>
#endif

#include "win32_utf8.h"
#include "wdltypes.h"
#include "wdlutf8.h"

#ifdef _WIN32

#if !defined(WDL_NO_SUPPORT_UTF8)

#ifdef __cplusplus
extern "C" {
#endif


#ifndef WDL_UTF8_MAXFNLEN
#define WDL_UTF8_MAXFNLEN 2048
#endif

#define MBTOWIDE(symbase, src) \
                int symbase##_size; \
                WCHAR symbase##_buf[1024]; \
                WCHAR *symbase = (symbase##_size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,src,-1,NULL,0)) >= 1000 ? (WCHAR *)malloc(symbase##_size * sizeof(WCHAR) + 10) : symbase##_buf; \
                int symbase##_ok = symbase ? (MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,src,-1,symbase,symbase##_size < 1 ? 1024 : symbase##_size)) : 0

#define MBTOWIDE_FREE(symbase) if (symbase != symbase##_buf) free(symbase)


#define WIDETOMB_ALLOC(symbase, length) \
            WCHAR symbase##_buf[1024]; \
            size_t symbase##_size = sizeof(symbase##_buf); \
            WCHAR *symbase = (length) > 1000 ? (WCHAR *)malloc(symbase##_size = (sizeof(WCHAR)*(length) + 10)) : symbase##_buf

#define WIDETOMB_FREE(symbase) if (symbase != symbase##_buf) free(symbase)

BOOL WDL_HasUTF8(const char *_str)
{
  return WDL_DetectUTF8(_str) > 0;
}

#ifdef AND_IS_NOT_WIN9X
#undef AND_IS_NOT_WIN9X
#endif
#ifdef IS_NOT_WIN9X_AND
#undef IS_NOT_WIN9X_AND
#endif

#ifdef WDL_SUPPORT_WIN9X
#define IS_NOT_WIN9X_AND (GetVersion() < 0x80000000) &&
#define AND_IS_NOT_WIN9X && (GetVersion() < 0x80000000)
#else
#define AND_IS_NOT_WIN9X
#define IS_NOT_WIN9X_AND
#endif

static ATOM s_combobox_atom;
#define WDL_UTF8_OLDPROCPROP "WDLUTF8OldProc"

int GetWindowTextUTF8(HWND hWnd, LPTSTR lpString, int nMaxCount)
{
  if (!lpString) return 0;
  if (nMaxCount>0 AND_IS_NOT_WIN9X)
  {
    int alloc_size=nMaxCount;

    // if a hooked combo box, and has an edit child, ask it directly
    if (s_combobox_atom && s_combobox_atom == GetClassWord(hWnd,GCW_ATOM) && GetProp(hWnd,WDL_UTF8_OLDPROCPROP))
    {
      HWND h2=FindWindowEx(hWnd,NULL,"Edit",NULL);
      if (h2) hWnd=h2;
      else
      {
        // get via selection
        int sel = (int) SendMessage(hWnd,CB_GETCURSEL,0,0);
        if (sel>=0)
        {
          int len = (int) SendMessage(hWnd,CB_GETLBTEXTLEN,sel,0);
          char *p = lpString;
          if (len > nMaxCount-1) 
          {
            p = (char*)calloc(len+1,1);
            len = nMaxCount-1;
          }
          lpString[0]=0;
          if (p)
          {
            SendMessage(hWnd,CB_GETLBTEXT,sel,(LPARAM)p);
            if (p!=lpString) 
            {
              memcpy(lpString,p,len);
              lpString[len]=0;
              free(p);
            }
            return len;
          }
        }
      }
    }

    // prevent large values of nMaxCount from allocating memory unless the underlying text is big too
    if (alloc_size > 512)  
    {
      int l=GetWindowTextLengthW(hWnd);
      if (l>=0 && l < 512) alloc_size=1000;
    }

    {
      WIDETOMB_ALLOC(wbuf, alloc_size);
      if (wbuf)
      {
        lpString[0]=0;
        if (GetWindowTextW(hWnd,wbuf,(int) (wbuf_size/sizeof(WCHAR))))
        {
          if (!WideCharToMultiByte(CP_UTF8,0,wbuf,-1,lpString,nMaxCount,NULL,NULL))
            lpString[nMaxCount-1]=0;
        }

        WIDETOMB_FREE(wbuf);

        return (int)strlen(lpString);
      }
    }
  }
  return GetWindowTextA(hWnd,lpString,nMaxCount);
}

UINT GetDlgItemTextUTF8(HWND hDlg, int nIDDlgItem, LPTSTR lpString, int nMaxCount)
{
  HWND h = GetDlgItem(hDlg,nIDDlgItem);
  if (h) return GetWindowTextUTF8(h,lpString,nMaxCount);
  return 0;
}


BOOL SetDlgItemTextUTF8(HWND hDlg, int nIDDlgItem, LPCTSTR lpString)
{
  HWND h = GetDlgItem(hDlg,nIDDlgItem);
  if (!h) return FALSE;

  if (WDL_HasUTF8(lpString) AND_IS_NOT_WIN9X)
  {
    MBTOWIDE(wbuf,lpString);
    if (wbuf_ok)
    {
      BOOL rv = SetWindowTextW(h, wbuf);
      MBTOWIDE_FREE(wbuf);
      return rv;
    }

    MBTOWIDE_FREE(wbuf);
  }

  return SetWindowTextA(h, lpString);

}

static LRESULT WINAPI __forceUnicodeWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (uMsg == WM_SETTEXT && lParam)
  {
    MBTOWIDE(wbuf,(const char *)lParam);
    if (wbuf_ok)
    {
      LRESULT rv = DefWindowProcW(hwnd, uMsg, wParam, (LPARAM)wbuf);
      MBTOWIDE_FREE(wbuf);
      return rv;
    }
    MBTOWIDE_FREE(wbuf);
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

BOOL SetWindowTextUTF8(HWND hwnd, LPCTSTR str)
{
  if (WDL_HasUTF8(str) AND_IS_NOT_WIN9X)
  {
    DWORD pid;
    if (GetWindowThreadProcessId(hwnd,&pid) == GetCurrentThreadId() && 
        pid == GetCurrentProcessId() && 
        !(GetWindowLong(hwnd,GWL_STYLE)&WS_CHILD))
    {
      LPARAM tmp = SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LPARAM)__forceUnicodeWndProc);
      BOOL rv = SetWindowTextA(hwnd, str);
      SetWindowLongPtr(hwnd, GWLP_WNDPROC, tmp);
      return rv;
    }
    else
    {
      MBTOWIDE(wbuf,str);
      if (wbuf_ok)
      {
        BOOL rv = SetWindowTextW(hwnd, wbuf);
        MBTOWIDE_FREE(wbuf);
        return rv;
      }

      MBTOWIDE_FREE(wbuf);
    }
  }

  return SetWindowTextA(hwnd,str);
}

int MessageBoxUTF8(HWND hwnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT fl)
{
  if ((WDL_HasUTF8(lpText)||WDL_HasUTF8(lpCaption)) AND_IS_NOT_WIN9X)
  {
    MBTOWIDE(wbuf,lpText);
    if (wbuf_ok)
    {
      MBTOWIDE(wcap,lpCaption?lpCaption:"");
      if (wcap_ok)
      {
        int ret=MessageBoxW(hwnd,wbuf,lpCaption?wcap:NULL,fl);
        MBTOWIDE_FREE(wcap);
        MBTOWIDE_FREE(wbuf);
        return ret;
      }
      MBTOWIDE_FREE(wbuf);
    }
  }
  return MessageBoxA(hwnd,lpText,lpCaption,fl);
}

UINT DragQueryFileUTF8(HDROP hDrop, UINT idx, char *buf, UINT bufsz)
{
  if (buf && bufsz && idx!=-1 AND_IS_NOT_WIN9X)
  {
    const UINT reqsz = DragQueryFileW(hDrop,idx,NULL,0);
    WIDETOMB_ALLOC(wbuf, reqsz+32);
    if (wbuf)
    {
      UINT rv=DragQueryFileW(hDrop,idx,wbuf,(int)(wbuf_size/sizeof(WCHAR)));
      if (rv)
      {
        if (!WideCharToMultiByte(CP_UTF8,0,wbuf,-1,buf,bufsz,NULL,NULL))
          buf[bufsz-1]=0;
      }
      WIDETOMB_FREE(wbuf);
      return rv;
    }
  }
  return DragQueryFileA(hDrop,idx,buf,bufsz);
}


WCHAR *WDL_UTF8ToWC(const char *buf, BOOL doublenull, int minsize, DWORD *sizeout)
{
  if (doublenull)
  {
    int sz=1;
    const char *p = (const char *)buf;
    WCHAR *pout,*ret;

    while (*p)
    {
      int a=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,p,-1,NULL,0);
      int sp=(int)strlen(p)+1;
      if (a < sp)a=sp; // in case it needs to be ansi mapped
      sz+=a;
      p+=sp;
    }
    if (sz < minsize) sz=minsize;

    pout = (WCHAR *) malloc(sizeof(WCHAR)*(sz+4));
    if (!pout) return NULL;

    ret=pout;
    p = (const char *)buf;
    while (*p)
    {
      int a;
      *pout=0;
      a = MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,p,-1,pout,(int) (sz-(pout-ret)));
      if (!a)
      {
        pout[0]=0;
        a=MultiByteToWideChar(CP_ACP,MB_ERR_INVALID_CHARS,p,-1,pout,(int) (sz-(pout-ret)));
      }
      pout += a;
      p+=strlen(p)+1;
    }
    *pout=0;
    pout[1]=0;
    if (sizeout) *sizeout=sz;
    return ret;
  }
  else
  {
    int srclen = (int)strlen(buf)+1;
    int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,buf,srclen,NULL,0);
    if (size < srclen)size=srclen; // for ansi code page
    if (size<minsize)size=minsize;

    {
      WCHAR *outbuf = (WCHAR *)malloc(sizeof(WCHAR)*(size+128));
      if (!outbuf) return NULL;

      *outbuf=0;
      if (srclen>1)
      {
        int a=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,buf,srclen,outbuf, size);
        if (!a)
        {
          outbuf[0]=0;
          a=MultiByteToWideChar(CP_ACP,MB_ERR_INVALID_CHARS,buf,srclen,outbuf,size);
        }
      }
      if (sizeout) *sizeout = size;
      return outbuf;
    }
  }
}

#ifndef WDL_WIN32_UTF8_NO_UI_IMPL
static BOOL GetOpenSaveFileNameUTF8(LPOPENFILENAME lpofn, BOOL save)
{

  OPENFILENAMEW tmp={sizeof(tmp),lpofn->hwndOwner,lpofn->hInstance,};
  BOOL ret;

  // allocate, convert input
  if (lpofn->lpstrFilter) tmp.lpstrFilter = WDL_UTF8ToWC(lpofn->lpstrFilter,TRUE,0,0);
  tmp.nFilterIndex = lpofn->nFilterIndex ;

  if (lpofn->lpstrFile) tmp.lpstrFile = WDL_UTF8ToWC(lpofn->lpstrFile,FALSE,lpofn->nMaxFile,&tmp.nMaxFile);
  if (lpofn->lpstrFileTitle) tmp.lpstrFileTitle = WDL_UTF8ToWC(lpofn->lpstrFileTitle,FALSE,lpofn->nMaxFileTitle,&tmp.nMaxFileTitle);
  if (lpofn->lpstrInitialDir) tmp.lpstrInitialDir = WDL_UTF8ToWC(lpofn->lpstrInitialDir,0,0,0);
  if (lpofn->lpstrTitle) tmp.lpstrTitle = WDL_UTF8ToWC(lpofn->lpstrTitle,0,0,0);
  if (lpofn->lpstrDefExt) tmp.lpstrDefExt = WDL_UTF8ToWC(lpofn->lpstrDefExt,0,0,0);
  tmp.Flags = lpofn->Flags;
  tmp.lCustData = lpofn->lCustData;
  tmp.lpfnHook = lpofn->lpfnHook;
  tmp.lpTemplateName  = (const WCHAR *)lpofn->lpTemplateName ;
 
  ret=save ? GetSaveFileNameW(&tmp) : GetOpenFileNameW(&tmp);

  // free, convert output
  if (ret && lpofn->lpstrFile && tmp.lpstrFile)
  {
    if ((tmp.Flags & OFN_ALLOWMULTISELECT) && tmp.lpstrFile[wcslen(tmp.lpstrFile)+1])
    {
      char *op = lpofn->lpstrFile;
      WCHAR *ip = tmp.lpstrFile;
      while (*ip)
      {
        const int bcount = WideCharToMultiByte(CP_UTF8,0,ip,-1,NULL,0,NULL,NULL);

        const int maxout=lpofn->nMaxFile - 2 - (int)(op - lpofn->lpstrFile);
        if (maxout < 2+bcount) break;
        op += WideCharToMultiByte(CP_UTF8,0,ip,-1,op,maxout,NULL,NULL);
        ip += wcslen(ip)+1;
      }
      *op=0;
    }
    else
    {
      int len = WideCharToMultiByte(CP_UTF8,0,tmp.lpstrFile,-1,lpofn->lpstrFile,lpofn->nMaxFile-1,NULL,NULL);
      if (len == 0 && GetLastError()==ERROR_INSUFFICIENT_BUFFER) len = lpofn->nMaxFile-2;
      lpofn->lpstrFile[len]=0;
      if (!len) 
      {
        lpofn->lpstrFile[len+1]=0;
        ret=0;
      }
    }
    // convert 
  }

  lpofn->nFileOffset  = tmp.nFileOffset ;
  lpofn->nFileExtension  = tmp.nFileExtension;
  lpofn->lCustData = tmp.lCustData;

  free((WCHAR *)tmp.lpstrFilter);
  free((WCHAR *)tmp.lpstrFile);
  free((WCHAR *)tmp.lpstrFileTitle);
  free((WCHAR *)tmp.lpstrInitialDir);
  free((WCHAR *)tmp.lpstrTitle);
  free((WCHAR *)tmp.lpstrDefExt );

  lpofn->nFilterIndex  = tmp.nFilterIndex ;
  return ret;
}

BOOL GetOpenFileNameUTF8(LPOPENFILENAME lpofn)
{
#ifdef WDL_SUPPORT_WIN9X
  if (GetVersion()&0x80000000) return GetOpenFileNameA(lpofn);
#endif
  return GetOpenSaveFileNameUTF8(lpofn,FALSE);
}

BOOL GetSaveFileNameUTF8(LPOPENFILENAME lpofn)
{
#ifdef WDL_SUPPORT_WIN9X
  if (GetVersion()&0x80000000) return GetSaveFileNameA(lpofn);
#endif
  return GetOpenSaveFileNameUTF8(lpofn,TRUE);
}

BOOL SHGetSpecialFolderPathUTF8(HWND hwndOwner, LPTSTR lpszPath, int pszPathLen, int csidl, BOOL create)
{
  if (lpszPath AND_IS_NOT_WIN9X)
  {
    WCHAR tmp[4096];
    if (SHGetSpecialFolderPathW(hwndOwner,tmp,csidl,create))
    {
      return WideCharToMultiByte(CP_UTF8,0,tmp,-1,lpszPath,pszPathLen,NULL,NULL) > 0;
    }
  }
  return SHGetSpecialFolderPathA(hwndOwner,lpszPath,csidl,create);
}


#if _MSC_VER > 1700 && defined(_WIN64)
BOOL SHGetPathFromIDListUTF8(const struct _ITEMIDLIST __unaligned *pidl, LPSTR pszPath, int pszPathLen)
#else
BOOL SHGetPathFromIDListUTF8(const struct _ITEMIDLIST *pidl, LPSTR pszPath, int pszPathLen)
#endif
{
  if (pszPath AND_IS_NOT_WIN9X)
  {
    const int alloc_sz = pszPathLen < 4096 ? 4096 : pszPathLen;
    WIDETOMB_ALLOC(wfn,alloc_sz);
    if (wfn)
    {
      BOOL b = FALSE;
      if (SHGetPathFromIDListW(pidl,wfn))
      {
        b = WideCharToMultiByte(CP_UTF8,0,wfn,-1,pszPath,pszPathLen,NULL,NULL) > 0;
      }
      WIDETOMB_FREE(wfn);
      return b;
    }
  }
  return SHGetPathFromIDListA(pidl,pszPath);
}

struct _ITEMIDLIST *SHBrowseForFolderUTF8(struct _browseinfoA *bi)
{
  if (bi && (WDL_HasUTF8(bi->pszDisplayName) || WDL_HasUTF8(bi->lpszTitle)) AND_IS_NOT_WIN9X)
  {
    MBTOWIDE(wfn,bi->pszDisplayName);
    if (wfn_ok)
    {
      MBTOWIDE(wtxt,bi->lpszTitle);
      if (wtxt_ok)
      {
        BROWSEINFOW biw ={ bi->hwndOwner,bi->pidlRoot,wfn,wtxt,bi->ulFlags,bi->lpfn,(LPARAM)bi->lParam,bi->iImage };
        LPITEMIDLIST idlist = SHBrowseForFolderW(&biw);
        MBTOWIDE_FREE(wfn);
        MBTOWIDE_FREE(wtxt);
        return (struct _ITEMIDLIST *) idlist;
      }
      MBTOWIDE_FREE(wtxt);
    }
    MBTOWIDE_FREE(wfn);
  }
  return (struct _ITEMIDLIST *)SHBrowseForFolderA(bi);
}

int WDL_UTF8_SendBFFM_SETSEL(HWND hwnd, const char *str)
{
  if (IS_NOT_WIN9X_AND WDL_HasUTF8(str))
  {
    MBTOWIDE(wc, str);
    if (wc_ok)
    {
      int r=(int)SendMessage(hwnd, BFFM_SETSELECTIONW, 1, (LPARAM)wc);
      MBTOWIDE_FREE(wc);
      return r;
    }
    MBTOWIDE_FREE(wc);
  }
  return (int) SendMessage(hwnd, BFFM_SETSELECTIONA, 1, (LPARAM)str);
}

#endif

BOOL SetCurrentDirectoryUTF8(LPCTSTR path)
{
  if (WDL_HasUTF8(path) AND_IS_NOT_WIN9X)
  {
    MBTOWIDE(wbuf,path);
    if (wbuf_ok)
    {
      BOOL rv=SetCurrentDirectoryW(wbuf);
      MBTOWIDE_FREE(wbuf);
      return rv;
    }
    MBTOWIDE_FREE(wbuf);
  }
  return SetCurrentDirectoryA(path);
}

BOOL RemoveDirectoryUTF8(LPCTSTR path)
{
  if (WDL_HasUTF8(path) AND_IS_NOT_WIN9X)
  {
    MBTOWIDE(wbuf,path);
    if (wbuf_ok)
    {
      BOOL rv=RemoveDirectoryW(wbuf);
      MBTOWIDE_FREE(wbuf);
      return rv;
    }
    MBTOWIDE_FREE(wbuf);
  }
  return RemoveDirectoryA(path);
}

HINSTANCE LoadLibraryUTF8(LPCTSTR path)
{
  if (WDL_HasUTF8(path) AND_IS_NOT_WIN9X)
  {
    MBTOWIDE(wbuf,path);
    if (wbuf_ok)
    {
      HINSTANCE rv=LoadLibraryW(wbuf);
      if (rv)
      {
        MBTOWIDE_FREE(wbuf);
        return rv;
      }
    }
    MBTOWIDE_FREE(wbuf);
  }
  return LoadLibraryA(path);
}

BOOL CreateDirectoryUTF8(LPCTSTR path, LPSECURITY_ATTRIBUTES attr)
{
  if (WDL_HasUTF8(path) AND_IS_NOT_WIN9X)
  {
    MBTOWIDE(wbuf,path);
    if (wbuf_ok)
    {
      BOOL rv=CreateDirectoryW(wbuf,attr);
      MBTOWIDE_FREE(wbuf);
      return rv;
    }
    MBTOWIDE_FREE(wbuf);
  }
  return CreateDirectoryA(path,attr);
}

BOOL DeleteFileUTF8(LPCTSTR path)
{
  if (WDL_HasUTF8(path) AND_IS_NOT_WIN9X)
  {
    MBTOWIDE(wbuf,path);
    if (wbuf_ok)
    {
      BOOL rv=DeleteFileW(wbuf);
      MBTOWIDE_FREE(wbuf);
      return rv;
    }
    MBTOWIDE_FREE(wbuf);
  }
  return DeleteFileA(path);
}

BOOL MoveFileUTF8(LPCTSTR existfn, LPCTSTR newfn)
{
  if ((WDL_HasUTF8(existfn)||WDL_HasUTF8(newfn)) AND_IS_NOT_WIN9X)
  {
    MBTOWIDE(wbuf,existfn);
    if (wbuf_ok)
    {
      MBTOWIDE(wbuf2,newfn);
      if (wbuf2_ok)
      {
        int rv=MoveFileW(wbuf,wbuf2);
        MBTOWIDE_FREE(wbuf2);
        MBTOWIDE_FREE(wbuf);
        return rv;
      }
      MBTOWIDE_FREE(wbuf2);
    }
    MBTOWIDE_FREE(wbuf);
  }
  return MoveFileA(existfn,newfn);
}

BOOL CopyFileUTF8(LPCTSTR existfn, LPCTSTR newfn, BOOL fie)
{
  if ((WDL_HasUTF8(existfn)||WDL_HasUTF8(newfn)) AND_IS_NOT_WIN9X)
  {
    MBTOWIDE(wbuf,existfn);
    if (wbuf_ok)
    {
      MBTOWIDE(wbuf2,newfn);
      if (wbuf2_ok)
      {
        int rv=CopyFileW(wbuf,wbuf2,fie);
        MBTOWIDE_FREE(wbuf2);
        MBTOWIDE_FREE(wbuf);
        return rv;
      }
      MBTOWIDE_FREE(wbuf2);
    }
    MBTOWIDE_FREE(wbuf);
  }
  return CopyFileA(existfn,newfn,fie);
}


DWORD GetModuleFileNameUTF8(HMODULE hModule, LPTSTR lpBuffer, DWORD nBufferLength)
{
  if (lpBuffer && nBufferLength > 1 AND_IS_NOT_WIN9X)
  {

    WCHAR wbuf[WDL_UTF8_MAXFNLEN];
    wbuf[0]=0;
    if (GetModuleFileNameW(hModule,wbuf,WDL_UTF8_MAXFNLEN) && wbuf[0])
    {
      int rv=WideCharToMultiByte(CP_UTF8,0,wbuf,-1,lpBuffer,nBufferLength,NULL,NULL);
      if (rv) return rv;
    }
  }
  return GetModuleFileNameA(hModule,lpBuffer,nBufferLength);
}


DWORD GetCurrentDirectoryUTF8(DWORD nBufferLength, LPTSTR lpBuffer)
{
  if (lpBuffer && nBufferLength > 1 AND_IS_NOT_WIN9X)
  {

    WCHAR wbuf[WDL_UTF8_MAXFNLEN];
    wbuf[0]=0;
    if (GetCurrentDirectoryW(WDL_UTF8_MAXFNLEN,wbuf) && wbuf[0])
    {
      int rv=WideCharToMultiByte(CP_UTF8,0,wbuf,-1,lpBuffer,nBufferLength,NULL,NULL);
      if (rv) return rv;
    }
  }
  return GetCurrentDirectoryA(nBufferLength,lpBuffer);
}

HANDLE CreateFileUTF8(LPCTSTR lpFileName,DWORD dwDesiredAccess,DWORD dwShareMode,LPSECURITY_ATTRIBUTES lpSecurityAttributes,DWORD dwCreationDisposition,DWORD dwFlagsAndAttributes,HANDLE hTemplateFile)
{
  if (WDL_HasUTF8(lpFileName) AND_IS_NOT_WIN9X)
  {
    HANDLE h = INVALID_HANDLE_VALUE;
    
    MBTOWIDE(wstr, lpFileName);
    if (wstr_ok) h = CreateFileW(wstr,dwDesiredAccess,dwShareMode,lpSecurityAttributes,dwCreationDisposition,dwFlagsAndAttributes,hTemplateFile);
    MBTOWIDE_FREE(wstr);

    if (h != INVALID_HANDLE_VALUE) return h;
  }
  return CreateFileA(lpFileName,dwDesiredAccess,dwShareMode,lpSecurityAttributes,dwCreationDisposition,dwFlagsAndAttributes,hTemplateFile);
}


int DrawTextUTF8(HDC hdc, LPCTSTR str, int nc, LPRECT lpRect, UINT format)
{
  if (WDL_HasUTF8(str) AND_IS_NOT_WIN9X)
  {
    if (nc<0) nc=(int)strlen(str);

    {
      MBTOWIDE(wstr, str);
      if (wstr_ok)
      {
        int rv=DrawTextW(hdc,wstr,-1,lpRect,format);;
        MBTOWIDE_FREE(wstr);
        return rv;
      }
      MBTOWIDE_FREE(wstr);
    }

  }
  return DrawTextA(hdc,str,nc,lpRect,format);
}


BOOL InsertMenuUTF8(HMENU hMenu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCTSTR str)
{
  if (!(uFlags&(MF_BITMAP|MF_SEPARATOR)) && str && WDL_HasUTF8(str) AND_IS_NOT_WIN9X)
  {
    MBTOWIDE(wbuf,str);
    if (wbuf_ok)
    {
      BOOL rv=InsertMenuW(hMenu,uPosition,uFlags,uIDNewItem,wbuf);
      MBTOWIDE_FREE(wbuf);
      return rv;
    }
  }
  return InsertMenuA(hMenu,uPosition,uFlags,uIDNewItem,str);
}

BOOL InsertMenuItemUTF8( HMENU hMenu,UINT uItem, BOOL fByPosition, LPMENUITEMINFO lpmii)
{
  if (!lpmii) return FALSE;
  if ((lpmii->fMask & MIIM_TYPE) && (lpmii->fType&(MFT_SEPARATOR|MFT_STRING|MFT_BITMAP)) == MFT_STRING && lpmii->dwTypeData && WDL_HasUTF8(lpmii->dwTypeData) AND_IS_NOT_WIN9X)
  {
    BOOL rv;
    MENUITEMINFOW tmp = *(MENUITEMINFOW*)lpmii;
    MBTOWIDE(wbuf,lpmii->dwTypeData);
    if (wbuf_ok)
    {

      tmp.cbSize=sizeof(tmp);
      tmp.dwTypeData = wbuf;
      rv=InsertMenuItemW(hMenu,uItem,fByPosition,&tmp);

      MBTOWIDE_FREE(wbuf);
      return rv;
    }
    MBTOWIDE_FREE(wbuf);
  }
  return InsertMenuItemA(hMenu,uItem,fByPosition,lpmii);
}
BOOL SetMenuItemInfoUTF8( HMENU hMenu,UINT uItem, BOOL fByPosition, LPMENUITEMINFO lpmii)
{
  if (!lpmii) return FALSE;
  if ((lpmii->fMask & MIIM_TYPE) && (lpmii->fType&(MFT_SEPARATOR|MFT_STRING|MFT_BITMAP)) == MFT_STRING && lpmii->dwTypeData && WDL_HasUTF8(lpmii->dwTypeData) AND_IS_NOT_WIN9X)
  {
    BOOL rv;
    MENUITEMINFOW tmp = *(MENUITEMINFOW*)lpmii;
    MBTOWIDE(wbuf,lpmii->dwTypeData);
    if (wbuf_ok)
    {
      tmp.cbSize=sizeof(tmp);
      tmp.dwTypeData = wbuf;
      rv=SetMenuItemInfoW(hMenu,uItem,fByPosition,&tmp);

      MBTOWIDE_FREE(wbuf);
      return rv;
    }
    MBTOWIDE_FREE(wbuf);
  }
  return SetMenuItemInfoA(hMenu,uItem,fByPosition,lpmii);
}

BOOL GetMenuItemInfoUTF8( HMENU hMenu,UINT uItem, BOOL fByPosition, LPMENUITEMINFO lpmii)
{
  if (!lpmii) return FALSE;
  if ((lpmii->fMask & MIIM_TYPE) && lpmii->dwTypeData && lpmii->cch AND_IS_NOT_WIN9X)
  {
    MENUITEMINFOW tmp = *(MENUITEMINFOW*)lpmii;
    WIDETOMB_ALLOC(wbuf,lpmii->cch);

    if (wbuf)
    {
      BOOL rv;
      char *otd=lpmii->dwTypeData;
      int osz=lpmii->cbSize;
      tmp.cbSize=sizeof(tmp);
      tmp.dwTypeData = wbuf;
      tmp.cch = (UINT)(wbuf_size/sizeof(WCHAR));
      rv=GetMenuItemInfoW(hMenu,uItem,fByPosition,&tmp);

      if (rv && (tmp.fType&(MFT_SEPARATOR|MFT_STRING|MFT_BITMAP)) == MFT_STRING)
      {
        if (!WideCharToMultiByte(CP_UTF8,0,wbuf,-1,lpmii->dwTypeData,lpmii->cch,NULL,NULL))
        {
          lpmii->dwTypeData[lpmii->cch-1]=0;
        }

        *lpmii = *(MENUITEMINFO*)&tmp; // copy results
        lpmii->cbSize=osz; // restore old stuff
        lpmii->dwTypeData = otd;
      }
      else rv=0;

      WIDETOMB_FREE(wbuf);
      if (rv)return rv;
    }
  }
  return GetMenuItemInfoA(hMenu,uItem,fByPosition,lpmii);
}


FILE *fopenUTF8(const char *filename, const char *mode)
{
  if (WDL_HasUTF8(filename) AND_IS_NOT_WIN9X)
  {
    MBTOWIDE(wbuf,filename);
    if (wbuf_ok)
    {
      FILE *rv;
      WCHAR tb[32];      
      tb[0]=0;
      MultiByteToWideChar(CP_UTF8,0,mode,-1,tb,32);
      rv=tb[0] ? _wfopen(wbuf,tb) : NULL;
      MBTOWIDE_FREE(wbuf);
      if (rv) return rv;
    }
  }
#ifdef fopen
#undef fopen
#endif
  return fopen(filename,mode);
#define fopen fopenUTF8
}

int statUTF8(const char *filename, struct stat *buffer)
{
  if (WDL_HasUTF8(filename) AND_IS_NOT_WIN9X)
  {
    MBTOWIDE(wbuf,filename);
    if (wbuf_ok)
    {
      int rv=_wstat(wbuf,(struct _stat*)buffer);
      MBTOWIDE_FREE(wbuf);
      if (!rv) return rv;
    }
    else
    {
      MBTOWIDE_FREE(wbuf);
    }
  }
  return stat(filename,buffer);
}

LPSTR GetCommandParametersUTF8()
{
  char *buf;
  int szneeded;
  LPWSTR w=GetCommandLineW();
  if (!w) return NULL;
  szneeded = WideCharToMultiByte(CP_UTF8,0,w,-1,NULL,0,NULL,NULL);
  if (szneeded<1) return NULL;
  buf = (char *)malloc(szneeded+10);
  if (!buf) return NULL;
  if (WideCharToMultiByte(CP_UTF8,0,w,-1,buf,szneeded+9,NULL,NULL)<1) return NULL;
  while (*buf == ' ') buf++;
  if (*buf == '\"')
  {
    buf++;
    while (*buf && *buf != '\"') buf++;
  }
  else
  {
    while (*buf && *buf != ' ') buf++;
  }
  if (*buf) buf++;
  while (*buf == ' ') buf++;
  if (*buf) return buf;

  return NULL;
}

int GetKeyNameTextUTF8(LONG lParam, LPTSTR lpString, int nMaxCount)
{
  if (!lpString) return 0;
  if (nMaxCount>0 AND_IS_NOT_WIN9X)
  {
    WIDETOMB_ALLOC(wbuf, nMaxCount);
    if (wbuf)
    {
      const int v = GetKeyNameTextW(lParam,wbuf,(int) (wbuf_size/sizeof(WCHAR)));

      if (v)
      {
        lpString[0]=0;
        if (!WideCharToMultiByte(CP_UTF8,0,wbuf,-1,lpString,nMaxCount,NULL,NULL))
          lpString[nMaxCount-1]=0;
      }
      WIDETOMB_FREE(wbuf);

      return v ? (int)strlen(lpString) : 0;
    }
  }
  return GetKeyNameTextA(lParam,lpString,nMaxCount);
}

HINSTANCE ShellExecuteUTF8(HWND hwnd, LPCTSTR lpOp, LPCTSTR lpFile, LPCTSTR lpParm, LPCTSTR lpDir, INT nShowCmd)
{
  if (IS_NOT_WIN9X_AND (WDL_HasUTF8(lpOp)||WDL_HasUTF8(lpFile)||WDL_HasUTF8(lpParm)||WDL_HasUTF8(lpDir)))
  {
    DWORD sz;
    WCHAR *p1=lpOp ? WDL_UTF8ToWC(lpOp,0,0,&sz) : NULL;
    WCHAR *p2=lpFile ? WDL_UTF8ToWC(lpFile,0,0,&sz) : NULL;
    WCHAR *p3=lpParm ? WDL_UTF8ToWC(lpParm,0,0,&sz) : NULL;
    WCHAR *p4=lpDir ? WDL_UTF8ToWC(lpDir,0,0,&sz) : NULL;
    HINSTANCE rv= p2 ? ShellExecuteW(hwnd,p1,p2,p3,p4,nShowCmd) : NULL;
    free(p1);
    free(p2);
    free(p3);
    free(p4);
    return rv;
  }
  return ShellExecuteA(hwnd,lpOp,lpFile,lpParm,lpDir,nShowCmd);
}

BOOL GetUserNameUTF8(LPTSTR lpString, LPDWORD nMaxCount)
{
  if (IS_NOT_WIN9X_AND lpString && nMaxCount)
  {
    WIDETOMB_ALLOC(wtmp,*nMaxCount);
    if (wtmp)
    {
      DWORD sz=(DWORD)(wtmp_size/sizeof(WCHAR));
      BOOL r = GetUserNameW(wtmp, &sz);
      if (r && (!*nMaxCount || (!WideCharToMultiByte(CP_UTF8,0,wtmp,-1,lpString,*nMaxCount,NULL,NULL) && GetLastError()==ERROR_INSUFFICIENT_BUFFER)))
      {
        if (*nMaxCount>0) lpString[*nMaxCount-1]=0;
        *nMaxCount=(int)wcslen(wtmp)+1;
        r=FALSE;
      }
      else
      {
        *nMaxCount=sz;
      }
      WIDETOMB_FREE(wtmp);
      return r;
    }
  }
  return GetUserNameA(lpString, nMaxCount);
}

BOOL GetComputerNameUTF8(LPTSTR lpString, LPDWORD nMaxCount)
{
  if (IS_NOT_WIN9X_AND lpString && nMaxCount)
  {
    WIDETOMB_ALLOC(wtmp,*nMaxCount);
    if (wtmp)
    {
      DWORD sz=(DWORD)(wtmp_size/sizeof(WCHAR));
      BOOL r = GetComputerNameW(wtmp, &sz);
      if (r && (!*nMaxCount || (!WideCharToMultiByte(CP_UTF8,0,wtmp,-1,lpString,*nMaxCount,NULL,NULL) && GetLastError()==ERROR_INSUFFICIENT_BUFFER)))
      {
        if (*nMaxCount>0) lpString[*nMaxCount-1]=0;
        *nMaxCount=(int)wcslen(wtmp)+1;
        r=FALSE;
      }
      else
      {
        *nMaxCount=sz;
      }
      WIDETOMB_FREE(wtmp);
      return r;
    }
  }
  return GetComputerNameA(lpString, nMaxCount);
}

#define MBTOWIDE_NULLOK(symbase, src) \
                int symbase##_size; \
                WCHAR symbase##_buf[256]; \
                WCHAR *symbase = (src)==NULL ? NULL : ((symbase##_size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,src,-1,NULL,0)) >= 248 ? (WCHAR *)malloc(symbase##_size * sizeof(WCHAR) + 10) : symbase##_buf); \
                int symbase##_ok = symbase ? (MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,src,-1,symbase,symbase##_size < 1 ? 256 : symbase##_size)) : (src)==NULL


// these only bother using Wide versions if the filename has wide chars
// (for now)
#define PROFILESTR_COMMON \
  if (IS_NOT_WIN9X_AND fnStr && WDL_HasUTF8(fnStr)) \
  { \
    MBTOWIDE(wfn,fnStr); \
    MBTOWIDE_NULLOK(wapp,appStr); \
    MBTOWIDE_NULLOK(wkey,keyStr); \
    if (wfn_ok && wapp_ok && wkey_ok) {

#define PROFILESTR_COMMON_END \
    MBTOWIDE_FREE(wfn); \
    MBTOWIDE_FREE(wapp); \
    MBTOWIDE_FREE(wkey); \
    return rv; \
    } }

UINT GetPrivateProfileIntUTF8(LPCTSTR appStr, LPCTSTR keyStr, INT def, LPCTSTR fnStr)
{
  PROFILESTR_COMMON

  const UINT rv = GetPrivateProfileIntW(wapp,wkey,def,wfn);

  PROFILESTR_COMMON_END
  return GetPrivateProfileIntA(appStr,keyStr,def,fnStr);
}

DWORD GetPrivateProfileStringUTF8(LPCTSTR appStr, LPCTSTR keyStr, LPCTSTR defStr, LPTSTR retStr, DWORD nSize, LPCTSTR fnStr)
{
  PROFILESTR_COMMON
  MBTOWIDE_NULLOK(wdef, defStr);

  WIDETOMB_ALLOC(buf, nSize);

  DWORD rv = GetPrivateProfileStringW(wapp,wkey,wdef,buf,(DWORD) (buf_size / sizeof(WCHAR)),wfn);

  const DWORD nullsz = (!wapp || !wkey) ? 2 : 1;
  if (nSize<=nullsz)
  {
    memset(retStr,0,nSize);
    rv=0;
  }
  else 
  {
    // rv does not include null character(s)
    if (rv>0) rv = WideCharToMultiByte(CP_UTF8,0,buf,rv,retStr,nSize-nullsz,NULL,NULL);
    if (rv > nSize-nullsz) rv=nSize-nullsz;
    memset(retStr + rv,0,nullsz);
  }
  
  WIDETOMB_FREE(buf);
  PROFILESTR_COMMON_END
  return GetPrivateProfileStringA(appStr,keyStr,defStr,retStr,nSize,fnStr);
}

BOOL WritePrivateProfileStringUTF8(LPCTSTR appStr, LPCTSTR keyStr, LPCTSTR str, LPCTSTR fnStr)
{
  PROFILESTR_COMMON
  MBTOWIDE_NULLOK(wval, str);

  const BOOL rv = WritePrivateProfileStringW(wapp,wkey,wval,wfn);

  MBTOWIDE_FREE(wval);

  PROFILESTR_COMMON_END
  return WritePrivateProfileStringA(appStr,keyStr,str,fnStr);
}

BOOL GetPrivateProfileStructUTF8(LPCTSTR appStr, LPCTSTR keyStr, LPVOID pStruct, UINT uSize, LPCTSTR fnStr)
{
  PROFILESTR_COMMON

  const BOOL rv = GetPrivateProfileStructW(wapp,wkey,pStruct,uSize,wfn);

  PROFILESTR_COMMON_END
  return GetPrivateProfileStructA(appStr,keyStr,pStruct,uSize,fnStr);
}

BOOL WritePrivateProfileStructUTF8(LPCTSTR appStr, LPCTSTR keyStr, LPVOID pStruct, UINT uSize, LPCTSTR fnStr)
{
  PROFILESTR_COMMON

  const BOOL rv = WritePrivateProfileStructW(wapp,wkey,pStruct,uSize,wfn);

  PROFILESTR_COMMON_END
  return WritePrivateProfileStructA(appStr,keyStr,pStruct,uSize,fnStr);
}


#undef PROFILESTR_COMMON
#undef PROFILESTR_COMMON_END


BOOL CreateProcessUTF8(LPCTSTR lpApplicationName,
  LPTSTR lpCommandLine, 
  LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
  BOOL bInheritHandles,
  DWORD dwCreationFlags, LPVOID lpEnvironment,  // pointer to new environment block
  LPCTSTR lpCurrentDirectory,
  LPSTARTUPINFO lpStartupInfo,
  LPPROCESS_INFORMATION lpProcessInformation )
{
  // special case ver
  if (IS_NOT_WIN9X_AND (
        WDL_HasUTF8(lpApplicationName) ||
        WDL_HasUTF8(lpCommandLine) ||
        WDL_HasUTF8(lpCurrentDirectory)
        )
      )
  {
    MBTOWIDE_NULLOK(appn, lpApplicationName);
    MBTOWIDE_NULLOK(cmdl, lpCommandLine);
    MBTOWIDE_NULLOK(curd, lpCurrentDirectory);

    if (appn_ok && cmdl_ok && curd_ok)
    {
      BOOL rv;
      WCHAR *free1=NULL, *free2=NULL;
      char *save1=NULL, *save2=NULL;

      if (lpStartupInfo && lpStartupInfo->cb >= sizeof(STARTUPINFO))
      {
        if (lpStartupInfo->lpDesktop)
          lpStartupInfo->lpDesktop = (char *) (free1 = WDL_UTF8ToWC(save1 = lpStartupInfo->lpDesktop,FALSE,0,NULL));
        if (lpStartupInfo->lpTitle)
          lpStartupInfo->lpTitle = (char*) (free2 = WDL_UTF8ToWC(save2 = lpStartupInfo->lpTitle,FALSE,0,NULL));
      }

      rv=CreateProcessW(appn,cmdl,lpProcessAttributes,lpThreadAttributes,bInheritHandles,dwCreationFlags,
        lpEnvironment,curd,(STARTUPINFOW*)lpStartupInfo,lpProcessInformation);

      if (lpStartupInfo && lpStartupInfo->cb >= sizeof(STARTUPINFO))
      {
        lpStartupInfo->lpDesktop = save1;
        lpStartupInfo->lpTitle = save2;
        free(free1);
        free(free2);
      }

      MBTOWIDE_FREE(appn);
      MBTOWIDE_FREE(cmdl);
      MBTOWIDE_FREE(curd);
      return rv;
    }
    MBTOWIDE_FREE(appn);
    MBTOWIDE_FREE(cmdl);
    MBTOWIDE_FREE(curd);
  }

  return CreateProcessA(lpApplicationName,lpCommandLine,lpProcessAttributes,lpThreadAttributes,bInheritHandles,dwCreationFlags,lpEnvironment,lpCurrentDirectory,lpStartupInfo,lpProcessInformation);
}


#if (defined(WDL_WIN32_UTF8_IMPL_NOTSTATIC) || defined(WDL_WIN32_UTF8_IMPL_STATICHOOKS)) && !defined(WDL_WIN32_UTF8_NO_UI_IMPL)

static LRESULT WINAPI cb_newProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  WNDPROC oldproc = (WNDPROC)GetProp(hwnd,WDL_UTF8_OLDPROCPROP);
  if (!oldproc) return 0;

  if (msg==WM_NCDESTROY)
  {
    SetWindowLongPtr(hwnd, GWLP_WNDPROC,(INT_PTR)oldproc);
    RemoveProp(hwnd,WDL_UTF8_OLDPROCPROP);
    RemoveProp(hwnd,WDL_UTF8_OLDPROCPROP "W");
  }
  else if (msg == CB_ADDSTRING || msg == CB_INSERTSTRING || msg == LB_ADDSTRING || msg == LB_INSERTSTRING)
  {
    char *str=(char*)lParam;
    if (lParam && WDL_HasUTF8(str))
    {
      MBTOWIDE(wbuf,str);
      if (wbuf_ok)
      {
        WNDPROC oldprocW = (WNDPROC)GetProp(hwnd,WDL_UTF8_OLDPROCPROP "W");
        LRESULT rv=CallWindowProcW(oldprocW ? oldprocW : oldproc,hwnd,msg,wParam,(LPARAM)wbuf);
        MBTOWIDE_FREE(wbuf);
        return rv;
      }

      MBTOWIDE_FREE(wbuf);
    }
  }
  else if ((msg == CB_GETLBTEXT || msg == LB_GETTEXT) && lParam)
  {
    WNDPROC oldprocW = (WNDPROC)GetProp(hwnd,WDL_UTF8_OLDPROCPROP "W");
    LRESULT l = CallWindowProcW(oldprocW ? oldprocW : oldproc,hwnd,msg == CB_GETLBTEXT ? CB_GETLBTEXTLEN : LB_GETTEXTLEN,wParam,0);
    
    if (l != CB_ERR)
    {
      WIDETOMB_ALLOC(tmp,l+1);
      if (tmp)
      {
        LRESULT rv=CallWindowProcW(oldprocW ? oldprocW : oldproc,hwnd,msg,wParam,(LPARAM)tmp);
        if (rv>=0)
        {
          *(char *)lParam=0;
          rv=WideCharToMultiByte(CP_UTF8,0,tmp,-1,(char *)lParam,((int)l)*4 + 32,NULL,NULL);
          if (rv>0) rv--;
        }
        WIDETOMB_FREE(tmp);

        return rv;
      }
    }
  }
  else if (msg == CB_GETLBTEXTLEN || msg == LB_GETTEXTLEN)
  {
    WNDPROC oldprocW = (WNDPROC)GetProp(hwnd,WDL_UTF8_OLDPROCPROP "W");
    return CallWindowProcW(oldprocW ? oldprocW : oldproc,hwnd,msg,wParam,lParam) * 4 + 32; // make sure caller allocates a lot extra
  }

  return CallWindowProc(oldproc,hwnd,msg,wParam,lParam);
}

void WDL_UTF8_HookComboBox(HWND h)
{
  if (!h||
    #ifdef WDL_SUPPORT_WIN9X
      GetVersion()>=0x80000000||
    #endif
    GetProp(h,WDL_UTF8_OLDPROCPROP)) return;
  SetProp(h,WDL_UTF8_OLDPROCPROP "W",(HANDLE)GetWindowLongPtrW(h,GWLP_WNDPROC));
  SetProp(h,WDL_UTF8_OLDPROCPROP,(HANDLE)SetWindowLongPtr(h,GWLP_WNDPROC,(INT_PTR)cb_newProc));

  if (!s_combobox_atom) s_combobox_atom = (ATOM)GetClassWord(h,GCW_ATOM);
}

void WDL_UTF8_HookListBox(HWND h)
{
  WDL_UTF8_HookComboBox(h);
}

static LRESULT WINAPI tc_newProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  WNDPROC oldproc = (WNDPROC)GetProp(hwnd,WDL_UTF8_OLDPROCPROP);
  if (!oldproc) return 0;

  if (msg==WM_NCDESTROY)
  {
    SetWindowLongPtr(hwnd, GWLP_WNDPROC,(INT_PTR)oldproc);
    RemoveProp(hwnd,WDL_UTF8_OLDPROCPROP);
  }
  else if (msg == TCM_INSERTITEMA) 
  {
    LPTCITEM pItem = (LPTCITEM) lParam;
    char *str;
    if (pItem && (str=pItem->pszText) && (pItem->mask&TCIF_TEXT) && WDL_HasUTF8(str))
    {
      MBTOWIDE(wbuf,str);
      if (wbuf_ok)
      {
        LRESULT rv;
        pItem->pszText=(char*)wbuf; // set new buffer
        rv=CallWindowProc(oldproc,hwnd,TCM_INSERTITEMW,wParam,lParam);
        pItem->pszText = str; // restore old pointer
        MBTOWIDE_FREE(wbuf);
        return rv;
      }

      MBTOWIDE_FREE(wbuf);
    }
  }


  return CallWindowProc(oldproc,hwnd,msg,wParam,lParam);
}


static LRESULT WINAPI tv_newProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  WNDPROC oldproc = (WNDPROC)GetProp(hwnd,WDL_UTF8_OLDPROCPROP);
  if (!oldproc) return 0;

  if (msg==WM_NCDESTROY)
  {
    SetWindowLongPtr(hwnd, GWLP_WNDPROC,(INT_PTR)oldproc);
    RemoveProp(hwnd,WDL_UTF8_OLDPROCPROP);
  }
  else if (msg == TVM_INSERTITEMA || msg == TVM_SETITEMA) 
  {
    LPTVITEM pItem = msg == TVM_INSERTITEMA ? &((LPTVINSERTSTRUCT)lParam)->item : (LPTVITEM) lParam;
    char *str;
    if (pItem && (str=pItem->pszText) && (pItem->mask&TVIF_TEXT) && WDL_HasUTF8(str))
    {
      MBTOWIDE(wbuf,str);
      if (wbuf_ok)
      {
        LRESULT rv;
        pItem->pszText=(char*)wbuf; // set new buffer
        rv=CallWindowProc(oldproc,hwnd,msg == TVM_INSERTITEMA ? TVM_INSERTITEMW : TVM_SETITEMW,wParam,lParam);
        pItem->pszText = str; // restore old pointer
        MBTOWIDE_FREE(wbuf);
        return rv;
      }

      MBTOWIDE_FREE(wbuf);
    }
  }
  else if (msg==TVM_GETITEMA)
  {
    LPTVITEM pItem = (LPTVITEM) lParam;
    char *obuf;
    if (pItem && (pItem->mask & TVIF_TEXT) && (obuf=pItem->pszText) && pItem->cchTextMax > 3)
    {
      WIDETOMB_ALLOC(wbuf,pItem->cchTextMax);
      if (wbuf)
      {
        LRESULT rv;
        int oldsz=pItem->cchTextMax;
        *wbuf=0;
        *obuf=0;
        pItem->cchTextMax=(int) (wbuf_size/sizeof(WCHAR));
        pItem->pszText = (char *)wbuf;
        rv=CallWindowProc(oldproc,hwnd,TVM_GETITEMW,wParam,lParam);

        if (!WideCharToMultiByte(CP_UTF8,0,wbuf,-1,obuf,oldsz,NULL,NULL))
          obuf[oldsz-1]=0;

        pItem->cchTextMax=oldsz;
        pItem->pszText=obuf;
        WIDETOMB_FREE(wbuf);

        if (obuf[0]) return rv;
      }
    }
  }

  return CallWindowProc(oldproc,hwnd,msg,wParam,lParam);
}

static LRESULT WINAPI lv_newProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  WNDPROC oldproc = (WNDPROC)GetProp(hwnd,WDL_UTF8_OLDPROCPROP);
  if (!oldproc) return 0;

  if (msg==WM_NCDESTROY)
  {
    SetWindowLongPtr(hwnd, GWLP_WNDPROC,(INT_PTR)oldproc);
    RemoveProp(hwnd,WDL_UTF8_OLDPROCPROP);
  }
  else if (msg == LVM_INSERTCOLUMNA || msg==LVM_SETCOLUMNA)
  {
    LPLVCOLUMNA pCol = (LPLVCOLUMNA) lParam;
    char *str;
    if (pCol && (str=pCol->pszText) && (pCol->mask & LVCF_TEXT) && WDL_HasUTF8(str))
    {
      MBTOWIDE(wbuf,str);
      if (wbuf_ok)
      {
        LRESULT rv;
        pCol->pszText=(char*)wbuf; // set new buffer
        rv=CallWindowProc(oldproc,hwnd,msg==LVM_INSERTCOLUMNA?LVM_INSERTCOLUMNW:LVM_SETCOLUMNW,wParam,lParam);
        pCol->pszText = str; // restore old pointer
        MBTOWIDE_FREE(wbuf);
        return rv;
      }

    }
  }
  else if (msg == LVM_INSERTITEMA || msg == LVM_SETITEMA || msg == LVM_SETITEMTEXTA) 
  {
    LPLVITEMA pItem = (LPLVITEMA) lParam;
    char *str;
    if (pItem && (str=pItem->pszText) && (msg==LVM_SETITEMTEXTA || (pItem->mask&LVIF_TEXT)) && WDL_HasUTF8(str))
    {
      MBTOWIDE(wbuf,str);
      if (wbuf_ok)
      {
        LRESULT rv;
        pItem->pszText=(char*)wbuf; // set new buffer
        rv=CallWindowProc(oldproc,hwnd,msg == LVM_INSERTITEMA ? LVM_INSERTITEMW : msg == LVM_SETITEMA ? LVM_SETITEMW : LVM_SETITEMTEXTW,wParam,lParam);
        pItem->pszText = str; // restore old pointer
        MBTOWIDE_FREE(wbuf);
        return rv;
      }

      MBTOWIDE_FREE(wbuf);
    }
  }
  else if (msg==LVM_GETITEMA||msg==LVM_GETITEMTEXTA)
  {
    LPLVITEMA pItem = (LPLVITEMA) lParam;
    char *obuf;
    if (pItem && (msg == LVM_GETITEMTEXTA || (pItem->mask & LVIF_TEXT)) && (obuf=pItem->pszText) && pItem->cchTextMax > 3)
    {
      WIDETOMB_ALLOC(wbuf,pItem->cchTextMax);
      if (wbuf)
      {
        LRESULT rv;
        int oldsz=pItem->cchTextMax;
        *wbuf=0;
        *obuf=0;
        pItem->cchTextMax=(int) (wbuf_size/sizeof(WCHAR));
        pItem->pszText = (char *)wbuf;
        rv=CallWindowProc(oldproc,hwnd,msg==LVM_GETITEMTEXTA ? LVM_GETITEMTEXTW : LVM_GETITEMW,wParam,lParam);

        if (!WideCharToMultiByte(CP_UTF8,0,wbuf,-1,obuf,oldsz,NULL,NULL))
          obuf[oldsz-1]=0;

        pItem->cchTextMax=oldsz;
        pItem->pszText=obuf;
        WIDETOMB_FREE(wbuf);

        if (obuf[0]) return rv;
      }
    }
  }

  return CallWindowProc(oldproc,hwnd,msg,wParam,lParam);
}

void WDL_UTF8_HookListView(HWND h)
{
  if (!h||
    #ifdef WDL_SUPPORT_WIN9X
      GetVersion()>=0x80000000||
    #endif
    GetProp(h,WDL_UTF8_OLDPROCPROP)) return;
  SetProp(h,WDL_UTF8_OLDPROCPROP,(HANDLE)SetWindowLongPtr(h,GWLP_WNDPROC,(INT_PTR)lv_newProc));
}

void WDL_UTF8_HookTreeView(HWND h)
{
  if (!h||
    #ifdef WDL_SUPPORT_WIN9X
      GetVersion()>=0x80000000||
    #endif
    GetProp(h,WDL_UTF8_OLDPROCPROP)) return;

  SetProp(h,WDL_UTF8_OLDPROCPROP,(HANDLE)SetWindowLongPtr(h,GWLP_WNDPROC,(INT_PTR)tv_newProc));
}

void WDL_UTF8_HookTabCtrl(HWND h)
{
  if (!h||
    #ifdef WDL_SUPPORT_WIN9X
      GetVersion()>=0x80000000||
    #endif
    GetProp(h,WDL_UTF8_OLDPROCPROP)) return;

  SetProp(h,WDL_UTF8_OLDPROCPROP,(HANDLE)SetWindowLongPtr(h,GWLP_WNDPROC,(INT_PTR)tc_newProc));
}

void WDL_UTF8_ListViewConvertDispInfoToW(void *_di)
{
  NMLVDISPINFO *di = (NMLVDISPINFO *)_di;
  if (di && (di->item.mask & LVIF_TEXT) && di->item.pszText && di->item.cchTextMax>0)
  {
    char tmp_buf[1024], *tmp=tmp_buf;
    char *src = di->item.pszText;

    if (strlen(src) < 1024) strcpy(tmp,src);
    else tmp = strdup(src);

    if (!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,tmp,-1,(LPWSTR)di->item.pszText,di->item.cchTextMax))
    {
      if (GetLastError()==ERROR_INSUFFICIENT_BUFFER)
      {
        ((WCHAR *)di->item.pszText)[di->item.cchTextMax-1] = 0;
      }
      else
      {
        if (!MultiByteToWideChar(CP_ACP,MB_ERR_INVALID_CHARS,tmp,-1,(LPWSTR)di->item.pszText,di->item.cchTextMax))
          ((WCHAR *)di->item.pszText)[di->item.cchTextMax-1] = 0;
      }
    }   

    if (tmp!=tmp_buf) free(tmp);

  }
}

#endif

#ifdef __cplusplus
};
#endif

#endif

#endif //_WIN32
