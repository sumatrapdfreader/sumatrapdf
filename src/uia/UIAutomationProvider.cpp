/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "UIAutomationProvider.h"

#include "UIAutomationConstants.h"
#include "UIAutomationDocumentProvider.h"
#include "UIAutomationStartPageProvider.h"
#include "WinUtil.h"

// not available under Win2000
typedef LRESULT (WINAPI *UiaReturnRawElementProviderProc)(HWND hwnd, WPARAM wParam, LPARAM lParam, IRawElementProviderSimple *el);
typedef HRESULT (WINAPI *UiaHostProviderFromHwndProc)(HWND hwnd, IRawElementProviderSimple ** pProvider);
typedef HRESULT (WINAPI *UiaRaiseAutomationEventProc)(IRawElementProviderSimple * pProvider, EVENTID id);
typedef HRESULT (WINAPI *UiaRaiseStructureChangedEventProc)(IRawElementProviderSimple * pProvider, StructureChangeType structureChangeType, int * pRuntimeId, int cRuntimeIdLen);
typedef HRESULT (WINAPI *UiaGetReservedNotSupportedValueProc)(IUnknown **punkNotSupportedValue);

namespace uia {

static bool gFuncsLoaded = false;
static UiaReturnRawElementProviderProc _UiaReturnRawElementProvider = NULL;
static UiaHostProviderFromHwndProc _UiaHostProviderFromHwnd = NULL;
static UiaRaiseAutomationEventProc _UiaRaiseAutomationEvent = NULL;
static UiaRaiseStructureChangedEventProc _UiaRaiseStructureChangedEvent = NULL;
static UiaGetReservedNotSupportedValueProc _UiaGetReservedNotSupportedValue = NULL;

static void Initialize()
{
    static bool funcsLoaded = false;
    if (funcsLoaded)
        return;

    HMODULE h = SafeLoadLibrary(L"uiautomationcore.dll");
#define Load(func) _ ## func = (func ## Proc)GetProcAddress(h, #func)
    Load(UiaReturnRawElementProvider);
    Load(UiaHostProviderFromHwnd);
    Load(UiaRaiseAutomationEvent);
    Load(UiaRaiseStructureChangedEvent);
    Load(UiaGetReservedNotSupportedValue);
#undef Load

    funcsLoaded = true;
}

LRESULT ReturnRawElementProvider(HWND hwnd, WPARAM wParam, LPARAM lParam, IRawElementProviderSimple *provider)
{
    Initialize();
    if (!_UiaReturnRawElementProvider)
        return 0;
    return _UiaReturnRawElementProvider(hwnd, wParam, lParam, provider);
}

HRESULT HostProviderFromHwnd(HWND hwnd, IRawElementProviderSimple ** pProvider)
{
    Initialize();
    if (!_UiaHostProviderFromHwnd)
        return E_NOTIMPL;
    return _UiaHostProviderFromHwnd(hwnd, pProvider);
}

HRESULT RaiseAutomationEvent(IRawElementProviderSimple * pProvider, EVENTID id)
{
    Initialize();
    if (!_UiaRaiseAutomationEvent)
        return E_NOTIMPL;
    return _UiaRaiseAutomationEvent(pProvider, id);
}

HRESULT RaiseStructureChangedEvent(IRawElementProviderSimple * pProvider, StructureChangeType structureChangeType, int * pRuntimeId, int cRuntimeIdLen)
{
    Initialize();
    if (!_UiaRaiseStructureChangedEvent)
        return E_NOTIMPL;
    return _UiaRaiseStructureChangedEvent(pProvider, structureChangeType, pRuntimeId, cRuntimeIdLen);
}

HRESULT GetReservedNotSupportedValue(IUnknown **punkNotSupportedValue)
{
    Initialize();
    if (!_UiaRaiseStructureChangedEvent)
        return E_NOTIMPL;
    return _UiaGetReservedNotSupportedValue(punkNotSupportedValue);
}

};

SumatraUIAutomationProvider::SumatraUIAutomationProvider(HWND hwnd) :
    refCount(1), canvasHwnd(hwnd), startpage(NULL), document(NULL)
{
    startpage = new SumatraUIAutomationStartPageProvider(hwnd, this);
}

SumatraUIAutomationProvider::~SumatraUIAutomationProvider()
{
    if (document) {
        document->FreeDocument(); // tell that the dm is now invalid
        document->Release(); // release our hooks
        document = NULL;
    }

    startpage->Release();
    startpage = NULL;
}

void SumatraUIAutomationProvider::OnDocumentLoad(DisplayModel *dm)
{
    AssertCrash(!document);

    document = new SumatraUIAutomationDocumentProvider(canvasHwnd, this);
    document->LoadDocument(dm);
    uia::RaiseStructureChangedEvent(this, StructureChangeType_ChildrenInvalidated, NULL, 0);
}

void SumatraUIAutomationProvider::OnDocumentUnload()
{
    if (document) {
        document->FreeDocument(); // tell that the dm is now invalid
        document->Release(); // release our hooks
        document = NULL;
        uia::RaiseStructureChangedEvent(this, StructureChangeType_ChildrenInvalidated, NULL, 0);
    }
}

void SumatraUIAutomationProvider::OnSelectionChanged()
{
    if (document)
        uia::RaiseAutomationEvent(document, UIA_Text_TextSelectionChangedEventId);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::QueryInterface(const IID & iid,void ** ppvObject)
{
    if (ppvObject == NULL)
        return E_POINTER;

    // TODO: per http://blogs.msdn.com/b/oldnewthing/archive/2004/03/26/96777.aspx should
    // respond to IUnknown
    if (iid == __uuidof(IRawElementProviderSimple)) {
        *ppvObject = static_cast<IRawElementProviderSimple*>(this);
        this->AddRef(); //New copy has entered the universe
        return S_OK;
    } else if (iid == __uuidof(IRawElementProviderFragment)) {
        *ppvObject = static_cast<IRawElementProviderFragment*>(this);
        this->AddRef(); //New copy has entered the universe
        return S_OK;
    } else if (iid == __uuidof(IRawElementProviderFragmentRoot)) {
        *ppvObject = static_cast<IRawElementProviderFragmentRoot*>(this);
        this->AddRef(); //New copy has entered the universe
        return S_OK;
    }

    *ppvObject = NULL;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationProvider::AddRef(void)
{
    return InterlockedIncrement(&refCount);
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationProvider::Release(void)
{
    LONG res = InterlockedDecrement(&refCount);
    CrashIf(res < 0);
    if (0 == res) {
        delete this;
    }
    return res;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetPatternProvider(PATTERNID patternId,IUnknown **pRetVal)
{
    *pRetVal = NULL;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetPropertyValue(PROPERTYID propertyId,VARIANT *pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;

    if (propertyId == UIA_NamePropertyId) {
        pRetVal->vt = VT_BSTR;
        pRetVal->bstrVal = SysAllocString(L"Canvas");
        return S_OK;
    } else if (propertyId == UIA_IsKeyboardFocusablePropertyId) {
        pRetVal->vt = VT_BOOL;
        pRetVal->boolVal = TRUE;
        return S_OK;
    } else if (propertyId == UIA_ControlTypePropertyId) {
        pRetVal->vt = VT_I4;
        pRetVal->lVal = UIA_CustomControlTypeId;
        return S_OK;
    } else if (propertyId == UIA_NativeWindowHandlePropertyId) {
        pRetVal->vt = VT_I4;
        pRetVal->lVal = (LONG)canvasHwnd;
        return S_OK;
    }

    pRetVal->vt = VT_EMPTY;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::get_HostRawElementProvider(IRawElementProviderSimple **pRetVal)
{
    return uia::HostProviderFromHwnd(canvasHwnd,pRetVal);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::get_ProviderOptions(ProviderOptions *pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;
    *pRetVal = ProviderOptions_ServerSideProvider;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::Navigate(enum NavigateDirection direction, IRawElementProviderFragment **pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;

    *pRetVal = NULL;
    // no siblings, no parent
    if (direction == NavigateDirection_Parent ||
        direction == NavigateDirection_NextSibling ||
        direction == NavigateDirection_PreviousSibling) {
        return S_OK;
    } else if (direction == NavigateDirection_FirstChild ||
             direction == NavigateDirection_LastChild) {
        // return document content element, or the start page element
        if (document)
            *pRetVal = document;
        else
            *pRetVal = startpage;

        if (*pRetVal)
            (*pRetVal)->AddRef();
        return S_OK;
    } else {
        return E_INVALIDARG;
    }
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetRuntimeId(SAFEARRAY **pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;

    // top-level elements should return NULL
    *pRetVal = NULL;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetEmbeddedFragmentRoots(SAFEARRAY **pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;

    // no other roots => return NULL
    *pRetVal = NULL;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::SetFocus(void)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::get_BoundingRectangle(struct UiaRect *pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;

    // return Bounding Rect of the Canvas area
    RECT canvas_rect;
    GetWindowRect(canvasHwnd, &canvas_rect);

    pRetVal->left   = canvas_rect.left;
    pRetVal->top    = canvas_rect.top;
    pRetVal->width  = canvas_rect.right - canvas_rect.left;
    pRetVal->height = canvas_rect.bottom - canvas_rect.top;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::get_FragmentRoot(IRawElementProviderFragmentRoot **pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;

    *pRetVal = this;
    (*pRetVal)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::ElementProviderFromPoint(double x,double y,IRawElementProviderFragment **pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;

    *pRetVal = this->GetElementFromPoint(x,y,this);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetFocus(IRawElementProviderFragment **pRetVal)
{
    if (pRetVal == NULL)
        return E_POINTER;

    // whichever child exists in the tree has the focus
    if (document)
        *pRetVal = document;
    else
        *pRetVal = startpage;
    if (*pRetVal)
        (*pRetVal)->AddRef();
    return S_OK;
}

IRawElementProviderFragment* SumatraUIAutomationProvider::GetElementFromPoint(double x,double y,IRawElementProviderFragment * root)
{
    if (!root)
        return NULL;

    // check the children
    IRawElementProviderFragment* it;
    root->Navigate(NavigateDirection_FirstChild,&it);

    while (it) {
        UiaRect rect;
        it->get_BoundingRectangle(&rect);

        // step into
        if (rect.left <= x && x <= rect.left+rect.width &&
            rect.top <= y && y <= rect.top+rect.height) {
            IRawElementProviderFragment* leaf = GetElementFromPoint(x,y,it);
            it->Release();
            return leaf;
        }

        // go to next element, release old one
        IRawElementProviderFragment* old_it = it;
        old_it->Navigate(NavigateDirection_NextSibling,&it);
        old_it->Release();
    }

    // no such child
    root->AddRef();
    return root;
}
