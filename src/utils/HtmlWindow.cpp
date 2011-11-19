/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#include "HtmlWindow.h"

#include <mshtml.h>
#include <mshtmhst.h>
#include <oaidl.h>
#include <exdispid.h>

#include "StrUtil.h"
#include "GeomUtil.h"
#include "WinUtil.h"
#include "Scopes.h"

//TODO: not working yet
//#define HOOK_BROWSER_CONTROL 1

// Info about implementing web browser control
// http://www.codeproject.com/KB/COM/cwebpage.aspx

// The code is structured in a similar way as wxWindows'
// browser wrapper
// http://codesearch.google.com/#cbxlbgWFJ4U/wxCode/components/iehtmlwin/src/IEHtmlWin.h
// http://codesearch.google.com/#cbxlbgWFJ4U/wxCode/components/iehtmlwin/src/IEHtmlWin.cpp

// Another code to get inspired: http://code.google.com/p/fidolook/source/browse/trunk/Qm/ui/messageviewwindow.cpp

// Implementing scrolling:
// Currently we implement scrolling by sending messages simulating user input
// to the browser control window that is responsible for processing those messages.
// It has a benefit of being simple to implement and matching ie's behavior closely.

// Another option would be to provide scrolling functions to be called by callers
// (e.g. from FrameOnKeydow()) by querying scroll state from IHTMLElement2 and setting
// a new scroll state http://www.codeproject.com/KB/miscctrl/scrollbrowser.aspx
// or using scrollTo() or scrollBy() on IHTMLWindow2:
// http://msdn.microsoft.com/en-us/library/aa741497(v=VS.85).aspx

class HW_IOleInPlaceFrame;
class HW_IOleInPlaceSiteWindowless;
class HW_IOleClientSite;
class HW_IOleControlSite;
class HW_IOleCommandTarget;
class HW_IOleItemContainer;
class HW_DWebBrowserEvents2;
class HW_IAdviseSink2;

inline void VariantSetBool(VARIANT *res, bool val)
{
    res->vt = VT_BOOL;
    res->boolVal = val ? VARIANT_TRUE : VARIANT_FALSE;;
}

inline void VariantSetLong(VARIANT *res, long val)
{
    res->vt = VT_I4;
    res->lVal = val;
}

// HW stands for HtmlWindow
class FrameSite : public IUnknown
{
    friend class HtmlWindow;
    friend class HW_IOleInPlaceFrame;
    friend class HW_IOleInPlaceSiteWindowless;
    friend class HW_IOleClientSite;
    friend class HW_IOleControlSite;
    friend class HW_IOleCommandTarget;
    friend class HW_IOleItemContainer;
    friend class HW_DWebBrowserEvents2;
    friend class HW_IAdviseSink2;

public:
    FrameSite(HtmlWindow * win);
    ~FrameSite();

    //IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

protected:
    int m_cRef;

    HW_IOleInPlaceFrame *           oleInPlaceFrame;
    HW_IOleInPlaceSiteWindowless *  oleInPlaceSiteWindowless;
    HW_IOleClientSite *             oleClientSite;
    HW_IOleControlSite *            oleControlSite;
    HW_IOleCommandTarget *          oleCommandTarget;
    HW_IOleItemContainer *          oleItemContainer;
    HW_DWebBrowserEvents2 *         hwDWebBrowserEvents2;
    HW_IAdviseSink2 *               adviseSink2;

    HtmlWindow * htmlWindow;

    //HDC m_hDCBuffer;
    HWND hwndParent;

    bool supportsWindowlessActivation;
    bool inPlaceLocked;
    bool inPlaceActive;
    bool uiActive;
    bool isWindowless;

    LCID        ambientLocale;
    COLORREF    ambientForeColor;
    COLORREF    ambientBackColor;
    bool        ambientShowHatching;
    bool        ambientShowGrabHandles;
    bool        ambientUserMode;
    bool        ambientAppearance;
};

class HW_IOleInPlaceFrame : public IOleInPlaceFrame
{
public:
    HW_IOleInPlaceFrame(FrameSite* fs) : fs(fs)
    {
    }
    ~HW_IOleInPlaceFrame() {}

    //IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }
    //IOleWindow
    STDMETHODIMP GetWindow(HWND*);
    STDMETHODIMP ContextSensitiveHelp(BOOL) { return S_OK; }
    //IOleInPlaceUIWindow
    STDMETHODIMP GetBorder(LPRECT);
    STDMETHODIMP RequestBorderSpace(LPCBORDERWIDTHS);
    STDMETHODIMP SetBorderSpace(LPCBORDERWIDTHS) { return S_OK; }
    STDMETHODIMP SetActiveObject(IOleInPlaceActiveObject*, LPCOLESTR) { return S_OK; }
    //IOleInPlaceFrame
    STDMETHODIMP InsertMenus(HMENU, LPOLEMENUGROUPWIDTHS) { return S_OK; }
    STDMETHODIMP SetMenu(HMENU, HOLEMENU, HWND) { return S_OK; }
    STDMETHODIMP RemoveMenus(HMENU) { return S_OK; }
    STDMETHODIMP SetStatusText(LPCOLESTR) { return S_OK; }
    STDMETHODIMP EnableModeless(BOOL) { return S_OK; }
    STDMETHODIMP TranslateAccelerator(LPMSG, WORD) { return E_NOTIMPL; }
protected:
    FrameSite * fs;
};

class HW_IOleInPlaceSiteWindowless : public IOleInPlaceSiteWindowless
{
public:
    HW_IOleInPlaceSiteWindowless(FrameSite* fs) : fs(fs) { }
    ~HW_IOleInPlaceSiteWindowless() {}

    //IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }
    //IOleWindow
    STDMETHODIMP GetWindow(HWND* h)
    { return fs->oleInPlaceFrame->GetWindow(h); }
    STDMETHODIMP ContextSensitiveHelp(BOOL b)
    { return fs->oleInPlaceFrame->ContextSensitiveHelp(b); }
    //IOleInPlaceSite
    STDMETHODIMP CanInPlaceActivate() { return S_OK; }
    STDMETHODIMP OnInPlaceActivate();
    STDMETHODIMP OnUIActivate();
    STDMETHODIMP GetWindowContext(IOleInPlaceFrame**, IOleInPlaceUIWindow**,
            LPRECT, LPRECT, LPOLEINPLACEFRAMEINFO);
    STDMETHODIMP Scroll(SIZE) { return S_OK; }
    STDMETHODIMP OnUIDeactivate(BOOL);
    STDMETHODIMP OnInPlaceDeactivate();
    STDMETHODIMP DiscardUndoState() { return S_OK; }
    STDMETHODIMP DeactivateAndUndo() { return S_OK; }
    STDMETHODIMP OnPosRectChange(LPCRECT) { return S_OK; }
    //IOleInPlaceSiteEx
    STDMETHODIMP OnInPlaceActivateEx(BOOL*, DWORD);
    STDMETHODIMP OnInPlaceDeactivateEx(BOOL) { return S_OK; }
    STDMETHODIMP RequestUIActivate() { return S_FALSE; }
    //IOleInPlaceSiteWindowless
    STDMETHODIMP CanWindowlessActivate();
    STDMETHODIMP GetCapture() { return S_FALSE; }
    STDMETHODIMP SetCapture(BOOL) { return S_FALSE; }
    STDMETHODIMP GetFocus() { return S_OK; }
    STDMETHODIMP SetFocus(BOOL) { return S_OK; }
    STDMETHODIMP GetDC(LPCRECT, DWORD, HDC*);
    STDMETHODIMP ReleaseDC(HDC) { return E_NOTIMPL; }
    STDMETHODIMP InvalidateRect(LPCRECT, BOOL);
    STDMETHODIMP InvalidateRgn(HRGN, BOOL) { return E_NOTIMPL; }
    STDMETHODIMP ScrollRect(INT, INT, LPCRECT, LPCRECT) { return E_NOTIMPL; }
    STDMETHODIMP AdjustRect(LPRECT) { return E_NOTIMPL; }
    STDMETHODIMP OnDefWindowMessage(UINT, WPARAM, LPARAM, LRESULT*) { return E_NOTIMPL; }
protected:
    FrameSite *fs;
};

class HW_IOleClientSite : public IOleClientSite
{
public:
    HW_IOleClientSite(FrameSite* fs) : fs(fs) { }
    ~HW_IOleClientSite() {}

    //IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }
    //IOleClientSite
    STDMETHODIMP SaveObject() { return S_OK; }
    STDMETHODIMP GetMoniker(DWORD, DWORD, IMoniker**) { return E_NOTIMPL; }
    STDMETHODIMP GetContainer(LPOLECONTAINER FAR*);
    STDMETHODIMP ShowObject() { return S_OK; }
    STDMETHODIMP OnShowWindow(BOOL) { return S_OK; }
    STDMETHODIMP RequestNewObjectLayout() { return E_NOTIMPL; }
protected:
    FrameSite * fs;
};

class HW_IOleControlSite : public IOleControlSite
{
public:
    HW_IOleControlSite(FrameSite* fs) : fs(fs) { }
    ~HW_IOleControlSite() {}

    //IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }
    //IOleControlSite
    STDMETHODIMP OnControlInfoChanged() { return S_OK; }
    STDMETHODIMP LockInPlaceActive(BOOL);
    STDMETHODIMP GetExtendedControl(IDispatch**) { return E_NOTIMPL; }
    STDMETHODIMP TransformCoords(POINTL*, POINTF*, DWORD);
    STDMETHODIMP TranslateAccelerator(LPMSG, DWORD) { return E_NOTIMPL; }
    STDMETHODIMP OnFocus(BOOL) { return S_OK; }
    STDMETHODIMP ShowPropertyFrame() { return E_NOTIMPL; }
protected:
    FrameSite * fs;
};

class HW_IOleCommandTarget : public IOleCommandTarget
{
public:
    HW_IOleCommandTarget(FrameSite* fs) : fs(fs) { }
    ~HW_IOleCommandTarget() {}

    //IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }
    //IOleCommandTarget
    STDMETHODIMP QueryStatus(const GUID*, ULONG, OLECMD[], OLECMDTEXT*);
    STDMETHODIMP Exec(const GUID*, DWORD, DWORD, VARIANTARG*, VARIANTARG*) { return OLECMDERR_E_NOTSUPPORTED; }
protected:
    FrameSite * fs;
};

class HW_IOleItemContainer : public IOleItemContainer
{
public:
    HW_IOleItemContainer(FrameSite* fs) : fs(fs) { }
    ~HW_IOleItemContainer() {}

    //IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }
    //IParseDisplayName
    STDMETHODIMP ParseDisplayName(IBindCtx*, LPOLESTR, ULONG*, IMoniker**) { return E_NOTIMPL; }
    //IOleContainer
    STDMETHODIMP EnumObjects(DWORD, IEnumUnknown**) { return E_NOTIMPL; }
    STDMETHODIMP LockContainer(BOOL) { return S_OK; }
    //IOleItemContainer
    STDMETHODIMP GetObject(LPOLESTR, DWORD, IBindCtx*, REFIID, void**);
    STDMETHODIMP GetObjectStorage(LPOLESTR, IBindCtx*, REFIID, void**);
    STDMETHODIMP IsRunning(LPOLESTR);
protected:
    FrameSite * fs;
};

class HW_DWebBrowserEvents2 : public DWebBrowserEvents2
{
    FrameSite * fs;

    HRESULT DispatchPropGet(DISPID dispIdMember, VARIANT *res);

public:
    HW_DWebBrowserEvents2(FrameSite* fs) : fs(fs) { }
    ~HW_DWebBrowserEvents2() {}

    //IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }
    //IDispatch
    STDMETHODIMP GetIDsOfNames(REFIID, OLECHAR**, unsigned int, LCID, DISPID*) { return E_NOTIMPL; }
    STDMETHODIMP GetTypeInfo(unsigned int, LCID, ITypeInfo**) { return E_NOTIMPL; }
    STDMETHODIMP GetTypeInfoCount(unsigned int*) { return E_NOTIMPL; }
    STDMETHODIMP Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*);
};

class HW_IAdviseSink2 : public IAdviseSink2, public IAdviseSinkEx
{
    FrameSite * fs;

public:
    HW_IAdviseSink2(FrameSite* fs) : fs(fs) { }
    ~HW_IAdviseSink2() {}

    //IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }
    //IAdviseSink
    void STDMETHODCALLTYPE OnDataChange(FORMATETC*, STGMEDIUM*) { }
    void STDMETHODCALLTYPE OnViewChange(DWORD, LONG) {
        // redraw the control
        fs->oleInPlaceSiteWindowless->InvalidateRect(NULL, FALSE);
    }
    void STDMETHODCALLTYPE OnRename(IMoniker*) { }
    void STDMETHODCALLTYPE OnSave() { }
    void STDMETHODCALLTYPE OnClose() { }
    //IAdviseSink2
    void STDMETHODCALLTYPE OnLinkSrcChange(IMoniker*) { }
    //IAdviseSinkEx
    void STDMETHODCALLTYPE OnViewStatusChange(DWORD) { }
};

static HWND GetBrowserControlHwnd(HWND hwndControlParent)
{
    // This is a fragile way to get the actual hwnd of the browser control
    // that is responsible for processing keyboard messages (I believe the
    // hierarchy might change depending on how the browser control is configured
    // e.g. if it has status window etc.).
    // But it works for us.
    HWND w1 = GetWindow(hwndControlParent, GW_CHILD);
    HWND w2 = GetWindow(w1, GW_CHILD);
    HWND w3 = GetWindow(w2, GW_CHILD);
    return w3;
}

// WndProc of the window that is a parent hwnd of embedded browser control.
static LRESULT CALLBACK WndProcParent(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HtmlWindow *win = (HtmlWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!win)
        return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_SIZE:
            if (SIZE_MINIMIZED != wParam) {
                int dx = LOWORD(lParam);
                int dy = HIWORD(lParam);
                win->OnSize(dx, dy);
                return 0;
            }
            break;

        // Note: not quite sure why I need this but if we don't swallow WM_MOUSEWHEEL
        // messages, we might get infinite recursion.
        case WM_MOUSEWHEEL:
            return 0;

        case WM_PARENTNOTIFY:
            if (LOWORD(wParam) == WM_LBUTTONDOWN)
                win->OnLButtonDown();
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// WndProc of the embedded web browser control.
static LRESULT CALLBACK WndProcBrowser(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HtmlWindow *win = (HtmlWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!win)
        return DefWindowProc(hwnd, msg, wParam, lParam);
    if (msg == WM_SETFOCUS) {
        win->OnLButtonDown();
    }
    if (win->wndProcBrowserPrev) {
        // TODO: why do we crash here? (if HOOK_BROWSER_CONTROL is defined)
        return CallWindowProc(win->wndProcBrowserPrev, hwnd, msg, wParam, lParam);
    }
    else
        return DefWindowProc(hwnd, msg, wParam, lParam);    
}

void HtmlWindow::SubclassHwnd()
{
    SetWindowLongPtr(hwndParent, GWLP_WNDPROC, (LONG_PTR)WndProcParent);
    SetWindowLongPtr(hwndParent, GWLP_USERDATA, (LONG_PTR)this);
#ifdef HOOK_BROWSER_CONTROL
    HWND hwndBrowser = GetBrowserControlHwnd(hwndParent);
    wndProcBrowserPrev = (WNDPROC)SetWindowLongPtr(hwndBrowser, GWLP_WNDPROC, (LONG_PTR)WndProcBrowser);
    SetWindowLongPtr(hwndBrowser, GWLP_USERDATA, (LONG_PTR)this);
#endif
}

void HtmlWindow::UnsubclassHwnd()
{
    SetWindowLongPtr(hwndParent, GWLP_USERDATA, (LONG_PTR)0);
#ifdef HOOK_BROWSER_CONTROL
    HWND hwndBrowser = GetBrowserControlHwnd(hwndParent);
    SetWindowLongPtr(hwndBrowser, GWLP_USERDATA, (LONG_PTR)0);
#endif
}

HtmlWindow::HtmlWindow(HWND hwndParent, HtmlWindowCallback *cb) :
    hwndParent(hwndParent), webBrowser(NULL), oleObject(NULL),
    oleInPlaceObject(NULL), viewObject(NULL),
    connectionPoint(NULL), oleObjectHwnd(NULL),
    adviseCookie(0), htmlWinCb(cb),
    wndProcBrowserPrev(NULL),
    canGoBack(false), canGoForward(false)
{
    assert(hwndParent);
    CreateBrowser();
}

void HtmlWindow::CreateBrowser()
{
    IUnknown *p;
    HRESULT hr = CoCreateInstance(CLSID_WebBrowser, NULL,
                    CLSCTX_ALL, IID_IUnknown, (void**)&p);
    assert(SUCCEEDED(hr));
    hr = p->QueryInterface(IID_IViewObject, (void**)&viewObject);
    assert(SUCCEEDED(hr));
    hr = p->QueryInterface(IID_IOleObject, (void**)&oleObject);
    assert(SUCCEEDED(hr));

    FrameSite *fs = new FrameSite(this);

    DWORD status;
    oleObject->GetMiscStatus(DVASPECT_CONTENT, &status);
    bool setClientSiteFirst = 0 != (status & OLEMISC_SETCLIENTSITEFIRST);
    bool invisibleAtRuntime = 0 != (status & OLEMISC_INVISIBLEATRUNTIME);

    if (setClientSiteFirst)
        oleObject->SetClientSite(fs->oleClientSite);

    IPersistStreamInit * psInit = NULL;
    hr = p->QueryInterface(IID_IPersistStreamInit, (void**)&psInit);
    if (SUCCEEDED(hr) && psInit != NULL) {
        hr = psInit->InitNew();
        assert(SUCCEEDED(hr));
    }

    hr = p->QueryInterface(IID_IOleInPlaceObject, (void**)&oleInPlaceObject);
    assert(SUCCEEDED(hr));

    hr = oleInPlaceObject->GetWindow(&oleObjectHwnd);
    assert(SUCCEEDED(hr));

    ::SetActiveWindow(oleObjectHwnd);
    RECT rc = ClientRect(hwndParent).ToRECT();

    oleInPlaceObject->SetObjectRects(&rc, &rc);
    if (!invisibleAtRuntime) {
        hr = oleObject->DoVerb(OLEIVERB_INPLACEACTIVATE, NULL,
                fs->oleClientSite, 0, hwndParent, &rc);
#if 0 // is this necessary?
        hr = oleObject->DoVerb(OLEIVERB_SHOW, 0, fs->oleClientSite, 0,
                hwnd, &rc);
#endif
    }

    if (!setClientSiteFirst)
        oleObject->SetClientSite(fs->oleClientSite);

    hr = p->QueryInterface(IID_IWebBrowser2, (void**)&webBrowser);
    assert(SUCCEEDED(hr));

    IConnectionPointContainer *cpContainer;
    hr = p->QueryInterface(IID_IConnectionPointContainer, (void**)&cpContainer);
    assert(SUCCEEDED(hr));
    hr = cpContainer->FindConnectionPoint(DIID_DWebBrowserEvents2, &connectionPoint);
    assert(SUCCEEDED(hr));
    connectionPoint->Advise(fs->hwDWebBrowserEvents2, &adviseCookie);
    cpContainer->Release();
    fs->Release();

    // TODO: disallow accessing any random url?
    //webBrowser->put_Offline(VARIANT_TRUE);

    webBrowser->put_MenuBar(VARIANT_FALSE);
    webBrowser->put_AddressBar(VARIANT_FALSE);
    webBrowser->put_StatusBar(VARIANT_FALSE);
    webBrowser->put_ToolBar(VARIANT_FALSE);

    webBrowser->put_RegisterAsBrowser(VARIANT_TRUE);
    webBrowser->put_RegisterAsDropTarget(VARIANT_TRUE);

    EnsureAboutBlankShown();
    SubclassHwnd();
}

HtmlWindow::~HtmlWindow()
{
    UnsubclassHwnd();
    if (oleInPlaceObject) {
        oleInPlaceObject->InPlaceDeactivate();
        oleInPlaceObject->UIDeactivate();
        oleInPlaceObject->Release();
    }
    if (connectionPoint) {
        connectionPoint->Unadvise(adviseCookie);
        connectionPoint->Release();
    }
    if (oleObject) {
        oleObject->Close(OLECLOSE_NOSAVE);
        oleObject->SetClientSite(NULL);
        oleObject->Release();
    }
    if (viewObject) {
        viewObject->Release();
    }
    if (webBrowser) {
        webBrowser->Release();
    }
}

void HtmlWindow::OnSize(int dx, int dy)
{
    if (webBrowser) {
        webBrowser->put_Width(dx);
        webBrowser->put_Height(dy);
    }

    RECT r = { 0, 0, dx, dy };
    if (oleInPlaceObject)
        oleInPlaceObject->SetObjectRects(&r, &r);
}

void HtmlWindow::OnLButtonDown() const
{
    if (htmlWinCb)
        htmlWinCb->OnLButtonDown();
}

void HtmlWindow::SetVisible(bool visible)
{
    if (visible)
        ShowWindow(hwndParent, SW_SHOW);
    else
        ShowWindow(hwndParent, SW_HIDE);
    if (webBrowser)
        webBrowser->put_Visible(visible ? TRUE : FALSE);
}

void HtmlWindow::NavigateToUrl(const TCHAR *urlStr)
{
    VARIANT url;
    VariantInit(&url);
    url.vt = VT_BSTR;
#ifdef UNICODE
    url.bstrVal = SysAllocString(urlStr);
#else
    url.bstrVal = SysAllocString(ScopedMem<WCHAR>(str::conv::ToWStr(urlStr)));
#endif
    if (!url.bstrVal)
        return;
    currentURL.Set(NULL);
    webBrowser->Navigate2(&url, 0, 0, 0, 0);
    VariantClear(&url);
}

void HtmlWindow::GoBack()
{
    if (webBrowser)
        webBrowser->GoBack();
}

void HtmlWindow::GoForward()
{
    if (webBrowser)
        webBrowser->GoForward();
}

int HtmlWindow::GetZoomPercent()
{
    VARIANT vtOut;
    HRESULT hr = webBrowser->ExecWB(OLECMDID_OPTICAL_ZOOM, OLECMDEXECOPT_DONTPROMPTUSER,
                                    NULL, &vtOut);
   if (FAILED(hr))
       return 100;
    return vtOut.lVal;
}

void HtmlWindow::SetZoomPercent(int zoom)
{
    VARIANT vtIn, vtOut;
    VariantSetLong(&vtIn, zoom);
    webBrowser->ExecWB(OLECMDID_OPTICAL_ZOOM, OLECMDEXECOPT_DONTPROMPTUSER,
                       &vtIn, &vtOut);
}

void HtmlWindow::PrintCurrentPage()
{
    webBrowser->ExecWB(OLECMDID_PRINT, OLECMDEXECOPT_PROMPTUSER, NULL, NULL);
}

void HtmlWindow::FindInCurrentPage()
{
    webBrowser->ExecWB(OLECMDID_FIND, OLECMDEXECOPT_PROMPTUSER, NULL, NULL);
}

void HtmlWindow::SelectAll()
{
    webBrowser->ExecWB(OLECMDID_SELECTALL, OLECMDEXECOPT_DODEFAULT, NULL, NULL);
}

void HtmlWindow::CopySelection()
{
    webBrowser->ExecWB(OLECMDID_COPY, OLECMDEXECOPT_DODEFAULT, NULL, NULL);
}

void HtmlWindow::EnsureAboutBlankShown()
{
    NavigateToUrl(_T("about:blank"));
    WaitUntilLoaded(INFINITE, _T("about:blank"));
}

void HtmlWindow::DisplayHtml(const TCHAR *html)
{
    // don't know why, but that's what other people do
    EnsureAboutBlankShown();

    ScopedComPtr<IDispatch> docDispatch;
    HRESULT hr = webBrowser->get_Document(&docDispatch);
    if (FAILED(hr))
        return;

    ScopedComQIPtr<IHTMLDocument2> doc(docDispatch);
    if (!doc)
        return;

    SAFEARRAY *arr = SafeArrayCreateVector(VT_VARIANT, 0, 1);
    if (!arr)
        return;

    VARIANT *var = NULL;
    hr = SafeArrayAccessData(arr, (void**)&var);
    if (FAILED(hr))
        goto Exit;
    var->vt = VT_BSTR;
#ifdef UNICODE
    var->bstrVal = SysAllocString(html);
#else
    var->bstrVal = SysAllocString(ScopedMem<WCHAR>(str::conv::ToWStr(html)));
#endif
    if (!var->bstrVal)
        goto Exit;
    SafeArrayUnaccessData(arr);

    doc->write(arr);
    doc->close();

Exit:
    SafeArrayDestroy(arr);
}

// Take a screenshot of a given <area> inside an html window and resize
// it to <finalSize>. It's up to the caller to make sure <area> fits
// within window (we don't check that's the case)
HBITMAP HtmlWindow::TakeScreenshot(RectI area, SizeI finalSize)
{
    using namespace Gdiplus;

    ScopedComPtr<IDispatch> docDispatch;
    HRESULT hr = webBrowser->get_Document(&docDispatch);
    if (FAILED(hr) || !docDispatch)
        return NULL;
    ScopedComQIPtr<IViewObject2> view(docDispatch);
    if (!view)
        return NULL;

    // capture the whole window (including scrollbars)
    // to image and create imageRes containing the area
    // user asked for
    WindowRect winRc(hwndParent);
    Bitmap image(winRc.dx, winRc.dy, PixelFormat24bppRGB);
    Graphics g(&image);

    HDC dc = g.GetHDC();
    RECTL rc = { 0, 0, winRc.dx, winRc.dy };
    hr = view->Draw(DVASPECT_CONTENT, -1, NULL, NULL, dc, dc, &rc, NULL, NULL, 0);
    g.ReleaseHDC(dc);
    if (FAILED(hr))
        return NULL;

    Bitmap imageRes(finalSize.dx, finalSize.dy, PixelFormat24bppRGB);
    Graphics g2(&imageRes);
    g2.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g2.DrawImage(&image, Gdiplus::Rect(0, 0, finalSize.dx, finalSize.dy),
                 area.x, area.y, area.dx, area.dy, UnitPixel);

    HBITMAP hbmp;
    Status ok = imageRes.GetHBITMAP(Color::White, &hbmp);
    if (ok != Ok)
        return NULL;
    return hbmp;
}

// called before an url is shown. If returns false, will cancel
// the navigation.
bool HtmlWindow::OnBeforeNavigate(const TCHAR *url, bool newWindow)
{
    currentURL.Set(NULL);
    if (htmlWinCb)
        return htmlWinCb->OnBeforeNavigate(url, newWindow);
    return true;
}

void HtmlWindow::OnDocumentComplete(const TCHAR *url)
{
    currentURL.Set(str::Dup(url));
}

static void PumpRemainingMessages()
{
    MSG msg;
    for (;;) {
        bool moreMessages = PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);
        if (!moreMessages)
            return;
        GetMessage(&msg, NULL, 0, 0);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void HtmlWindow::SendMsg(UINT msg, WPARAM wp, LPARAM lp)
{
    HWND hwndBrowser = GetBrowserControlHwnd(hwndParent);
    SendMessage(hwndBrowser, msg, wp, lp);
}

static bool LoadedExpectedPage(const TCHAR *expectedUrl, const TCHAR *loadedUrl)
{
    if (!loadedUrl)
        return false;
    if (!expectedUrl)
        return true;
    return str::Eq(expectedUrl, loadedUrl);
}

bool HtmlWindow::WaitUntilLoaded(DWORD maxWaitMs, const TCHAR *url)
{
    MillisecondTimer timer(true);
    // in some cases (like reading chm from network drive without the right permissions)
    // web control might navigate to about:blank instead of the url we asked for, so
    // we stop when navigation is finished but only consider it successful if
    // we navigated to the url we asked for
    // TODO: we have a race here: if user chooses e.g. to close the document while we're
    // here, we'll close the ChmEngine etc. and try to use it after we exit.
    while ((currentURL.Get() == NULL) && (timer.GetCurrTimeInMs() < maxWaitMs)) {
        PumpRemainingMessages();
        Sleep(100);
    }
    return LoadedExpectedPage(url, currentURL.Get());
}

FrameSite::FrameSite(HtmlWindow * win)
{
    m_cRef = 1;

    htmlWindow = win;
    supportsWindowlessActivation = true;
    inPlaceLocked = false;
    uiActive = false;
    inPlaceActive = false;
    isWindowless = false;

    ambientLocale = 0;
    ambientForeColor = ::GetSysColor(COLOR_WINDOWTEXT);
    ambientBackColor = ::GetSysColor(COLOR_WINDOW);
    ambientUserMode = true;
    ambientShowHatching = true;
    ambientShowGrabHandles = true;
    ambientAppearance = true;

    //m_hDCBuffer = NULL;
    hwndParent = htmlWindow->hwndParent;

    oleInPlaceFrame = new HW_IOleInPlaceFrame(this);
    oleInPlaceSiteWindowless = new HW_IOleInPlaceSiteWindowless(this);
    oleClientSite = new HW_IOleClientSite(this);
    oleControlSite = new HW_IOleControlSite(this);
    oleCommandTarget = new HW_IOleCommandTarget(this);
    oleItemContainer = new HW_IOleItemContainer(this);
    hwDWebBrowserEvents2 = new HW_DWebBrowserEvents2(this);
    adviseSink2 = new HW_IAdviseSink2(this);
}

FrameSite::~FrameSite()
{
    delete adviseSink2;
    delete hwDWebBrowserEvents2;
    delete oleItemContainer;
    delete oleCommandTarget;
    delete oleControlSite;
    delete oleClientSite;
    delete oleInPlaceSiteWindowless;
    delete oleInPlaceFrame;
}

//IUnknown
STDMETHODIMP FrameSite::QueryInterface(REFIID riid, void **ppv)
{
    if (ppv == NULL)
        return E_INVALIDARG;

    *ppv = NULL;
    if (riid == IID_IUnknown)
        *ppv = this;
    else if (riid == IID_IOleWindow ||
        riid == IID_IOleInPlaceUIWindow ||
        riid == IID_IOleInPlaceFrame)
        *ppv = oleInPlaceFrame;
    else if (riid == IID_IOleInPlaceSite ||
        riid == IID_IOleInPlaceSiteEx ||
        riid == IID_IOleInPlaceSiteWindowless)
        *ppv = oleInPlaceSiteWindowless;
    else if (riid == IID_IOleClientSite)
        *ppv = oleClientSite;
    else if (riid == IID_IOleControlSite)
        *ppv = oleControlSite;
    else if (riid == IID_IOleCommandTarget)
        *ppv = oleCommandTarget;
    else if (riid == IID_IOleItemContainer ||
        riid == IID_IOleContainer ||
        riid == IID_IParseDisplayName)
        *ppv = oleItemContainer;
    else if (riid == IID_IDispatch ||
        riid == DIID_DWebBrowserEvents2)
        *ppv = hwDWebBrowserEvents2;
    else if (riid == IID_IAdviseSink ||
        riid == IID_IAdviseSink2 ||
        riid == IID_IAdviseSinkEx)
        *ppv = adviseSink2;

    if (*ppv == NULL)
        return E_NOINTERFACE;
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) FrameSite::AddRef()
{
    return ++m_cRef;
}

STDMETHODIMP_(ULONG) FrameSite::Release()
{
    assert(m_cRef > 0);
    if (--m_cRef == 0) {
        delete this;
        return 0;
    } else
        return m_cRef;
}

//IDispatch
HRESULT HW_DWebBrowserEvents2::DispatchPropGet(DISPID dispIdMember, VARIANT *res)
{
    if (res == NULL)
        return E_INVALIDARG;

    switch (dispIdMember)
    {
        case DISPID_AMBIENT_APPEARANCE:
            VariantSetBool(res, fs->ambientAppearance);
            break;

        case DISPID_AMBIENT_FORECOLOR:
            VariantSetLong(res, (long)fs->ambientForeColor);
            break;

        case DISPID_AMBIENT_BACKCOLOR:
            VariantSetLong(res, (long)fs->ambientBackColor);
            break;

        case DISPID_AMBIENT_LOCALEID:
            VariantSetLong(res, (long)fs->ambientLocale);
            break;

        case DISPID_AMBIENT_USERMODE:
            VariantSetBool(res, fs->ambientUserMode);
            break;

        case DISPID_AMBIENT_SHOWGRABHANDLES:
            VariantSetBool(res, fs->ambientShowGrabHandles);
            break;

        case DISPID_AMBIENT_SHOWHATCHING:
            VariantSetBool(res, fs->ambientShowHatching);
            break;

        default:
            return DISP_E_MEMBERNOTFOUND;
    }
    return S_OK;
}

HRESULT HW_DWebBrowserEvents2::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid,
    WORD flags, DISPPARAMS * pDispParams, VARIANT * pVarResult,
    EXCEPINFO * pExcepInfo, unsigned int * puArgErr)
{
    if (flags & DISPATCH_PROPERTYGET)
        return DispatchPropGet(dispIdMember, pVarResult);

    switch (dispIdMember)
    {
        case DISPID_BEFORENAVIGATE2:
        {
            BSTR url;
            VARIANT *vurl = pDispParams->rgvarg[5].pvarVal;
            if (vurl->vt & VT_BYREF)
                url = *vurl->pbstrVal;
            else
                url = vurl->bstrVal;
#ifdef UNICODE
            bool shouldCancel = !fs->htmlWindow->OnBeforeNavigate(url, false);
#else
            bool shouldCancel = !fs->htmlWindow->OnBeforeNavigate(ScopedMem<TCHAR>(str::conv::FromWStr(url)), false);
#endif
            *pDispParams->rgvarg[0].pboolVal = shouldCancel ? VARIANT_TRUE : VARIANT_FALSE;
            break;
        }

#if 0
        case DISPID_PROGRESSCHANGE:
        {
            long current = pDispParams->rgvarg[1].lVal;
            long maximum = pDispParams->rgvarg[0].lVal;
            fs->htmlWindow->OnProgressURL(current, maximum);
            break;
        }
#endif

        case DISPID_DOCUMENTCOMPLETE:
        {
            BSTR url;
            VARIANT *vurl = pDispParams->rgvarg[0].pvarVal;
            if (vurl->vt & VT_BYREF)
                url = *vurl->pbstrVal;
            else
                url = vurl->bstrVal;
#ifdef UNICODE
            fs->htmlWindow->OnDocumentComplete(url);
#else
            fs->htmlWindow->OnDocumentComplete(ScopedMem<TCHAR>(str::conv::FromWStr(url)));
#endif
            break;
        }

        case DISPID_COMMANDSTATECHANGE:
            switch (pDispParams->rgvarg[1].lVal) {
            case CSC_NAVIGATEBACK:
                fs->htmlWindow->canGoBack = pDispParams->rgvarg[0].boolVal;
                break;
            case CSC_NAVIGATEFORWARD:
                fs->htmlWindow->canGoForward = pDispParams->rgvarg[0].boolVal;
                break;
            }
            break;

        case DISPID_NEWWINDOW3:
        {
            BSTR url = pDispParams->rgvarg[0].bstrVal;
#ifdef UNICODE
            bool shouldCancel = !fs->htmlWindow->OnBeforeNavigate(url, true);
#else
            bool shouldCancel = !fs->htmlWindow->OnBeforeNavigate(ScopedMem<TCHAR>(str::conv::FromWStr(url)), true);
#endif
            *pDispParams->rgvarg[3].pboolVal = shouldCancel ? VARIANT_TRUE : VARIANT_FALSE;
            break;
        }

        case DISPID_NEWWINDOW2:
            // prior to Windows XP SP2, there's no way of getting the URL
            // to be opened, so we have to fail silently
            *pDispParams->rgvarg[0].pboolVal = VARIANT_FALSE;
            break;
    }

    return S_OK;
}

//IOleWindow
HRESULT HW_IOleInPlaceFrame::GetWindow(HWND *phwnd)
{
    if (phwnd == NULL)
        return E_INVALIDARG;
    *phwnd = fs->hwndParent;
    return S_OK;
}

//IOleInPlaceUIWindow
HRESULT HW_IOleInPlaceFrame::GetBorder(LPRECT lprectBorder)
{
    if (lprectBorder == NULL)
        return E_INVALIDARG;
    return INPLACE_E_NOTOOLSPACE;
}

HRESULT HW_IOleInPlaceFrame::RequestBorderSpace(LPCBORDERWIDTHS pborderwidths)
{
    if (pborderwidths == NULL)
        return E_INVALIDARG;
    return INPLACE_E_NOTOOLSPACE;
}

//IOleInPlaceSite
HRESULT HW_IOleInPlaceSiteWindowless::OnInPlaceActivate()
{
    fs->inPlaceActive = true;
    return S_OK;
}

HRESULT HW_IOleInPlaceSiteWindowless::OnUIActivate()
{
    fs->uiActive = true;
    return S_OK;
}

HRESULT HW_IOleInPlaceSiteWindowless::GetWindowContext(
    IOleInPlaceFrame **ppFrame, IOleInPlaceUIWindow **ppDoc,
    LPRECT lprcPosRect, LPRECT lprcClipRect,
    LPOLEINPLACEFRAMEINFO lpFrameInfo)
{
    if (ppFrame == NULL || ppDoc == NULL || lprcPosRect == NULL ||
            lprcClipRect == NULL || lpFrameInfo == NULL)
    {
        if (ppFrame != NULL)
            *ppFrame = NULL;
        if (ppDoc != NULL)
            *ppDoc = NULL;
        return E_INVALIDARG;
    }

    *ppDoc = *ppFrame = fs->oleInPlaceFrame;
    (*ppDoc)->AddRef();
    (*ppFrame)->AddRef();

    lpFrameInfo->fMDIApp = FALSE;
    lpFrameInfo->hwndFrame = fs->hwndParent;
    lpFrameInfo->haccel = NULL;
    lpFrameInfo->cAccelEntries = 0;

    return S_OK;
}

HRESULT HW_IOleInPlaceSiteWindowless::OnUIDeactivate(BOOL fUndoable)
{
    fs->uiActive = false;
    return S_OK;
}

HRESULT HW_IOleInPlaceSiteWindowless::OnInPlaceDeactivate()
{
    fs->inPlaceActive = false;
    return S_OK;
}

//IOleInPlaceSiteEx
HRESULT HW_IOleInPlaceSiteWindowless::OnInPlaceActivateEx(BOOL * pfNoRedraw, DWORD dwFlags)
{
    if (pfNoRedraw)
        *pfNoRedraw = FALSE;
    return S_OK;
}

//IOleInPlaceSiteWindowless
HRESULT HW_IOleInPlaceSiteWindowless::CanWindowlessActivate()
{
    return fs->supportsWindowlessActivation ? S_OK : S_FALSE;
}

HRESULT HW_IOleInPlaceSiteWindowless::GetDC(LPCRECT pRect, DWORD grfFlags, HDC* phDC)
{
    if (phDC == NULL)
        return E_INVALIDARG;

#if 0
    if (grfFlags & OLEDC_NODRAW)
    {
        *phDC = mfs->hDCBuffer;
        return S_OK;
    }

    if (fs->hDCBuffer != NULL)
        return E_UNEXPECTED;
#endif
    return E_NOTIMPL;
}

HRESULT HW_IOleInPlaceSiteWindowless::InvalidateRect(LPCRECT pRect, BOOL fErase)
{

    ::InvalidateRect(fs->hwndParent, NULL, fErase);
    return S_OK;
}

//IOleClientSite
HRESULT HW_IOleClientSite::GetContainer(LPOLECONTAINER * ppContainer)
{
    if (ppContainer == NULL)
        return E_INVALIDARG;
    return QueryInterface(IID_IOleContainer, (void**)ppContainer);
}

//IOleItemContainer
HRESULT HW_IOleItemContainer::GetObject(LPOLESTR pszItem,
    DWORD dwSpeedNeeded, IBindCtx * pbc, REFIID riid, void ** ppvObject)
{
    if (pszItem == NULL)
        return E_INVALIDARG;
    if (ppvObject == NULL)
        return E_INVALIDARG;
    *ppvObject = NULL;
    return MK_E_NOOBJECT;
}

HRESULT HW_IOleItemContainer::GetObjectStorage(LPOLESTR pszItem,
    IBindCtx * pbc, REFIID riid, void ** ppvStorage)
{
    if (pszItem == NULL)
        return E_INVALIDARG;
    if (ppvStorage == NULL)
        return E_INVALIDARG;
    *ppvStorage = NULL;
    return MK_E_NOOBJECT;
}

HRESULT HW_IOleItemContainer::IsRunning(LPOLESTR pszItem)
{
    if (pszItem == NULL)
        return E_INVALIDARG;
    return MK_E_NOOBJECT;
}

//IOleControlSite
HRESULT HW_IOleControlSite::LockInPlaceActive(BOOL fLock)
{
    fs->inPlaceLocked = (fLock == TRUE);
    return S_OK;
}

HRESULT HW_IOleControlSite::TransformCoords(POINTL *pPtlHimetric,
    POINTF *pPtfContainer, DWORD dwFlags)
{
    HRESULT hr = S_OK;
    if (pPtlHimetric == NULL)
            return E_INVALIDARG;
    if (pPtfContainer == NULL)
            return E_INVALIDARG;
    return hr;
}

//IOleCommandTarget
HRESULT HW_IOleCommandTarget::QueryStatus(const GUID *pguidCmdGroup, 
    ULONG cCmds, OLECMD *prgCmds, OLECMDTEXT *pCmdTet)
{
    if (prgCmds == NULL)
        return E_INVALIDARG;
    return OLECMDERR_E_UNKNOWNGROUP;
}
