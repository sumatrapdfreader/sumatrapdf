#include "win7filedialog.h"

#include "ptrlist.h"
#include "win32_utf8.h"


Win7FileDialog::Win7FileDialog(const char *name, int issave)
{
  m_dlgid = 0;
  m_inst = 0;
  m_proc = NULL;

  if(!issave)
    CoCreateInstance(__uuidof(FileOpenDialog), NULL, CLSCTX_INPROC_SERVER, IID_IUnknown, reinterpret_cast<void**>(&m_fod));
  else
    CoCreateInstance(__uuidof(FileSaveDialog), NULL, CLSCTX_INPROC_SERVER, IID_IUnknown, reinterpret_cast<void**>(&m_fod));

  if(m_fod != NULL)
  {
    m_fdc = m_fod;

#if defined(WDL_NO_SUPPORT_UTF8)
    WCHAR tmp[1024];
    mbstowcs(tmp, name, 1023);
    tmp[1023]=0;
    m_fod->SetTitle(tmp);
#else
    WCHAR *tmp = WDL_UTF8ToWC(name, false, 0, NULL);
    m_fod->SetTitle(tmp);
    free(tmp);
#endif
  }
}

Win7FileDialog::~Win7FileDialog()
{
}

void Win7FileDialog::setFilterList(const char *list)
{
  //generate wchar filter list
  WDL_PtrList<WCHAR> wlist;
  {
    const char *p = list;
    while(*p)
    {
#if defined(WDL_NO_SUPPORT_UTF8)
      int l = strlen(p);
      WCHAR *n = (WCHAR*)malloc(sizeof(WCHAR)*(l+1));
      mbstowcs(n, p, l+1);
      wlist.Add(n);
#else
      wlist.Add(WDL_UTF8ToWC(p, false, 0, NULL));
#endif
      p+=strlen(p)+1;
    }
  }
  m_fod->SetFileTypes(wlist.GetSize()/2, (_COMDLG_FILTERSPEC *)wlist.GetList());
  wlist.Empty(true,free);
}

void Win7FileDialog::addOptions(DWORD o)
{
  DWORD fileOpenDialogOptions; 
  m_fod->GetOptions(&fileOpenDialogOptions); 
  fileOpenDialogOptions |= o; 
  m_fod->SetOptions(fileOpenDialogOptions); 
}

void Win7FileDialog::setDefaultExtension(const char *ext)
{
#if defined(WDL_NO_SUPPORT_UTF8)
  WCHAR tmp[1024];
  mbstowcs(tmp, ext, 1023);
  tmp[1023]=0;
  m_fod->SetDefaultExtension(tmp);
#else
  WCHAR *tmp = WDL_UTF8ToWC(ext, false, 0, NULL);
  m_fod->SetDefaultExtension(tmp);
  free(tmp);
#endif
}

void Win7FileDialog::setFileTypeIndex(int i)
{
  m_fod->SetFileTypeIndex(i);
}

void Win7FileDialog::setFolder(const char *folder, int def)
{
  static HRESULT (WINAPI *my_SHCreateItemFromParsingName)(PCWSTR pszPath, IBindCtx *pbc, REFIID riid, void **ppv) = NULL;
  if(!my_SHCreateItemFromParsingName)
  {
    HMODULE dll = LoadLibrary("shell32.dll");
    if(dll)
    {
      *((void **)(&my_SHCreateItemFromParsingName)) = (void *)GetProcAddress(dll, "SHCreateItemFromParsingName");
    }
  }
  if(!my_SHCreateItemFromParsingName) return;

#if defined(WDL_NO_SUPPORT_UTF8)
  WCHAR tmp[4096];
  mbstowcs(tmp, folder, 4095);
  tmp[4095]=0;
#else
  WCHAR *tmp = WDL_UTF8ToWC(folder, false, 0, NULL);
#endif

  IShellItemPtr si;
  my_SHCreateItemFromParsingName(tmp, NULL, __uuidof(IShellItem), (void **)&si);

#if !defined(WDL_NO_SUPPORT_UTF8)
  free(tmp);
#endif

  if(si == NULL) return;

  if(def)
    m_fod->SetDefaultFolder(si);
  else
    m_fod->SetFolder(si);
}

void Win7FileDialog::addText(DWORD id, char *txt)
{
  if(m_fdc == NULL) return;
#if defined(WDL_NO_SUPPORT_UTF8)
  WCHAR tmp[1024];
  mbstowcs(tmp, txt, 1023);
  tmp[1023]=0;
  m_fdc->AddText(id, tmp);
#else
  WCHAR *tmp = WDL_UTF8ToWC(txt, false, 0, NULL);
  m_fdc->AddText(id, tmp);
  free(tmp);
#endif
}

void Win7FileDialog::addCheckbox(char *name, DWORD id, int defval)
{
  if(m_fdc == NULL) return;

#if defined(WDL_NO_SUPPORT_UTF8)
  WCHAR tmp[1024];
  mbstowcs(tmp, name, 1023);
  tmp[1023]=0;
  m_fdc->AddCheckButton(id, tmp, defval);
#else
  WCHAR *tmp = WDL_UTF8ToWC(name, false, 0, NULL);
  m_fdc->AddCheckButton(id, tmp, defval);
  free(tmp);
#endif
}

void Win7FileDialog::startGroup(DWORD id, char *label)
{
  if(m_fdc == NULL) return;
#if defined(WDL_NO_SUPPORT_UTF8)
  WCHAR tmp[1024];
  mbstowcs(tmp, label, 1023);
  tmp[1023]=0;
  m_fdc->StartVisualGroup(id, tmp);
#else
  WCHAR *tmp = WDL_UTF8ToWC(label, false, 0, NULL);
  m_fdc->StartVisualGroup(id, tmp);
  free(tmp);
#endif
}

void Win7FileDialog::endGroup()
{
  if(m_fdc == NULL) return;
  m_fdc->EndVisualGroup();
}

void Win7FileDialog::getResult(char *fn, int maxlen)
{
  IShellItemPtr si;
  m_fod->GetResult(&si);
  if(si == NULL)
  {
    fn[0] = 0;
    return;
  }
  WCHAR *res = NULL;
  si->GetDisplayName(SIGDN_FILESYSPATH, &res);
  if(!res)
  {
    fn[0] = 0;
    return;
  }

#if defined(WDL_NO_SUPPORT_UTF8)
    if (wcstombs(fn,res,maxlen) == (size_t)-1) fn[0]=0;
#else
  int len = WideCharToMultiByte(CP_UTF8,0,res,-1,fn,maxlen-1,NULL,NULL); 
  fn[len] = 0;
#endif

  CoTaskMemFree(res);
}

int Win7FileDialog::getResult(int i, char *fn, int maxlen)
{
  IShellItemArrayPtr sia;
  ((IFileOpenDialogPtr)m_fod)->GetResults(&sia); // good enough: only makes sense with IFileOpenDialog
  if (sia == NULL)
  {
    fn[0] = 0;
    return 0;
  }

  IShellItemPtr item;
  if (SUCCEEDED(sia->GetItemAt(i, &item)))
  {
    WCHAR *res = NULL;
    item->GetDisplayName(SIGDN_FILESYSPATH, &res);
    if(!res)
    {
      fn[0] = 0;
      return 0;
    }

    int len=0;
#if defined(WDL_NO_SUPPORT_UTF8)
    size_t l = wcstombs(fn, res, maxlen);
    if (l==(size_t)-1) fn[0]=0;
    else len=l+1;
#else
    len = WideCharToMultiByte(CP_UTF8,0,res,-1,fn,maxlen-1,NULL,NULL);
#endif

    CoTaskMemFree(res);
    return len;
  }
  return 0;
}

int Win7FileDialog::getResultCount()
{
  IShellItemArrayPtr sia;
  ((IFileOpenDialogPtr)m_fod)->GetResults(&sia); // good enough: only makes sense with IFileOpenDialog
  if (sia == NULL) return 0;

  DWORD cnt;
  return SUCCEEDED(sia->GetCount(&cnt)) ? cnt : 0;
}

int Win7FileDialog::getState(DWORD id)
{
  BOOL c = FALSE;
  m_fdc->GetCheckButtonState(id, &c);
  return c;
}

static WNDPROC m_oldproc, m_oldproc2;
static LRESULT CALLBACK newWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  if(msg == WM_SIZE) 
  {
    //disable the win7 dialog to resize our custom dialog
    static int reent = 0;
    if(!reent)
    {
      reent = 1;
      RECT r2;
      GetWindowRect(hwnd, &r2);
      SetWindowPos(hwnd, NULL, r2.left, r2.top, 1000, r2.bottom-r2.top, SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
      reent = 0;
    }
    return 0;
  }
  if(msg == WM_GETDLGCODE)
  {
    int a=1;
  }
  return CallWindowProc(m_oldproc, hwnd, msg, wparam, lparam);
}

class myEvent : public IFileDialogEvents
{
public:
  myEvent(HINSTANCE inst, char *dlgid, LPOFNHOOKPROC proc, char *statictxt)
  {
    m_didhook = 0;
    m_didhook2 = 0;
    m_inst = inst;
    m_dlgid = dlgid;
    m_proc = proc;
    m_crwnd = NULL;
    m_statictxt.Set(statictxt);
  }

  STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
  {
    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef()
  {
    return 1;
  }
  STDMETHODIMP_(ULONG) Release()
  {
    if(m_crwnd) DestroyWindow(m_crwnd);
    return 0;
  }

  virtual HRESULT STDMETHODCALLTYPE OnFileOk( 
    /* [in] */ __RPC__in_opt IFileDialog *pfd)
  {
    return E_NOTIMPL;
  }
    
  virtual HRESULT STDMETHODCALLTYPE OnFolderChanging( 
    /* [in] */ __RPC__in_opt IFileDialog *pfd,
    /* [in] */ __RPC__in_opt IShellItem *psiFolder)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT STDMETHODCALLTYPE OnFolderChange( 
    /* [in] */ __RPC__in_opt IFileDialog *pfd)
  {
    doHook2(pfd); //post a msg for the actual hook
    return E_NOTIMPL;
  }
  
  virtual HRESULT STDMETHODCALLTYPE OnSelectionChange( 
    /* [in] */ __RPC__in_opt IFileDialog *pfd)
  {
    doHook2(pfd); //post a msg for the actual hook
    return E_NOTIMPL;
  }
  
  virtual HRESULT STDMETHODCALLTYPE OnShareViolation( 
    /* [in] */ __RPC__in_opt IFileDialog *pfd,
    /* [in] */ __RPC__in_opt IShellItem *psi,
    /* [out] */ __RPC__out FDE_SHAREVIOLATION_RESPONSE *pResponse)
  {
    return E_NOTIMPL;
  }
  
  virtual HRESULT STDMETHODCALLTYPE OnTypeChange( 
    /* [in] */ __RPC__in_opt IFileDialog *pfd)
  {
    return E_NOTIMPL;
  }
  
  virtual HRESULT STDMETHODCALLTYPE OnOverwrite( 
    /* [in] */ __RPC__in_opt IFileDialog *pfd,
    /* [in] */ __RPC__in_opt IShellItem *psi,
    /* [out] */ __RPC__out FDE_OVERWRITE_RESPONSE *pResponse)
  {
    return E_NOTIMPL;
  }

  static BOOL CALLBACK enumProc(HWND hwnd, LPARAM lParam)
  {
    char tmp[1024]={0,};
    GetClassName(hwnd, tmp, 1023);
    if(!stricmp(tmp,"FloatNotifySink"))
    {
      myEvent *me = (myEvent *)lParam;
      if(!FindWindowEx(hwnd, NULL, NULL, me->m_statictxt.Get()))
        return TRUE;
      me->m_findwnd = hwnd;
      return FALSE;
    }
    return TRUE;
  }

  void doHook()
  {
    if(m_dlgid && !m_didhook)
    {
      IOleWindowPtr ow = m_lastpfd;
      if(ow!=NULL)
      {
        HWND filehwnd = NULL;
        ow->GetWindow(&filehwnd);
        if(filehwnd)
        {
          m_findwnd = NULL;
          EnumChildWindows(filehwnd, enumProc, (LPARAM)this);
          if(m_findwnd) 
          {
            //hide other button
            HWND h3 = m_findwnd;
            HWND h5 = FindWindowEx(h3, NULL, NULL, NULL);
            ShowWindow(h5, SW_HIDE);
            
            //resize the sink
            RECT r,r2;
            GetWindowRect(h3, &r2);
            SetWindowPos(h3, NULL, r2.left, r2.top, 1000, r2.bottom-r2.top, SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
            
            //put our own dialog instead
            HWND h4 = CreateDialog(m_inst, m_dlgid, h3, (DLGPROC)m_proc);

            //SetWindowLong(h4, GWL_ID, 1001);
            //SetWindowLong(h4, GWL_STYLE, GetWindowLong(h4, GWL_STYLE)&~(DS_CONTROL|DS_3DLOOK|DS_SETFONT));
            //SetWindowLong(h4, GWL_STYLE, GetWindowLong(h4, GWL_STYLE)|WS_GROUP);
            SetWindowLong(h3, GWL_STYLE, GetWindowLong(h3, GWL_STYLE)|WS_TABSTOP);

            m_crwnd = h4;
            ShowWindow(h4, SW_SHOW);
            
            GetClientRect(h3, &r);
            SetWindowPos(h4, NULL, 0, 0, r.right, r.bottom, 0);
            
            //disable the win7 dialog to resize our custom dialog sink
            m_oldproc = (WNDPROC)SetWindowLongPtr(h3, GWLP_WNDPROC, (LPARAM)&newWndProc);
            m_didhook = 1;
          }
        }
      }
    }
  }

  static LRESULT CALLBACK newWndProc2(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
  {
    if(msg == WM_USER+666)
    {
      myEvent *me = (myEvent*)lparam;
      me->doHook();
    }
    return CallWindowProc(m_oldproc2, hwnd, msg, wparam, lparam);
  }

  void doHook2(IFileDialog *pfd)
  {
    if(m_dlgid && !m_didhook2)
    {
      IOleWindowPtr ow = pfd;
      HWND filehwnd = NULL;
      ow->GetWindow(&filehwnd);
      if(filehwnd)
      {
        m_lastpfd = pfd;
        m_oldproc2 = (WNDPROC)SetWindowLongPtr(filehwnd, GWLP_WNDPROC, (LPARAM)&newWndProc2);
        PostMessage(filehwnd, WM_USER+666,0,(LPARAM)this);
        m_didhook2 = 1;
      }
    }
  }

  int m_didhook, m_didhook2;
  HINSTANCE m_inst;
  char *m_dlgid;
  LPOFNHOOKPROC m_proc;
  HWND m_findwnd;
  HWND m_crwnd;
  WDL_String m_statictxt;
  IFileDialog *m_lastpfd;
};

int Win7FileDialog::show(HWND parent)
{
  DWORD c;
  myEvent *ev = new myEvent(m_inst, (char*)m_dlgid, m_proc, m_statictxt.Get());
  m_fod->Advise(ev, &c);

  int res = SUCCEEDED(m_fod->Show(parent)); 

  m_fod->Unadvise(c);
  delete ev;
  return res;
}

#ifndef DLGTEMPLATEEX
#pragma pack(push, 1)
typedef struct
{
  WORD dlgVer;
  WORD signature;
  DWORD helpID;
  DWORD exStyle;
  DWORD style;
  WORD cDlgItems;
  short x;
  short y;
  short cx;
  short cy;
} DLGTEMPLATEEX;
#pragma pack(pop)
#endif

void Win7FileDialog::setTemplate(HINSTANCE inst, const char *dlgid, LPOFNHOOKPROC proc)
{
  //get the dialog height size
  HRSRC r = FindResource(inst, dlgid, RT_DIALOG);
  if(!r) return;
  HGLOBAL hTemplate = LoadResource(inst, r);
  if(!hTemplate) return;
  int ysizedlg = 0;
  DLGTEMPLATEEX* pTemplate = (DLGTEMPLATEEX*)LockResource(hTemplate);
  if(pTemplate->signature == 0xffff)
    ysizedlg = pTemplate->cy;
  else
  {
    DLGTEMPLATE *p2 = (DLGTEMPLATE*)pTemplate;
    ysizedlg = p2->cy;
  }
  UnlockResource(hTemplate);
  FreeResource(hTemplate);

  int ysize = ysizedlg/8;

  //make room for our custom template dialog
  WDL_String txt(".");
  if(ysize)
  {
    while(--ysize)
    {
      txt.Append("\n.");
    }
  }
  addText(1, txt.Get());
  m_statictxt.Set(txt.Get());
  addText(2, "");

  m_inst = inst; 
  m_dlgid = dlgid; 
  m_proc = proc;
}

void Win7FileDialog::setFilename(const char *fn)
{
  if(m_fod == NULL) return;

#if defined(WDL_NO_SUPPORT_UTF8)
  WCHAR tmp[4096];
  mbstowcs(tmp, fn, 4095);
  tmp[4095]=0;
  m_fod->SetFileName(tmp);
#else
  WCHAR *tmp = WDL_UTF8ToWC(fn, false, 0, NULL);
  m_fod->SetFileName(tmp);
  free(tmp);
#endif

}
