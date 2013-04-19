#ifndef UIAutomationTextRange_h
#define UIAutomationTextRange_h

#include <UIAutomation.h>

class TextSelection;
class SumatraUIAutomationDocumentProvider;


class SumatraUIAutomationTextRange : public ITextRangeProvider
{
	ULONG m_ref_count;

	// used for getting dm and document state (== is document closed == dm is invalid)
	// text range will hold reference to m_document to prevent it from being removed
	SumatraUIAutomationDocumentProvider* m_document;
	
	// TODO: this part is very much like TextSelection. Merge them somehow?
    // TODO: extend TextSelection to make these unnecessary
    int m_startPage, m_endPage;
    int m_startGlyph, m_endGlyph;
public:
	// creates empty range
	SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document);
	// creates range containing the given page
	SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document, int page_num);
	// creates range containing the given TextSelection range
	SumatraUIAutomationTextRange(SumatraUIAutomationDocumentProvider* document, TextSelection* range);
	// creates a copy of give range
	SumatraUIAutomationTextRange(const SumatraUIAutomationTextRange&);
	~SumatraUIAutomationTextRange();

	void DecreaseRefCount(); // HACK: Sometimes refcounts need some "special" treatment
	bool operator==(const SumatraUIAutomationTextRange&) const;

	void SetToDocumentRange();
	void SetToNullRange();
	bool IsNullRange() const;
	bool IsEmptyRange() const;

	int GetPageGlyphCount(int page_num);
	int GetPageCount();

	void ValidateStartEndpoint();
	void ValidateEndEndpoint();

	int FindPreviousWordEndpoint(int pageno, int idx, bool dont_return_initial=false);
	int FindNextWordEndpoint(int pageno, int idx, bool dont_return_initial=false);
	int FindPreviousLineEndpoint(int pageno, int idx, bool dont_return_initial=false);
	int FindNextLineEndpoint(int pageno, int idx, bool dont_return_initial=false);

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

#endif // UIAutomationTextRange_h
