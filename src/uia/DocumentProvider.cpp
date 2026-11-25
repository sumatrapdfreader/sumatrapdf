/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "TextSelection.h"
#include "base/File.h"
#include "uia/Common.h"
#include "uia/PageProvider.h"
#include "uia/Provider.h"
#include "uia/TextRange.h"
#include "uia/DocumentProvider.h"

SumatraUIAutomationDocumentProvider::SumatraUIAutomationDocumentProvider(HWND canvasHwnd,
                                                                         SumatraUIAutomationProvider* root)
    : refCount(1), canvasHwnd(canvasHwnd), root(root), released(true), dm(nullptr) {
    // root->AddRef(); Don't add refs to our parent & owner.
}

SumatraUIAutomationDocumentProvider::~SumatraUIAutomationDocumentProvider() {
    this->FreeDocument();
}

void SumatraUIAutomationDocumentProvider::LoadDocument(DisplayModel* newDm) {
    this->FreeDocument();

    // no mutexes needed, this function is called from thread that created dm
    int nPages = std::max(0, newDm->PageCount());
    VecReserve(pages, nPages);
    for (int i = 0; i < nPages; i++) {
        VecAppend(pages, (SumatraUIAutomationPageProvider*)nullptr);
    }

    dm = newDm;
    released = false;
}

void SumatraUIAutomationDocumentProvider::FreeDocument() {
    // release our refs to the page elements
    if (released) {
        return;
    }

    released = true;
    dm = nullptr;
    // clients may keep this provider after the root (which doesn't ref-count us) is gone
    root = nullptr;

    for (SumatraUIAutomationPageProvider* page : pages) {
        if (!page) {
            continue;
        }
        page->released = true; // disallow DisplayModel access
        page->dm = nullptr;
        page->root = nullptr;
        page->Release();
    }
    VecReset(pages);
}

bool SumatraUIAutomationDocumentProvider::IsDocumentLoaded() const {
    return !released;
}

DisplayModel* SumatraUIAutomationDocumentProvider::GetDM() {
    // May be null after FreeDocument while UIA clients still hold COM refs.
    return dm;
}

// the window text ranges map their page coordinates to, for the screen
// rectangles UIA clients ask for
HWND SumatraUIAutomationDocumentProvider::GetCanvasHwnd() const {
    return canvasHwnd;
}

SumatraUIAutomationPageProvider* SumatraUIAutomationDocumentProvider::GetPage(int pageNum) {
    if (released || pageNum < 1 || pageNum > len(pages)) {
        return nullptr;
    }
    SumatraUIAutomationPageProvider* page = pages[pageNum - 1];
    if (!page) {
        page = new SumatraUIAutomationPageProvider(pageNum, canvasHwnd, dm, this);
        pages[pageNum - 1] = page;
    }
    return page;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {QITABENT(SumatraUIAutomationDocumentProvider, IRawElementProviderSimple),
                                QITABENT(SumatraUIAutomationDocumentProvider, IRawElementProviderFragment),
                                QITABENT(SumatraUIAutomationDocumentProvider, ITextProvider),
                                {nullptr}};
    return QISearch(this, qit, riid, ppv);
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::AddRef() {
    return AtomicIntInc(&refCount);
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::Release() {
    return ComRelease(&refCount, [this] { delete this; });
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::Navigate(enum NavigateDirection direction,
                                                                        IRawElementProviderFragment** pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }
    *pRetVal = nullptr;

    if (direction == NavigateDirection_NextSibling || direction == NavigateDirection_PreviousSibling) {
        return S_OK;
    } else if (direction == NavigateDirection_FirstChild || direction == NavigateDirection_LastChild) {
        // don't allow traversion to enter invalid nodes
        if (released || len(pages) == 0) {
            return S_OK;
        }

        int pageNum = (direction == NavigateDirection_FirstChild) ? 1 : len(pages);
        *pRetVal = GetPage(pageNum);
        if (*pRetVal) {
            (*pRetVal)->AddRef();
        }
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

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::GetRuntimeId(SAFEARRAY** pRetVal) {
    return UiaMakeRuntimeId(canvasHwnd, kSumatraUiaDocumentRuntimeId, pRetVal);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::GetEmbeddedFragmentRoots(SAFEARRAY** pRetVal) {
    return UiaReturnNull(pRetVal);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::SetFocus() {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::get_BoundingRectangle(struct UiaRect* pRetVal) {
    return UiaRootRect(root, pRetVal);
}

HRESULT STDMETHODCALLTYPE
SumatraUIAutomationDocumentProvider::get_FragmentRoot(IRawElementProviderFragmentRoot** pRetVal) {
    return UiaRootFragmentRoot(root, pRetVal);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::GetPatternProvider(PATTERNID patternId,
                                                                                  IUnknown** pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }

    if (patternId == UIA_TextPatternId) {
        *pRetVal = static_cast<ITextProvider*>(this);
        AddRef();
        return S_OK;
    }

    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::GetPropertyValue(PROPERTYID propertyId,
                                                                                VARIANT* pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }
    if (released) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }

    // properties that are simply TRUE for a document element
    bool isTrueBoolProp = propertyId == UIA_IsTextPatternAvailablePropertyId ||
                          propertyId == UIA_IsContentElementPropertyId || propertyId == UIA_IsControlElementPropertyId;
    if (propertyId == UIA_NamePropertyId) {
        // typically filename
        TempStr s = path::GetBaseNameTemp(dm->GetEngine()->FilePath());
        return UiaSetBstr(pRetVal, CWStrTemp(s));
    } else if (isTrueBoolProp) {
        // must be VARIANT_TRUE (-1), not TRUE (1): UI Automation compares
        // against VARIANT_TRUE, so a 1 here reads as false and kept the
        // document (the element that has the text) out of the control and
        // content views that screen readers navigate (#321)
        pRetVal->vt = VT_BOOL;
        pRetVal->boolVal = VARIANT_TRUE;
        return S_OK;
    } else if (propertyId == UIA_ControlTypePropertyId) {
        pRetVal->vt = VT_I4;
        pRetVal->lVal = UIA_DocumentControlTypeId;
        return S_OK;
    } else if (propertyId == UIA_NativeWindowHandlePropertyId) {
        pRetVal->vt = VT_I4;
        pRetVal->lVal = 0;
        return S_OK;
    } else if (propertyId == UIA_AutomationIdPropertyId) {
        return UiaSetBstr(pRetVal, L"Document");
    }

    pRetVal->vt = VT_EMPTY;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
SumatraUIAutomationDocumentProvider::get_HostRawElementProvider(IRawElementProviderSimple** pRetVal) {
    return UiaReturnNull(pRetVal);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::get_ProviderOptions(ProviderOptions* pRetVal) {
    return UiaProviderOptions(pRetVal);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::GetSelection(SAFEARRAY** pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }
    if (released) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }

    SAFEARRAY* psa = SafeArrayCreateVector(VT_UNKNOWN, 0, 1);
    if (!psa) {
        return E_OUTOFMEMORY;
    }

    SumatraUIAutomationTextRange* selection = new SumatraUIAutomationTextRange(this, dm->textSelection);

    LONG index = 0;
    HRESULT hr = SafeArrayPutElement(psa, &index, selection);
    // the array now owns the selection (or we drop it on failure)
    selection->Release();
    if (FAILED(hr)) {
        SafeArrayDestroy(psa);
        return hr;
    }

    *pRetVal = psa;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::GetVisibleRanges(SAFEARRAY** pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }
    if (released) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }

    // return all pages' ranges that are even partially visible
    Vec<SumatraUIAutomationTextRange*> rangeArray;
    int nPages = dm->PageCount();
    for (int pageNo = 1; pageNo <= nPages; pageNo++) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (pi && pi->isShown && pi->visibleRatio > 0.0f) {
            VecAppend(rangeArray, new SumatraUIAutomationTextRange(this, pageNo));
        }
    }

    SAFEARRAY* psa = SafeArrayCreateVector(VT_UNKNOWN, 0, (ULONG)len(rangeArray));
    if (!psa) {
        for (int i = 0; i < len(rangeArray); i++) {
            rangeArray[i]->Release();
        }
        return E_OUTOFMEMORY;
    }

    for (LONG i = 0; i < (LONG)len(rangeArray); i++) {
        HRESULT hr = SafeArrayPutElement(psa, &i, rangeArray[i]);
        rangeArray[i]->Release();
        if (FAILED(hr)) {
            for (LONG j = i + 1; j < (LONG)len(rangeArray); j++) {
                rangeArray[j]->Release();
            }
            SafeArrayDestroy(psa);
            return hr;
        }
    }

    *pRetVal = psa;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::RangeFromChild(IRawElementProviderSimple* childElement,
                                                                              ITextRangeProvider** pRetVal) {
    if (pRetVal == nullptr || childElement == nullptr) {
        return E_POINTER;
    }
    if (released) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }

    // a client can pass any element (e.g. the Document itself), so only
    // accept one of our live pages
    for (SumatraUIAutomationPageProvider* page : pages) {
        if (!page || static_cast<IRawElementProviderSimple*>(page) != childElement) {
            continue;
        }
        *pRetVal = new SumatraUIAutomationTextRange(this, page->pageNum);
        return S_OK;
    }
    return E_INVALIDARG;
}

// The empty range at the character nearest to a point on the screen. Screen
// readers use it to read what is under the mouse or the finger (Narrator's
// mouse mode, touch exploration): they call this and then expand the range to
// a word or a line. Returning E_NOTIMPL, as we used to, means none of that
// reads anything.
HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::RangeFromPoint(struct UiaPoint point,
                                                                              ITextRangeProvider** pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }
    if (released || !dm) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }

    Point pt = HwndScreenToClient(canvasHwnd, Point((int)point.x, (int)point.y));
    int pageNo = dm->GetPageNoByPoint(pt);
    auto* res = new SumatraUIAutomationTextRange(this);
    if (dm->ValidPageNo(pageNo)) {
        PointF ptD = dm->CvtFromScreen(pt, pageNo);
        int glyphIdx = dm->textSelection->FindClosestGlyphAt(pageNo, ptD.x, ptD.y);
        res->SetToDegenerateAt(pageNo, glyphIdx);
    } else {
        // not over a page (window margin, between pages): the nearest text is
        // the start of the page being looked at
        res->SetToDegenerateAt(dm->CurrentPageNo(), 0);
    }
    *pRetVal = res;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationDocumentProvider::get_DocumentRange(ITextRangeProvider** pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }
    if (released) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }

    SumatraUIAutomationTextRange* documentRange = new SumatraUIAutomationTextRange(this);
    documentRange->SetToDocumentRange();

    *pRetVal = documentRange;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
SumatraUIAutomationDocumentProvider::get_SupportedTextSelection(enum SupportedTextSelection* pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }
    *pRetVal = SupportedTextSelection_Single;
    return S_OK;
}
