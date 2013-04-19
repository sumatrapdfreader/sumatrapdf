#ifndef UIAutomationPageProvider_h
#define UIAutomationPageProvider_h

#include <UIAutomationCore.h>

class DisplayModel;
class SumatraUIAutomationDocumentProvider;
class SumatraUIAutomationPageProvider : public IRawElementProviderFragment, public IRawElementProviderSimple, public IValueProvider
{
	ULONG m_ref_count;
	int m_page_num; // starts from 1
	HWND m_canvas_hwnd;
	DisplayModel* m_dm;
	SumatraUIAutomationDocumentProvider* m_root;

	SumatraUIAutomationPageProvider* m_sibling_prev;
	SumatraUIAutomationPageProvider* m_sibling_next;
	friend class SumatraUIAutomationDocumentProvider; // for setting up next/prev sibling

	// is dm released, and our root has released us.
	// Only UIA keeps us alive but we can't access anything
	bool m_released;

public:
	SumatraUIAutomationPageProvider(int page_num,HWND canvas_hwnd, DisplayModel*dm, SumatraUIAutomationDocumentProvider* root);
	~SumatraUIAutomationPageProvider();

	int GetPageNum() const;
	SumatraUIAutomationPageProvider* GetNextPage();
	SumatraUIAutomationPageProvider* GetPreviousPage();

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

	//IValueProvider
	HRESULT STDMETHODCALLTYPE SetValue(LPCWSTR val);
	HRESULT STDMETHODCALLTYPE get_Value(BSTR *pRetVal);
	HRESULT STDMETHODCALLTYPE get_IsReadOnly(BOOL *pRetVal);
};

#endif //UIAutomationPageProvider_h
