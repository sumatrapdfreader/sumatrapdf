/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/UIModels.h"
#include "gui/BrowserView.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "EngineBase.h"
#include "DocController.h"

Location DocController::CurrentLocation() {
    return LocFromPageNo(CurrentPageNo());
}

void DocController::GoToLocation(Location loc, bool addNavPoint) {
    GoToPage(loc.page, addNavPoint);
}

Location DocController::LocationFromPageNo(int pageNo) {
    return LocFromPageNo(pageNo);
}

int DocController::PageNoFromLocation(Location loc) {
    return loc.page;
}

// default: dest->loc if valid else LocFromPageNo(dest->pageNo)
Location DocController::ResolveDest(IPageDestination* dest) {
    if (!dest) {
        return kInvalidLocation;
    }
    return dest->loc.IsValid() ? dest->loc : LocFromPageNo(dest->pageNo);
}

// default: no chapters, no engine bookmark
TempStr DocController::MakeBookmarkTemp(__unused Location loc) {
    return {};
}

Location DocController::LookupBookmark(__unused Str s) {
    return kInvalidLocation;
}

// default: single-chapter, clamp page into [1, PageCount()]
Location DocController::ClampLocation(Location loc) {
    return {1, limitValue(loc.page, 1, PageCount())};
}

BrowserDocController::BrowserDocController(DocControllerCallback* cb) : DocController(cb), initZoom(kInvalidZoom) {}

BrowserDocController::~BrowserDocController() {
    str::Free(currentPageUrl);
    str::Free(pendingFindTerm);
}

int BrowserDocController::PageCount() const {
    return len(pages);
}

int BrowserDocController::CurrentPageNo() const {
    return currentPageNo;
}

bool BrowserDocController::CanNavigate(int dir) const {
    return dir < 0 ? BrowserViewCanGoBack(docView) : BrowserViewCanGoForward(docView);
}

void BrowserDocController::Navigate(int dir) {
    if (!docView) {
        return;
    }

    while (dir < 0 && CanNavigate(dir)) {
        BrowserViewGoBack(docView);
        dir++;
    }
    while (dir > 0 && CanNavigate(dir)) {
        BrowserViewGoForward(docView);
        dir--;
    }
}

void BrowserDocController::SetDisplayMode(DisplayMode, bool) {}

DisplayMode BrowserDocController::GetDisplayMode() const {
    return DisplayMode::SinglePage;
}

void BrowserDocController::SetInPresentation(bool) {}

void BrowserDocController::SetViewPortSize(Size) {}

void BrowserDocController::SetZoomVirtual(float zoom, Point*) {
    if (zoom > 0) {
        zoom = limitValue(zoom, kZoomMin, kZoomMax);
    }
    if (!IsValidZoom(zoom)) {
        zoom = 100.0f;
    }
    BrowserViewSetZoomPercent(docView, (int)zoom);
    zoomVirtual = zoom;
    initZoom = zoom;
}

float BrowserDocController::GetZoomVirtual(bool) const {
    return docView ? (float)BrowserViewGetZoomPercent(docView) : zoomVirtual;
}

float BrowserDocController::GetNextZoomStep(float towardsLevel) const {
    float currZoom = GetZoomVirtual(true);
    if (MaybeGetNextZoomByIncrement(&currZoom, towardsLevel)) {
        if ((int)currZoom == (int)GetZoomVirtual(true)) {
            currZoom += 1.f;
        }
        return currZoom;
    }

    int nZoomLevels;
    float* zoomLevels = GetDefaultZoomLevels(&nZoomLevels);
    int curr = (int)currZoom;
    int next = (int)towardsLevel;
    if (currZoom < towardsLevel) {
        for (int i = 0; i < nZoomLevels; i++) {
            if ((int)zoomLevels[i] > curr) {
                next = (int)zoomLevels[i];
                break;
            }
        }
    } else if (currZoom > towardsLevel) {
        for (int i = nZoomLevels - 1; i >= 0; i--) {
            if ((int)zoomLevels[i] < curr) {
                next = (int)zoomLevels[i];
                break;
            }
        }
    }
    return (float)next;
}

void BrowserDocController::PrintCurrentPage(bool showUI) const {
    BrowserViewPrint(docView, showUI);
}

void BrowserDocController::FindInCurrentPage() const {
    BrowserViewFindInPageUI(docView);
}

bool BrowserDocController::CanFindInPage() const {
    return BrowserViewCanFindInPage(docView);
}

void BrowserDocController::FindStart(Str term, bool matchCase, bool wholeWord, int gen) {
    BrowserViewFindStart(docView, term, matchCase, wholeWord, gen, -1);
}

void BrowserDocController::FindGoto(int idx) {
    BrowserViewFindGoto(docView, idx);
}

void BrowserDocController::GoToPageWithFind(int pageNo, Str term, bool matchCase, bool wholeWord, int idx, int gen) {
    if (!ValidPageNo(pageNo)) {
        return;
    }
    str::ReplaceWithCopy(&pendingFindTerm, term);
    pendingFindMatchCase = matchCase;
    pendingFindWholeWord = wholeWord;
    pendingFindIdx = idx;
    pendingFindGen = gen;
    hasPendingFind = true;
    GoToPage(pageNo, false);
}

void BrowserDocController::FindClear() {
    BrowserViewFindClear(docView);
}

void BrowserDocController::SelectAll() const {
    BrowserViewSelectAll(docView);
}

void BrowserDocController::CopySelection() const {
    BrowserViewCopySelection(docView);
}

LRESULT BrowserDocController::PassUIMsg(UINT msg, WPARAM wp, LPARAM lp) const {
    if (!docView || sendingBrowserMsg) {
        return 0;
    }
    sendingBrowserMsg = true;
    LRESULT res = BrowserViewPassUIMsg(docView, msg, wp, lp);
    sendingBrowserMsg = false;
    return res;
}

void BrowserDocController::FinishPendingFind() {
    if (!hasPendingFind || !docView) {
        return;
    }
    BrowserViewFindStart(docView, pendingFindTerm, pendingFindMatchCase, pendingFindWholeWord, pendingFindGen,
                         pendingFindIdx);
    hasPendingFind = false;
    str::FreePtr(&pendingFindTerm);
}

void BrowserDocController::OnFindResult(int gen, int current, int total) {
    cb->FindResultReceived(gen, current, total);
}

void BrowserDocController::OnFindAllResult(Str payload) {
    cb->FindAllResultReceived(payload);
}
