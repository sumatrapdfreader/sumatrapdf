/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>
#include <OleAcc.h>
#include "utils/ScopedWin.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "EngineManager.h"
#include "DisplayModel.h"
#include "uia/PageProvider.h"
#include "uia/Constants.h"
#include "uia/DocumentProvider.h"
#include "uia/Provider.h"
#include "TextSelection.h"

SumatraUIAutomationPageProvider::SumatraUIAutomationPageProvider(int pageNum, HWND canvasHwnd, DisplayModel* dm,
                                                                 SumatraUIAutomationDocumentProvider* root)
    : refCount(1),
      pageNum(pageNum),
      canvasHwnd(canvasHwnd),
      dm(dm),
      root(root),
      sibling_prev(nullptr),
      sibling_next(nullptr),
      released(false) {
    // root->AddRef(); Don't add refs to our parent & owner.
}

SumatraUIAutomationPageProvider::~SumatraUIAutomationPageProvider() {}

int SumatraUIAutomationPageProvider::GetPageNum() const {
    return pageNum;
}

SumatraUIAutomationPageProvider* SumatraUIAutomationPageProvider::GetNextPage() {
    return sibling_next;
}

SumatraUIAutomationPageProvider* SumatraUIAutomationPageProvider::GetPreviousPage() {
    return sibling_prev;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {QITABENT(SumatraUIAutomationPageProvider, IRawElementProviderSimple),
                                QITABENT(SumatraUIAutomationPageProvider, IRawElementProviderFragment),
                                QITABENT(SumatraUIAutomationPageProvider, IValueProvider),
                                {0}};
    return QISearch(this, qit, riid, ppv);
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationPageProvider::AddRef(void) {
    return InterlockedIncrement(&refCount);
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationPageProvider::Release(void) {
    LONG res = InterlockedDecrement(&refCount);
    CrashIf(res < 0);
    if (0 == res)
        delete this;
    return res;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::Navigate(enum NavigateDirection direction,
                                                                    IRawElementProviderFragment** pRetVal) {
    if (pRetVal == nullptr)
        return E_POINTER;

    // disallow traverse if we are lingering
    if (released)
        return E_FAIL;

    *pRetVal = nullptr;
    if (direction == NavigateDirection_PreviousSibling) {
        *pRetVal = sibling_prev;
    } else if (direction == NavigateDirection_NextSibling) {
        *pRetVal = sibling_next;
    } else if (direction == NavigateDirection_FirstChild || direction == NavigateDirection_LastChild) {
        // do nothing
    } else if (direction == NavigateDirection_Parent) {
        *pRetVal = root;
    } else {
        return E_INVALIDARG;
    }

    if (*pRetVal)
        (*pRetVal)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::GetRuntimeId(SAFEARRAY** pRetVal) {
    if (pRetVal == nullptr)
        return E_POINTER;

    SAFEARRAY* psa = SafeArrayCreateVector(VT_I4, 0, 2);
    if (!psa)
        return E_OUTOFMEMORY;

    // RuntimeID magic, use hwnd to differentiate providers of different windows
    int rId[] = {(int)canvasHwnd, SUMATRA_UIA_PAGE_RUNTIME_ID(pageNum)};
    for (LONG i = 0; i < 2; i++) {
        HRESULT hr = SafeArrayPutElement(psa, &i, (void*)&(rId[i]));
        CrashIf(FAILED(hr));
    }

    *pRetVal = psa;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::GetEmbeddedFragmentRoots(SAFEARRAY** pRetVal) {
    if (pRetVal == nullptr)
        return E_POINTER;

    // no other roots => return nullptr
    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::SetFocus(void) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::get_BoundingRectangle(struct UiaRect* pRetVal) {
    if (pRetVal == nullptr)
        return E_POINTER;

    if (released)
        return E_FAIL;

    // some engines might not support GetPageInfo
    const PageInfo* page = dm->GetPageInfo(pageNum);
    if (!page)
        return E_FAIL;

    RECT canvasRect;
    GetWindowRect(canvasHwnd, &canvasRect);

    pRetVal->left = canvasRect.left + page->pageOnScreen.x;
    pRetVal->top = canvasRect.top + page->pageOnScreen.y;
    pRetVal->width = page->pageOnScreen.dx;
    pRetVal->height = page->pageOnScreen.dy;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::get_FragmentRoot(IRawElementProviderFragmentRoot** pRetVal) {
    if (released)
        return E_FAIL;

    // let our parent to handle this
    return root->get_FragmentRoot(pRetVal);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::GetPatternProvider(PATTERNID patternId, IUnknown** pRetVal) {
    if (pRetVal == nullptr)
        return E_POINTER;

    if (patternId == UIA_ValuePatternId) {
        *pRetVal = static_cast<IValueProvider*>(this);
        AddRef();
        return S_OK;
    }

    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::GetPropertyValue(PROPERTYID propertyId, VARIANT* pRetVal) {
    if (pRetVal == nullptr)
        return E_POINTER;

    if (propertyId == UIA_NamePropertyId) {
        pRetVal->vt = VT_BSTR;
        pRetVal->bstrVal = SysAllocString(AutoFreeWstr(str::Format(L"Page %d", pageNum)));
        return S_OK;
    } else if (propertyId == UIA_IsValuePatternAvailablePropertyId) {
        pRetVal->vt = VT_BOOL;
        pRetVal->boolVal = TRUE;
        return S_OK;
    }

    pRetVal->vt = VT_EMPTY;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
SumatraUIAutomationPageProvider::get_HostRawElementProvider(IRawElementProviderSimple** pRetVal) {
    if (pRetVal == nullptr)
        return E_POINTER;
    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::get_ProviderOptions(ProviderOptions* pRetVal) {
    if (pRetVal == nullptr)
        return E_POINTER;
    *pRetVal = ProviderOptions_ServerSideProvider;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::SetValue(LPCWSTR val) {
    UNUSED(val);
    return E_ACCESSDENIED;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::get_Value(BSTR* pRetVal) {
    if (pRetVal == nullptr)
        return E_POINTER;
    if (released)
        return E_FAIL;

    const WCHAR* pageContent = dm->textCache->GetData(pageNum);
    if (!pageContent) {
        *pRetVal = nullptr;
        return S_OK;
    }

    *pRetVal = SysAllocString(pageContent);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationPageProvider::get_IsReadOnly(BOOL* pRetVal) {
    if (pRetVal == nullptr)
        return E_POINTER;

    *pRetVal = TRUE;
    return S_OK;
}
