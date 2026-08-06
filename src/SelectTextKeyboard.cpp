/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Dpi.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "TextSelection.h"
#include "DisplayModel.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Canvas.h"
#include "Commands.h"
#include "Accelerators.h"
#include "Selection.h"
#include "Notifications.h"
#include "Translations.h"
#include "SelectTextKeyboard.h"

Kind kNotifTextSelectMode = "notifTextSelectMode";

// Keyboard text selection, a.k.a. caret browsing: F7 puts a text caret in the
// page which the arrow keys move, so text can be selected and copied without
// the mouse (issues #4684, #4116). Shift + movement extends the selection, and
// `v` turns on visual mode, where plain movement extends it - the same two
// modes Vimium's caret / visual mode gives a web page.
//
// The caret is a position in the page's text stream: (page, glyph index), the
// same coordinates TextSelection works in, so extending the selection is just
// TextSelection::StartAt(anchor) + SelectUpTo(caret).

// text selection needs a fixed-page engine with extractable text. Image
// collections have no text and the browser-backed controllers (CHM, markdown)
// have no DisplayModel and do their own selection.
bool CanSelectTextWithKeyboard(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm || !dm->textSelection) {
        return false;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine || engine->IsImageCollection()) {
        return false;
    }
    Kind k = engine->kind;
    if (k == kindEngineImage || k == kindEngineImageDir || k == kindEngineComicBooks) {
        return false;
    }
    return true;
}

bool SelectTextWithKeyboardActive(MainWindow* win) {
    return win && win->textSelectModeActive;
}

static int PageTextLen(EngineBase* engine, int pageNo) {
    int textLen = 0;
    engine->GetTextForPage(pageNo, &textLen);
    return textLen;
}

// Not every glyph has a box to work from: line breaks are zero-width and spaces
// can come back with no height. Find the nearest one that does, preferring the
// glyph before the caret so the caret lands at the end of the text to its left.
static bool GlyphHasBox(Rect* coords, int i) {
    return coords[i].dy > 0 && (coords[i].x || coords[i].dx);
}

static int NearestGlyphWithBox(Rect* coords, int textLen, int ix, bool* isBeforeOut) {
    ix = limitValue(ix, 0, textLen - 1);
    for (int i = ix; i >= 0; i--) {
        if (GlyphHasBox(coords, i)) {
            *isBeforeOut = i < ix;
            return i;
        }
    }
    for (int i = ix + 1; i < textLen; i++) {
        if (GlyphHasBox(coords, i)) {
            *isBeforeOut = false;
            return i;
        }
    }
    return -1;
}

// Screen rect of the caret: a thin bar on the leading edge of the glyph the
// caret sits before, or on the trailing edge of the glyph to its left when that
// glyph is the last one with a box (end of line, end of text).
static bool CaretScreenRect(MainWindow* win, Rect& out) {
    DisplayModel* dm = win->AsFixed();
    if (!dm || !dm->ValidPageNo(win->textSelectPage)) {
        return false;
    }
    PageInfo* pi = dm->GetPageInfo(win->textSelectPage);
    if (!pi || !pi->isShown) {
        return false;
    }
    Rect* coords = nullptr;
    int textLen = 0;
    dm->GetEngine()->GetTextForPage(win->textSelectPage, &textLen, &coords);
    if (textLen <= 0 || !coords) {
        return false;
    }
    bool trailing = false;
    int ix = NearestGlyphWithBox(coords, textLen, win->textSelectGlyph, &trailing);
    if (ix < 0) {
        return false;
    }
    // past the last glyph the caret belongs after it, not before it
    trailing = trailing || win->textSelectGlyph >= textLen;
    Rect r = dm->CvtToScreen(win->textSelectPage, ToRectF(coords[ix]));
    int caretDx = std::max(DpiScale(win->hwndCanvas, 2), 2);
    out = Rect{trailing ? r.x + r.dx : r.x, r.y, caretDx, r.dy};
    return true;
}

static void ScrollCaretIntoView(MainWindow* win) {
    Rect r;
    if (!CaretScreenRect(win, r)) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    // give the caret a bit of context instead of parking it on the very edge
    int pad = r.dy;
    Rect withContext{r.x, r.y - pad, r.dx, r.dy + (2 * pad)};
    dm->ScrollScreenToRect(win->textSelectPage, withContext);
}

// Push anchor..caret into the document's text selection (or clear it when the
// caret is just being moved around).
static void ApplySelection(MainWindow* win, bool selecting) {
    DisplayModel* dm = win->AsFixed();
    if (!dm || !dm->textSelection) {
        return;
    }
    if (!selecting) {
        dm->textSelection->Reset();
        dm->textSelection->startPage = dm->textSelection->endPage = -1;
        dm->textSelection->startGlyph = dm->textSelection->endGlyph = -1;
        DeleteOldSelectionInfo(win, false);
        return;
    }
    dm->textSelection->StartAt(win->textSelectAnchorPage, win->textSelectAnchorGlyph);
    dm->textSelection->SelectUpTo(win->textSelectPage, win->textSelectGlyph);
    UpdateTextSelection(win, false);
}

static void RestartCaretBlink(MainWindow* win) {
    win->textSelectCaretVisible = true;
    if (!win->hwndCanvas) {
        return;
    }
    uint blinkMs = GetCaretBlinkTime();
    if (blinkMs == 0 || blinkMs == INFINITE) {
        // blinking turned off system-wide: leave the caret solid
        KillTimer(win->hwndCanvas, kTextSelectCaretTimerID);
        return;
    }
    SetTimer(win->hwndCanvas, kTextSelectCaretTimerID, blinkMs, nullptr);
}

void SelectTextWithKeyboardBlinkCaret(MainWindow* win) {
    if (!SelectTextWithKeyboardActive(win)) {
        return;
    }
    win->textSelectCaretVisible = !win->textSelectCaretVisible;
    ScheduleRepaint(win, 0);
}

// Where the caret goes when the mode is turned on: continue an existing text
// selection if there is one, otherwise the start of the current page's text.
static void PlaceInitialCaret(MainWindow* win) {
    DisplayModel* dm = win->AsFixed();
    TextSelection* ts = dm->textSelection;
    int fromPage, fromGlyph, toPage, toGlyph;
    ts->GetGlyphRange(&fromPage, &fromGlyph, &toPage, &toGlyph);
    if (ts->result.len > 0 && fromPage > 0 && toPage > 0) {
        win->textSelectAnchorPage = fromPage;
        win->textSelectAnchorGlyph = fromGlyph;
        win->textSelectPage = toPage;
        win->textSelectGlyph = toGlyph;
        return;
    }
    int pageNo = win->ctrl->CurrentPageNo();
    if (!dm->ValidPageNo(pageNo)) {
        pageNo = 1;
    }
    win->textSelectPage = pageNo;
    win->textSelectGlyph = 0;
    win->textSelectAnchorPage = pageNo;
    win->textSelectAnchorGlyph = 0;
}

bool StopSelectTextWithKeyboard(MainWindow* win) {
    if (!win || !win->textSelectModeActive) {
        return false;
    }
    win->textSelectModeActive = false;
    win->textSelectModeVisual = false;
    if (win->hwndCanvas) {
        KillTimer(win->hwndCanvas, kTextSelectCaretTimerID);
        RemoveNotificationsForGroup(win->hwndCanvas, kNotifTextSelectMode);
    }
    ScheduleRepaint(win, 0);
    return true;
}

static void ShowModeNotification(MainWindow* win) {
    NotificationCreateArgs args;
    args.hwndParent = win->hwndCanvas;
    args.msg = win->textSelectModeVisual ? _TRA("Select text: arrows extend the selection, Ctrl+C copies, Esc exits")
                                         : _TRA(
                                               "Select text: arrows move the caret, Shift+arrows select, v selects, "
                                               "Ctrl+C copies, Esc exits");
    // no timeout: the keys are the whole interface of this mode, so the hint
    // stays up for as long as the mode does (removed by StopSelectTextWithKeyboard).
    // The close button is still there for anyone who has learned them.
    args.timeoutMs = kNotifNoTimeout;
    args.groupId = kNotifTextSelectMode;
    ShowNotification(args);
}

void ToggleSelectTextWithKeyboard(MainWindow* win) {
    if (StopSelectTextWithKeyboard(win)) {
        return;
    }
    if (!CanSelectTextWithKeyboard(win)) {
        return;
    }
    if (PageTextLen(win->AsFixed()->GetEngine(), win->ctrl->CurrentPageNo()) <= 0) {
        // no text to put a caret in: don't leave the user in a mode with no
        // feedback (scanned pages without OCR, blank pages)
        NotificationCreateArgs args;
        args.hwndParent = win->hwndCanvas;
        args.msg = _TRA("No text on this page");
        args.timeoutMs = 2000;
        args.groupId = kNotifTextSelectMode;
        ShowNotification(args);
        return;
    }
    win->textSelectModeActive = true;
    win->textSelectModeVisual = false;
    PlaceInitialCaret(win);
    RestartCaretBlink(win);
    ScrollCaretIntoView(win);
    ShowModeNotification(win);
    ScheduleRepaint(win, 0);
}

// how many lines fit on screen, used for PageUp / PageDown
static int LinesPerScreen(MainWindow* win) {
    Rect caret;
    int lineDy = CaretScreenRect(win, caret) ? caret.dy : 0;
    if (lineDy <= 0) {
        return 10;
    }
    DisplayModel* dm = win->AsFixed();
    int n = dm->GetViewPort().dy / lineDy;
    return limitValue(n - 1, 1, 200);
}

// first / last glyph of the visual line the caret is on (Home / End)
static bool MoveToLineEdge(MainWindow* win, int dir) {
    DisplayModel* dm = win->AsFixed();
    Rect* coords = nullptr;
    int textLen = 0;
    dm->GetEngine()->GetTextForPage(win->textSelectPage, &textLen, &coords);
    if (textLen <= 0 || !coords) {
        return false;
    }
    bool trailing = false; // not used here: we want the line, not the caret side
    int ix = NearestGlyphWithBox(coords, textLen, win->textSelectGlyph, &trailing);
    if (ix < 0) {
        return false;
    }
    // glyphs of one visual line share a y band; a glyph whose center is more
    // than half a line height away is on another line
    int refY = coords[ix].y + (coords[ix].dy / 2);
    int slop = std::max(coords[ix].dy / 2, 2);
    int best = ix;
    for (int i = 0; i < textLen; i++) {
        if (!GlyphHasBox(coords, i)) {
            continue;
        }
        int cy = coords[i].y + (coords[i].dy / 2);
        if (std::abs(cy - refY) > slop) {
            continue;
        }
        if (dir < 0 ? i < best : i > best) {
            best = i;
        }
    }
    int newGlyph = dir < 0 ? best : best + 1; // the caret sits before its glyph
    if (newGlyph == win->textSelectGlyph) {
        return false;
    }
    win->textSelectGlyph = newGlyph;
    return true;
}

// previous / next word start (Ctrl + Left / Right)
static bool MoveByWord(MainWindow* win, int dir) {
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm->GetEngine();
    int page = win->textSelectPage;
    int glyph = win->textSelectGlyph;
    int textLen = 0;
    Str text = engine->GetTextForPage(page, &textLen);
    if (textLen <= 0) {
        return false;
    }
    auto charAt = [&](int ix) -> int {
        if (ix < 0 || ix >= textLen) {
            return 0;
        }
        int byteIdx = Utf8CodepointToByteIndex(text, ix);
        int next = byteIdx;
        return Utf8CodepointNext(text, next);
    };
    int startPage = page;
    int startGlyph = glyph;
    // step off the current position, then over any run of non-word characters,
    // then to the far side of the word we land in
    if (!TextPosMoveBy(engine, page, glyph, TextSelectUnit::Glyph, dir)) {
        return false;
    }
    if (page != startPage) {
        win->textSelectPage = page;
        win->textSelectGlyph = glyph;
        return true;
    }
    while (glyph > 0 && glyph < textLen && !isWordChar(charAt(dir < 0 ? glyph - 1 : glyph))) {
        if (!TextPosMoveBy(engine, page, glyph, TextSelectUnit::Glyph, dir) || page != startPage) {
            break;
        }
    }
    while (glyph > 0 && glyph < textLen && isWordChar(charAt(dir < 0 ? glyph - 1 : glyph))) {
        if (!TextPosMoveBy(engine, page, glyph, TextSelectUnit::Glyph, dir) || page != startPage) {
            break;
        }
    }
    if (page == startPage && glyph == startGlyph) {
        return false;
    }
    win->textSelectPage = page;
    win->textSelectGlyph = glyph;
    return true;
}

static bool MoveCaret(MainWindow* win, WPARAM key) {
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm->GetEngine();
    bool isCtrl = IsCtrlPressed();
    int page = win->textSelectPage;
    int glyph = win->textSelectGlyph;

    switch (key) {
        case VK_LEFT:
        case VK_RIGHT: {
            int dir = key == VK_RIGHT ? 1 : -1;
            if (isCtrl) {
                return MoveByWord(win, dir);
            }
            if (!TextPosMoveBy(engine, page, glyph, TextSelectUnit::Glyph, dir)) {
                return false;
            }
            break;
        }
        case VK_UP:
        case VK_DOWN: {
            int dir = key == VK_DOWN ? 1 : -1;
            if (!TextPosMoveBy(engine, page, glyph, TextSelectUnit::Line, dir)) {
                return false;
            }
            break;
        }
        case VK_PRIOR:
        case VK_NEXT: {
            int dir = key == VK_NEXT ? 1 : -1;
            int n = LinesPerScreen(win);
            bool moved = false;
            for (int i = 0; i < n; i++) {
                if (!TextPosMoveBy(engine, page, glyph, TextSelectUnit::Line, dir)) {
                    break;
                }
                moved = true;
            }
            if (!moved) {
                return false;
            }
            break;
        }
        case VK_HOME:
        case VK_END: {
            int dir = key == VK_HOME ? -1 : 1;
            if (!isCtrl) {
                return MoveToLineEdge(win, dir);
            }
            if (dir < 0) {
                page = 1;
                glyph = 0;
            } else {
                page = engine->PageCount();
                glyph = PageTextLen(engine, page);
            }
            break;
        }
        default:
            return false;
    }
    if (page == win->textSelectPage && glyph == win->textSelectGlyph) {
        return false;
    }
    win->textSelectPage = page;
    win->textSelectGlyph = glyph;
    return true;
}

static bool IsCaretMoveKey(WPARAM key) {
    switch (key) {
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_HOME:
        case VK_END:
            return true;
        default:
            return false;
    }
}

// Movement keys, handled before they can scroll the view. Returns true if the
// key was consumed.
bool SelectTextWithKeyboardOnKeyDown(MainWindow* win, WPARAM key) {
    if (!SelectTextWithKeyboardActive(win)) {
        return false;
    }
    if (!IsCaretMoveKey(key) || IsAltPressed()) {
        return false;
    }
    if (!CanSelectTextWithKeyboard(win)) {
        StopSelectTextWithKeyboard(win);
        return false;
    }
    bool selecting = IsShiftPressed() || win->textSelectModeVisual;
    if (!selecting) {
        // a plain move drops the old selection and re-anchors here
        win->textSelectAnchorPage = win->textSelectPage;
        win->textSelectAnchorGlyph = win->textSelectGlyph;
    }
    if (MoveCaret(win, key)) {
        if (!selecting) {
            win->textSelectAnchorPage = win->textSelectPage;
            win->textSelectAnchorGlyph = win->textSelectGlyph;
        }
        ApplySelection(win, selecting);
        ScrollCaretIntoView(win);
    }
    RestartCaretBlink(win);
    ScheduleRepaint(win, 0);
    return true;
}

// 'v' toggles visual mode, 'y' / Ctrl+C copy. Returns true if consumed.
bool SelectTextWithKeyboardOnChar(MainWindow* win, WPARAM key) {
    if (!SelectTextWithKeyboardActive(win)) {
        return false;
    }
    if (key == 'v' || key == 'V') {
        win->textSelectModeVisual = !win->textSelectModeVisual;
        if (win->textSelectModeVisual) {
            // start selecting from where the caret is now
            win->textSelectAnchorPage = win->textSelectPage;
            win->textSelectAnchorGlyph = win->textSelectGlyph;
        }
        ShowModeNotification(win);
        ScheduleRepaint(win, 0);
        return true;
    }
    if (key == 'y' || key == 'Y') {
        CopySelectionToClipboard(win);
        StopSelectTextWithKeyboard(win);
        return true;
    }
    return false;
}

// State dump for -dbg-control tests (tests/issue-4684.ts).
TempStr SelectTextKeyboardResultTemp(int* exitCodeOut) {
    str::Builder out;
    if (len(gWindows) == 0) {
        *exitCodeOut = 2;
        out.Append("NOTREADY no-window\n");
        return ToStrTemp(out);
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win->AsFixed();
    int nSelRects = (dm && dm->textSelection) ? dm->textSelection->result.len : 0;
    out.Append(fmt("active=%d visual=%d canSelect=%d page=%d glyph=%d anchorPage=%d anchorGlyph=%d selRects=%d\n",
                   win->textSelectModeActive ? 1 : 0, win->textSelectModeVisual ? 1 : 0,
                   CanSelectTextWithKeyboard(win) ? 1 : 0, win->textSelectPage, win->textSelectGlyph,
                   win->textSelectAnchorPage, win->textSelectAnchorGlyph, nSelRects));
    Rect caret;
    if (CaretScreenRect(win, caret)) {
        out.Append(fmt("caret=%d,%d,%d,%d\n", caret.x, caret.y, caret.dx, caret.dy));
    }
    if (nSelRects > 0) {
        bool isTextOnly = false;
        TempStr sel = GetSelectedTextTemp(win->CurrentTab(), StrL("\n"), isTextOnly);
        out.Append(fmt("text=%s\n", sel));
    }
    *exitCodeOut = 0;
    return ToStrTemp(out);
}

void PaintKeyboardTextCaret(MainWindow* win, HDC hdc) {
    if (!SelectTextWithKeyboardActive(win) || !win->textSelectCaretVisible) {
        return;
    }
    Rect r;
    if (!CaretScreenRect(win, r)) {
        return;
    }
    if (win->canvasRc.Intersect(r).IsEmpty()) {
        return;
    }
    // invert instead of painting a color: the caret then shows up on any page
    // background, in any theme, without a contrast calculation
    RECT rc = ToRECT(r);
    PatBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, DSTINVERT);
}
