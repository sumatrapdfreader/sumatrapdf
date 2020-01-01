/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class TextSelection;
class SumatraUIAutomationDocumentProvider;

class SumatraUIAutomationTextRange : public ITextRangeProvider {
    LONG refCount;

    // used for getting dm and document state (== is document closed == dm is invalid)
    // text range will hold reference to document to prevent it from being removed
    SumatraUIAutomationDocumentProvider* document;
    
    // TODO: this part is very much like TextSelection. Merge them somehow?
    // TODO: extend TextSelection to make these unnecessary
    int startPage, endPage;
    int startGlyph, endGlyph;
public:
    // creates empty range
    SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document);
    // creates range containing the given page
    SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document, int pageNum);
    // creates range containing the given TextSelection range
    SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document, TextSelection* range);
    // creates a copy of give range
    SumatraUIAutomationTextRange(const SumatraUIAutomationTextRange&);
    ~SumatraUIAutomationTextRange();

    bool operator==(const SumatraUIAutomationTextRange&) const;

    void SetToDocumentRange();
    void SetToNullRange();
    bool IsNullRange() const;
    bool IsEmptyRange() const;

    int GetPageGlyphCount(int pageNum);
    int GetPageCount();

    void ValidateStartEndpoint();
    void ValidateEndEndpoint();

    int FindPreviousWordEndpoint(int pageno, int idx, bool dontReturnInitial=false);
    int FindNextWordEndpoint(int pageno, int idx, bool dontReturnInitial=false);
    int FindPreviousLineEndpoint(int pageno, int idx, bool dontReturnInitial=false);
    int FindNextLineEndpoint(int pageno, int idx, bool dontReturnInitial=false);

    //IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(const IID &,void **);
    ULONG   STDMETHODCALLTYPE AddRef(void);
    ULONG   STDMETHODCALLTYPE Release(void);
    
    //ITextRangeProvider
    HRESULT STDMETHODCALLTYPE Clone(ITextRangeProvider **clonedRange);
    HRESULT STDMETHODCALLTYPE Compare(ITextRangeProvider *range, BOOL *areSame);
    HRESULT STDMETHODCALLTYPE CompareEndpoints(enum TextPatternRangeEndpoint srcEndPoint, ITextRangeProvider *range, enum TextPatternRangeEndpoint targetEndPoint, int *compValue);
    HRESULT STDMETHODCALLTYPE ExpandToEnclosingUnit(enum TextUnit textUnit);
    HRESULT STDMETHODCALLTYPE FindAttribute(TEXTATTRIBUTEID attr, VARIANT val, BOOL backward, ITextRangeProvider **found);
    HRESULT STDMETHODCALLTYPE FindText(BSTR text, BOOL backward, BOOL ignoreCase, ITextRangeProvider **found);
    HRESULT STDMETHODCALLTYPE GetAttributeValue(TEXTATTRIBUTEID attr, VARIANT *value);
    HRESULT STDMETHODCALLTYPE GetBoundingRectangles(SAFEARRAY * *boundingRects);
    HRESULT STDMETHODCALLTYPE GetEnclosingElement(IRawElementProviderSimple **enclosingElement);
    HRESULT STDMETHODCALLTYPE GetText(int maxLength, BSTR *text);
    HRESULT STDMETHODCALLTYPE Move(enum TextUnit unit,int count, int *moved);
    HRESULT STDMETHODCALLTYPE MoveEndpointByUnit(TextPatternRangeEndpoint endpoint, TextUnit unit, int count, int *moved);
    HRESULT STDMETHODCALLTYPE MoveEndpointByRange(TextPatternRangeEndpoint srcEndPoint, ITextRangeProvider *range, TextPatternRangeEndpoint targetEndPoint);
    HRESULT STDMETHODCALLTYPE Select(void);
    HRESULT STDMETHODCALLTYPE AddToSelection(void);
    HRESULT STDMETHODCALLTYPE RemoveFromSelection(void);
    HRESULT STDMETHODCALLTYPE ScrollIntoView(BOOL alignToTop);
    HRESULT STDMETHODCALLTYPE GetChildren(SAFEARRAY **children);
};
