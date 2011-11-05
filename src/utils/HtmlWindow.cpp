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

// Info about implementing web browser control
// http://www.codeproject.com/KB/COM/cwebpage.aspx

// The code is structured in a similar way as wxWindows'
// browser wrapper
// http://codesearch.google.com/#cbxlbgWFJ4U/wxCode/components/iehtmlwin/src/IEHtmlWin.h
// http://codesearch.google.com/#cbxlbgWFJ4U/wxCode/components/iehtmlwin/src/IEHtmlWin.cpp

// Another code to get inspired: http://code.google.com/p/fidolook/source/browse/trunk/Qm/ui/messageviewwindow.cpp
// To show a page within chm file use url in the form:
// "its:MyChmFile.chm::mywebpage.htm"
// http://msdn.microsoft.com/en-us/library/aa164814(v=office.10).aspx

class HW_IOleInPlaceFrame;
class HW_IOleInPlaceSiteWindowless;
class HW_IOleClientSite;
class HW_IOleControlSite;
class HW_IOleCommandTarget;
class HW_IOleItemContainer;
class HW_IDispatch;
class HW_DWebBrowserEvents2;
class HW_IAdviseSink2;
class HW_IAdviseSinkEx;

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
    friend class HW_IDispatch;
    friend class HW_DWebBrowserEvents2;
    friend class HW_IAdviseSink2;
    friend class HW_IAdviseSinkEx;

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
    HW_IDispatch *                  hwIDispatch;
    HW_DWebBrowserEvents2 *         hwDWebBrowserEvents2;
    HW_IAdviseSink2 *               adviseSink2;
    HW_IAdviseSinkEx *              adviseSinkEx;

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
    STDMETHODIMP ContextSensitiveHelp(BOOL);
    //IOleInPlaceUIWindow
    STDMETHODIMP GetBorder(LPRECT);
    STDMETHODIMP RequestBorderSpace(LPCBORDERWIDTHS);
    STDMETHODIMP SetBorderSpace(LPCBORDERWIDTHS);
    STDMETHODIMP SetActiveObject(IOleInPlaceActiveObject*, LPCOLESTR);
    //IOleInPlaceFrame
    STDMETHODIMP InsertMenus(HMENU, LPOLEMENUGROUPWIDTHS);
    STDMETHODIMP SetMenu(HMENU, HOLEMENU, HWND);
    STDMETHODIMP RemoveMenus(HMENU);
    STDMETHODIMP SetStatusText(LPCOLESTR);
    STDMETHODIMP EnableModeless(BOOL);
    STDMETHODIMP TranslateAccelerator(LPMSG, WORD);
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
    STDMETHODIMP CanInPlaceActivate();
    STDMETHODIMP OnInPlaceActivate();
    STDMETHODIMP OnUIActivate();
    STDMETHODIMP GetWindowContext(IOleInPlaceFrame**, IOleInPlaceUIWindow**,
            LPRECT, LPRECT, LPOLEINPLACEFRAMEINFO);
    STDMETHODIMP Scroll(SIZE);
    STDMETHODIMP OnUIDeactivate(BOOL);
    STDMETHODIMP OnInPlaceDeactivate();
    STDMETHODIMP DiscardUndoState();
    STDMETHODIMP DeactivateAndUndo();
    STDMETHODIMP OnPosRectChange(LPCRECT);
    //IOleInPlaceSiteEx
    STDMETHODIMP OnInPlaceActivateEx(BOOL*, DWORD);
    STDMETHODIMP OnInPlaceDeactivateEx(BOOL);
    STDMETHODIMP RequestUIActivate();
    //IOleInPlaceSiteWindowless
    STDMETHODIMP CanWindowlessActivate();
    STDMETHODIMP GetCapture();
    STDMETHODIMP SetCapture(BOOL);
    STDMETHODIMP GetFocus();
    STDMETHODIMP SetFocus(BOOL);
    STDMETHODIMP GetDC(LPCRECT, DWORD, HDC*);
    STDMETHODIMP ReleaseDC(HDC);
    STDMETHODIMP InvalidateRect(LPCRECT, BOOL);
    STDMETHODIMP InvalidateRgn(HRGN, BOOL);
    STDMETHODIMP ScrollRect(INT, INT, LPCRECT, LPCRECT);
    STDMETHODIMP AdjustRect(LPRECT);
    STDMETHODIMP OnDefWindowMessage(UINT, WPARAM, LPARAM, LRESULT*);
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
    STDMETHODIMP SaveObject();
    STDMETHODIMP GetMoniker(DWORD, DWORD, IMoniker**);
    STDMETHODIMP GetContainer(LPOLECONTAINER FAR*);
    STDMETHODIMP ShowObject();
    STDMETHODIMP OnShowWindow(BOOL);
    STDMETHODIMP RequestNewObjectLayout();
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
    STDMETHODIMP OnControlInfoChanged();
    STDMETHODIMP LockInPlaceActive(BOOL);
    STDMETHODIMP GetExtendedControl(IDispatch**);
    STDMETHODIMP TransformCoords(POINTL*, POINTF*, DWORD);
    STDMETHODIMP TranslateAccelerator(LPMSG, DWORD);
    STDMETHODIMP OnFocus(BOOL);
    STDMETHODIMP ShowPropertyFrame();
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
    STDMETHODIMP Exec(const GUID*, DWORD, DWORD, VARIANTARG*, VARIANTARG*);
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
    STDMETHODIMP ParseDisplayName(IBindCtx*, LPOLESTR, ULONG*, IMoniker**);
    //IOleContainer
    STDMETHODIMP EnumObjects(DWORD, IEnumUnknown**);
    STDMETHODIMP LockContainer(BOOL);
    //IOleItemContainer
    STDMETHODIMP GetObject(LPOLESTR, DWORD, IBindCtx*, REFIID, void**);
    STDMETHODIMP GetObjectStorage(LPOLESTR, IBindCtx*, REFIID, void**);
    STDMETHODIMP IsRunning(LPOLESTR);
protected:
    FrameSite * fs;
};

class HW_IDispatch : public IDispatch
{
public:
    HW_IDispatch(FrameSite* fs) : fs(fs) { }
    ~HW_IDispatch() {}

    //IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }
    //IDispatch
    STDMETHODIMP GetIDsOfNames(REFIID, OLECHAR**, unsigned int, LCID, DISPID*);
    STDMETHODIMP GetTypeInfo(unsigned int, LCID, ITypeInfo**);
    STDMETHODIMP GetTypeInfoCount(unsigned int*);
    STDMETHODIMP Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*);

    // helper
    HRESULT DispatchPropGet(DISPID dispIdMember, VARIANT *res);

protected:
    FrameSite * fs;
};

class HW_DWebBrowserEvents2 : public DWebBrowserEvents2
{
public:
    HW_DWebBrowserEvents2(FrameSite* fs) : fs(fs) { }
    ~HW_DWebBrowserEvents2() {}

    //IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }
    //IDispatch
    STDMETHODIMP GetIDsOfNames(REFIID r, OLECHAR** o, unsigned int i, LCID l, DISPID* d)
    { return fs->hwIDispatch->GetIDsOfNames(r, o, i, l, d); }
    STDMETHODIMP GetTypeInfo(unsigned int i, LCID l, ITypeInfo** t)
    { return fs->hwIDispatch->GetTypeInfo(i, l, t); }
    STDMETHODIMP GetTypeInfoCount(unsigned int* i)
    { return fs->hwIDispatch->GetTypeInfoCount(i); }
    STDMETHODIMP Invoke(DISPID d, REFIID r, LCID l, WORD w, DISPPARAMS* dp,
            VARIANT* v, EXCEPINFO* e, UINT* u)
    { return fs->hwIDispatch->Invoke(d, r, l, w, dp, v, e, u); }
protected:
    FrameSite * fs;
};

class HW_IAdviseSink2 : public IAdviseSink2
{
public:
    HW_IAdviseSink2(FrameSite* fs) : fs(fs) { }
    ~HW_IAdviseSink2() {}

    //IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }
    //IAdviseSink
    void STDMETHODCALLTYPE OnDataChange(FORMATETC*, STGMEDIUM*);
    void STDMETHODCALLTYPE OnViewChange(DWORD, LONG);
    void STDMETHODCALLTYPE OnRename(IMoniker*);
    void STDMETHODCALLTYPE OnSave();
    void STDMETHODCALLTYPE OnClose();
    //IAdviseSink2
    void STDMETHODCALLTYPE OnLinkSrcChange(IMoniker*);
protected:
    FrameSite * fs;
};

class HW_IAdviseSinkEx : public IAdviseSinkEx
{
public:
    HW_IAdviseSinkEx(FrameSite* fs) : fs(fs) { }
    ~HW_IAdviseSinkEx() {}

    //IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }
    //IAdviseSink
    void STDMETHODCALLTYPE OnDataChange(FORMATETC* f, STGMEDIUM* s)
    { fs->adviseSink2->OnDataChange(f, s); }
    void STDMETHODCALLTYPE OnViewChange(DWORD d, LONG l)
    { fs->adviseSink2->OnViewChange(d, l); }
    void STDMETHODCALLTYPE OnRename(IMoniker* i)
    { fs->adviseSink2->OnRename(i); }
    void STDMETHODCALLTYPE OnSave()
    { fs->adviseSink2->OnSave(); }
    void STDMETHODCALLTYPE OnClose()
    { fs->adviseSink2->OnClose(); }
    //IAdviseSinkEx
    void STDMETHODCALLTYPE OnViewStatusChange(DWORD);
protected:
    FrameSite * fs;
};

static LRESULT CALLBACK WndProcHtml(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
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
            }
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void SubclassHtmlHwnd(HWND hwnd, HtmlWindow *htmlWin)
{
    // Note: assuming hwnd is plain hwnd, with no special hwnd proc
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WndProcHtml);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)htmlWin);
}

void UnsubclassHtmlHwnd(HWND hwnd)
{
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)0);
}

HtmlWindow::HtmlWindow(HWND hwnd, HtmlWindowCallback *cb) :
    hwnd(hwnd), webBrowser(NULL), oleObject(NULL),
    oleInPlaceObject(NULL), viewObject(NULL),
    connectionPoint(NULL), oleObjectHwnd(NULL),
    adviseCookie(0), aboutBlankShown(false), htmlWinCb(cb),
    documentLoaded(false)
{
    assert(hwnd);
    SubclassHtmlHwnd(hwnd, this);
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
    RECT rc = ClientRect(hwnd).ToRECT();

    oleInPlaceObject->SetObjectRects(&rc, &rc);
    if (!invisibleAtRuntime) {
        hr = oleObject->DoVerb(OLEIVERB_INPLACEACTIVATE, NULL,
                fs->oleClientSite, 0, hwnd, &rc);
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
}

HtmlWindow::~HtmlWindow()
{
    UnsubclassHtmlHwnd(hwnd);
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

void HtmlWindow::SetVisible(bool visible)
{
    if (visible)
        ShowWindow(hwnd, SW_SHOW);
    else
        ShowWindow(hwnd, SW_HIDE);
    if (webBrowser)
        webBrowser->put_Visible(visible ? TRUE : FALSE);
}

void HtmlWindow::NavigateToUrl(const TCHAR *urlStr)
{
    VARIANT url;
    VariantInit(&url);
    url.vt = VT_BSTR;
    url.bstrVal = SysAllocString(urlStr);
    if (!url.bstrVal)
        return;
    webBrowser->Navigate2(&url, 0, 0, 0, 0);
    VariantClear(&url);
}

void HtmlWindow::GoBack()
{
    aboutBlankShown = false; // TODO: is this necessary?
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

void HtmlWindow::EnsureAboutBlankShown()
{
    if (aboutBlankShown)
        return;
    aboutBlankShown = true;

    NavigateToUrl(_T("about:blank"));
    IHTMLDocument2 *doc = NULL;
    HRESULT hr = S_OK;
    // wait until shown
    while ((doc == NULL) && (hr == S_OK)) {
        Sleep(0);
        IDispatch *docDispatch = NULL;
        hr = webBrowser->get_Document(&docDispatch);
        if (SUCCEEDED(hr) && (docDispatch != NULL)) {
            hr = docDispatch->QueryInterface(IID_IHTMLDocument2, (void **)&doc);
            docDispatch->Release();
        }
    }

    if (doc != NULL)
        doc->Release();
}

void HtmlWindow::DisplayHtml(const TCHAR *html)
{
    LPDISPATCH       docDispatch = NULL;
    IHTMLDocument2 * doc = NULL;
    SAFEARRAY *      arr = NULL;
    VARIANT *        var = NULL;

    // don't know why, but that's what other people do
    EnsureAboutBlankShown();
    HRESULT hr = webBrowser->get_Document(&docDispatch);
    if (FAILED(hr))
        goto Exit;

    hr = docDispatch->QueryInterface(IID_IHTMLDocument2, (void**)&doc);
    if (FAILED(hr))
        goto Exit;

    arr = SafeArrayCreateVector(VT_VARIANT, 0, 1);
    if (!arr)
        goto Exit;
    hr = SafeArrayAccessData(arr, (void**)&var);
    if (FAILED(hr))
        goto Exit;
    var->vt = VT_BSTR;
    var->bstrVal = SysAllocString(html);
    if (!var->bstrVal)
        goto Exit;
    SafeArrayUnaccessData(arr);

    hr = doc->write(arr);
    doc->close();
    if (FAILED(hr))
        goto Exit;

Exit:
    if (arr)
        SafeArrayDestroy(arr);
    if (doc)
        doc->Release();
    if (docDispatch)
        docDispatch->Release();
}

#include <atlbase.h>
#include <atlwin.h>
#include <atlcom.h>
#include <atlhost.h>
#include <atlimage.h>

// Take a screenshot of a given <area> inside an html window and resize
// it to <finalSize>. It's up to the caller to make sure <area> fits
// within window (we don't check that's the case)
HBITMAP HtmlWindow::TakeScreenshot(RectI area, SizeI finalSize)
{
    HRESULT             hr;
    IDispatch*          docDispatch = NULL;
    IHTMLDocument3 *    doc3        = NULL;
    IViewObject2 *      view        = NULL;
    HDC                 dc = NULL;
    CImage              image;
    CImage              imageRes;
    RECTL               rc = { 0, 0, 0, 0};
    HBITMAP             hbmp = NULL;
    WindowRect          winRc(hwnd);

    hr = webBrowser->get_Document(&docDispatch);
    if (FAILED(hr))
        goto Exit;

    hr = docDispatch->QueryInterface(IID_IHTMLDocument3, (void**)&doc3);
    if (FAILED(hr))
        goto Exit;

    hr = doc3->QueryInterface(IID_IViewObject2, (void**)&view);
    if (FAILED(hr))
        goto Exit;

    // capture the whole window (including scrollbars)
    // to image and create imageRes containing the area
    // user asked for
    image.Create(winRc.dx, winRc.dy, 24);
    dc = image.GetDC();
    rc.right = winRc.dx;
    rc.bottom = winRc.dy;
    hr = view->Draw(DVASPECT_CONTENT, -1, NULL, NULL, dc,
                          dc, &rc, NULL, NULL, 0);
    image.ReleaseDC();
    if (FAILED(hr))
        goto Exit;

    imageRes.Create(finalSize.dx, finalSize.dy, 24);
    dc = imageRes.GetDC();
    // TODO: the quality of the resize is poor. Probably should use Gdi+ instead
    // and high-quality resize method
    image.Draw(dc, 0, 0, finalSize.dx, finalSize.dy,
                   0, 0, area.dx, area.dy);
    imageRes.ReleaseDC();
    hbmp = imageRes.Detach();

Exit:
    if (view)
        view->Release();
    if (doc3)
        doc3->Release();
    if (docDispatch)
        docDispatch->Release();
    return hbmp;
}

// the format for chm page is: "its:MyChmFile.chm::mywebpage.htm"
void HtmlWindow::DisplayChmPage(const TCHAR *chmFilePath, const TCHAR *chmPage)
{
    if (str::StartsWith(chmPage, _T("/")))
        chmPage++;
    ScopedMem<TCHAR> url(str::Format(_T("its:%s::/%s"), chmFilePath, chmPage));
    NavigateToUrl(url);
}

// called before an url is shown. If returns false, will cancel
// the navigation.
bool HtmlWindow::OnBeforeNavigate(const TCHAR *url)
{
    documentLoaded = false;
    if (htmlWinCb)
        return htmlWinCb->OnBeforeNavigate(url);
    return true;
}

void HtmlWindow::OnDocumentComplete(const TCHAR *url)
{
    documentLoaded = true;
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

// Probably could check IHTMLDocument2::::get_readyState() but checking
// documentLoaded works
bool HtmlWindow::WaitUntilLoaded(DWORD maxWaitMs)
{
    const DWORD sleepTimeMs = 200; // 0.2 sec
    DWORD waitTimeMs = 0;
    while (!documentLoaded && (waitTimeMs < maxWaitMs))
    {
        PumpRemainingMessages();
        Sleep(sleepTimeMs);
        waitTimeMs += sleepTimeMs;
    }
    return documentLoaded;
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
    hwndParent = htmlWindow->hwnd;

    oleInPlaceFrame = new HW_IOleInPlaceFrame(this);
    oleInPlaceSiteWindowless = new HW_IOleInPlaceSiteWindowless(this);
    oleClientSite = new HW_IOleClientSite(this);
    oleControlSite = new HW_IOleControlSite(this);
    oleCommandTarget = new HW_IOleCommandTarget(this);
    oleItemContainer = new HW_IOleItemContainer(this);
    hwIDispatch = new HW_IDispatch(this);
    hwDWebBrowserEvents2 = new HW_DWebBrowserEvents2(this);
    adviseSink2 = new HW_IAdviseSink2(this);
    adviseSinkEx = new HW_IAdviseSinkEx(this);
}

FrameSite::~FrameSite()
{
    delete adviseSinkEx;
    delete adviseSink2;
    delete hwDWebBrowserEvents2;
    delete hwIDispatch;
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
    else if (riid == IID_IDispatch)
        *ppv = hwIDispatch;
    else if (riid == DIID_DWebBrowserEvents2)
        *ppv = hwDWebBrowserEvents2;
    else if (riid == IID_IAdviseSink ||
        riid == IID_IAdviseSink2)
        *ppv = adviseSink2;
    else if (riid == IID_IAdviseSinkEx)
        *ppv = adviseSinkEx;

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
HRESULT HW_IDispatch::GetIDsOfNames(REFIID riid, OLECHAR ** rgszNames,
    unsigned int cNames, LCID lcid, DISPID * rgDispId)
{
    return E_NOTIMPL;
}

HRESULT HW_IDispatch::GetTypeInfo(unsigned int iTInfo, LCID lcid, ITypeInfo ** ppTInfo)
{
    return E_NOTIMPL;
}

HRESULT HW_IDispatch::GetTypeInfoCount(unsigned int * pcTInfo)
{
    return E_NOTIMPL;
}

HRESULT HW_IDispatch::DispatchPropGet(DISPID dispIdMember, VARIANT *res)
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

HRESULT HW_IDispatch::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid,
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
            bool shouldCancel = !fs->htmlWindow->OnBeforeNavigate(url);
            *pDispParams->rgvarg->pboolVal = shouldCancel ? VARIANT_TRUE : VARIANT_FALSE;
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
            fs->htmlWindow->OnDocumentComplete(url);
            break;
        }

        default:
            return S_OK;
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

HRESULT HW_IOleInPlaceFrame::ContextSensitiveHelp(BOOL fEnterMode)
{
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

HRESULT HW_IOleInPlaceFrame::SetBorderSpace(LPCBORDERWIDTHS pborderwidths)
{
    return S_OK;
}

HRESULT HW_IOleInPlaceFrame::SetActiveObject(IOleInPlaceActiveObject *pActiveObject, LPCOLESTR pszObjName)
{
    return S_OK;
}

//IOleInPlaceFrame
HRESULT HW_IOleInPlaceFrame::InsertMenus(HMENU hmenuShared, LPOLEMENUGROUPWIDTHS lpMenuWidths)
{
    return S_OK;
}

HRESULT HW_IOleInPlaceFrame::SetMenu(HMENU hmenuShared, HOLEMENU holemenu, HWND hwndActiveObject)
{
    return S_OK;
}

HRESULT HW_IOleInPlaceFrame::RemoveMenus(HMENU hmenuShared)
{
    return S_OK;
}

HRESULT HW_IOleInPlaceFrame::SetStatusText(LPCOLESTR pszStatusText)
{
    return S_OK;
}

HRESULT HW_IOleInPlaceFrame::EnableModeless(BOOL fEnable)
{
    return S_OK;
}

HRESULT HW_IOleInPlaceFrame::TranslateAccelerator(LPMSG lpmsg, WORD wID)
{
    return E_NOTIMPL;
}

//IOleInPlaceSite
HRESULT HW_IOleInPlaceSiteWindowless::CanInPlaceActivate()
{
    return S_OK;
}

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

HRESULT HW_IOleInPlaceSiteWindowless::Scroll(SIZE scrollExtent)
{
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

HRESULT HW_IOleInPlaceSiteWindowless::DiscardUndoState()
{
    return S_OK;
}

HRESULT HW_IOleInPlaceSiteWindowless::DeactivateAndUndo()
{
    return S_OK;
}

HRESULT HW_IOleInPlaceSiteWindowless::OnPosRectChange(LPCRECT lprcPosRect)
{
    return S_OK;
}

//IOleInPlaceSiteEx
HRESULT HW_IOleInPlaceSiteWindowless::OnInPlaceActivateEx(BOOL * pfNoRedraw, DWORD dwFlags)
{
    if (pfNoRedraw)
        *pfNoRedraw = FALSE;
    return S_OK;
}

HRESULT HW_IOleInPlaceSiteWindowless::OnInPlaceDeactivateEx(BOOL fNoRedraw)
{
    return S_OK;
}

HRESULT HW_IOleInPlaceSiteWindowless::RequestUIActivate()
{
    return S_FALSE;
}

//IOleInPlaceSiteWindowless
HRESULT HW_IOleInPlaceSiteWindowless::CanWindowlessActivate()
{
    return fs->supportsWindowlessActivation ? S_OK : S_FALSE;
}

HRESULT HW_IOleInPlaceSiteWindowless::GetCapture()
{
    return S_FALSE;
}

HRESULT HW_IOleInPlaceSiteWindowless::SetCapture(BOOL fCapture)
{
    return S_FALSE;
}

HRESULT HW_IOleInPlaceSiteWindowless::GetFocus()
{
    return S_OK;
}

HRESULT HW_IOleInPlaceSiteWindowless::SetFocus(BOOL fFocus)
{
    return S_OK;
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

HRESULT HW_IOleInPlaceSiteWindowless::ReleaseDC(HDC hDC)
{
    return E_NOTIMPL;
}

HRESULT HW_IOleInPlaceSiteWindowless::InvalidateRect(LPCRECT pRect, BOOL fErase)
{

    ::InvalidateRect(fs->hwndParent, NULL, fErase);
    return S_OK;
}

HRESULT HW_IOleInPlaceSiteWindowless::InvalidateRgn(HRGN, BOOL)
{
    return E_NOTIMPL;
}

HRESULT HW_IOleInPlaceSiteWindowless::ScrollRect(INT, INT, LPCRECT, LPCRECT)
{
    return E_NOTIMPL;
}

HRESULT HW_IOleInPlaceSiteWindowless::AdjustRect(LPRECT)
{
    return E_NOTIMPL;
}

HRESULT HW_IOleInPlaceSiteWindowless::OnDefWindowMessage(UINT, WPARAM, LPARAM, LRESULT*)
{
    return E_NOTIMPL;
}

//IOleClientSite
HRESULT HW_IOleClientSite::SaveObject()
{
    return S_OK;
}

HRESULT HW_IOleClientSite::GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker,
                                      IMoniker ** ppmk)
{
    return E_NOTIMPL;
}

HRESULT HW_IOleClientSite::GetContainer(LPOLECONTAINER * ppContainer)
{
    if (ppContainer == NULL)
        return E_INVALIDARG;
    return QueryInterface(IID_IOleContainer, (void**)ppContainer);
}

HRESULT HW_IOleClientSite::ShowObject()
{
    return S_OK;
}

HRESULT HW_IOleClientSite::OnShowWindow(BOOL fShow)
{
    return S_OK;
}

HRESULT HW_IOleClientSite::RequestNewObjectLayout()
{
    return E_NOTIMPL;
}

//IParseDisplayName
HRESULT HW_IOleItemContainer::ParseDisplayName(IBindCtx *pbc,
    LPOLESTR pszDisplayName, ULONG *pchEaten, IMoniker **ppmkOut)
{
    return E_NOTIMPL;
}

//IOleContainer
HRESULT HW_IOleItemContainer::EnumObjects(DWORD grfFlags, IEnumUnknown **ppenum)
{
    return E_NOTIMPL;
}

HRESULT HW_IOleItemContainer::LockContainer(BOOL fLock)
{
    return S_OK;
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
HRESULT HW_IOleControlSite::OnControlInfoChanged()
{
    return S_OK;
}

HRESULT HW_IOleControlSite::LockInPlaceActive(BOOL fLock)
{
    fs->inPlaceLocked = (fLock == TRUE);
    return S_OK;
}

HRESULT HW_IOleControlSite::GetExtendedControl(IDispatch ** ppDisp)
{
    return E_NOTIMPL;
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

HRESULT HW_IOleControlSite::TranslateAccelerator(LPMSG pMsg, DWORD grfModifiers)
{
    return E_NOTIMPL;
}

HRESULT HW_IOleControlSite::OnFocus(BOOL fGotFocus)
{
    return S_OK;
}

HRESULT HW_IOleControlSite::ShowPropertyFrame()
{
    return E_NOTIMPL;
}

//IOleCommandTarget
HRESULT HW_IOleCommandTarget::QueryStatus(const GUID *pguidCmdGroup, 
    ULONG cCmds, OLECMD *prgCmds, OLECMDTEXT *pCmdTet)
{
    if (prgCmds == NULL)
        return E_INVALIDARG;
    return OLECMDERR_E_UNKNOWNGROUP;
}

HRESULT HW_IOleCommandTarget::Exec(const GUID *pguidCmdGroup, DWORD nCmdID,
    DWORD nCmdExecOpt, VARIANTARG *pVaIn, VARIANTARG *pVaOut)
{
    return OLECMDERR_E_NOTSUPPORTED;
}

//IAdviseSink
void STDMETHODCALLTYPE HW_IAdviseSink2::OnDataChange(FORMATETC * pFormatEtc, STGMEDIUM * pgStgMed)
{
}

void STDMETHODCALLTYPE HW_IAdviseSink2::OnViewChange(DWORD dwAspect, LONG lIndex)
{
    // redraw the control
    fs->oleInPlaceSiteWindowless->InvalidateRect(NULL, FALSE);
}

void STDMETHODCALLTYPE HW_IAdviseSink2::OnRename(IMoniker * pmk)
{
}

void STDMETHODCALLTYPE HW_IAdviseSink2::OnSave()
{
}

void STDMETHODCALLTYPE HW_IAdviseSink2::OnClose()
{
}

//IAdviseSink2
void STDMETHODCALLTYPE HW_IAdviseSink2::OnLinkSrcChange(IMoniker * pmk)
{
}

//IAdviseSinkEx
void STDMETHODCALLTYPE HW_IAdviseSinkEx::OnViewStatusChange(DWORD dwViewStatus)
{
}
