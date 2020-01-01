/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>
#include "uia/StartPageProvider.h"
#include "uia/Constants.h"
#include "uia/Provider.h"

SumatraUIAutomationStartPageProvider::SumatraUIAutomationStartPageProvider(HWND canvasHwnd, SumatraUIAutomationProvider* root) :
    refCount(1), canvasHwnd(canvasHwnd), root(root)
{
    //root->AddRef(); Don't add refs to our parent & owner.
}

SumatraUIAutomationStartPageProvider::~SumatraUIAutomationStartPageProvider()
{
}

// IUnknown
HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::QueryInterface(REFIID riid, void **ppv)
{
    static const QITAB qit[] = {
        QITABENT(SumatraUIAutomationStartPageProvider, IRawElementProviderSimple),
        QITABENT(SumatraUIAutomationStartPageProvider, IRawElementProviderFragment),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::AddRef(void)
{
    return InterlockedIncrement(&refCount);
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::Release(void)
{
    LONG res = InterlockedDecrement(&refCount);
    CrashIf(res < 0);
    if (0 == res)
        delete this;
    return res;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::Navigate(enum NavigateDirection direction, IRawElementProviderFragment **pRetVal)
{
    if (pRetVal == nullptr)
        return E_POINTER;

    *pRetVal = nullptr;
    // no siblings, no children
    if (direction == NavigateDirection_NextSibling ||
        direction == NavigateDirection_PreviousSibling ||
        direction == NavigateDirection_FirstChild ||
        direction == NavigateDirection_LastChild) {
        return S_OK;
    } else if (direction == NavigateDirection_Parent) {
        *pRetVal = root;
        (*pRetVal)->AddRef();
        return S_OK;
    } else {
        return E_INVALIDARG;
    }
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::GetRuntimeId(SAFEARRAY **pRetVal)
{
    if (pRetVal == nullptr)
        return E_POINTER;

    SAFEARRAY *psa = SafeArrayCreateVector(VT_I4, 0, 2);
    if (!psa)
        return E_OUTOFMEMORY;

    // RuntimeID magic, use hwnd to differentiate providers of different windows
    int rId[] = { (int)canvasHwnd, SUMATRA_UIA_STARTPAGE_RUNTIME_ID };
    for (LONG i = 0; i < 2; i++) {
        HRESULT hr = SafeArrayPutElement(psa, &i, (void*)&(rId[i]));
        CrashIf(FAILED(hr));
    }

    *pRetVal = psa;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::GetEmbeddedFragmentRoots(SAFEARRAY **pRetVal)
{
    if (pRetVal == nullptr)
        return E_POINTER;

    // no other roots => return nullptr
    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::SetFocus(void)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::get_BoundingRectangle(struct UiaRect *pRetVal)
{
    // share area with the canvas uia provider
    return root->get_BoundingRectangle(pRetVal);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::get_FragmentRoot(IRawElementProviderFragmentRoot **pRetVal)
{
    if (pRetVal == nullptr)
        return E_POINTER;

    *pRetVal = root;
    root->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::GetPatternProvider(PATTERNID patternId,IUnknown **pRetVal)
{
    UNUSED(patternId);
    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::GetPropertyValue(PROPERTYID propertyId,VARIANT *pRetVal)
{
    if (propertyId == UIA_NamePropertyId) {
        pRetVal->vt = VT_BSTR;
        pRetVal->bstrVal = SysAllocString(L"Start Page");
        return S_OK;
    }

    pRetVal->vt = VT_EMPTY;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::get_HostRawElementProvider(IRawElementProviderSimple **pRetVal)
{
    if (pRetVal == nullptr)
        return E_POINTER;
    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::get_ProviderOptions(ProviderOptions *pRetVal)
{
    if (pRetVal == nullptr)
        return E_POINTER;
    *pRetVal = ProviderOptions_ServerSideProvider;
    return S_OK;
}
