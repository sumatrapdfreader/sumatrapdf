/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct DisplayModel;
class SumatraUIAutomationProvider;
class SumatraUIAutomationPageProvider;
class SumatraUIAutomationTextRange;

class SumatraUIAutomationDocumentProvider : public IRawElementProviderFragment,
                                            public IRawElementProviderSimple,
                                            public ITextProvider {
    AtomicInt refCount;
    HWND canvasHwnd;
    SumatraUIAutomationProvider* root;
    bool released;

    // lazily created per page (1-based pageNum -> index pageNum - 1)
    Vec<SumatraUIAutomationPageProvider*> pages;

    DisplayModel* dm;

  public:
    SumatraUIAutomationDocumentProvider(HWND canvasHwnd, SumatraUIAutomationProvider* root);
    SumatraUIAutomationDocumentProvider(const SumatraUIAutomationDocumentProvider&) = delete;
    SumatraUIAutomationDocumentProvider& operator=(const SumatraUIAutomationDocumentProvider&) = delete;
    ~SumatraUIAutomationDocumentProvider();

    void LoadDocument(DisplayModel* dm);
    void FreeDocument();
    bool IsDocumentLoaded() const;

    // GetDM() must not be called if IsDocumentLoaded()==FALSE
    DisplayModel* GetDM();
    HWND GetCanvasHwnd() const;

    SumatraUIAutomationPageProvider* GetPage(int pageNum);

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

    // ITextProvider
    HRESULT STDMETHODCALLTYPE GetSelection(SAFEARRAY** pRetVal) override;
    HRESULT STDMETHODCALLTYPE GetVisibleRanges(SAFEARRAY** pRetVal) override;
    HRESULT STDMETHODCALLTYPE RangeFromChild(IRawElementProviderSimple* childElement,
                                             ITextRangeProvider** pRetVal) override;
    HRESULT STDMETHODCALLTYPE RangeFromPoint(struct UiaPoint point, ITextRangeProvider** pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_DocumentRange(ITextRangeProvider** pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_SupportedTextSelection(enum SupportedTextSelection* pRetVal) override;
};
