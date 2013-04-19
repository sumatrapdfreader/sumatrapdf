#ifndef UIAutomationDocumentProvider_h
#define UIAutomationDocumentProvider_h

#include <UIAutomation.h>

class DisplayModel;
class SumatraUIAutomationProvider;
class SumatraUIAutomationPageProvider;
class SumatraUIAutomationTextRange;
class SumatraUIAutomationDocumentProvider : public IRawElementProviderFragment, public IRawElementProviderSimple, public ITextProvider, public IAccIdentity
{
	ULONG m_ref_count;
	HWND m_canvas_hwnd;
	SumatraUIAutomationProvider* m_root;
	bool m_released;

	SumatraUIAutomationPageProvider* m_child_first;
	SumatraUIAutomationPageProvider* m_child_last;

	DisplayModel* m_dm;
public:
	SumatraUIAutomationDocumentProvider(HWND canvas_hwnd, SumatraUIAutomationProvider* root);
	~SumatraUIAutomationDocumentProvider();

	/*
	 * reads page count and creates a child element for each page
	 */
	void LoadDocument(DisplayModel* dm);
	void FreeDocument();
	bool IsDocumentLoaded() const;

	// GetDM() must not be called if IsDocumentLoaded()==FALSE
	DisplayModel* GetDM();

	SumatraUIAutomationPageProvider* GetFirstPage();
	SumatraUIAutomationPageProvider* GetLastPage();

	//IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(const IID &,void **);
	ULONG   STDMETHODCALLTYPE AddRef(void);
	ULONG   STDMETHODCALLTYPE Release(void);
	
	//IRawElementProviderFragment
	HRESULT STDMETHODCALLTYPE Navigate(enum NavigateDirection direction, IRawElementProviderFragment **pRetVal);
	HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY **pRetVal);
	HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY **pRetVal);
	HRESULT STDMETHODCALLTYPE SetFocus(void);
	HRESULT STDMETHODCALLTYPE get_BoundingRectangle(struct UiaRect *pRetVal);
	HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot **pRetVal);

	//IRawElementProviderSimple
	HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID patternId,IUnknown **pRetVal);
	HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId,VARIANT *pRetVal);
	HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple **pRetVal);
	HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions *pRetVal);
	
	//ITextProvider
	HRESULT STDMETHODCALLTYPE GetSelection(SAFEARRAY * *pRetVal);
	HRESULT STDMETHODCALLTYPE GetVisibleRanges(SAFEARRAY * *pRetVal);
	HRESULT STDMETHODCALLTYPE RangeFromChild(IRawElementProviderSimple *childElement, ITextRangeProvider **pRetVal);
	HRESULT STDMETHODCALLTYPE RangeFromPoint( struct UiaPoint point, ITextRangeProvider **pRetVal);
	HRESULT STDMETHODCALLTYPE get_DocumentRange(  ITextRangeProvider **pRetVal);
	HRESULT STDMETHODCALLTYPE get_SupportedTextSelection( enum SupportedTextSelection *pRetVal);

	//IAccIdentity
	HRESULT STDMETHODCALLTYPE GetIdentityString(DWORD dwIDChild, BYTE **ppIDString, DWORD *pdwIDStringLen);
};

#endif //UIAutomationDocumentProvider_h
