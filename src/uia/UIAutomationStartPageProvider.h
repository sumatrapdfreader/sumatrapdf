#ifndef UIAutomationStartPageProvider_h
#define UIAutomationStartPageProvider_h

#include <UIAutomationCore.h>
#include "UIAutomationProvider.h"

class SumatraUIAutomationStartPageProvider : public IRawElementProviderFragment, public IRawElementProviderSimple
{
	ULONG m_ref_count;
	HWND m_canvas_hwnd;
	SumatraUIAutomationProvider* m_root;

public:
	SumatraUIAutomationStartPageProvider(HWND canvas_hwnd, SumatraUIAutomationProvider* root);
	~SumatraUIAutomationStartPageProvider();
	
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
};

#endif //UIAutomationStartPageProvider_h
