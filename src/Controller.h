/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Controller_h
#define Controller_h

#include "DisplayState.h"
#include "EngineManager.h"

class FixedPageUIController;
class ChmUIController;
class EbookUIController;

class Controller {
public:
    virtual ~Controller() { }

    // meta data
    virtual const WCHAR *FilePath() const = 0;
    virtual const WCHAR *DefaultFileExt() const = 0;
    virtual int PageCount() const = 0;
    virtual WCHAR *GetProperty(DocumentProperty prop) = 0;

    // page navigation (stateful)
    virtual int CurrentPageNo() = 0;
    virtual void GoToPage(int pageNo, bool addNavPoint=true) = 0;
    virtual bool CanNavigate(int dir) = 0;
    virtual void Navigate(int dir) = 0;

    // view settings
    virtual void SetDisplayMode(DisplayMode mode, bool keepContinuous=true) = 0;
    virtual DisplayMode GetDisplayMode() const = 0;
    virtual void SetPresentationMode(bool enable) = 0;
    virtual void SetZoomVirtual(float zoom, PointI *fixPt=NULL) = 0;
    virtual float GetZoomVirtual() const = 0;
    virtual float GetNextZoomStep(float towards) const = 0;
    virtual void SetViewPortSize(SizeI size) = 0;

    // table of contents
    virtual bool HasTocTree() const = 0;
    virtual DocTocItem *GetTocTree() = 0;
    virtual void GotoLink(PageDestination *dest) = 0;
    virtual PageDestination *GetNamedDest(const WCHAR *name) = 0;

    // state export
    virtual void UpdateDisplayState(DisplayState *ds) = 0;
    // asynchronously calls SaveThumbnailForFile (fails silently)
    virtual void CreateThumbnail(DisplayState *ds) = 0;

    // page labels (optional)
    virtual bool HasPageLabels() const { return false; }
    virtual WCHAR *GetPageLabel(int pageNo) const { return str::Format(L"%d", pageNo); }
    virtual int GetPageByLabel(const WCHAR *label) const { return _wtoi(label); }

    // common shortcuts
    virtual bool ValidPageNo(int pageNo) const {
        return 1 <= pageNo && pageNo <= PageCount();
    }
    virtual bool GoToNextPage() {
        if (CurrentPageNo() == PageCount())
            return false;
        GoToPage(CurrentPageNo() + 1, false);
        return true;
    }
    virtual bool GoToPrevPage(bool toBottom=false) {
        if (CurrentPageNo() == 1)
            return false;
        GoToPage(CurrentPageNo() - 1, false);
        return true;
    }
    virtual bool GoToFirstPage() {
        if (CurrentPageNo() == 1)
            return false;
        GoToPage(1);
        return true;
    }
    virtual bool GoToLastPage() {
        if (CurrentPageNo() == PageCount())
            return false;
        GoToPage(PageCount());
        return true;
    }

    // for quick type determination and type-safe casting
    virtual FixedPageUIController *AsFixed() { return NULL; }
    virtual ChmUIController *AsChm() { return NULL; }
    virtual EbookUIController *AsEbook() { return NULL; }
};

class DisplayModel;
class LinkHandler;

class FixedPageUIController : public Controller {
public:
    FixedPageUIController() : userAnnots(NULL), userAnnotsModified(false), engineType(Engine_None) { }
    virtual ~FixedPageUIController() { delete userAnnots; }

    virtual DisplayModel *model() = 0;
    virtual BaseEngine *engine() = 0;

    // controller-specific data (easier to save here than on WindowInfo)
    EngineType engineType;
    Vec<PageAnnotation> *userAnnots;
    bool userAnnotsModified;

    static FixedPageUIController *Create(DisplayModel *dm, LinkHandler *linkHandler);
};

class ChmEngine;
class WindowInfo;

class ChmUIController : public Controller {
public:
    virtual ChmEngine *engine() = 0;

    static ChmUIController *Create(ChmEngine *engine, WindowInfo *win);
};

class EbookController;
struct EbookControls;
class Doc;

class EbookUIController : public Controller {
public:
    virtual EbookController *ctrl() = 0;
    virtual EbookControls *ctrls() = 0;
    virtual Doc *doc() = 0;

    virtual LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& wasHandled) = 0;
    // EbookController's constructor calls UpdateWindow which
    // must not happen before EbookUIController::Create returns
    virtual void SetController(EbookController *ctrl) = 0;

    static EbookUIController *Create(HWND hwnd);
};

#endif
