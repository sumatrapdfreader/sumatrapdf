/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "uia/TextRange.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "EngineManager.h"
#include "DisplayModel.h"
#include "uia/DocumentProvider.h"
#include "uia/Constants.h"
#include "uia/PageProvider.h"
#include "uia/Provider.h"
#include "TextSelection.h"

SumatraUIAutomationTextRange::SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document)
    : refCount(1), document(document) {
    document->AddRef();

    SetToNullRange();
}

SumatraUIAutomationTextRange::SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document, int pageNum)
    : refCount(1), document(document) {
    document->AddRef();

    startPage = pageNum;
    startGlyph = 0;
    endPage = pageNum;
    endGlyph = GetPageGlyphCount(pageNum);
}

SumatraUIAutomationTextRange::SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document,
                                                           TextSelection* range)
    : refCount(1), document(document) {
    document->AddRef();

    range->GetGlyphRange(&startPage, &startGlyph, &endPage, &endGlyph);
    // null-range check
    if (startPage == -1 || endPage == -1) {
        SetToNullRange();
    }
}

SumatraUIAutomationTextRange::SumatraUIAutomationTextRange(const SumatraUIAutomationTextRange& b)
    : refCount(1), document(b.document) {
    document->AddRef();

    startPage = b.startPage;
    startGlyph = b.startGlyph;
    endPage = b.endPage;
    endGlyph = b.endGlyph;
}

SumatraUIAutomationTextRange::~SumatraUIAutomationTextRange() {
    document->Release();
}

bool SumatraUIAutomationTextRange::operator==(const SumatraUIAutomationTextRange& b) const {
    return document == b.document && startPage == b.startPage && endPage == b.endPage && startGlyph == b.startGlyph &&
           endGlyph == b.endGlyph;
}

void SumatraUIAutomationTextRange::SetToDocumentRange() {
    startPage = 1;
    startGlyph = 0;
    endPage = document->GetDM()->PageCount();
    endGlyph = GetPageGlyphCount(endPage);
}

void SumatraUIAutomationTextRange::SetToNullRange() {
    startPage = -1;
    startGlyph = 0;
    endPage = -1;
    endGlyph = 0;
}

bool SumatraUIAutomationTextRange::IsNullRange() const {
    return (startPage == -1 && endPage == -1);
}

bool SumatraUIAutomationTextRange::IsEmptyRange() const {
    return (startPage == endPage && startGlyph == endGlyph);
}

int SumatraUIAutomationTextRange::GetPageGlyphCount(int pageNum) {
    AssertCrash(document->IsDocumentLoaded());
    AssertCrash(pageNum > 0);

    int pageLen;
    document->GetDM()->textCache->GetData(pageNum, &pageLen);
    return pageLen;
}

int SumatraUIAutomationTextRange::GetPageCount() {
    AssertCrash(document->IsDocumentLoaded());

    return document->GetDM()->PageCount();
}

void SumatraUIAutomationTextRange::ValidateStartEndpoint() {
    // ensure correct ordering of endpoints
    if (startPage > endPage || (startPage == endPage && startGlyph > endGlyph)) {
        startPage = endPage;
        startGlyph = endGlyph;
    }
}

void SumatraUIAutomationTextRange::ValidateEndEndpoint() {
    // ensure correct ordering of endpoints
    if (startPage > endPage || (startPage == endPage && startGlyph > endGlyph)) {
        endPage = startPage;
        endGlyph = startGlyph;
    }
}

int SumatraUIAutomationTextRange::FindPreviousWordEndpoint(int pageno, int idx, bool dontReturnInitial) {
    // based on TextSelection::SelectWordAt
    int textLen;
    const WCHAR* pageText = document->GetDM()->textCache->GetData(pageno, &textLen);

    if (dontReturnInitial) {
        for (; idx > 0; idx--) {
            if (isWordChar(pageText[idx - 1]))
                break;
        }
    }

    for (; idx > 0; idx--) {
        if (!isWordChar(pageText[idx - 1]))
            break;
    }
    return idx;
}

int SumatraUIAutomationTextRange::FindNextWordEndpoint(int pageno, int idx, bool dontReturnInitial) {
    int textLen;
    const WCHAR* pageText = document->GetDM()->textCache->GetData(pageno, &textLen);

    if (dontReturnInitial) {
        for (; idx < textLen; idx++) {
            if (isWordChar(pageText[idx]))
                break;
        }
    }

    for (; idx < textLen; idx++) {
        if (!isWordChar(pageText[idx]))
            break;
    }
    return idx;
}

int SumatraUIAutomationTextRange::FindPreviousLineEndpoint(int pageno, int idx, bool dontReturnInitial) {
    int textLen;
    const WCHAR* pageText = document->GetDM()->textCache->GetData(pageno, &textLen);

    if (dontReturnInitial) {
        for (; idx > 0; idx--) {
            if (pageText[idx - 1] != L'\n')
                break;
        }
    }

    for (; idx > 0; idx--) {
        if (pageText[idx - 1] == L'\n')
            break;
    }
    return idx;
}

int SumatraUIAutomationTextRange::FindNextLineEndpoint(int pageno, int idx, bool dontReturnInitial) {
    int textLen;
    const WCHAR* pageText = document->GetDM()->textCache->GetData(pageno, &textLen);

    if (dontReturnInitial) {
        for (; idx < textLen; idx++) {
            if (pageText[idx] != L'\n')
                break;
        }
    }

    for (; idx < textLen; idx++) {
        if (pageText[idx] == L'\n')
            break;
    }
    return idx;
}

// IUnknown
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {QITABENT(SumatraUIAutomationTextRange, ITextRangeProvider), {0}};
    return QISearch(this, qit, riid, ppv);
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationTextRange::AddRef(void) {
    return InterlockedIncrement(&refCount);
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationTextRange::Release(void) {
    LONG res = InterlockedDecrement(&refCount);
    CrashIf(res < 0);
    if (0 == res)
        delete this;
    return res;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::Clone(ITextRangeProvider** clonedRange) {
    if (clonedRange == nullptr)
        return E_POINTER;
    *clonedRange = new SumatraUIAutomationTextRange(*this);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::Compare(ITextRangeProvider* range, BOOL* areSame) {
    if (areSame == nullptr)
        return E_POINTER;
    if (range == nullptr)
        return E_POINTER;
    // TODO: is range guaranteed to be a SumatraUIAutomationTextRange?
    if (*((SumatraUIAutomationTextRange*)range) == *this)
        *areSame = TRUE;
    else
        *areSame = FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::CompareEndpoints(enum TextPatternRangeEndpoint srcEndPoint,
                                                                         ITextRangeProvider* range,
                                                                         enum TextPatternRangeEndpoint targetEndPoint,
                                                                         int* compValue) {
    if (range == nullptr)
        return E_POINTER;
    if (compValue == nullptr)
        return E_POINTER;

    int comp_a_page, comp_a_idx;
    if (srcEndPoint == TextPatternRangeEndpoint_Start) {
        comp_a_page = this->startPage;
        comp_a_idx = this->startGlyph;
    } else if (srcEndPoint == TextPatternRangeEndpoint_End) {
        comp_a_page = this->endPage;
        comp_a_idx = this->endGlyph;
    } else {
        return E_INVALIDARG;
    }

    // TODO: is range guaranteed to be a SumatraUIAutomationTextRange?
    SumatraUIAutomationTextRange* target = (SumatraUIAutomationTextRange*)range;

    int comp_b_page, comp_b_idx;
    if (targetEndPoint == TextPatternRangeEndpoint_Start) {
        comp_b_page = target->startPage;
        comp_b_idx = target->startGlyph;
    } else if (targetEndPoint == TextPatternRangeEndpoint_End) {
        comp_b_page = target->endPage;
        comp_b_idx = target->endGlyph;
    } else {
        return E_INVALIDARG;
    }

    if (comp_a_page < comp_b_page)
        *compValue = -1;
    else if (comp_a_page > comp_b_page)
        *compValue = 1;
    else
        *compValue = comp_a_idx - comp_b_idx;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::ExpandToEnclosingUnit(enum TextUnit textUnit) {
    // if document is closed, don't do anything
    if (!document->IsDocumentLoaded())
        return E_FAIL;

    // if not set, don't do anything
    if (IsNullRange())
        return S_OK;

    if (textUnit == TextUnit_Character) {
        // done
        return S_OK;
    } else if (textUnit == TextUnit_Format) {
        // what is a "format" anyway?
        return S_OK;
    } else if (textUnit == TextUnit_Word) {
        // select current word at start endpoint
        int word_beg = FindPreviousWordEndpoint(startPage, startGlyph);
        int word_end = FindNextWordEndpoint(startPage, startGlyph);

        endPage = startPage;

        startGlyph = word_beg;
        endGlyph = word_end;

        return S_OK;
    } else if (textUnit == TextUnit_Line || textUnit == TextUnit_Paragraph) {
        // select current line or current paragraph. In general case these cannot be differentiated? Right?
        int word_beg = FindPreviousLineEndpoint(startPage, startGlyph);
        int word_end = FindNextLineEndpoint(startPage, startGlyph);

        endPage = startPage;

        startGlyph = word_beg;
        endGlyph = word_end;

        return S_OK;
    } else if (textUnit == TextUnit_Page) {
        // select current page

        // start from the beginning of start page
        startGlyph = 0;

        // to the end of the end page
        endGlyph = GetPageGlyphCount(endPage);

        return S_OK;
    } else if (textUnit == TextUnit_Document) {
        SetToDocumentRange();
        return S_OK;
    } else {
        return E_INVALIDARG;
    }
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::FindAttribute(TEXTATTRIBUTEID attr, VARIANT val, BOOL backward,
                                                                      ITextRangeProvider** found) {
    UNUSED(attr);
    UNUSED(val);
    UNUSED(backward);
    if (found == nullptr)
        return E_POINTER;
    if (!document->IsDocumentLoaded())
        return E_FAIL;

    // raw text doesn't have attributes so just don't find anything
    *found = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::FindText(BSTR text, BOOL backward, BOOL ignoreCase,
                                                                 ITextRangeProvider** found) {
    UNUSED(text);
    UNUSED(backward);
    UNUSED(ignoreCase);
    if (found == nullptr)
        return E_POINTER;
    if (!document->IsDocumentLoaded())
        return E_FAIL;

    // TODO: Support text searching
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::GetAttributeValue(TEXTATTRIBUTEID attr, VARIANT* value) {
    UNUSED(attr);
    if (value == nullptr)
        return E_POINTER;
    if (!document->IsDocumentLoaded())
        return E_FAIL;

    // text doesn't have attributes, we don't support those
    IUnknown* not_supported = nullptr;
    HRESULT hr = uia::GetReservedNotSupportedValue(&not_supported);
    if (FAILED(hr))
        return hr;

    value->vt = VT_UNKNOWN;
    value->punkVal = not_supported;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::GetBoundingRectangles(SAFEARRAY** boundingRects) {
    if (boundingRects == nullptr)
        return E_POINTER;
    if (!document->IsDocumentLoaded())
        return E_FAIL;

    if (IsNullRange()) {
        SAFEARRAY* sarray = SafeArrayCreateVector(VT_R8, 0, 0);
        if (!sarray)
            return E_OUTOFMEMORY;

        *boundingRects = sarray;
        return S_OK;
    }

    // TODO: support GetBoundingRectangles
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
SumatraUIAutomationTextRange::GetEnclosingElement(IRawElementProviderSimple** enclosingElement) {
    if (enclosingElement == nullptr)
        return E_POINTER;
    if (!document->IsDocumentLoaded())
        return E_FAIL;

    *enclosingElement = document;
    (*enclosingElement)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::GetText(int maxLength, BSTR* text) {
    if (text == nullptr)
        return E_POINTER;
    if (!document->IsDocumentLoaded())
        return E_FAIL;

    if (IsNullRange() || IsEmptyRange()) {
        *text = SysAllocString(L""); // 0-sized not-null string
        return S_OK;
    }

    TextSelection selection(document->GetDM()->GetEngine(), document->GetDM()->textCache);
    selection.StartAt(startPage, startGlyph);
    selection.SelectUpTo(endPage, endGlyph);

    AutoFreeWstr selected_text(selection.ExtractText(L"\r\n"));
    size_t selected_text_length = str::Len(selected_text);

    // -1 and [0, inf) are allowed
    if (maxLength > -2) {
        if (maxLength != -1 && selected_text_length > (size_t)maxLength)
            selected_text[maxLength] = '\0'; // truncate

        *text = SysAllocString(selected_text);
        if (*text)
            return S_OK;
        else
            return E_OUTOFMEMORY;
    } else {
        return E_INVALIDARG;
    }
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::Move(enum TextUnit unit, int count, int* moved) {
    if (moved == nullptr)
        return E_POINTER;

    // if document is closed, don't do anything
    if (!document->IsDocumentLoaded())
        return E_FAIL;

    // Just move the endpoints using other methods
    *moved = 0;
    this->ExpandToEnclosingUnit(unit);

    if (count > 0) {
        for (int i = 0; i < count; ++i) {
            int sub_moved;
            this->MoveEndpointByUnit(TextPatternRangeEndpoint_End, unit, 1, &sub_moved);

            // Move end first, other will succeed if this succeeds
            if (sub_moved == 0)
                break;

            this->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, unit, 1, &sub_moved);
            ++*moved;
        }
    } else if (count < 0) {
        for (int i = 0; i < -count; ++i) {
            int sub_moved;
            this->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, unit, -1, &sub_moved);

            // Move start first, other will succeed if this succeeds
            if (sub_moved == 0)
                break;

            this->MoveEndpointByUnit(TextPatternRangeEndpoint_End, unit, -1, &sub_moved);
            ++*moved;
        }
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::MoveEndpointByUnit(TextPatternRangeEndpoint endpoint,
                                                                           TextUnit unit, int count, int* moved) {
    if (moved == nullptr)
        return E_POINTER;

    // if document is closed, don't do anything
    if (!document->IsDocumentLoaded())
        return E_FAIL;

    // if not set, don't do anything
    if (IsNullRange())
        return S_OK;

    // what to move
    int *target_page, *target_glyph;
    if (endpoint == TextPatternRangeEndpoint_Start) {
        target_page = &startPage;
        target_glyph = &startGlyph;
    } else if (endpoint == TextPatternRangeEndpoint_End) {
        target_page = &endPage;
        target_glyph = &endGlyph;
    } else {
        return E_INVALIDARG;
    }

    class EndPointMover {
      protected:
        SumatraUIAutomationTextRange* target;
        int* target_page;
        int* target_glyph;

      public:
        // return false when cannot be moved
        virtual bool NextEndpoint() const {
            // HACK: Declaring these as pure virtual causes "unreferenced local variable" warnings ==> define a dummy
            // body to get rid of warnings
            CrashIf(true);
            return false;
        }
        virtual bool PrevEndpoint() const {
            CrashIf(true);
            return false;
        }

        // return false when not appliable
        bool NextPage() const {
            int max_glyph = target->GetPageGlyphCount(*target_page);

            if (*target_glyph == max_glyph) {
                if (*target_page == target->GetPageCount()) {
                    // last page
                    return false;
                }

                // go to next page
                (*target_page)++;
                (*target_glyph) = 0;
            }
            return true;
        }
        bool PreviousPage() const {
            if (*target_glyph == 0) {
                if (*target_page == 1) {
                    // first page
                    return false;
                }

                // go to next page
                (*target_page)--;
                (*target_glyph) = target->GetPageGlyphCount(*target_page);
            }
            return true;
        }

        // do the moving
        int Move(int count, SumatraUIAutomationTextRange* target, int* target_page, int* target_glyph) {
            this->target = target;
            this->target_page = target_page;
            this->target_glyph = target_glyph;

            int retVal = 0;
            if (count > 0) {
                for (int i = 0; i < count && (NextPage() || NextEndpoint()); ++i)
                    ++retVal;
            } else {
                for (int i = 0; i < -count && (PreviousPage() || PrevEndpoint()); ++i)
                    ++retVal;
            }

            return retVal;
        }
    };
    class CharEndPointMover : public EndPointMover {
        bool NextEndpoint() const {
            (*target_glyph)++;
            return true;
        }
        bool PrevEndpoint() const {
            (*target_glyph)--;
            return true;
        }
    };
    class WordEndPointMover : public EndPointMover {
        bool NextEndpoint() const {
            (*target_glyph) = target->FindNextWordEndpoint(*target_page, *target_glyph, true);
            return true;
        }
        bool PrevEndpoint() const {
            (*target_glyph) = target->FindPreviousWordEndpoint(*target_page, *target_glyph, true);
            (*target_glyph)--;
            return true;
        }
    };
    class LineEndPointMover : public EndPointMover {
        bool NextEndpoint() const {
            (*target_glyph) = target->FindNextLineEndpoint(*target_page, *target_glyph, true);
            return true;
        }
        bool PrevEndpoint() const {
            (*target_glyph) = target->FindPreviousLineEndpoint(*target_page, *target_glyph, true);
            (*target_glyph)--;
            return true;
        }
    };

    // how much to move
    if (unit == TextUnit_Character) {
        CharEndPointMover mover;
        *moved = mover.Move(count, this, target_page, target_glyph);
    } else if (unit == TextUnit_Word || unit == TextUnit_Format) {
        WordEndPointMover mover;
        *moved = mover.Move(count, this, target_page, target_glyph);
    } else if (unit == TextUnit_Line || unit == TextUnit_Paragraph) {
        LineEndPointMover mover;
        *moved = mover.Move(count, this, target_page, target_glyph);
    } else if (unit == TextUnit_Page) {
        *moved = 0;
        *target_glyph = 0;

        if (count > 0) {
            // GetPageCount()+1 => allow overflow momentarily
            for (int i = 0; i < count && *target_page != GetPageCount() + 1; ++i) {
                (*target_page)++;
                (*moved)++;
            }

            // fix overflow, allow seeking to the end this way
            if (*target_page == GetPageCount() + 1) {
                *target_page = GetPageCount();
                *target_glyph = GetPageGlyphCount(*target_page);
            }
        } else {
            for (int i = 0; i < -count && *target_page != 1; ++i) {
                (*target_page)--;
                (*moved)++;
            }
        }
    } else if (unit == TextUnit_Document) {
        if (count > 0) {
            int end_page = GetPageCount();
            int end_glyph = GetPageGlyphCount(*target_page);

            if (*target_page != end_page || *target_glyph != end_glyph) {
                *target_page = end_page;
                *target_glyph = end_glyph;
                *moved = 1;
            } else {
                *moved = 0;
            }
        } else {
            const int beg_page = 0;
            const int beg_glyph = 0;

            if (*target_page != beg_page || *target_glyph != beg_glyph) {
                *target_page = beg_page;
                *target_glyph = beg_glyph;
                *moved = 1;
            } else {
                *moved = 0;
            }
        }
    } else {
        return E_INVALIDARG;
    }

    // keep range valid
    if (endpoint == TextPatternRangeEndpoint_Start) {
        // drag end with start
        ValidateEndEndpoint();
    } else if (endpoint == TextPatternRangeEndpoint_End) {
        // drag start with end
        ValidateStartEndpoint();
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::MoveEndpointByRange(TextPatternRangeEndpoint srcEndPoint,
                                                                            ITextRangeProvider* range,
                                                                            TextPatternRangeEndpoint targetEndPoint) {
    if (range == nullptr)
        return E_POINTER;

    // TODO: is range guaranteed to be a SumatraUIAutomationTextRange?
    SumatraUIAutomationTextRange* target = (SumatraUIAutomationTextRange*)range;

    // extract target location
    int target_page, target_idx;
    if (targetEndPoint == TextPatternRangeEndpoint_Start) {
        target_page = target->startPage;
        target_idx = target->startGlyph;
    } else if (targetEndPoint == TextPatternRangeEndpoint_End) {
        target_page = target->endPage;
        target_idx = target->endGlyph;
    } else {
        return E_INVALIDARG;
    }

    // apply
    if (srcEndPoint == TextPatternRangeEndpoint_Start) {
        startPage = target_page;
        startGlyph = target_idx;

        // drag end with start
        ValidateEndEndpoint();
    } else if (srcEndPoint == TextPatternRangeEndpoint_End) {
        endPage = target_page;
        endGlyph = target_idx;

        // drag start with end
        ValidateStartEndpoint();
    } else {
        return E_INVALIDARG;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::Select(void) {
    if (!document->IsDocumentLoaded())
        return E_FAIL;

    if (IsNullRange() || IsEmptyRange()) {
        document->GetDM()->textSelection->Reset();
    } else {
        document->GetDM()->textSelection->Reset();
        document->GetDM()->textSelection->StartAt(startPage, startGlyph);
        document->GetDM()->textSelection->SelectUpTo(endPage, endGlyph);
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::AddToSelection(void) {
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::RemoveFromSelection(void) {
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::ScrollIntoView(BOOL alignToTop) {
    if (!document->IsDocumentLoaded())
        return E_FAIL;

    // extract target location
    int target_page, target_idx;
    if (IsNullRange()) {
        target_page = 0;
        target_idx = 0;
    } else if (alignToTop) {
        target_page = startPage;
        target_idx = startGlyph;
    } else {
        target_page = endPage;
        target_idx = endGlyph;
    }

    // TODO: Scroll to target_page, target_idx
    // document->GetDM()->ScrollYTo()
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::GetChildren(SAFEARRAY** children) {
    if (children == nullptr)
        return E_POINTER;
    if (!document->IsDocumentLoaded())
        return E_FAIL;

    // return all children in range
    if (IsNullRange()) {
        SAFEARRAY* psa = SafeArrayCreateVector(VT_UNKNOWN, 0, 0);
        if (!psa)
            return E_OUTOFMEMORY;

        *children = psa;
        return S_OK;
    }

    SAFEARRAY* psa = SafeArrayCreateVector(VT_UNKNOWN, 0, endPage - startPage + 1);
    if (!psa)
        return E_OUTOFMEMORY;

    SumatraUIAutomationPageProvider* it = document->GetFirstPage();
    while (it) {
        if (it->GetPageNum() >= startPage || it->GetPageNum() <= endPage) {
            LONG index = it->GetPageNum() - startPage;

            HRESULT hr = SafeArrayPutElement(psa, &index, it);
            CrashIf(FAILED(hr));
            it->AddRef();
        }

        it = it->GetNextPage();
    }

    *children = psa;
    return S_OK;
}
