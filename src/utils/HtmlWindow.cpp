/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

// http://www.codeproject.com/KB/COM/cwebpage.aspx
#include "HtmlWindow.h"

// IOleClientSite functions are called by the browser object
// to interact with the window that contains the browser object
// IOleInPlaceFrame and IOleInPlaceSite are considered to be
// sub-objects of IOleClientSite so IOleClientSite::QueryInterface
// returns them.

// http://msdn.microsoft.com/en-us/library/ms693706(v=vs.85).aspx
class OleClientSite : public IOleClientSite
{
    IOleInPlaceSite       *oleInPlaceSite;
    IDocHostUIHandler     *docHostUIHandler;
    DWebBrowserEvents2    *browserEvents;
public:
    OleClientSite(IOleInPlaceSite *oleInPlaceSite, IDocHostUIHandler *docHostUIHandler, DWebBrowserEvents2* browserEvents) :
        oleInPlaceSite(oleInPlaceSite),
        docHostUIHandler(docHostUIHandler),
        browserEvents(browserEvents)
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

    ULONG STDMETHODCALLTYPE AddRef() {
        return 1;
    }

    ULONG STDMETHODCALLTYPE Release() {
        return 1;
    }

    HRESULT STDMETHODCALLTYPE SaveObject() {
        assert(0);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker, IMoniker **ppmk)
    {
        assert(0);
        return S_OK;
    }
  
    HRESULT STDMETHODCALLTYPE GetContainer(LPOLECONTAINER *ppContainer)
    {
        *ppContainer = NULL;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE ShowObject() {
        return NOERROR;
    }

    HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL show) {
        assert(0);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE RequestNewObjectLayout() {
        assert(0);
        return S_OK;
    }
};

HRESULT OleClientSite::QueryInterface(REFIID riid, void **ppvObject)
{
    if (IID_IUnknown == riid || IID_IOleClientSite == riid) {
        *ppvObject = this;
        return S_OK;
    }

    // TODO: use == instead of memeq
    if (memeq(&riid, &IID_IUnknown, sizeof(GUID)) || 
        memeq(&riid, &IID_IOleClientSite, sizeof(GUID)))
    {
        *ppvObject = this;
        return S_OK;
    }

    if (memeq(&riid, &IID_IOleInPlaceSite, sizeof(GUID))) {
        *ppvObject = oleInPlaceSite;
        return S_OK;
    }

    if (memeq(&riid, &IID_IDocHostUIHandler, sizeof(GUID))) {
        *ppvObject = docHostUIHandler;
        return S_OK;
    }

    if (riid == DIID_DWebBrowserEvents2) {
      *ppvObject = browserEvents;
      return S_OK;
    }

    if (riid == IID_IDispatch) {
      *ppvObject = browserEvents;
      return S_OK;
    }

    *ppvObject = 0;
    return E_NOINTERFACE;
}

// http://msdn.microsoft.com/en-us/library/ms692770(VS.85).aspx
class OleInPlaceFrame : public IOleInPlaceFrame
{
    HWND hwnd;
public:
    OleInPlaceFrame(HWND hwnd) : hwnd(hwnd)
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID *ppvObj)
    {
        assert(0);
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() { 
        return 1;
    }

    ULONG STDMETHODCALLTYPE Release() {
        return 1;
    }

    HRESULT STDMETHODCALLTYPE GetWindow(HWND *lphwnd) {
        *lphwnd = hwnd;
        return S_OK;
    }
  
    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL enterMode)
    {
        assert(0);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetBorder(RECT *rectBorder) 
    {
        assert(0);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE RequestBorderSpace(LPCBORDERWIDTHS pborderwidths)
    {
        assert(0);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetBorderSpace(LPCBORDERWIDTHS pborderwidths)
    {
        assert(0);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetActiveObject(IOleInPlaceActiveObject *pActiveObject, LPCOLESTR pszObjName)
    {
        return S_OK;
    }
  
    HRESULT STDMETHODCALLTYPE InsertMenus(HMENU hmenuShared, LPOLEMENUGROUPWIDTHS lpMenuWidths)
    {
        assert(0);
        return S_OK;
    }
  
    HRESULT STDMETHODCALLTYPE SetMenu(HMENU hmenuShared, HOLEMENU holemenu, HWND hwndActiveObject)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE RemoveMenus(HMENU hmenuShared) 
    {
        assert(0);
        return S_OK;
    }
  
    HRESULT STDMETHODCALLTYPE SetStatusText(LPCOLESTR pszStatusText)
    {
        return S_OK;
    }
  
    HRESULT STDMETHODCALLTYPE EnableModeless(BOOL fEnable)
    {
        return S_OK;
    }
  
    HRESULT STDMETHODCALLTYPE TranslateAccelerator(LPMSG lpmsg, WORD wID)
    {
        assert(0);
        return S_OK;
    }
};

// http://msdn.microsoft.com/en-us/library/ms686586(VS.85).aspx
class OleInPlaceSite : public IOleInPlaceSite {
private:
    IOleClientSite    *clientSite;
    IOleInPlaceFrame  *inPlaceFrame;
    IOleObject        *browserObject;
    HWND               hwnd;

  public:
     OleInPlaceSite(IOleInPlaceFrame* inPlaceFrame, HWND hwnd) :
        clientSite(NULL), inPlaceFrame(inPlaceFrame),
        browserObject(NULL), hwnd(hwnd)
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID *ppvObj) {
      return clientSite->QueryInterface(riid, ppvObj);
    }

    ULONG STDMETHODCALLTYPE AddRef() {
        return 1;
    }
  
    ULONG STDMETHODCALLTYPE Release() {
        return 1;
    }
  
    HRESULT STDMETHODCALLTYPE GetWindow(HWND *lphwnd) {
        *lphwnd = hwnd;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL fEnterMode) {
        assert(0);
        return S_OK;
    }
  
    HRESULT STDMETHODCALLTYPE CanInPlaceActivate() {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnInPlaceActivate() {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnUIActivate() {
        return S_OK;
    }
  
    HRESULT STDMETHODCALLTYPE GetWindowContext(LPOLEINPLACEFRAME *lplpFrame,
        LPOLEINPLACEUIWINDOW *lplpDoc, LPRECT lprcPosRect, 
        LPRECT lprcClipRect, OLEINPLACEFRAMEINFO *frameInfo)
    {
        *lplpFrame = inPlaceFrame;
        *lplpDoc = NULL;
        frameInfo->fMDIApp = FALSE;
        frameInfo->hwndFrame = hwnd;
        frameInfo->haccel = 0;
        frameInfo->cAccelEntries = 0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Scroll(SIZE scrollExtent) {
        assert(0);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnUIDeactivate(BOOL fUndoable) {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate() {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DiscardUndoState() {
        assert(0);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DeactivateAndUndo() {
        assert(0);
        return S_OK;
    }
  
    // Called when the position of the browser object is changed, 
    // e.g. after IWebBrowser2::put_Width() etc.
    HRESULT STDMETHODCALLTYPE OnPosRectChange(LPCRECT lprcPosRect)
    {
        IOleInPlaceObject *inPlace;
        HRESULT res = browserObject->QueryInterface(IID_IOleInPlaceObject, (void**)&inPlace);
        if (S_OK == res) {
            inPlace->SetObjectRects(lprcPosRect, lprcPosRect);
        }
        return S_OK;
    }

    void BrowserObject(IOleObject* o) {
        browserObject = o;
    }

    void ClientSite(IOleClientSite* client) {
        clientSite = client;
    }
};

// TODO: implement IDocHostUIHandler


