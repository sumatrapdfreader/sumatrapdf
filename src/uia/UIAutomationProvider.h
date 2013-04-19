#ifndef UIAutomationProvider_h
#define UIAutomationProvider_h

#include <UIAutomationCore.h>

const int SUMATRA_UIA_STARTPAGE_RUNTIME_ID = 1;
const int SUMATRA_UIA_DOCUMENT_RUNTIME_ID = 2;
#define SUMATRA_UIA_PAGE_RUNTIME_ID(X) (100 + (X))

class WindowInfo;
class SumatraUIAutomationStartPageProvider;
class SumatraUIAutomationDocumentProvider;
class SumatraUIAutomationProvider : public IRawElementProviderSimple, public IRawElementProviderFragment, public IRawElementProviderFragmentRoot
{
	ULONG m_ref_count;

	const WindowInfo* m_win;
	HWND m_canvas_hwnd;
	SumatraUIAutomationStartPageProvider* m_startpage;
	SumatraUIAutomationDocumentProvider* m_document;

public:
	SumatraUIAutomationProvider(const WindowInfo*);
private: //ensure no accidental destruction of this class and bypassing refcounting
	~SumatraUIAutomationProvider();

public:
	void OnDocumentLoad();
	void OnDocumentUnload();
	void OnSelectionChanged();

	//IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(const IID &,void **);
	ULONG   STDMETHODCALLTYPE AddRef(void);
	ULONG   STDMETHODCALLTYPE Release(void);

	//IRawElementProviderSimple
	HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID patternId,IUnknown **pRetVal);
	HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId,VARIANT *pRetVal);
	HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple **pRetVal);
	HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions *pRetVal);

	//IRawElementProviderFragment
	HRESULT STDMETHODCALLTYPE Navigate(enum NavigateDirection direction, IRawElementProviderFragment **pRetVal);
	HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY **pRetVal);
	HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY **pRetVal);
	HRESULT STDMETHODCALLTYPE SetFocus(void);
	HRESULT STDMETHODCALLTYPE get_BoundingRectangle(struct UiaRect *pRetVal);
	HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot **pRetVal);

	//IRawElementProviderFragmentRoot
	HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(double,double,IRawElementProviderFragment **);
	HRESULT STDMETHODCALLTYPE GetFocus(IRawElementProviderFragment **);

private:
	IRawElementProviderFragment* GetElementFromPoint(double,double,IRawElementProviderFragment *);
};

#endif //UIAutomationProvider_h
