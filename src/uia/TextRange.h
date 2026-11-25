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
    SumatraUIAutomationTextRange* SameDocRange(ITextRangeProvider* range);

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

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE Clone(ITextRangeProvider** clonedRange) override;
    HRESULT STDMETHODCALLTYPE Compare(ITextRangeProvider* range, BOOL* areSame) override;
    HRESULT STDMETHODCALLTYPE CompareEndpoints(enum TextPatternRangeEndpoint srcEndPoint, ITextRangeProvider* range,
                                               enum TextPatternRangeEndpoint targetEndPoint, int* compValue) override;
    HRESULT STDMETHODCALLTYPE ExpandToEnclosingUnit(enum TextUnit textUnit) override;
    HRESULT STDMETHODCALLTYPE FindAttribute(TEXTATTRIBUTEID attr, VARIANT val, BOOL backward,
                                            ITextRangeProvider** found) override;
    HRESULT STDMETHODCALLTYPE FindText(BSTR text, BOOL backward, BOOL ignoreCase, ITextRangeProvider** found) override;
    HRESULT STDMETHODCALLTYPE GetAttributeValue(TEXTATTRIBUTEID attr, VARIANT* value) override;
    HRESULT STDMETHODCALLTYPE GetBoundingRectangles(SAFEARRAY** boundingRects) override;
    HRESULT STDMETHODCALLTYPE GetEnclosingElement(IRawElementProviderSimple** enclosingElement) override;
    HRESULT STDMETHODCALLTYPE GetText(int maxLength, BSTR* text) override;
    HRESULT STDMETHODCALLTYPE Move(enum TextUnit unit, int count, int* moved) override;
    HRESULT STDMETHODCALLTYPE MoveEndpointByUnit(TextPatternRangeEndpoint endpoint, TextUnit unit, int count,
                                                 int* moved) override;
    HRESULT STDMETHODCALLTYPE MoveEndpointByRange(TextPatternRangeEndpoint srcEndPoint, ITextRangeProvider* range,
                                                  TextPatternRangeEndpoint targetEndPoint) override;
    HRESULT STDMETHODCALLTYPE Select() override;
    HRESULT STDMETHODCALLTYPE AddToSelection() override;
    HRESULT STDMETHODCALLTYPE RemoveFromSelection() override;
    HRESULT STDMETHODCALLTYPE ScrollIntoView(BOOL alignToTop) override;
    HRESULT STDMETHODCALLTYPE GetChildren(SAFEARRAY** children) override;
};
