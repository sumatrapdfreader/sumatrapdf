/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include <uiautomationcore.h>
#include "gui/Dpi.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/Gfx.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "GlobalPrefs.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "DisplayModel.h"
#include "TextSelection.h"
#include "Notifications.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Canvas.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Selection.h"
#include "SelectionToolbar.h"
#include "SelectTextKeyboard.h"
#include "Toolbar.h"
#include "Translations.h"
#include "uia/Provider.h"

SelectionOnPage::SelectionOnPage(int pageNo, const RectF* const rect) {
    this->pageNo = pageNo;
    if (rect) {
        this->rect = *rect;
    } else {
        this->rect = RectF();
    }
}

Rect SelectionOnPage::GetRect(DisplayModel* dm) const {
    // if the page is not visible, we return an empty rectangle
    PageInfo* pageInfo = dm->GetPageInfo(pageNo);
    if (!pageInfo || pageInfo->visibleRatio <= 0.0) {
        return {};
    }

    return dm->CvtToScreen(pageNo, rect);
}

Vec<SelectionOnPage>* SelectionOnPage::FromRectangle(DisplayModel* dm, Rect rect) {
    Vec<SelectionOnPage>* sel = new Vec<SelectionOnPage>();

    for (int pageNo = dm->GetEngine()->PageCount(); pageNo >= 1; --pageNo) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        ReportIf(!(!pi || 0.0 == pi->visibleRatio || pi->isShown));
        if (!pi || !pi->isShown) {
            continue;
        }

        Rect intersect = rect.Intersect(pi->pageOnScreen);
        if (intersect.IsEmpty()) {
            continue;
        }

        /* selection intersects with a page <pageNo> on the screen */
        RectF isectD = dm->CvtFromScreen(intersect, pageNo);
        sel->Append(SelectionOnPage(pageNo, &isectD));
    }
    VecReverse(*sel);

    if (len(*sel) == 0) {
        delete sel;
        return nullptr;
    }
    return sel;
}

Vec<SelectionOnPage>* SelectionOnPage::FromTextSelect(TextSel* textSel) {
    Vec<SelectionOnPage>* sel = new Vec<SelectionOnPage>();
    VecReserve(*sel, textSel->len);

    for (int i = textSel->len - 1; i >= 0; i--) {
        RectF rect = ToRectF(textSel->rects[i]);
        sel->Append(SelectionOnPage(textSel->pages[i], &rect));
    }
    VecReverse(*sel);

    if (len(*sel) == 0) {
        delete sel;
        return nullptr;
    }
    return sel;
}

void DeleteOldSelectionInfo(MainWindow* win, bool alsoTextSel) {
    HideSelectionToolbar(win);
    win->showSelection = false;
    win->selectionMeasure = SizeF();
    win->selectionDragEdge = SelectionDragEdge::None;
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return;
    }

    delete tab->selectionOnPage;
    tab->selectionOnPage = nullptr;
    if (alsoTextSel && tab->AsFixed()) {
        tab->AsFixed()->textSelection->Reset();
    }
}

// Rectangular (Ctrl+drag) selection: move/resize after it exists.
bool IsRectangularSelection(MainWindow* win) {
    if (!win || !win->showSelection) {
        return false;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->selectionOnPage || len(*tab->selectionOnPage) == 0) {
        return false;
    }
    DisplayModel* dm = tab->AsFixed();
    if (!dm || !dm->textSelection) {
        return false;
    }
    // text selection has glyphs; rectangular (Ctrl+drag) does not
    return dm->textSelection->result.len == 0;
}

Rect GetRectangularSelectionScreenRect(MainWindow* win) {
    Rect bounds;
    if (!IsRectangularSelection(win)) {
        return bounds;
    }
    DisplayModel* dm = win->AsFixed();
    bool first = true;
    for (SelectionOnPage& sel : *win->CurrentTab()->selectionOnPage) {
        Rect r = sel.GetRect(dm);
        if (r.IsEmpty()) {
            continue;
        }
        if (first) {
            bounds = r;
            first = false;
        } else {
            bounds = bounds.Union(r);
        }
    }
    return bounds;
}

static Rect NormalizeScreenRect(Rect r) {
    if (r.dx < 0) {
        r.x += r.dx;
        r.dx = -r.dx;
    }
    if (r.dy < 0) {
        r.y += r.dy;
        r.dy = -r.dy;
    }
    return r;
}

// Apply edge/corner/move drag to an original normalized rect (screen coords).
static Rect ApplySelectionEdgeDrag(Rect orig, SelectionDragEdge edge, int dx, int dy) {
    int x = orig.x;
    int y = orig.y;
    int w = orig.dx;
    int h = orig.dy;

    if (edge == SelectionDragEdge::Move) {
        return {x + dx, y + dy, w, h};
    }

    if (edge == SelectionDragEdge::Left || edge == SelectionDragEdge::TopLeft ||
        edge == SelectionDragEdge::BottomLeft) {
        x = orig.x + dx;
        w = orig.dx - dx;
    }
    if (edge == SelectionDragEdge::Right || edge == SelectionDragEdge::TopRight ||
        edge == SelectionDragEdge::BottomRight) {
        w = orig.dx + dx;
    }
    if (edge == SelectionDragEdge::Top || edge == SelectionDragEdge::TopLeft || edge == SelectionDragEdge::TopRight) {
        y = orig.y + dy;
        h = orig.dy - dy;
    }
    if (edge == SelectionDragEdge::Bottom || edge == SelectionDragEdge::BottomLeft ||
        edge == SelectionDragEdge::BottomRight) {
        h = orig.dy + dy;
    }

    // minimum 1px (same idea as crop dialog)
    if (w < 1) {
        w = 1;
        if (edge == SelectionDragEdge::Left || edge == SelectionDragEdge::TopLeft ||
            edge == SelectionDragEdge::BottomLeft) {
            x = orig.x + orig.dx - 1;
        } else {
            x = orig.x;
        }
    }
    if (h < 1) {
        h = 1;
        if (edge == SelectionDragEdge::Top || edge == SelectionDragEdge::TopLeft ||
            edge == SelectionDragEdge::TopRight) {
            y = orig.y + orig.dy - 1;
        } else {
            y = orig.y;
        }
    }
    return {x, y, w, h};
}

SelectionDragEdge HitTestRectangularSelection(MainWindow* win, int mx, int my) {
    if (!IsRectangularSelection(win)) {
        return SelectionDragEdge::None;
    }
    Rect r = GetRectangularSelectionScreenRect(win);
    if (r.IsEmpty()) {
        return SelectionDragEdge::None;
    }
    int t = DpiScale(6);
    int left = r.x;
    int right = r.x + r.dx;
    int top = r.y;
    int bottom = r.y + r.dy;

    bool onLeft = (mx >= left - t && mx <= left + t);
    bool onRight = (mx >= right - t && mx <= right + t);
    bool onTop = (my >= top - t && my <= top + t);
    bool onBottom = (my >= bottom - t && my <= bottom + t);
    bool inVertRange = (my >= top - t && my <= bottom + t);
    bool inHorzRange = (mx >= left - t && mx <= right + t);

    if (onLeft && onTop) {
        return SelectionDragEdge::TopLeft;
    }
    if (onRight && onTop) {
        return SelectionDragEdge::TopRight;
    }
    if (onLeft && onBottom) {
        return SelectionDragEdge::BottomLeft;
    }
    if (onRight && onBottom) {
        return SelectionDragEdge::BottomRight;
    }
    if (onLeft && inVertRange) {
        return SelectionDragEdge::Left;
    }
    if (onRight && inVertRange) {
        return SelectionDragEdge::Right;
    }
    if (onTop && inHorzRange) {
        return SelectionDragEdge::Top;
    }
    if (onBottom && inHorzRange) {
        return SelectionDragEdge::Bottom;
    }
    if (mx > left + t && mx < right - t && my > top + t && my < bottom - t) {
        return SelectionDragEdge::Move;
    }
    return SelectionDragEdge::None;
}

LPWSTR CursorIdForSelectionEdge(SelectionDragEdge edge) {
    switch (edge) {
        case SelectionDragEdge::Left:
        case SelectionDragEdge::Right:
            return IDC_SIZEWE;
        case SelectionDragEdge::Top:
        case SelectionDragEdge::Bottom:
            return IDC_SIZENS;
        case SelectionDragEdge::TopLeft:
        case SelectionDragEdge::BottomRight:
            return IDC_SIZENWSE;
        case SelectionDragEdge::TopRight:
        case SelectionDragEdge::BottomLeft:
            return IDC_SIZENESW;
        case SelectionDragEdge::Move:
            return IDC_SIZEALL;
        default:
            return IDC_ARROW;
    }
}

bool StartRectangularSelectionEdit(MainWindow* win, int x, int y, SelectionDragEdge edge) {
    if (!win || edge == SelectionDragEdge::None || !IsRectangularSelection(win)) {
        return false;
    }
    Rect bounds = GetRectangularSelectionScreenRect(win);
    if (bounds.IsEmpty()) {
        return false;
    }
    win->selectionDragEdge = edge;
    win->selectionEditOrig = NormalizeScreenRect(bounds);
    win->selectionRect = win->selectionEditOrig;
    win->dragStart = Point(x, y);
    win->dragStartPending = true;
    win->showSelection = true;
    win->selectingByWord = false;
    win->mouseAction = MouseAction::Selecting;
    win->linkOnLastButtonDown = nullptr;
    win->textDragPending = false;
    win->imageDragPending = false;
    SetCapture(win->hwndCanvas);
    SetTimer(win->hwndCanvas, SMOOTHSCROLL_TIMER_ID, SMOOTHSCROLL_DELAY_IN_MS, nullptr);
    ScheduleRepaint(win, 0);
    return true;
}

void UpdateRectangularSelectionEdit(MainWindow* win, int x, int y) {
    if (!win || win->selectionDragEdge == SelectionDragEdge::None) {
        return;
    }
    int dx = x - win->dragStart.x;
    int dy = y - win->dragStart.y;
    win->selectionRect = ApplySelectionEdgeDrag(win->selectionEditOrig, win->selectionDragEdge, dx, dy);
    win->selectionMeasure = win->AsFixed() ? win->AsFixed()->CvtFromScreen(win->selectionRect).Size() : SizeF();
}

void PaintTransparentRectangles(Gfx* gfx, Rect screenRc, Vec<Rect>& rects, Color selectionColor, u8 alpha, int pad,
                                bool drawBorder) {
    Vec<Rect> paintedRects;
    screenRc.Inflate(pad, pad);
    for (int i = 0; i < len(rects); i++) {
        Rect rc = rects[i];
        if (pad > 0) {
            rc.Inflate(pad, pad);
        }
        rc = rc.Intersect(screenRc);
        if (!rc.IsEmpty()) {
            paintedRects.Append(rc);
        }
    }
    int outlineWidth = drawBorder ? pad : 0;
    gfx->FillRects(paintedRects.els, len(paintedRects), selectionColor, alpha, outlineWidth);
}

// Touch selection handles: a dot under each end of the selection, big enough
// to grab with a fingertip. kTouchSelHandleDip is the dot's diameter; the
// touchable area around it is padded so a slightly-off tap still lands.
constexpr int kTouchSelHandleDip = 14;
constexpr int kTouchSelHandleHitPadDip = 10;

bool GetTouchSelHandleRects(MainWindow* win, Rect& startOut, Rect& endOut) {
    DisplayModel* dm = win->AsFixed();
    WindowTab* tab = win->CurrentTab();
    if (!dm || !tab || !tab->selectionOnPage) {
        return false;
    }
    Vec<SelectionOnPage>& sel = *tab->selectionOnPage;
    int n = len(sel);
    if (n == 0) {
        return false;
    }
    // the selection runs first rect -> last rect, so the handles belong under
    // the bottom-left of the first and the bottom-right of the last
    Rect first = sel[0].GetRect(dm);
    Rect last = sel[n - 1].GetRect(dm);
    int dxy = DpiScale(kTouchSelHandleDip);
    int r = dxy / 2;
    startOut = Rect(first.x - r, first.y + first.dy, dxy, dxy);
    endOut = Rect(last.x + last.dx - r, last.y + last.dy, dxy, dxy);
    return true;
}

TouchSelHandle HitTestTouchSelHandle(MainWindow* win, int x, int y) {
    if (!win->touchSelHandles) {
        return TouchSelHandle::None;
    }
    Rect start, end;
    if (!GetTouchSelHandleRects(win, start, end)) {
        return TouchSelHandle::None;
    }
    int pad = DpiScale(kTouchSelHandleHitPadDip);
    start.Inflate(pad, pad);
    end.Inflate(pad, pad);
    Point pt(x, y);
    // the end handle wins a tie: it's the one a reader adjusts most
    if (end.Contains(pt)) {
        return TouchSelHandle::End;
    }
    if (start.Contains(pt)) {
        return TouchSelHandle::Start;
    }
    return TouchSelHandle::None;
}

void HideTouchSelHandles(MainWindow* win) {
    if (!win->touchSelHandles) {
        return;
    }
    win->touchSelHandles = false;
    win->touchSelDragging = TouchSelHandle::None;
    ScheduleRepaint(win, 0);
}

static void PaintTouchSelHandles(MainWindow* win, Gfx* gfx) {
    Rect start, end;
    if (!win->touchSelHandles || !GetTouchSelHandleRects(win, start, end)) {
        return;
    }
    ParsedColor* parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.selectionColor);
    Color col = parsedCol->col;
    gfx->FillEllipse(start, col);
    gfx->FillEllipse(end, col);
}

void PaintSelection(MainWindow* win, Gfx* gfx) {
    ReportIf(!win->AsFixed());

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
            // double/triple-click set the glyph range immediately; only extend
            // on repaint when the pointer has actually moved (issue #5712).
            int endX = win->selectionRect.x + win->selectionRect.dx;
            int endY = win->selectionRect.y + win->selectionRect.dy;
            bool dragged = IsDragDistance(win->selectionRect.x, endX, win->selectionRect.y, endY);
            UpdateTextSelection(win, dragged);
            if (!win->CurrentTab()->selectionOnPage) {
                // prevent the selection from disappearing while the
                // user is still at it (OnSelectionStop removes it
                // if it is still empty at the end)
                win->CurrentTab()->selectionOnPage = new Vec<SelectionOnPage>();
                win->showSelection = true;
            }
        }

        ReportDebugIf(!win->CurrentTab()->selectionOnPage);
        if (!win->CurrentTab()->selectionOnPage) {
            return;
        }

        for (SelectionOnPage& sel : *win->CurrentTab()->selectionOnPage) {
            rects.Append(sel.GetRect(win->AsFixed()));
        }
    }

    ParsedColor* parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.selectionColor);
    // honor the alpha channel of SelectionColor (#aarrggbb): a smaller alpha makes
    // the overlay more transparent so the selected text stays crisp (issue #3209).
    // Fall back to the historical default when no alpha is given (e.g. #rrggbb).
    u8 alpha = GetAlpha(parsedCol->col);
    if (alpha == 0) {
        alpha = kSelectionDefaultAlpha;
    }
    PaintTransparentRectangles(gfx, win->canvasRc, rects, parsedCol->col, alpha, 2, /*drawBorder*/ true);
    PaintTouchSelHandles(win, gfx);
}

void UpdateTextSelection(MainWindow* win, bool select) {
    if (!win->AsFixed()) {
        return;
    }

    // logf("UpdateTextSelection: select: %d\n", (int)select);
    DisplayModel* dm = win->AsFixed();
    if (select) {
        int pageNo = dm->GetPageNoByPoint(win->selectionRect.BR());
        if (win->ctrl->ValidPageNo(pageNo)) {
            PointF pt = dm->CvtFromScreen(win->selectionRect.BR(), pageNo);
            if (win->selectingByWord) {
                // double-click-drag: extend a whole word at a time (issue #4761)
                dm->textSelection->SelectWordsUpTo(pageNo, pt.x, pt.y);
            } else {
                dm->textSelection->SelectUpTo(pageNo, pt.x, pt.y);
            }
        }
    }

    DeleteOldSelectionInfo(win);
    win->CurrentTab()->selectionOnPage = SelectionOnPage::FromTextSelect(&dm->textSelection->result);
    win->showSelection = win->CurrentTab()->selectionOnPage != nullptr;

    if (win->uiaProvider) {
        win->uiaProvider->OnSelectionChanged();
    }
}

// isTextSelectionOut is set to true if this is text-only selection (as opposed to
// rectangular selection)
TempStr GetSelectedTextTemp(WindowTab* tab, Str lineSep, bool& isTextOnlySelectionOut) {
    if (!tab || !tab->selectionOnPage) {
        return {};
    }
    if (len(*tab->selectionOnPage) == 0) {
        return {};
    }
    DisplayModel* dm = tab->AsFixed();
    ReportIf(!dm);
    if (!dm) {
        return {};
    }
    if (dm->GetEngine()->IsImageCollection()) {
        return {};
    }

    isTextOnlySelectionOut = dm->textSelection->result.len > 0;
    if (isTextOnlySelectionOut) {
        Str s = dm->textSelection->ExtractText(lineSep);
        TempStr res = str::DupTemp(s);
        str::Free(s);
        return res;
    }
    StrVec selections;
    for (SelectionOnPage& sel : *tab->selectionOnPage) {
        // selection may reference pages that no longer exist after a reload
        if (!dm->ValidPageNo(sel.pageNo)) {
            continue;
        }
        Str text = dm->GetTextInRegion(sel.pageNo, sel.rect);
        if (text) {
            selections.Append(text);
        }
    }
    if (len(selections) == 0) {
        return {};
    }
    TempStr s = JoinTemp(&selections, lineSep);
    return s;
}

void CopySelectionToClipboard(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    ReportIf(len(*tab->selectionOnPage) == 0 && win->mouseAction != MouseAction::SelectingText);

    if (!OpenClipboardForUpdate()) {
        return;
    }
    AutoCall closeClipboard(CloseClipboardAfterUpdate);

    DisplayModel* dm = win->AsFixed();
    TempStr selText = nullptr;
    bool isTextOnlySelectionOut = false;
    if (!gDisableDocumentRestrictions && (dm && !dm->GetEngine()->AllowsCopyingText())) {
        NotificationCreateArgs args;
        args.hwndParent = win->hwndCanvas;
        args.msg = _TRA("Copying text was denied (copying as image only)");
        ShowNotification(args);
    } else {
        selText = GetSelectedTextTemp(tab, "\r\n", isTextOnlySelectionOut);
    }

    if (len(selText) > 0) {
        AppendTextToClipboard(selText);
    }

    if (isTextOnlySelectionOut) {
        // don't also copy the first line of a text selection as an image
        return;
    }

    if (!dm || !tab->selectionOnPage || len(*tab->selectionOnPage) == 0) {
        return;
    }
    /* also copy a screenshot of the current selection to the clipboard */
    SelectionOnPage* selOnPage = &(*tab->selectionOnPage)[0];
    if (!dm->ValidPageNo(selOnPage->pageNo)) {
        return;
    }
    float zoom = dm->GetZoomReal(selOnPage->pageNo);
    int rotation = dm->GetRotation();
    RenderPageArgs args(selOnPage->pageNo, zoom, rotation, &selOnPage->rect, RenderTarget::Export);
    Pixmap* bmp = dm->GetEngine()->RenderPage(args);
    if (!bmp) {
        logf("CopySelectionToClipboard: RenderPage(page %d) failed\n", selOnPage->pageNo);
        return;
    }
    // EngineImages (image files, cbz/cbr) renders sub-rects through GDI+ and
    // returns a malloc-backed Pixmap with no DIB section, so bmp->hbmp is null.
    // RenderedBitmapFromPixmap() makes one when needed (and consumes bmp).
    RenderedBitmap* rbmp = RenderedBitmapFromPixmap(bmp);
    if (!rbmp) {
        logf("CopySelectionToClipboard: RenderedBitmapFromPixmap() failed\n");
        return;
    }
    if (!CopyImageToClipboard(rbmp->GetBitmap(), true)) {
        logf("CopySelectionToClipboard: CopyImageToClipboard() failed\n");
    }
    delete rbmp;
}

void OnSelectAll(MainWindow* win, bool textOnly) {
    if (!HasPermission(Perm::CopySelection)) {
        return;
    }

    if ((win->findEdit && win->findEdit->IsFocused()) || (win->pageEdit && win->pageEdit->IsFocused())) {
        EditSelectAll(GetFocus());
        return;
    }

    if (win->AsChm()) {
        win->AsChm()->SelectAll();
    } else if (win->AsMarkdown()) {
        win->AsMarkdown()->SelectAll();
        return;
    }
    if (!win->AsFixed()) {
        return;
    }

    DisplayModel* dm = win->AsFixed();
    if (textOnly) {
        int pageNo;
        for (pageNo = 1; !dm->PageShown(pageNo); pageNo++) {
            ;
        }
        dm->textSelection->StartAt(pageNo, 0);
        for (pageNo = win->ctrl->PageCount(); !dm->PageShown(pageNo); pageNo--) {
            ;
        }
        dm->textSelection->SelectUpTo(pageNo, -1);
        win->selectionRect = Rect::FromXY(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
        UpdateTextSelection(win);
    } else {
        DeleteOldSelectionInfo(win, true);
        win->selectionRect = Rect::FromXY(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
        win->CurrentTab()->selectionOnPage = SelectionOnPage::FromRectangle(dm, win->selectionRect);
    }

    win->showSelection = win->CurrentTab()->selectionOnPage != nullptr;
    ScheduleRepaint(win, 0);
}

#define SELECT_AUTOSCROLL_AREA_WIDTH DpiScale(15)
#define SELECT_AUTOSCROLL_STEP_LENGTH DpiScale(10)

bool NeedsSelectionEdgeAutoscroll(MainWindow* win, int x, int y) {
    return x < SELECT_AUTOSCROLL_AREA_WIDTH || x > win->canvasRc.dx - SELECT_AUTOSCROLL_AREA_WIDTH ||
           y < SELECT_AUTOSCROLL_AREA_WIDTH || y > win->canvasRc.dy - SELECT_AUTOSCROLL_AREA_WIDTH;
}

// Horizontal auto-scroll while selecting text exists to reveal text the
// selection has reached. Once the selected text's leading edge is on screen
// there is nothing left to reveal, and scrolling on just pans the page out from
// under the user: at high zoom the cursor sits in the right margin long before
// the line ends, so the view runs away while the selection stays put (#5497).
// Vertical auto-scroll is untouched - there the next line really is off screen.
// Returns how much of `dx` is still needed, 0 once the selection is visible.
static int LimitTextSelectionAutoscrollDx(MainWindow* win, int dx) {
    DisplayModel* dm = win->AsFixed();
    if (!dm || !dm->textSelection) {
        return dx;
    }
    TextSel* sel = &dm->textSelection->result;
    if (sel->len == 0 || !sel->pages || !sel->rects) {
        return dx;
    }
    int selLeft = INT_MAX;
    int selRight = INT_MIN;
    for (int i = 0; i < sel->len; i++) {
        int pageNo = sel->pages[i];
        if (!dm->PageVisible(pageNo)) {
            continue;
        }
        Rect rc = dm->CvtToScreen(pageNo, ToRectF(sel->rects[i]));
        selLeft = std::min(selLeft, rc.x);
        selRight = std::max(selRight, rc.x + rc.dx);
    }
    if (selLeft > selRight) {
        return dx; // nothing selected on a visible page
    }
    int margin = SELECT_AUTOSCROLL_AREA_WIDTH;
    if (dx > 0) {
        int needed = selRight - (win->canvasRc.dx - margin);
        return limitValue(needed, 0, dx);
    }
    int needed = selLeft - margin;
    return limitValue(needed, dx, 0);
}

void OnSelectionEdgeAutoscroll(MainWindow* win, int x, int y) {
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

    ReportIf(NeedsSelectionEdgeAutoscroll(win, x, y) != (dx != 0 || dy != 0));
    // after the assert: clamping can legitimately leave dx at 0 while the
    // cursor is still in the auto-scroll strip
    if (dx != 0 && MouseAction::SelectingText == win->mouseAction) {
        dx = LimitTextSelectionAutoscrollDx(win, dx);
    }
    if (dx != 0 || dy != 0) {
        ReportIf(!win->AsFixed());
        DisplayModel* dm = win->AsFixed();
        Point oldOffset = dm->GetViewPort().TL();
        win->MoveDocBy(dx, dy);

        dx = dm->GetViewPort().x - oldOffset.x;
        dy = dm->GetViewPort().y - oldOffset.y;
        if (win->selectionDragEdge != SelectionDragEdge::None) {
            // move/resize: keep the selection fixed on the document as the view pans
            win->selectionEditOrig.x -= dx;
            win->selectionEditOrig.y -= dy;
            win->dragStart.x -= dx;
            win->dragStart.y -= dy;
            win->selectionRect.x -= dx;
            win->selectionRect.y -= dy;
        } else {
            // new selection: keep the start corner fixed on the document
            win->selectionRect.x -= dx;
            win->selectionRect.y -= dy;
            win->selectionRect.dx += dx;
            win->selectionRect.dy += dy;
        }
    }
}

void OnSelectionStart(MainWindow* win, int x, int y, WPARAM /*key*/) {
    ReportIf(!win->AsFixed());
    // selecting with the mouse takes over: leave keyboard selection mode so its
    // caret and help bar don't linger over a mouse selection
    StopSelectTextWithKeyboard(win);
    DeleteOldSelectionInfo(win, true);

    win->selectionDragEdge = SelectionDragEdge::None;
    win->selectionRect = Rect(x, y, 0, 0);
    win->showSelection = true;
    win->selectingByWord = false;
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
    ScheduleRepaint(win, 0);
}

void OnSelectionStop(MainWindow* win, int x, int y, bool aborted) {
    if (GetCapture() == win->hwndCanvas) {
        ReleaseCapture();
    }
    KillTimer(win->hwndCanvas, SMOOTHSCROLL_TIMER_ID);

    bool editingRect = win->selectionDragEdge != SelectionDragEdge::None && win->mouseAction == MouseAction::Selecting;

    // update the text selection before changing the selectionRect
    if (MouseAction::SelectingText == win->mouseAction) {
        // double/triple-click set the glyph range immediately; a tiny mouse jitter
        // while the button is held still updates selectionRect.dx/dy. Only extend
        // the selection on mouse-up when the pointer actually moved (issue #5712).
        bool dragged = IsDragDistance(win->selectionRect.x, x, win->selectionRect.y, y);
        UpdateTextSelection(win, dragged);
    }

    if (editingRect) {
        if (aborted) {
            // click without drag on a handle: keep previous selection
            win->selectionRect = win->selectionEditOrig;
        } else {
            UpdateRectangularSelectionEdit(win, x, y);
            win->selectionRect = NormalizeScreenRect(win->selectionRect);
        }
        delete win->CurrentTab()->selectionOnPage;
        win->CurrentTab()->selectionOnPage = SelectionOnPage::FromRectangle(win->AsFixed(), win->selectionRect);
        win->showSelection = win->CurrentTab()->selectionOnPage != nullptr;
        if (win->showSelection) {
            win->selectionMeasure = win->AsFixed()->CvtFromScreen(win->selectionRect).Size();
        } else {
            win->selectionMeasure = SizeF();
        }
        win->selectionDragEdge = SelectionDragEdge::None;
    } else {
        win->selectionRect = Rect::FromXY(win->selectionRect.x, win->selectionRect.y, x, y);
        if (aborted || (MouseAction::Selecting == win->mouseAction ? win->selectionRect.IsEmpty()
                                                                   : !win->CurrentTab()->selectionOnPage)) {
            DeleteOldSelectionInfo(win, true);
        } else if (win->mouseAction == MouseAction::Selecting) {
            win->selectionRect = NormalizeScreenRect(win->selectionRect);
            win->CurrentTab()->selectionOnPage = SelectionOnPage::FromRectangle(win->AsFixed(), win->selectionRect);
            win->showSelection = win->CurrentTab()->selectionOnPage != nullptr;
        }
        win->selectionDragEdge = SelectionDragEdge::None;
    }
    win->selectingByWord = false;
    // refresh selection-dependent toolbar buttons once, when the selection is
    // finalized, rather than on every repaint while dragging (UpdateTextSelection
    // runs from PaintSelection on each frame, which flickered the toolbar)
    ToolbarUpdateStateForWindow(win, false);
    ScheduleRepaint(win, 0);

    // show the floating selection toolbar for a finished text selection
    // (self-guards: needs a non-empty on-screen text selection)
    if (!aborted || editingRect) {
        ShowSelectionToolbar(win);
    }
}
