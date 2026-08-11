/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct TextSelection;
class SumatraUIAutomationDocumentProvider;

class SumatraUIAutomationTextRange : public ITextRangeProvider {
    AtomicInt refCount;

    // used for getting dm and document state (== is document closed == dm is invalid)
    // text range will hold reference to document to prevent it from being removed
    SumatraUIAutomationDocumentProvider* document;

    // TODO: this part is very much like TextSelection. Merge them somehow?
    // TODO: extend TextSelection to make these unnecessary
    int startPage, endPage;
    int startGlyph, endGlyph;

  public:
    SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document);
    SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document, int pageNum);
    SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document, TextSelection* range);
    SumatraUIAutomationTextRange(const SumatraUIAutomationTextRange&);
    SumatraUIAutomationTextRange& operator=(const SumatraUIAutomationTextRange&) = delete;

    ~SumatraUIAutomationTextRange();

    bool operator==(const SumatraUIAutomationTextRange&) const;

    void SetToDocumentRange();
    void SetToDegenerateAt(int pageNo, int glyphIdx);
    void SetToNullRange();
    bool IsNullRange() const;
    bool IsEmptyRange() const;

    int GetPageGlyphCount(int pageNum);
    int GetPageCount();

    void ValidateStartEndpoint();
    void ValidateEndEndpoint();

    int FindPreviousWordEndpoint(int pageno, int idx, bool dontReturnInitial = false);
    int FindNextWordEndpoint(int pageno, int idx, bool dontReturnInitial = false);
    int FindPreviousLineEndpoint(int pageno, int idx, bool dontReturnInitial = false);
    int FindNextLineEndpoint(int pageno, int idx, bool dontReturnInitial = false);

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE Clone(ITextRangeProvider** clonedRange);
    HRESULT STDMETHODCALLTYPE Compare(ITextRangeProvider* range, BOOL* areSame);
    HRESULT STDMETHODCALLTYPE CompareEndpoints(enum TextPatternRangeEndpoint srcEndPoint, ITextRangeProvider* range,
                                               enum TextPatternRangeEndpoint targetEndPoint, int* compValue);
    HRESULT STDMETHODCALLTYPE ExpandToEnclosingUnit(enum TextUnit textUnit);
    HRESULT STDMETHODCALLTYPE FindAttribute(TEXTATTRIBUTEID attr, VARIANT val, BOOL backward,
                                            ITextRangeProvider** found);
    HRESULT STDMETHODCALLTYPE FindText(BSTR text, BOOL backward, BOOL ignoreCase, ITextRangeProvider** found);
    HRESULT STDMETHODCALLTYPE GetAttributeValue(TEXTATTRIBUTEID attr, VARIANT* value);
    HRESULT STDMETHODCALLTYPE GetBoundingRectangles(SAFEARRAY** boundingRects);
    HRESULT STDMETHODCALLTYPE GetEnclosingElement(IRawElementProviderSimple** enclosingElement);
    HRESULT STDMETHODCALLTYPE GetText(int maxLength, BSTR* text);
    HRESULT STDMETHODCALLTYPE Move(enum TextUnit unit, int count, int* moved);
    HRESULT STDMETHODCALLTYPE MoveEndpointByUnit(TextPatternRangeEndpoint endpoint, TextUnit unit, int count,
                                                 int* moved);
    HRESULT STDMETHODCALLTYPE MoveEndpointByRange(TextPatternRangeEndpoint srcEndPoint, ITextRangeProvider* range,
                                                  TextPatternRangeEndpoint targetEndPoint);
    HRESULT STDMETHODCALLTYPE Select();
    HRESULT STDMETHODCALLTYPE AddToSelection();
    HRESULT STDMETHODCALLTYPE RemoveFromSelection();
    HRESULT STDMETHODCALLTYPE ScrollIntoView(BOOL alignToTop);
    HRESULT STDMETHODCALLTYPE GetChildren(SAFEARRAY** children);
};
