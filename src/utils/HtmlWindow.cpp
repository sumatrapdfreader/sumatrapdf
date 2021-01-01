/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "HtmlWindow.h"
#include <mshtml.h>
#include <mshtmhst.h>
#include <oaidl.h>
#include <exdispid.h>
#include "Timer.h"
#include "ScopedWin.h"
#include "WinUtil.h"
#include "GdiPlusUtil.h"

// An important (to Sumatra) use case is displaying CHM documents. First we used
// IE's built-in support form CHM documents (using its: protocol
// http://msdn.microsoft.com/en-us/library/aa164814(v=office.10).aspx).
// However, that doesn't work for CHM documents from network drives
// (http://code.google.com/p/sumatrapdf/issues/detail?id=1706)
// To solve that we ended up the following solution:
// * an app can provide html as data in memory. We write the data using custom
//   IMoniker implementation with IE's IPersistentMoniker::Load() function.
//   This allows us to provide base url which will be used to resolve relative
//   links within the html (e.g. to embedded images in <img> tags etc.)
// * We register application-global protocol handler and provide custom IInternetProtocol
//   implementation which is called to handle getting content for URLs in that namespace.
//   I've decided to over-ride its: protocol for our needs. A protocol unique to our
//   code would be better, but completely new protocol don't seem to work with
//   IPersistentMoniker::Load() code (I can see our IMoniker::GetDisplayName() (which
//   returns the base url) called twice from mshtml/ieframe code but if the returned
//   base url doesn't start with protocol that IE already understands, IPersistentMoniker::Load()
//   fails) so I was forced to over-ride existing protocol name.
//
// I also tried the approach of implementing IInternetSecurityManager thinking that I can just
// use built-in its: handling and tell IE to trust those links, but it seems that in case
// of its: links for CHM files from network drives, that code isn't even reached.

// Implementing scrolling:
// Currently we implement scrolling by sending messages simulating user input
// to the browser control window that is responsible for processing those messages.
// It has a benefit of being simple to implement and matching ie's behavior closely.

// Another option would be to provide scrolling functions to be called by callers
// (e.g. from FrameOnKeydow()) by querying scroll state from IHTMLElement2 and setting
// a new scroll state http://www.codeproject.com/KB/miscctrl/scrollbrowser.aspx
// or using scrollTo() or scrollBy() on IHTMLWindow2:
// http://msdn.microsoft.com/en-us/library/aa741497(v=VS.85).aspx

// The more advanced ways of interacting with mshtml/ieframe are extremely poorly
// documented so I mostly puzzled it out based on existing open source code that
// does similar things. Some useful resources:

// Book on ATL: http://369o.com/data/books/atl/index.html, which is
// helpful in understanding basics of COM, has chapter on basics of embedding IE.

// http://www.codeproject.com/KB/COM/cwebpage.aspx

// This code is structured in a similar way as wxWindows'
// browser wrapper
// https://github.com/Aegisub/traydict/blob/master/IEHtmlWin.h
// https://github.com/Aegisub/traydict/blob/master/IEHtmlWin.cpp

// Info about IInternetProtocol: http://www.codeproject.com/KB/IP/DataProtocol.aspx

// All the ways to load html into mshtml:
// http://qualapps.blogspot.com/2008/10/how-to-load-mshtml-with-data.html

// how to handle custom protocol like myapp://
// http://www.nuonsoft.com/blog/2010/04/05/how-to-handle-custom-url-protocols-with-the-microsoft-webbrowser-control/
// http://www.nuonsoft.com/blog/2010/04/05/how-to-navigate-to-an-anchor-in-the-microsoft-webbrowser-control-when-rendering-html-from-memory/
// http://www.nuonsoft.com/blog/2010/03/24/how-to-use-the-microsoft-webbrowser-control-to-render-html-from-memory/

// http://geekswithblogs.net/dotnetnomad/archive/2008/01/29/119065.aspx

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
// http://www.codeproject.com/Articles/3365/Embed-an-HTML-control-in-your-own-window-using-pla
// http://www.codeproject.com/Articles/642/Processing-HTML-Forms-From-a-CHtmlView
// https://github.com/salsita/ProtocolLibrary
// https://github.com/salsita/libbhohelper
// https://github.com/salsita/libprothandlers
// http://www.codeproject.com/Articles/1094/The-MFC-CDHtmlDialog-class#xx568889xx
// http://www.codeproject.com/Articles/642/Processing-HTML-Forms-From-a-CHtmlView
// http://www.codeproject.com/Articles/10401/Handling-HTML-Element-Events-in-CHtmlView-and-Reus
// http://www.codeproject.com/Articles/3919/Using-the-WebBrowser-control-simplified

// https://groups.google.com/forum/#!topic/microsoft.public.inetsdk.programming.mshtml_hosting/jogeNC4NXzU
// https://blog.javascripting.com/2012/11/07/igor-tandetniks-passthrough-app-now-on-github/

// Series of articles:
// http://starkravingfinkle.org/blog/2005/04/mshtml-hosting-drawing-on-webbrowser/
// http://starkravingfinkle.org/blog/2005/04/mshtml-hosting-more-tricks/
// http://starkravingfinkle.org/blog/2005/02/mshtml-hosting-editing-tricks/
// http://starkravingfinkle.org/blog/2005/01/mshtml-hosting-more-editing/
// http://starkravingfinkle.org/blog/2004/12/mshtml-hosting-calling-javascript-from-host/
// http://starkravingfinkle.org/blog/2004/10/mshtml-hosting-editing/
// http://starkravingfinkle.org/blog/2004/10/mshtml-hosting-idochostuihandler/
// http://starkravingfinkle.org/blog/2004/09/mshtml-hosting-odds-ends/
// http://starkravingfinkle.org/blog/2004/09/mshtml-hosting-building-uis/
// http://starkravingfinkle.org/blog/2004/08/mshtml-hosting-the-basics/

class HW_IOleInPlaceFrame;
class HW_IOleInPlaceSiteWindowless;
class HW_IOleClientSite;
class HW_IOleControlSite;
class HW_IOleCommandTarget;
class HW_IOleItemContainer;
class HW_DWebBrowserEvents2;
class HW_IAdviseSink2;
class HW_IDocHostUIHandler;
class HW_IDropTarget;
class HW_IServiceProvider;

inline void VariantSetBool(VARIANT* res, bool val) {
    res->vt = VT_BOOL;
    res->boolVal = val ? VARIANT_TRUE : VARIANT_FALSE;
}

inline void VariantSetLong(VARIANT* res, long val) {
    res->vt = VT_I4;
    res->lVal = val;
}

bool IsBlankUrl(const WCHAR* url) {
    return str::EqI(L"about:blank", url);
}

// HW stands for HtmlWindow
// FrameSite ties together HtmlWindow and all the COM interfaces we need to implement
// to support it
class FrameSite : public IUnknown {
    friend class HtmlWindow;
    friend class HW_IOleInPlaceFrame;
    friend class HW_IOleInPlaceSiteWindowless;
    friend class HW_IOleClientSite;
    friend class HW_IOleControlSite;
    friend class HW_IOleCommandTarget;
    friend class HW_IOleItemContainer;
    friend class HW_DWebBrowserEvents2;
    friend class HW_IAdviseSink2;
    friend class HW_IDocHostUIHandler;
    friend class HW_IDropTarget;
    friend class HW_IServiceProvider;

  public:
    explicit FrameSite(HtmlWindow* win);
    ~FrameSite();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject);
    ULONG STDMETHODCALLTYPE AddRef() {
        return InterlockedIncrement(&refCount);
    }
    ULONG STDMETHODCALLTYPE Release();

  protected:
    LONG refCount;

    HW_IOleInPlaceFrame* oleInPlaceFrame;
    HW_IOleInPlaceSiteWindowless* oleInPlaceSiteWindowless;
    HW_IOleClientSite* oleClientSite;
    HW_IOleControlSite* oleControlSite;
    HW_IOleCommandTarget* oleCommandTarget;
    HW_IOleItemContainer* oleItemContainer;
    HW_DWebBrowserEvents2* hwDWebBrowserEvents2;
    HW_IAdviseSink2* adviseSink2;
    HW_IDocHostUIHandler* docHostUIHandler;
    HW_IDropTarget* dropTarget;
    HW_IServiceProvider* serviceProvider;

    HtmlWindow* htmlWindow;

    // HDC m_hDCBuffer;
    HWND hwndParent;

    bool supportsWindowlessActivation;
    bool inPlaceLocked;
    bool inPlaceActive;
    bool uiActive;
    bool isWindowless;

    LCID ambientLocale;
    COLORREF ambientForeColor;
    COLORREF ambientBackColor;
    bool ambientShowHatching;
    bool ambientShowGrabHandles;
    bool ambientUserMode;
    bool ambientAppearance;
};

// For simplicity, we just add to the array. We don't bother
// reclaiming ids for deleted windows. I don't expect number
// of HtmlWindow objects created to be so high as to be problematic
// (1 thousand objects is just 4K of memory for the vector)
static Vec<HtmlWindow*> gHtmlWindows;

HtmlWindow* FindHtmlWindowById(int windowId) {
    return gHtmlWindows.at(windowId);
}

static int GenNewWindowId(HtmlWindow* htmlWin) {
    int newWindowId = (int)gHtmlWindows.size();
    gHtmlWindows.Append(htmlWin);
    CrashIf(htmlWin != FindHtmlWindowById(newWindowId));
    return newWindowId;
}

static void FreeWindowId(int windowId) {
    CrashIf(nullptr == gHtmlWindows.at(windowId));
    gHtmlWindows.at(windowId) = nullptr;
}

// Re-using its protocol, see comments at the top.
#define HW_PROTO_PREFIX L"its"

// {F1EC293F-DBBD-4A4B-94F4-FA52BA0BA6EE}
static const GUID CLSID_HW_IInternetProtocol = {0xf1ec293f,
                                                0xdbbd,
                                                0x4a4b,
                                                {0x94, 0xf4, 0xfa, 0x52, 0xba, 0xb, 0xa6, 0xee}};

class HW_IInternetProtocolInfo : public IInternetProtocolInfo {
  public:
    HW_IInternetProtocolInfo() {
    }

  protected:
    virtual ~HW_IInternetProtocolInfo() {
    }

  public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
    ULONG STDMETHODCALLTYPE AddRef() {
        return InterlockedIncrement(&refCount);
    }
    ULONG STDMETHODCALLTYPE Release();

    // IInternetProtocolInfo
    STDMETHODIMP ParseUrl([[maybe_unused]] LPCWSTR pwzUrl, [[maybe_unused]] PARSEACTION parseAction,
                          [[maybe_unused]] DWORD dwParseFlags, [[maybe_unused]] LPWSTR pwzResult,
                          [[maybe_unused]] DWORD cchResult, [[maybe_unused]] DWORD* pcchResult,
                          [[maybe_unused]] DWORD dwReserved) {
        return INET_E_DEFAULT_ACTION;
    }

    STDMETHODIMP CombineUrl([[maybe_unused]] LPCWSTR pwzBaseUrl, [[maybe_unused]] LPCWSTR pwzRelativeUrl,
                            [[maybe_unused]] DWORD dwCombineFlags, [[maybe_unused]] LPWSTR pwzResult,
                            [[maybe_unused]] DWORD cchResult, [[maybe_unused]] DWORD* pcchResult,
                            [[maybe_unused]] DWORD dwReserved) {
        return INET_E_DEFAULT_ACTION;
    }

    STDMETHODIMP CompareUrl([[maybe_unused]] LPCWSTR pwzUrl1, [[maybe_unused]] LPCWSTR pwzUrl2,
                            [[maybe_unused]] DWORD dwCompareFlags) {
        return INET_E_DEFAULT_ACTION;
    }

    STDMETHODIMP QueryInfo([[maybe_unused]] LPCWSTR pwzUrl, [[maybe_unused]] QUERYOPTION queryOption,
                           [[maybe_unused]] DWORD dwQueryFlags, [[maybe_unused]] LPVOID pBuffer,
                           [[maybe_unused]] DWORD cbBuffer, [[maybe_unused]] DWORD* pcbBuf,
                           [[maybe_unused]] DWORD dwReserved) {
        return INET_E_DEFAULT_ACTION;
    }

  protected:
    LONG refCount{1};
};

ULONG STDMETHODCALLTYPE HW_IInternetProtocolInfo::Release() {
    LONG res = InterlockedDecrement(&refCount);
    CrashIf(res < 0);
    if (0 == res) {
        delete this;
    }
    return res;
}

STDMETHODIMP HW_IInternetProtocolInfo::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {QITABENT(HW_IInternetProtocolInfo, IInternetProtocolInfo), {0}};
    return QISearch(this, qit, riid, ppv);
}

class HW_IInternetProtocol : public IInternetProtocol {
  public:
    HW_IInternetProtocol() {
    }

  protected:
    virtual ~HW_IInternetProtocol() {
    }

  public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
    ULONG STDMETHODCALLTYPE AddRef() {
        return InterlockedIncrement(&refCount);
    }
    ULONG STDMETHODCALLTYPE Release();

    // IInternetProtocol
    STDMETHODIMP Start(LPCWSTR szUrl, IInternetProtocolSink* pIProtSink, IInternetBindInfo* pIBindInfo, DWORD grfSTI,
                       HANDLE_PTR dwReserved);
    STDMETHODIMP Continue([[maybe_unused]] PROTOCOLDATA* pStateInfo) {
        return S_OK;
    }
    STDMETHODIMP Abort([[maybe_unused]] HRESULT hrReason, [[maybe_unused]] DWORD dwOptions) {
        return S_OK;
    }
    STDMETHODIMP Terminate([[maybe_unused]] DWORD dwOptions) {
        return S_OK;
    }
    STDMETHODIMP Suspend() {
        return E_NOTIMPL;
    }
    STDMETHODIMP Resume() {
        return E_NOTIMPL;
    }
    STDMETHODIMP Read(void* pv, ULONG cb, ULONG* pcbRead);
    STDMETHODIMP Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition);
    STDMETHODIMP LockRequest([[maybe_unused]] DWORD dwOptions) {
        return S_OK;
    }
    STDMETHODIMP UnlockRequest() {
        return S_OK;
    }

  protected:
    LONG refCount = 1;

    // those are filled in Start() and represent data to be sent
    // for a given url
    std::span<u8> data{};
    size_t dataCurrPos = 0;
};

ULONG STDMETHODCALLTYPE HW_IInternetProtocol::Release() {
    LONG res = InterlockedDecrement(&refCount);
    CrashIf(res < 0);
    if (0 == res) {
        delete this;
    }
    return res;
}

STDMETHODIMP HW_IInternetProtocol::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {
        QITABENT(HW_IInternetProtocol, IInternetProtocol), QITABENT(HW_IInternetProtocol, IInternetProtocolRoot), {0}};
    return QISearch(this, qit, riid, ppv);
}

// given url in the form "its://$htmlWindowId/$urlRest, parses
// out $htmlWindowId and $urlRest. Returns false if url doesn't conform
// to this pattern.
static bool ParseProtoUrl(const WCHAR* url, int* htmlWindowId, AutoFreeWstr* urlRest) {
    const WCHAR* rest = str::Parse(url, HW_PROTO_PREFIX L"://%d/%S", htmlWindowId, urlRest);
    return rest && !*rest;
}

#define DEFAULT_MIME_TYPE L"text/html"

// caller must free() the result
static WCHAR* MimeFromUrl(const WCHAR* url, const WCHAR* imgExt = nullptr) {
    const WCHAR* ext = str::FindCharLast(url, '.');
    if (!ext) {
        return str::Dup(DEFAULT_MIME_TYPE);
    }

    if (str::FindChar(ext, ';')) {
        // some CHM documents use (image) URLs that are followed by
        // a semi-colon and a number after the file's extension
        AutoFreeWstr newUrl(str::DupN(url, str::FindChar(ext, ';') - url));
        return MimeFromUrl(newUrl, imgExt);
    }

    static const struct {
        const WCHAR* ext;
        const WCHAR* mimetype;
    } mimeTypes[] = {
        {L".html", L"text/html"}, {L".htm", L"text/html"},  {L".gif", L"image/gif"},
        {L".png", L"image/png"},  {L".jpg", L"image/jpeg"}, {L".jpeg", L"image/jpeg"},
        {L".bmp", L"image/bmp"},  {L".css", L"text/css"},   {L".txt", L"text/plain"},
    };

    for (int i = 0; i < dimof(mimeTypes); i++) {
        if (str::EqI(ext, mimeTypes[i].ext)) {
            // trust an image's data more than its extension
            if (imgExt && !str::Eq(imgExt, mimeTypes[i].ext) && str::StartsWith(mimeTypes[i].mimetype, L"image/")) {
                for (int j = 0; j < dimof(mimeTypes); j++) {
                    if (str::Eq(imgExt, mimeTypes[j].ext)) {
                        return str::Dup(mimeTypes[j].mimetype);
                    }
                }
            }
            return str::Dup(mimeTypes[i].mimetype);
        }
    }

    AutoFreeWstr contentType(ReadRegStr(HKEY_CLASSES_ROOT, ext, L"Content Type"));
    if (contentType) {
        return contentType.StealData();
    }

    return str::Dup(DEFAULT_MIME_TYPE);
}

// TODO: return an error page html in case of errors?
STDMETHODIMP HW_IInternetProtocol::Start(LPCWSTR szUrl, IInternetProtocolSink* pIProtSink,
                                         [[maybe_unused]] IInternetBindInfo* pIBindInfo, [[maybe_unused]] DWORD grfSTI,
                                         [[maybe_unused]] HANDLE_PTR dwReserved) {
    // TODO: others seem to return S_OK even if there is no content
    //       for a URL (unless the PI_PARSE_URL bit is set on grfSTI),
    //       this does however lead to this HW_IInternetProtocol being
    //       leaked and to DISPID_DOCUMENTCOMPLETE never being fired

    int htmlWindowId;
    AutoFreeWstr urlRest;
    bool ok = ParseProtoUrl(szUrl, &htmlWindowId, &urlRest);
    if (!ok) {
        return INET_E_INVALID_URL;
    }

    pIProtSink->ReportProgress(BINDSTATUS_FINDINGRESOURCE, urlRest);
    pIProtSink->ReportProgress(BINDSTATUS_CONNECTING, urlRest);
    pIProtSink->ReportProgress(BINDSTATUS_SENDINGREQUEST, urlRest);

    HtmlWindow* win = FindHtmlWindowById(htmlWindowId);
    // TODO: this now happens due to events happening on HtmlWindow
    // used to take a screenshot, so ignore it. Is there a way
    // to cancel things and not get her?
    // CrashIf(!win);
    if (!win) {
        return INET_E_OBJECT_NOT_FOUND;
    }
    if (!win->htmlWinCb) {
        return INET_E_OBJECT_NOT_FOUND;
    }
    data = win->htmlWinCb->GetDataForUrl(urlRest);
    if (data.empty()) {
        return INET_E_DATA_NOT_AVAILABLE;
    }

    const WCHAR* imgExt = GfxFileExtFromData({(u8*)data.data(), data.size()});
    AutoFreeWstr mime(MimeFromUrl(urlRest, imgExt));
    pIProtSink->ReportProgress(BINDSTATUS_VERIFIEDMIMETYPEAVAILABLE, mime);
#ifdef _WIN64
    // not going to report data in parts for unexpectedly huge webpages
    CrashIf(data.size() > ULONG_MAX);
#endif
    pIProtSink->ReportData(BSCF_FIRSTDATANOTIFICATION | BSCF_LASTDATANOTIFICATION | BSCF_DATAFULLYAVAILABLE,
                           (ULONG)data.size(), (ULONG)data.size());
    pIProtSink->ReportResult(S_OK, 200, nullptr);
    return S_OK;
}

STDMETHODIMP HW_IInternetProtocol::Read(void* pv, ULONG cb, ULONG* pcbRead) {
    if (data.empty()) {
        return S_FALSE;
    }
    size_t dataAvail = data.size() - dataCurrPos;
    if (0 == dataAvail) {
        return S_FALSE;
    }
    ULONG toRead = cb;
    if (toRead > dataAvail) {
        toRead = (ULONG)dataAvail;
    }
    u8* dataToRead = data.data() + dataCurrPos;
    memcpy(pv, dataToRead, toRead);
    dataCurrPos += toRead;
    *pcbRead = toRead;
    return S_OK;
}

STDMETHODIMP HW_IInternetProtocol::Seek(LARGE_INTEGER /*dlibMove*/, DWORD /*dwOrigin*/,
                                        ULARGE_INTEGER* /*plibNewPosition*/) {
    // doesn't seem to be called
    return E_NOTIMPL;
}

class HW_IInternetProtocolFactory : public IClassFactory {
  protected:
    virtual ~HW_IInternetProtocolFactory() {
    }

  public:
    HW_IInternetProtocolFactory() {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
    ULONG STDMETHODCALLTYPE AddRef() {
        return InterlockedIncrement(&refCount);
    }
    ULONG STDMETHODCALLTYPE Release();

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject);
    STDMETHODIMP LockServer([[maybe_unused]] BOOL fLock) {
        return S_OK;
    }

  protected:
    LONG refCount{1};
};

STDMETHODIMP_(ULONG) HW_IInternetProtocolFactory::Release() {
    LONG res = InterlockedDecrement(&refCount);
    CrashIf(res < 0);
    if (0 == res) {
        delete this;
    }
    return res;
}

STDMETHODIMP HW_IInternetProtocolFactory::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {QITABENT(HW_IInternetProtocolFactory, IClassFactory), {0}};
    return QISearch(this, qit, riid, ppv);
}

STDMETHODIMP HW_IInternetProtocolFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) {
    if (pUnkOuter != nullptr) {
        return CLASS_E_NOAGGREGATION;
    }
    if (riid == IID_IInternetProtocol) {
        ScopedComPtr<IInternetProtocol> proto(new HW_IInternetProtocol());
        return proto->QueryInterface(riid, ppvObject);
    }
    if (riid == IID_IInternetProtocolInfo) {
        ScopedComPtr<IInternetProtocolInfo> proto(new HW_IInternetProtocolInfo());
        return proto->QueryInterface(riid, ppvObject);
    }
    return E_NOINTERFACE;
}

static LONG gProtocolFactoryRefCount = 0;
HW_IInternetProtocolFactory* gInternetProtocolFactory = nullptr;

// Register our protocol so that urlmon will call us for every
// url that starts with HW_PROTO_PREFIX
static void RegisterInternetProtocolFactory() {
    LONG val = InterlockedIncrement(&gProtocolFactoryRefCount);
    if (val > 1) {
        return;
    }

    ScopedComPtr<IInternetSession> internetSession;
    HRESULT hr = CoInternetGetSession(0, &internetSession, 0);
    CrashIf(FAILED(hr));
    CrashIf(nullptr != gInternetProtocolFactory);
    gInternetProtocolFactory = new HW_IInternetProtocolFactory();
    hr = internetSession->RegisterNameSpace(gInternetProtocolFactory, CLSID_HW_IInternetProtocol, HW_PROTO_PREFIX, 0,
                                            nullptr, 0);
    CrashIf(FAILED(hr));
}

static void UnregisterInternetProtocolFactory() {
    LONG val = InterlockedDecrement(&gProtocolFactoryRefCount);
    if (val > 0) {
        return;
    }
    ScopedComPtr<IInternetSession> internetSession;
    HRESULT hr = CoInternetGetSession(0, &internetSession, 0);
    CrashIf(FAILED(hr));
    internetSession->UnregisterNameSpace(gInternetProtocolFactory, HW_PROTO_PREFIX);
    ULONG refCount = gInternetProtocolFactory->Release();
    CrashIf(refCount != 0);
    gInternetProtocolFactory = nullptr;
}

class HW_IOleInPlaceFrame : public IOleInPlaceFrame {
  public:
    explicit HW_IOleInPlaceFrame(FrameSite* fs) : fs(fs) {
    }
    ~HW_IOleInPlaceFrame() {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() {
        return fs->Release();
    }

    // IOleWindow
    STDMETHODIMP GetWindow(HWND*);
    STDMETHODIMP ContextSensitiveHelp(BOOL) {
        return S_OK;
    }

    // IOleInPlaceUIWindow
    STDMETHODIMP GetBorder(LPRECT);
    STDMETHODIMP RequestBorderSpace(LPCBORDERWIDTHS);
    STDMETHODIMP SetBorderSpace(LPCBORDERWIDTHS) {
        return S_OK;
    }
    STDMETHODIMP SetActiveObject(IOleInPlaceActiveObject*, LPCOLESTR) {
        return S_OK;
    }

    // IOleInPlaceFrame
    STDMETHODIMP InsertMenus(HMENU, LPOLEMENUGROUPWIDTHS) {
        return S_OK;
    }
    STDMETHODIMP SetMenu(HMENU, HOLEMENU, HWND) {
        return S_OK;
    }
    STDMETHODIMP RemoveMenus(HMENU) {
        return S_OK;
    }
    STDMETHODIMP SetStatusText(LPCOLESTR) {
        return S_OK;
    }
    STDMETHODIMP EnableModeless(BOOL) {
        return S_OK;
    }
    STDMETHODIMP TranslateAccelerator(LPMSG, WORD) {
        return E_NOTIMPL;
    }

  protected:
    FrameSite* fs;
};

class HW_IOleInPlaceSiteWindowless : public IOleInPlaceSiteWindowless {
  public:
    explicit HW_IOleInPlaceSiteWindowless(FrameSite* fs) : fs(fs) {
    }
    ~HW_IOleInPlaceSiteWindowless() {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() {
        return fs->Release();
    }

    // IOleWindow
    STDMETHODIMP GetWindow(HWND* h) {
        return fs->oleInPlaceFrame->GetWindow(h);
    }
    STDMETHODIMP ContextSensitiveHelp(BOOL b) {
        return fs->oleInPlaceFrame->ContextSensitiveHelp(b);
    }

    // IOleInPlaceSite
    STDMETHODIMP CanInPlaceActivate() {
        return S_OK;
    }
    STDMETHODIMP OnInPlaceActivate();
    STDMETHODIMP OnUIActivate();
    STDMETHODIMP GetWindowContext(IOleInPlaceFrame**, IOleInPlaceUIWindow**, LPRECT, LPRECT, LPOLEINPLACEFRAMEINFO);
    STDMETHODIMP Scroll(SIZE) {
        return S_OK;
    }
    STDMETHODIMP OnUIDeactivate(BOOL);
    STDMETHODIMP OnInPlaceDeactivate();
    STDMETHODIMP DiscardUndoState() {
        return S_OK;
    }
    STDMETHODIMP DeactivateAndUndo() {
        return S_OK;
    }
    STDMETHODIMP OnPosRectChange(LPCRECT) {
        return S_OK;
    }

    // IOleInPlaceSiteEx
    STDMETHODIMP OnInPlaceActivateEx(BOOL*, DWORD);
    STDMETHODIMP OnInPlaceDeactivateEx(BOOL) {
        return S_OK;
    }
    STDMETHODIMP RequestUIActivate() {
        return S_FALSE;
    }

    // IOleInPlaceSiteWindowless
    STDMETHODIMP CanWindowlessActivate();
    STDMETHODIMP GetCapture() {
        return S_FALSE;
    }
    STDMETHODIMP SetCapture(BOOL) {
        return S_FALSE;
    }
    STDMETHODIMP GetFocus() {
        return S_OK;
    }
    STDMETHODIMP SetFocus(BOOL) {
        return S_OK;
    }
    STDMETHODIMP GetDC(LPCRECT, DWORD, HDC*);
    STDMETHODIMP ReleaseDC(HDC) {
        return E_NOTIMPL;
    }
    STDMETHODIMP InvalidateRect(LPCRECT, BOOL);
    STDMETHODIMP InvalidateRgn(HRGN, BOOL) {
        return E_NOTIMPL;
    }
    STDMETHODIMP ScrollRect(INT, INT, LPCRECT, LPCRECT) {
        return E_NOTIMPL;
    }
    STDMETHODIMP AdjustRect(LPRECT) {
        return E_NOTIMPL;
    }
    STDMETHODIMP OnDefWindowMessage(UINT, WPARAM, LPARAM, LRESULT*) {
        return E_NOTIMPL;
    }

  protected:
    FrameSite* fs;
};

class HW_IOleClientSite : public IOleClientSite {
  public:
    explicit HW_IOleClientSite(FrameSite* fs) : fs(fs) {
    }
    ~HW_IOleClientSite() {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() {
        return fs->Release();
    }

    // IOleClientSite
    STDMETHODIMP SaveObject() {
        return S_OK;
    }
    STDMETHODIMP GetMoniker(DWORD, DWORD, IMoniker**) {
        return E_NOTIMPL;
    }
    STDMETHODIMP GetContainer(LPOLECONTAINER FAR*);
    STDMETHODIMP ShowObject() {
        return S_OK;
    }
    STDMETHODIMP OnShowWindow(BOOL) {
        return S_OK;
    }
    STDMETHODIMP RequestNewObjectLayout() {
        return E_NOTIMPL;
    }

  protected:
    FrameSite* fs;
};

class HW_IOleControlSite : public IOleControlSite {
  public:
    explicit HW_IOleControlSite(FrameSite* fs) : fs(fs) {
    }
    ~HW_IOleControlSite() {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() {
        return fs->Release();
    }

    // IOleControlSite
    STDMETHODIMP OnControlInfoChanged() {
        return S_OK;
    }
    STDMETHODIMP LockInPlaceActive(BOOL);
    STDMETHODIMP GetExtendedControl(IDispatch**) {
        return E_NOTIMPL;
    }
    STDMETHODIMP TransformCoords(POINTL*, POINTF*, DWORD);
    STDMETHODIMP TranslateAccelerator(LPMSG, DWORD) {
        return E_NOTIMPL;
    }
    STDMETHODIMP OnFocus(BOOL) {
        return S_OK;
    }
    STDMETHODIMP ShowPropertyFrame() {
        return E_NOTIMPL;
    }

  protected:
    FrameSite* fs;
};

class HW_IOleCommandTarget : public IOleCommandTarget {
  public:
    explicit HW_IOleCommandTarget(FrameSite* fs) : fs(fs) {
    }
    ~HW_IOleCommandTarget() {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() {
        return fs->Release();
    }

    // IOleCommandTarget
    STDMETHODIMP QueryStatus(const GUID*, ULONG, OLECMD[], OLECMDTEXT*);
    STDMETHODIMP Exec(const GUID*, DWORD, DWORD, VARIANTARG*, VARIANTARG*) {
        return OLECMDERR_E_NOTSUPPORTED;
    }

  protected:
    FrameSite* fs;
};

class HW_IOleItemContainer : public IOleItemContainer {
  public:
    explicit HW_IOleItemContainer(FrameSite* fs) : fs(fs) {
    }
    ~HW_IOleItemContainer() {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() {
        return fs->Release();
    }

    // IParseDisplayName
    STDMETHODIMP ParseDisplayName(IBindCtx*, LPOLESTR, ULONG*, IMoniker**) {
        return E_NOTIMPL;
    }

    // IOleContainer
    STDMETHODIMP EnumObjects(DWORD, IEnumUnknown**) {
        return E_NOTIMPL;
    }
    STDMETHODIMP LockContainer(BOOL) {
        return S_OK;
    }

    // IOleItemContainer
    STDMETHODIMP GetObject(LPOLESTR, DWORD, IBindCtx*, REFIID, void**);
    STDMETHODIMP GetObjectStorage(LPOLESTR, IBindCtx*, REFIID, void**);
    STDMETHODIMP IsRunning(LPOLESTR);

  protected:
    FrameSite* fs;
};

class HW_DWebBrowserEvents2 : public DWebBrowserEvents2 {
    FrameSite* fs;

    HRESULT DispatchPropGet(DISPID dispIdMember, VARIANT* res);

  public:
    explicit HW_DWebBrowserEvents2(FrameSite* fs) : fs(fs) {
    }
    ~HW_DWebBrowserEvents2() {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() {
        return fs->Release();
    }

    // IDispatch
    STDMETHODIMP GetIDsOfNames(REFIID, OLECHAR**, unsigned int, LCID, DISPID*) {
        return E_NOTIMPL;
    }
    STDMETHODIMP GetTypeInfo(unsigned int, LCID, ITypeInfo**) {
        return E_NOTIMPL;
    }
    STDMETHODIMP GetTypeInfoCount(unsigned int*) {
        return E_NOTIMPL;
    }
    STDMETHODIMP Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*);
};

class HW_IAdviseSink2 : public IAdviseSink2, public IAdviseSinkEx {
    FrameSite* fs;

  public:
    explicit HW_IAdviseSink2(FrameSite* fs) : fs(fs) {
    }
    ~HW_IAdviseSink2() {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() {
        return fs->Release();
    }

    // IAdviseSink
    void STDMETHODCALLTYPE OnDataChange(FORMATETC*, STGMEDIUM*) {
    }
    void STDMETHODCALLTYPE OnViewChange(DWORD, LONG) {
        // redraw the control
        fs->oleInPlaceSiteWindowless->InvalidateRect(nullptr, FALSE);
    }
    void STDMETHODCALLTYPE OnRename(IMoniker*) {
    }
    void STDMETHODCALLTYPE OnSave() {
    }
    void STDMETHODCALLTYPE OnClose() {
    }

    // IAdviseSink2
    void STDMETHODCALLTYPE OnLinkSrcChange(IMoniker*) {
    }

    // IAdviseSinkEx
    void STDMETHODCALLTYPE OnViewStatusChange(DWORD) {
    }
};

// http://www.popkistopki.ru/ch09b.shtml
class HW_IDocHostUIHandler : public IDocHostUIHandler {
    FrameSite* fs;

  public:
    explicit HW_IDocHostUIHandler(FrameSite* fs) : fs(fs) {
    }
    ~HW_IDocHostUIHandler() {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() {
        return fs->Release();
    }

    // IDocHostUIHandler
    STDMETHODIMP ShowContextMenu([[maybe_unused]] DWORD dwID, [[maybe_unused]] POINT* ppt,
                                 [[maybe_unused]] IUnknown* pcmdtReserved, [[maybe_unused]] IDispatch* pdispReserved) {
        return S_FALSE;
    }
    STDMETHODIMP GetHostInfo(DOCHOSTUIINFO* pInfo);
    STDMETHODIMP ShowUI([[maybe_unused]] DWORD dwID, [[maybe_unused]] IOleInPlaceActiveObject* pActiveObject,
                        [[maybe_unused]] IOleCommandTarget* pCommandTarget, [[maybe_unused]] IOleInPlaceFrame* pFrame,
                        [[maybe_unused]] IOleInPlaceUIWindow* pDoc) {
        return S_FALSE;
    }
    STDMETHODIMP HideUI() {
        return E_NOTIMPL;
    }
    STDMETHODIMP UpdateUI() {
        return E_NOTIMPL;
    }
    STDMETHODIMP EnableModeless([[maybe_unused]] BOOL fEnable) {
        return E_NOTIMPL;
    }
    STDMETHODIMP OnDocWindowActivate([[maybe_unused]] BOOL fActivate) {
        return E_NOTIMPL;
    }
    STDMETHODIMP OnFrameWindowActivate([[maybe_unused]] BOOL fActivate) {
        return E_NOTIMPL;
    }
    STDMETHODIMP ResizeBorder([[maybe_unused]] LPCRECT prcBorder, [[maybe_unused]] IOleInPlaceUIWindow* pUIWindow,
                              [[maybe_unused]] BOOL fRameWindow) {
        return E_NOTIMPL;
    }
    STDMETHODIMP TranslateAccelerator([[maybe_unused]] LPMSG lpMsg, [[maybe_unused]] const GUID* pguidCmdGroup,
                                      [[maybe_unused]] DWORD nCmdID) {
        return S_FALSE;
    }
    STDMETHODIMP GetOptionKeyPath([[maybe_unused]] LPOLESTR* pchKey, [[maybe_unused]] DWORD dw) {
        return S_FALSE;
    }
    STDMETHODIMP GetDropTarget([[maybe_unused]] IDropTarget* pDropTarget, IDropTarget** ppDropTarget) {
        return fs->QueryInterface(IID_PPV_ARGS(ppDropTarget));
    }
    STDMETHODIMP GetExternal(IDispatch** ppDispatch) {
        if (ppDispatch) {
            *ppDispatch = nullptr;
        }
        return S_FALSE;
    }
    STDMETHODIMP TranslateUrl([[maybe_unused]] DWORD dwTranslate, [[maybe_unused]] OLECHAR* pchURLIn,
                              [[maybe_unused]] OLECHAR** ppchURLOut) {
        return S_FALSE;
    }
    STDMETHODIMP FilterDataObject([[maybe_unused]] IDataObject* pDO, IDataObject** ppDORet) {
        if (ppDORet) {
            *ppDORet = nullptr;
        }
        return S_FALSE;
    }
};

STDMETHODIMP HW_IDocHostUIHandler::GetHostInfo(DOCHOSTUIINFO* pInfo) {
    if (!pInfo) {
        return S_FALSE;
    }
    pInfo->pchHostCss = nullptr;
    pInfo->pchHostNS = nullptr;

    // Note: I was hoping that also setting  DOCHOSTUIFLAG_SCROLL_NO
    // would get rid of vertical scrollbar when not necessary, but alas it
    // always removes it
    pInfo->dwFlags = DOCHOSTUIFLAG_NO3DBORDER | DOCHOSTUIFLAG_NO3DOUTERBORDER;
    pInfo->dwDoubleClick = DOCHOSTUIDBLCLK_DEFAULT;
    return S_OK;
}

class HW_IDropTarget : public IDropTarget {
    FrameSite* fs;

  public:
    explicit HW_IDropTarget(FrameSite* fs) : fs(fs) {
    }
    ~HW_IDropTarget() {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() {
        return fs->Release();
    }

    STDMETHODIMP DragEnter(IDataObject* pDataObj, [[maybe_unused]] DWORD grfKeyState, [[maybe_unused]] POINTL pt,
                           DWORD* pdwEffect) {
        HRESULT hr = fs->htmlWindow->OnDragEnter(pDataObj);
        if (SUCCEEDED(hr)) {
            *pdwEffect = DROPEFFECT_COPY;
        }
        return hr;
    }
    STDMETHODIMP DragOver([[maybe_unused]] DWORD grfKeyState, [[maybe_unused]] POINTL pt, DWORD* pdwEffect) {
        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }
    STDMETHODIMP DragLeave() {
        return S_OK;
    }
    STDMETHODIMP Drop(IDataObject* pDataObj, [[maybe_unused]] DWORD grfKeyState, [[maybe_unused]] POINTL pt,
                      DWORD* pdwEffect) {
        *pdwEffect = DROPEFFECT_COPY;
        return fs->htmlWindow->OnDragDrop(pDataObj);
    }
};

#ifndef __IDownloadManager_INTERFACE_DEFINED__
#define __IDownloadManager_INTERFACE_DEFINED__

#define DEFINE_GUID_STATIC(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}
DEFINE_GUID_STATIC(IID_IDownloadManager, 0x988934a4, 0x064b, 0x11d3, 0xbb, 0x80, 0x0, 0x10, 0x4b, 0x35, 0xe7, 0xf9);
#define SID_SDownloadManager IID_IDownloadManager

MIDL_INTERFACE("988934A4-064B-11D3-BB80-00104B35E7F9")
IDownloadManager : public IUnknown {
  public:
    virtual STDMETHODIMP Download(IMoniker __RPC_FAR * pmk, IBindCtx __RPC_FAR * pbc, DWORD dwBindVerb, LONG grfBINDF,
                                  BINDINFO __RPC_FAR * pBindInfo, LPCOLESTR pszHeaders, LPCOLESTR pszRedir,
                                  UINT uiCP) = 0;
};

#endif

class HW_IDownloadManager : public IDownloadManager {
    LONG refCount{1};

  public:
    HW_IDownloadManager() {
    }
    ~HW_IDownloadManager() {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        static const QITAB qit[] = {QITABENT(HW_IDownloadManager, IDownloadManager), {0}};
        return QISearch(this, qit, riid, ppv);
    }
    ULONG STDMETHODCALLTYPE AddRef() {
        return InterlockedIncrement(&refCount);
    }
    ULONG STDMETHODCALLTYPE Release() {
        LONG res = InterlockedDecrement(&refCount);
        CrashIf(res < 0);
        if (0 == res) {
            delete this;
        }
        return res;
    }

    // IDownloadManager
    STDMETHODIMP Download(IMoniker __RPC_FAR* pmk, IBindCtx __RPC_FAR* pbc, [[maybe_unused]] DWORD dwBindVerb,
                          [[maybe_unused]] LONG grfBINDF, [[maybe_unused]] BINDINFO __RPC_FAR* pBindInfo,
                          [[maybe_unused]] LPCOLESTR pszHeaders, [[maybe_unused]] LPCOLESTR pszRedir,
                          [[maybe_unused]] UINT uiCP) {
        LPOLESTR urlToFile;
        HRESULT hr = pmk->GetDisplayName(pbc, nullptr, &urlToFile);
        if (FAILED(hr)) {
            return hr;
        }
        // parse the URL (only internal its:// URLs are supported)
        int htmlWindowId;
        AutoFreeWstr urlRest;
        bool ok = ParseProtoUrl(urlToFile, &htmlWindowId, &urlRest);
        // free urlToFile using IMalloc::Free
        IMalloc* pMalloc = nullptr;
        if (SUCCEEDED(CoGetMalloc(1, &pMalloc))) {
            pMalloc->Free(urlToFile);
        } else {
            CoTaskMemFree(urlToFile);
        }

        if (!ok) {
            return INET_E_INVALID_URL;
        }

        // fetch the data
        HtmlWindow* win = FindHtmlWindowById(htmlWindowId);
        if (!win || !win->htmlWinCb) {
            return INET_E_OBJECT_NOT_FOUND;
        }
        auto data = win->htmlWinCb->GetDataForUrl(urlRest);
        if (data.empty()) {
            return INET_E_DATA_NOT_AVAILABLE;
        }
        // ask the UI to let the user save the file
        win->htmlWinCb->DownloadData(urlRest, data);
        return S_OK;
    }
};

class HW_IServiceProvider : public IServiceProvider {
    FrameSite* fs;

  public:
    explicit HW_IServiceProvider(FrameSite* fs) : fs(fs) {
    }
    ~HW_IServiceProvider() {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() {
        return fs->Release();
    }

    // IServiceProvider
    STDMETHODIMP QueryService(REFGUID guidService, REFIID riid, void** ppv) {
        if (guidService == SID_SDownloadManager) {
            ScopedComPtr<IDownloadManager> dm(new HW_IDownloadManager());
            return dm->QueryInterface(riid, ppv);
        }
        return E_NOINTERFACE;
    }
};

class HtmlMoniker : public IMoniker {
  public:
    HtmlMoniker();
    virtual ~HtmlMoniker();

    HRESULT SetHtml(std::span<u8>);
    HRESULT SetBaseUrl(const WCHAR* baseUrl);

  public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    // IMoniker
    STDMETHODIMP BindToStorage(IBindCtx* pbc, IMoniker* pmkToLeft, REFIID riid, void** ppvObj);
    STDMETHODIMP GetDisplayName(IBindCtx* pbc, IMoniker* pmkToLeft, LPOLESTR* ppszDisplayName);
    STDMETHODIMP BindToObject([[maybe_unused]] IBindCtx* pbc, [[maybe_unused]] IMoniker* pmkToLeft,
                              [[maybe_unused]] REFIID riidResult, [[maybe_unused]] void** ppvResult) {
        return E_NOTIMPL;
    }
    STDMETHODIMP Reduce([[maybe_unused]] IBindCtx* pbc, [[maybe_unused]] DWORD dwReduceHowFar,
                        [[maybe_unused]] IMoniker** ppmkToLeft, [[maybe_unused]] IMoniker** ppmkReduced) {
        return E_NOTIMPL;
    }
    STDMETHODIMP ComposeWith([[maybe_unused]] IMoniker* pmkRight, [[maybe_unused]] BOOL fOnlyIfNotGeneric,
                             [[maybe_unused]] IMoniker** ppmkComposite) {
        return E_NOTIMPL;
    }
    STDMETHODIMP Enum([[maybe_unused]] BOOL fForward, [[maybe_unused]] IEnumMoniker** ppenumMoniker) {
        return E_NOTIMPL;
    }
    STDMETHODIMP IsEqual([[maybe_unused]] IMoniker* pmkOtherMoniker) {
        return E_NOTIMPL;
    }
    STDMETHODIMP Hash([[maybe_unused]] DWORD* pdwHash) {
        return E_NOTIMPL;
    }
    STDMETHODIMP IsRunning([[maybe_unused]] IBindCtx* pbc, [[maybe_unused]] IMoniker* pmkToLeft,
                           [[maybe_unused]] IMoniker* pmkNewlyRunning) {
        return E_NOTIMPL;
    }
    STDMETHODIMP GetTimeOfLastChange([[maybe_unused]] IBindCtx* pbc, [[maybe_unused]] IMoniker* pmkToLeft,
                                     [[maybe_unused]] FILETIME* pFileTime) {
        return E_NOTIMPL;
    }
    STDMETHODIMP Inverse([[maybe_unused]] IMoniker** ppmk) {
        return E_NOTIMPL;
    }
    STDMETHODIMP CommonPrefixWith([[maybe_unused]] IMoniker* pmkOther, [[maybe_unused]] IMoniker** ppmkPrefix) {
        return E_NOTIMPL;
    }
    STDMETHODIMP RelativePathTo([[maybe_unused]] IMoniker* pmkOther, [[maybe_unused]] IMoniker** ppmkRelPath) {
        return E_NOTIMPL;
    }
    STDMETHODIMP ParseDisplayName(IBindCtx* pbc, IMoniker* pmkToLeft, LPOLESTR pszDisplayName, ULONG* pchEaten,
                                  IMoniker** ppmkOut);
    STDMETHODIMP IsSystemMoniker(DWORD* pdwMksys) {
        if (!pdwMksys) {
            return E_POINTER;
        }
        *pdwMksys = MKSYS_NONE;
        return S_OK;
    }

    // IPersistStream methods
    STDMETHODIMP Save([[maybe_unused]] IStream* pStm, [[maybe_unused]] BOOL fClearDirty) {
        return E_NOTIMPL;
    }
    STDMETHODIMP IsDirty() {
        return E_NOTIMPL;
    }
    STDMETHODIMP Load([[maybe_unused]] IStream* pStm) {
        return E_NOTIMPL;
    }
    STDMETHODIMP GetSizeMax([[maybe_unused]] ULARGE_INTEGER* pcbSize) {
        return E_NOTIMPL;
    }

    // IPersist
    STDMETHODIMP GetClassID([[maybe_unused]] CLSID* pClassID) {
        return E_NOTIMPL;
    }

  private:
    LONG refCount{1};

    char* htmlData{nullptr};
    IStream* htmlStream{nullptr};

    WCHAR* baseUrl{nullptr};
};

HtmlMoniker::HtmlMoniker() {
}

HtmlMoniker::~HtmlMoniker() {
    if (htmlStream) {
        htmlStream->Release();
    }

    free(htmlData);
    free(baseUrl);
}

HRESULT HtmlMoniker::SetHtml(std::span<u8> d) {
    free(htmlData);
    htmlData = str::DupN(d);
    if (htmlStream) {
        htmlStream->Release();
    }
    htmlStream = CreateStreamFromData({(u8*)htmlData, d.size()});
    return S_OK;
}

HRESULT HtmlMoniker::SetBaseUrl(const WCHAR* newBaseUrl) {
    free(baseUrl);
    baseUrl = str::Dup(newBaseUrl);
    return S_OK;
}

STDMETHODIMP HtmlMoniker::BindToStorage([[maybe_unused]] IBindCtx* pbc, [[maybe_unused]] IMoniker* pmkToLeft,
                                        REFIID riid, void** ppvObj) {
    LARGE_INTEGER seek = {0};
    htmlStream->Seek(seek, STREAM_SEEK_SET, nullptr);
    return htmlStream->QueryInterface(riid, ppvObj);
}

static LPOLESTR OleStrDup(const WCHAR* s) {
    size_t cb = sizeof(WCHAR) * (str::Len(s) + 1);
    LPOLESTR ret = (LPOLESTR)CoTaskMemAlloc(cb);
    if (ret) {
        memcpy(ret, s, cb);
    }
    return ret;
}

STDMETHODIMP HtmlMoniker::GetDisplayName([[maybe_unused]] IBindCtx* pbc, [[maybe_unused]] IMoniker* pmkToLeft,
                                         LPOLESTR* ppszDisplayName) {
    if (!ppszDisplayName) {
        return E_POINTER;
    }
    *ppszDisplayName = OleStrDup(baseUrl ? baseUrl : L"");
    return *ppszDisplayName ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP HtmlMoniker::ParseDisplayName([[maybe_unused]] IBindCtx* pbc,
                                           [[maybe_unused]] [[maybe_unused]] IMoniker* pmkToLeft,
                                           [[maybe_unused]] LPOLESTR pszDisplayName, [[maybe_unused]] ULONG* pchEaten,
                                           [[maybe_unused]] IMoniker** ppmkOut) {
    return E_NOTIMPL;
}

STDMETHODIMP HtmlMoniker::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {
        QITABENT(HtmlMoniker, IMoniker), QITABENT(HtmlMoniker, IPersistStream), QITABENT(HtmlMoniker, IPersist), {0}};
    return QISearch(this, qit, riid, ppv);
}

ULONG STDMETHODCALLTYPE HtmlMoniker::AddRef() {
    return InterlockedIncrement(&refCount);
}

ULONG STDMETHODCALLTYPE HtmlMoniker::Release() {
    LONG res = InterlockedDecrement(&refCount);
    CrashIf(res < 0);
    if (0 == res) {
        delete this;
    }
    return res;
}

static HWND GetBrowserControlHwnd(HWND hwndControlParent) {
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
static LRESULT CALLBACK WndProcParent(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    HtmlWindow* win = (HtmlWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!win) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_SIZE:
            if (SIZE_MINIMIZED != wp) {
                win->OnSize(Size(LOWORD(lp), HIWORD(lp)));
                return 0;
            }
            break;

        // Note: not quite sure why I need this but if we don't swallow WM_MOUSEWHEEL
        // messages, we might get infinite recursion.
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            return 0;

        case WM_PARENTNOTIFY:
            if (LOWORD(wp) == WM_LBUTTONDOWN) {
                win->OnLButtonDown();
            }
            break;

        case WM_DROPFILES:
            return CallWindowProc(win->wndProcBrowserPrev, hwnd, msg, wp, lp);

        case WM_VSCROLL:
            win->SendMsg(msg, wp, lp);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void HtmlWindow::SubclassHwnd() {
    wndProcBrowserPrev = (WNDPROC)SetWindowLongPtr(hwndParent, GWLP_WNDPROC, (LONG_PTR)WndProcParent);
    userDataBrowserPrev = SetWindowLongPtr(hwndParent, GWLP_USERDATA, (LONG_PTR)this);
}

void HtmlWindow::UnsubclassHwnd() {
    if (!wndProcBrowserPrev) {
        return;
    }
    SetWindowLongPtr(hwndParent, GWLP_WNDPROC, (LONG_PTR)wndProcBrowserPrev);
    SetWindowLongPtr(hwndParent, GWLP_USERDATA, (LONG_PTR)userDataBrowserPrev);
}

HtmlWindow::HtmlWindow(HWND hwndParent, HtmlWindowCallback* cb)
    : hwndParent(hwndParent),
      webBrowser(nullptr),
      oleObject(nullptr),
      oleInPlaceObject(nullptr),
      viewObject(nullptr),
      connectionPoint(nullptr),
      htmlContent(nullptr),
      oleObjectHwnd(nullptr),
      adviseCookie(0),
      htmlWinCb(cb),
      wndProcBrowserPrev(nullptr),
      userDataBrowserPrev(0),
      canGoBack(false),
      canGoForward(false) {
    CrashIf(!hwndParent);
    RegisterInternetProtocolFactory();
    windowId = GenNewWindowId(this);
    htmlSetInProgress = nullptr;
    htmlSetInProgressUrl = nullptr;
}

bool HtmlWindow::CreateBrowser() {
    HRESULT hr;
    ScopedComPtr<IUnknown> p;
    if (!p.Create(CLSID_WebBrowser)) {
        return false;
    }
    hr = p->QueryInterface(&viewObject);
    if (FAILED(hr)) {
        return false;
    }
    hr = p->QueryInterface(&oleObject);
    if (FAILED(hr)) {
        return false;
    }

    DWORD status;
    hr = oleObject->GetMiscStatus(DVASPECT_CONTENT, &status);
    if (FAILED(hr)) {
        return false;
    }
    bool setClientSiteFirst = 0 != (status & OLEMISC_SETCLIENTSITEFIRST);
    bool invisibleAtRuntime = 0 != (status & OLEMISC_INVISIBLEATRUNTIME);

    FrameSite* fs = new FrameSite(this);
    ScopedComPtr<IUnknown> fsScope(fs);

    if (setClientSiteFirst) {
        oleObject->SetClientSite(fs->oleClientSite);
    }

    ScopedComQIPtr<IPersistStreamInit> psInit(p);
    if (psInit) {
        hr = psInit->InitNew();
        CrashIf(!SUCCEEDED(hr));
    }

    hr = p->QueryInterface(&oleInPlaceObject);
    if (FAILED(hr)) {
        return false;
    }
    hr = oleInPlaceObject->GetWindow(&oleObjectHwnd);
    if (FAILED(hr)) {
        return false;
    }

    ::SetActiveWindow(oleObjectHwnd);
    RECT rc = ToRECT(ClientRect(hwndParent));

    oleInPlaceObject->SetObjectRects(&rc, &rc);
    if (!invisibleAtRuntime) {
        hr = oleObject->DoVerb(OLEIVERB_INPLACEACTIVATE, nullptr, fs->oleClientSite, 0, hwndParent, &rc);
        if (FAILED(hr)) {
            return false;
        }
#if 0 // is this necessary?
        hr = oleObject->DoVerb(OLEIVERB_SHOW, 0, fs->oleClientSite, 0,
                hwnd, &rc);
#endif
    }

    if (!setClientSiteFirst) {
        oleObject->SetClientSite(fs->oleClientSite);
    }

    hr = p->QueryInterface(&webBrowser);
    if (FAILED(hr)) {
        return false;
    }

    ScopedComQIPtr<IConnectionPointContainer> cpContainer(p);
    if (!cpContainer) {
        return false;
    }
    hr = cpContainer->FindConnectionPoint(DIID_DWebBrowserEvents2, &connectionPoint);
    if (FAILED(hr)) {
        return false;
    }
    connectionPoint->Advise(fs->hwDWebBrowserEvents2, &adviseCookie);

    // TODO: disallow accessing any random url?
    // webBrowser->put_Offline(VARIANT_TRUE);

    webBrowser->put_MenuBar(VARIANT_FALSE);
    webBrowser->put_AddressBar(VARIANT_FALSE);
    webBrowser->put_StatusBar(VARIANT_FALSE);
    webBrowser->put_ToolBar(VARIANT_FALSE);
    webBrowser->put_Silent(VARIANT_TRUE);

    webBrowser->put_RegisterAsBrowser(VARIANT_FALSE);
    webBrowser->put_RegisterAsDropTarget(VARIANT_TRUE);

    // TODO: do I need this anymore?
    // NavigateToAboutBlank();
    SubclassHwnd();

    return true;
}

HtmlWindow* HtmlWindow::Create(HWND hwndParent, HtmlWindowCallback* cb) {
    HtmlWindow* htmlWin = new HtmlWindow(hwndParent, cb);
    if (!htmlWin->CreateBrowser()) {
        delete htmlWin;
        return nullptr;
    }
    return htmlWin;
}

HtmlWindow::~HtmlWindow() {
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
        oleObject->SetClientSite(nullptr);
        oleObject->Release();
    }

    if (viewObject) {
        viewObject->Release();
    }
    if (htmlContent) {
        htmlContent->Release();
    }
    if (webBrowser) {
        ULONG refCount = webBrowser->Release();
        DebugCrashIf(refCount != 0);
    }

    FreeWindowId(windowId);
    UnregisterInternetProtocolFactory();
    FreeHtmlSetInProgressData();
}

void HtmlWindow::OnSize(Size size) {
    if (webBrowser) {
        webBrowser->put_Width(size.dx);
        webBrowser->put_Height(size.dy);
    }

    if (oleInPlaceObject) {
        RECT r = ToRECT(Rect({}, size));
        oleInPlaceObject->SetObjectRects(&r, &r);
    }
}

void HtmlWindow::OnLButtonDown() const {
    if (htmlWinCb) {
        htmlWinCb->OnLButtonDown();
    }
}

void HtmlWindow::SetVisible(bool visible) {
    win::SetVisibility(hwndParent, visible);
    if (webBrowser) {
        webBrowser->put_Visible(visible ? VARIANT_TRUE : VARIANT_FALSE);
    }
}

// Use for urls for which data will be provided by HtmlWindowCallback::GetHtmlForUrl()
// (will be called from OnBeforeNavigate())
void HtmlWindow::NavigateToDataUrl(const WCHAR* url) {
    AutoFreeWstr fullUrl(str::Format(L"its://%d/%s", windowId, url));
    NavigateToUrl(fullUrl);
}

void HtmlWindow::NavigateToUrl(const WCHAR* url) {
    VARIANT urlVar;
    VariantInitBstr(urlVar, url);
    currentURL.Reset();
    webBrowser->Navigate2(&urlVar, 0, 0, 0, 0);
    VariantClear(&urlVar);
}

void HtmlWindow::GoBack() {
    if (webBrowser) {
        webBrowser->GoBack();
    }
}

void HtmlWindow::GoForward() {
    if (webBrowser) {
        webBrowser->GoForward();
    }
}

int HtmlWindow::GetZoomPercent() {
    VARIANT vtOut = {0};
    HRESULT hr = webBrowser->ExecWB(OLECMDID_OPTICAL_ZOOM, OLECMDEXECOPT_DONTPROMPTUSER, nullptr, &vtOut);
    if (FAILED(hr)) {
        return 100;
    }
    return vtOut.lVal;
}

void HtmlWindow::SetZoomPercent(int zoom) {
    VARIANT vtIn = {0};
    VARIANT vtOut = {0};
    VariantSetLong(&vtIn, zoom);
    webBrowser->ExecWB(OLECMDID_OPTICAL_ZOOM, OLECMDEXECOPT_DONTPROMPTUSER, &vtIn, &vtOut);
}

void HtmlWindow::PrintCurrentPage(bool showUI) {
    OLECMDEXECOPT cmdexecopt = showUI ? OLECMDEXECOPT_PROMPTUSER : OLECMDEXECOPT_DONTPROMPTUSER;
    webBrowser->ExecWB(OLECMDID_PRINT, cmdexecopt, nullptr, nullptr);
}

void HtmlWindow::FindInCurrentPage() {
    webBrowser->ExecWB(OLECMDID_FIND, OLECMDEXECOPT_PROMPTUSER, nullptr, nullptr);
}

void HtmlWindow::SelectAll() {
    webBrowser->ExecWB(OLECMDID_SELECTALL, OLECMDEXECOPT_DODEFAULT, nullptr, nullptr);
}

void HtmlWindow::CopySelection() {
    webBrowser->ExecWB(OLECMDID_COPY, OLECMDEXECOPT_DODEFAULT, nullptr, nullptr);
}

void HtmlWindow::NavigateToAboutBlank() {
    NavigateToUrl(L"about:blank");
}

void HtmlWindow::SetHtml(std::span<u8> d, const WCHAR* url) {
    FreeHtmlSetInProgressData();
    htmlSetInProgress = str::DupN(d);
    htmlSetInProgressUrl = str::Dup(url);
    NavigateToAboutBlank();
    // the real work will happen in OnDocumentComplete()
}

// TODO: we don't call OnDocumentComplete() on this because the url
// reports "about:blank" and we suppress that. Figure out a way
// to fix that (change the url somehow?)
// TODO: IHtmlDocument2->write() seems like a simpler method
// http://www.codeproject.com/Articles/3365/Embed-an-HTML-control-in-your-own-window-using-pla#BUFFER
// https://github.com/ReneNyffenegger/development_misc/blob/master/windows/mshtml/HTMLWindow.cpp#L143
void HtmlWindow::SetHtmlReal(std::span<u8> d) {
    if (htmlContent) {
        htmlContent->Release();
    }
    htmlContent = new HtmlMoniker();
    htmlContent->SetHtml(d);
    AutoFreeWstr baseUrl(str::Format(HW_PROTO_PREFIX L"://%d/", windowId));
    htmlContent->SetBaseUrl(baseUrl);

    ScopedComPtr<IDispatch> docDispatch;
    HRESULT hr = webBrowser->get_Document(&docDispatch);
    if (FAILED(hr) || !docDispatch) {
        return;
    }

    ScopedComQIPtr<IHTMLDocument2> doc(docDispatch);
    if (!doc) {
        return;
    }

    ScopedComQIPtr<IPersistMoniker> perstMon(doc);
    if (!perstMon) {
        return;
    }
    ScopedComQIPtr<IMoniker> htmlMon(htmlContent);
    hr = perstMon->Load(TRUE, htmlMon, nullptr, STGM_READ);
    CrashIf(FAILED(hr));
}

// http://stackoverflow.com/questions/9778206/how-i-can-get-information-about-the-scrollbars-of-an-webbrowser-control-instance
// http://stackoverflow.com/questions/8630173/hide-scrollbars-in-webbrowser-control-mfc
// This is equivalent of <body scroll=auto> but for any html
// This seems to be the only way to hide vertical scrollbar if it's not necessary
void HtmlWindow::SetScrollbarToAuto() {
    ScopedComPtr<IDispatch> docDispatch;
    HRESULT hr = webBrowser->get_Document(&docDispatch);
    if (FAILED(hr) || !docDispatch) {
        return;
    }

    ScopedComQIPtr<IHTMLDocument2> doc2(docDispatch);
    if (!doc2) {
        return;
    }

    ScopedComPtr<IHTMLElement> bodyElement;
    hr = doc2->get_body(&bodyElement);
    if (FAILED(hr) || !bodyElement) {
        return;
    }

    ScopedComQIPtr<IHTMLBodyElement> body(bodyElement);
    if (!body) {
        return;
    }

    BSTR s = SysAllocString(L"auto");
    hr = body->put_scroll(s);
    CrashIf(FAILED(hr));
    SysFreeString(s);
}

// Take a screenshot of a given <area> inside an html window and resize
// it to <finalSize>. It's up to the caller to make sure <area> fits
// within window (we don't check that's the case)
HBITMAP HtmlWindow::TakeScreenshot(Rect area, Size finalSize) {
    ScopedComPtr<IDispatch> docDispatch;
    HRESULT hr = webBrowser->get_Document(&docDispatch);
    if (FAILED(hr) || !docDispatch) {
        return nullptr;
    }
    ScopedComQIPtr<IViewObject2> view(docDispatch);
    if (!view) {
        return nullptr;
    }

    // capture the whole window (including scrollbars)
    // to image and create imageRes containing the area
    // user asked for
    Rect winRc = WindowRect(hwndParent);
    Gdiplus::Bitmap image(winRc.dx, winRc.dy, PixelFormat24bppRGB);
    Gdiplus::Graphics g(&image);

    HDC dc = g.GetHDC();
    RECTL rc = {0, 0, winRc.dx, winRc.dy};
    hr = view->Draw(DVASPECT_CONTENT, -1, nullptr, nullptr, dc, dc, &rc, nullptr, nullptr, 0);
    g.ReleaseHDC(dc);
    if (FAILED(hr)) {
        return nullptr;
    }

    Gdiplus::Bitmap imageRes(finalSize.dx, finalSize.dy, PixelFormat24bppRGB);
    Gdiplus::Graphics g2(&imageRes);
    g2.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g2.DrawImage(&image, Gdiplus::Rect(0, 0, finalSize.dx, finalSize.dy), area.x, area.y, area.dx, area.dy,
                 Gdiplus::UnitPixel);

    HBITMAP hbmp;
    Gdiplus::Status ok = imageRes.GetHBITMAP((Gdiplus::ARGB)Gdiplus::Color::White, &hbmp);
    if (ok != Gdiplus::Ok) {
        return nullptr;
    }
    return hbmp;
}

// called before an url is shown. If returns false, will cancel
// the navigation.
bool HtmlWindow::OnBeforeNavigate(const WCHAR* url, bool newWindow) {
    currentURL.Reset();
    if (!htmlWinCb) {
        return true;
    }
    if (IsBlankUrl(url)) {
        return true;
    }

    // if it's url for our internal protocol, strip the protocol
    // part as we don't want to expose it to clients.
    int protoWindowId;
    AutoFreeWstr urlReal(str::Dup(url));
    bool ok = ParseProtoUrl(url, &protoWindowId, &urlReal);
    CrashIf(ok && (protoWindowId != windowId));
    bool shouldNavigate = htmlWinCb->OnBeforeNavigate(urlReal, newWindow);
    return shouldNavigate;
}

void HtmlWindow::FreeHtmlSetInProgressData() {
    str::Free(this->htmlSetInProgress);
    str::Free(this->htmlSetInProgressUrl);
    this->htmlSetInProgress = nullptr;
    this->htmlSetInProgressUrl = nullptr;
}

void HtmlWindow::OnDocumentComplete(const WCHAR* url) {
    if (IsBlankUrl(url)) {
        if (htmlSetInProgress != nullptr) {
            // TODO: I think this triggers another OnDocumentComplete() for "about:blank",
            // which we should ignore?
            SetHtmlReal(str::ToSpan(htmlSetInProgress));
            if (htmlWinCb) {
                if (htmlSetInProgressUrl) {
                    htmlWinCb->OnDocumentComplete(htmlSetInProgressUrl);
                } else {
                    htmlWinCb->OnDocumentComplete(htmlSetInProgressUrl);
                }
            }

            FreeHtmlSetInProgressData();
            SetScrollbarToAuto();
            return;
        }
    }

    // if it's url for our internal protocol, strip the protocol
    // part as we don't want to expose it to clients.
    int protoWindowId;
    AutoFreeWstr urlReal(str::Dup(url));
    bool ok = ParseProtoUrl(url, &protoWindowId, &urlReal);
    CrashIf(ok && (protoWindowId != windowId));

    currentURL.Set(urlReal.StealData());
    if (htmlWinCb) {
        htmlWinCb->OnDocumentComplete(currentURL);
    }
    SetScrollbarToAuto();
}

HRESULT HtmlWindow::OnDragEnter(IDataObject* dataObj) {
    ScopedComQIPtr<IDataObject> data(dataObj);
    if (!data) {
        return E_INVALIDARG;
    }
    FORMATETC fe = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stg = {0};
    if (FAILED(data->GetData(&fe, &stg))) {
        return E_FAIL;
    }
    ReleaseStgMedium(&stg);
    return S_OK;
}

HRESULT HtmlWindow::OnDragDrop(IDataObject* dataObj) {
    ScopedComQIPtr<IDataObject> data(dataObj);
    if (!data) {
        return E_INVALIDARG;
    }
    FORMATETC fe = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stg = {0};
    if (FAILED(data->GetData(&fe, &stg))) {
        return E_FAIL;
    }

    HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
    if (hDrop) {
        SendMessageW(hwndParent, WM_DROPFILES, (WPARAM)hDrop, 1);
        GlobalUnlock(stg.hGlobal);
    }
    ReleaseStgMedium(&stg);
    return hDrop != nullptr ? S_OK : E_FAIL;
}

LRESULT HtmlWindow::SendMsg(UINT msg, WPARAM wp, LPARAM lp) {
    HWND hwndBrowser = GetBrowserControlHwnd(hwndParent);
    return SendMessageW(hwndBrowser, msg, wp, lp);
}

FrameSite::FrameSite(HtmlWindow* win) {
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

    // m_hDCBuffer = nullptr;
    hwndParent = htmlWindow->hwndParent;

    oleInPlaceFrame = new HW_IOleInPlaceFrame(this);
    oleInPlaceSiteWindowless = new HW_IOleInPlaceSiteWindowless(this);
    oleClientSite = new HW_IOleClientSite(this);
    oleControlSite = new HW_IOleControlSite(this);
    oleCommandTarget = new HW_IOleCommandTarget(this);
    oleItemContainer = new HW_IOleItemContainer(this);
    hwDWebBrowserEvents2 = new HW_DWebBrowserEvents2(this);
    adviseSink2 = new HW_IAdviseSink2(this);
    docHostUIHandler = new HW_IDocHostUIHandler(this);
    dropTarget = new HW_IDropTarget(this);
    serviceProvider = new HW_IServiceProvider(this);
}

FrameSite::~FrameSite() {
    delete serviceProvider;
    delete dropTarget;
    delete docHostUIHandler;
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
STDMETHODIMP FrameSite::QueryInterface(REFIID riid, void** ppv) {
    if (ppv == nullptr) {
        return E_INVALIDARG;
    }

    *ppv = nullptr;
    if (riid == IID_IUnknown) {
        *ppv = this;
    } else if (riid == IID_IOleWindow || riid == IID_IOleInPlaceUIWindow || riid == IID_IOleInPlaceFrame) {
        *ppv = oleInPlaceFrame;
    } else if (riid == IID_IOleInPlaceSite || riid == IID_IOleInPlaceSiteEx || riid == IID_IOleInPlaceSiteWindowless) {
        *ppv = oleInPlaceSiteWindowless;
    } else if (riid == IID_IOleClientSite) {
        *ppv = oleClientSite;
    } else if (riid == IID_IOleControlSite) {
        *ppv = oleControlSite;
    } else if (riid == IID_IOleCommandTarget) {
        *ppv = oleCommandTarget;
    } else if (riid == IID_IOleItemContainer || riid == IID_IOleContainer || riid == IID_IParseDisplayName) {
        *ppv = oleItemContainer;
    } else if (riid == IID_IDispatch || riid == DIID_DWebBrowserEvents2) {
        *ppv = hwDWebBrowserEvents2;
    } else if (riid == IID_IAdviseSink || riid == IID_IAdviseSink2 || riid == IID_IAdviseSinkEx) {
        *ppv = adviseSink2;
    } else if (riid == IID_IDocHostUIHandler) {
        *ppv = docHostUIHandler;
    } else if (riid == IID_IDropTarget) {
        *ppv = dropTarget;
    } else if (riid == IID_IServiceProvider) {
        *ppv = serviceProvider;
    } else {
        return E_NOINTERFACE;
    }
    if (!*ppv) {
        return E_OUTOFMEMORY;
    }
    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE FrameSite::Release() {
    LONG res = InterlockedDecrement(&refCount);
    CrashIf(res < 0);
    if (0 == res) {
        delete this;
    }
    return res;
}

// IDispatch
HRESULT HW_DWebBrowserEvents2::DispatchPropGet(DISPID dispIdMember, VARIANT* res) {
    if (res == nullptr) {
        return E_INVALIDARG;
    }

    switch (dispIdMember) {
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

static BSTR BstrFromVariant(VARIANT* vurl) {
    if (vurl->vt & VT_BYREF) {
        return *vurl->pbstrVal;
    } else {
        return vurl->bstrVal;
    }
}

HRESULT HW_DWebBrowserEvents2::Invoke(DISPID dispIdMember, [[maybe_unused]] REFIID riid, [[maybe_unused]] LCID lcid,
                                      WORD flags, DISPPARAMS* pDispParams, VARIANT* pVarResult,
                                      [[maybe_unused]] EXCEPINFO* pExcepInfo, [[maybe_unused]] unsigned int* puArgErr) {
    if (flags & DISPATCH_PROPERTYGET) {
        return DispatchPropGet(dispIdMember, pVarResult);
    }

    switch (dispIdMember) {
        case DISPID_BEFORENAVIGATE2: {
            BSTR url = BstrFromVariant(pDispParams->rgvarg[5].pvarVal);
            bool shouldCancel = !fs->htmlWindow->OnBeforeNavigate(url, false);
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

        case DISPID_DOCUMENTCOMPLETE: {
            // TODO: there are complexities related to multi-frame documents. This
            // gets called on every frame and we should probably only notify
            // on completion of top-level frame. On the other hand, I haven't
            // encountered problems related to that yet
            BSTR url = BstrFromVariant(pDispParams->rgvarg[0].pvarVal);
            fs->htmlWindow->OnDocumentComplete(url);
            break;
        }

        case DISPID_NAVIGATEERROR: {
            // TODO: probably should notify about that too
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

        case DISPID_NEWWINDOW3: {
            BSTR url = pDispParams->rgvarg[0].bstrVal;
            bool shouldCancel = !fs->htmlWindow->OnBeforeNavigate(url, true);
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
HRESULT HW_IOleInPlaceFrame::GetWindow(HWND* phwnd) {
    if (phwnd == nullptr) {
        return E_INVALIDARG;
    }
    *phwnd = fs->hwndParent;
    return S_OK;
}

// IOleInPlaceUIWindow
HRESULT HW_IOleInPlaceFrame::GetBorder(LPRECT lprectBorder) {
    if (lprectBorder == nullptr) {
        return E_INVALIDARG;
    }
    return INPLACE_E_NOTOOLSPACE;
}

HRESULT HW_IOleInPlaceFrame::RequestBorderSpace(LPCBORDERWIDTHS pborderwidths) {
    if (pborderwidths == nullptr) {
        return E_INVALIDARG;
    }
    return INPLACE_E_NOTOOLSPACE;
}

// IOleInPlaceSite
HRESULT HW_IOleInPlaceSiteWindowless::OnInPlaceActivate() {
    fs->inPlaceActive = true;
    return S_OK;
}

HRESULT HW_IOleInPlaceSiteWindowless::OnUIActivate() {
    fs->uiActive = true;
    return S_OK;
}

HRESULT HW_IOleInPlaceSiteWindowless::GetWindowContext(IOleInPlaceFrame** ppFrame, IOleInPlaceUIWindow** ppDoc,
                                                       LPRECT lprcPosRect, LPRECT lprcClipRect,
                                                       LPOLEINPLACEFRAMEINFO lpFrameInfo) {
    if (ppFrame == nullptr || ppDoc == nullptr || lprcPosRect == nullptr || lprcClipRect == nullptr ||
        lpFrameInfo == nullptr) {
        if (ppFrame != nullptr) {
            *ppFrame = nullptr;
        }
        if (ppDoc != nullptr) {
            *ppDoc = nullptr;
        }
        return E_INVALIDARG;
    }

    *ppDoc = *ppFrame = fs->oleInPlaceFrame;
    (*ppDoc)->AddRef();
    (*ppFrame)->AddRef();

    lpFrameInfo->fMDIApp = FALSE;
    lpFrameInfo->hwndFrame = fs->hwndParent;
    lpFrameInfo->haccel = nullptr;
    lpFrameInfo->cAccelEntries = 0;

    return S_OK;
}

HRESULT HW_IOleInPlaceSiteWindowless::OnUIDeactivate([[maybe_unused]] BOOL fUndoable) {
    fs->uiActive = false;
    return S_OK;
}

HRESULT HW_IOleInPlaceSiteWindowless::OnInPlaceDeactivate() {
    fs->inPlaceActive = false;
    return S_OK;
}

// IOleInPlaceSiteEx
HRESULT HW_IOleInPlaceSiteWindowless::OnInPlaceActivateEx(BOOL* pfNoRedraw, [[maybe_unused]] DWORD dwFlags) {
    if (pfNoRedraw) {
        *pfNoRedraw = FALSE;
    }
    return S_OK;
}

// IOleInPlaceSiteWindowless
HRESULT HW_IOleInPlaceSiteWindowless::CanWindowlessActivate() {
    return fs->supportsWindowlessActivation ? S_OK : S_FALSE;
}

HRESULT HW_IOleInPlaceSiteWindowless::GetDC([[maybe_unused]] LPCRECT pRect, [[maybe_unused]] DWORD grfFlags,
                                            HDC* phDC) {
    if (phDC == nullptr) {
        return E_INVALIDARG;
    }

#if 0
    if (grfFlags & OLEDC_NODRAW)
    {
        *phDC = mfs->hDCBuffer;
        return S_OK;
    }

    if (fs->hDCBuffer != nullptr)
        return E_UNEXPECTED;
#endif
    return E_NOTIMPL;
}

HRESULT HW_IOleInPlaceSiteWindowless::InvalidateRect([[maybe_unused]] LPCRECT pRect, BOOL fErase) {
    ::InvalidateRect(fs->hwndParent, nullptr, fErase);
    return S_OK;
}

// IOleClientSite
HRESULT HW_IOleClientSite::GetContainer(LPOLECONTAINER* ppContainer) {
    if (ppContainer == nullptr) {
        return E_INVALIDARG;
    }
    return QueryInterface(IID_IOleContainer, (void**)ppContainer);
}

// IOleItemContainer
HRESULT HW_IOleItemContainer::GetObject(LPOLESTR pszItem, [[maybe_unused]] DWORD dwSpeedNeeded,
                                        [[maybe_unused]] IBindCtx* pbc, [[maybe_unused]] REFIID riid,
                                        void** ppvObject) {
    if (pszItem == nullptr) {
        return E_INVALIDARG;
    }
    if (ppvObject == nullptr) {
        return E_INVALIDARG;
    }
    *ppvObject = nullptr;
    return MK_E_NOOBJECT;
}

HRESULT HW_IOleItemContainer::GetObjectStorage(LPOLESTR pszItem, [[maybe_unused]] IBindCtx* pbc,
                                               [[maybe_unused]] REFIID riid, void** ppvStorage) {
    if (pszItem == nullptr) {
        return E_INVALIDARG;
    }
    if (ppvStorage == nullptr) {
        return E_INVALIDARG;
    }
    *ppvStorage = nullptr;
    return MK_E_NOOBJECT;
}

HRESULT HW_IOleItemContainer::IsRunning(LPOLESTR pszItem) {
    if (pszItem == nullptr) {
        return E_INVALIDARG;
    }
    return MK_E_NOOBJECT;
}

// IOleControlSite
HRESULT HW_IOleControlSite::LockInPlaceActive(BOOL fLock) {
    fs->inPlaceLocked = (fLock == TRUE);
    return S_OK;
}

HRESULT HW_IOleControlSite::TransformCoords(POINTL* pPtlHimetric, POINTF* pPtfContainer,
                                            [[maybe_unused]] DWORD dwFlags) {
    HRESULT hr = S_OK;
    if (pPtlHimetric == nullptr) {
        return E_INVALIDARG;
    }
    if (pPtfContainer == nullptr) {
        return E_INVALIDARG;
    }
    return hr;
}

// IOleCommandTarget
HRESULT HW_IOleCommandTarget::QueryStatus([[maybe_unused]] const GUID* pguidCmdGroup, [[maybe_unused]] ULONG cCmds,
                                          OLECMD* prgCmds, [[maybe_unused]] OLECMDTEXT* pCmdTet) {
    if (prgCmds == nullptr) {
        return E_INVALIDARG;
    }
    return OLECMDERR_E_UNKNOWNGROUP;
}
