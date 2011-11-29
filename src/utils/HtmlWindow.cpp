/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#include "HtmlWindow.h"

#include <mshtml.h>
#include <mshtmhst.h>
#include <oaidl.h>
#include <exdispid.h>
#include <wininet.h>

#include "StrUtil.h"
#include "GeomUtil.h"
#include "WinUtil.h"
#include "Scopes.h"

#pragma comment(lib, "urlmon")

// Unresolved issues: when loading html from memory (SetHtml()) links are not resolved.
// Possible solutions:
// 1. choose a custom prefix for our urls (e.g. sumhtml:), implement IInternetProtocolInfo
//    that understands those urls and register it as a namespace with IInternetSessions
//    http://code.google.com/p/atc32/source/browse/trunk/WorldWindProject/lib-external/webview/windows/WebViewWindow.cpp
//    (WebViewWindow::InitializeWebViewProtocol). Combine that with ability to set
//    base url (sumhtml:/) for HtmlMoniker
// 2. use its: and add its:filename to IE's trusted zone (http://csexwb2.googlecode.com/svn/trunk/cEXWB.cs, AddUrlToTrustedZone)
// 3. something simpler?

// Book on ATL: http://369o.com/data/books/atl/index.html, which is
// helpful in understanding embedding of the browser control

// Info about implementing web browser control
// http://www.codeproject.com/KB/COM/cwebpage.aspx

// The code is structured in a similar way as wxWindows'
// browser wrapper
// http://codesearch.google.com/#cbxlbgWFJ4U/wxCode/components/iehtmlwin/src/IEHtmlWin.h
// http://codesearch.google.com/#cbxlbgWFJ4U/wxCode/components/iehtmlwin/src/IEHtmlWin.cpp

// Info about IInternetProtocol: http://www.codeproject.com/KB/IP/DataProtocol.aspx
// Source code for inspiration: http://codesearch.google.com/#X_1VXq0XKpU/trunk/shareaza/IEProtocol.h

// Other code that does advanced things with embedding IE or providing it with non-trivial
// interfaces:
// http://osh.codeplex.com/
// http://code.google.com/p/atc32/source/browse/trunk/WorldWindProject/lib-external/webview/windows/
// http://code.google.com/p/fidolook/source/browse/trunk/Qm/ui/messageviewwindow.cpp
// http://code.google.com/p/csexwb2/
// chrome frame: http://codesearch.google.com/#wZuuyuB8jKQ/chromium/src/chrome_frame/chrome_protocol.h
// gears: http://code.google.com/p/gears/
// http://code.google.com/p/fictionbookeditor/
// http://code.google.com/p/easymule/
// http://code.google.com/p/svnprotocolhandler/ (IInternetProtocolInfo implementation)
// https://github.com/facebook/ie-toolbar (also IInternetProtocolInfo implementation)
// http://code.google.com/p/veryie/


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
class HW_IServiceProvider;
class HW_IInternetSecurityManager;
class HW_IDocHostUIHandler;
class HW_IDropTarget;
class HW_IInternetProtocolFactory;
class HW_IInternetProtocol;

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
// FrameSite ties together HtmlWindow and all the COM interfaces we need to implement
// to support it
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
    friend class HW_IServiceProvider;
    friend class HW_IInternetSecurityManager;
    friend class HW_IDocHostUIHandler;
    friend class HW_IDropTarget;

public:
    FrameSite(HtmlWindow * win);
    ~FrameSite();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

protected:
    int refCount;

    HW_IOleInPlaceFrame *           oleInPlaceFrame;
    HW_IOleInPlaceSiteWindowless *  oleInPlaceSiteWindowless;
    HW_IOleClientSite *             oleClientSite;
    HW_IOleControlSite *            oleControlSite;
    HW_IOleCommandTarget *          oleCommandTarget;
    HW_IOleItemContainer *          oleItemContainer;
    HW_DWebBrowserEvents2 *         hwDWebBrowserEvents2;
    HW_IAdviseSink2 *               adviseSink2;
    HW_IServiceProvider *           serviceProvider;
    HW_IInternetSecurityManager *   internetSecurityManager;
    HW_IDocHostUIHandler *          docHostUIHandler;
    HW_IDropTarget *                dropTarget;

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

#define WH_PROTO_PREFIX _T("hw_html_window")

// {F1EC293F-DBBD-4A4B-94F4-FA52BA0BA6EE}
static const GUID CLSID_HW_IInternetProtocol = { 0xf1ec293f, 0xdbbd, 0x4a4b, { 0x94, 0xf4, 0xfa, 0x52, 0xba, 0xb, 0xa6, 0xee } };

class HW_IInternetProtocol : public IInternetProtocolInfo
{
public:
    HW_IInternetProtocol() : refCount(1) { }

protected:
    virtual ~HW_IInternetProtocol() { }

public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject);
    ULONG STDMETHODCALLTYPE AddRef() { return ++refCount; }
    ULONG STDMETHODCALLTYPE Release();

    // IInternetProtocolInfo
    HRESULT STDMETHODCALLTYPE ParseUrl(LPCWSTR pwzUrl, PARSEACTION ParseAction, DWORD dwParseFlags,
        LPWSTR pwzResult, DWORD cchResult, DWORD *pcchResult, DWORD dwReserved);

    HRESULT STDMETHODCALLTYPE CombineUrl(LPCWSTR pwzBaseUrl, LPCWSTR pwzRelativeUrl,
        DWORD dwCombineFlags, LPWSTR pwzResult, DWORD cchResult, DWORD *pcchResult,
        DWORD dwReserved);
        
    HRESULT STDMETHODCALLTYPE CompareUrl(LPCWSTR pwzUrl1, LPCWSTR pwzUrl2, DWORD dwCompareFlags)
    {
        return INET_E_DEFAULT_ACTION;
    }
        
    HRESULT STDMETHODCALLTYPE QueryInfo(LPCWSTR pwzUrl, QUERYOPTION queryOption, DWORD dwQueryFlags,
        LPVOID pBuffer, DWORD cbBuffer, DWORD *pcbBuf, DWORD dwReserved)
    {
        return E_NOTIMPL;
    }

protected:
    int refCount;
};

ULONG STDMETHODCALLTYPE HW_IInternetProtocol::Release()
{
    if (--refCount != 0)
        return refCount;
    delete this;
    return 0;
}

STDMETHODIMP HW_IInternetProtocol::QueryInterface(REFIID riid, void **ppvObject)
{
    *ppvObject = NULL;

    if (riid == IID_IUnknown)
        *ppvObject = static_cast<IUnknown*>(this);
    else if (riid == IID_IInternetProtocolInfo)
        *ppvObject = static_cast<IInternetProtocolInfo*>(this);

    if (*ppvObject)
    {
        static_cast<IUnknown*>(*ppvObject)->AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE HW_IInternetProtocol::ParseUrl(LPCWSTR pwzUrl, PARSEACTION ParseAction,
    DWORD dwParseFlags, LPWSTR pwzResult, DWORD cchResult, DWORD *pcchResult, DWORD dwReserved)
{
    if (pwzUrl == NULL || pwzResult == NULL || pcchResult == NULL)
        return E_POINTER;

    // TODO: write me
    return S_FALSE;
}

HRESULT STDMETHODCALLTYPE HW_IInternetProtocol::CombineUrl(LPCWSTR pwzBaseUrl, LPCWSTR pwzRelativeUrl,
    DWORD dwCombineFlags, LPWSTR pwzResult, DWORD cchResult, DWORD *pcchResult,
    DWORD dwReserved)
{
    if (pwzBaseUrl == NULL || pwzRelativeUrl == NULL || pwzResult == NULL || pcchResult == NULL)
        return E_POINTER;

    *pcchResult = cchResult;
    BOOL ret = InternetCombineUrlW(pwzBaseUrl, pwzRelativeUrl, pwzResult, pcchResult, ICU_NO_ENCODE | ICU_NO_META);
    if (!ret)
    {
        DWORD err = GetLastError();
        if (err == ERROR_INSUFFICIENT_BUFFER)
            return S_FALSE; // Buffer too small
        else
            return E_FAIL;
    }
    return S_OK;
}

static LONG internetProtocolFactoryServerLocks = 0;

class HW_IInternetProtocolFactory : public IClassFactory
{
protected:
    virtual ~HW_IInternetProtocolFactory() { }

public:
    HW_IInternetProtocolFactory() : refCount(1) { }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject);
    ULONG STDMETHODCALLTYPE AddRef() { return ++refCount; }
    ULONG STDMETHODCALLTYPE Release();

    // IClassFactory
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject);
    HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock);

protected:
    int refCount;
};

ULONG STDMETHODCALLTYPE HW_IInternetProtocolFactory::Release()
{
    if (--refCount != 0)
        return refCount;
    delete this;
    return 0;
}

STDMETHODIMP HW_IInternetProtocolFactory::QueryInterface(REFIID riid, void **ppvObject)
{
    *ppvObject = NULL;
    if (riid == IID_IUnknown)
        *ppvObject = static_cast<IUnknown*>(this);
    else if (riid == IID_IClassFactory)
        *ppvObject = static_cast<IClassFactory*>(this);
    if (*ppvObject)
    {
        static_cast<IUnknown*>(*ppvObject)->AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE HW_IInternetProtocolFactory::LockServer(BOOL fLock)
{
    if (fLock)
        InterlockedIncrement(&internetProtocolFactoryServerLocks);
    else
        InterlockedDecrement(&internetProtocolFactoryServerLocks);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HW_IInternetProtocolFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
{
    if (pUnkOuter != NULL)
        return CLASS_E_NOAGGREGATION;
    HW_IInternetProtocol *proto = new HW_IInternetProtocol();
    HRESULT hr = proto->QueryInterface(riid, (void**) ppvObject);
    proto->Release();
    return hr;
}

// Register our protocol so that urlmon will call us for every
// url that starts with WH_PROTO_PREFIX
void RegisterInternetProtocolFactory()
{
    static bool initialized = false;
    if (initialized)
        return;

    ScopedComPtr<IInternetSession> internetSession;
    HRESULT hr = CoInternetGetSession(0, &internetSession, 0);
    if (FAILED(hr))
        return;
    ScopedComPtr<HW_IInternetProtocolFactory> factory(new HW_IInternetProtocolFactory());
    internetSession->RegisterNameSpace(factory, CLSID_HW_IInternetProtocol, WH_PROTO_PREFIX, 0, NULL, 0);
    initialized = true;
}

class HW_IOleInPlaceFrame : public IOleInPlaceFrame
{
public:
    HW_IOleInPlaceFrame(FrameSite* fs) : fs(fs)
    {
    }
    ~HW_IOleInPlaceFrame() {}

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }

    // IOleWindow
    STDMETHODIMP GetWindow(HWND*);
    STDMETHODIMP ContextSensitiveHelp(BOOL) { return S_OK; }

    // IOleInPlaceUIWindow
    STDMETHODIMP GetBorder(LPRECT);
    STDMETHODIMP RequestBorderSpace(LPCBORDERWIDTHS);
    STDMETHODIMP SetBorderSpace(LPCBORDERWIDTHS) { return S_OK; }
    STDMETHODIMP SetActiveObject(IOleInPlaceActiveObject*, LPCOLESTR) { return S_OK; }

    // IOleInPlaceFrame
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

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }

    // IOleWindow
    STDMETHODIMP GetWindow(HWND* h)
    { return fs->oleInPlaceFrame->GetWindow(h); }
    STDMETHODIMP ContextSensitiveHelp(BOOL b)
    { return fs->oleInPlaceFrame->ContextSensitiveHelp(b); }

    // IOleInPlaceSite
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

    // IOleInPlaceSiteEx
    STDMETHODIMP OnInPlaceActivateEx(BOOL*, DWORD);
    STDMETHODIMP OnInPlaceDeactivateEx(BOOL) { return S_OK; }
    STDMETHODIMP RequestUIActivate() { return S_FALSE; }

    // IOleInPlaceSiteWindowless
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

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }

    // IOleClientSite
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

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }

    // IOleControlSite
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

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }

    // IOleCommandTarget
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

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }

    // IParseDisplayName
    STDMETHODIMP ParseDisplayName(IBindCtx*, LPOLESTR, ULONG*, IMoniker**) { return E_NOTIMPL; }

    // IOleContainer
    STDMETHODIMP EnumObjects(DWORD, IEnumUnknown**) { return E_NOTIMPL; }
    STDMETHODIMP LockContainer(BOOL) { return S_OK; }

    // IOleItemContainer
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

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }

    // IDispatch
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

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }

    // IAdviseSink
    void STDMETHODCALLTYPE OnDataChange(FORMATETC*, STGMEDIUM*) { }
    void STDMETHODCALLTYPE OnViewChange(DWORD, LONG) {
        // redraw the control
        fs->oleInPlaceSiteWindowless->InvalidateRect(NULL, FALSE);
    }
    void STDMETHODCALLTYPE OnRename(IMoniker*) { }
    void STDMETHODCALLTYPE OnSave() { }
    void STDMETHODCALLTYPE OnClose() { }

    // IAdviseSink2
    void STDMETHODCALLTYPE OnLinkSrcChange(IMoniker*) { }

    // IAdviseSinkEx
    void STDMETHODCALLTYPE OnViewStatusChange(DWORD) { }
};

class HW_IServiceProvider : public IServiceProvider
{
    FrameSite * fs;
public:
    HW_IServiceProvider(FrameSite* fs) : fs(fs) { }
    ~HW_IServiceProvider() {}

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }

    // IServiceProvider
    STDMETHODIMP QueryService(REFGUID guidService, REFIID riid, void **ppv) {
        if (guidService != SID_SInternetSecurityManager)
            return E_UNEXPECTED;

        if (riid != IID_IInternetSecurityManager)
            return E_NOINTERFACE;

        return fs->QueryInterface(riid, ppv);
    }
};

// In theory we should be able to change the trust for its:: urls via MapUrlToZone()
// and ProcessUrlAction() (see e.g. http://codesearch.google.com/#nJHaZQ1IJ84/trunk/gears/ui/ie/html_dialog_host.cc)
// so that we can read chm files from network drives but they don't even seem to be
// reaching here if chm file is on network drive (but do reach if it's on regular drive).
// Do I have to RegisterNameSpace() with urlmon.dll as in wxWebViewIE::RegisterHandler in
// http://trac.wxwidgets.org/browser/wxWidgets/trunk/src/msw/webview_ie.cpp ?
// Or maybe we should implement reading of chm files ourselves instead of relying
// on IE to load them, as seem to be done in wxHtmlWindow::LoadPage() in
// http://trac.wxwidgets.org/browser/wxWidgets/trunk/src/html/htmlwin.cpp
// combined with CHMFSHandler() in xchm's CHMFSHandler.cpp
class HW_IInternetSecurityManager : public IInternetSecurityManager
{
    FrameSite * fs;
public:
    HW_IInternetSecurityManager(FrameSite* fs) : fs(fs) { }
    ~HW_IInternetSecurityManager() {}

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }

    // IInternetSecurityManager
    STDMETHODIMP GetSecurityId(LPCWSTR pwszUrl, BYTE *pbSecurityId, DWORD *pcbSecurityId, DWORD_PTR dwReserved) {
        DBG_OUT("GetSecurityId()");
        if (str::StartsWith(pwszUrl, L"its")) {
            return INET_E_DEFAULT_ACTION;
        }
        return INET_E_DEFAULT_ACTION;
    }

    STDMETHODIMP GetSecuritySite(IInternetSecurityMgrSite **ppSite) {
        DBG_OUT("GetSecuritySite()");
        return INET_E_DEFAULT_ACTION;
    }

    STDMETHODIMP GetZoneMappings(DWORD dwZone, IEnumString **ppenumString, DWORD dwFlags) {
        DBG_OUT("GetZoneMappings()");
        return INET_E_DEFAULT_ACTION;
    }

    STDMETHODIMP MapUrlToZone(LPCWSTR pwszUrl, DWORD *pdwZone, DWORD dwFlags) {
        DBG_OUT("MapUrlToZone()");
        if (str::StartsWith(pwszUrl, L"its")) {
            return INET_E_DEFAULT_ACTION;
        }
        return INET_E_DEFAULT_ACTION;
    }

    STDMETHODIMP ProcessUrlAction(LPCWSTR pwszUrl, DWORD dwAction, BYTE *pPolicy, DWORD cbPolicy, BYTE *pContext, DWORD cbContext, DWORD dwFlags, DWORD dwReserved) {
        if (cbPolicy != 4) 
            return INET_E_DEFAULT_ACTION;
        if (str::StartsWith(pwszUrl, L"its")) {
            *pPolicy = URLPOLICY_ALLOW;
            return S_OK;
        }
        return INET_E_DEFAULT_ACTION;
    }
    STDMETHODIMP QueryCustomPolicy(LPCWSTR pwszUrl, REFGUID guidKey, BYTE **ppPolicy, DWORD *pcbPolicy, BYTE *pContext, DWORD cbContext, DWORD dwReserved) {
        DBG_OUT("QueryCustomPolicy()");
        if (str::StartsWith(pwszUrl, L"its")) {
            return INET_E_DEFAULT_ACTION;
        }
        return INET_E_DEFAULT_ACTION;
    }

    STDMETHODIMP SetSecuritySite(IInternetSecurityMgrSite *pSite) {
        DBG_OUT("SetSecuritySite()");
        return INET_E_DEFAULT_ACTION;
    }

    STDMETHODIMP SetZoneMapping(DWORD dwZone, LPCWSTR lpszPattern, DWORD dwFlags) {
        DBG_OUT("SetZoneMapping()");
        return INET_E_DEFAULT_ACTION;
    }
};

class HW_IDocHostUIHandler : public IDocHostUIHandler
{
    FrameSite * fs;
public:
    HW_IDocHostUIHandler(FrameSite* fs) : fs(fs) { }
    ~HW_IDocHostUIHandler() {}

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }

    // IDocHostUIHandler
    STDMETHODIMP ShowContextMenu(DWORD dwID, POINT *ppt, IUnknown *pcmdtReserved, IDispatch *pdispReserved) { return S_FALSE; }
    STDMETHODIMP GetHostInfo(DOCHOSTUIINFO *pInfo) { return E_NOTIMPL; }
    STDMETHODIMP ShowUI(DWORD dwID, IOleInPlaceActiveObject *pActiveObject, IOleCommandTarget *pCommandTarget, IOleInPlaceFrame *pFrame, IOleInPlaceUIWindow *pDoc) { return S_FALSE; }
    STDMETHODIMP HideUI(void) { return E_NOTIMPL; }
    STDMETHODIMP UpdateUI(void) { return E_NOTIMPL; }
    STDMETHODIMP EnableModeless(BOOL fEnable) { return E_NOTIMPL; }
    STDMETHODIMP OnDocWindowActivate(BOOL fActivate) { return E_NOTIMPL; }
    STDMETHODIMP OnFrameWindowActivate(BOOL fActivate) { return E_NOTIMPL; }
    STDMETHODIMP ResizeBorder(LPCRECT prcBorder, IOleInPlaceUIWindow *pUIWindow, BOOL fRameWindow) { return E_NOTIMPL; }
    STDMETHODIMP TranslateAccelerator(LPMSG lpMsg, const GUID *pguidCmdGroup, DWORD nCmdID) { return S_FALSE; }
    STDMETHODIMP GetOptionKeyPath(LPOLESTR *pchKey, DWORD dw) { return S_FALSE; }
    STDMETHODIMP GetDropTarget(IDropTarget *pDropTarget, IDropTarget **ppDropTarget) { return fs->QueryInterface(IID_PPV_ARGS(ppDropTarget)); }
    STDMETHODIMP GetExternal(IDispatch **ppDispatch) { if (ppDispatch) *ppDispatch = NULL; return S_FALSE; }
    STDMETHODIMP TranslateUrl(DWORD dwTranslate, OLECHAR *pchURLIn, OLECHAR **ppchURLOut) { return S_FALSE; }
    STDMETHODIMP FilterDataObject(IDataObject *pDO, IDataObject **ppDORet) { if (ppDORet) *ppDORet = NULL; return S_FALSE; }
};

class HW_IDropTarget : public IDropTarget
{
    FrameSite * fs;
public:
    HW_IDropTarget(FrameSite* fs) : fs(fs) { }
    ~HW_IDropTarget() {}

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject) { return fs->QueryInterface(iid, ppvObject); }
    ULONG STDMETHODCALLTYPE AddRef() { return fs->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return fs->Release(); }

    STDMETHODIMP DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) {
        HRESULT hr = fs->htmlWindow->OnDragEnter(pDataObj);
        if (SUCCEEDED(hr))
            *pdwEffect = DROPEFFECT_COPY;
        return hr;
    }
    STDMETHODIMP DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) {
        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }
    STDMETHODIMP DragLeave(void) { return S_OK; }
    STDMETHODIMP Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) {
        return fs->htmlWindow->OnDragDrop(pDataObj);
    }
};

class HtmlMoniker : public IMoniker
{
public:
    HtmlMoniker();
    virtual ~HtmlMoniker();

    HRESULT SetHtml(const char *s, size_t len);
    HRESULT SetBaseUrl(const TCHAR *baseUrl, size_t baseUrlLen);

public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IMoniker
    STDMETHODIMP BindToStorage(IBindCtx *pbc, IMoniker *pmkToLeft, REFIID riid, void **ppvObj);
    STDMETHODIMP GetDisplayName(IBindCtx *pbc, IMoniker *pmkToLeft, LPOLESTR *ppszDisplayName);
    STDMETHODIMP BindToObject(IBindCtx *pbc, IMoniker *pmkToLeft, REFIID riidResult, void **ppvResult) { return E_NOTIMPL; }
    STDMETHODIMP Reduce(IBindCtx *pbc, DWORD dwReduceHowFar, IMoniker **ppmkToLeft, IMoniker **ppmkReduced) { return E_NOTIMPL; }
    STDMETHODIMP ComposeWith(IMoniker *pmkRight, BOOL fOnlyIfNotGeneric, IMoniker **ppmkComposite) { return E_NOTIMPL; }
    STDMETHODIMP Enum(BOOL fForward, IEnumMoniker **ppenumMoniker) { return E_NOTIMPL; }
    STDMETHODIMP IsEqual(IMoniker *pmkOtherMoniker) { return E_NOTIMPL; }
    STDMETHODIMP Hash(DWORD *pdwHash) { return E_NOTIMPL; }
    STDMETHODIMP IsRunning(IBindCtx *pbc, IMoniker *pmkToLeft, IMoniker *pmkNewlyRunning) { return E_NOTIMPL; }
    STDMETHODIMP GetTimeOfLastChange(IBindCtx *pbc, IMoniker *pmkToLeft, FILETIME *pFileTime) { return E_NOTIMPL; }
    STDMETHODIMP Inverse(IMoniker **ppmk) { return E_NOTIMPL; }
    STDMETHODIMP CommonPrefixWith(IMoniker *pmkOther, IMoniker **ppmkPrefix) { return E_NOTIMPL; }
    STDMETHODIMP RelativePathTo(IMoniker *pmkOther, IMoniker **ppmkRelPath) { return E_NOTIMPL; }
    STDMETHODIMP ParseDisplayName(IBindCtx *pbc, IMoniker *pmkToLeft,LPOLESTR pszDisplayName,
        ULONG *pchEaten, IMoniker **ppmkOut)
    { return E_NOTIMPL; }
    STDMETHODIMP IsSystemMoniker(DWORD *pdwMksys) {
        if (!pdwMksys)
            return E_POINTER;
        *pdwMksys = MKSYS_NONE;
        return S_OK;
    }

    // IPersistStream methods
    STDMETHODIMP Save(IStream *pStm, BOOL fClearDirty)  { return E_NOTIMPL; }
    STDMETHODIMP IsDirty() { return E_NOTIMPL; }
    STDMETHODIMP Load(IStream *pStm) { return E_NOTIMPL; }
    STDMETHODIMP GetSizeMax(ULARGE_INTEGER *pcbSize) { return E_NOTIMPL; }

    // IPersist
    STDMETHODIMP GetClassID(CLSID *pClassID) { return E_NOTIMPL; }

private:
    int         refCount;

    char *      htmlData;
    IStream *   htmlStream;

    TCHAR *     baseUrl;
};

HtmlMoniker::HtmlMoniker()
    : refCount(1),
      htmlData(NULL),
      htmlStream(NULL),
      baseUrl(NULL)
{
}

HtmlMoniker::~HtmlMoniker()
{
    if (htmlStream)
        htmlStream->Release();

    free(htmlData);
    free(baseUrl);
}

HRESULT HtmlMoniker::SetHtml(const char *s, size_t len)
{
    free(htmlData);
    htmlData = str::DupN(s, len);
    if (htmlStream)
        htmlStream->Release();
    // TODO: SHCreateMemStream() might be faster
    htmlStream = CreateStreamFromData(htmlData, len);
    return S_OK;
}

HRESULT HtmlMoniker::SetBaseUrl(const TCHAR *newBaseUrl, size_t newBaseUrlLen)
{
    free(baseUrl);
    baseUrl = str::DupN(newBaseUrl, newBaseUrlLen);
    return S_OK;
}

STDMETHODIMP HtmlMoniker::BindToStorage(IBindCtx *pbc, IMoniker *pmkToLeft, REFIID riid, void **ppvObj)
{
    LARGE_INTEGER seek = {0};
    htmlStream->Seek(seek, STREAM_SEEK_SET, NULL);
    return htmlStream->QueryInterface(riid, ppvObj);
}

static LPOLESTR OleStrDup(TCHAR *s)
{
    size_t cb = sizeof(TCHAR) * (str::Len(s) + 1);
    LPOLESTR ret = (LPOLESTR)CoTaskMemAlloc(cb);
    memcpy(ret, s, cb);
    return ret;
}

STDMETHODIMP HtmlMoniker::GetDisplayName(IBindCtx *pbc, IMoniker *pmkToLeft, LPOLESTR *ppszDisplayName)
{
    if (!ppszDisplayName)
        return E_POINTER;
    if (baseUrl)
        *ppszDisplayName = OleStrDup(baseUrl);
    else
        *ppszDisplayName = OleStrDup(_T(""));
    return S_OK;
}

STDMETHODIMP HtmlMoniker::QueryInterface(REFIID riid, void **ppvObject)
{
    *ppvObject = NULL;
    if (riid == IID_IUnknown)
        *ppvObject = static_cast<IUnknown*>(this);
    else if (riid == IID_IMoniker)
        *ppvObject = static_cast<IMoniker*>(this);
    if (*ppvObject)
    {
        ((IUnknown*)*ppvObject)->AddRef();
        return S_OK;
    }
    else return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE HtmlMoniker::AddRef()
{
    return refCount++;
}

ULONG STDMETHODCALLTYPE HtmlMoniker::Release()
{
    if (--refCount != 0)
        return refCount;
    delete this;
    return 0;
}

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
                win->OnSize(SizeI(LOWORD(lParam), HIWORD(lParam)));
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

        case WM_DROPFILES:
            return CallWindowProc(win->wndProcBrowserPrev, hwnd, msg, wParam, lParam);

        case WM_VSCROLL:
            win->SendMsg(msg, wParam, lParam);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void HtmlWindow::SubclassHwnd()
{
    wndProcBrowserPrev = (WNDPROC)SetWindowLongPtr(hwndParent, GWLP_WNDPROC, (LONG_PTR)WndProcParent);
    SetWindowLongPtr(hwndParent, GWLP_USERDATA, (LONG_PTR)this);
}

void HtmlWindow::UnsubclassHwnd()
{
    SetWindowLongPtr(hwndParent, GWLP_WNDPROC, (LONG_PTR)wndProcBrowserPrev);
    SetWindowLongPtr(hwndParent, GWLP_USERDATA, (LONG_PTR)0);
}

HtmlWindow::HtmlWindow(HWND hwndParent, HtmlWindowCallback *cb) :
    hwndParent(hwndParent), webBrowser(NULL), oleObject(NULL),
    oleInPlaceObject(NULL), viewObject(NULL),
    connectionPoint(NULL), htmlContent(NULL), oleObjectHwnd(NULL),
    adviseCookie(0), blankWasShown(false), htmlWinCb(cb),
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

    webBrowser->put_RegisterAsBrowser(VARIANT_FALSE);
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

    if (viewObject)
        viewObject->Release();

    if (htmlContent)
        htmlContent->Release();

    if (webBrowser)
        webBrowser->Release();
}

void HtmlWindow::OnSize(SizeI size)
{
    if (webBrowser) {
        webBrowser->put_Width(size.dx);
        webBrowser->put_Height(size.dy);
    }

    if (oleInPlaceObject) {
        RECT r = RectI(PointI(), size).ToRECT();
        oleInPlaceObject->SetObjectRects(&r, &r);
    }
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
        webBrowser->put_Visible(visible ? VARIANT_TRUE : VARIANT_FALSE);
}

void HtmlWindow::NavigateToUrl(const TCHAR *urlStr)
{
    VARIANT url;
    VariantInit(&url);
    url.vt = VT_BSTR;
    url.bstrVal = SysAllocString(AsWStrQ(urlStr));
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
    if (blankWasShown)
        return;
    NavigateToUrl(_T("about:blank"));
    WaitUntilLoaded(INFINITE, _T("about:blank"));
    blankWasShown = true;
}

static SAFEARRAY *CreateOneElementWstrArray(const WCHAR *s)
{
    SAFEARRAY *arr = SafeArrayCreateVector(VT_VARIANT, 0, 1);
    VARIANT *var = NULL;
    SafeArrayAccessData(arr, (void**)&var);
    var->vt = VT_BSTR;
    var->bstrVal = SysAllocString(s);
    SafeArrayUnaccessData(arr);
    if (!var->bstrVal) {
        SafeArrayDestroy(arr);
        return NULL;
    }
    return arr;
}

// TODO: this looses the encoding of the file. Maybe I can use Load method
// instead as shown here:
// http://stackoverflow.com/questions/724746/problem-with-ihtmldocument2write
static SAFEARRAY *CreateOneElementStrArray(const char *s)
{
    ScopedMem<WCHAR> tmp(str::conv::FromAnsi(s));
    return CreateOneElementWstrArray(tmp);
}

void HtmlWindow::WriteHtml(SAFEARRAY* htmlArr)
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

    doc->write(htmlArr);
    doc->close();
}

void HtmlWindow::DisplayHtml(const WCHAR *html)
{
    SAFEARRAY *arr = CreateOneElementWstrArray(html);
    WriteHtml(arr);
    SafeArrayDestroy(arr);
}

void HtmlWindow::DisplayHtml(const char *html)
{
    SAFEARRAY *arr = CreateOneElementStrArray(html);
    WriteHtml(arr);
    SafeArrayDestroy(arr);
}

#define ABOUT_BLANK _T("about:blank")
#define ABOUT_BLANK_LEN (dimof(ABOUT_BLANK)-1)

void HtmlWindow::SetHtml(const char *s, size_t len)
{
    if (-1 == len)
        len = str::Len(s);

    if (htmlContent)
        htmlContent->Release();
    htmlContent = new HtmlMoniker();
    htmlContent->SetHtml(s, len);

    ScopedComPtr<IDispatch> docDispatch;
    HRESULT hr = webBrowser->get_Document(&docDispatch);
    if (FAILED(hr) || !docDispatch)
        return;

    ScopedComQIPtr<IHTMLDocument2> doc(docDispatch);
    if (!doc)
        return;

    ScopedComQIPtr<IPersistMoniker> perstMon(doc);
    if (!perstMon)
        return;

    ScopedComQIPtr<IMoniker> htmlMon(htmlContent);
    hr = perstMon->Load(TRUE, htmlMon, NULL, STGM_READ);
    // this might fail if HtmlMoniker::GetDisplayName() returns bad base url
    // (and apparently empty string is considered bad)
    // so set it to "about:blank" (which works) and retry
    // TODO: possibly HtmlMoniker::GetDisplayName() should just return "about:blank"
    // if none has been set
    if (FAILED(hr))
    {
        htmlContent->SetBaseUrl(ABOUT_BLANK, ABOUT_BLANK_LEN);
        hr = perstMon->Load(TRUE, htmlMon, NULL, STGM_READ);
        assert(!FAILED(hr));
    }
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
    if (!htmlWinCb)
        return true;
    bool shouldNavigate = htmlWinCb->OnBeforeNavigate(url, newWindow);
    if (!shouldNavigate)
        return false;
    char *data = NULL;
    size_t len = 0;
    bool gotHtmlData = htmlWinCb->GetHtmlForUrl(url, &data, &len);
    if (gotHtmlData) {
        SetHtml(data, len);
        return false;
    }
    return true;
}

void HtmlWindow::OnDocumentComplete(const TCHAR *url)
{
    currentURL.Set(str::Dup(url));
}

HRESULT HtmlWindow::OnDragEnter(IDataObject *dataObj)
{
    ScopedComQIPtr<IDataObject> data(dataObj);
    if (!data)
        return E_INVALIDARG;
    FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stg = { 0 };
    if (FAILED(data->GetData(&fe, &stg)))
        return E_FAIL;
    ReleaseStgMedium(&stg);
    return S_OK;
}

HRESULT HtmlWindow::OnDragDrop(IDataObject *dataObj)
{
    ScopedComQIPtr<IDataObject> data(dataObj);
    if (!data)
        return E_INVALIDARG;
    FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stg = { 0 };
    if (FAILED(data->GetData(&fe, &stg)))
        return E_FAIL;

    HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
    if (hDrop) {
        SendMessage(hwndParent, WM_DROPFILES, (WPARAM)hDrop, 0);
        GlobalUnlock(stg.hGlobal);
    }
    ReleaseStgMedium(&stg);
    return hDrop != NULL ? S_OK : E_FAIL;
}

// Just to be safe, we use Interlocked*() functions
// to maintain pumpNestCount
static LONG pumpNestCount = 0;

static void PumpRemainingMessages()
{
    MSG msg;
    InterlockedIncrement(&pumpNestCount);
    for (;;) {
        bool moreMessages = PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);
        if (!moreMessages)
            goto Exit;
        GetMessage(&msg, NULL, 0, 0);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
Exit:
    InterlockedDecrement(&pumpNestCount);
}

// TODO: this is a terrible hack. When we're processing messages
// with the intention of advancing browser window, we might process
// a message that will cause us to close the chm document and related
// classes while we're still using them, so we use this function
// to block those cases.
// The right fix is to move to truly async processing where instead
// of busy-waiting for html loading ot finish, we schedule the
// remaining of the code to be executed on document loaded
// notification/callback
bool InHtmlNestedMessagePump()
{
    return pumpNestCount > 0;
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
    while (!currentURL && (timer.GetCurrTimeInMs() < maxWaitMs)) {
        PumpRemainingMessages();
        Sleep(100);
    }
    return LoadedExpectedPage(url, currentURL);
}

FrameSite::FrameSite(HtmlWindow * win)
{
    refCount = 1;

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

    oleInPlaceFrame             = new HW_IOleInPlaceFrame(this);
    oleInPlaceSiteWindowless    = new HW_IOleInPlaceSiteWindowless(this);
    oleClientSite               = new HW_IOleClientSite(this);
    oleControlSite              = new HW_IOleControlSite(this);
    oleCommandTarget            = new HW_IOleCommandTarget(this);
    oleItemContainer            = new HW_IOleItemContainer(this);
    hwDWebBrowserEvents2        = new HW_DWebBrowserEvents2(this);
    adviseSink2                 = new HW_IAdviseSink2(this);
    serviceProvider             = new HW_IServiceProvider(this);
    internetSecurityManager     = new HW_IInternetSecurityManager(this);
    docHostUIHandler            = new HW_IDocHostUIHandler(this);
    dropTarget                  = new HW_IDropTarget(this);
}

FrameSite::~FrameSite()
{
    delete dropTarget;
    delete docHostUIHandler;
    delete internetSecurityManager;
    delete serviceProvider;
    delete adviseSink2;
    delete hwDWebBrowserEvents2;
    delete oleItemContainer;
    delete oleCommandTarget;
    delete oleControlSite;
    delete oleClientSite;
    delete oleInPlaceSiteWindowless;
    delete oleInPlaceFrame;
}

// IUnknown
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
    else if (riid == IID_IServiceProvider)
        *ppv = serviceProvider;
    else if (riid == IID_IInternetSecurityManager)
        *ppv = internetSecurityManager;
    else if (riid == IID_IDocHostUIHandler)
        *ppv = docHostUIHandler;
    else if (riid == IID_IDropTarget)
        *ppv = dropTarget;

    if (*ppv == NULL)
        return E_NOINTERFACE;
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) FrameSite::AddRef()
{
    return ++refCount;
}

STDMETHODIMP_(ULONG) FrameSite::Release()
{
    assert(refCount > 0);
    if (--refCount != 0)
        return refCount;
    delete this;
    return 0;
}

// IDispatch
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
            bool shouldCancel = !fs->htmlWindow->OnBeforeNavigate(AsTStrQ(url), false);
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
            fs->htmlWindow->OnDocumentComplete(AsTStrQ(url));
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
            bool shouldCancel = !fs->htmlWindow->OnBeforeNavigate(AsTStrQ(url), true);
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

// IOleWindow
HRESULT HW_IOleInPlaceFrame::GetWindow(HWND *phwnd)
{
    if (phwnd == NULL)
        return E_INVALIDARG;
    *phwnd = fs->hwndParent;
    return S_OK;
}

// IOleInPlaceUIWindow
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

// IOleInPlaceSite
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

// IOleInPlaceSiteEx
HRESULT HW_IOleInPlaceSiteWindowless::OnInPlaceActivateEx(BOOL * pfNoRedraw, DWORD dwFlags)
{
    if (pfNoRedraw)
        *pfNoRedraw = FALSE;
    return S_OK;
}

// IOleInPlaceSiteWindowless
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

// IOleClientSite
HRESULT HW_IOleClientSite::GetContainer(LPOLECONTAINER * ppContainer)
{
    if (ppContainer == NULL)
        return E_INVALIDARG;
    return QueryInterface(IID_IOleContainer, (void**)ppContainer);
}

// IOleItemContainer
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

// IOleControlSite
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

// IOleCommandTarget
HRESULT HW_IOleCommandTarget::QueryStatus(const GUID *pguidCmdGroup, 
    ULONG cCmds, OLECMD *prgCmds, OLECMDTEXT *pCmdTet)
{
    if (prgCmds == NULL)
        return E_INVALIDARG;
    return OLECMDERR_E_UNKNOWNGROUP;
}
