/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class DisplayModel;
class SumatraUIAutomationDocumentProvider;
class SumatraUIAutomationPageProvider : public IRawElementProviderFragment, public IRawElementProviderSimple, public IValueProvider {
    LONG                                    refCount;
    int                                     pageNum;
    HWND                                    canvasHwnd;
    DisplayModel*                           dm;
    SumatraUIAutomationDocumentProvider*    root;

    SumatraUIAutomationPageProvider*        sibling_prev;
    SumatraUIAutomationPageProvider*        sibling_next;

    // is dm released, and our root has released us.
    // Only UIA keeps us alive but we can't access anything
    bool                                    released;
    
    friend class SumatraUIAutomationDocumentProvider; // for setting up next/prev sibling

public:
    SumatraUIAutomationPageProvider(int pageNum,HWND canvasHwnd, DisplayModel*dm, SumatraUIAutomationDocumentProvider* root);
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
