/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/WinDynCalls.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "uia/TextRange.h"
#include "DisplayModel.h"
#include "uia/DocumentProvider.h"
#include "uia/PageProvider.h"
#include "TextSelection.h"

// creates a copy of give range
// creates range containing the given TextSelection range
// creates range containing the given page
// creates empty range
SumatraUIAutomationTextRange::SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document)
    : refCount(1), document(document) {
    document->AddRef();

    SetToNullRange();
}

// creates a copy of give range
// creates range containing the given TextSelection range
// creates range containing the given page
// creates empty range
SumatraUIAutomationTextRange::SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document, int pageNum)
    : refCount(1), document(document) {
    document->AddRef();

    startPage = pageNum;
    startGlyph = 0;
    endPage = pageNum;
    endGlyph = GetPageGlyphCount(pageNum);
}

// creates a copy of give range
// creates range containing the given TextSelection range
// creates range containing the given page
// creates empty range
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

// creates a copy of give range
// creates range containing the given TextSelection range
// creates range containing the given page
// creates empty range
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

// an empty range sitting at one spot in the text, which is what a client gets
// from RangeFromPoint() before it expands the range to a word / line
void SumatraUIAutomationTextRange::SetToDegenerateAt(int pageNo, int glyphIdx) {
    startPage = pageNo;
    endPage = pageNo;
    startGlyph = limitValue(glyphIdx, 0, GetPageGlyphCount(pageNo));
    endGlyph = startGlyph;
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

// UIA may call into a text range after the document was unloaded (tab switch /
// close). Prefer a soft failure over ReportIf/AV.
static HRESULT EnsureDocumentLoaded(SumatraUIAutomationDocumentProvider* document) {
    if (!document || !document->IsDocumentLoaded() || !document->GetDM()) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    return S_OK;
}

int SumatraUIAutomationTextRange::GetPageGlyphCount(int pageNum) {
    if (FAILED(EnsureDocumentLoaded(document)) || pageNum <= 0) {
        return 0;
    }

    int pageLen = 0;
    document->GetDM()->GetEngine()->GetTextForPage(pageNum, &pageLen);
    return pageLen;
}

int SumatraUIAutomationTextRange::GetPageCount() {
    if (FAILED(EnsureDocumentLoaded(document))) {
        return 0;
    }
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
    auto* engine = document->GetDM()->GetEngine();
    Str pageText = engine->GetTextForPage(pageno, &textLen);

    int byteIdx = Utf8CodepointToByteIndex(pageText, idx);
    if (dontReturnInitial) {
        while (idx > 0) {
            int prevByte = byteIdx;
            int c = Utf8CodepointPrev(pageText, prevByte);
            if (isWordChar(c)) {
                break;
            }
            byteIdx = prevByte;
            idx--;
        }
    }

    while (idx > 0) {
        int prevByte = byteIdx;
        int c = Utf8CodepointPrev(pageText, prevByte);
        if (!isWordChar(c)) {
            break;
        }
        byteIdx = prevByte;
        idx--;
    }
    return idx;
}

int SumatraUIAutomationTextRange::FindNextWordEndpoint(int pageno, int idx, bool dontReturnInitial) {
    int textLen;
    auto* engine = document->GetDM()->GetEngine();
    Str pageText = engine->GetTextForPage(pageno, &textLen);

    int byteIdx = Utf8CodepointToByteIndex(pageText, idx);
    if (dontReturnInitial) {
        while (idx < textLen) {
            int nextByte = byteIdx;
            int c = Utf8CodepointNext(pageText, nextByte);
            if (isWordChar(c)) {
                break;
            }
            byteIdx = nextByte;
            idx++;
        }
    }

    while (idx < textLen) {
        int nextByte = byteIdx;
        int c = Utf8CodepointNext(pageText, nextByte);
        if (!isWordChar(c)) {
            break;
        }
        byteIdx = nextByte;
        idx++;
    }
    return idx;
}

int SumatraUIAutomationTextRange::FindPreviousLineEndpoint(int pageno, int idx, bool dontReturnInitial) {
    int textLen;
    auto* engine = document->GetDM()->GetEngine();
    Str pageText = engine->GetTextForPage(pageno, &textLen);

    int byteIdx = Utf8CodepointToByteIndex(pageText, idx);
    if (dontReturnInitial) {
        while (idx > 0) {
            int prevByte = byteIdx;
            int c = Utf8CodepointPrev(pageText, prevByte);
            if (c != '\n') {
                break;
            }
            byteIdx = prevByte;
            idx--;
        }
    }

    while (idx > 0) {
        int prevByte = byteIdx;
        int c = Utf8CodepointPrev(pageText, prevByte);
        if (c == '\n') {
            break;
        }
        byteIdx = prevByte;
        idx--;
    }
    return idx;
}

int SumatraUIAutomationTextRange::FindNextLineEndpoint(int pageno, int idx, bool dontReturnInitial) {
    int textLen;
    auto* engine = document->GetDM()->GetEngine();
    Str pageText = engine->GetTextForPage(pageno, &textLen);

    int byteIdx = Utf8CodepointToByteIndex(pageText, idx);
    if (dontReturnInitial) {
        while (idx < textLen) {
            int nextByte = byteIdx;
            int c = Utf8CodepointNext(pageText, nextByte);
            if (c != '\n') {
                break;
            }
            byteIdx = nextByte;
            idx++;
        }
    }

    while (idx < textLen) {
        int nextByte = byteIdx;
        int c = Utf8CodepointNext(pageText, nextByte);
        if (c == '\n') {
            break;
        }
        byteIdx = nextByte;
        idx++;
    }
    return idx;
}

// IUnknown
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {QITABENT(SumatraUIAutomationTextRange, ITextRangeProvider), {nullptr}};
    return QISearch(this, qit, riid, ppv);
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationTextRange::AddRef() {
    return AtomicIntInc(&refCount);
}

ULONG STDMETHODCALLTYPE SumatraUIAutomationTextRange::Release() {
    LONG res = InterlockedDecrement(&refCount);
    ReportIf(res < 0);
    if (0 == res) {
        delete this;
    }
    return res;
}

// ITextRangeProvider
HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::Clone(ITextRangeProvider** clonedRange) {
    if (clonedRange == nullptr) {
        return E_POINTER;
    }
    HRESULT hr = EnsureDocumentLoaded(document);
    if (FAILED(hr)) {
        return hr;
    }
    *clonedRange = new SumatraUIAutomationTextRange(*this);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::Compare(ITextRangeProvider* range, BOOL* areSame) {
    if (areSame == nullptr) {
        return E_POINTER;
    }
    if (range == nullptr) {
        return E_POINTER;
    }
    // TODO: is range guaranteed to be a SumatraUIAutomationTextRange?
    if (*((SumatraUIAutomationTextRange*)range) == *this) {
        *areSame = TRUE;
    } else {
        *areSame = FALSE;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::CompareEndpoints(enum TextPatternRangeEndpoint srcEndPoint,
                                                                         ITextRangeProvider* range,
                                                                         enum TextPatternRangeEndpoint targetEndPoint,
                                                                         int* compValue) {
    if (range == nullptr) {
        return E_POINTER;
    }
    if (compValue == nullptr) {
        return E_POINTER;
    }

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

    if (comp_a_page < comp_b_page) {
        *compValue = -1;
    } else if (comp_a_page > comp_b_page) {
        *compValue = 1;
    } else {
        *compValue = comp_a_idx - comp_b_idx;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::ExpandToEnclosingUnit(enum TextUnit textUnit) {
    HRESULT hrDoc = EnsureDocumentLoaded(document);
    if (FAILED(hrDoc)) {
        return hrDoc;
    }

    // if not set, don't do anything
    if (IsNullRange()) {
        return S_OK;
    }

    // Format: what is a "format" anyway?
    if (textUnit == TextUnit_Character || textUnit == TextUnit_Format) {
        // a degenerate range must come back holding one character, or a screen
        // reader walking the document character by character reads nothing
        if (textUnit == TextUnit_Character && IsEmptyRange()) {
            if (endGlyph < GetPageGlyphCount(endPage)) {
                endGlyph++;
            } else if (endPage < GetPageCount()) {
                endPage++;
                endGlyph = std::min(1, GetPageGlyphCount(endPage));
            }
        }
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

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::FindAttribute(__unused TEXTATTRIBUTEID attr,
                                                                      __unused VARIANT val, __unused BOOL backward,
                                                                      ITextRangeProvider** found) {
    if (found == nullptr) {
        return E_POINTER;
    }
    HRESULT hrDoc = EnsureDocumentLoaded(document);
    if (FAILED(hrDoc)) {
        return hrDoc;
    }

    // raw text doesn't have attributes so just don't find anything
    *found = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::FindText(__unused BSTR text, __unused BOOL backward,
                                                                 __unused BOOL ignoreCase, ITextRangeProvider** found) {
    if (found == nullptr) {
        return E_POINTER;
    }
    HRESULT hrDoc = EnsureDocumentLoaded(document);
    if (FAILED(hrDoc)) {
        return hrDoc;
    }

    // TODO: Support text searching
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::GetAttributeValue(__unused TEXTATTRIBUTEID attr,
                                                                          VARIANT* value) {
    if (value == nullptr) {
        return E_POINTER;
    }
    HRESULT hrDoc = EnsureDocumentLoaded(document);
    if (FAILED(hrDoc)) {
        return hrDoc;
    }

    // text doesn't have attributes, we don't support those
    IUnknown* not_supported = nullptr;
    HRESULT hr = UiaGetReservedNotSupportedValue(&not_supported);
    if (FAILED(hr)) {
        return hr;
    }

    value->vt = VT_UNKNOWN;
    value->punkVal = not_supported;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::GetBoundingRectangles(SAFEARRAY** boundingRects) {
    if (boundingRects == nullptr) {
        return E_POINTER;
    }
    HRESULT hrDoc = EnsureDocumentLoaded(document);
    if (FAILED(hrDoc)) {
        return hrDoc;
    }

    // one rectangle per line of the range, in screen coordinates. This is how a
    // screen reader knows where on screen the text it reads is: Narrator draws
    // its highlight around it and touch/braille clients locate text with it.
    // Text on pages that aren't laid out on screen contributes nothing.
    Vec<double> coords;
    if (!IsNullRange() && !IsEmptyRange()) {
        DisplayModel* dm = document->GetDM();
        HWND hwnd = document->GetCanvasHwnd();
        TextSelection selection(dm->GetEngine());
        selection.StartAt(startPage, startGlyph);
        selection.SelectUpTo(endPage, endGlyph);
        TextSel* sel = &selection.result;
        for (int i = 0; i < sel->len; i++) {
            int pageNo = sel->pages[i];
            PageInfo* pi = dm->GetPageInfo(pageNo);
            if (!pi || !pi->isShown || pi->visibleRatio <= 0.f) {
                continue;
            }
            Rect rc = dm->CvtToScreen(pageNo, ToRectF(sel->rects[i]));
            if (rc.IsEmpty()) {
                continue;
            }
            Point tl = HwndClientToScreen(hwnd, rc.TL());
            coords.Append((double)tl.x);
            coords.Append((double)tl.y);
            coords.Append((double)rc.dx);
            coords.Append((double)rc.dy);
        }
    }

    SAFEARRAY* sarray = SafeArrayCreateVector(VT_R8, 0, (ULONG)len(coords));
    if (!sarray) {
        return E_OUTOFMEMORY;
    }
    for (LONG i = 0; i < (LONG)len(coords); i++) {
        HRESULT hr = SafeArrayPutElement(sarray, &i, &coords[i]);
        if (FAILED(hr)) {
            SafeArrayDestroy(sarray);
            return hr;
        }
    }
    *boundingRects = sarray;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
SumatraUIAutomationTextRange::GetEnclosingElement(IRawElementProviderSimple** enclosingElement) {
    if (enclosingElement == nullptr) {
        return E_POINTER;
    }
    HRESULT hrDoc = EnsureDocumentLoaded(document);
    if (FAILED(hrDoc)) {
        return hrDoc;
    }

    *enclosingElement = document;
    (*enclosingElement)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::GetText(int maxLength, BSTR* text) {
    if (text == nullptr) {
        return E_POINTER;
    }
    HRESULT hrDoc = EnsureDocumentLoaded(document);
    if (FAILED(hrDoc)) {
        return hrDoc;
    }

    if (IsNullRange() || IsEmptyRange()) {
        *text = SysAllocString(L""); // 0-sized not-null string
        return S_OK;
    }

    TextSelection selection(document->GetDM()->GetEngine());
    selection.StartAt(startPage, startGlyph);
    selection.SelectUpTo(endPage, endGlyph);

    Str selected_text = selection.ExtractText("\r\n");

    // -1 and [0, inf) are allowed
    if (maxLength < -1) {
        str::Free(selected_text);
        return E_INVALIDARG;
    }
    if (maxLength != -1 && Utf8CodepointCount(selected_text) > maxLength) {
        int byteIdx = Utf8CodepointToByteIndex(selected_text, maxLength);
        selected_text.s[byteIdx] = '\0';
        selected_text.len = byteIdx;
    }
    TempWStr selectedTextW = ToWStrTemp(selected_text);
    *text = SysAllocString(selectedTextW.s);
    str::Free(selected_text);
    return *text ? S_OK : E_OUTOFMEMORY;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::Move(enum TextUnit unit, int count, int* moved) {
    if (moved == nullptr) {
        return E_POINTER;
    }

    // if document is closed, don't do anything
    HRESULT hrDoc = EnsureDocumentLoaded(document);
    if (FAILED(hrDoc)) {
        return hrDoc;
    }

    // Just move the endpoints using other methods
    *moved = 0;
    // A degenerate range is grown to a whole unit first. An already expanded
    // one must not be re-expanded: after a move its start endpoint sits at the
    // end of the previous unit, and expanding from there snaps the range back
    // onto that unit, so repeated Move() calls -- how a screen reader walks a
    // document -- never got past the first line / word (#321)
    if (IsEmptyRange()) {
        this->ExpandToEnclosingUnit(unit);
    }

    if (count > 0) {
        for (int i = 0; i < count; ++i) {
            int sub_moved = 0;
            this->MoveEndpointByUnit(TextPatternRangeEndpoint_End, unit, 1, &sub_moved);

            // Move end first, other will succeed if this succeeds
            if (sub_moved == 0) {
                break;
            }

            this->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, unit, 1, &sub_moved);
            ++*moved;
        }
    } else if (count < 0) {
        for (int i = 0; i < -count; ++i) {
            int sub_moved = 0;
            this->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, unit, -1, &sub_moved);

            // Move start first, other will succeed if this succeeds
            if (sub_moved == 0) {
                break;
            }

            this->MoveEndpointByUnit(TextPatternRangeEndpoint_End, unit, -1, &sub_moved);
            ++*moved;
        }
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::MoveEndpointByUnit(TextPatternRangeEndpoint endpoint,
                                                                           TextUnit unit, int count, int* moved) {
    if (moved == nullptr) {
        return E_POINTER;
    }

    // if document is closed, don't do anything
    HRESULT hrDoc = EnsureDocumentLoaded(document);
    if (FAILED(hrDoc)) {
        return hrDoc;
    }

    // if not set, don't do anything
    if (IsNullRange()) {
        return S_OK;
    }

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
            ReportIf(true);
            return false;
        }
        virtual bool PrevEndpoint() const {
            ReportIf(true);
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

        // do the moving. NextPage() / PreviousPage() only hop to the adjacent
        // page when the endpoint sits at a page boundary; they return false at
        // the very start / end of the document. The endpoint itself is then
        // moved by NextEndpoint() / PrevEndpoint() -- both must run for each
        // step (`||` between them short-circuited the actual move away, so
        // Move() reported success while the range never advanced, #321)
        int Move(int count, SumatraUIAutomationTextRange* target, int* target_page, int* target_glyph) {
            this->target = target;
            this->target_page = target_page;
            this->target_glyph = target_glyph;

            int retVal = 0;
            for (int i = 0; i < abs(count); ++i) {
                bool ok = count > 0 ? (NextPage() && NextEndpoint()) : (PreviousPage() && PrevEndpoint());
                if (!ok) {
                    break;
                }
                // never leave an endpoint outside its page
                *target_glyph = limitValue(*target_glyph, 0, target->GetPageGlyphCount(*target_page));
                ++retVal;
            }

            return retVal;
        }
    };
    class CharEndPointMover : public EndPointMover {
        bool NextEndpoint() const override {
            (*target_glyph)++;
            return true;
        }
        bool PrevEndpoint() const override {
            (*target_glyph)--;
            return true;
        }
    };
    class WordEndPointMover : public EndPointMover {
        bool NextEndpoint() const override {
            (*target_glyph) = target->FindNextWordEndpoint(*target_page, *target_glyph, true);
            return true;
        }
        bool PrevEndpoint() const override {
            (*target_glyph) = target->FindPreviousWordEndpoint(*target_page, *target_glyph, true);
            (*target_glyph)--;
            return true;
        }
    };
    class LineEndPointMover : public EndPointMover {
        bool NextEndpoint() const override {
            (*target_glyph) = target->FindNextLineEndpoint(*target_page, *target_glyph, true);
            return true;
        }
        bool PrevEndpoint() const override {
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
        // a page further is the start of that page for the start endpoint and
        // its end for the end endpoint - moving both to the start would
        // collapse a page-sized range to nothing
        bool isEnd = endpoint == TextPatternRangeEndpoint_End;
        int lastPage = GetPageCount();
        *moved = 0;
        for (int i = 0; i < abs(count); ++i) {
            bool canChangePage = count > 0 ? (*target_page < lastPage) : (*target_page > 1);
            if (canChangePage) {
                *target_page += count > 0 ? 1 : -1;
                *target_glyph = isEnd ? GetPageGlyphCount(*target_page) : 0;
                (*moved)++;
                continue;
            }
            // on the first / last page one more step seeks to the start / end
            // of the document; past that there is nowhere left to go and we
            // must report it, or a screen reader reading page by page never
            // reaches the end of the document
            int edgeGlyph = count > 0 ? GetPageGlyphCount(lastPage) : 0;
            if (*target_glyph != edgeGlyph) {
                *target_glyph = edgeGlyph;
                (*moved)++;
            }
            break;
        }
    } else if (unit == TextUnit_Document) {
        if (count > 0) {
            int end_page = GetPageCount();
            int end_glyph = GetPageGlyphCount(end_page);

            if (*target_page != end_page || *target_glyph != end_glyph) {
                *target_page = end_page;
                *target_glyph = end_glyph;
                *moved = 1;
            } else {
                *moved = 0;
            }
        } else {
            // pages are 1-based; page 0 is not a valid endpoint
            const int beg_page = 1;
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
    if (range == nullptr) {
        return E_POINTER;
    }

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

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::Select() {
    HRESULT hrDoc = EnsureDocumentLoaded(document);
    if (FAILED(hrDoc)) {
        return hrDoc;
    }

    if (IsNullRange() || IsEmptyRange()) {
        document->GetDM()->textSelection->Reset();
    } else {
        document->GetDM()->textSelection->Reset();
        document->GetDM()->textSelection->StartAt(startPage, startGlyph);
        document->GetDM()->textSelection->SelectUpTo(endPage, endGlyph);
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::AddToSelection() {
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::RemoveFromSelection() {
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::ScrollIntoView(BOOL /*alignToTop*/) {
    HRESULT hrDoc = EnsureDocumentLoaded(document);
    if (FAILED(hrDoc)) {
        return hrDoc;
    }

#if 0
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
#endif

    // TODO: Scroll to target_page, target_idx
    // document->GetDM()->ScrollYTo()
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationTextRange::GetChildren(SAFEARRAY** children) {
    if (children == nullptr) {
        return E_POINTER;
    }
    HRESULT hrDoc = EnsureDocumentLoaded(document);
    if (FAILED(hrDoc)) {
        return hrDoc;
    }

    // return all children in range
    if (IsNullRange()) {
        SAFEARRAY* psa = SafeArrayCreateVector(VT_UNKNOWN, 0, 0);
        if (!psa) {
            return E_OUTOFMEMORY;
        }

        *children = psa;
        return S_OK;
    }

    SAFEARRAY* psa = SafeArrayCreateVector(VT_UNKNOWN, 0, endPage - startPage + 1);
    if (!psa) {
        return E_OUTOFMEMORY;
    }

    SumatraUIAutomationPageProvider* it = document->GetFirstPage();
    while (it) {
        if (it->GetPageNum() >= startPage || it->GetPageNum() <= endPage) {
            LONG index = it->GetPageNum() - startPage;

            HRESULT hr = SafeArrayPutElement(psa, &index, it);
            ReportIf(FAILED(hr));
            it->AddRef();
        }

        it = it->GetNextPage();
    }

    *children = psa;
    return S_OK;
}
