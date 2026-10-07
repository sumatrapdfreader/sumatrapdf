/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: ChmModel / MarkdownModel live in the `app` library, which the console
// tools link without gpui. They get this instead of gui/BrowserView.cpp, the
// same way the app picks between CrashHandler.cpp and CrashHandlerNoOp.cpp.

#include "base/Base.h"

#include "gui/BrowserView.h"

bool BrowserViewAvailable() {
    return false;
}

BrowserView* BrowserViewCreate(MainWindow*, HWND, BrowserViewCallback*, Str) {
    return nullptr;
}

void BrowserViewDelete(BrowserView*) {}

void BrowserViewSetWindow(BrowserView*, MainWindow*) {}

MainWindow* BrowserViewWindow(BrowserView*) {
    return nullptr;
}

void BrowserViewSetVisible(BrowserView*, bool) {}

bool BrowserViewIsVisible(BrowserView*) {
    return false;
}

void BrowserViewRefreshSurface(BrowserView*) {}

void BrowserViewNavigate(BrowserView*, Str) {}

void BrowserViewGoBack(BrowserView*) {}

void BrowserViewGoForward(BrowserView*) {}

bool BrowserViewCanGoBack(BrowserView*) {
    return false;
}

bool BrowserViewCanGoForward(BrowserView*) {
    return false;
}

void BrowserViewSetZoomPercent(BrowserView*, int) {}

int BrowserViewGetZoomPercent(BrowserView*) {
    return 100;
}

Point BrowserViewGetScrollPos(BrowserView*) {
    return Point(-1, -1);
}

void BrowserViewSetScrollPos(BrowserView*, Point) {}

void BrowserViewEval(BrowserView*, Str) {}

void BrowserViewSelectAll(BrowserView*) {}

void BrowserViewCopySelection(BrowserView*) {}

void BrowserViewPrint(BrowserView*, bool) {}

void BrowserViewFindInPageUI(BrowserView*) {}

bool BrowserViewCanFindInPage(BrowserView*) {
    return false;
}

void BrowserViewFindStart(BrowserView*, Str, bool, bool, int, int) {}

void BrowserViewFindAllPages(BrowserView*, const StrVec&, Str, bool, bool, int) {}

void BrowserViewFindGoto(BrowserView*, int) {}

void BrowserViewFindClear(BrowserView*) {}

LRESULT BrowserViewPassUIMsg(BrowserView*, UINT, WPARAM, LPARAM) {
    return 0;
}

gpui::El* BrowserViewBuild(BrowserView*, gpui::Ctx*) {
    return nullptr;
}

void BrowserViewCreatePending(MainWindow*, gpui::Ctx*) {}
void BrowserViewSetOwnHost(BrowserView*) {}
void BrowserViewCreatePendingIn(BrowserView*, gpui::Ctx*) {}
