/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class SumatraUIAutomationProvider;
class SumatraUIAutomationStartPageProvider : public IRawElementProviderFragment, public IRawElementProviderSimple {
    AtomicInt refCount;
    HWND canvasHwnd;
    SumatraUIAutomationProvider* root; // not ref-counted; nulled by DetachRoot()

  public:
    SumatraUIAutomationStartPageProvider(HWND canvasHwnd, SumatraUIAutomationProvider* root);
    SumatraUIAutomationStartPageProvider(const SumatraUIAutomationStartPageProvider&) = delete;
    SumatraUIAutomationStartPageProvider& operator=(const SumatraUIAutomationStartPageProvider&) = delete;
    void DetachRoot();
    ~SumatraUIAutomationStartPageProvider();

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE Navigate(enum NavigateDirection direction,
                                       IRawElementProviderFragment** pRetVal) override;
    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** pRetVal) override;
    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** pRetVal) override;
    HRESULT STDMETHODCALLTYPE SetFocus() override;
    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(struct UiaRect* pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot** pRetVal) override;

    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID patternId, IUnknown** pRetVal) override;
    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId, VARIANT* pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* pRetVal) override;
};
