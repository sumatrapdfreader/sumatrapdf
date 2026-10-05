/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's LinkFollow.cpp, unchanged except for what was win32: the badge is
// drawn through the canvas helpers instead of Gfx, the recompute timer is a
// countdown the shell's tick drives, and the key arguments are ints rather
// than WPARAM.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "Commands.h"
#include "ShortcutParse.h"
#include "Accelerators.h"
#include "Translations.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "Selection.h"
#include "LinkFollow.h"

#include "SumatraLog.h"

Kind kNotifLinkFollow = "notifLinkFollow";

// Vimium-style keyboard link following: Shift + F labels the links on screen
// with letter hints; typing a hint follows the link without the mouse (issue
// #2629).

// Vimium's default alphabet favors keys on or close to the home row. Its order
// also matters: when more hints are needed, earlier characters become prefixes
// of multi-letter hints first.
static constexpr char kLinkHintChars[] = "SADFJKLEWCMPGH";

// orig recomputes 300 ms after scrolling stops
constexpr int kLinkFollowRecomputeDelayInMs = 300;

struct LinkHint {
    char s[kMaxKeyboardLinkHintLength + 1]{};
    int len = 0;
};

static int CmpLinkHints(const LinkHint* a, const LinkHint* b) {
    return str::Cmp(Str(a->s, a->len), Str(b->s, b->len));
}

// Build a prefix-free breadth-first hint tree, matching Vimium's AlphabetHints
// algorithm. Expanding a leaf replaces its one-letter hint with a group of
// longer hints, so no complete hint can also prefix another hint.
static void AssignLinkHints(Vec<KeyboardLinkTarget>& targets) {
    int nTargets = len(targets);
    if (nTargets == 0) {
        return;
    }

    Vec<LinkHint> hints;
    VecAppend(hints, LinkHint{});
    int offset = 0;
    while ((len(hints) - offset < nTargets) || len(hints) == 1) {
        LinkHint hint = hints[offset++];
        if (hint.len >= kMaxKeyboardLinkHintLength) {
            ReportIf(true);
            return;
        }
        for (int i = 0; i < LenL(kLinkHintChars); i++) {
            LinkHint next;
            next.s[0] = kLinkHintChars[i];
            memcpy(next.s + 1, hint.s, hint.len);
            next.len = hint.len + 1;
            VecAppend(hints, next);
        }
    }

    Vec<LinkHint> selected;
    for (int i = 0; i < nTargets; i++) {
        VecAppend(selected, hints[offset + i]);
    }
    VecSort(selected, CmpLinkHints);
    for (int i = 0; i < nTargets; i++) {
        LinkHint& hint = selected[i];
        for (int j = 0; j < hint.len / 2; j++) {
            char tmp = hint.s[j];
            hint.s[j] = hint.s[hint.len - j - 1];
            hint.s[hint.len - j - 1] = tmp;
        }
        KeyboardLinkTarget& target = targets[i];
        memcpy(target.hint, hint.s, hint.len);
        target.hintLen = hint.len;
    }
}

// only formats whose pages can carry links. image collections (comic books,
// image folders, single images) never do, and the browser-backed controllers
// (CHM, markdown) have their own hyperlink handling and no DisplayModel.
bool CanFollowLinksWithKeyboard(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return false;
    }
    if (gSettings && gSettings->disableLinks) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
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

bool KeyboardLinkFollowingActive(MainWindow* win) {
    return win && win->linkFollowActive;
}

// Unmodified letter keys and Backspace type a hint. Callers must already have
// filtered out Ctrl/Alt/Shift so Shift+F still toggles the mode off. Issue #6019.
bool KeyboardLinkFollowingCapturesKey(MainWindow* win, int vk) {
    if (!KeyboardLinkFollowingActive(win)) {
        return false;
    }
    if (vk == VK_BACK) {
        return true;
    }
    if (vk >= 'A' && vk <= 'Z') {
        return true;
    }
    if (vk >= 'a' && vk <= 'z') {
        return true;
    }
    return false;
}

struct ScreenTarget {
    Rect screenRect;
    KeyboardLinkTarget target;
};

// sort what's on screen into reading order (top to bottom, then left to right)
// so the labels don't jump around; rows are matched with a tolerance
// because links on the same line rarely share an exact top edge
static int CmpTargetsInReadingOrder(const ScreenTarget* a, const ScreenTarget* b) {
    const Rect& ta = a->screenRect;
    const Rect& tb = b->screenRect;
    int rowTolerance = 8;
    if (abs(ta.y - tb.y) > rowTolerance) {
        return ta.y < tb.y ? -1 : 1;
    }
    if (ta.x != tb.x) {
        return ta.x < tb.x ? -1 : 1;
    }
    return 0;
}

void KeyboardLinkFollowingRecompute(MainWindow* win) {
    VecReset(win->linkFollowTargets);
    win->linkFollowInputLen = 0;
    if (!KeyboardLinkFollowingActive(win) || !CanFollowLinksWithKeyboard(win)) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm->GetEngine();
    Rect viewPort(Point(), dm->GetViewPort().Size());

    Vec<ScreenTarget> found;
    int nPages = dm->PageCount();
    for (int pageNo = 1; pageNo <= nPages; pageNo++) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (!pi || !pi->isShown || pi->visibleRatio <= 0.0f) {
            continue;
        }
        // TryGetElements so a busy render thread holding pagesLock can't stall
        // the UI here; a page we miss is picked up by the next recompute
        Vec<IPageElement*> els;
        if (!engine->TryGetElements(pageNo, &els)) {
            continue;
        }
        for (IPageElement* el : els) {
            if (!el->IsLink()) {
                continue;
            }
            Rect screenRect = dm->CvtToScreen(pageNo, el->GetRect());
            if (viewPort.Intersect(screenRect).IsEmpty()) {
                continue;
            }
            ScreenTarget st;
            st.screenRect = screenRect;
            st.target.pageNo = pageNo;
            st.target.rect = el->GetRect();
            VecAppend(found, st);
        }
    }

    VecSort(found, CmpTargetsInReadingOrder);

    for (int i = 0; i < len(found); i++) {
        VecAppend(win->linkFollowTargets, found[i].target);
    }
    AssignLinkHints(win->linkFollowTargets);
}

// returns true if the mode was on (and is now off), so callers can tell whether
// they consumed the key
bool StopKeyboardLinkFollowing(MainWindow* win) {
    if (!win || !win->linkFollowActive) {
        return false;
    }
    win->linkFollowActive = false;
    VecReset(win->linkFollowTargets);
    win->linkFollowInputLen = 0;
    AppShellInvalidate(win);
    return true;
}

void ToggleKeyboardLinkFollowing(MainWindow* win) {
    if (StopKeyboardLinkFollowing(win)) {
        return;
    }
    if (!CanFollowLinksWithKeyboard(win)) {
        return;
    }
    win->linkFollowActive = true;
    KeyboardLinkFollowingRecompute(win);
    if (len(win->linkFollowTargets) == 0) {
        // nothing to follow: don't leave the user in a mode with no feedback
        win->linkFollowActive = false;
        NotificationCreateArgs args;
        args.win = win;
        args.msg = Tr("No links on this page");
        args.timeoutMs = 2000;
        args.groupId = kNotifLinkFollow;
        ShowNotification(args);
        return;
    }
    AppShellInvalidate(win);
}

// Recomputing on every scroll step would re-enumerate every visible page's
// elements at wheel/animation rate, so coalesce into one recompute 300ms after
// scrolling stops. The badges stay glued to their links meanwhile because they
// are stored in page coordinates.
// ng: orig arms a win32 timer; here the shell's tick counts the delay down.
static int gRecomputePendingMs = -1;

void KeyboardLinkFollowingViewportChanged(MainWindow* win, int elapsedMs) {
    if (!KeyboardLinkFollowingActive(win)) {
        gRecomputePendingMs = -1;
        return;
    }
    if (elapsedMs < 0) {
        gRecomputePendingMs = kLinkFollowRecomputeDelayInMs;
        return;
    }
    if (gRecomputePendingMs < 0) {
        return;
    }
    gRecomputePendingMs -= elapsedMs;
    if (gRecomputePendingMs > 0) {
        return;
    }
    gRecomputePendingMs = -1;
    KeyboardLinkFollowingRecompute(win);
    AppShellInvalidate(win);
}

// mirrors what a left click on a link does in OnMouseLeftButtonUp
static void FollowKeyboardLinkTarget(MainWindow* win, const KeyboardLinkTarget& target) {
    DisplayModel* dm = win->AsFixed();
    if (!dm || !dm->ValidPageNo(target.pageNo)) {
        return;
    }
    // re-resolve the element instead of holding a pointer: the elements belong
    // to the engine's page cache and can be freed under us (reload, page evicted)
    PointF center(target.rect.x + (target.rect.dx / 2), target.rect.y + (target.rect.dy / 2));
    IPageElement* el = dm->GetEngine()->GetElementAtPos(target.pageNo, center);
    if (!el) {
        return;
    }
    IPageDestination* dest = el->AsLink();
    if (!dest) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    Kind kind = dest->GetKind();
    if (tab && (kindDestinationLaunchURL == kind || kindDestinationLaunchFile == kind)) {
        // highlight the followed link, like clicking one does, as a reminder of
        // the last action once the user comes back
        DeleteOldSelectionInfo(win, true);
        tab->selectionOnPage = SelectionOnPage::FromRectangle(dm, dm->CvtToScreen(target.pageNo, target.rect));
        win->showSelection = tab->selectionOnPage != nullptr;
    }
    win->ctrl->HandleLink(dest, win->linkHandler);
}

static bool HintStartsWith(const KeyboardLinkTarget& target, const char* prefix, int prefixLen) {
    return target.hintLen >= prefixLen && memcmp(target.hint, prefix, prefixLen) == 0;
}

// Letter hints are prefix-free, so a complete match can be followed
// immediately even when other targets use multi-letter hints.
bool KeyboardLinkFollowingOnChar(MainWindow* win, int key) {
    if (!KeyboardLinkFollowingActive(win)) {
        return false;
    }

    if (key == VK_BACK) {
        if (win->linkFollowInputLen == 0) {
            StopKeyboardLinkFollowing(win);
        } else {
            win->linkFollowInputLen--;
            AppShellInvalidate(win);
        }
        return true;
    }
    if (key >= 'a' && key <= 'z') {
        key -= 'a' - 'A';
    }
    if (key < 'A' || key > 'Z') {
        return false;
    }

    int inputLen = win->linkFollowInputLen;
    if (inputLen >= kMaxKeyboardLinkHintLength) {
        StopKeyboardLinkFollowing(win);
        return true;
    }
    win->linkFollowInput[inputLen++] = (char)key;
    win->linkFollowInputLen = inputLen;

    int exactMatch = -1;
    int nMatches = 0;
    for (int i = 0; i < len(win->linkFollowTargets); i++) {
        const KeyboardLinkTarget& target = win->linkFollowTargets[i];
        if (!HintStartsWith(target, win->linkFollowInput, inputLen)) {
            continue;
        }
        nMatches++;
        if (target.hintLen == inputLen) {
            exactMatch = i;
        }
    }
    if (nMatches == 0) {
        // Like Vimium, an input that cannot match a hint cancels the mode and
        // is consumed instead of becoming an unrelated application shortcut.
        StopKeyboardLinkFollowing(win);
        return true;
    }
    if (exactMatch < 0) {
        AppShellInvalidate(win);
        return true;
    }

    KeyboardLinkTarget target = win->linkFollowTargets[exactMatch];
    // leave the mode first: following the link scrolls (or opens a browser), so
    // the hints are stale either way
    StopKeyboardLinkFollowing(win);
    FollowKeyboardLinkTarget(win, target);
    return true;
}

// State dump for the scripted tests: whether the mode is on, which page we're
// on, and the labeled links with their screen rects. action "stop" leaves the
// mode; "char" types chars as hint input.
TempStr KeyboardLinkFollowResultTemp(Str action, Str chars, int* exitCodeOut) {
    str::Builder out;
    if (len(gWindows) == 0) {
        *exitCodeOut = 2;
        out.Append(StrL("NOTREADY no-window\n"));
        return ToStrTemp(out);
    }
    MainWindow* win = gWindows[0];
    if (str::Eq(action, StrL("stop"))) {
        StopKeyboardLinkFollowing(win);
    } else if (str::Eq(action, StrL("char"))) {
        for (int i = 0; i < len(chars); i++) {
            KeyboardLinkFollowingOnChar(win, (u8)chars.s[i]);
        }
    } else if (len(action) > 0) {
        *exitCodeOut = 1;
        out.Append(StrL("ERROR unknown-action\n"));
        return ToStrTemp(out);
    }
    DisplayModel* dm = win->AsFixed();
    int currPage = win->ctrl ? win->ctrl->CurrentPageNo() : 0;
    TempStr accel = AppendAccelKeyToMenuStringTemp({}, CmdToggleKeyboardLinkFollowing);
    if (accel.len > 1 && accel.s[0] == '\t') {
        accel = TempStr(accel.s + 1, accel.len - 1);
    }
    out.Append(fmt("active=%d canFollow=%d page=%d count=%d accel=%s\n", win->linkFollowActive ? 1 : 0,
                   CanFollowLinksWithKeyboard(win) ? 1 : 0, currPage, len(win->linkFollowTargets), accel));
    int n = len(win->linkFollowTargets);
    for (int i = 0; i < n; i++) {
        const KeyboardLinkTarget& t = win->linkFollowTargets[i];
        Rect r = dm ? dm->CvtToScreen(t.pageNo, t.rect) : Rect{};
        Str hint(t.hint, t.hintLen);
        out.Append(fmt("link=%d page=%d rect=%d,%d,%d,%d hint=%s\n", i + 1, t.pageNo, r.x, r.y, r.dx, r.dy, hint));
    }
    *exitCodeOut = 0;
    return ToStrTemp(out);
}

constexpr Color kLinkFollowHighlightCol = MkRgb(0xff, 0xf1, 0x00);
constexpr Color kLinkFollowBadgeBgCol = MkRgb(0xd3, 0x2f, 0x2f);
constexpr Color kLinkFollowBadgeTextCol = kColWhite;
constexpr float kLinkFollowBadgeFontSize = 11;

static void PaintLinkBadge(gp::PaintCtx* ctx, const Rect& linkRect, Str label) {
    Size textSize = CanvasMeasureText(ctx, label, kLinkFollowBadgeFontSize);
    int padX = textSize.dy / 3;
    int dx = textSize.dx + (2 * padX);
    int dy = textSize.dy;
    // sit at the link's top-left corner, pulled slightly outside it so the badge
    // doesn't cover the link text itself
    int x = linkRect.x - (dx / 3);
    int y = linkRect.y - (dy / 3);
    if (x < 0) {
        x = linkRect.x;
    }
    if (y < 0) {
        y = linkRect.y;
    }

    Rect badge{x, y, dx, dy};
    CanvasFillRects(ctx, &badge, 1, kLinkFollowBadgeBgCol, 235, 0);
    CanvasDrawText(ctx, label, {x + padX, y}, kLinkFollowBadgeTextCol, kLinkFollowBadgeFontSize);
}

void PaintKeyboardLinkTargets(MainWindow* win, gp::PaintCtx* ctx) {
    if (!KeyboardLinkFollowingActive(win)) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    int n = len(win->linkFollowTargets);
    if (n == 0) {
        return;
    }

    Vec<Rect> screenRects;
    for (int i = 0; i < n; i++) {
        const KeyboardLinkTarget& t = win->linkFollowTargets[i];
        if (!HintStartsWith(t, win->linkFollowInput, win->linkFollowInputLen)) {
            continue;
        }
        VecAppend(screenRects, dm->CvtToScreen(t.pageNo, t.rect));
    }
    Rect canvas(Point(), dm->GetViewPort().Size());
    PaintTransparentRectangles(ctx, canvas, screenRects, kLinkFollowHighlightCol, 90, 2, false);

    int rectIdx = 0;
    for (int i = 0; i < n; i++) {
        const KeyboardLinkTarget& t = win->linkFollowTargets[i];
        if (!HintStartsWith(t, win->linkFollowInput, win->linkFollowInputLen)) {
            continue;
        }
        PaintLinkBadge(ctx, screenRects[rectIdx++], Str(t.hint, t.hintLen));
    }
}
