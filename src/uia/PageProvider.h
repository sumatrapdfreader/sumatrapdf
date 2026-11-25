/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct DisplayModel;
class SumatraUIAutomationDocumentProvider;
class SumatraUIAutomationPageProvider : public IRawElementProviderFragment,
                                        public IRawElementProviderSimple,
                                        public IValueProvider {
    AtomicInt refCount;
    int pageNum;
    HWND canvasHwnd;
    DisplayModel* dm;
    SumatraUIAutomationDocumentProvider* root;

    // is dm released, and our root has released us.
    // Only UIA keeps us alive but we can't access anything
    bool released;

    friend class SumatraUIAutomationDocumentProvider;

  public:
    SumatraUIAutomationPageProvider(int pageNum, HWND canvasHwnd, DisplayModel* dm,
                                    SumatraUIAutomationDocumentProvider* root);
    SumatraUIAutomationPageProvider(const SumatraUIAutomationPageProvider&) = delete;
    SumatraUIAutomationPageProvider& operator=(const SumatraUIAutomationPageProvider&) = delete;
    ~SumatraUIAutomationPageProvider();

    int GetPageNum() const;

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IRawElementProviderFragment
    HRESULT STDMETHODCALLTYPE Navigate(enum NavigateDirection direction,
                                       IRawElementProviderFragment** pRetVal) override;
    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** pRetVal) override;
    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** pRetVal) override;
    HRESULT STDMETHODCALLTYPE SetFocus() override;
    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(struct UiaRect* pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot** pRetVal) override;

    // IRawElementProviderSimple
    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID patternId, IUnknown** pRetVal) override;
    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId, VARIANT* pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* pRetVal) override;

    // IValueProvider
    HRESULT STDMETHODCALLTYPE SetValue(LPCWSTR val) override;
    HRESULT STDMETHODCALLTYPE get_Value(BSTR* pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_IsReadOnly(BOOL* pRetVal) override;
};
