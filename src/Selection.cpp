/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "StrUtil.h"
#include "WinUtil.h"

#include "Selection.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "Toolbar.h"
#include "translations.h"
#include "Notifications.h"
#ifdef BUILD_RIBBON
#include "Ribbon.h"
#endif

#define COL_SELECTION_RECT      RGB(0xF5, 0xFC, 0x0C)

RectI SelectionOnPage::GetRect(DisplayModel *dm)
{
    // if the page is not visible, we return an empty rectangle
    PageInfo *pageInfo = dm->GetPageInfo(pageNo);
    if (!pageInfo || pageInfo->visibleRatio <= 0.0)
        return RectI();

    return dm->CvtToScreen(pageNo, rect);
}

Vec<SelectionOnPage> *SelectionOnPage::FromRectangle(DisplayModel *dm, RectI rect)
{
    Vec<SelectionOnPage> *sel = new Vec<SelectionOnPage>();

    for (int pageNo = dm->PageCount(); pageNo >= 1; --pageNo) {
        PageInfo *pageInfo = dm->GetPageInfo(pageNo);
        assert(!pageInfo || 0.0 == pageInfo->visibleRatio || pageInfo->shown);
        if (!pageInfo || !pageInfo->shown)
            continue;

        RectI intersect = rect.Intersect(pageInfo->pageOnScreen);
        if (intersect.IsEmpty())
            continue;

        /* selection intersects with a page <pageNo> on the screen */
        RectD isectD = dm->CvtFromScreen(intersect, pageNo);
        sel->Append(SelectionOnPage(pageNo, &isectD));
    }
    sel->Reverse();

    return sel;
}

Vec<SelectionOnPage> *SelectionOnPage::FromTextSelect(TextSel *textSel)
{
    Vec<SelectionOnPage> *sel = new Vec<SelectionOnPage>(textSel->len);

    for (int i = textSel->len - 1; i >= 0; i--)
        sel->Append(SelectionOnPage(textSel->pages[i], &textSel->rects[i].Convert<double>()));
    sel->Reverse();

    return sel;
}

void DeleteOldSelectionInfo(WindowInfo *win, bool alsoTextSel)
{
    delete win->selectionOnPage;
    win->selectionOnPage = NULL;
    win->showSelection = false;

    if (alsoTextSel && win->IsDocLoaded())
        win->dm->textSelection->Reset();
}

void PaintTransparentRectangles(HDC hdc, RectI screenRc, Vec<RectI>& rects, COLORREF selectionColor, BYTE alpha, int margin)
{
    using namespace Gdiplus;

    // create path from rectangles
    GraphicsPath path(FillModeWinding);
    screenRc.Inflate(margin, margin);
    for (size_t i = 0; i < rects.Count(); i++) {
        RectI rc = rects.At(i).Intersect(screenRc);
        if (!rc.IsEmpty())
            path.AddRectangle(Gdiplus::Rect(rc.x, rc.y, rc.dx, rc.dy));
    }

    // fill path (and draw optional outline margin)
    Graphics gs(hdc);
    Color c(alpha, GetRValue(selectionColor), GetGValue(selectionColor), GetBValue(selectionColor));
    gs.FillPath(&SolidBrush(c), &path);
    if (margin) {
        path.Outline(NULL, 0.2f);
        gs.DrawPath(&Pen(Color(alpha, 0, 0, 0), (REAL)margin), &path);
    }
}

void PaintSelection(WindowInfo *win, HDC hdc)
{
    Vec<RectI> rects;

    if (win->mouseAction == MA_SELECTING) {
        // during selecting
        RectI selRect = win->selectionRect;
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
        if (MA_SELECTING_TEXT == win->mouseAction)
            UpdateTextSelection(win);

        assert(win->selectionOnPage);
        if (!win->selectionOnPage)
            return;

        // after selection is done
        for (size_t i = 0; i < win->selectionOnPage->Count(); i++)
            rects.Append(win->selectionOnPage->At(i).GetRect(win->dm));
    }

    PaintTransparentRectangles(hdc, win->canvasRc, rects, COL_SELECTION_RECT);
}

void UpdateTextSelection(WindowInfo *win, bool select)
{
    assert(win->IsDocLoaded());
    if (!win->IsDocLoaded()) return;

    if (select) {
        int pageNo = win->dm->GetPageNoByPoint(win->selectionRect.BR());
        if (win->dm->ValidPageNo(pageNo)) {
            PointD pt = win->dm->CvtFromScreen(win->selectionRect.BR(), pageNo);
            win->dm->textSelection->SelectUpTo(pageNo, pt.x, pt.y);
        }
    }

    DeleteOldSelectionInfo(win);
    win->selectionOnPage = SelectionOnPage::FromTextSelect(&win->dm->textSelection->result);
    win->showSelection = true;
}

void ZoomToSelection(WindowInfo *win, float factor, bool relative)
{
    if (!win->IsDocLoaded())
        return;

    PointI pt;
    bool zoomToPt = win->showSelection && win->selectionOnPage;

    // either scroll towards the center of the current selection
    if (zoomToPt) {
        RectI selRect;
        for (size_t i = 0; i < win->selectionOnPage->Count(); i++) {
            selRect = selRect.Union(win->selectionOnPage->At(i).GetRect(win->dm));
        }

        ClientRect rc(win->hwndCanvas);
        pt.x = 2 * selRect.x + selRect.dx - rc.dx / 2;
        pt.y = 2 * selRect.y + selRect.dy - rc.dy / 2;

        pt.x = limitValue(pt.x, selRect.x, selRect.x + selRect.dx);
        pt.y = limitValue(pt.y, selRect.y, selRect.y + selRect.dy);

        int pageNo = win->dm->GetPageNoByPoint(pt);
        if (!win->dm->ValidPageNo(pageNo) || !win->dm->PageVisible(pageNo))
            zoomToPt = false;
    }
    // or towards the top-left-most part of the first visible page
    else {
        int page = win->dm->FirstVisiblePageNo();
        PageInfo *pageInfo = win->dm->GetPageInfo(page);
        if (pageInfo) {
            RectI visible = pageInfo->pageOnScreen.Intersect(win->canvasRc);
            pt = visible.TL();

            int pageNo = win->dm->GetPageNoByPoint(pt);
            if (!visible.IsEmpty() && win->dm->ValidPageNo(pageNo) && win->dm->PageVisible(pageNo))
                zoomToPt = true;
        }
    }

    if (relative)
        win->dm->ZoomBy(factor, zoomToPt ? &pt : NULL);
    else
        win->dm->ZoomTo(factor, zoomToPt ? &pt : NULL);

    UpdateToolbarState(win);
#ifdef BUILD_RIBBON
    if (win->ribbonSupport)
        win->ribbonSupport->UpdateState();
#endif
}

void CopySelectionToClipboard(WindowInfo *win)
{
    if (!win->selectionOnPage) return;
    if (!win->dm->engine) return;

    if (!OpenClipboard(NULL)) return;
    EmptyClipboard();

    if (!win->dm->engine->IsCopyingTextAllowed())
        ShowNotification(win, _TR("Copying text was denied (copying as image only)"));
    else if (!win->dm->engine->IsImageCollection()) {
        ScopedMem<TCHAR> selText;
        bool isTextSelection = win->dm->textSelection->result.len > 0;
        if (isTextSelection) {
            selText.Set(win->dm->textSelection->ExtractText(_T("\r\n")));
        }
        else {
            StrVec selections;
            for (size_t i = 0; i < win->selectionOnPage->Count(); i++) {
                SelectionOnPage *selOnPage = &win->selectionOnPage->At(i);
                TCHAR *text = win->dm->GetTextInRegion(selOnPage->pageNo, selOnPage->rect);
                if (text)
                    selections.Push(text);
            }
            selText.Set(selections.Join());
        }

        // don't copy empty text
        if (!str::IsEmpty(selText.Get()))
            CopyTextToClipboard(selText, true);

        if (isTextSelection) {
            // don't also copy the first line of a text selection as an image
            CloseClipboard();
            return;
        }
    }

    /* also copy a screenshot of the current selection to the clipboard */
    SelectionOnPage *selOnPage = &win->selectionOnPage->At(0);
    RenderedBitmap * bmp = win->dm->engine->RenderBitmap(selOnPage->pageNo,
        win->dm->ZoomReal(), win->dm->Rotation(), &selOnPage->rect, Target_Export);
    if (bmp) {
        if (!SetClipboardData(CF_BITMAP, bmp->GetBitmap()))
            SeeLastError();
        delete bmp;
    }

    CloseClipboard();
}

void OnSelectAll(WindowInfo *win, bool textOnly)
{
    if (!HasPermission(Perm_CopySelection))
        return;
    if (!win->IsDocLoaded())
        return;

    if (win->hwndFindBox == GetFocus() || win->hwndPageBox == GetFocus()) {
        Edit_SelectAll(GetFocus());
        return;
    }
    if (win->IsChm()) {
        win->dm->AsChmEngine()->SelectAll();
        return;
    }

    if (textOnly) {
        int pageNo;
        for (pageNo = 1; !win->dm->GetPageInfo(pageNo)->shown; pageNo++);
        win->dm->textSelection->StartAt(pageNo, 0);
        for (pageNo = win->dm->PageCount(); !win->dm->GetPageInfo(pageNo)->shown; pageNo--);
        win->dm->textSelection->SelectUpTo(pageNo, -1);
        win->selectionRect = RectI::FromXY(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
        UpdateTextSelection(win);
    }
    else {
        DeleteOldSelectionInfo(win, true);
        win->selectionRect = RectI::FromXY(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
        win->selectionOnPage = SelectionOnPage::FromRectangle(win->dm, win->selectionRect);
    }

    win->showSelection = true;
    win->RepaintAsync();
}

#define SELECT_AUTOSCROLL_AREA_WIDTH 15
#define SELECT_AUTOSCROLL_STEP_LENGTH 10

void OnSelectionEdgeAutoscroll(WindowInfo *win, int x, int y)
{
    int dx = 0, dy = 0;

    if (x < SELECT_AUTOSCROLL_AREA_WIDTH * win->uiDPIFactor)
        dx = -SELECT_AUTOSCROLL_STEP_LENGTH;
    else if (x > (win->canvasRc.dx - SELECT_AUTOSCROLL_AREA_WIDTH) * win->uiDPIFactor)
        dx = SELECT_AUTOSCROLL_STEP_LENGTH;
    if (y < SELECT_AUTOSCROLL_AREA_WIDTH * win->uiDPIFactor)
        dy = -SELECT_AUTOSCROLL_STEP_LENGTH;
    else if (y > (win->canvasRc.dy - SELECT_AUTOSCROLL_AREA_WIDTH) * win->uiDPIFactor)
        dy = SELECT_AUTOSCROLL_STEP_LENGTH;

    if (dx != 0 || dy != 0) {
        PointI oldOffset = win->dm->viewPort.TL();
        win->MoveDocBy(dx, dy);

        dx = win->dm->viewPort.x - oldOffset.x;
        dy = win->dm->viewPort.y - oldOffset.y;
        win->selectionRect.x -= dx;
        win->selectionRect.y -= dy;
        win->selectionRect.dx += dx;
        win->selectionRect.dy += dy;
    }
}

void OnSelectionStart(WindowInfo *win, int x, int y, WPARAM key)
{
    DeleteOldSelectionInfo(win, true);

    win->selectionRect = RectI(x, y, 0, 0);
    win->showSelection = true;
    win->mouseAction = MA_SELECTING;

    // Ctrl+drag forces a rectangular selection
    if (!(key & MK_CONTROL) || (key & MK_SHIFT)) {
        int pageNo = win->dm->GetPageNoByPoint(PointI(x, y));
        if (win->dm->ValidPageNo(pageNo)) {
            PointD pt = win->dm->CvtFromScreen(PointI(x, y), pageNo);
            win->dm->textSelection->StartAt(pageNo, pt.x, pt.y);
            win->mouseAction = MA_SELECTING_TEXT;
        }
    }

    SetCapture(win->hwndCanvas);
    SetTimer(win->hwndCanvas, SMOOTHSCROLL_TIMER_ID, SMOOTHSCROLL_DELAY_IN_MS, NULL);

    win->RepaintAsync();
}

void OnSelectionStop(WindowInfo *win, int x, int y, bool aborted)
{
    if (GetCapture() == win->hwndCanvas)
        ReleaseCapture();
    KillTimer(win->hwndCanvas, SMOOTHSCROLL_TIMER_ID);

    // update the text selection before changing the selectionRect
    if (MA_SELECTING_TEXT == win->mouseAction)
        UpdateTextSelection(win);

    win->selectionRect = RectI::FromXY(win->selectionRect.x, win->selectionRect.y, x, y);
    if (aborted || (MA_SELECTING == win->mouseAction ? win->selectionRect.IsEmpty() : !win->selectionOnPage))
        DeleteOldSelectionInfo(win, true);
    else if (win->mouseAction == MA_SELECTING)
        win->selectionOnPage = SelectionOnPage::FromRectangle(win->dm, win->selectionRect);
    win->RepaintAsync();
}
