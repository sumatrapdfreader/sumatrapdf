/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ChmModel_h
#define ChmModel_h

#include "Controller.h"

class ChmDoc;
struct ChmTocTraceItem;
class HtmlWindow;
class HtmlWindowCallback;
class ChmCacheEntry;

class ChmModel : public Controller {
public:
    explicit ChmModel(ControllerCallback *cb);
    virtual ~ChmModel();

    // meta data
    virtual const WCHAR *FilePath() const { return fileName; }
    virtual const WCHAR *DefaultFileExt() const { return L".chm"; }
    virtual int PageCount() const;
    virtual WCHAR *GetProperty(DocumentProperty prop);

    // page navigation (stateful)
    virtual int CurrentPageNo() const { return currentPageNo; }
    virtual void GoToPage(int pageNo, bool addNavPoint) { CrashIf(!ValidPageNo(pageNo)); DisplayPage(pages.At(pageNo - 1)); }
    virtual bool CanNavigate(int dir) const;
    virtual void Navigate(int dir);

    // view settings
    virtual void SetDisplayMode(DisplayMode mode, bool keepContinuous=false) { /* not supported */ }
    virtual DisplayMode GetDisplayMode() const { return DM_SINGLE_PAGE; }
    virtual void SetPresentationMode(bool enable) { /* not supported */ }
    virtual void SetZoomVirtual(float zoom, PointI *fixPt=NULL);
    virtual float GetZoomVirtual() const;
    virtual float GetNextZoomStep(float towards) const;
    virtual void SetViewPortSize(SizeI size) { /* not needed(?) */ }

    // table of contents
    virtual bool HasTocTree() const;
    virtual DocTocItem *GetTocTree();
    virtual void ScrollToLink(PageDestination *dest);
    virtual PageDestination *GetNamedDest(const WCHAR *name);

    // state export
    virtual void UpdateDisplayState(DisplayState *ds);
    // asynchronously calls ThumbnailCallback::SaveThumbnail (fails silently)
    virtual void CreateThumbnail(SizeI size, ThumbnailCallback *tnCb);

    // for quick type determination and type-safe casting
    virtual ChmModel *AsChm() { return this; }

    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static ChmModel *Create(const WCHAR *fileName, ControllerCallback *cb=NULL);

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
    ScopedMem<WCHAR> fileName;
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

#endif
