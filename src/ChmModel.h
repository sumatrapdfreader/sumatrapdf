/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class ChmDoc;
struct ChmTocTraceItem;
class HtmlWindow;
class HtmlWindowCallback;
class ChmCacheEntry;

class ChmModel : public Controller {
public:
    explicit ChmModel(ControllerCallback *cb);
    ~ChmModel() override;

    // meta data
    const WCHAR *FilePath() const override { return fileName; }
    const WCHAR *DefaultFileExt() const override { return L".chm"; }
    int PageCount() const override;
    WCHAR *GetProperty(DocumentProperty prop) override;

    // page navigation (stateful)
    int CurrentPageNo() const override { return currentPageNo; }
    void GoToPage(int pageNo, bool addNavPoint) override {
        UNUSED(addNavPoint);
        CrashIf(!ValidPageNo(pageNo));
        if (ValidPageNo(pageNo))
            DisplayPage(pages.At(pageNo - 1));
    }
    bool CanNavigate(int dir) const override;
    void Navigate(int dir) override;

    // view settings
    void SetDisplayMode(DisplayMode mode, bool keepContinuous = false) override { UNUSED(mode); UNUSED(keepContinuous); /* not supported */ }
    DisplayMode GetDisplayMode() const override { return DM_SINGLE_PAGE; }
    void SetPresentationMode(bool enable) override { UNUSED(enable); /* not supported */ }
    void SetZoomVirtual(float zoom, PointI *fixPt=nullptr) override;
    float GetZoomVirtual(bool absolute=false) const override;
    float GetNextZoomStep(float towards) const override;
    void SetViewPortSize(SizeI size)  override { UNUSED(size); /* not needed(?) */ }

    // table of contents
    bool HasTocTree() const override;
    DocTocItem *GetTocTree() override;
    void ScrollToLink(PageDestination *dest) override;
    PageDestination *GetNamedDest(const WCHAR *name) override;

    // state export
    void UpdateDisplayState(DisplayState *ds) override;
    // asynchronously calls saveThumbnail (fails silently)
    void CreateThumbnail(SizeI size, const onBitmapRenderedCb& saveThumbnail) override;

    // for quick type determination and type-safe casting
    ChmModel *AsChm() override { return this; }

    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static ChmModel *Create(const WCHAR *fileName, ControllerCallback *cb=nullptr);

public:
    // the following is specific to ChmModel

    bool SetParentHwnd(HWND hwnd);
    void RemoveParentHwnd();

    void PrintCurrentPage(bool showUI);
    void FindInCurrentPage();
    void SelectAll();
    void CopySelection();
    LRESULT PassUIMsg(UINT msg, WPARAM wParam, LPARAM lParam);

    // for HtmlWindowCallback (called through htmlWindowCb)
    bool OnBeforeNavigate(const WCHAR *url, bool newWindow);
    void OnDocumentComplete(const WCHAR *url);
    void OnLButtonDown();
    const unsigned char *GetDataForUrl(const WCHAR *url, size_t *len);
    void DownloadData(const WCHAR *url, const unsigned char *data, size_t len);

protected:
    AutoFreeW fileName;
    ChmDoc *doc;
    CRITICAL_SECTION docAccess;
    Vec<ChmTocTraceItem> *tocTrace;

    WStrList pages;
    int currentPageNo;
    HtmlWindow *htmlWindow;
    HtmlWindowCallback *htmlWindowCb;
    float initZoom;

    Vec<ChmCacheEntry*> urlDataCache;
    // use a pool allocator for strings that aren't freed until this ChmModel
    // is deleted (e.g. for titles and URLs for ChmTocItem and ChmCacheEntry)
    PoolAllocator poolAlloc;

    bool Load(const WCHAR *fileName);
    void DisplayPage(const WCHAR *pageUrl);

    ChmCacheEntry *FindDataForUrl(const WCHAR *url);

    void ZoomTo(float zoomLevel);
};
