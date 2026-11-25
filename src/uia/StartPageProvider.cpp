/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
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
    return UiaMakeRuntimeId(canvasHwnd, kSumatraUiaStartPageRuntimeId, pRetVal);
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
    if (pRetVal == nullptr) {
        return E_POINTER;
    }
    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationStartPageProvider::GetPropertyValue(PROPERTYID propertyId,
                                                                                 VARIANT* pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }
    if (propertyId == UIA_NamePropertyId) {
        return UiaSetBstr(pRetVal, L"Start Page");
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
