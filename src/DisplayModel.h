/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef DisplayModel_h
#define DisplayModel_h

#include "Controller.h"

// define the following if you want shadows drawn around the pages
// #define DRAW_PAGE_SHADOWS

#define INVALID_PAGE_NO     -1

extern bool gPredictiveRender;

/* Describes many attributes of one page in one, convenient place */
struct PageInfo {
    /* data that is constant for a given page. page size in document units */
    RectD           page;

    /* data that is calculated when needed. actual content size within a page (View target) */
    RectD           contentBox;

    /* data that needs to be set before DisplayModel::Relayout().
       Determines whether a given page should be shown on the screen. */
    bool            shown;

    /* data that changes when zoom and rotation changes */
    /* position and size within total area after applying zoom and rotation.
       Represents display rectangle for a given page.
       Calculated in DisplayModel::Relayout() */
    RectI           pos;

    /* data that changes due to scrolling. Calculated in DisplayModel::RecalcVisibleParts() */
    float           visibleRatio; /* (0.0 = invisible, 1.0 = fully visible) */
    /* position of page relative to visible view port: pos.Offset(-viewPort.x, -viewPort.y) */
    RectI           pageOnScreen;
};

/* The current scroll state (needed for saving/restoring the scroll position) */
/* coordinates are in user space units (per page) */
struct ScrollState {
    explicit ScrollState(int page=0, double x=0, double y=0) : page(page), x(x), y(y) { }
    bool operator==(const ScrollState& other) const {
        return page == other.page && x == other.x && y == other.y;
    }

    int page;
    double x, y;
};

class PageTextCache;
class TextSelection;
class TextSearch;
struct TextSel;
class Synchronizer;
enum EngineType;

// TODO: in hindsight, zoomVirtual is not a good name since it's either
// virtual zoom level OR physical zoom level. Would be good to find
// better naming scheme (call it zoomLevel?)

/* Information needed to drive the display of a given document on a screen.
   You can think of it as a model in the MVC pardigm.
   All the display changes should be done through changing this model via
   API and re-displaying things based on new display information */
class DisplayModel : public Controller
{
public:
    DisplayModel(BaseEngine *engine, EngineType type, ControllerCallback *cb);
    ~DisplayModel();

    // meta data
    virtual const WCHAR *FilePath() const { return _engine->FileName(); }
    virtual const WCHAR *DefaultFileExt() const { return _engine->GetDefaultFileExt(); }
    virtual int PageCount() const { return _engine->PageCount(); }
    virtual WCHAR *GetProperty(DocumentProperty prop) { return _engine->GetProperty(prop); }

    // page navigation (stateful)
    virtual int CurrentPageNo() const;
    virtual void GoToPage(int pageNo, bool addNavPoint) { GoToPage(pageNo, 0, addNavPoint); }
    virtual bool CanNavigate(int dir) const;
    virtual void Navigate(int dir);

    // view settings
    virtual void SetDisplayMode(DisplayMode mode, bool keepContinuous=true);
    virtual DisplayMode GetDisplayMode() const { return displayMode; }
    virtual void SetPresentationMode(bool enable);
    virtual void SetZoomVirtual(float zoom, PointI *fixPt=NULL) { ZoomTo(zoom, fixPt); }
    virtual float GetZoomVirtual() const { return zoomVirtual; }
    virtual float GetNextZoomStep(float towards) const;
    virtual void SetViewPortSize(SizeI size);

    // table of contents
    virtual bool HasTocTree() const { return _engine->HasTocTree(); }
    virtual DocTocItem *GetTocTree() { return _engine->GetTocTree(); }
    virtual void ScrollToLink(PageDestination *dest);
    virtual PageDestination *GetNamedDest(const WCHAR *name) { return _engine->GetNamedDest(name); }

    // state export
    virtual void UpdateDisplayState(DisplayState *ds);
    // asynchronously calls ThumbnailCallback::SaveThumbnail (fails silently)
    virtual void CreateThumbnail(SizeI size, ThumbnailCallback *tnCb) { cb->RenderThumbnail(this, size, tnCb); }

    // page labels (optional)
    virtual bool HasPageLabels() const { return _engine->HasPageLabels(); }
    virtual WCHAR *GetPageLabel(int pageNo) const { return _engine->GetPageLabel(pageNo); }
    virtual int GetPageByLabel(const WCHAR *label) const { return _engine->GetPageByLabel(label); }

    // common shortcuts
    virtual bool ValidPageNo(int pageNo) const { return 1 <= pageNo && pageNo <= _engine->PageCount(); }
    virtual bool GoToNextPage();
    virtual bool GoToPrevPage(bool toBottom=false) { return GoToPrevPage(toBottom ? -1 : 0); }
    virtual bool GoToFirstPage();
    virtual bool GoToLastPage();

    // for quick type determination and type-safe casting
    virtual DisplayModel *AsFixed() { return this; }

public:
    // the following is specific to DisplayModel

    BaseEngine *engine() const { return _engine; }

    // controller-specific data (easier to save here than on WindowInfo)
    EngineType      engineType;
    Vec<PageAnnotation> *userAnnots;
    bool            userAnnotsModified;
    Synchronizer *  pdfSync;

    PageTextCache * textCache;
    TextSelection * textSelection;
    // access only from Search thread
    TextSearch *    textSearch;

    PageInfo *      GetPageInfo(int pageNo) const;

    /* current rotation selected by user */
    int             GetRotation() const { return rotation; }
    // Note: zoomReal contains dpiFactor premultiplied
    float           GetZoomReal() const { return zoomReal; }
    float           GetZoomReal(int pageNo) const;
    void            Relayout(float zoomVirtual, int rotation);

    RectI           GetViewPort() const { return viewPort; }
    bool            NeedHScroll() const { return viewPort.dy < totalViewPortSize.dy; }
    bool            NeedVScroll() const { return viewPort.dx < totalViewPortSize.dx; }
    SizeI           GetCanvasSize() const { return canvasSize; }

    bool            PageShown(int pageNo) const;
    bool            PageVisible(int pageNo) const;
    bool            PageVisibleNearby(int pageNo) const;
    int             FirstVisiblePageNo() const;
    bool            FirstBookPageVisible() const;
    bool            LastBookPageVisible() const;

    void            ScrollXTo(int xOff);
    void            ScrollXBy(int dx);
    void            ScrollYTo(int yOff);
    void            ScrollYBy(int dy, bool changePage);
    /* a "virtual" zoom level. Can be either a real zoom level in percent
       (i.e. 100.0 is original size) or one of virtual values ZOOM_FIT_PAGE,
       ZOOM_FIT_WIDTH or ZOOM_FIT_CONTENT, whose real value depends on draw area size */
    void            ZoomTo(float zoomVirtual, PointI *fixPt=NULL);
    void            ZoomBy(float zoomFactor, PointI *fixPt=NULL);
    void            RotateBy(int rotation);

    WCHAR *         GetTextInRegion(int pageNo, RectD region);
    bool            IsOverText(PointI pt);
    PageElement *   GetElementAtPos(PointI pt);

    int             GetPageNoByPoint(PointI pt);
    PointI          CvtToScreen(int pageNo, PointD pt);
    RectI           CvtToScreen(int pageNo, RectD r);
    PointD          CvtFromScreen(PointI pt, int pageNo=INVALID_PAGE_NO);
    RectD           CvtFromScreen(RectI r, int pageNo=INVALID_PAGE_NO);

    bool            ShowResultRectToScreen(TextSel *res);

    ScrollState     GetScrollState();
    void            SetScrollState(ScrollState state);

    void            CopyNavHistory(DisplayModel& orig);

    void            SetInitialViewSettings(DisplayMode displayMode, int newStartPage, SizeI viewPort, int screenDPI);
    void            SetDisplayR2L(bool r2l) { displayR2L = r2l; }
    bool            GetDisplayR2L() const { return displayR2L; }

    bool            ShouldCacheRendering(int pageNo);
    // called when we decide that the display needs to be redrawn
    void            RepaintDisplay() { cb->Repaint(); }

    /* allow resizing a window without triggering a new rendering (needed for window destruction) */
    bool            dontRenderFlag;

    bool            GetPresentationMode() const { return presentationMode; }

protected:

    void            BuildPagesInfo();
    float           ZoomRealFromVirtualForPage(float zoomVirtual, int pageNo) const;
    SizeD           PageSizeAfterRotation(int pageNo, bool fitToContent=false) const;
    void            ChangeStartPage(int startPage);
    PointI          GetContentStart(int pageNo);
    void            RecalcVisibleParts();
    void            RenderVisibleParts();
    void            AddNavPoint();
    RectD           GetContentBox(int pageNo, RenderTarget target=Target_View);
    void            CalcZoomVirtual(float zoomVirtual);
    float           GetZoomAbsolute() const { return zoomReal * 100 / dpiFactor; }
    void            GoToPage(int pageNo, int scrollY, bool addNavPt=false, int scrollX=-1);
    bool            GoToPrevPage(int scrollY);
    int             GetPageNextToPoint(PointI pt);

    BaseEngine *    _engine;

    /* an array of PageInfo, len of array is pageCount */
    PageInfo *      pagesInfo;

    DisplayMode     displayMode;
    /* In non-continuous mode is the first page from a file that we're
       displaying.
       No meaning in continous mode. */
    int             startPage;

    /* size of virtual canvas containing all rendered pages. */
    SizeI           canvasSize;
    /* size and position of the viewport on the canvas (resp size of the visible
       part of the canvase available for content (totalViewPortSize minus scroll bars)
       (canvasSize is always at least as big as viewPort.Size()) */
    RectI           viewPort;
    /* total size of view port (draw area), including scroll bars */
    SizeI           totalViewPortSize;

    WindowMargin    windowMargin;
    SizeI           pageSpacing;

    /* real zoom value calculated from zoomVirtual. Same as
       zoomVirtual * 0.01 * dpiFactor
       except for ZOOM_FIT_PAGE, ZOOM_FIT_WIDTH and ZOOM_FIT_CONTENT */
    float           zoomReal;
    float           zoomVirtual;
    int             rotation;
    /* dpi correction factor by which _zoomVirtual has to be multiplied in
       order to get _zoomReal */
    float           dpiFactor;
    /* whether to display pages Left-to-Right or Right-to-Left.
       this value is extracted from the PDF document */
    bool            displayR2L;

    /* when we're in presentation mode, _pres* contains the pre-presentation values */
    bool            presentationMode;
    float           presZoomVirtual;
    DisplayMode     presDisplayMode;

    Vec<ScrollState>navHistory;
    /* index of the "current" history entry (to be updated on navigation),
       resp. number of Back history entries */
    size_t          navHistoryIx;
};

int     NormalizeRotation(int rotation);

#endif
