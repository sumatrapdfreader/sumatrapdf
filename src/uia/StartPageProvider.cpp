/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>
#include "uia/Constants.h"
#include "uia/Common.h"
#include "uia/Provider.h"
#include "uia/StartPageProvider.h"

SumatraUIAutomationStartPageProvider::SumatraUIAutomationStartPageProvider(HWND canvasHwnd,
                                                                           SumatraUIAutomationProvider* root)
    : refCount(1), canvasHwnd(canvasHwnd), root(root) {
    // root->AddRef(); Don't add refs to our parent & owner.
}

SumatraUIAutomationStartPageProvider::~SumatraUIAutomationStartPageProvider() = default;

// IUnknown
HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {QITABENT(SumatraUIAutomationStartPageProvider, IRawElementProviderSimple),
                                QITABENT(SumatraUIAutomationStartPageProvider, IRawElementProviderFragment),
                                {nullptr}};
    return QISearch(this, qit, riid, ppv);
}

// clients may keep this provider after the root that owns it is gone
void SumatraUIAutomationStartPageProvider::DetachRoot() {
    root = nullptr;
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::AddRef() {
    return AtomicIntInc(&refCount);
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::Release() {
    return ComRelease(&refCount, [this] { delete this; });
}

// IRawElementProviderFragment
HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::Navigate(enum NavigateDirection direction,
                                                                         IRawElementProviderFragment** pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }

    *pRetVal = nullptr;
    // no siblings, no children
    if (direction == NavigateDirection_NextSibling || direction == NavigateDirection_PreviousSibling ||
        direction == NavigateDirection_FirstChild || direction == NavigateDirection_LastChild) {
        return S_OK;
    } else if (direction == NavigateDirection_Parent) {
        if (!root) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        *pRetVal = root;
        (*pRetVal)->AddRef();
        return S_OK;
    } else {
        return E_INVALIDARG;
    }
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::GetRuntimeId(SAFEARRAY** pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }

    SAFEARRAY* psa = SafeArrayCreateVector(VT_I4, 0, 2);
    if (!psa) {
        return E_OUTOFMEMORY;
    }

    // RuntimeID magic, use hwnd to differentiate providers of different windows
    int rId[] = {HandleToLong(canvasHwnd), kSumatraUiaStartPageRuntimeId};
    for (LONG i = 0; i < 2; i++) {
        HRESULT hr = SafeArrayPutElement(psa, &i, (void*)&(rId[i]));
        ReportIf(FAILED(hr));
    }

    *pRetVal = psa;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::GetEmbeddedFragmentRoots(SAFEARRAY** pRetVal) {
    return UiaReturnNull(pRetVal);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::SetFocus() {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::get_BoundingRectangle(struct UiaRect* pRetVal) {
    return UiaRootRect(root, pRetVal);
}

HRESULT STDMETHODCALLTYPE
SumatraUIAutomationStartPageProvider::get_FragmentRoot(IRawElementProviderFragmentRoot** pRetVal) {
    return UiaRootFragmentRoot(root, pRetVal);
}

// IRawElementProviderSimple
HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::GetPatternProvider(__unused PATTERNID patternId,
                                                                                   IUnknown** pRetVal) {
    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::GetPropertyValue(PROPERTYID propertyId,
                                                                                 VARIANT* pRetVal) {
    if (propertyId == UIA_NamePropertyId) {
        pRetVal->vt = VT_BSTR;
        pRetVal->bstrVal = SysAllocString(L"Start Page");
        return S_OK;
    }

    pRetVal->vt = VT_EMPTY;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
SumatraUIAutomationStartPageProvider::get_HostRawElementProvider(IRawElementProviderSimple** pRetVal) {
    return UiaReturnNull(pRetVal);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::get_ProviderOptions(ProviderOptions* pRetVal) {
    return UiaProviderOptions(pRetVal);
}
