/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "Annotation.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Selection.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "Notifications.h"
#include "SumatraDialogs.h"
#include "SignDocumentDialogCommon.h"

// Default size of a new signature when the user clicks rather than dragging
// a rectangle. 2" x 0.75" at 72 pt/in — enough for name, date and reason.
constexpr float kDefaultSignatureDx = 144;

constexpr float kDefaultSignatureDy = 54;

EngineBase* GetPdfEngine(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->IsDocLoaded()) {
        return nullptr;
    }
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine || !EngineMupdfSupportsAnnotations(engine)) {
        return nullptr;
    }
    return engine;
}

// Bounding box of the current selection. Sets pageNoOut to the page of the
// first non-empty piece. Empty if nothing is selected.
RectF SelectionRect(WindowTab* tab, int* pageNoOut) {
    RectF res;
    if (!tab || !tab->selectionOnPage) {
        return res;
    }
    for (auto& sel : *tab->selectionOnPage) {
        if (sel.rect.IsEmpty()) {
            continue;
        }
        if (res.IsEmpty()) {
            if (pageNoOut) {
                *pageNoOut = sel.pageNo;
            }
            res = sel.rect;
        } else if (!pageNoOut || sel.pageNo == *pageNoOut) {
            res = res.Union(sel.rect);
        }
    }
    return res;
}

static RectF ClampRectToPage(RectF r, RectF page) {
    if (page.IsEmpty()) {
        return r;
    }
    if (r.dx > page.dx) {
        r.dx = page.dx;
    }
    if (r.dy > page.dy) {
        r.dy = page.dy;
    }
    if (r.x < page.x) {
        r.x = page.x;
    }
    if (r.y < page.y) {
        r.y = page.y;
    }
    if (r.x + r.dx > page.x + page.dx) {
        r.x = page.x + page.dx - r.dx;
    }
    if (r.y + r.dy > page.y + page.dy) {
        r.y = page.y + page.dy - r.dy;
    }
    return r;
}

// A default-size box centered on the click, kept on the page.
RectF DefaultSignatureRectAt(DisplayModel* dm, int pageNo, PointF pt) {
    RectF r(pt.x - (kDefaultSignatureDx / 2), pt.y - (kDefaultSignatureDy / 2), kDefaultSignatureDx,
            kDefaultSignatureDy);
    PageInfo* pi = dm ? dm->GetPageInfo(pageNo) : nullptr;
    if (!pi || !IsMediaBoxKnown(pi->mediaBox)) {
        return r;
    }
    return ClampRectToPage(r, pi->mediaBox);
}
