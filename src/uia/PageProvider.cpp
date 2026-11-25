/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "uia/Common.h"
#include "uia/DocumentProvider.h"
#include "uia/Provider.h"
#include "uia/PageProvider.h"

SumatraUIAutomationPageProvider::SumatraUIAutomationPageProvider(int pageNum, HWND canvasHwnd, DisplayModel* dm,
                                                                 SumatraUIAutomationDocumentProvider* root)
    : refCount(1), pageNum(pageNum), canvasHwnd(canvasHwnd), dm(dm), root(root), released(false) {
    // root->AddRef(); Don't add refs to our parent & owner.
}

SumatraUIAutomationPageProvider::~SumatraUIAutomationPageProvider() = default;

int SumatraUIAutomationPageProvider::GetPageNum() const {
    return pageNum;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {QITABENT(SumatraUIAutomationPageProvider, IRawElementProviderSimple),
                                QITABENT(SumatraUIAutomationPageProvider, IRawElementProviderFragment),
                                QITABENT(SumatraUIAutomationPageProvider, IValueProvider),
                                {nullptr}};
    return QISearch(this, qit, riid, ppv);
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationPageProvider::AddRef() {
    return AtomicIntInc(&refCount);
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationPageProvider::Release() {
    return ComRelease(&refCount, [this] { delete this; });
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::Navigate(enum NavigateDirection direction,
                                                                    IRawElementProviderFragment** pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }

    // disallow traverse if we are lingering
    if (released) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }

    *pRetVal = nullptr;
    if (direction == NavigateDirection_PreviousSibling) {
        *pRetVal = root->GetPage(pageNum - 1);
    } else if (direction == NavigateDirection_NextSibling) {
        *pRetVal = root->GetPage(pageNum + 1);
    } else if (direction == NavigateDirection_FirstChild || direction == NavigateDirection_LastChild) {
        // do nothing
    } else if (direction == NavigateDirection_Parent) {
        *pRetVal = root;
    } else {
        return E_INVALIDARG;
    }

    if (*pRetVal) {
        (*pRetVal)->AddRef();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::GetRuntimeId(SAFEARRAY** pRetVal) {
    return UiaMakeRuntimeId(canvasHwnd, kSumatraUiaPageRuntimeId(pageNum), pRetVal);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::GetEmbeddedFragmentRoots(SAFEARRAY** pRetVal) {
    return UiaReturnNull(pRetVal);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::SetFocus() {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::get_BoundingRectangle(struct UiaRect* pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }

    if (released) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }

    // some engines might not support GetPageInfo
    const PageInfo* page = dm->GetPageInfo(pageNum);
    if (!page) {
        return E_FAIL;
    }

    Point tl = HwndClientToScreen(canvasHwnd, page->pageOnScreen.TL());

    pRetVal->left = tl.x;
    pRetVal->top = tl.y;
    pRetVal->width = page->pageOnScreen.dx;
    pRetVal->height = page->pageOnScreen.dy;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::get_FragmentRoot(IRawElementProviderFragmentRoot** pRetVal) {
    if (released) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }

    // let our parent to handle this
    return root->get_FragmentRoot(pRetVal);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::GetPatternProvider(PATTERNID patternId, IUnknown** pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }

    if (patternId == UIA_ValuePatternId) {
        *pRetVal = static_cast<IValueProvider*>(this);
        AddRef();
        return S_OK;
    }

    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::GetPropertyValue(PROPERTYID propertyId, VARIANT* pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }

    if (propertyId == UIA_NamePropertyId) {
        TempStr s = fmt("Page %d", pageNum);
        return UiaSetBstr(pRetVal, CWStrTemp(s));
    } else if (propertyId == UIA_IsValuePatternAvailablePropertyId) {
        // VARIANT_TRUE (-1), not TRUE (1) - see DocumentProvider::GetPropertyValue
        pRetVal->vt = VT_BOOL;
        pRetVal->boolVal = VARIANT_TRUE;
        return S_OK;
    }

    pRetVal->vt = VT_EMPTY;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
SumatraUIAutomationPageProvider::get_HostRawElementProvider(IRawElementProviderSimple** pRetVal) {
    return UiaReturnNull(pRetVal);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::get_ProviderOptions(ProviderOptions* pRetVal) {
    return UiaProviderOptions(pRetVal);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::SetValue(__unused LPCWSTR val) {
    return E_ACCESSDENIED;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::get_Value(BSTR* pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }
    if (released) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }

    Str pageContent = dm->GetEngine()->GetTextForPage(pageNum);
    if (len(pageContent) == 0) {
        *pRetVal = nullptr;
        return S_OK;
    }

    TempWStr pageContentW = ToWStrTemp(pageContent);
    *pRetVal = SysAllocString(pageContentW.s);
    return *pRetVal ? S_OK : E_OUTOFMEMORY;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::get_IsReadOnly(BOOL* pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }

    *pRetVal = TRUE;
    return S_OK;
}
