/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class SumatraUIAutomationProvider;
class SumatraUIAutomationStartPageProvider : public IRawElementProviderFragment, public IRawElementProviderSimple {
    LONG                         refCount;
    HWND                         canvasHwnd;
    SumatraUIAutomationProvider* root;

public:
    SumatraUIAutomationStartPageProvider(HWND canvasHwnd, SumatraUIAutomationProvider* root);
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
