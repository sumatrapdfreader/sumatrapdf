/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/WinDynCalls.h"
#include "utils/DbgHelpDyn.h"
#include "utils/WinUtil.h"
#include "uia/Provider.h"
#include "uia/Constants.h"
#include "uia/DocumentProvider.h"
#include "uia/StartPageProvider.h"

SumatraUIAutomationProvider::SumatraUIAutomationProvider(HWND hwnd) :
    refCount(1), canvasHwnd(hwnd), startpage(nullptr), document(nullptr)
{
    startpage = new SumatraUIAutomationStartPageProvider(hwnd, this);
}

SumatraUIAutomationProvider::~SumatraUIAutomationProvider()
{
    if (document) {
        document->FreeDocument(); // tell that the dm is now invalid
        document->Release(); // release our hooks
        document = nullptr;
    }

    startpage->Release();
    startpage = nullptr;
}

void SumatraUIAutomationProvider::OnDocumentLoad(DisplayModel *dm)
{
    AssertCrash(!document);

    document = new SumatraUIAutomationDocumentProvider(canvasHwnd, this);
    document->LoadDocument(dm);
    uia::RaiseStructureChangedEvent(this, StructureChangeType_ChildrenInvalidated, nullptr, 0);
}

void SumatraUIAutomationProvider::OnDocumentUnload()
{
    if (document) {
        document->FreeDocument(); // tell that the dm is now invalid
        document->Release(); // release our hooks
        document = nullptr;
        uia::RaiseStructureChangedEvent(this, StructureChangeType_ChildrenInvalidated, nullptr, 0);
    }
}

void SumatraUIAutomationProvider::OnSelectionChanged()
{
    if (document)
        uia::RaiseAutomationEvent(document, UIA_Text_TextSelectionChangedEventId);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::QueryInterface(REFIID riid, void **ppv)
{
    static const QITAB qit[] = {
        QITABENT(SumatraUIAutomationProvider, IRawElementProviderSimple),
        QITABENT(SumatraUIAutomationProvider, IRawElementProviderFragment),
        QITABENT(SumatraUIAutomationProvider, IRawElementProviderFragmentRoot),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationProvider::AddRef(void)
{
    return InterlockedIncrement(&refCount);
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationProvider::Release(void)
{
    LONG res = InterlockedDecrement(&refCount);
    CrashIf(res < 0);
    if (0 == res)
        delete this;
    return res;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetPatternProvider(PATTERNID patternId,IUnknown **pRetVal)
{
    UNUSED(patternId);
    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetPropertyValue(PROPERTYID propertyId,VARIANT *pRetVal)
{
    if (pRetVal == nullptr)
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
    if (pRetVal == nullptr)
        return E_POINTER;
    *pRetVal = ProviderOptions_ServerSideProvider;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::Navigate(enum NavigateDirection direction, IRawElementProviderFragment **pRetVal)
{
    if (pRetVal == nullptr)
        return E_POINTER;

    *pRetVal = nullptr;
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
    if (pRetVal == nullptr)
        return E_POINTER;

    // top-level elements should return nullptr
    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetEmbeddedFragmentRoots(SAFEARRAY **pRetVal)
{
    if (pRetVal == nullptr)
        return E_POINTER;

    // no other roots => return nullptr
    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::SetFocus(void)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::get_BoundingRectangle(struct UiaRect *pRetVal)
{
    if (pRetVal == nullptr)
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
    if (pRetVal == nullptr)
        return E_POINTER;

    *pRetVal = this;
    (*pRetVal)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::ElementProviderFromPoint(double x,double y,IRawElementProviderFragment **pRetVal)
{
    if (pRetVal == nullptr)
        return E_POINTER;

    *pRetVal = this->GetElementFromPoint(x,y,this);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetFocus(IRawElementProviderFragment **pRetVal)
{
    if (pRetVal == nullptr)
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
        return nullptr;

    // check the children
    IRawElementProviderFragment* it;
    HRESULT hr = root->Navigate(NavigateDirection_FirstChild, &it);

    while (SUCCEEDED(hr) && it) {
        UiaRect rect;
        hr = it->get_BoundingRectangle(&rect);

        // step into
        if (SUCCEEDED(hr) && rect.left <= x && x <= rect.left + rect.width && rect.top <= y && y <= rect.top + rect.height) {
            IRawElementProviderFragment* leaf = GetElementFromPoint(x, y, it);
            it->Release();
            return leaf;
        }

        // go to next element, release old one
        IRawElementProviderFragment* old_it = it;
        hr = old_it->Navigate(NavigateDirection_NextSibling, &it);
        old_it->Release();
    }

    // no such child
    root->AddRef();
    return root;
}
