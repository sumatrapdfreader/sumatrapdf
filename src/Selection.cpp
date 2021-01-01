/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>
#include "utils/ScopedWin.h"
#include "utils/Dpi.h"
#include "utils/WinUtil.h"

#include "wingui/TreeModel.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EngineCreate.h"
#include "DisplayMode.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "GlobalPrefs.h"
#include "ChmModel.h"
#include "DisplayModel.h"
#include "TextSelection.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "Selection.h"
#include "Toolbar.h"
#include "Translations.h"
#include "uia/Provider.h"

SelectionOnPage::SelectionOnPage(int pageNo, RectF* rect) {
    this->pageNo = pageNo;
    if (rect) {
        this->rect = *rect;
    } else {
        this->rect = RectF();
    }
}

Rect SelectionOnPage::GetRect(DisplayModel* dm) {
    // if the page is not visible, we return an empty rectangle
    PageInfo* pageInfo = dm->GetPageInfo(pageNo);
    if (!pageInfo || pageInfo->visibleRatio <= 0.0) {
        return Rect();
    }

    return dm->CvtToScreen(pageNo, rect);
}

Vec<SelectionOnPage>* SelectionOnPage::FromRectangle(DisplayModel* dm, Rect rect) {
    Vec<SelectionOnPage>* sel = new Vec<SelectionOnPage>();

    for (int pageNo = dm->GetEngine()->PageCount(); pageNo >= 1; --pageNo) {
        PageInfo* pageInfo = dm->GetPageInfo(pageNo);
        CrashIf(!(!pageInfo || 0.0 == pageInfo->visibleRatio || pageInfo->shown));
        if (!pageInfo || !pageInfo->shown) {
            continue;
        }

        Rect intersect = rect.Intersect(pageInfo->pageOnScreen);
        if (intersect.IsEmpty()) {
            continue;
        }

        /* selection intersects with a page <pageNo> on the screen */
        RectF isectD = dm->CvtFromScreen(intersect, pageNo);
        sel->Append(SelectionOnPage(pageNo, &isectD));
    }
    sel->Reverse();

    if (sel->size() == 0) {
        delete sel;
        return nullptr;
    }
    return sel;
}

Vec<SelectionOnPage>* SelectionOnPage::FromTextSelect(TextSel* textSel) {
    Vec<SelectionOnPage>* sel = new Vec<SelectionOnPage>(textSel->len);

    for (int i = textSel->len - 1; i >= 0; i--) {
        RectF rect = ToRectFl(textSel->rects[i]);
        sel->Append(SelectionOnPage(textSel->pages[i], &rect));
    }
    sel->Reverse();

    if (sel->size() == 0) {
        delete sel;
        return nullptr;
    }
    return sel;
}

void DeleteOldSelectionInfo(WindowInfo* win, bool alsoTextSel) {
    win->showSelection = false;
    win->selectionMeasure = SizeF();
    TabInfo* tab = win->currentTab;
    if (!tab) {
        return;
    }

    delete tab->selectionOnPage;
    tab->selectionOnPage = nullptr;
    if (alsoTextSel && tab->AsFixed()) {
        tab->AsFixed()->textSelection->Reset();
    }
}

void PaintTransparentRectangles(HDC hdc, Rect screenRc, Vec<Rect>& rects, COLORREF selectionColor, u8 alpha,
                                int margin) {
    // create path from rectangles
    Gdiplus::GraphicsPath path(Gdiplus::FillModeWinding);
    screenRc.Inflate(margin, margin);
    for (size_t i = 0; i < rects.size(); i++) {
        Rect rc = rects.at(i).Intersect(screenRc);
        if (!rc.IsEmpty()) {
            path.AddRectangle(ToGdipRect(rc));
        }
    }

    // fill path (and draw optional outline margin)
    Gdiplus::Graphics gs(hdc);
    u8 r, g, b;
    UnpackRgb(selectionColor, r, g, b);
    Gdiplus::Color c(alpha, r, g, b);
    Gdiplus::SolidBrush tmpBrush(c);
    gs.FillPath(&tmpBrush, &path);
    if (margin) {
        path.Outline(nullptr, 0.2f);
        Gdiplus::Pen tmpPen(Gdiplus::Color(alpha, 0, 0, 0), (float)margin);
        gs.DrawPath(&tmpPen, &path);
    }
}

void PaintSelection(WindowInfo* win, HDC hdc) {
    CrashIf(!win->AsFixed());

    Vec<Rect> rects;

    if (win->mouseAction == MouseAction::Selecting) {
        // during rectangle selection
        Rect selRect = win->selectionRect;
        if (selRect.dx < 0) {
            selRect.x += selRect.dx;
            selRect.dx *= -1;
        }
        if (selRect.dy < 0) {
            selRect.y += selRect.dy;
            selRect.dy *= -1;
        }

        rects.Append(selRect);
    } else {
        // during text selection or after selection is done
        if (MouseAction::SelectingText == win->mouseAction) {
            UpdateTextSelection(win);
            if (!win->currentTab->selectionOnPage) {
                // prevent the selection from disappearing while the
                // user is still at it (OnSelectionStop removes it
                // if it is still empty at the end)
                win->currentTab->selectionOnPage = new Vec<SelectionOnPage>();
                win->showSelection = true;
            }
        }

        CrashIf(!win->currentTab->selectionOnPage);
        if (!win->currentTab->selectionOnPage) {
            return;
        }

        for (SelectionOnPage& sel : *win->currentTab->selectionOnPage) {
            rects.Append(sel.GetRect(win->AsFixed()));
        }
    }

    PaintTransparentRectangles(hdc, win->canvasRc, rects, gGlobalPrefs->fixedPageUI.selectionColor);
}

void UpdateTextSelection(WindowInfo* win, bool select) {
    if (!win->AsFixed()) {
        return;
    }

    DisplayModel* dm = win->AsFixed();
    if (select) {
        int pageNo = dm->GetPageNoByPoint(win->selectionRect.BR());
        if (win->ctrl->ValidPageNo(pageNo)) {
            PointF pt = dm->CvtFromScreen(win->selectionRect.BR(), pageNo);
            dm->textSelection->SelectUpTo(pageNo, pt.x, pt.y);
        }
    }

    DeleteOldSelectionInfo(win);
    win->currentTab->selectionOnPage = SelectionOnPage::FromTextSelect(&dm->textSelection->result);
    win->showSelection = win->currentTab->selectionOnPage != nullptr;

    if (win->uiaProvider) {
        win->uiaProvider->OnSelectionChanged();
    }
}

void ZoomToSelection(WindowInfo* win, float factor, bool scrollToFit, bool relative) {
    if (!win->IsDocLoaded()) {
        return;
    }

    Point pt;
    bool zoomToPt = false;

    if (win->AsFixed()) {
        DisplayModel* dm = win->AsFixed();
        // when not zooming to fit (which contradicts zooming to a specific point), ...
        if (!relative && (ZOOM_FIT_PAGE == factor || ZOOM_FIT_CONTENT == factor) && scrollToFit) {
            zoomToPt = false;
        }
        // either scroll towards the center of the current selection (if there is any) ...
        else if (win->showSelection && win->currentTab->selectionOnPage) {
            Rect selRect;
            for (SelectionOnPage& sel : *win->currentTab->selectionOnPage) {
                selRect = selRect.Union(sel.GetRect(dm));
            }

            Rect rc = ClientRect(win->hwndCanvas);
            pt.x = 2 * selRect.x + selRect.dx - rc.dx / 2;
            pt.y = 2 * selRect.y + selRect.dy - rc.dy / 2;
            pt.x = limitValue(pt.x, selRect.x, selRect.x + selRect.dx);
            pt.y = limitValue(pt.y, selRect.y, selRect.y + selRect.dy);

            int pageNo = dm->GetPageNoByPoint(pt);
            zoomToPt = dm->ValidPageNo(pageNo) && dm->PageVisible(pageNo);
        }
        // or towards the top-left-most part of the first visible page
        else {
            int page = dm->FirstVisiblePageNo();
            PageInfo* pageInfo = dm->GetPageInfo(page);
            if (pageInfo) {
                Rect visible = pageInfo->pageOnScreen.Intersect(win->canvasRc);
                pt = visible.TL();

                int pageNo = dm->GetPageNoByPoint(pt);
                zoomToPt = !visible.IsEmpty() && dm->ValidPageNo(pageNo) && dm->PageVisible(pageNo);
            }
        }
    }

    win->ctrl->SetZoomVirtual(factor * (relative ? win->ctrl->GetZoomVirtual(true) : 1), zoomToPt ? &pt : nullptr);
    UpdateToolbarState(win);
}

// isTextSelectionOut is set to true if this is text-only selection (as opposed to
// rectangular selection)
// caller needs to str::Free() the result
WCHAR* GetSelectedText(WindowInfo* win, const WCHAR* lineSep, bool& isTextOnlySelectionOut) {
    if (!win->currentTab || !win->currentTab->selectionOnPage) {
        return nullptr;
    }
    if (win->currentTab->selectionOnPage->size() == 0) {
        return nullptr;
    }
    DisplayModel* dm = win->AsFixed();
    CrashIf(!dm);
    if (!dm) {
        return nullptr;
    }
    if (dm->GetEngine()->IsImageCollection()) {
        return nullptr;
    }

    isTextOnlySelectionOut = dm->textSelection->result.len > 0;
    if (isTextOnlySelectionOut) {
        WCHAR* s = dm->textSelection->ExtractText(lineSep);
        return s;
    }
    WStrVec selections;
    for (SelectionOnPage& sel : *win->currentTab->selectionOnPage) {
        WCHAR* text = dm->GetTextInRegion(sel.pageNo, sel.rect);
        if (!str::IsEmpty(text)) {
            selections.Append(text);
        }
    }
    if (selections.size() == 0) {
        return nullptr;
    }
    WCHAR* s = selections.Join(lineSep);
    return s;
}

void CopySelectionToClipboard(WindowInfo* win) {
    CrashIf(win->currentTab->selectionOnPage->size() == 0 && win->mouseAction != MouseAction::SelectingText);

    if (!OpenClipboard(nullptr)) {
        return;
    }
    EmptyClipboard();
    defer {
        CloseClipboard();
    };

    WCHAR* selText = nullptr;
    bool isTextOnlySelectionOut = false;
    if (!gDisableDocumentRestrictions && !win->AsFixed()->GetEngine()->AllowsCopyingText()) {
        win->ShowNotification(_TR("Copying text was denied (copying as image only)"));
    } else {
        selText = GetSelectedText(win, L"\r\n", isTextOnlySelectionOut);
    }

    // don't copy empty text
    if (!str::IsEmpty(selText)) {
        CopyTextToClipboard(selText, true);
    }
    if (isTextOnlySelectionOut) {
        // don't also copy the first line of a text selection as an image
        return;
    }

    DisplayModel* dm = win->AsFixed();
    if (!dm || !win->currentTab->selectionOnPage || win->currentTab->selectionOnPage->size() == 0) {
        return;
    }
    /* also copy a screenshot of the current selection to the clipboard */
    SelectionOnPage* selOnPage = &win->currentTab->selectionOnPage->at(0);
    float zoom = dm->GetZoomReal(selOnPage->pageNo);
    int rotation = dm->GetRotation();
    RenderPageArgs args(selOnPage->pageNo, zoom, rotation, &selOnPage->rect, RenderTarget::Export);
    RenderedBitmap* bmp = dm->GetEngine()->RenderPage(args);
    if (bmp) {
        CopyImageToClipboard(bmp->GetBitmap(), true);
    }
    delete bmp;
}

void OnSelectAll(WindowInfo* win, bool textOnly) {
    if (!HasPermission(Perm_CopySelection)) {
        return;
    }

    if (IsFocused(win->hwndFindBox) || IsFocused(win->hwndPageBox)) {
        Edit_SelectAll(GetFocus());
        return;
    }

    if (win->AsChm()) {
        win->AsChm()->SelectAll();
        return;
    }
    if (!win->AsFixed()) {
        return;
    }

    DisplayModel* dm = win->AsFixed();
    if (textOnly) {
        int pageNo;
        for (pageNo = 1; !dm->GetPageInfo(pageNo)->shown; pageNo++) {
            ;
        }
        dm->textSelection->StartAt(pageNo, 0);
        for (pageNo = win->ctrl->PageCount(); !dm->GetPageInfo(pageNo)->shown; pageNo--) {
            ;
        }
        dm->textSelection->SelectUpTo(pageNo, -1);
        win->selectionRect = Rect::FromXY(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
        UpdateTextSelection(win);
    } else {
        DeleteOldSelectionInfo(win, true);
        win->selectionRect = Rect::FromXY(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
        win->currentTab->selectionOnPage = SelectionOnPage::FromRectangle(dm, win->selectionRect);
    }

    win->showSelection = win->currentTab->selectionOnPage != nullptr;
    RepaintAsync(win, 0);
}

#define SELECT_AUTOSCROLL_AREA_WIDTH DpiScale(win->hwndFrame, 15)
#define SELECT_AUTOSCROLL_STEP_LENGTH DpiScale(win->hwndFrame, 10)

bool NeedsSelectionEdgeAutoscroll(WindowInfo* win, int x, int y) {
    return x < SELECT_AUTOSCROLL_AREA_WIDTH || x > win->canvasRc.dx - SELECT_AUTOSCROLL_AREA_WIDTH ||
           y < SELECT_AUTOSCROLL_AREA_WIDTH || y > win->canvasRc.dy - SELECT_AUTOSCROLL_AREA_WIDTH;
}

void OnSelectionEdgeAutoscroll(WindowInfo* win, int x, int y) {
    int dx = 0, dy = 0;

    if (x < SELECT_AUTOSCROLL_AREA_WIDTH) {
        dx = -SELECT_AUTOSCROLL_STEP_LENGTH;
    } else if (x > win->canvasRc.dx - SELECT_AUTOSCROLL_AREA_WIDTH) {
        dx = SELECT_AUTOSCROLL_STEP_LENGTH;
    }
    if (y < SELECT_AUTOSCROLL_AREA_WIDTH) {
        dy = -SELECT_AUTOSCROLL_STEP_LENGTH;
    } else if (y > win->canvasRc.dy - SELECT_AUTOSCROLL_AREA_WIDTH) {
        dy = SELECT_AUTOSCROLL_STEP_LENGTH;
    }

    CrashIf(NeedsSelectionEdgeAutoscroll(win, x, y) != (dx != 0 || dy != 0));
    if (dx != 0 || dy != 0) {
        CrashIf(!win->AsFixed());
        DisplayModel* dm = win->AsFixed();
        Point oldOffset = dm->GetViewPort().TL();
        win->MoveDocBy(dx, dy);

        dx = dm->GetViewPort().x - oldOffset.x;
        dy = dm->GetViewPort().y - oldOffset.y;
        win->selectionRect.x -= dx;
        win->selectionRect.y -= dy;
        win->selectionRect.dx += dx;
        win->selectionRect.dy += dy;
    }
}

void OnSelectionStart(WindowInfo* win, int x, int y, [[maybe_unused]] WPARAM key) {
    CrashIf(!win->AsFixed());
    DeleteOldSelectionInfo(win, true);

    win->selectionRect = Rect(x, y, 0, 0);
    win->showSelection = true;
    win->mouseAction = MouseAction::Selecting;

    bool isShift = IsShiftPressed();
    bool isCtrl = IsCtrlPressed();

    // Ctrl+drag forces a rectangular selection
    if (!isCtrl || isShift) {
        DisplayModel* dm = win->AsFixed();
        int pageNo = dm->GetPageNoByPoint(Point(x, y));
        if (dm->ValidPageNo(pageNo)) {
            PointF pt = dm->CvtFromScreen(Point(x, y), pageNo);
            dm->textSelection->StartAt(pageNo, pt.x, pt.y);
            win->mouseAction = MouseAction::SelectingText;
        }
    }

    SetCapture(win->hwndCanvas);
    SetTimer(win->hwndCanvas, SMOOTHSCROLL_TIMER_ID, SMOOTHSCROLL_DELAY_IN_MS, nullptr);
    RepaintAsync(win, 0);
}

void OnSelectionStop(WindowInfo* win, int x, int y, bool aborted) {
    if (GetCapture() == win->hwndCanvas) {
        ReleaseCapture();
    }
    KillTimer(win->hwndCanvas, SMOOTHSCROLL_TIMER_ID);

    // update the text selection before changing the selectionRect
    if (MouseAction::SelectingText == win->mouseAction) {
        UpdateTextSelection(win);
    }

    win->selectionRect = Rect::FromXY(win->selectionRect.x, win->selectionRect.y, x, y);
    if (aborted || (MouseAction::Selecting == win->mouseAction ? win->selectionRect.IsEmpty()
                                                               : !win->currentTab->selectionOnPage)) {
        DeleteOldSelectionInfo(win, true);
    } else if (win->mouseAction == MouseAction::Selecting) {
        win->currentTab->selectionOnPage = SelectionOnPage::FromRectangle(win->AsFixed(), win->selectionRect);
        win->showSelection = win->currentTab->selectionOnPage != nullptr;
    }
    RepaintAsync(win, 0);
}
