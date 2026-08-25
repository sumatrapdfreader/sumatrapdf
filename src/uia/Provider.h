/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

constexpr int kSumatraUiaStartPageRuntimeId = 1;
constexpr int kSumatraUiaDocumentRuntimeId = 2;

constexpr int kSumatraUiaPageRuntimeId(int pageNum) {
    return 100 + pageNum;
}

struct DisplayModel;
class SumatraUIAutomationStartPageProvider;
class SumatraUIAutomationDocumentProvider;

class SumatraUIAutomationProvider : public IRawElementProviderSimple,
                                    public IRawElementProviderFragment,
                                    public IRawElementProviderFragmentRoot {
    AtomicInt refCount;

    HWND canvasHwnd;
    SumatraUIAutomationStartPageProvider* startpage;
    SumatraUIAutomationDocumentProvider* document;

  public:
    explicit SumatraUIAutomationProvider(HWND hwnd);

  private: // ensure no accidental destruction of this class and bypassing refcounting
    ~SumatraUIAutomationProvider();

  public:
    void OnDocumentLoad(DisplayModel* dm);
    void OnDocumentUnload();
    void OnSelectionChanged();

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IRawElementProviderSimple
    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID patternId, IUnknown** pRetVal) override;
    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId, VARIANT* pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* pRetVal) override;

    // IRawElementProviderFragment
    HRESULT STDMETHODCALLTYPE Navigate(enum NavigateDirection direction,
                                       IRawElementProviderFragment** pRetVal) override;
    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** pRetVal) override;
    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** pRetVal) override;
    HRESULT STDMETHODCALLTYPE SetFocus() override;
    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(struct UiaRect* pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot** pRetVal) override;

    // IRawElementProviderFragmentRoot
    HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(double, double, IRawElementProviderFragment**) override;
    HRESULT STDMETHODCALLTYPE GetFocus(IRawElementProviderFragment**) override;

  private:
    IRawElementProviderFragment* GetElementFromPoint(double, double, IRawElementProviderFragment*);
};
