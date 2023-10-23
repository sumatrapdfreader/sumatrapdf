/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "Dpi.h"
#include "ScopedWin.h"
#include "WinUtil.h"
#include "GuessFileType.h"

#include <mshtml.h>
#include <mshtmhst.h>
#include <oaidl.h>
#include <exdispid.h>

#include "HtmlWindow.h"

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

bool IsBlankUrl(const char* url) {
    return str::EqI("about:blank", url);
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
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&refCount);
    }
    ULONG STDMETHODCALLTYPE Release() override;

  protected:
    LONG refCount = 0;

    HW_IOleInPlaceFrame* oleInPlaceFrame = nullptr;
    HW_IOleInPlaceSiteWindowless* oleInPlaceSiteWindowless = nullptr;
    HW_IOleClientSite* oleClientSite = nullptr;
    HW_IOleControlSite* oleControlSite = nullptr;
    HW_IOleCommandTarget* oleCommandTarget = nullptr;
    HW_IOleItemContainer* oleItemContainer = nullptr;
    HW_DWebBrowserEvents2* hwDWebBrowserEvents2 = nullptr;
    HW_IAdviseSink2* adviseSink2 = nullptr;
    HW_IDocHostUIHandler* docHostUIHandler = nullptr;
    HW_IDropTarget* dropTarget = nullptr;
    HW_IServiceProvider* serviceProvider = nullptr;

    HtmlWindow* htmlWindow;

    // HDC m_hDCBuffer;
    HWND hwndParent = nullptr;

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

#define HW_PROTO_PREFIXA "its"

// {F1EC293F-DBBD-4A4B-94F4-FA52BA0BA6EE}
static const GUID CLSID_HW_IInternetProtocol = {0xf1ec293f,
                                                0xdbbd,
                                                0x4a4b,
                                                {0x94, 0xf4, 0xfa, 0x52, 0xba, 0xb, 0xa6, 0xee}};

class HW_IInternetProtocolInfo : public IInternetProtocolInfo {
  public:
    HW_IInternetProtocolInfo() = default;

  protected:
    virtual ~HW_IInternetProtocolInfo() = default;

  public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&refCount);
    }
    ULONG STDMETHODCALLTYPE Release() override;

    // IInternetProtocolInfo
    STDMETHODIMP ParseUrl(__unused LPCWSTR pwzUrl, __unused PARSEACTION parseAction, __unused DWORD dwParseFlags,
                          __unused LPWSTR pwzResult, __unused DWORD cchResult, __unused DWORD* pcchResult,
                          __unused DWORD dwReserved) override {
        return INET_E_DEFAULT_ACTION;
    }

    STDMETHODIMP CombineUrl(__unused LPCWSTR pwzBaseUrl, __unused LPCWSTR pwzRelativeUrl, __unused DWORD dwCombineFlags,
                            __unused LPWSTR pwzResult, __unused DWORD cchResult, __unused DWORD* pcchResult,
                            __unused DWORD dwReserved) override {
        return INET_E_DEFAULT_ACTION;
    }

    STDMETHODIMP CompareUrl(__unused LPCWSTR pwzUrl1, __unused LPCWSTR pwzUrl2,
                            __unused DWORD dwCompareFlags) override {
        return INET_E_DEFAULT_ACTION;
    }

    STDMETHODIMP QueryInfo(__unused LPCWSTR pwzUrl, __unused QUERYOPTION queryOption, __unused DWORD dwQueryFlags,
                           __unused LPVOID pBuffer, __unused DWORD cbBuffer, __unused DWORD* pcbBuf,
                           __unused DWORD dwReserved) override {
        return INET_E_DEFAULT_ACTION;
    }

  protected:
    LONG refCount = 1;
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
    static const QITAB qit[] = {QITABENT(HW_IInternetProtocolInfo, IInternetProtocolInfo), {nullptr}};
    return QISearch(this, qit, riid, ppv);
}

class HW_IInternetProtocol : public IInternetProtocol {
  public:
    HW_IInternetProtocol() = default;

  protected:
    virtual ~HW_IInternetProtocol() = default;

  public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&refCount);
    }
    ULONG STDMETHODCALLTYPE Release() override;

    // IInternetProtocol
    STDMETHODIMP Start(LPCWSTR szUrl, IInternetProtocolSink* pIProtSink, IInternetBindInfo* pIBindInfo, DWORD grfSTI,
                       HANDLE_PTR dwReserved) override;
    STDMETHODIMP Continue(__unused PROTOCOLDATA* pStateInfo) override {
        return S_OK;
    }
    STDMETHODIMP Abort(__unused HRESULT hrReason, __unused DWORD dwOptions) override {
        return S_OK;
    }
    STDMETHODIMP Terminate(__unused DWORD dwOptions) override {
        return S_OK;
    }
    STDMETHODIMP Suspend() override {
        return E_NOTIMPL;
    }
    STDMETHODIMP Resume() override {
        return E_NOTIMPL;
    }
    STDMETHODIMP Read(void* pv, ULONG cb, ULONG* pcbRead) override;
    STDMETHODIMP Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) override;
    STDMETHODIMP LockRequest(__unused DWORD dwOptions) override {
        return S_OK;
    }
    STDMETHODIMP UnlockRequest() override {
        return S_OK;
    }

  protected:
    LONG refCount = 1;

    // those are filled in Start() and represent data to be sent
    // for a given url
    ByteSlice data{};
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
    static const QITAB qit[] = {QITABENT(HW_IInternetProtocol, IInternetProtocol),
                                QITABENT(HW_IInternetProtocol, IInternetProtocolRoot),
                                {nullptr}};
    return QISearch(this, qit, riid, ppv);
}

// given url in the form "its://$htmlWindowId/$urlRest, parses
// out $htmlWindowId and $urlRest. Returns false if url doesn't conform
// to this pattern.
static bool ParseProtoUrl(const WCHAR* url, int* htmlWindowId, AutoFreeWstr* urlRest) {
    const WCHAR* rest = str::Parse(url, HW_PROTO_PREFIX L"://%d/%S", htmlWindowId, urlRest);
    return rest && !*rest;
}

// given url in the form "its://$htmlWindowId/$urlRest, parses
// out $htmlWindowId and $urlRest. Returns false if url doesn't conform
// to this pattern.
static bool ParseProtoUrl(const char* url, int* htmlWindowId, AutoFreeStr* urlRest) {
    const char* rest = str::Parse(url, HW_PROTO_PREFIXA "://%d/%S", htmlWindowId, urlRest);
    return rest && !*rest;
}

#define kDefaultMimeType "text/html"

// caller must free() the result
static char* MimeFromUrl(const char* url, const char* imgExt = nullptr) {
    const char* ext = str::FindCharLast(url, '.');
    if (!ext) {
        return str::Dup(kDefaultMimeType);
    }

    if (str::FindChar(ext, ';')) {
        // some CHM documents use (image) URLs that are followed by
        // a semi-colon and a number after the file's extension
        char* newUrl = str::DupTemp(url, str::FindChar(ext, ';') - url);
        return MimeFromUrl(newUrl, imgExt);
    }

    static const struct {
        const char* ext;
        const char* mimetype;
    } mimeTypes[] = {
        {".html", "text/html"}, {".htm", "text/html"},  {".gif", "image/gif"},
        {".png", "image/png"},  {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
        {".bmp", "image/bmp"},  {".css", "text/css"},   {".txt", "text/plain"},
    };

    for (int i = 0; i < dimof(mimeTypes); i++) {
        if (str::EqI(ext, mimeTypes[i].ext)) {
            // trust an image's data more than its extension
            if (imgExt && !str::Eq(imgExt, mimeTypes[i].ext) && str::StartsWith(mimeTypes[i].mimetype, "image/")) {
                for (int j = 0; j < dimof(mimeTypes); j++) {
                    if (str::Eq(imgExt, mimeTypes[j].ext)) {
                        return str::Dup(mimeTypes[j].mimetype);
                    }
                }
            }
            return str::Dup(mimeTypes[i].mimetype);
        }
    }

    char* contentType = ReadRegStrTemp(HKEY_CLASSES_ROOT, ext, "Content Type");
    if (contentType) {
        return str::Dup(contentType);
    }

    return str::Dup(kDefaultMimeType);
}

// TODO: return an error page html in case of errors?
STDMETHODIMP HW_IInternetProtocol::Start(LPCWSTR szUrl, IInternetProtocolSink* pIProtSink,
                                         __unused IInternetBindInfo* pIBindInfo, __unused DWORD grfSTI,
                                         __unused HANDLE_PTR dwReserved) {
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
    char* urlRestA = ToUtf8Temp(urlRest);
    data = win->htmlWinCb->GetDataForUrl(urlRestA);
    if (data.empty()) {
        return INET_E_DATA_NOT_AVAILABLE;
    }

    const char* imgExt = GfxFileExtFromData({(u8*)data.data(), data.size()});
    char* mime = MimeFromUrl(urlRestA, imgExt);
    WCHAR* mimeW = ToWStrTemp(mime);
    str::Free(mime);
    pIProtSink->ReportProgress(BINDSTATUS_VERIFIEDMIMETYPEAVAILABLE, mimeW);
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
    virtual ~HW_IInternetProtocolFactory() = default;

  public:
    HW_IInternetProtocolFactory() = default;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&refCount);
    }
    ULONG STDMETHODCALLTYPE Release() override;

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) override;
    STDMETHODIMP LockServer(__unused BOOL fLock) override {
        return S_OK;
    }

  protected:
    LONG refCount = 1;
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
    static const QITAB qit[] = {QITABENT(HW_IInternetProtocolFactory, IClassFactory), {nullptr}};
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
    ~HW_IOleInPlaceFrame() = default;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() override {
        return fs->Release();
    }

    // IOleWindow
    STDMETHODIMP GetWindow(HWND*) override;
    STDMETHODIMP ContextSensitiveHelp(BOOL) override {
        return S_OK;
    }

    // IOleInPlaceUIWindow
    STDMETHODIMP GetBorder(LPRECT) override;
    STDMETHODIMP RequestBorderSpace(LPCBORDERWIDTHS) override;
    STDMETHODIMP SetBorderSpace(LPCBORDERWIDTHS) override {
        return S_OK;
    }
    STDMETHODIMP SetActiveObject(IOleInPlaceActiveObject*, LPCOLESTR) override {
        return S_OK;
    }

    // IOleInPlaceFrame
    STDMETHODIMP InsertMenus(HMENU, LPOLEMENUGROUPWIDTHS) override {
        return S_OK;
    }
    STDMETHODIMP SetMenu(HMENU, HOLEMENU, HWND) override {
        return S_OK;
    }
    STDMETHODIMP RemoveMenus(HMENU) override {
        return S_OK;
    }
    STDMETHODIMP SetStatusText(LPCOLESTR) override {
        return S_OK;
    }
    STDMETHODIMP EnableModeless(BOOL) override {
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
    ~HW_IOleInPlaceSiteWindowless() = default;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() override {
        return fs->Release();
    }

    // IOleWindow
    STDMETHODIMP GetWindow(HWND* h) override {
        return fs->oleInPlaceFrame->GetWindow(h);
    }
    STDMETHODIMP ContextSensitiveHelp(BOOL b) override {
        return fs->oleInPlaceFrame->ContextSensitiveHelp(b);
    }

    // IOleInPlaceSite
    STDMETHODIMP CanInPlaceActivate() override {
        return S_OK;
    }
    STDMETHODIMP OnInPlaceActivate() override;
    STDMETHODIMP OnUIActivate() override;
    STDMETHODIMP GetWindowContext(IOleInPlaceFrame**, IOleInPlaceUIWindow**, LPRECT, LPRECT,
                                  LPOLEINPLACEFRAMEINFO) override;
    STDMETHODIMP Scroll(SIZE) override {
        return S_OK;
    }
    STDMETHODIMP OnUIDeactivate(BOOL) override;
    STDMETHODIMP OnInPlaceDeactivate() override;
    STDMETHODIMP DiscardUndoState() override {
        return S_OK;
    }
    STDMETHODIMP DeactivateAndUndo() override {
        return S_OK;
    }
    STDMETHODIMP OnPosRectChange(LPCRECT) override {
        return S_OK;
    }

    // IOleInPlaceSiteEx
    STDMETHODIMP OnInPlaceActivateEx(BOOL*, DWORD) override;
    STDMETHODIMP OnInPlaceDeactivateEx(BOOL) override {
        return S_OK;
    }
    STDMETHODIMP RequestUIActivate() override {
        return S_FALSE;
    }

    // IOleInPlaceSiteWindowless
    STDMETHODIMP CanWindowlessActivate() override;
    STDMETHODIMP GetCapture() override {
        return S_FALSE;
    }
    STDMETHODIMP SetCapture(BOOL) override {
        return S_FALSE;
    }
    STDMETHODIMP GetFocus() override {
        return S_OK;
    }
    STDMETHODIMP SetFocus(BOOL) override {
        return S_OK;
    }
    STDMETHODIMP GetDC(LPCRECT, DWORD, HDC*) override;
    STDMETHODIMP ReleaseDC(HDC) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP InvalidateRect(LPCRECT, BOOL) override;
    STDMETHODIMP InvalidateRgn(HRGN, BOOL) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP ScrollRect(INT, INT, LPCRECT, LPCRECT) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP AdjustRect(LPRECT) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP OnDefWindowMessage(UINT, WPARAM, LPARAM, LRESULT*) override {
        return E_NOTIMPL;
    }

  protected:
    FrameSite* fs;
};

class HW_IOleClientSite : public IOleClientSite {
  public:
    explicit HW_IOleClientSite(FrameSite* fs) : fs(fs) {
    }
    ~HW_IOleClientSite() = default;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() override {
        return fs->Release();
    }

    // IOleClientSite
    STDMETHODIMP SaveObject() override {
        return S_OK;
    }
    STDMETHODIMP GetMoniker(DWORD, DWORD, IMoniker**) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP GetContainer(LPOLECONTAINER FAR*) override;
    STDMETHODIMP ShowObject() override {
        return S_OK;
    }
    STDMETHODIMP OnShowWindow(BOOL) override {
        return S_OK;
    }
    STDMETHODIMP RequestNewObjectLayout() override {
        return E_NOTIMPL;
    }

  protected:
    FrameSite* fs;
};

class HW_IOleControlSite : public IOleControlSite {
  public:
    explicit HW_IOleControlSite(FrameSite* fs) : fs(fs) {
    }
    ~HW_IOleControlSite() = default;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() override {
        return fs->Release();
    }

    // IOleControlSite
    STDMETHODIMP OnControlInfoChanged() override {
        return S_OK;
    }
    STDMETHODIMP LockInPlaceActive(BOOL) override;
    STDMETHODIMP GetExtendedControl(IDispatch**) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP TransformCoords(POINTL*, POINTF*, DWORD) override;
    STDMETHODIMP TranslateAccelerator(LPMSG, DWORD) {
        return E_NOTIMPL;
    }
    STDMETHODIMP OnFocus(BOOL) override {
        return S_OK;
    }
    STDMETHODIMP ShowPropertyFrame() override {
        return E_NOTIMPL;
    }

  protected:
    FrameSite* fs;
};

class HW_IOleCommandTarget : public IOleCommandTarget {
  public:
    explicit HW_IOleCommandTarget(FrameSite* fs) : fs(fs) {
    }
    ~HW_IOleCommandTarget() = default;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() override {
        return fs->Release();
    }

    // IOleCommandTarget
    STDMETHODIMP QueryStatus(const GUID*, ULONG, OLECMD[], OLECMDTEXT*) override;
    STDMETHODIMP Exec(const GUID*, DWORD, DWORD, VARIANTARG*, VARIANTARG*) override {
        return OLECMDERR_E_NOTSUPPORTED;
    }

  protected:
    FrameSite* fs;
};

class HW_IOleItemContainer : public IOleItemContainer {
  public:
    explicit HW_IOleItemContainer(FrameSite* fs) : fs(fs) {
    }
    ~HW_IOleItemContainer() = default;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() override {
        return fs->Release();
    }

    // IParseDisplayName
    STDMETHODIMP ParseDisplayName(IBindCtx*, LPOLESTR, ULONG*, IMoniker**) override {
        return E_NOTIMPL;
    }

    // IOleContainer
    STDMETHODIMP EnumObjects(DWORD, IEnumUnknown**) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP LockContainer(BOOL) override {
        return S_OK;
    }

    // IOleItemContainer
    STDMETHODIMP GetObject(LPOLESTR, DWORD, IBindCtx*, REFIID, void**);
    STDMETHODIMP GetObjectStorage(LPOLESTR, IBindCtx*, REFIID, void**) override;
    STDMETHODIMP IsRunning(LPOLESTR) override;

  protected:
    FrameSite* fs;
};

class HW_DWebBrowserEvents2 : public DWebBrowserEvents2 {
    FrameSite* fs;

    HRESULT DispatchPropGet(DISPID dispIdMember, VARIANT* res);

  public:
    explicit HW_DWebBrowserEvents2(FrameSite* fs) : fs(fs) {
    }
    ~HW_DWebBrowserEvents2() = default;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() override {
        return fs->Release();
    }

    // IDispatch
    STDMETHODIMP GetIDsOfNames(REFIID, OLECHAR**, unsigned int, LCID, DISPID*) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP GetTypeInfo(unsigned int, LCID, ITypeInfo**) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP GetTypeInfoCount(unsigned int*) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*) override;
};

class HW_IAdviseSink2 : public IAdviseSink2, public IAdviseSinkEx {
    FrameSite* fs;

  public:
    explicit HW_IAdviseSink2(FrameSite* fs) : fs(fs) {
    }
    ~HW_IAdviseSink2() = default;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() override {
        return fs->Release();
    }

    // IAdviseSink
    void STDMETHODCALLTYPE OnDataChange(FORMATETC*, STGMEDIUM*) override {
    }
    void STDMETHODCALLTYPE OnViewChange(DWORD, LONG) override {
        // redraw the control
        fs->oleInPlaceSiteWindowless->InvalidateRect(nullptr, FALSE);
    }
    void STDMETHODCALLTYPE OnRename(IMoniker*) override {
    }
    void STDMETHODCALLTYPE OnSave() override {
    }
    void STDMETHODCALLTYPE OnClose() override {
    }

    // IAdviseSink2
    void STDMETHODCALLTYPE OnLinkSrcChange(IMoniker*) override {
    }

    // IAdviseSinkEx
    void STDMETHODCALLTYPE OnViewStatusChange(DWORD) override {
    }
};

// http://www.popkistopki.ru/ch09b.shtml
class HW_IDocHostUIHandler : public IDocHostUIHandler {
    FrameSite* fs;

  public:
    explicit HW_IDocHostUIHandler(FrameSite* fs) : fs(fs) {
    }
    ~HW_IDocHostUIHandler() = default;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() override {
        return fs->Release();
    }

    // IDocHostUIHandler
    STDMETHODIMP ShowContextMenu(__unused DWORD dwID, __unused POINT* ppt, __unused IUnknown* pcmdtReserved,
                                 __unused IDispatch* pdispReserved) override {
        return S_FALSE;
    }
    STDMETHODIMP GetHostInfo(DOCHOSTUIINFO* pInfo) override;
    STDMETHODIMP ShowUI(__unused DWORD dwID, __unused IOleInPlaceActiveObject* pActiveObject,
                        __unused IOleCommandTarget* pCommandTarget, __unused IOleInPlaceFrame* pFrame,
                        __unused IOleInPlaceUIWindow* pDoc) override {
        return S_FALSE;
    }
    STDMETHODIMP HideUI() override {
        return E_NOTIMPL;
    }
    STDMETHODIMP UpdateUI() override {
        return E_NOTIMPL;
    }
    STDMETHODIMP EnableModeless(__unused BOOL fEnable) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP OnDocWindowActivate(__unused BOOL fActivate) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP OnFrameWindowActivate(__unused BOOL fActivate) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP ResizeBorder(__unused LPCRECT prcBorder, __unused IOleInPlaceUIWindow* pUIWindow,
                              __unused BOOL fRameWindow) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP TranslateAccelerator(__unused LPMSG lpMsg, __unused const GUID* pguidCmdGroup, __unused DWORD nCmdID) {
        return S_FALSE;
    }
    STDMETHODIMP GetOptionKeyPath(__unused LPOLESTR* pchKey, __unused DWORD dw) override {
        return S_FALSE;
    }
    STDMETHODIMP GetDropTarget(__unused IDropTarget* pDropTarget, IDropTarget** ppDropTarget) override {
        return fs->QueryInterface(IID_PPV_ARGS(ppDropTarget));
    }
    STDMETHODIMP GetExternal(IDispatch** ppDispatch) override {
        if (ppDispatch) {
            *ppDispatch = nullptr;
        }
        return S_FALSE;
    }
    STDMETHODIMP TranslateUrl(__unused DWORD dwTranslate, __unused OLECHAR* pchURLIn,
                              __unused OLECHAR** ppchURLOut) override {
        return S_FALSE;
    }
    STDMETHODIMP FilterDataObject(__unused IDataObject* pDO, IDataObject** ppDORet) override {
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
    ~HW_IDropTarget() = default;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() override {
        return fs->Release();
    }

    STDMETHODIMP DragEnter(IDataObject* pDataObj, __unused DWORD grfKeyState, __unused POINTL pt,
                           DWORD* pdwEffect) override {
        HRESULT hr = fs->htmlWindow->OnDragEnter(pDataObj);
        if (SUCCEEDED(hr)) {
            *pdwEffect = DROPEFFECT_COPY;
        }
        return hr;
    }
    STDMETHODIMP DragOver(__unused DWORD grfKeyState, __unused POINTL pt, DWORD* pdwEffect) override {
        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }
    STDMETHODIMP DragLeave() override {
        return S_OK;
    }
    STDMETHODIMP Drop(IDataObject* pDataObj, __unused DWORD grfKeyState, __unused POINTL pt,
                      DWORD* pdwEffect) override {
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
    LONG refCount = 1;

  public:
    HW_IDownloadManager() = default;
    ~HW_IDownloadManager() = default;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        static const QITAB qit[] = {QITABENT(HW_IDownloadManager, IDownloadManager), {nullptr}};
        return QISearch(this, qit, riid, ppv);
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&refCount);
    }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG res = InterlockedDecrement(&refCount);
        CrashIf(res < 0);
        if (0 == res) {
            delete this;
        }
        return res;
    }

    // IDownloadManager
    STDMETHODIMP Download(IMoniker __RPC_FAR* pmk, IBindCtx __RPC_FAR* pbc, __unused DWORD dwBindVerb,
                          __unused LONG grfBINDF, __unused BINDINFO __RPC_FAR* pBindInfo, __unused LPCOLESTR pszHeaders,
                          __unused LPCOLESTR pszRedir, __unused UINT uiCP) override {
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
        char* urlRestA = ToUtf8Temp(urlRest);
        auto data = win->htmlWinCb->GetDataForUrl(urlRestA);
        if (data.empty()) {
            return INET_E_DATA_NOT_AVAILABLE;
        }
        // ask the UI to let the user save the file
        win->htmlWinCb->DownloadData(urlRestA, data);
        return S_OK;
    }
};

class HW_IServiceProvider : public IServiceProvider {
    FrameSite* fs;

  public:
    explicit HW_IServiceProvider(FrameSite* fs) : fs(fs) {
    }
    ~HW_IServiceProvider() = default;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override {
        return fs->QueryInterface(iid, ppvObject);
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return fs->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() override {
        return fs->Release();
    }

    // IServiceProvider
    STDMETHODIMP QueryService(REFGUID guidService, REFIID riid, void** ppv) override {
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

    HRESULT SetHtml(const ByteSlice&);
    HRESULT SetBaseUrl(const WCHAR* baseUrl);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IMoniker
    STDMETHODIMP BindToStorage(IBindCtx* pbc, IMoniker* pmkToLeft, REFIID riid, void** ppvObj) override;
    STDMETHODIMP GetDisplayName(IBindCtx* pbc, IMoniker* pmkToLeft, LPOLESTR* ppszDisplayName) override;
    STDMETHODIMP BindToObject(__unused IBindCtx* pbc, __unused IMoniker* pmkToLeft, __unused REFIID riidResult,
                              __unused void** ppvResult) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP Reduce(__unused IBindCtx* pbc, __unused DWORD dwReduceHowFar, __unused IMoniker** ppmkToLeft,
                        __unused IMoniker** ppmkReduced) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP ComposeWith(__unused IMoniker* pmkRight, __unused BOOL fOnlyIfNotGeneric,
                             __unused IMoniker** ppmkComposite) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP Enum(__unused BOOL fForward, __unused IEnumMoniker** ppenumMoniker) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP IsEqual(__unused IMoniker* pmkOtherMoniker) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP Hash(__unused DWORD* pdwHash) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP IsRunning(__unused IBindCtx* pbc, __unused IMoniker* pmkToLeft,
                           __unused IMoniker* pmkNewlyRunning) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP GetTimeOfLastChange(__unused IBindCtx* pbc, __unused IMoniker* pmkToLeft,
                                     __unused FILETIME* pFileTime) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP Inverse(__unused IMoniker** ppmk) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP CommonPrefixWith(__unused IMoniker* pmkOther, __unused IMoniker** ppmkPrefix) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP RelativePathTo(__unused IMoniker* pmkOther, __unused IMoniker** ppmkRelPath) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP ParseDisplayName(IBindCtx* pbc, IMoniker* pmkToLeft, LPOLESTR pszDisplayName, ULONG* pchEaten,
                                  IMoniker** ppmkOut) override;
    STDMETHODIMP IsSystemMoniker(DWORD* pdwMksys) override {
        if (!pdwMksys) {
            return E_POINTER;
        }
        *pdwMksys = MKSYS_NONE;
        return S_OK;
    }

    // IPersistStream methods
    STDMETHODIMP Save(__unused IStream* pStm, __unused BOOL fClearDirty) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP IsDirty() override {
        return E_NOTIMPL;
    }
    STDMETHODIMP Load(__unused IStream* pStm) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP GetSizeMax(__unused ULARGE_INTEGER* pcbSize) override {
        return E_NOTIMPL;
    }

    // IPersist
    STDMETHODIMP GetClassID(__unused CLSID* pClassID) override {
        return E_NOTIMPL;
    }

  private:
    LONG refCount = 1;

    char* htmlData = nullptr;
    IStream* htmlStream = nullptr;

    WCHAR* baseUrl = nullptr;
};

HtmlMoniker::HtmlMoniker() = default;

HtmlMoniker::~HtmlMoniker() {
    if (htmlStream) {
        htmlStream->Release();
    }

    free(htmlData);
    free(baseUrl);
}

HRESULT HtmlMoniker::SetHtml(const ByteSlice& d) {
    free(htmlData);
    htmlData = str::Dup(d);
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

STDMETHODIMP HtmlMoniker::BindToStorage(__unused IBindCtx* pbc, __unused IMoniker* pmkToLeft, REFIID riid,
                                        void** ppvObj) {
    LARGE_INTEGER seek{};
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

STDMETHODIMP HtmlMoniker::GetDisplayName(__unused IBindCtx* pbc, __unused IMoniker* pmkToLeft,
                                         LPOLESTR* ppszDisplayName) {
    if (!ppszDisplayName) {
        return E_POINTER;
    }
    *ppszDisplayName = OleStrDup(baseUrl ? baseUrl : L"");
    return *ppszDisplayName ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP HtmlMoniker::ParseDisplayName(__unused IBindCtx* pbc, __unused __unused IMoniker* pmkToLeft,
                                           __unused LPOLESTR pszDisplayName, __unused ULONG* pchEaten,
                                           __unused IMoniker** ppmkOut) {
    return E_NOTIMPL;
}

STDMETHODIMP HtmlMoniker::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {QITABENT(HtmlMoniker, IMoniker),
                                QITABENT(HtmlMoniker, IPersistStream),
                                QITABENT(HtmlMoniker, IPersist),
                                {nullptr}};
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

HtmlWindow::HtmlWindow(HWND parent, HtmlWindowCallback* cb) {
    CrashIf(!parent);
    hwndParent = parent;
    htmlWinCb = cb;
    RegisterInternetProtocolFactory();
    windowId = GenNewWindowId(this);
    zoomDPI = DpiGet(parent);
    if (zoomDPI < 96) {
        zoomDPI = 96;
    }
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
        ReportIf(refCount != 0);
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
    HwndSetVisibility(hwndParent, visible);
    if (webBrowser) {
        webBrowser->put_Visible(visible ? VARIANT_TRUE : VARIANT_FALSE);
    }
}

// Use for urls for which data will be provided by HtmlWindowCallback::GetHtmlForUrl()
// (will be called from OnBeforeNavigate())
void HtmlWindow::NavigateToDataUrl(const char* url) {
    TempStr fullUrl = str::FormatTemp("its://%d/%s", windowId, url);
    NavigateToUrl(fullUrl);
}

void HtmlWindow::NavigateToUrl(const char* urlA) {
    WCHAR* url = ToWStrTemp(urlA);
    VARIANT urlVar;
    VariantInitBstr(urlVar, url);
    currentURL.Reset();
    webBrowser->Navigate2(&urlVar, nullptr, nullptr, nullptr, nullptr);
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
    VARIANT vtOut{};
    HRESULT hr = webBrowser->ExecWB(OLECMDID_OPTICAL_ZOOM, OLECMDEXECOPT_DONTPROMPTUSER, nullptr, &vtOut);
    if (FAILED(hr)) {
        return 100;
    }
    int zoom = vtOut.lVal;
    CrashIf(zoomDPI < 96);
    zoom = (zoom * 96) / zoomDPI; // undo what we do in SetZoomPercent()
    return zoom;
}

void HtmlWindow::SetZoomPercent(int zoom) {
    VARIANT vtIn{};
    VARIANT vtOut{};
    CrashIf(zoomDPI < 96);
    zoom = (zoom * zoomDPI) / 96;
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
    NavigateToUrl("about:blank");
}

void HtmlWindow::SetHtml(const ByteSlice& d, const char* url) {
    FreeHtmlSetInProgressData();
    str::ReplaceWithCopy(&htmlSetInProgress, d);
    str::ReplaceWithCopy(&htmlSetInProgress, url);
    NavigateToAboutBlank();
    // the real work will happen in OnDocumentComplete()
}

// TODO: we don't call OnDocumentComplete() on this because the url
// reports "about:blank" and we suppress that. Figure out a way
// to fix that (change the url somehow?)
// TODO: IHtmlDocument2->write() seems like a simpler method
// http://www.codeproject.com/Articles/3365/Embed-an-HTML-control-in-your-own-window-using-pla#BUFFER
// https://github.com/ReneNyffenegger/development_misc/blob/master/windows/mshtml/HTMLWindow.cpp#L143
void HtmlWindow::SetHtmlReal(const ByteSlice& d) {
    if (htmlContent) {
        htmlContent->Release();
    }
    htmlContent = new HtmlMoniker();
    htmlContent->SetHtml(d);
    TempStr baseUrl = str::FormatTemp(HW_PROTO_PREFIXA "://%d/", windowId);
    htmlContent->SetBaseUrl(ToWStrTemp(baseUrl));

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
bool HtmlWindow::OnBeforeNavigate(const WCHAR* urlW, bool newWindow) {
    currentURL.Reset();
    if (!htmlWinCb) {
        return true;
    }
    char* url = ToUtf8Temp(urlW);
    if (IsBlankUrl(url)) {
        return true;
    }

    // if it's url for our internal protocol, strip the protocol
    // part as we don't want to expose it to clients.
    int protoWindowId;
    AutoFreeStr urlReal(str::Dup(url));
    bool ok = ParseProtoUrl(url, &protoWindowId, &urlReal);
    CrashIf(ok && (protoWindowId != windowId));
    bool shouldNavigate = htmlWinCb->OnBeforeNavigate(urlReal, newWindow);
    return shouldNavigate;
}

void HtmlWindow::FreeHtmlSetInProgressData() {
    str::FreePtr(&this->htmlSetInProgress);
    str::FreePtr(&this->htmlSetInProgressUrl);
}

void HtmlWindow::OnDocumentComplete(const WCHAR* urlW) {
    char* url = ToUtf8Temp(urlW);
    if (IsBlankUrl(url)) {
        if (htmlSetInProgress != nullptr) {
            // TODO: I think this triggers another OnDocumentComplete() for "about:blank",
            // which we should ignore?
            SetHtmlReal(ToByteSlice(htmlSetInProgress));
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
    AutoFreeStr urlReal(str::Dup(url));
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
    STGMEDIUM stg{};
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
    STGMEDIUM stg{};
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

HRESULT HW_DWebBrowserEvents2::Invoke(DISPID dispIdMember, __unused REFIID riid, __unused LCID lcid, WORD flags,
                                      DISPPARAMS* pDispParams, VARIANT* pVarResult, __unused EXCEPINFO* pExcepInfo,
                                      __unused unsigned int* puArgErr) {
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

HRESULT HW_IOleInPlaceSiteWindowless::OnUIDeactivate(__unused BOOL fUndoable) {
    fs->uiActive = false;
    return S_OK;
}

HRESULT HW_IOleInPlaceSiteWindowless::OnInPlaceDeactivate() {
    fs->inPlaceActive = false;
    return S_OK;
}

// IOleInPlaceSiteEx
HRESULT HW_IOleInPlaceSiteWindowless::OnInPlaceActivateEx(BOOL* pfNoRedraw, __unused DWORD dwFlags) {
    if (pfNoRedraw) {
        *pfNoRedraw = FALSE;
    }
    return S_OK;
}

// IOleInPlaceSiteWindowless
HRESULT HW_IOleInPlaceSiteWindowless::CanWindowlessActivate() {
    return fs->supportsWindowlessActivation ? S_OK : S_FALSE;
}

HRESULT HW_IOleInPlaceSiteWindowless::GetDC(__unused LPCRECT pRect, __unused DWORD grfFlags, HDC* phDC) {
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

HRESULT HW_IOleInPlaceSiteWindowless::InvalidateRect(__unused LPCRECT pRect, BOOL fErase) {
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
HRESULT HW_IOleItemContainer::GetObject(LPOLESTR pszItem, __unused DWORD dwSpeedNeeded, __unused IBindCtx* pbc,
                                        __unused REFIID riid, void** ppvObject) {
    if (pszItem == nullptr) {
        return E_INVALIDARG;
    }
    if (ppvObject == nullptr) {
        return E_INVALIDARG;
    }
    *ppvObject = nullptr;
    return MK_E_NOOBJECT;
}

HRESULT HW_IOleItemContainer::GetObjectStorage(LPOLESTR pszItem, __unused IBindCtx* pbc, __unused REFIID riid,
                                               void** ppvStorage) {
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

HRESULT HW_IOleControlSite::TransformCoords(POINTL* pPtlHimetric, POINTF* pPtfContainer, __unused DWORD dwFlags) {
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
HRESULT HW_IOleCommandTarget::QueryStatus(__unused const GUID* pguidCmdGroup, __unused ULONG cCmds, OLECMD* prgCmds,
                                          __unused OLECMDTEXT* pCmdTet) {
    if (prgCmds == nullptr) {
        return E_INVALIDARG;
    }
    return OLECMDERR_E_UNKNOWNGROUP;
}
