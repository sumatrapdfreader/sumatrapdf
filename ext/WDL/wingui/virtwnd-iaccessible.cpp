#if defined(_WIN32) && !defined(WDL_DISABLE_IACCESSIBLE)

#include <windows.h>

#include <oleacc.h>
#if !defined(_MSC_VER) || _MSC_VER < 1600
#include <winable.h>
#else
#include <WinUser.h>
#endif


#include "virtwnd-controls.h"
#include "../wdltypes.h"
#include "../wdlcstring.h"

static BSTR SysAllocStringUTF8(const char *str)
{
  WCHAR tmp[1024];
  const int slen = (int)strlen(str)+1;
  WCHAR *wstr = slen < 1000 ? tmp : (WCHAR*)malloc(2*slen+32);
  if (!wstr) return NULL;

  wstr[0]=0;
  int a=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,str,slen,wstr, slen<1000?1024:slen+16);
  if (!a)
  {
    wstr[0]=0;
    a=MultiByteToWideChar(CP_ACP,MB_ERR_INVALID_CHARS,str,slen,wstr,slen<1000?1024:slen+16);
  }

  BSTR ret = SysAllocString(wstr);
  if (wstr != tmp) free(wstr);
  return ret;
}

class CVWndAccessible;
class VWndBridge : public WDL_VWnd_IAccessibleBridge
{
public:
  VWndBridge() { }
  virtual ~VWndBridge() { }
  virtual void Release() {  vwnd=0; }

  virtual void OnFocused()
  {
    DoNotify(EVENT_OBJECT_FOCUS);
  }
  virtual void OnStateChange() 
  {
    DoNotify(EVENT_OBJECT_VALUECHANGE);
  }
  void DoNotify(int mode)
  {
    if (vwnd)
    {
      HWND hwnd = vwnd->GetRealParent();
      if (hwnd)
      {
        int idx = CHILDID_SELF;
        WDL_VWnd *lpar = vwnd, *par = NULL;
        // todo: better handle hierarchy?
        if (lpar) while ((par=lpar->GetParent()))
        {
          if (!par->GetParent()) break;
          lpar=par;
        }
        if (par)
        {
          for (idx=0; idx < par->GetNumChildren(); idx++) if (par->EnumChildren(idx)==lpar) break;
          if (idx >= par->GetNumChildren()) idx = CHILDID_SELF;
          else idx++;
        }
        NotifyWinEvent(mode,hwnd,OBJID_CLIENT,idx); 
      }
    }
  }

  CVWndAccessible *par;
  WDL_VWnd *vwnd;

};
static IAccessible *GetVWndIAccessible(WDL_VWnd *vwnd);

static int g_freelist_acc_size;
static CVWndAccessible *g_freelist_acc;

static HRESULT (WINAPI *__CreateStdAccessibleObject)(
  HWND hwnd,
  LONG idObject,
  REFIID riidInterface,
  void **ppvObject
);


static int allocated_cnt;
class CVWndAccessible : public IAccessible
{
public:
  CVWndAccessible(WDL_VWnd *vwnd) 
  { 
    m_br.vwnd=vwnd;
    m_br.par = this;
    m_refCnt = 1;
    allocated_cnt++;
  }
  virtual ~CVWndAccessible() 
  { 
    allocated_cnt--;
    //char buf[512];
    //sprintf(buf,"allocated total = %d\n",allocated_cnt);
  //  OutputDebugString(buf);
  }

 //IUnknown interface 
  STDMETHOD_(HRESULT, QueryInterface)(REFIID riid , void **ppObj)
  {
    if (IsEqualIID(riid, IID_IUnknown))
    {
      *ppObj = (IUnknown*)this;
    }
    else if (IsEqualIID(riid, IID_IAccessible))
    {
      *ppObj = (IAccessible*)this;
    }
    else if (IsEqualIID(riid, IID_IDispatch))
    {
      *ppObj = (IDispatch*)this;
    }
    else
    {
      *ppObj = NULL;
      return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
  }
  STDMETHOD_(ULONG, AddRef)()
  {
    return InterlockedIncrement(&m_refCnt);
  }
  STDMETHOD_(ULONG, Release)()
  {
    LONG nRefCount=0;
    nRefCount=InterlockedDecrement(&m_refCnt) ;
    if (nRefCount == 0) 
    {
      if (m_br.vwnd)
      {
        m_br.vwnd->SetAccessibilityBridge(NULL);
        m_br.vwnd=0;
      }

      if (g_freelist_acc_size<2048)
      {
        g_freelist_acc_size++;
        _freelist_next = g_freelist_acc;
        g_freelist_acc = this;
      }
      else
      {
        delete this;
      }
    }
    return nRefCount;
  }


  //IDispatch
  STDMETHOD(GetTypeInfoCount)(unsigned int FAR* pctinfo )
  {
    *pctinfo=0;
    return E_NOTIMPL;
  }
  STDMETHOD(GetTypeInfo)(unsigned int iTInfo, LCID lcid, ITypeInfo FAR* FAR* ppTInfo)
  {
    return E_NOTIMPL;
  }
  STDMETHOD( GetIDsOfNames)( 
    REFIID riid, 
    OLECHAR FAR* FAR* rgszNames, 
    unsigned int cNames, 
    LCID lcid, 
    DISPID FAR* rgDispId 
  )
  {
    *rgDispId=0;
    return E_NOTIMPL;
  }

  STDMETHOD(Invoke)( 
    DISPID dispIdMember, 
    REFIID riid, 
    LCID lcid, 
    WORD wFlags, 
    DISPPARAMS FAR* pDispParams, 
    VARIANT FAR* pVarResult, 
    EXCEPINFO FAR* pExcepInfo, 
    unsigned int FAR* puArgErr 
  )
  {
    return E_NOTIMPL;
  }

  // IAccessible


  STDMETHOD(get_accParent)(THIS_ IDispatch * FAR* ppdispParent) 
  {
    WDL_VWnd *par=m_br.vwnd ? m_br.vwnd->GetParent() : NULL;
    if (par)
    {
      *ppdispParent = GetVWndIAccessible(par);
      if (*ppdispParent) return S_OK;
    }

    *ppdispParent = NULL;

    if (__CreateStdAccessibleObject && m_br.vwnd)
    {
      HWND realparent = m_br.vwnd->GetRealParent();
      if (realparent)
      {
        HRESULT res = __CreateStdAccessibleObject(realparent,OBJID_WINDOW,IID_IAccessible,(void**)ppdispParent); // should this be OBJID_CLIENT?
        if (SUCCEEDED(res))
          return S_OK;
      }
    }

    return S_FALSE;
  }
#define ISVWNDLIST(x) ((x)&&!strcmp((x)->GetType(),"vwnd_listbox"))
  STDMETHOD(get_accChildCount)(THIS_ long FAR* pChildCount) 
  {
    *pChildCount = m_br.vwnd ? m_br.vwnd->GetNumChildren() : 0;
    HWND realparent;
    if (__CreateStdAccessibleObject && m_br.vwnd && !m_br.vwnd->GetParent() && (realparent=m_br.vwnd->GetRealParent()))
    { 
      HWND h=GetWindow(realparent,GW_CHILD);
      while (h)
      {
        (*pChildCount) += 1;
        h=GetWindow(h,GW_HWNDNEXT);
      }
    }

    if (ISVWNDLIST(m_br.vwnd))
    {
      WDL_VirtualListBox *list=(WDL_VirtualListBox*)m_br.vwnd;
      int c = 0;
      if (list->m_GetItemInfo) c=list->m_GetItemInfo(list,-1,NULL,0,NULL,NULL);
      if(c<0)c=0;
      *pChildCount += c+2;
    }
    return S_OK;
  }
  STDMETHOD(get_accChild)(THIS_ VARIANT varChildIndex, IDispatch * FAR* ppdispChild) 
  {
    *ppdispChild=0;
    if (!m_br.vwnd || varChildIndex.vt != VT_I4) return E_INVALIDARG;

    WDL_VWnd *vw = m_br.vwnd->EnumChildren(varChildIndex.lVal-1);
    if (vw)
    {
      *ppdispChild=GetVWndIAccessible(vw);
      if (*ppdispChild) return S_OK;
    }

    int index =  varChildIndex.lVal-1 - m_br.vwnd->GetNumChildren();

    if (ISVWNDLIST(m_br.vwnd))
    {
      WDL_VirtualListBox *list=(WDL_VirtualListBox*)m_br.vwnd;
      int c = 0;
      if (list->m_GetItemInfo) c=list->m_GetItemInfo(list,-1,NULL,0,NULL,NULL);
      if(c<0)c=0;
      index -= c+2;
    }


    HWND realparent;
    if (index >= 0 && __CreateStdAccessibleObject && m_br.vwnd && !m_br.vwnd->GetParent() && (realparent=m_br.vwnd->GetRealParent()))
    {      
      HWND h=GetWindow(realparent,GW_CHILD);
      while (h)
      {
        if (!index) break;
        index--;
        h=GetWindow(h,GW_HWNDNEXT);
      }

      if (h)
      {
        *ppdispChild=0;
        HRESULT res = __CreateStdAccessibleObject(h,OBJID_CLIENT,IID_IAccessible,(void**)ppdispChild);
        if (SUCCEEDED(res)) 
        {
          return S_OK;
        }

      }
    }

    
    return S_FALSE;
  }

  STDMETHOD(get_accName)(THIS_ VARIANT varChild, BSTR* pszOut) 
  {
    *pszOut=NULL;
    if (!m_br.vwnd || varChild.vt != VT_I4) 
    {
      return E_INVALIDARG;
    }
    WDL_VWnd *vw = varChild.lVal == CHILDID_SELF ? m_br.vwnd : m_br.vwnd->EnumChildren(varChild.lVal-1);

    if (vw)
    {
      const char *txt=NULL;
      const char *ctltype = vw->GetType();
      if (!strcmp(ctltype,"vwnd_iconbutton"))
        txt = ((WDL_VirtualIconButton*)vw)->GetTextLabel();
      else if (!strcmp(ctltype,"vwnd_statictext"))
        txt = ((WDL_VirtualStaticText*)vw)->GetText();
      else if (!strcmp(ctltype,"vwnd_combobox"))
        txt = ((WDL_VirtualComboBox*)vw)->GetItem(((WDL_VirtualComboBox*)vw)->GetCurSel());

      const char *p = vw->GetAccessDesc();
      if (p && *p && txt && *txt)
      {
        char buf[1024];
        snprintf(buf,sizeof(buf),"%.500s %.500s",p,txt);
        *pszOut= SysAllocStringUTF8(buf);
        if (!*pszOut) return E_OUTOFMEMORY;
      }
      else if (txt && *txt)
      {
        *pszOut= SysAllocStringUTF8(txt);
        if (!*pszOut) return E_OUTOFMEMORY;
      }
      else if (p && *p)
      {
        *pszOut = SysAllocStringUTF8(p);
        if (!*pszOut) return E_OUTOFMEMORY;
      }
    }
    else if (ISVWNDLIST(m_br.vwnd))
    {
      WDL_VirtualListBox *list=(WDL_VirtualListBox*)m_br.vwnd;
      if (list->m_GetItemInfo)
      {
        int idx = varChild.lVal-1 - m_br.vwnd->GetNumChildren();
        int ni = list->m_GetItemInfo(list,-1,NULL,0,NULL,NULL);
        char buf[2048];
        buf[0]=0;
        if (idx>=0&&idx<ni)
          list->m_GetItemInfo(list,idx,buf,512,NULL,NULL);
        else if (idx==ni||idx==ni+1)
        {
          strcpy(buf,idx==ni?"Scroll previous" : "Scroll next");
        }

        // we put this in the desc field instead
        /*const char *txt1 = list->GetAccessDesc();
        if (txt1)
        {
          if (buf[0]) strcat(buf," ");
          lstrcpyn_safe(buf+strlen(buf),txt1,512);
        }*/

//          OutputDebugString(buf);
        if (buf[0])
        {
          *pszOut = SysAllocStringUTF8(buf);
          if (!*pszOut) return E_OUTOFMEMORY;
        }
      }
    }
    return S_OK;

  }
  STDMETHOD(get_accValue)(THIS_ VARIANT varChild, BSTR* pszValue)
  {
    *pszValue=NULL;
    if (!m_br.vwnd || varChild.vt != VT_I4) 
    {
      return E_INVALIDARG;
    }
    WDL_VWnd *vw = varChild.lVal == CHILDID_SELF ? m_br.vwnd : m_br.vwnd->EnumChildren(varChild.lVal-1);
    if (vw && !strcmp(vw->GetType(),"vwnd_slider"))
    {
      char buf[1024];
      buf[0]=0;
      if (vw->GetAccessValueDesc(buf,sizeof(buf)) && buf[0])
      {
        *pszValue = SysAllocStringUTF8(buf);
        if (!*pszValue) return E_OUTOFMEMORY;
        return S_OK;
      }
    }
    return DISP_E_MEMBERNOTFOUND;
  }
  STDMETHOD(get_accDescription)(THIS_ VARIANT varChild, BSTR FAR* pszOut)
  {
    *pszOut=NULL;
    if (!m_br.vwnd || varChild.vt != VT_I4) 
    {
      return E_INVALIDARG;
    }
    WDL_VWnd *vw = varChild.lVal == CHILDID_SELF ? m_br.vwnd : m_br.vwnd->EnumChildren(varChild.lVal-1);
    if (vw) 
    {
      return S_FALSE;
    }
    else if (ISVWNDLIST(m_br.vwnd))
    {
      WDL_VirtualListBox *list=(WDL_VirtualListBox*)m_br.vwnd;
      if (list->m_GetItemInfo)
      {
        const char *txt = list->GetAccessDesc();
        if (txt)
        {
          *pszOut = SysAllocStringUTF8(txt);
          if (!*pszOut) return E_OUTOFMEMORY;
          return S_OK;
        }
      }
    }
    return E_INVALIDARG;
  }

  STDMETHOD(get_accRole)(THIS_ VARIANT varChild, VARIANT *pvarRole) 
  {
    if (!m_br.vwnd || varChild.vt != VT_I4) 
    {
      pvarRole->vt = VT_EMPTY;
      return E_INVALIDARG;
    }
    WDL_VWnd *vw = varChild.lVal == CHILDID_SELF ? m_br.vwnd : m_br.vwnd->EnumChildren(varChild.lVal-1);

    if (!vw && ISVWNDLIST(m_br.vwnd))
    {
      WDL_VirtualListBox *list=(WDL_VirtualListBox*)m_br.vwnd;
      if (list->m_GetItemInfo)
      {
        int idx = varChild.lVal-1 - m_br.vwnd->GetNumChildren();
        int ni = list->m_GetItemInfo(list,-1,NULL,0,NULL,NULL);
        if (idx>=0&&idx<ni)
        {
          pvarRole->vt = VT_I4;
          pvarRole->lVal = ROLE_SYSTEM_LISTITEM;
          return S_OK;
        }
        else if (idx==ni || idx==ni+1)
        {
          pvarRole->vt = VT_I4;
          pvarRole->lVal = ROLE_SYSTEM_PUSHBUTTON;
          return S_OK;          
        }
      }
    }

    if (!vw) 
    {
      pvarRole->vt = VT_EMPTY;
      return E_INVALIDARG;
    }

    pvarRole->vt = VT_I4;
    const char *type = vw->GetType();

    if (!strcmp(type,"vwnd_iconbutton")) 
    {
      WDL_VirtualIconButton *vb = (WDL_VirtualIconButton*)vw;
      if (vb->GetIsButton())
      {
        if (vb->GetCheckState()>=0) pvarRole->lVal = ROLE_SYSTEM_CHECKBUTTON;
        else pvarRole->lVal = ROLE_SYSTEM_PUSHBUTTON;
      }
      else
        pvarRole->lVal = ROLE_SYSTEM_STATICTEXT;       
    }
    else if (!strcmp(type,"vwnd_statictext"))  pvarRole->lVal = ROLE_SYSTEM_STATICTEXT;
    else if (!strcmp(type,"vwnd_combobox"))  pvarRole->lVal = ROLE_SYSTEM_COMBOBOX;
    else if (!strcmp(type,"vwnd_slider"))  pvarRole->lVal = ROLE_SYSTEM_SLIDER;
    else if (!strcmp(type,"vwnd_tabctrl_proxy"))  pvarRole->lVal = ROLE_SYSTEM_PAGETABLIST;
    else if (!strcmp(type,"vwnd_tabctrl_child"))  pvarRole->lVal = ROLE_SYSTEM_PAGETAB;
    else if (!strcmp(type,"vwnd_listbox"))  pvarRole->lVal = ROLE_SYSTEM_LIST;
    else if (vw->GetNumChildren()) pvarRole->lVal =  ROLE_SYSTEM_GROUPING;
    else pvarRole->lVal=ROLE_SYSTEM_CLIENT;

    return S_OK;
  }

  STDMETHOD(get_accState)(THIS_ VARIANT varChild, VARIANT *pvarState) 
  {
    if (!m_br.vwnd || varChild.vt != VT_I4) 
    {
      pvarState->vt = VT_EMPTY;
      return E_INVALIDARG;
    }
    WDL_VWnd *vw = varChild.lVal == CHILDID_SELF ? m_br.vwnd : m_br.vwnd->EnumChildren(varChild.lVal-1);

    if (!vw) 
    {
      if (ISVWNDLIST(m_br.vwnd))
      {
        WDL_VirtualListBox *list=(WDL_VirtualListBox*)m_br.vwnd;
        if (list->m_GetItemInfo) 
        {
          int index = varChild.lVal-1 - m_br.vwnd->GetNumChildren();
          int c=list->m_GetItemInfo(list,-1,NULL,0,NULL,NULL);
          if (index>=0&&index<c)
          {
            pvarState->vt = VT_I4;
            pvarState->lVal = 0;
            RECT r;
            if (list->GetItemRect(index,&r))
            {
            }
            else 
              pvarState->lVal|=STATE_SYSTEM_INVISIBLE;

            return S_OK;
          }          
          else if (index==c||index==c+1)
          {
            pvarState->vt = VT_I4;
            pvarState->lVal = 0;
            if (!list->GetScrollButtonRect(index==c+1))
              pvarState->lVal |= STATE_SYSTEM_UNAVAILABLE;
            return S_OK;

          }
        }
      }

      pvarState->vt = VT_EMPTY;
      return E_INVALIDARG;
    }

    const char *type = vw->GetType();
    pvarState->vt = VT_I4;
    pvarState->lVal = 0;
    if (!vw->IsVisible()) pvarState->lVal |= STATE_SYSTEM_INVISIBLE;
    else
    {
      if (!vw->GetParent() && vw->GetRealParent() && GetFocus() == vw->GetRealParent())
        pvarState->lVal |= STATE_SYSTEM_FOCUSED;
    }

    if (!strcmp(type,"vwnd_iconbutton")) 
    {
      WDL_VirtualIconButton *vb = (WDL_VirtualIconButton*)vw;
      if (vb->GetIsButton())
      {
        if (vb->GetCheckState()>0) pvarState->lVal |= STATE_SYSTEM_CHECKED;
      }
    }

    return S_OK;
  }
  STDMETHOD(get_accHelp)(THIS_ VARIANT varChild, BSTR* pszHelp) 
  {
    *pszHelp=NULL;
    if (!m_br.vwnd || varChild.vt != VT_I4) 
    {
      return E_INVALIDARG;
    }
    WDL_VWnd *vw = varChild.lVal == CHILDID_SELF ? m_br.vwnd : m_br.vwnd->EnumChildren(varChild.lVal-1);

    if (!vw) 
    {
      return E_INVALIDARG;
    }
    return S_FALSE;
  }
  STDMETHOD(get_accHelpTopic)(THIS_ BSTR* pszHelpFile, VARIANT varChild, long* pidTopic)
  {
    return DISP_E_MEMBERNOTFOUND;
  }
  STDMETHOD(get_accKeyboardShortcut)(THIS_ VARIANT varChild, BSTR* pszKeyboardShortcut) 
  {
    *pszKeyboardShortcut=NULL;

    if (!m_br.vwnd || varChild.vt != VT_I4) 
    {
      return E_INVALIDARG;
    }
    WDL_VWnd *vw = varChild.lVal == CHILDID_SELF ? m_br.vwnd : m_br.vwnd->EnumChildren(varChild.lVal-1);

    if (!vw) 
    {
      return E_INVALIDARG;
    }
    return S_FALSE;
  }
  STDMETHOD(get_accFocus)(THIS_ VARIANT FAR * pvarFocusChild)
  {
    pvarFocusChild->vt= VT_EMPTY;
    if (!m_br.vwnd) 
    {
      return S_FALSE;
    }
    //return DISP_E_MEMBERNOTFOUND;

    if (!m_br.vwnd->GetParent() &&
        m_br.vwnd->GetRealParent() && 
        GetFocus()==m_br.vwnd->GetRealParent())
    {
      pvarFocusChild->vt=VT_I4;
      pvarFocusChild->lVal = CHILDID_SELF;
    }
    return S_OK;
  }
  STDMETHOD(get_accSelection)(THIS_ VARIANT FAR * pvarSelectedChildren) 
  {
    pvarSelectedChildren->vt= VT_EMPTY;
    if (!m_br.vwnd) 
    {
      return S_FALSE;
    }
    return S_OK;
  }
  STDMETHOD(get_accDefaultAction)(THIS_ VARIANT varChild, BSTR* pszDefaultAction) 
  {
    *pszDefaultAction=NULL;

    if (!m_br.vwnd || varChild.vt != VT_I4) 
    {
      return E_INVALIDARG;
    }
    WDL_VWnd *vw = varChild.lVal == CHILDID_SELF ? m_br.vwnd : m_br.vwnd->EnumChildren(varChild.lVal-1);

    if (!vw) return E_INVALIDARG;

    const char *type = vw->GetType();
    if (type)
    {
      const char *str=NULL;
      if (!strcmp(type,"vwnd_combobox")) str = "Activate";
      else if (!strcmp(type,"vwnd_iconbutton")) str="Click";
      else if (!strcmp(type,"vwnd_tabctrl_child")) str="Select";
      else if (!strcmp(type,"vwnd_statictext")) str="Click";
      if (str)
      {
        *pszDefaultAction = SysAllocStringUTF8(str);
        if (!*pszDefaultAction) return E_OUTOFMEMORY;
        return S_OK;
      }
    }

    return S_FALSE;
  }

  STDMETHOD(accSelect)(THIS_ long flagsSelect, VARIANT varChild) 
  {
    return S_FALSE;
  }
  STDMETHOD(accLocation)(THIS_ long* pxLeft, long* pyTop, long* pcxWidth, long* pcyHeight, VARIANT varChild) 
  {
    *pxLeft=*pyTop=*pcxWidth=*pcyHeight=0;
    if (!m_br.vwnd || varChild.vt != VT_I4) 
    {
      return E_INVALIDARG;
    }
    WDL_VWnd *vw = varChild.lVal == CHILDID_SELF ? m_br.vwnd : m_br.vwnd->EnumChildren(varChild.lVal-1);

    if (!vw && ISVWNDLIST(m_br.vwnd))
    {
      WDL_VirtualListBox *list=(WDL_VirtualListBox*)m_br.vwnd;
      if (list->m_GetItemInfo)
      {
        int idx = varChild.lVal-1 - m_br.vwnd->GetNumChildren();
        int ni = list->m_GetItemInfo(list,-1,NULL,0,NULL,NULL);
        if (idx>=0&&idx<ni)
        {
          RECT r2={0,0,0,0};
          if (list->GetItemRect(idx,&r2))
          {
            HWND h = list->GetRealParent();
            RECT r;
            list->GetPositionInTopVWnd(&r);
            ClientToScreen(h,(LPPOINT)&r);
            ClientToScreen(h,((LPPOINT)&r)+1);
            *pxLeft=r.left+r2.left;
            *pyTop=r.top+r2.top;
            *pcxWidth=r2.right-r2.left;
            *pcyHeight=r2.bottom-r2.top;
          }
          else *pxLeft=*pyTop=*pcxWidth=*pcyHeight=0; 
          return S_OK;
        }
        else if (idx==ni||idx==ni+1)
        {
          RECT *rr = list->GetScrollButtonRect(idx==ni+1);
          if (rr)
          {
            HWND h = list->GetRealParent();
            RECT r;
            list->GetPositionInTopVWnd(&r);
            ClientToScreen(h,(LPPOINT)&r);
            ClientToScreen(h,((LPPOINT)&r)+1);
            *pxLeft=r.left+rr->left;
            *pyTop=r.top+rr->top;
            *pcxWidth=rr->right-rr->left;
            *pcyHeight=rr->bottom-rr->top;
          }
          else *pxLeft=*pyTop=*pcxWidth=*pcyHeight=0; 
          return S_OK;

        }
      }
    }
    if (!vw) return E_INVALIDARG;

    HWND h = vw->GetRealParent();
    if (h)
    {
      RECT r;
      vw->GetPositionInTopVWnd(&r);
      ClientToScreen(h,(LPPOINT)&r);
      ClientToScreen(h,((LPPOINT)&r)+1);
      *pxLeft=r.left;
      *pyTop=r.top;
      *pcxWidth=r.right-r.left;
      *pcyHeight=r.bottom-r.top;
    }
    return S_OK;   
  }

  STDMETHOD(accNavigate)(THIS_ long navDir, VARIANT varStart, VARIANT * pvarEndUpAt) 
  {
    if (!pvarEndUpAt) return E_INVALIDARG;

    if (!m_br.vwnd || varStart.vt != VT_I4) 
    {
      return DISP_E_MEMBERNOTFOUND;
    }
    pvarEndUpAt->vt = VT_I4;
    pvarEndUpAt->lVal = VT_EMPTY;

    WDL_VWnd *vw = varStart.lVal == CHILDID_SELF ? m_br.vwnd : m_br.vwnd->EnumChildren(varStart.lVal-1);
    if (!vw) return S_FALSE;

    if (navDir == NAVDIR_FIRSTCHILD || navDir == NAVDIR_LASTCHILD)
    {
      int n = vw->GetNumChildren();
      if (ISVWNDLIST(vw))
      {
        WDL_VirtualListBox *list=(WDL_VirtualListBox*)m_br.vwnd;
        if (list->m_GetItemInfo) n += list->m_GetItemInfo(list,-1,NULL,0,NULL,NULL);
      }

      if (n<1) return S_FALSE;
      pvarEndUpAt->vt = VT_I4;
      pvarEndUpAt->lVal = navDir == NAVDIR_FIRSTCHILD ? 1 : n;
      return S_OK;
    }

    if (navDir == NAVDIR_NEXT || navDir == NAVDIR_PREVIOUS)
    {
      if (varStart.lVal != CHILDID_SELF)
      {
        int n = m_br.vwnd->GetNumChildren();
        if (ISVWNDLIST(m_br.vwnd))
        {
          WDL_VirtualListBox *list=(WDL_VirtualListBox*)m_br.vwnd;
          if (list->m_GetItemInfo) n += list->m_GetItemInfo(list,-1,NULL,0,NULL,NULL);
        }
        int x = varStart.lVal - 1;
        if (navDir == NAVDIR_NEXT)
        {
          if (++x >= n) return S_FALSE;
        }
        else
        {
          if (--x<0) return S_FALSE;
        }
        pvarEndUpAt->vt = VT_I4;
        pvarEndUpAt->lVal = 1 + x;
        return S_OK;
      }

      // passed CHILDID_SELF, need to scan to find index
      WDL_VWnd *par = vw->GetParent();
      if (par)
      {
        const int n = par->GetNumChildren();
        int x;
        for (x=0;x < n;x++) 
        {
          WDL_VWnd *c = par->EnumChildren(x);
          if (c == vw) 
          {
            if (navDir == NAVDIR_NEXT) x++;
            else x--;

            WDL_VWnd *hit = par->EnumChildren(x);
            if (!hit) break;

            IAccessible *pac = GetVWndIAccessible(hit);
            if (pac)
            {
              pvarEndUpAt->vt = VT_DISPATCH;
              pvarEndUpAt->pdispVal = (IDispatch *)pac;
            }
            else
            {
              pvarEndUpAt->vt = VT_I4;
              pvarEndUpAt->lVal = 1 + x;
            }
            return S_OK;
          }
        }
      }
      return S_FALSE;
    }

    return E_INVALIDARG;
  }
  STDMETHOD(accHitTest)(THIS_ long xLeft, long yTop, VARIANT * pvarChildAtPoint) 
  {
    pvarChildAtPoint->vt = VT_EMPTY;

    if (!m_br.vwnd) 
    {
      return E_INVALIDARG;
    }

    HWND h = m_br.vwnd->GetRealParent();
    if (!h) return S_FALSE;

    POINT p={xLeft,yTop};
    ScreenToClient(h,&p);
    if (!m_br.vwnd->GetParent() && __CreateStdAccessibleObject)
    {
      HWND hhit = ChildWindowFromPoint(h,p);
      if (hhit && hhit != h)
      {
        pvarChildAtPoint->pdispVal=0;
        HRESULT res = __CreateStdAccessibleObject(hhit,OBJID_CLIENT,IID_IAccessible,(void**)&pvarChildAtPoint->pdispVal);
        if (SUCCEEDED(res)) 
        {
          pvarChildAtPoint->vt = VT_DISPATCH;
          return S_OK;
        }
      }
    }

    RECT r;
    m_br.vwnd->GetPositionInTopVWnd(&r);
    if (!PtInRect(&r,p)) return S_FALSE;

    WDL_VWnd *vw = m_br.vwnd->VirtWndFromPoint(p.x-r.left,p.y-r.top,0);
    if (vw&&vw != m_br.vwnd)
    {
      IAccessible *pac = GetVWndIAccessible(vw);
      if (pac)
      {
        pvarChildAtPoint->vt = VT_DISPATCH;
        pvarChildAtPoint->pdispVal = pac;
        return S_OK;
      }
    }
    else if (ISVWNDLIST(m_br.vwnd))
    {
      WDL_VirtualListBox *list=(WDL_VirtualListBox*)m_br.vwnd;
      if (list->m_GetItemInfo)
      {
        int c = list->m_GetItemInfo(list,-1,NULL,0,NULL,NULL);
        if (c<0)c=0;
        int a= list->IndexFromPt(p.x-r.left,p.y-r.top);
        if (a>=0 && a<c)
        {
          pvarChildAtPoint->vt = VT_I4;
          pvarChildAtPoint->lVal = 1+list->GetNumChildren()+a;;
          return S_OK;
        }
        else
        {
          POINT pp = { p.x-r.left, p.y-r.top};
          int x;
          for(x=0;x<2;x++)
          {
            RECT *rr = list->GetScrollButtonRect(!!x);
            if (rr && PtInRect(rr,pp))
            {
              pvarChildAtPoint->vt = VT_I4;
              pvarChildAtPoint->lVal = 1+list->GetNumChildren()+c+x;;
              return S_OK;
            }
          }
        }
      }
    }

    pvarChildAtPoint->vt = VT_I4;
    pvarChildAtPoint->lVal = CHILDID_SELF;
    return S_OK;
  }
  STDMETHOD(accDoDefaultAction)(THIS_ VARIANT varChild) 
  {
    if (!m_br.vwnd || varChild.vt != VT_I4) 
    {
      return E_INVALIDARG;
    }
    WDL_VWnd *vw = varChild.lVal == CHILDID_SELF ? m_br.vwnd : m_br.vwnd->EnumChildren(varChild.lVal-1);

    if (!vw) return E_INVALIDARG;

    const char *type = vw->GetType();
    if (type)
    {
      if (!strcmp(type,"vwnd_combobox") ||
          !strcmp(type,"vwnd_iconbutton") ||
          !strcmp(type,"vwnd_tabctrl_child"))
      {
        vw->OnMouseDown(0,0);
        vw->OnMouseUp(0,0);      
        return S_OK;
      }
      else if (!strcmp(type,"vwnd_statictext"))
      {
        vw->OnMouseDblClick(0,0);
        return S_OK;
      }
    }

    return S_FALSE;
  }

    STDMETHOD(put_accName)(THIS_ VARIANT varChild, BSTR szName) 
    {
      return E_NOTIMPL;
    }

    STDMETHOD(put_accValue)(THIS_ VARIANT varChild, BSTR pszValue) 
    {
      return E_NOTIMPL;
    }

  VWndBridge m_br;
  LONG m_refCnt;

  CVWndAccessible *_freelist_next;

};


static IAccessible *GetVWndIAccessible(WDL_VWnd *vwnd)
{
  if (!vwnd) return 0;

  WDL_VWnd_IAccessibleBridge *br = vwnd->GetAccessibilityBridge();
  if (!br)
  {
    CVWndAccessible *acc;
    if (g_freelist_acc)
    {
      g_freelist_acc_size--;
      acc=g_freelist_acc;
      g_freelist_acc=acc->_freelist_next;

      acc->m_br.vwnd = vwnd;
      acc->m_refCnt = 1;
    }
    else 
      acc = new CVWndAccessible(vwnd);

    br = &acc->m_br;
    vwnd->SetAccessibilityBridge(br);
  }
  else ((VWndBridge*)br)->par->AddRef();

  return ((VWndBridge*)br)->par;
}


LRESULT WDL_AccessibilityHandleForVWnd(bool isDialog, HWND hwnd, WDL_VWnd *vw, WPARAM wParam, LPARAM lParam)
{
  if (vw)
  {
    if ((DWORD)lParam != (DWORD)OBJID_CLIENT) return 0;

    static LRESULT (WINAPI *__LresultFromObject)(REFIID riid, WPARAM, LPUNKNOWN);

    static int init;
    if (!init)
    {
      init=1;
      HINSTANCE hInst = LoadLibrary("oleacc.dll");
      if (hInst)
      {
        *(FARPROC *)&__LresultFromObject = GetProcAddress(hInst,"LresultFromObject");
        *(FARPROC *)&__CreateStdAccessibleObject = GetProcAddress(hInst,"CreateStdAccessibleObject");
      }
    }
    if (!__LresultFromObject) return 0;

    IAccessible *ac = GetVWndIAccessible(vw);
    if (!ac) return 0;


    LRESULT res = __LresultFromObject(IID_IAccessible,wParam,ac); // lresultfromobject retains?
    ac->Release();
    if (isDialog)
    {
      SetWindowLongPtr(hwnd,DWLP_MSGRESULT,res);
      return 1;
    }
    return res;
  }
  return 0;
}

#else

#ifdef _WIN32
#include <windows.h>
#else
#include "../swell/swell.h"
#endif 

#include "virtwnd.h"

#ifdef __APPLE__

// see virtwnd-nsaccessibility.mm

#else

LRESULT WDL_AccessibilityHandleForVWnd(bool isDialog, HWND hwnd, WDL_VWnd *vw, WPARAM wParam, LPARAM lParam)
{
  return 0;
}

#endif

#endif
