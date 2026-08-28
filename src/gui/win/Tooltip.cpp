/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "gui/Layout.h"
#include "gui/GuiColors.h"
#include "gui/win/WinGui.h"

static void TooltipApplyColors(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    GuiColorsInitIfNeeded();
    // TOOLTIPS_CLASS ships with a visual style; TTM_SETTIP* colors are
    // ignored until the style is stripped (issue #6000).
    SetWindowTheme(hwnd, L"", L"");
    SendMessageW(hwnd, TTM_SETTIPBKCOLOR, (WPARAM)gColsWin[kColWinBg], 0);
    SendMessageW(hwnd, TTM_SETTIPTEXTCOLOR, (WPARAM)gColsWin[kColWinText], 0);
}

//--- Tooltip

// https://docs.microsoft.com/en-us/windows/win32/controls/tooltip-control-reference

static Kind kindTooltip = "tooltip";

static AtomicInt gTolltipID = 0;

// Canvas infotips (home thumbnails, page elements) are shown from WM_SETCURSOR
// via SetSingle. TTF_SUBCLASS on the canvas breaks after open→close→home
// (tooltip control subclasses the canvas; document UI / UIA / child edit
// churn leaves mouse tracking dead). Track mode shows/hides under our control
// and never subclasses the canvas.
static constexpr UINT kTrackToolFlags = TTF_TRACK | TTF_ABSOLUTE | TTF_TRANSPARENT;

static int GetNextTooltipID() {
    return AtomicIntInc(&gTolltipID);
}

int TooltipGetCount(HWND hwnd) {
    int n = (int)SendMessageW(hwnd, TTM_GETTOOLCOUNT, 0, 0);
    return n;
}

void TooltipRemoveAll(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    int n = TooltipGetCount(hwnd);
    for (int i = n - 1; i >= 0; i--) {
        TOOLINFOW ti{};
        ti.cbSize = sizeof(ti);
        BOOL ok = (BOOL)SendMessageW(hwnd, TTM_ENUMTOOLSW, i, (LPARAM)&ti);
        if (ok) {
            SendMessageW(hwnd, TTM_DELTOOLW, 0, (LPARAM)&ti);
        }
    }
}

static TempStr TooltipGetTextTemp(HWND hwnd, HWND owner, int id) {
    WCHAR buf[512];
    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    ti.hwnd = owner;
    ti.uId = (UINT_PTR)id;
    ti.lpszText = buf;
    SendMessageW(hwnd, TTM_GETTEXT, 512, (LPARAM)&ti);
    return ToUtf8Temp(buf);
}

constexpr int kMultilineInfotipWidthPx = 500;

static void SetMaxWidthForText(HWND hwnd, Str s, bool multiline) {
    int dx = -1;
    if (multiline || str::ContainsChar(s, '\n')) {
        // TODO: dpi scale
        dx = kMultilineInfotipWidthPx;
    }
    SendMessageW(hwnd, TTM_SETMAXTIPWIDTH, 0, dx);
}

static bool TooltipUpdateText(HWND hwnd, HWND owner, int id, Str s, bool multiline) {
    // avoid flickering
    TempStr s2 = TooltipGetTextTemp(hwnd, owner, id);
    if (str::Eq(s, s2)) {
        return false;
    }

    SetMaxWidthForText(hwnd, s, multiline);
    TempWStr ws = ToWStrTemp(s);
    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    ti.hwnd = owner;
    ti.uId = (UINT_PTR)id;
    ti.lpszText = (WCHAR*)ws.s;
    ti.uFlags = kTrackToolFlags;
    SendMessageW(hwnd, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
    bool isRtl = IsTextRtl(ws);
    HwndSetRtl(hwnd, isRtl);
    return true;
}

static void TooltipUpdateRect(HWND hwnd, HWND owner, int id, const Rect& rc) {
    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    ti.hwnd = owner;
    ti.uId = (UINT_PTR)id;
    ti.rect = ToRECT(rc);
    SendMessageW(hwnd, TTM_NEWTOOLRECT, 0, (LPARAM)&ti);
}

static void TooltipTrackDeactivate(HWND hwndTT, HWND owner, int id) {
    if (!hwndTT || !owner || id == 0) {
        return;
    }
    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    ti.hwnd = owner;
    ti.uId = (UINT_PTR)id;
    ti.uFlags = kTrackToolFlags;
    SendMessageW(hwndTT, TTM_TRACKACTIVATE, FALSE, (LPARAM)&ti);
}

// Tip bubble size after the tool exists (for placement under a keyboard selection).
static Size TooltipGetBubbleSize(HWND hwndTT, HWND owner, int id) {
    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    ti.hwnd = owner;
    ti.uId = (UINT_PTR)id;
    ti.uFlags = kTrackToolFlags;
    LRESULT lr = SendMessageW(hwndTT, TTM_GETBUBBLESIZE, 0, (LPARAM)&ti);
    if (!lr) {
        return {};
    }
    return {LOWORD(lr), HIWORD(lr)};
}

// TTF_ABSOLUTE disables the tooltip control's own edge-flip, so a wide
// annotation / link tip at the right (or bottom) of the screen was clipped
// (issue #6002). Shift (x,y) so a tip of size tipSz stays in the work area.
static void ClampTipPosToWorkArea(int& x, int& y, Size tipSz) {
    if (tipSz.dx <= 0 || tipSz.dy <= 0) {
        return;
    }
    HMONITOR mon = MonitorFromPoint(POINT{x, y}, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    if (!GetMonitorInfoW(mon, &mi)) {
        return;
    }
    RECT wa = mi.rcWork;
    if (x + tipSz.dx > wa.right) {
        x = wa.right - tipSz.dx;
    }
    x = std::max<LONG>(x, wa.left);
    if (y + tipSz.dy > wa.bottom) {
        y = wa.bottom - tipSz.dy;
    }
    y = std::max<LONG>(y, wa.top);
}

static void TooltipMoveOntoWorkArea(HWND hwndTT) {
    if (!hwndTT) {
        return;
    }
    RECT wr;
    if (!GetWindowRect(hwndTT, &wr)) {
        return;
    }
    int x = wr.left;
    int y = wr.top;
    Size sz{wr.right - wr.left, wr.bottom - wr.top};
    ClampTipPosToWorkArea(x, y, sz);
    if (x != wr.left || y != wr.top) {
        SetWindowPos(hwndTT, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

static void TooltipTrackActivateAtScreen(HWND hwndTT, HWND owner, int id, int screenX, int screenY) {
    if (!hwndTT || !owner || id == 0) {
        return;
    }
    SendMessageW(hwndTT, TTM_TRACKPOSITION, 0, MAKELPARAM(screenX, screenY));
    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    ti.hwnd = owner;
    ti.uId = (UINT_PTR)id;
    ti.uFlags = kTrackToolFlags;
    SendMessageW(hwndTT, TTM_TRACKACTIVATE, TRUE, (LPARAM)&ti);
    // TTM_GETBUBBLESIZE can be 0 before the first show; use the real window.
    TooltipMoveOntoWorkArea(hwndTT);
}

static void TooltipTrackActivateAtCursor(HWND hwndTT, HWND owner, int id) {
    if (!hwndTT || !owner || id == 0) {
        return;
    }
    POINT pt;
    if (!GetCursorPos(&pt)) {
        return;
    }
    // Slight offset so the tip is not under the cursor hot-spot.
    int x = pt.x + 12;
    int y = pt.y + 18;
    ClampTipPosToWorkArea(x, y, TooltipGetBubbleSize(hwndTT, owner, id));
    TooltipTrackActivateAtScreen(hwndTT, owner, id, x, y);
}

Tooltip::Tooltip() {
    kind = kindTooltip;
}

Tooltip::~Tooltip() {
    str::Free(lastText);
}

HWND Tooltip::Create(const CreateArgs& args) {
    CreateControlArgs cargs;
    cargs.className = TOOLTIPS_CLASS;
    cargs.font = args.font;
    cargs.isRtl = args.isRtl;
    // Popup tip window; do not set cargs.parent (that would force WS_CHILD).
    cargs.style = WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP;
    cargs.exStyle = WS_EX_TOPMOST;
    cargs.visible = false;

    parent = args.parent;

    ControlBase::CreateControl(cargs);
    SetDelayTime(TTDT_AUTOPOP, 32767);
    TooltipApplyColors(hwnd);
    return hwnd;
}
Size Tooltip::GetIdealSize() {
    // not used as this is top-level window
    return {100, 32};
}

void Tooltip::SetMaxWidth(int dx) {
    SendMessageW(hwnd, TTM_SETMAXTIPWIDTH, 0, dx);
}

int Tooltip::Add(Str s, const Rect& rc, bool multiline) {
    TooltipApplyColors(hwnd);
    int id = GetNextTooltipID();
    SetMaxWidthForText(hwnd, s, multiline);
    TempWStr ws = ToWStrTemp(s);
    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    ti.hwnd = parent;
    ti.uId = (UINT_PTR)id;
    ti.uFlags = kTrackToolFlags;
    ti.rect = ToRECT(rc);
    ti.lpszText = (WCHAR*)ws.s;
    BOOL ok = (BOOL)SendMessageW(hwnd, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    if (!ok) {
        logf("Tooltip::Add: TTM_ADDTOOLW failed\n");
        return -1;
    }
    bool isRtl = IsTextRtl(ws);
    HwndSetRtl(hwnd, isRtl);
    VecAppend(tooltipIds, id);
    return id;
}

TempStr Tooltip::GetTextTemp(int id) {
    return TooltipGetTextTemp(hwnd, parent, id);
}

void Tooltip::Update(int id, Str s, const Rect& rc, bool multiline) {
    TooltipUpdateText(hwnd, parent, id, s, multiline);
    TooltipUpdateRect(hwnd, parent, id, rc);
}

// this assumes we only have at most one tool per this tooltip
int Tooltip::SetSingle(Str s, const Rect& rc, bool multiline) {
    if (!hwnd || !parent) {
        return -1;
    }
    if (len(s) > 256) {
        // pathological cases make for tooltips that take too long to display
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/2814
        s = Str(ShortenStringUtf8InTheMiddleTemp(s, 250));
    }

    // Same tip text: keep the tool and leave it where it first appeared.
    // Re-tracking to the cursor made the bubble follow the mouse inside the
    // same control (home-page buttons, rich-text links, canvas infotips).
    // Compare against lastText, not TTM_GETTEXT: the latter can fail to echo
    // the string back and would then re-create the tip on every WM_SETCURSOR.
    if (len(tooltipIds) == 1 && str::Eq(s, lastText)) {
        TooltipUpdateRect(hwnd, parent, tooltipIds[0], rc);
        return tooltipIds[0];
    }

    // Different text or no tool: replace.
    Delete();
    int id = Add(s, rc, multiline);
    if (id < 0) {
        return -1;
    }
    str::ReplaceWithCopy(&lastText, s);
    TooltipTrackActivateAtCursor(hwnd, parent, id);
    return id;
}

// Like SetSingle, but pins the tip at screenPos. If maxRightScreen > 0, shifts
// left so the bubble's right edge does not pass that x (e.g. last thumbnail).
// Also keeps the bubble inside the monitor work area when size is known.
int Tooltip::SetSingleAt(Str s, const Rect& rc, Point screenPos, bool multiline, int maxRightScreen) {
    if (!hwnd || !parent) {
        return -1;
    }
    if (len(s) > 256) {
        s = Str(ShortenStringUtf8InTheMiddleTemp(s, 250));
    }

    int id = -1;
    if (len(tooltipIds) == 1 && str::Eq(s, lastText)) {
        id = tooltipIds[0];
        TooltipUpdateRect(hwnd, parent, id, rc);
    }
    if (id < 0) {
        Delete();
        id = Add(s, rc, multiline);
        if (id < 0) {
            return -1;
        }
    }
    str::ReplaceWithCopy(&lastText, s);

    Size tipSz = TooltipGetBubbleSize(hwnd, parent, id);
    int x = screenPos.x;
    int y = screenPos.y;
    if (tipSz.dx > 0) {
        if (maxRightScreen > 0 && x + tipSz.dx > maxRightScreen) {
            x = maxRightScreen - tipSz.dx;
        }
    }
    ClampTipPosToWorkArea(x, y, tipSz);
    TooltipTrackActivateAtScreen(hwnd, parent, id, x, y);
    return id;
}

int Tooltip::Count() {
    if (!hwnd) {
        return 0;
    }
    return TooltipGetCount(hwnd);
}

void Tooltip::Delete(int id) {
    (void)id;
    str::Free(lastText);
    lastText = {};
    if (!hwnd) {
        VecReset(tooltipIds);
        return;
    }
    if (len(tooltipIds) > 0) {
        TooltipTrackDeactivate(hwnd, parent, tooltipIds[0]);
    }
    SendMessageW(hwnd, TTM_POP, 0, 0);
    TooltipRemoveAll(hwnd);
    VecReset(tooltipIds);
}

// https://docs.microsoft.com/en-us/windows/win32/controls/ttm-setdelaytime
// type is: TTDT_AUTOPOP, TTDT_INITIAL, TTDT_RESHOW, TTDT_AUTOMATIC
// timeInMs is max 32767 (~32 secs)
void Tooltip::SetDelayTime(int type, int timeInMs) {
    ReportIf(!IsValidDelayType(type));
    ReportIf(timeInMs < 0);
    ReportIf(timeInMs > 32767);
    SendMessageW(hwnd, TTM_SETDELAYTIME, type, (LPARAM)timeInMs);
}
