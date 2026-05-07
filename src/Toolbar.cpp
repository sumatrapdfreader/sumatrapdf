/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern "C" {
#include <mupdf/fitz.h>
}

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include "utils/Dpi.h"
#include "utils/WinUtil.h"
#include "utils/BitManip.h"

#include "wingui/UIModels.h"

#include "Accelerators.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "FzImgReader.h"
#include "GlobalPrefs.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SumatraPDF.h"
#include "prettysumatra/BridgeDispatcher.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "resource.h"
#include "Commands.h"
#include "AppTools.h"
#include "Menu.h"
#include "SearchAndDDE.h"
#include "Toolbar.h"
#include "Translations.h"
#include "SvgIcons.h"
#include "SumatraConfig.h"
#include "Theme.h"
#include "DarkModeSubclass.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/WebView.h"

#include "utils/Log.h"

// ── DPI / Zoom helpers ──────────────────────────────────────────────────────
// Minimum icon size in logical (96-DPI) pixels.
constexpr int kMinIconSizeLogical = 14;
// Maximum icon size in logical (96-DPI) pixels. Prevents oversized icons on
// very-high-DPI displays when toolbarSize is left at the default value.
constexpr int kMaxIconSizeLogical = 48;

// Clamp an already-DPI-scaled icon size to sane bounds so the toolbar never
// looks broken at extreme zoom levels. Rounds to the nearest even number for
// clean sub-pixel rendering.
static int ClampIconSize(int sz) {
    sz = (sz + 1) & ~1;   // round to nearest even
    if (sz < kMinIconSizeLogical) sz = kMinIconSizeLogical;
    if (sz > kMaxIconSizeLogical) sz = kMaxIconSizeLogical;
    return sz;
}
// ────────────────────────────────────────────────────────────────────────────


// https://docs.microsoft.com/en-us/windows/win32/controls/toolbar-control-reference

// NOTE: These were compile-time constants.
// They are now computed per-window so they scale with the monitor DPI and
// the Windows display zoom setting (100%, 125%, 150%, 200%, ...).
static inline int ButtonSpacingX(HWND hwnd) { return DpiScale(hwnd, 4); }
static inline int TextPaddingRight(HWND hwnd) { return DpiScale(hwnd, 6); }

enum class ToolbarDensity {
    Auto,
    Normal,
    Compact,
};

static ToolbarDensity GetToolbarDensityFromEnv() {
    char buf[16] = {};
    DWORD n = GetEnvironmentVariableA("PRETTYSUMATRA_TOOLBAR_DENSITY", buf, dimof(buf));
    if (n == 0 || n >= dimof(buf)) {
        return ToolbarDensity::Auto;
    }
    if (str::EqI(buf, "compact")) {
        return ToolbarDensity::Compact;
    }
    if (str::EqI(buf, "normal")) {
        return ToolbarDensity::Normal;
    }
    return ToolbarDensity::Auto;
}

static bool IsToolbarCompactDensity(MainWindow* win) {
    auto density = GetToolbarDensityFromEnv();
    if (density == ToolbarDensity::Compact) {
        return true;
    }
    if (density == ToolbarDensity::Normal) {
        return false;
    }
    Rect rc = ClientRect(win->hwndFrame);
    // 1180 is in logical (96-DPI) pixels; DpiScale converts it to physical
    // pixels for the monitor the window is currently on, so the compact
    // layout triggers at the same visual width at every zoom level.
    int compactThreshold = DpiScale(win->hwndFrame, 1180);
    return rc.dx > 0 && rc.dx <= compactThreshold;
}

bool IsToolbarCompact(MainWindow* win) {
    return IsToolbarCompactDensity(win);
}

static bool IsPillToolbarCmd(int cmdId) {
    return cmdId == CmdZoomFitWidthAndContinuous || cmdId == CmdZoomFitPageAndSinglePage ||
           cmdId == CmdToggleTableOfContents;
}

static bool IsPillToolbarCmdActive(MainWindow* win, int cmdId) {
    if (!win) {
        return false;
    }
    if (cmdId == CmdToggleTableOfContents) {
        return win->tocVisible;
    }
    if (!win->IsDocLoaded() || !win->ctrl) {
        return false;
    }
    DisplayMode dm = win->ctrl->GetDisplayMode();
    if (cmdId == CmdZoomFitWidthAndContinuous) {
        return dm == DisplayMode::Continuous;
    }
    if (cmdId == CmdZoomFitPageAndSinglePage) {
        return dm == DisplayMode::SinglePage;
    }
    return false;
}

// those are not real commands but we refer to toolbar placeholders by command id
constexpr int PageInfoId = (int)CmdLast + 16;
constexpr int WarningMsgId = (int)CmdLast + 17;
constexpr int MoreActionsId = (int)CmdLast + 18;

static bool IsToolbarOverflowMode(MainWindow* win) {
    if (!IsToolbarCompactDensity(win)) {
        return false;
    }
    Rect rc = ClientRect(win->hwndFrame);
    // Below 980 logical pixels secondary actions collapse into the "More"
    // overflow menu. DpiScale converts to physical pixels for the current
    // monitor so the threshold is consistent across all zoom levels.
    int overflowThreshold = DpiScale(win->hwndFrame, 980);
    return rc.dx > 0 && rc.dx <= overflowThreshold;
}

static bool IsOverflowSecondaryCmd(int cmdId) {
    return cmdId == CmdPrint || cmdId == CmdRotateLeft || cmdId == CmdRotateRight;
}

static bool TryGetToolbarRectByCmd(HWND hwndToolbar, int cmdId, RECT& out) {
    RECT r{};
    LRESULT ok = SendMessageW(hwndToolbar, TB_GETRECT, cmdId, (LPARAM)&r);
    if (!ok || (r.right <= r.left) || (r.bottom <= r.top)) {
        return false;
    }
    out = r;
    return true;
}

static bool GetToolbarContentRect(HWND hwndToolbar, RECT& outRect) {
    int nButtons = (int)SendMessageW(hwndToolbar, TB_BUTTONCOUNT, 0, 0);
    if (nButtons <= 0) {
        return false;
    }
    RECT content{};
    bool haveAny = false;
    for (int i = 0; i < nButtons; i++) {
        RECT r{};
        if (!SendMessageW(hwndToolbar, TB_GETITEMRECT, i, (LPARAM)&r)) {
            continue;
        }
        if (r.right <= r.left || r.bottom <= r.top) {
            continue;
        }
        if (!haveAny) {
            content = r;
            haveAny = true;
        } else {
            content.left = std::min(content.left, r.left);
            content.top = std::min(content.top, r.top);
            content.right = std::max(content.right, r.right);
            content.bottom = std::max(content.bottom, r.bottom);
        }
    }
    if (!haveAny) {
        return false;
    }
    outRect = content;
    return true;
}

static void FillRoundRect(HDC hdc, const RECT& rc, int radius, COLORREF fillCol, COLORREF strokeCol) {
    if (radius <= 1) {
        HBRUSH fill = CreateSolidBrush(fillCol);
        HBRUSH stroke = CreateSolidBrush(strokeCol);
        FillRect(hdc, &rc, fill);
        FrameRect(hdc, &rc, stroke);
        DeleteObject(stroke);
        DeleteObject(fill);
        return;
    }
    HBRUSH fill = CreateSolidBrush(fillCol);
    HPEN pen = CreatePen(PS_SOLID, 1, strokeCol);
    HGDIOBJ oldBrush = SelectObject(hdc, fill);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(fill);
}

static void StrokeRoundRect(HDC hdc, const RECT& rc, int radius, COLORREF strokeCol) {
    if (radius <= 1) {
        HBRUSH stroke = CreateSolidBrush(strokeCol);
        FrameRect(hdc, &rc, stroke);
        DeleteObject(stroke);
        return;
    }
    HPEN pen = CreatePen(PS_SOLID, 1, strokeCol);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
}

static COLORREF LerpColor(COLORREF a, COLORREF b, int t256) {
    int ar = GetRValue(a);
    int ag = GetGValue(a);
    int ab = GetBValue(a);
    int br = GetRValue(b);
    int bg = GetGValue(b);
    int bb = GetBValue(b);
    int r = ar + ((br - ar) * t256) / 256;
    int g = ag + ((bg - ag) * t256) / 256;
    int bl = ab + ((bb - ab) * t256) / 256;
    return RGB(r, g, bl);
}

static void DrawVerticalBlend(HDC hdc, const RECT& rc, COLORREF topCol, COLORREF bottomCol) {
    int h = rc.bottom - rc.top;
    if (h <= 0 || rc.right <= rc.left) {
        return;
    }
    HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(DC_PEN));
    for (int y = 0; y < h; y++) {
        int t256 = (h <= 1) ? 256 : (y * 256) / (h - 1);
        COLORREF c = LerpColor(topCol, bottomCol, t256);
        SetDCPenColor(hdc, c);
        int yPos = rc.top + y;
        MoveToEx(hdc, rc.left, yPos, nullptr);
        LineTo(hdc, rc.right, yPos);
    }
    SelectObject(hdc, oldPen);
}

static RECT ToToolbarClientRect(HWND hwndToolbar, HWND child) {
    RECT r{};
    if (!child || !GetWindowRect(child, &r)) {
        return {0, 0, 0, 0};
    }
    MapWindowPoints(HWND_DESKTOP, hwndToolbar, (LPPOINT)&r, 2);
    return r;
}

static bool IsToolbarLightColor(COLORREF c) {
    int r = GetRValue(c);
    int g = GetGValue(c);
    int b = GetBValue(c);
    // ITU-R BT.709 luma approximation
    int luma = (2126 * r + 7152 * g + 722 * b) / 10000;
    return luma >= 160;
}

struct ToolbarVisualTokens {
    COLORREF shellShadowFill;
    COLORREF shellShadowStroke;
    COLORREF shellFill;
    COLORREF shellStroke;
    COLORREF shellTopTint;
    COLORREF groupFill;
    COLORREF groupStroke;
    COLORREF blendTop;
    COLORREF hoverFill;
    COLORREF hoverStroke;
    COLORREF pressedFill;
    COLORREF pressedStroke;
    COLORREF checkedFill;
    COLORREF checkedStroke;
    COLORREF checkedText;
    int shellRadius;
    int groupRadius;
    int buttonRadius;
    int pillRadius;
};

static ToolbarVisualTokens GetToolbarVisualTokens(MainWindow* win, COLORREF base, bool isLight) {
    ToolbarVisualTokens t{};
    if (isLight) {
        t.shellShadowFill = RGB(232, 238, 248);
        t.shellShadowStroke = RGB(220, 228, 240);
        t.shellFill = RGB(249, 251, 255);
        t.shellStroke = RGB(203, 214, 230);
        t.shellTopTint = RGB(255, 255, 255);
        t.groupFill = RGB(241, 246, 253);
        t.groupStroke = RGB(218, 227, 239);
        t.blendTop = RGB(234, 241, 251);
        t.hoverFill = RGB(232, 239, 250);
        t.hoverStroke = RGB(201, 213, 230);
        t.pressedFill = RGB(221, 232, 247);
        t.pressedStroke = RGB(182, 198, 220);
        t.checkedFill = RGB(0, 120, 212);
        t.checkedStroke = RGB(0, 95, 170);
        t.checkedText = RGB(255, 255, 255);
    } else {
        t.shellShadowFill = AccentColor(base, -8);
        t.shellShadowStroke = AccentColor(base, -4);
        t.shellFill = AccentColor(base, 10);
        t.shellStroke = AccentColor(base, 24);
        t.shellTopTint = AccentColor(base, 17);
        t.groupFill = AccentColor(base, 14);
        t.groupStroke = AccentColor(base, 28);
        t.blendTop = AccentColor(base, 20);
        t.hoverFill = AccentColor(base, 22);
        t.hoverStroke = AccentColor(base, 34);
        t.pressedFill = AccentColor(base, 17);
        t.pressedStroke = AccentColor(base, 31);
        t.checkedFill = AccentColor(base, 48);
        t.checkedStroke = AccentColor(base, 63);
        t.checkedText = RGB(255, 255, 255);
    }
    t.shellRadius = 0;
    t.groupRadius = DpiScale(win->hwndFrame, 4);
    t.buttonRadius = DpiScale(win->hwndFrame, 6);
    t.pillRadius = DpiScale(win->hwndFrame, 8);
    return t;
}

static void DrawToolbarFloatingShell(MainWindow* win, HDC hdc, HWND hwndToolbar) {
    RECT rc{};
    GetClientRect(hwndToolbar, &rc);

    RECT content{};
    if (!GetToolbarContentRect(hwndToolbar, content)) {
        return;
    }

    RECT pageRect = ToToolbarClientRect(hwndToolbar, win->hwndPageBg);
    if (pageRect.right > pageRect.left && pageRect.bottom > pageRect.top) {
        content.left = std::min(content.left, pageRect.left);
        content.top = std::min(content.top, pageRect.top);
        content.right = std::max(content.right, pageRect.right);
        content.bottom = std::max(content.bottom, pageRect.bottom);
    }
    RECT findRect = ToToolbarClientRect(hwndToolbar, win->hwndFindBg);
    if (findRect.right > findRect.left && findRect.bottom > findRect.top && IsWindowVisible(win->hwndFindBg)) {
        content.left = std::min(content.left, findRect.left);
        content.top = std::min(content.top, findRect.top);
        content.right = std::max(content.right, findRect.right);
        content.bottom = std::max(content.bottom, findRect.bottom);
    }

    bool isCompact = IsToolbarCompactDensity(win);
    int shellPadX = DpiScale(win->hwndFrame, isCompact ? 10 : 14);
    int shellPadY = DpiScale(win->hwndFrame, isCompact ? 5 : 7);
    RECT shell = {content.left - shellPadX, content.top - shellPadY, content.right + shellPadX, content.bottom + shellPadY};
    shell.left = std::max(shell.left, rc.left + DpiScale(win->hwndFrame, 8));
    shell.right = std::min(shell.right, rc.right - DpiScale(win->hwndFrame, 8));
    shell.top = std::max(shell.top, rc.top + DpiScale(win->hwndFrame, 3));
    shell.bottom = std::min(shell.bottom, rc.bottom - DpiScale(win->hwndFrame, 1));
    if (shell.right <= shell.left || shell.bottom <= shell.top) {
        return;
    }

        COLORREF base = PrettyStyleEnabled() ? PrettySurfaceAltColor() : ThemeControlBackgroundColor();
    bool isLight = IsToolbarLightColor(base);
    ToolbarVisualTokens tokens = GetToolbarVisualTokens(win, base, isLight);
    RECT shadow = shell;
    OffsetRect(&shadow, 0, DpiScale(win->hwndFrame, 1));
    FillRoundRect(hdc, shadow, tokens.shellRadius, tokens.shellShadowFill, tokens.shellShadowStroke);
    FillRoundRect(hdc, shell, tokens.shellRadius, tokens.shellFill, tokens.shellFill);

    // Top tint keeps depth subtle and avoids legacy glossy look.
    RECT gloss = shell;
    gloss.bottom = gloss.top + ((shell.bottom - shell.top) * 36) / 100;
    if (gloss.bottom > gloss.top + 1) {
        HBRUSH glossBrush = CreateSolidBrush(tokens.shellTopTint);
        FillRect(hdc, &gloss, glossBrush);
        DeleteObject(glossBrush);
    }
    StrokeRoundRect(hdc, shell, tokens.shellRadius, tokens.shellStroke);

    struct GroupRange {
        int firstCmd;
        int lastCmd;
    };
    GroupRange groups[] = {
        {CmdGoToPrevPage, PageInfoId},
        {CmdZoomOut, CmdToggleTableOfContents},
        {CmdFindFirst, CmdFindToggleMatchCase},
        {MoreActionsId, MoreActionsId},
        {CmdPrint, CmdRotateRight},
    };

    for (auto& g : groups) {
        RECT r1{}, r2{};
        if (!TryGetToolbarRectByCmd(hwndToolbar, g.firstCmd, r1) || !TryGetToolbarRectByCmd(hwndToolbar, g.lastCmd, r2)) {
            continue;
        }
        RECT gr = {r1.left - DpiScale(win->hwndFrame, isCompact ? 4 : 6), shell.top + DpiScale(win->hwndFrame, 6),
                   r2.right + DpiScale(win->hwndFrame, isCompact ? 4 : 6), shell.bottom - DpiScale(win->hwndFrame, 6)};
        gr.left = std::max(gr.left, shell.left + DpiScale(win->hwndFrame, 3));
        gr.right = std::min(gr.right, shell.right - DpiScale(win->hwndFrame, 3));
        if (gr.right <= gr.left) {
            continue;
        }
        FillRoundRect(hdc, gr, tokens.groupRadius, tokens.groupFill, tokens.groupStroke);
    }

    int blendDy = DpiScale(win->hwndFrame, isCompact ? 4 : 6);
    RECT blend = {shell.left + DpiScale(win->hwndFrame, 12), std::max(shell.bottom - DpiScale(win->hwndFrame, 1), rc.top),
                  shell.right - DpiScale(win->hwndFrame, 12), std::min(shell.bottom + blendDy, rc.bottom)};
    if (blend.bottom > blend.top && blend.right > blend.left) {
            COLORREF blendBottom = PrettyStyleEnabled() ? PrettySurfaceAltColor() : ThemeControlBackgroundColor();
        DrawVerticalBlend(hdc, blend, tokens.blendTop, blendBottom);
    }
}

struct ToolbarButtonInfo {
    /* index in the toolbar bitmap (-1 for separators) */
    TbIcon bmpIndex;
    int cmdId;
    const char* toolTip;
};

static ToolbarButtonInfo gToolbarButtons[] = {
    // group: primary action
    {TbIcon::Text, CmdOpenFile, _TRN("Open")},
    {TbIcon::None, 0, nullptr},

    // group: navigation + page indicator
    {TbIcon::PagePrev, CmdGoToPrevPage, _TRN("Previous Page")},
    {TbIcon::PageNext, CmdGoToNextPage, _TRN("Next Page")},
    {TbIcon::None, PageInfoId, nullptr}, // text box for page number + show current page / no of pages
    {TbIcon::None, 0, nullptr}, // separator

    // group: view and zoom
    {TbIcon::ZoomOut, CmdZoomOut, _TRN("Zoom Out")},
    {TbIcon::ZoomIn, CmdZoomIn, _TRN("Zoom In")},
    {TbIcon::LayoutContinuous, CmdZoomFitWidthAndContinuous, _TRN("Fit Width and Show Pages Continuously")},
    {TbIcon::LayoutSinglePage, CmdZoomFitPageAndSinglePage, _TRN("Fit a Single Page")},
    {TbIcon::Text, CmdToggleTableOfContents, _TRN("Sidebar")},
    {TbIcon::None, 0, nullptr}, // separator

    // group: search
    {TbIcon::None, CmdFindFirst, nullptr},
    {TbIcon::SearchPrev, CmdFindPrev, _TRN("Find Previous")},
    {TbIcon::SearchNext, CmdFindNext, _TRN("Find Next")},
    {TbIcon::MatchCase, CmdFindToggleMatchCase, _TRN("Toggle Match Case")},
    {TbIcon::None, 0, nullptr}, // separator

    // group: overflow trigger for small widths
    {TbIcon::Text, MoreActionsId, _TRN("More")},
    {TbIcon::None, 0, nullptr}, // separator

    // group: secondary actions
    {TbIcon::Print, CmdPrint, _TRN("Print")},
    {TbIcon::RotateLeft, CmdRotateLeft, _TRN("Rotate &Left")},
    {TbIcon::RotateRight, CmdRotateRight, _TRN("Rotate &Right")},
};
// unicode chars: https://www.compart.com/en/unicode/U+25BC

constexpr int kButtonsCount = dimof(gToolbarButtons);

// 128 should be more than enough
// we use static array so that we don't have to generate
// code for Vec<ToolbarButtonInfo>
constexpr int kMaxCustomButtons = 127;
// +1 to ensure there's always space for WarningsMsgId button
static ToolbarButtonInfo gCustomButtons[kMaxCustomButtons + 1];
int gCustomButtonsCount = 0;

static bool SkipBuiltInButton(const ToolbarButtonInfo& tbi) {
    return tbi.bmpIndex == TbIcon::None;
}

static void UpdateToolbarButtonStateByIdx(HWND hwnd, int idx, bool set, BYTE flag) {
    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_BYINDEX | TBIF_STATE;
    if (!SendMessageW(hwnd, TB_GETBUTTONINFOW, idx, (LPARAM)&bi)) {
        return;
    }
    BYTE newState = set ? (BYTE)(bi.fsState | flag) : (BYTE)(bi.fsState & ~flag);
    if (newState == bi.fsState) {
        return;
    }
    bi.fsState = newState;
    SendMessageW(hwnd, TB_SETBUTTONINFOW, idx, (LPARAM)&bi);
}

static int TotalButtonsCount() {
    return kButtonsCount + gCustomButtonsCount;
}

static ToolbarButtonInfo& GetToolbarButtonInfoByIdx(int idx) {
    if (idx < kButtonsCount) return gToolbarButtons[idx];
    return gCustomButtons[idx - kButtonsCount];
}

// more than one because users can add custom buttons with overlapping ids
static int GetToolbarButtonsByID(int cmdId, int (&buttons)[4]) {
    int nFound = 0;
    int n = TotalButtonsCount();
    int targetCmdId = cmdId;
    if (auto cmd = FindCustomCommand(targetCmdId)) {
        targetCmdId = cmd->origId;
    }
    for (int idx = 0; idx < n; idx++) {
        ToolbarButtonInfo& tb = GetToolbarButtonInfoByIdx(idx);
        int tbCmdId = tb.cmdId;
        auto cmd = FindCustomCommand(tbCmdId);
        if (cmd) tbCmdId = cmd->origId;
        if (targetCmdId != tbCmdId) continue;
        buttons[nFound++] = idx;
        if (nFound >= 4) {
            return nFound;
        }
    }
    return nFound;
}

void SetToolbarButtonCheckedState(MainWindow* win, int cmdId, bool isChecked) {
    int buttons[4];
    int n = GetToolbarButtonsByID(cmdId, buttons);
    if (n == 0) return;
    for (int i = 0; i < n; i++) {
        int idx = buttons[i];
        UpdateToolbarButtonStateByIdx(win->hwndToolbar, idx, isChecked, TBSTATE_CHECKED);
    }
}

static void TbSetButtonDx(HWND hwndToolbar, int cmd, int dx) {
    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    bi.cx = (WORD)dx;
    TbSetButtonInfoById(hwndToolbar, cmd, &bi);
}

static void TbSetButtonDxByIdx(HWND hwndToolbar, int idx, int dx) {
    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_BYINDEX | TBIF_SIZE;
    bi.cx = (WORD)dx;
    SendMessageW(hwndToolbar, TB_SETBUTTONINFOW, idx, (LPARAM)&bi);
}

// which documents support rotation
static bool NeedsRotateUI(MainWindow* win) {
    if (win->AsChm()) {
        return false;
    }
    return true;
}

static bool IsCmdAvailableWithCtx(MainWindow* win, int cmdId, BuildMenuCtx* ctx) {
    switch (cmdId) {
        case CmdZoomFitWidthAndContinuous:
        case CmdZoomFitPageAndSinglePage:
            return !win->AsChm();
        case CmdRotateLeft:
        case CmdRotateRight:
            return NeedsRotateUI(win);
        case CmdFindFirst:
        case CmdFindNext:
        case CmdFindPrev:
        case CmdFindToggleMatchCase:
            return NeedsFindUI(win);
        case PageInfoId:
        case MoreActionsId:
            return true;
    }
    auto [remove, disable] = GetCommandIdState(ctx, cmdId);
    return !remove;
}

static bool IsCmdEnabledWithCtx(MainWindow* win, int cmdId, BuildMenuCtx* ctx) {
    switch (cmdId) {
        case CmdNextTab:
        case CmdPrevTab:
        case CmdNextTabSmart:
        case CmdPrevTabSmart:
            return SettingsUseTabs();
        case PageInfoId:
        case MoreActionsId:
            return true;
    }

    auto [remove, disable] = GetCommandIdState(ctx, cmdId);
    if (remove || disable) {
        return false;
    }
    bool isAllowed = true;
    switch (cmdId) {
        case CmdOpenFile:
            isAllowed = CanAccessDisk();
            break;
        case CmdPrint:
            isAllowed = HasPermission(Perm::PrinterAccess);
            break;
    }
    if (!isAllowed) {
        return false;
    }

    // If no file open, only enable open button
    if (!win->IsDocLoaded()) {
        return CmdOpenFile == cmdId;
    }

    switch (cmdId) {
        case CmdOpenFile:
            // opening different files isn't allowed in plugin mode
            return !gPluginMode;

#ifndef DISABLE_DOCUMENT_RESTRICTIONS
        case CmdPrint:
            return !win->AsFixed() || win->AsFixed()->GetEngine()->AllowsPrinting();
#endif

        case CmdFindNext:
        case CmdFindPrev:
            // TODO: Update on whether there's more to find, not just on whether there is text.
            return HwndGetTextLen(win->hwndFindEdit) > 0;

        case CmdGoToNextPage:
            return win->ctrl->CurrentPageNo() < win->ctrl->PageCount();
        case CmdGoToPrevPage:
            return win->ctrl->CurrentPageNo() > 1;

        default:
            return true;
    }
}

static TBBUTTON TbButtonFromButtonInfo(const ToolbarButtonInfo& bi, bool noTranslate = false) {
    TBBUTTON b{};
    b.idCommand = bi.cmdId;
    if (SkipBuiltInButton(bi)) {
        b.fsStyle = BTNS_SEP;
        return b;
    }
    b.iBitmap = (int)bi.bmpIndex;
    b.fsState = TBSTATE_ENABLED;
    b.fsStyle = BTNS_BUTTON;
    if (bi.cmdId == CmdFindToggleMatchCase) {
        b.fsStyle = BTNS_CHECK;
    }
    if (bi.bmpIndex == TbIcon::Text) {
        // b.fsStyle = BTNS_DROPDOWN;
        b.fsStyle |= BTNS_SHOWTEXT;
        b.fsStyle |= BTNS_AUTOSIZE;
    }
    auto s = noTranslate ? bi.toolTip : trans::GetTranslation(bi.toolTip);
    b.iString = (INT_PTR)ToWStrTemp(s);
    return b;
}

// Set toolbar button tooltips taking current language into account.
void UpdateToolbarButtonsToolTipsForWindow(MainWindow* win) {
    TBBUTTONINFO binfo{};
    HWND hwnd = win->hwndToolbar;
    for (int i = 0; i < kButtonsCount; i++) {
        const ToolbarButtonInfo& bi = gToolbarButtons[i];
        if (!bi.toolTip) {
            continue;
        }
        if (bi.bmpIndex == TbIcon::Text) {
            continue;
        }
        const char* accelStr = AppendAccelKeyToMenuStringTemp(nullptr, bi.cmdId);
        TempStr s = (TempStr)trans::GetTranslation(bi.toolTip);
        if (accelStr) {
            TempStr s2 = str::FormatTemp(" (%s)", accelStr + 1); // +1 to skip \t
            s = str::JoinTemp(s, s2);
        }

        binfo.cbSize = sizeof(TBBUTTONINFO);
        binfo.dwMask = TBIF_TEXT | TBIF_BYINDEX;
        binfo.pszText = ToWStrTemp(s);
        WPARAM buttonId = (WPARAM)i;
        TbSetButtonInfoById(hwnd, buttonId, &binfo);
    }
    // TODO: need an explicit tooltip window https://chatgpt.com/c/18fb77c8-761c-4314-a1ac-e55b93edfeef
#if 0
    if (gCustomToolbarButtons) {
        int n = gCustomToolbarButtons->Size();
        for (int i = 0; i < n; i++) {
            const ToolbarButtonInfo& bi = gCustomToolbarButtons->At(i);
            const char* accelStr = AppendAccelKeyToMenuStringTemp(nullptr, bi.cmdId);
            TempStr s = (TempStr)bi.toolTip;
            if (accelStr) {
                TempStr s2 = str::FormatTemp(" (%s)", accelStr + 1); // +1 to skip \t
                s = str::JoinTemp(s, s2);
            }

            binfo.cbSize = sizeof(TBBUTTONINFO);
            binfo.dwMask = TBIF_TEXT | TBIF_BYINDEX;
            binfo.pszText = ToWStrTemp(s);
            WPARAM buttonId = (WPARAM)(kButtonsCount + i);
            TbSetButtonInfoById(hwnd, buttonId, &binfo);
        }
    }
#endif
}

// TODO: this is called too often
// TODO: also set checked state instead of calling SetToolbarButtonCheckedState() all over
void ToolbarUpdateStateForWindow(MainWindow* win, bool setButtonsVisibility) {
    HWND hwnd = win->hwndToolbar;
    int n = TotalButtonsCount();
    bool overflowMode = IsToolbarOverflowMode(win);
    auto ctx = NewBuildMenuCtx(win->CurrentTab(), Point{0, 0});
    AutoRun delCtx(DeleteBuildMenuCtx, ctx);
    for (int i = 0; i < n; i++) {
        auto& tb = GetToolbarButtonInfoByIdx(i);
        int cmdId = tb.cmdId;
        if (setButtonsVisibility && cmdId != WarningMsgId) {
            bool hide = !IsCmdAvailableWithCtx(win, cmdId, ctx);
            if (IsOverflowSecondaryCmd(cmdId)) {
                hide = hide || overflowMode;
            } else if (cmdId == MoreActionsId) {
                hide = hide || !overflowMode;
            }
            UpdateToolbarButtonStateByIdx(hwnd, i, hide, TBSTATE_HIDDEN);
        }
        if (SkipBuiltInButton(tb)) {
            continue;
        }
        bool isEnabled = IsCmdEnabledWithCtx(win, cmdId, ctx);
        if (cmdId == MoreActionsId) {
            bool hasAny = false;
            int overflowCmds[] = {CmdPrint, CmdRotateLeft, CmdRotateRight};
            for (int c : overflowCmds) {
                if (IsCmdAvailableWithCtx(win, c, ctx)) {
                    hasAny = true;
                    break;
                }
            }
            isEnabled = isEnabled && hasAny;
        }
        UpdateToolbarButtonStateByIdx(hwnd, i, isEnabled, TBSTATE_ENABLED);
    }

    // Find labels may have to be repositioned if some
    // toolbar buttons were shown/hidden
    if (setButtonsVisibility && NeedsFindUI(win)) {
        UpdateToolbarFindText(win);
    }

    // update dirty (unsaved annotations) flag and tooltip on each tab
    if (win->tabsCtrl) {
        int nTabs = win->TabCount();
        for (int i = 0; i < nTabs; i++) {
            WindowTab* tab = win->GetTab(i);
            bool dirty = false;
            if (tab && tab->AsFixed()) {
                dirty = EngineHasUnsavedAnnotations(tab->AsFixed()->GetEngine());
            }
            // update tooltip before SetTabDirty (which rebuilds tooltips via LayoutTabs)
            TabInfo* ti = win->tabsCtrl->GetTab(i);
            if (ti && tab && tab->filePath) {
                const char* path = tab->filePath;
                if (dirty) {
                    TempStr tooltip = str::JoinTemp(path, " ", _TRA("(unsaved annotations)"));
                    str::ReplaceWithCopy(&ti->tooltip, tooltip);
                } else {
                    str::ReplaceWithCopy(&ti->tooltip, path);
                }
            }
            win->tabsCtrl->SetTabDirty(i, dirty);
        }
    }
}

void SetToolbarButtonEnableState(MainWindow* win, int cmdId, bool isEnabled) {
    int buttons[4];
    int n = GetToolbarButtonsByID(cmdId, buttons);
    if (n == 0) return;
    for (int i = 0; i < n; i++) {
        int idx = buttons[i];
        UpdateToolbarButtonStateByIdx(win->hwndToolbar, idx, isEnabled, TBSTATE_ENABLED);
    }
}
bool ShouldShowToolbar(MainWindow* win) {
    if (!gGlobalPrefs->showToolbar) {
        return false;
    }
    if (win->presentation) {
        return false;
    }
    if (win->isFullScreen) {
        return gGlobalPrefs->fullscreen.showToolbar;
    }
    // hide toolbar on about/home page when not using tabs
    if (!SettingsUseTabs() && win->IsCurrentTabAbout()) {
        return false;
    }
    return true;
}

void ShowOrHideToolbar(MainWindow* win) {
    bool show = ShouldShowToolbar(win);
    if (show == win->isToolbarVisible) {
        return;
    }
    win->isToolbarVisible = show;
    if (!show) {
        // Move the focus out of the toolbar
        if (HwndIsFocused(win->hwndFindEdit) || HwndIsFocused(win->hwndPageEdit)) {
            HwndSetFocus(win->hwndFrame);
        }
    }
    RelayoutWindow(win);
}

static void ShowToolbarOverflowMenu(MainWindow* win);

void UpdateFindbox(MainWindow* win) {
    // remove SS_WHITERECT so WM_CTLCOLORSTATIC controls the background color
    SetWindowStyle(win->hwndFindBg, SS_WHITERECT, false);
    SetWindowStyle(win->hwndPageBg, SS_WHITERECT, false);

    InvalidateRect(win->hwndToolbar, nullptr, TRUE);
    if (IsWindowVisible(win->hwndFrame)) {
        UpdateWindow(win->hwndToolbar);
    }

    auto cursorId = win->IsDocLoaded() ? IDC_IBEAM : IDC_ARROW;
    SetClassLongPtrW(win->hwndFindEdit, GCLP_HCURSOR, (LONG_PTR)GetCachedCursor(cursorId));
    if (!win->IsDocLoaded()) {
        // avoid focus on Find box
        HideCaret(nullptr);
    } else {
        ShowCaret(nullptr);
    }
}

LRESULT CALLBACK ReBarWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass,
                              DWORD_PTR dwRefData) {
    if (WM_ERASEBKGND == uMsg && ThemeColorizeControls()) {
        HDC hdc = (HDC)wParam;
        RECT rect;
        GetClientRect(hWnd, &rect);
        SetTextColor(hdc, ThemeWindowTextColor());
        COLORREF bgCol = ThemeControlBackgroundColor();
        SetBkColor(hdc, bgCol);
        auto bgBrush = CreateSolidBrush(bgCol);
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush);
        return 1;
    }
    if (WM_NOTIFY == uMsg) {
        auto win = FindMainWindowByHwnd(hWnd);
        NMHDR* hdr = (NMHDR*)lParam;
        HWND chwnd = hdr->hwndFrom;
        if (hdr->code == NM_CUSTOMDRAW) {
            if (win && win->hwndToolbar == chwnd) {
                NMTBCUSTOMDRAW* custDraw = (NMTBCUSTOMDRAW*)hdr;
                switch (custDraw->nmcd.dwDrawStage) {
                    case CDDS_PREPAINT:
                        DrawToolbarFloatingShell(win, custDraw->nmcd.hdc, chwnd);
                        return CDRF_NOTIFYITEMDRAW | TBCDRF_USECDCOLORS;

                    case CDDS_ITEMPREPAINT: {
                        auto col = ThemeWindowTextColor();
                        // col = RGB(255, 0, 0);
                        // SetTextColor(custDraw->nmcd.hdc, col);
                        UINT itemState = custDraw->nmcd.uItemState;
                        if (itemState & CDIS_DISABLED) {
                            // TODO: this doesn't work
                            col = ThemeWindowTextDisabledColor();
                            // col = RGB(255, 0, 0);
                            custDraw->clrText = col;
                        } else if (false && itemState & CDIS_SELECTED) {
                            custDraw->clrText = RGB(0, 255, 0);
                        } else if (false && itemState & CDIS_GRAYED) {
                            custDraw->clrText = RGB(0, 0, 255);
                        } else {
                            custDraw->clrText = col;
                        }
                        int cmdId = (int)custDraw->nmcd.dwItemSpec;
                        bool isDisabled = (itemState & CDIS_DISABLED) != 0;
                        bool isHot = (itemState & CDIS_HOT) != 0;
                        bool isPressed = (itemState & CDIS_SELECTED) != 0;
                        bool isChecked = (itemState & CDIS_CHECKED) != 0;
                        bool isPill = IsPillToolbarCmd(cmdId);
                        if (!isDisabled && (isPill || isHot || isPressed || isChecked)) {
                            RECT rc = custDraw->nmcd.rc;
                            int insetX = DpiScale(win->hwndFrame, 2);
                            int insetY = DpiScale(win->hwndFrame, 3);
                            InflateRect(&rc, -insetX, -insetY);

                            bool visualActive = isPill ? IsPillToolbarCmdActive(win, cmdId) : isChecked || isPressed;
                            COLORREF base = ThemeControlBackgroundColor();
                            bool isLight = IsToolbarLightColor(base);
                            ToolbarVisualTokens tokens = GetToolbarVisualTokens(win, base, isLight);
                            COLORREF fillCol = tokens.hoverFill;
                            COLORREF strokeCol = tokens.hoverStroke;
                            if (isPressed && !visualActive) {
                                fillCol = tokens.pressedFill;
                                strokeCol = tokens.pressedStroke;
                            }
                            if (visualActive) {
                                fillCol = tokens.checkedFill;
                                strokeCol = tokens.checkedStroke;
                            }
                            if (isHot || visualActive || isPressed) {
                                HBRUSH fill = CreateSolidBrush(fillCol);
                                HPEN pen = CreatePen(PS_SOLID, 1, strokeCol);
                                HGDIOBJ oldBrush = SelectObject(custDraw->nmcd.hdc, fill);
                                HGDIOBJ oldPen = SelectObject(custDraw->nmcd.hdc, pen);
                                int radius = isPill ? tokens.pillRadius : tokens.buttonRadius;
                                RoundRect(custDraw->nmcd.hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
                                SelectObject(custDraw->nmcd.hdc, oldPen);
                                SelectObject(custDraw->nmcd.hdc, oldBrush);
                                DeleteObject(pen);
                                DeleteObject(fill);
                            }
                            custDraw->clrText = visualActive ? tokens.checkedText : ThemeWindowTextColor();
                            return TBCDRF_NOEDGES | TBCDRF_NOOFFSET | TBCDRF_NOETCHEDEFFECT;
                        }
                        return TBCDRF_NOEDGES | TBCDRF_NOOFFSET | TBCDRF_NOETCHEDEFFECT;
                        // return CDRF_NEWFONT;
                    }
                }
            }
        }
    }
    if (WM_COMMAND == uMsg) {
        int cmdId = LOWORD(wParam);
        if (cmdId == MoreActionsId) {
            auto win = FindMainWindowByHwnd(hWnd);
            ShowToolbarOverflowMenu(win);
            return 0;
        }
    }
    // allow window dragging from empty rebar area (main toolbar)
    if (WM_LBUTTONDOWN == uMsg) {
        auto win = FindMainWindowByHwnd(hWnd);
        if (win && win->tabsInTitlebar) {
            HWND hwndFrame = GetAncestor(hWnd, GA_ROOT);
            ReleaseCapture();
            SendMessageW(hwndFrame, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }
    }
    if (WM_LBUTTONDBLCLK == uMsg) {
        auto win = FindMainWindowByHwnd(hWnd);
        if (win && win->tabsInTitlebar) {
            HWND hwndFrame = GetAncestor(hWnd, GA_ROOT);
            WPARAM cmd = IsZoomed(hwndFrame) ? SC_RESTORE : SC_MAXIMIZE;
            PostMessageW(hwndFrame, WM_SYSCOMMAND, cmd, 0);
            return 0;
        }
    }
    if (WM_NCDESTROY == uMsg) {
        RemoveWindowSubclass(hWnd, ReBarWndProc, uIdSubclass);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static WNDPROC DefWndProcEditBg = nullptr;
static LRESULT CALLBACK WndProcEditBg(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    LRESULT res = CallWindowProc(DefWndProcEditBg, hwnd, msg, wp, lp);
    if (msg == WM_PAINT) {
        HDC hdc = GetDC(hwnd);
        RECT rc;
        GetClientRect(hwnd, &rc);
        MainWindow* win = FindMainWindowByHwnd(hwnd);
        if (win) {
            COLORREF base = PrettyStyleEnabled() ? PrettySurfaceColor() : ThemeControlBackgroundColor();
            bool isLight = IsToolbarLightColor(base);
            ToolbarVisualTokens tokens = GetToolbarVisualTokens(win, base, isLight);
            COLORREF fillCol = isLight ? RGB(252, 254, 255) : AccentColor(base, 13);
            COLORREF strokeCol = isLight ? RGB(194, 207, 226) : AccentColor(base, 33);
            int radius = DpiScale(win->hwndFrame, 7);
            if (ThemeColorizeControls()) {
                fillCol = LerpColor(tokens.shellFill, fillCol, 112);
            }
            FillRoundRect(hdc, rc, radius, fillCol, strokeCol);
        }
        ReleaseDC(hwnd, hdc);
    }
    return res;
}

static WNDPROC DefWndProcToolbar = nullptr;
static LRESULT CALLBACK WndProcToolbar(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_CTLCOLORSTATIC == msg || WM_CTLCOLOREDIT == msg) {
        HWND hwndCtrl = (HWND)lp;
        HDC hdc = (HDC)wp;
        MainWindow* win = FindMainWindowByHwnd(hwndCtrl);
        if (!win) {
            return CallWindowProc(DefWndProcToolbar, hwnd, msg, wp, lp);
        }
        {
            bool isBgCtrl = (win->hwndFindBg == hwndCtrl || win->hwndPageBg == hwndCtrl);
            bool isEditCtrl = (win->hwndFindEdit == hwndCtrl || win->hwndPageEdit == hwndCtrl);
            SetTextColor(hdc, ThemeWindowTextColor());
            SetBkMode(hdc, TRANSPARENT);
            if ((isBgCtrl || isEditCtrl) && !ThemeColorizeControls()) {
                SetBkColor(hdc, RGB(0xff, 0xff, 0xff));
                return (LRESULT)GetStockObject(WHITE_BRUSH);
            }
            return (LRESULT)win->brControlBgColor;
        }
    }

    if (WM_COMMAND == msg) {
        HWND hEdit = (HWND)lp;
        MainWindow* win = FindMainWindowByHwnd(hEdit);
        // "find as you type" - skip if edit was not modified by user (e.g. programmatic text set)
        if (EN_UPDATE == HIWORD(wp) && hEdit == win->hwndFindEdit && gGlobalPrefs->showToolbar) {
            bool wasModified = Edit_GetModify(win->hwndFindEdit);
            if (wasModified) {
                FindTextOnThread(win, TextSearch::Direction::Forward, false);
            }
        }
    }

    // allow window dragging from empty toolbar areas
    if (WM_LBUTTONDOWN == msg || WM_LBUTTONDBLCLK == msg) {
        MainWindow* win = FindMainWindowByHwnd(hwnd);
        if (win && win->tabsInTitlebar) {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            int idx = (int)SendMessageW(hwnd, TB_HITTEST, 0, (LPARAM)&pt);
            if (idx < 0) {
                // also check we're not over a child control (find box, page box)
                POINT ptScreen = pt;
                ClientToScreen(hwnd, &ptScreen);
                HWND childAtPoint = ChildWindowFromPoint(hwnd, pt);
                if (!childAtPoint || childAtPoint == hwnd) {
                    HWND hwndFrame = GetAncestor(hwnd, GA_ROOT);
                    if (WM_LBUTTONDBLCLK == msg) {
                        WPARAM cmd = IsZoomed(hwndFrame) ? SC_RESTORE : SC_MAXIMIZE;
                        PostMessageW(hwndFrame, WM_SYSCOMMAND, cmd, 0);
                    } else {
                        ReleaseCapture();
                        SendMessageW(hwndFrame, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                    }
                    return 0;
                }
            }
        }
    }

    return CallWindowProc(DefWndProcToolbar, hwnd, msg, wp, lp);
}

static void ShowToolbarOverflowMenu(MainWindow* win) {
    if (!win || !win->hwndToolbar) {
        return;
    }

    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    auto ctx = NewBuildMenuCtx(win->CurrentTab(), Point{0, 0});
    AutoRun delCtx(DeleteBuildMenuCtx, ctx);

    struct MenuItem {
        int cmdId;
        const char* label;
    };
    MenuItem items[] = {
        {CmdPrint, _TRN("Print")},
        {CmdRotateLeft, _TRN("Rotate &Left")},
        {CmdRotateRight, _TRN("Rotate &Right")},
    };

    bool hasItems = false;
    for (auto& item : items) {
        if (!IsCmdAvailableWithCtx(win, item.cmdId, ctx)) {
            continue;
        }
        UINT flags = MF_STRING;
        if (!IsCmdEnabledWithCtx(win, item.cmdId, ctx)) {
            flags |= MF_GRAYED;
        }
        AppendMenuW(menu, flags, (UINT_PTR)item.cmdId, ToWStrTemp(trans::GetTranslation(item.label)));
        hasItems = true;
    }

    if (!hasItems) {
        DestroyMenu(menu);
        return;
    }

    RECT rect{};
    if (!TryGetToolbarRectByCmd(win->hwndToolbar, MoreActionsId, rect)) {
        DestroyMenu(menu);
        return;
    }
    MapWindowPoints(win->hwndToolbar, HWND_DESKTOP, (POINT*)&rect, 2);

    UINT flags = TPM_TOPALIGN | TPM_RETURNCMD;
    if (IsUIRtl()) {
        flags |= TPM_RIGHTALIGN;
    } else {
        flags |= TPM_LEFTALIGN;
    }
    UINT_PTR selected = TrackPopupMenu(menu, flags, rect.left, rect.bottom, 0, win->hwndFrame, nullptr);
    DestroyMenu(menu);

    if (selected != 0) {
        PostMessageW(win->hwndFrame, WM_COMMAND, selected, 0);
    }
}

static WNDPROC DefWndProcEditSearch = nullptr;
static LRESULT CALLBACK WndProcEditSearch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win || !win->IsDocLoaded()) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    if (ExtendedEditWndProc(hwnd, msg, wp, lp)) {
        // select the whole find box on a non-selecting click
    } else if (WM_CHAR == msg) {
        switch (wp) {
            case VK_ESCAPE:
                if (win->findThread) {
                    AbortFinding(win, true);
                } else {
                    HwndSetFocus(win->hwndFrame);
                }
                return 1;

            case VK_RETURN: {
                if (IsShiftPressed()) {
                    FindPrev(win);
                } else {
                    FindNext(win);
                }
                return 1;
            }

            case VK_TAB:
                AdvanceFocus(win);
                return 1;
        }
    } else if (WM_ERASEBKGND == msg) {
        RECT r;
        Edit_GetRect(hwnd, &r);
        if (r.left == 0 && r.top == 0) { // virgin box
            r.left += 4;
            r.top += 3;
            r.bottom += 3;
            r.right -= 2;
            Edit_SetRectNoPaint(hwnd, &r);
        }
    } else if (WM_KEYDOWN == msg) {
        // TODO: if user re-binds F3 it'll not be picked up
        // we would have to either run accelerators after
        if (wp == VK_F3) {
            auto searchDir = IsShiftPressed() ? TextSearch::Direction::Backward : TextSearch::Direction::Forward;
            FindTextOnThread(win, searchDir, true);
            // Note: we don't return but let default processing take place
        }
        // TODO: here we used to call FrameOnKeydown() to make keys
        // like pageup etc. work even when focus is in text field
        // that no longer works because we moved most keys handling
        // to accelerators and we don't want to process acceleratos
        // while in edit control.
        // We could try to manually run accelerators but only if they
        // are virtual and don't prevent edit control from working
        // or maybe explicitly forword built-in accelerator for
        // white-listed shortucts but only if they were not modified by the user
    }

    LRESULT ret = CallWindowProc(DefWndProcEditSearch, hwnd, msg, wp, lp);

    // TOOD: why do we do it? re-eneable when we notice what breaks
    // the intent seems to be "after content of edit box changed"
    // but how does that afect state of the toolbar?

#if 0
    switch (msg) {
        case WM_CHAR:
        case WM_PASTE:
        case WM_CUT:
        case WM_CLEAR:
        case WM_UNDO:
        case WM_KEYUP:
            ToolbarUpdateStateForWindow(win, false);
            break;
    }
#endif

    return ret;
}

void UpdateToolbarFindText(MainWindow* win) {
    if (!win->hwndToolbar) {
        return;
    }
    if (!IsWindowVisible(win->hwndFrame)) {
        HwndSetVisibility(win->hwndFindLabel, false);
        HwndSetVisibility(win->hwndFindBg, false);
        HwndSetVisibility(win->hwndFindEdit, false);
        return;
    }
    bool showUI = NeedsFindUI(win);
    HwndSetVisibility(win->hwndFindLabel, false);
    HwndSetVisibility(win->hwndFindBg, showUI);
    HwndSetVisibility(win->hwndFindEdit, showUI);
    if (!showUI) {
        return;
    }

    const char* text = "";
    HwndSetText(win->hwndFindLabel, text);

    Rect findWndRect = WindowRect(win->hwndFindBg);

    RECT r{};
    TbGetRectById(win->hwndToolbar, CmdFindFirst, &r);
    int currX = r.left + DpiScale(win->hwndToolbar, 6);
    int currY = (r.bottom - findWndRect.dy) / 2;

    Size size = HwndMeasureText(win->hwndFindLabel, text);
    size.dx += DpiScale(win->hwndFrame, TextPaddingRight(win->hwndFrame));
    size.dx += DpiScale(win->hwndFrame, ButtonSpacingX(win->hwndFrame));

    int padding = GetSystemMetrics(SM_CXEDGE);
    int x = currX;
    int y = (findWndRect.dy - size.dy + 1) / 2 + currY;
    MoveWindow(win->hwndFindLabel, x, y, size.dx, size.dy, TRUE);
    x = currX + size.dx;
    y = currY;
    MoveWindow(win->hwndFindBg, x, y, findWndRect.dx, findWndRect.dy, FALSE);
    x = currX + size.dx + padding;
    y = (findWndRect.dy - size.dy + 1) / 2 + currY;
    int dx = findWndRect.dx - 2 * padding;
    MoveWindow(win->hwndFindEdit, x, y, dx, size.dy, FALSE);

    dx = size.dx + findWndRect.dx + 12;
    TbSetButtonDx(win->hwndToolbar, CmdFindFirst, dx);
}

void UpdateToolbarState(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }

    if (prettysumatra::bridge::HasHybridToolbar(win->hwndFrame) && win->ctrl) {
        prettysumatra::bridge::SyncHybridToolbarPageState(win->hwndFrame, win->ctrl->CurrentPageNo(), win->ctrl->PageCount());
        prettysumatra::bridge::SyncHybridToolbarZoomState(win->hwndFrame, win->ctrl->GetZoomVirtual(true));
    }

    HWND hwnd = win->hwndToolbar;
    DisplayMode dm = win->ctrl->GetDisplayMode();
    if (dm != DisplayMode::SinglePage) {
        win->CurrentTab()->prevZoomVirtual = kInvalidZoom;
    }
}

static void CreateFindBox(MainWindow* win, HFONT hfont, int iconDy) {
    bool isRtl = IsUIRtl();
    // The probe string determines the initial width of the find box.
    // HwndMeasureText uses the already-DPI-scaled hfont, so findBoxDx is
    // already in physical pixels for the current monitor - no extra DpiScale().
    const char* probe = IsToolbarCompactDensity(win) ? "story of" : "this is a story of my";
    int findBoxDx = HwndMeasureText(win->hwndFrame, probe, hfont).dx;
    HMODULE hmod = GetModuleHandleW(nullptr);
    HWND p = win->hwndToolbar;
    DWORD style = WS_VISIBLE | WS_CHILD;
    DWORD exStyle = 0;
    if (isRtl) exStyle |= WS_EX_LAYOUTRTL;
    int dy = iconDy + 2;
    // Size textSize = HwndMeasureText(win->hwndFrame, L"M", hfont);
    HWND findBg =
        CreateWindowEx(exStyle, WC_STATIC, L"", style, 0, 1, findBoxDx, dy, p, (HMENU) nullptr, hmod, nullptr);

    if (!DefWndProcEditBg) {
        DefWndProcEditBg = (WNDPROC)GetWindowLongPtr(findBg, GWLP_WNDPROC);
    }
    SetWindowLongPtr(findBg, GWLP_WNDPROC, (LONG_PTR)WndProcEditBg);

    style = WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL;
    // dy = iconDy + DpiScale(win->hwndFrame, 2);
    dy = iconDy;
    exStyle = 0;
    if (isRtl) exStyle |= WS_EX_LAYOUTRTL;
    HWND find = CreateWindowExW(exStyle, WC_EDIT, L"", style, 0, 1, findBoxDx, dy, p, (HMENU) nullptr, hmod, nullptr);

    style = WS_VISIBLE | WS_CHILD;
    exStyle = 0;
    if (isRtl) exStyle |= WS_EX_LAYOUTRTL;
    HWND label = CreateWindowExW(exStyle, WC_STATIC, L"", style, 0, 1, 0, 0, p, (HMENU) nullptr, hmod, nullptr);

    SetWindowFont(label, hfont, FALSE);
    SetWindowFont(find, hfont, FALSE);

    if (!DefWndProcToolbar) {
        DefWndProcToolbar = (WNDPROC)GetWindowLongPtr(win->hwndToolbar, GWLP_WNDPROC);
    }
    SetWindowLongPtr(win->hwndToolbar, GWLP_WNDPROC, (LONG_PTR)WndProcToolbar);

    if (!DefWndProcEditSearch) {
        DefWndProcEditSearch = (WNDPROC)GetWindowLongPtr(find, GWLP_WNDPROC);
    }
    SetWindowLongPtr(find, GWLP_WNDPROC, (LONG_PTR)WndProcEditSearch);

    win->hwndFindLabel = label;
    win->hwndFindEdit = find;
    win->hwndFindBg = findBg;
}

static WNDPROC DefWndProcPageBox = nullptr;
static LRESULT CALLBACK WndProcPageBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win || !win->IsDocLoaded()) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    if (ExtendedEditWndProc(hwnd, msg, wp, lp)) {
        // select the whole page box on a non-selecting click
    } else if (WM_CHAR == msg) {
        switch (wp) {
            case VK_RETURN: {
                char* s = HwndGetTextTemp(win->hwndPageEdit);
                int newPageNo = win->ctrl->GetPageByLabel(s);
                if (win->ctrl->ValidPageNo(newPageNo)) {
                    win->ctrl->GoToPage(newPageNo, true);
                    HwndSetFocus(win->hwndFrame);
                }
                return 1;
            }
            case VK_ESCAPE:
                HwndSetFocus(win->hwndFrame);
                return 1;

            case VK_TAB:
                AdvanceFocus(win);
                return 1;
        }
    } else if (WM_ERASEBKGND == msg) {
        RECT r;
        Edit_GetRect(hwnd, &r);
        if (r.left == 0 && r.top == 0) { // virgin box
            r.left += 4;
            r.top += 3;
            r.bottom += 3;
            r.right -= 2;
            Edit_SetRectNoPaint(hwnd, &r);
        }
    } else if (WM_KEYDOWN == msg) {
        // TODO: see WndProcEditSearch for note on enabling accelerators here as well
    }

    return CallWindowProc(DefWndProcPageBox, hwnd, msg, wp, lp);
}

void UpdateToolbarPageText(MainWindow* win, int pageCount, bool updateOnly) {
    if (!win->hwndToolbar) {
        return;
    }
    const char* text = "";
    if (!updateOnly) {
        HwndSetText(win->hwndPageLabel, text);
    }
    HwndSetVisibility(win->hwndPageLabel, false);
    int padX = DpiScale(win->hwndFrame, TextPaddingRight(win->hwndFrame));
    Size size = HwndMeasureText(win->hwndPageLabel, text);
    size.dx += padX;
    size.dx += DpiScale(win->hwndFrame, ButtonSpacingX(win->hwndFrame));

    Rect pageWndRect = WindowRect(win->hwndPageBg);

    RECT r{};
    TbGetRectById(win->hwndToolbar, PageInfoId, &r);
    int currX = r.left + DpiScale(win->hwndFrame, 4);
    int currY = (r.bottom - pageWndRect.dy) / 2;

    TempStr txt = nullptr;
    Size size2;
    Size minSize = HwndMeasureText(win->hwndPageTotal, "999 / 999");
    minSize.dx += padX;
    int labelDx = 0;
    if (-1 == pageCount) {
#if 0
        // preserve hwndPageTotal's text and size
        txt = HwndGetTextTemp(win->hwndPageTotal);
        size2 = ClientRect(win->hwndPageTotal).Size();
        size2.dx -= padX;
        size2.dx -= DpiScale(win->hwndFrame, ButtonSpacingX(win->hwndFrame));
#endif
        // hack: https://github.com/sumatrapdfreader/sumatrapdf/issues/4475
        txt = (TempStr) " ";
        minSize.dx = 0;
        size2.dx = 0;
    } else if (!pageCount) {
        // hack: https://github.com/sumatrapdfreader/sumatrapdf/issues/4475
        txt = (TempStr) " ";
        minSize.dx = 0;
        size2.dx = 0;
    } else if (!win->ctrl || !win->ctrl->HasPageLabels()) {
        txt = str::FormatTemp(" / %d", pageCount);
        size2 = HwndMeasureText(win->hwndPageTotal, txt);
        minSize.dx = size2.dx;
    } else {
        txt = str::FormatTemp("%d / %d", win->ctrl->CurrentPageNo(), pageCount);
        // TempStr txt2 = str::FormatTemp(" (%d / %d)", pageCount, pageCount);
        size2 = HwndMeasureText(win->hwndPageTotal, txt);
    }
    labelDx = size2.dx;
    size2.dx = std::max(size2.dx, minSize.dx);

    HwndSetText(win->hwndPageTotal, txt);
    if (0 == size2.dx) {
        size2 = HwndMeasureText(win->hwndPageTotal, txt);
    }
    size2.dx += padX;
    size2.dx += DpiScale(win->hwndFrame, ButtonSpacingX(win->hwndFrame));

    int padding = GetSystemMetrics(SM_CXEDGE);
    int x = currX - 1;
    int y = (pageWndRect.dy - size.dy + 1) / 2 + currY;
    MoveWindow(win->hwndPageLabel, x, y, size.dx, size.dy, FALSE);
    if (IsUIRtl()) {
        currX += size2.dx;
        currX -= padX;
        currX -= DpiScale(win->hwndFrame, ButtonSpacingX(win->hwndFrame));
    }
    x = currX + size.dx;
    y = currY;
    MoveWindow(win->hwndPageBg, x, y, pageWndRect.dx, pageWndRect.dy, FALSE);
    x = currX + size.dx + padding;
    y = (pageWndRect.dy - size.dy + 1) / 2 + currY;
    int dx = pageWndRect.dx - 2 * padding;
    MoveWindow(win->hwndPageEdit, x, y, dx, size.dy, FALSE);
    // in right-to-left layout, the total comes "before" the current page number
    if (IsUIRtl()) {
        currX -= size2.dx;
        x = currX + size.dx;
        y = (pageWndRect.dy - size.dy + 1) / 2 + currY;
        MoveWindow(win->hwndPageTotal, x, y, size2.dx, size.dy, FALSE);
    } else {
        x = currX + size.dx + pageWndRect.dx;
        int midX = (size2.dx - labelDx) / 2;
        y = (pageWndRect.dy - size.dy + 1) / 2 + currY;
        MoveWindow(win->hwndPageTotal, x + midX, y, labelDx, size.dy, FALSE);
    }

    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    SendMessageW(win->hwndToolbar, TB_GETBUTTONINFO, PageInfoId, (LPARAM)&bi);
    size2.dx += size.dx + pageWndRect.dx + 12;
    if (bi.cx != size2.dx || !updateOnly) {
        TbSetButtonDx(win->hwndToolbar, PageInfoId, size2.dx);
    }
    InvalidateRect(win->hwndToolbar, nullptr, TRUE);

    if (prettysumatra::bridge::HasHybridToolbar(win->hwndFrame) && win->ctrl) {
        int total = pageCount;
        if (total <= 0) {
            total = win->ctrl->PageCount();
        }
        prettysumatra::bridge::SyncHybridToolbarPageState(win->hwndFrame, win->ctrl->CurrentPageNo(), total);
        prettysumatra::bridge::SyncHybridToolbarZoomState(win->hwndFrame, win->ctrl->GetZoomVirtual(true));
    }
}

static void CreatePageBox(MainWindow* win, HFONT font, int iconDy) {
    bool isRtl = IsUIRtl();

    auto hwndFrame = win->hwndFrame;
    auto hwndToolbar = win->hwndToolbar;
    // TODO: this is broken, result is way too small
    int boxWidth = HwndMeasureText(hwndFrame, "999999", font).dx;
    DWORD style = WS_VISIBLE | WS_CHILD;
    auto h = GetModuleHandle(nullptr);
    int dx = boxWidth;
    int dy = iconDy + 2;
    DWORD exStyle = 0;
    if (isRtl) exStyle |= WS_EX_LAYOUTRTL;

    HWND pageBg =
        CreateWindowExW(exStyle, WC_STATICW, L"", style, 0, 1, dx, dy, hwndToolbar, (HMENU) nullptr, h, nullptr);
    SetWindowLongPtr(pageBg, GWLP_WNDPROC, (LONG_PTR)WndProcEditBg);
    HWND label = CreateWindowExW(0, WC_STATICW, L"", style, 0, 1, 0, 0, hwndToolbar, (HMENU) nullptr, h, nullptr);
    HWND total = CreateWindowExW(0, WC_STATICW, L"", style, 0, 1, 0, 0, hwndToolbar, (HMENU) nullptr, h, nullptr);

    style = WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_NUMBER | ES_RIGHT;
    dx = boxWidth - DpiScale(hwndFrame, 4); // 4 pixels padding on the right side of the text box
    dy = iconDy;
    exStyle = 0;
    if (isRtl) exStyle |= WS_EX_LAYOUTRTL;
    HWND page = CreateWindowExW(exStyle, WC_EDIT, L"0", style, 0, 1, dx, dy, hwndToolbar, (HMENU) nullptr, h, nullptr);

    SetWindowFont(label, font, FALSE);
    SetWindowFont(page, font, FALSE);
    SetWindowFont(total, font, FALSE);

    if (!DefWndProcPageBox) {
        DefWndProcPageBox = (WNDPROC)GetWindowLongPtr(page, GWLP_WNDPROC);
    }
    SetWindowLongPtr(page, GWLP_WNDPROC, (LONG_PTR)WndProcPageBox);

    win->hwndPageLabel = label;
    win->hwndPageEdit = page;
    win->hwndPageBg = pageBg;
    win->hwndPageTotal = total;
}

void LogBitmapInfo(HBITMAP hbmp) {
    BITMAP bmpInfo;
    GetObject(hbmp, sizeof(BITMAP), &bmpInfo);
    logf("dx: %d, dy: %d, stride: %d, bitsPerPixel: %d\n", (int)bmpInfo.bmWidth, (int)bmpInfo.bmHeight,
         (int)bmpInfo.bmWidthBytes, (int)bmpInfo.bmBitsPixel);
    u8* bits = (u8*)bmpInfo.bmBits;
    u8* d;
    for (int y = 0; y < 5; y++) {
        d = bits + (size_t)bmpInfo.bmWidthBytes * y;
        logf("y: %d, d: 0x%p\n", y, d);
    }
}

static void BlitPixmap(u8* dstSamples, ptrdiff_t dstStride, fz_pixmap* src, int dstX, int dstY, COLORREF bgCol) {
    int dx = src->w;
    int dy = src->h;
    int srcN = src->n;
    int dstN = 4;
    auto srcStride = src->stride;
    u8 r, g, b;
    UnpackColor(bgCol, r, g, b);
    for (size_t y = 0; y < (size_t)dy; y++) {
        u8* s = src->samples + (srcStride * (size_t)y);
        size_t atY = y + (size_t)dstY;
        u8* d = dstSamples + (dstStride * atY) + ((size_t)dstX * dstN);
        for (int x = 0; x < dx; x++) {
            bool isTransparent = (s[0] == r) && (s[1] == g) && (s[2] == b);
            // note: we're swapping red and green channel because src is rgb
            // and we want bgr for Toolbar's IMAGELIST
            d[0] = s[2];
            d[1] = s[1];
            d[2] = s[0];
            if (isTransparent) {
                d[3] = 0;
            } else {
                d[3] = 0xff;
            }
            d += dstN;
            s += srcN;
        }
    }
}

static HBITMAP BuildIconsBitmap(int dx, int dy) {
    fz_context* ctx = fz_new_context_windows();
    int nIcons = (int)TbIcon::kMax;
    int destDx = dx * nIcons;
    ptrdiff_t dstStride;

    u8* hbmpData = nullptr;
    HBITMAP hbmp;
    {
        int w = destDx;
        int h = dy;
        int n = 4;
        dstStride = destDx * n;
        int imgSize = (int)dstStride * h;
        int bitsCount = n * 8;

        size_t bmiSize = sizeof(BITMAPINFO) + 255 * sizeof(RGBQUAD);
        auto bmi = (BITMAPINFO*)calloc(1, bmiSize);
        defer {
            free(bmi);
        };
        BITMAPINFOHEADER* bmih = &bmi->bmiHeader;
        bmih->biSize = sizeof(*bmih);
        bmih->biWidth = w;
        bmih->biHeight = -h;
        bmih->biPlanes = 1;
        bmih->biCompression = BI_RGB;
        bmih->biBitCount = bitsCount;
        bmih->biSizeImage = imgSize;
        bmih->biClrUsed = 0;
        HANDLE hFile = INVALID_HANDLE_VALUE;
        DWORD fl = PAGE_READWRITE;
        HANDLE hMap = CreateFileMappingW(hFile, nullptr, fl, 0, imgSize, nullptr);
        uint usage = DIB_RGB_COLORS;
        hbmp = CreateDIBSection(nullptr, bmi, usage, (void**)&hbmpData, hMap, 0);
    }

    COLORREF fgCol = ThemeWindowTextColor();
    COLORREF bgCol = ThemeControlBackgroundColor();
    for (int i = 0; i < nIcons; i++) {
        const char* svgData = GetSvgIcon((TbIcon)i);
        TempStr strokeCol = SerializeColorTemp(fgCol);
        TempStr fillCol = SerializeColorTemp(bgCol);
        TempStr fillColRepl = str::JoinTemp("fill=\"", fillCol, "\""); // fill="${col}"
        svgData = str::ReplaceTemp(svgData, "currentColor", strokeCol);
        svgData = str::ReplaceTemp(svgData, R"(fill="none")", fillColRepl);
        fz_buffer* buf = fz_new_buffer_from_copied_data(ctx, (u8*)svgData, str::Len(svgData));
        fz_image* image = fz_new_image_from_svg(ctx, buf, nullptr, nullptr);
        image->w = dx;
        image->h = dy;
        fz_pixmap* pixmap = fz_get_pixmap_from_image(ctx, image, nullptr, nullptr, nullptr, nullptr);
        BlitPixmap(hbmpData, dstStride, pixmap, dx * i, 0, bgCol);
        fz_drop_pixmap(ctx, pixmap);
        fz_drop_image(ctx, image);
        fz_drop_buffer(ctx, buf);
    }

    fz_drop_context_windows(ctx);
    return hbmp;
}

constexpr int kDefaultIconSize = 18;

static int SetToolbarIconsImageList(MainWindow* win) {
    HWND hwndToolbar = win->hwndToolbar;
    HWND hwndParent = GetParent(hwndToolbar);

    // we call it ToolbarSize for users, but it's really size of the icon
    // toolbar size is iconSize + padding (seems to be 6)
    int iconSize = gGlobalPrefs->toolbarSize;
    if (iconSize == kDefaultIconSize) {
        // scale if default size
        iconSize = DpiScale(hwndParent, iconSize);
    }
    // Clamp to the valid logical-pixel range, then round to the nearest even
    // number. An even dimension is sufficient to avoid icon shearing and
    // works correctly at every DPI / display-zoom combination.
    iconSize = ClampIconSize(iconSize);
    iconSize = RoundUp(iconSize, 2);
    int dx = iconSize;
    // this doesn't seem to be required and doesn't help with weird sizes like 22
    // but the docs say to do it
    SendMessage(hwndToolbar, TB_SETBITMAPSIZE, 0, (LPARAM)MAKELONG(dx, dx));

    // assume square icons
    HIMAGELIST himl = ImageList_Create(dx, dx, ILC_COLOR32, kButtonsCount, 0);
    HBITMAP hbmp = BuildIconsBitmap(dx, dx);
    ImageList_Add(himl, hbmp, nullptr);
    DeleteObject(hbmp);
    SendMessageW(hwndToolbar, TB_SETIMAGELIST, 0, (LPARAM)himl);
    return iconSize;
}

void UpdateToolbarAfterThemeChange(MainWindow* win) {
    SetToolbarIconsImageList(win);
    HwndScheduleRepaint(win->hwndToolbar);
}

// Call this after WM_DPICHANGED so icons and layout metrics are recomputed
// for the new DPI / zoom level without destroying every child window.
// This is faster than ReCreateToolbar for live DPI changes (e.g. dragging
// the window between monitors with different scale settings).
void UpdateToolbarAfterDpiChange(MainWindow* win) {
    if (!win->hwndToolbar) return;

    // 1. Rebuild SVG icon bitmap at the new DPI-scaled size.
    SetToolbarIconsImageList(win);

    // 2. Recompute TBMETRICS (padding, button spacing) that depend on DPI.
    const bool isCompact = IsToolbarCompactDensity(win);
    int iconSize = gGlobalPrefs->toolbarSize;
    if (iconSize == kDefaultIconSize) {
        iconSize = DpiScale(win->hwndFrame, iconSize);
    }
    iconSize = ClampIconSize(iconSize);
    iconSize = RoundUp(iconSize, 2);

    const int yPad = DpiScale(win->hwndFrame, isCompact ? 4 : 7);
    TBMETRICS tbMetrics{};
    tbMetrics.cbSize = sizeof(tbMetrics);
    tbMetrics.dwMask = TBMF_PAD | TBMF_BUTTONSPACING;
    tbMetrics.cxPad = DpiScale(win->hwndFrame, isCompact ? 6 : 10);
    tbMetrics.cyPad = yPad;
    tbMetrics.cxButtonSpacing = ButtonSpacingX(win->hwndFrame);
    tbMetrics.cyButtonSpacing = DpiScale(win->hwndFrame, 4);
    TbSetMetrics(win->hwndToolbar, &tbMetrics);

    // 3. Update button dimensions.
    const int buttonDy = iconSize + DpiScale(win->hwndFrame, isCompact ? 10 : 14);
    SendMessageW(win->hwndToolbar, TB_SETBUTTONSIZE, 0, MAKELONG(iconSize, buttonDy));

    // 4. Refresh separator widths and fixed-width text buttons.
    const int sepDx = DpiScale(win->hwndFrame, isCompact ? 8 : 12);
    for (int i = 0; i < kButtonsCount; i++) {
        if (gToolbarButtons[i].bmpIndex == TbIcon::None && gToolbarButtons[i].cmdId == 0) {
            TbSetButtonDxByIdx(win->hwndToolbar, i, sepDx);
        }
    }
    TbSetButtonDx(win->hwndToolbar, CmdOpenFile,
                  DpiScale(win->hwndFrame, isCompact ? 62 : 84));
    TbSetButtonDx(win->hwndToolbar, CmdToggleTableOfContents,
                  DpiScale(win->hwndFrame, isCompact ? 70 : 92));
    TbSetButtonDx(win->hwndToolbar, MoreActionsId,
                  DpiScale(win->hwndFrame, isCompact ? 58 : 72));

    HwndScheduleRepaint(win->hwndToolbar);
}

// https://docs.microsoft.com/en-us/windows/win32/controls/toolbar-control-reference
void CreateToolbar(MainWindow* win) {
    bool isRtl = IsUIRtl();
    bool isCompact = IsToolbarCompactDensity(win);

    // buttonSpacingOverride: set to 0 so cxButtonSpacing is not incremented
    // during toolbar creation (spacing is already baked into the button sizes).
    const int buttonSpacingOverride = 0;
    HINSTANCE hinst = GetModuleHandle(nullptr);
    HWND hwndParent = win->hwndFrame;

    DWORD style = WS_CHILD | WS_CLIPCHILDREN | RBS_VARHEIGHT;
    style |= CCS_NODIVIDER | CCS_NOPARENTALIGN | WS_VISIBLE;
    DWORD exStyle = WS_EX_TOOLWINDOW;
    if (isRtl) exStyle |= WS_EX_LAYOUTRTL;

    win->hwndReBar = CreateWindowExW(exStyle, REBARCLASSNAME, nullptr, style, 0, 0, 0, 0, hwndParent, (HMENU)IDC_REBAR,
                                     hinst, nullptr);
    SetWindowSubclass(win->hwndReBar, ReBarWndProc, 0, 0);

    REBARINFO rbi{};
    rbi.cbSize = sizeof(REBARINFO);
    rbi.fMask = 0;
    rbi.himl = (HIMAGELIST) nullptr;
    SendMessageW(win->hwndReBar, RB_SETBARINFO, 0, (LPARAM)&rbi);
    SendMessageW(win->hwndReBar, RB_SETBKCOLOR, 0, PrettyStyleEnabled() ? PrettySurfaceAltColor() : ThemeControlBackgroundColor());

    style = WS_CHILD | WS_CLIPSIBLINGS | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT;
    style |= TBSTYLE_LIST | CCS_NODIVIDER | CCS_NOPARENTALIGN;
    exStyle = 0;
    if (isRtl) exStyle |= WS_EX_LAYOUTRTL;
    HMENU cmd = (HMENU)IDC_TOOLBAR;
    HWND hwndToolbar =
        CreateWindowExW(exStyle, TOOLBARCLASSNAME, nullptr, style, 0, 0, 0, 0, win->hwndReBar, cmd, hinst, nullptr);
    win->hwndToolbar = hwndToolbar;
    SendMessageW(hwndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

    if (!UseDarkModeLib() || !DarkMode::isEnabled()) {
        if (!IsCurrentThemeDefault()) {
            // without this custom draw code doesn't work
            SetWindowTheme(hwndToolbar, L"", L"");
        }
    }

    if (UseDarkModeLib()) {
        DarkMode::setWindowNotifyCustomDrawSubclass(win->hwndReBar);
    }

    int iconSize = SetToolbarIconsImageList(win);

    TBMETRICS tbMetrics{};
    tbMetrics.cbSize = sizeof(tbMetrics);
    // tbMetrics.dwMask = TBMF_PAD;
    tbMetrics.dwMask = TBMF_BUTTONSPACING;
    TbGetMetrics(hwndToolbar, &tbMetrics);
    int yPad = DpiScale(win->hwndFrame, isCompact ? 4 : 7);
    tbMetrics.cxPad += DpiScale(win->hwndFrame, isCompact ? 6 : 10);
    tbMetrics.cyPad += yPad;
    tbMetrics.cxButtonSpacing += DpiScale(win->hwndFrame, buttonSpacingOverride);
    // tbMetrics.cyButtonSpacing += DpiScale(win->hwndFrame, 4);
    TbSetMetrics(hwndToolbar, &tbMetrics);

    LRESULT exstyle = SendMessageW(hwndToolbar, TB_GETEXTENDEDSTYLE, 0, 0);
    exstyle |= TBSTYLE_EX_MIXEDBUTTONS;
    SendMessageW(hwndToolbar, TB_SETEXTENDEDSTYLE, 0, exstyle);

    TBBUTTON tbButtons[kButtonsCount];
    for (int i = 0; i < kButtonsCount; i++) {
        const ToolbarButtonInfo& bi = gToolbarButtons[i];
        tbButtons[i] = TbButtonFromButtonInfo(bi);
    }
    SendMessageW(hwndToolbar, TB_ADDBUTTONS, kButtonsCount, (LPARAM)tbButtons);

    // Widen separators so command groups are visually distinct, closer to the mockup structure.
    int sepDx = DpiScale(win->hwndFrame, isCompact ? 8 : 12);
    for (int i = 0; i < kButtonsCount; i++) {
        if (gToolbarButtons[i].bmpIndex == TbIcon::None && gToolbarButtons[i].cmdId == 0) {
            TbSetButtonDxByIdx(hwndToolbar, i, sepDx);
        }
    }

    TbSetButtonDx(hwndToolbar, CmdOpenFile, DpiScale(win->hwndFrame, isCompact ? 62 : 84));
    TbSetButtonDx(hwndToolbar, CmdToggleTableOfContents, DpiScale(win->hwndFrame, isCompact ? 70 : 92));
    TbSetButtonDx(hwndToolbar, MoreActionsId, DpiScale(win->hwndFrame, isCompact ? 58 : 72));

    gCustomButtonsCount = 0;

    char* text;
    for (Shortcut* shortcut : *gGlobalPrefs->shortcuts) {
        if (gCustomButtonsCount >= kMaxCustomButtons) {
            break;
        }
        text = shortcut->toolbarText;
        if (str::IsEmptyOrWhiteSpace(text)) {
            continue;
        }
        ToolbarButtonInfo tbi;
        tbi.bmpIndex = TbIcon::Text;
        tbi.cmdId = shortcut->cmdId;
        tbi.toolTip = text;
        gCustomButtons[gCustomButtonsCount++] = tbi;
    }

    // add toolbar buttons from custom commands with ToolbarText (e.g. ExternalViewers)
    for (auto cc = gFirstCustomCommand; cc; cc = cc->next) {
        if (gCustomButtonsCount >= kMaxCustomButtons) {
            break;
        }
        const char* tbText = GetCommandStringArg(cc, kCmdArgToolbarText, nullptr);
        if (str::IsEmptyOrWhiteSpace(tbText)) {
            continue;
        }
        ToolbarButtonInfo tbi;
        tbi.bmpIndex = TbIcon::Text;
        tbi.cmdId = cc->id;
        tbi.toolTip = tbText;
        gCustomButtons[gCustomButtonsCount++] = tbi;
    }

    TBBUTTON* buttons = AllocArrayTemp<TBBUTTON>(gCustomButtonsCount);
    for (int i = 0; i < gCustomButtonsCount; i++) {
        ToolbarButtonInfo& tbi = gCustomButtons[i];
        buttons[i] = TbButtonFromButtonInfo(tbi, true);
    }
    SendMessageW(hwndToolbar, TB_ADDBUTTONS, gCustomButtonsCount, (LPARAM)buttons);
    int buttonDy = iconSize + DpiScale(win->hwndFrame, isCompact ? 10 : 14);
    SendMessageW(hwndToolbar, TB_SETBUTTONSIZE, 0, MAKELONG(iconSize, buttonDy));

    RECT rc;
    LRESULT res = SendMessageW(hwndToolbar, TB_GETITEMRECT, 0, (LPARAM)&rc);
    if (!res) {
        rc.left = rc.right = rc.top = rc.bottom = 0;
    }

    ShowWindow(hwndToolbar, SW_SHOW);

    REBARBANDINFOW rbBand{};
    rbBand.cbSize = sizeof(REBARBANDINFOW);
    rbBand.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE;
    rbBand.fStyle = RBBS_FIXEDSIZE;
    rbBand.hbmBack = nullptr;
    rbBand.lpText = (WCHAR*)L"Toolbar"; // NOLINT
    rbBand.hwndChild = hwndToolbar;
    rbBand.cxMinChild = (rc.right - rc.left) * kButtonsCount;
    rbBand.cyMinChild = (rc.bottom - rc.top) + 2 * rc.top;
    rbBand.cx = 0;
    SendMessageW(win->hwndReBar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

    SetWindowPos(win->hwndReBar, nullptr, 0, 0, 0, 0, SWP_NOZORDER);

    int defFontSize = GetAppFontSize();
    // 18 was the default toolbar size, we want to scale the fonts in proportion
    int newSize = (defFontSize * gGlobalPrefs->toolbarSize) / kDefaultIconSize;
    // Leave room for top+bottom padding plus a 1-px safety margin so the
    // font never overflows the button height at any DPI / zoom level.
    int maxFontSize = iconSize - yPad * 2 - 1;
    if (newSize > maxFontSize) {
        logfa("CreateToolbar: setting toolbar font size to %d (scaled was %d, default size: %d)\n", maxFontSize,
              newSize, defFontSize);
        newSize = maxFontSize;
    } else {
        logfa("CreateToolbar: setting toolbar font size to %d (default size: %d)\n", newSize, defFontSize);
    }
    auto font = GetDefaultGuiFontOfSize(newSize);
    HwndSetFont(hwndToolbar, font);

    CreatePageBox(win, font, iconSize);
    CreateFindBox(win, font, iconSize);

    UpdateToolbarPageText(win, -1);
    UpdateToolbarFindText(win);

    if (prettysumatra::bridge::UseHybridToolbar()) {
        ShowWindow(win->hwndReBar, SW_HIDE);
    }
}

// ReCreateToolbar must be called whenever the window moves to a monitor with
// a different DPI (WM_DPICHANGED) or when the user changes the display zoom
// in Windows Settings -> Display -> Scale. Example handler snippet:
//
//   case WM_DPICHANGED: {
//       const RECT* r = reinterpret_cast<const RECT*>(lParam);
//       SetWindowPos(hwnd, nullptr, r->left, r->top,
//                    r->right - r->left, r->bottom - r->top,
//                    SWP_NOZORDER | SWP_NOACTIVATE);
//       ReCreateToolbar(win);
//       RelayoutWindow(win);
//       break;
//   }
void ReCreateToolbar(MainWindow* win) {
    if (win->hwndReBar) {
        HwndDestroyWindowSafe(&win->hwndPageLabel);
        HwndDestroyWindowSafe(&win->hwndPageEdit);
        HwndDestroyWindowSafe(&win->hwndPageBg);
        HwndDestroyWindowSafe(&win->hwndPageTotal);
        HwndDestroyWindowSafe(&win->hwndFindLabel);
        HwndDestroyWindowSafe(&win->hwndFindEdit);
        HwndDestroyWindowSafe(&win->hwndFindBg);
        HwndDestroyWindowSafe(&win->hwndToolbar);
        HwndDestroyWindowSafe(&win->hwndReBar);
    }
    CreateToolbar(win);
}

// --- Menu bar as rebar control (used when tabs are in titlebar) ---

static LRESULT CALLBACK MenuBarReBarWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass,
                                            DWORD_PTR dwRefData) {
    if (WM_ERASEBKGND == uMsg) {
        // always paint background with theme color to avoid gray strips in light theme
        HDC hdc = (HDC)wParam;
        RECT rect;
        GetClientRect(hWnd, &rect);
        COLORREF bgCol = ThemeControlBackgroundColor();
        auto bgBrush = CreateSolidBrush(bgCol);
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush);
        return 1;
    }
    if (WM_NOTIFY == uMsg) {
        auto win = FindMainWindowByHwnd(hWnd);
        NMHDR* hdr = (NMHDR*)lParam;
        if (win && hdr->code == NM_CUSTOMDRAW && hdr->hwndFrom == win->hwndMenuToolbar) {
            NMTBCUSTOMDRAW* custDraw = (NMTBCUSTOMDRAW*)hdr;
            switch (custDraw->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT: {
                    auto col = ThemeWindowTextColor();
                    UINT itemState = custDraw->nmcd.uItemState;
                    if (itemState & CDIS_DISABLED) {
                        col = ThemeWindowTextDisabledColor();
                    }
                    custDraw->clrText = col;
                    return CDRF_DODEFAULT;
                }
            }
        }
    }
    if (WM_NCDESTROY == uMsg) {
        RemoveWindowSubclass(hWnd, MenuBarReBarWndProc, uIdSubclass);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK MenuBarToolbarWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass,
                                              DWORD_PTR dwRefData) {
    if (WM_ERASEBKGND == uMsg) {
        // don't erase background here; toolbar paints its own background during WM_PAINT
        // filling here causes visible flicker (erase then paint) during window resize
        return 1;
    }
    if (WM_NCDESTROY == uMsg) {
        RemoveWindowSubclass(hWnd, MenuBarToolbarWndProc, uIdSubclass);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

constexpr int kMenuBarCmdFirst = 50000;
constexpr int kMenuBarCmdLast = 50020;

struct MenuBarPopupNav {
    MainWindow* win = nullptr;
    HMENU rootMenu = nullptr;
    HMENU currentMenu = nullptr;
    UINT currentFlags = 0;
    int nextMenuIdx = -1;
};

static MenuBarPopupNav gMenuBarPopupNav;

// track when a menu popup was last dismissed so a second click on the same
// menu bar button closes the popup instead of immediately reopening it
static int gMenuBarLastDismissedIdx = -1;
static DWORD gMenuBarLastDismissedTick = 0;

static bool ShouldSwitchCustomMenuBarPopup(UINT vk) {
    if (!gMenuBarPopupNav.win || !gMenuBarPopupNav.rootMenu) {
        return false;
    }
    if (!gMenuBarPopupNav.currentMenu || gMenuBarPopupNav.currentMenu != gMenuBarPopupNav.rootMenu) {
        return false;
    }
    if (bit::IsMaskSet(gMenuBarPopupNav.currentFlags, (UINT)MF_POPUP)) {
        return false;
    }

    int menuCount = GetMenuItemCount(gMenuBarPopupNav.win->menu);
    if (menuCount <= 1) {
        return false;
    }

    int step = 0;
    if (vk == VK_LEFT) {
        step = -1;
    } else if (vk == VK_RIGHT) {
        step = 1;
    }
    if (step == 0) {
        return false;
    }

    gMenuBarPopupNav.nextMenuIdx += step;
    if (gMenuBarPopupNav.nextMenuIdx < 0) {
        gMenuBarPopupNav.nextMenuIdx = menuCount - 1;
    } else if (gMenuBarPopupNav.nextMenuIdx >= menuCount) {
        gMenuBarPopupNav.nextMenuIdx = 0;
    }
    return true;
}

// check if mouse is over a different toolbar button and switch to it
static bool ShouldSwitchMenuBarOnMouseMove() {
    if (!gMenuBarPopupNav.win || !gMenuBarPopupNav.win->hwndMenuToolbar) {
        return false;
    }
    HWND hwndTb = gMenuBarPopupNav.win->hwndMenuToolbar;

    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(hwndTb, &pt);

    // hit-test the toolbar
    int btnCount = (int)SendMessageW(hwndTb, TB_BUTTONCOUNT, 0, 0);
    for (int i = 0; i < btnCount; i++) {
        RECT rc;
        SendMessageW(hwndTb, TB_GETITEMRECT, i, (LPARAM)&rc);
        if (PtInRect(&rc, pt)) {
            TBBUTTON tb{};
            SendMessageW(hwndTb, TB_GETBUTTON, i, (LPARAM)&tb);
            int menuIdx = tb.idCommand - kMenuBarCmdFirst;
            if (menuIdx != gMenuBarPopupNav.nextMenuIdx) {
                gMenuBarPopupNav.nextMenuIdx = menuIdx;
                return true;
            }
            return false;
        }
    }
    return false;
}

static LRESULT CALLBACK MenuBarMsgFilterHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code == MSGF_MENU && gMenuBarPopupNav.win) {
        MSG* msg = (MSG*)lParam;
        if ((msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) &&
            ShouldSwitchCustomMenuBarPopup((UINT)msg->wParam)) {
            EndMenu();
            return 1;
        }
        if (msg->message == WM_MOUSEMOVE && ShouldSwitchMenuBarOnMouseMove()) {
            EndMenu();
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void UpdateCustomMenuBarMenuSelect(MainWindow* win, WPARAM wp, LPARAM lp) {
    if (gMenuBarPopupNav.win != win) {
        return;
    }

    UINT flags = HIWORD(wp);
    HMENU menu = (HMENU)lp;
    if (flags == 0xFFFF && !menu) {
        gMenuBarPopupNav.currentMenu = nullptr;
        gMenuBarPopupNav.currentFlags = 0;
        return;
    }

    gMenuBarPopupNav.currentMenu = menu;
    gMenuBarPopupNav.currentFlags = flags;
}

void RebuildMenuBarButtons(MainWindow* win) {
    HWND hwndMb = win->hwndMenuToolbar;
    if (!hwndMb) {
        return;
    }

    // remove existing buttons
    while (SendMessageW(hwndMb, TB_DELETEBUTTON, 0, 0)) {
    }

    HMENU menu = win->menu;
    int count = GetMenuItemCount(menu);
    if (count <= 0) {
        return;
    }

    MENUITEMINFOW mii{};
    mii.cbSize = sizeof(MENUITEMINFOW);
    mii.fMask = MIIM_SUBMENU | MIIM_STRING;

    for (int i = 0; i < count && i < (kMenuBarCmdLast - kMenuBarCmdFirst); i++) {
        mii.dwTypeData = nullptr;
        mii.cch = 0;
        GetMenuItemInfoW(menu, i, TRUE, &mii);
        if (!mii.hSubMenu || !mii.cch) {
            continue;
        }
        mii.cch++;
        AutoFreeWStr name(AllocArray<WCHAR>(mii.cch));
        mii.dwTypeData = name;
        GetMenuItemInfoW(menu, i, TRUE, &mii);

        TBBUTTON b{};
        b.iBitmap = I_IMAGENONE;
        b.idCommand = kMenuBarCmdFirst + i;
        b.fsState = TBSTATE_ENABLED;
        b.fsStyle = BTNS_AUTOSIZE | BTNS_SHOWTEXT;
        b.iString = (INT_PTR)name.Get();
        SendMessageW(hwndMb, TB_ADDBUTTONS, 1, (LPARAM)&b);
    }
}

void CreateMenuBarRebar(MainWindow* win) {
    if (win->hwndMenuReBar) {
        return;
    }

    bool isRtl = IsUIRtl();
    HINSTANCE hinst = GetModuleHandle(nullptr);
    HWND hwndParent = win->hwndFrame;

    // create hidden; caller shows after RelayoutWindow positions it
    // no WS_BORDER (avoids 1px gap) and no RBS_BANDBORDERS (avoids gray band separators)
    DWORD style = WS_CHILD | WS_CLIPCHILDREN | RBS_VARHEIGHT;
    style |= CCS_NODIVIDER | CCS_NOPARENTALIGN;
    DWORD exStyle = WS_EX_TOOLWINDOW;
    if (isRtl) {
        exStyle |= WS_EX_LAYOUTRTL;
    }

    win->hwndMenuReBar = CreateWindowExW(exStyle, REBARCLASSNAME, nullptr, style, 0, 0, 0, 0, hwndParent,
                                         (HMENU)IDC_MENUBAR_REBAR, hinst, nullptr);
    SetWindowSubclass(win->hwndMenuReBar, MenuBarReBarWndProc, 0, 0);

    REBARINFO rbi{};
    rbi.cbSize = sizeof(REBARINFO);
    SendMessageW(win->hwndMenuReBar, RB_SETBARINFO, 0, (LPARAM)&rbi);
    SendMessageW(win->hwndMenuReBar, RB_SETBKCOLOR, 0, ThemeControlBackgroundColor());

    style = WS_CHILD | WS_CLIPSIBLINGS | TBSTYLE_FLAT | TBSTYLE_LIST;
    style |= CCS_NODIVIDER | CCS_NOPARENTALIGN;
    exStyle = 0;
    if (isRtl) {
        exStyle |= WS_EX_LAYOUTRTL;
    }

    win->hwndMenuToolbar = CreateWindowExW(exStyle, TOOLBARCLASSNAME, nullptr, style, 0, 0, 0, 0, win->hwndMenuReBar,
                                           (HMENU)IDC_MENUBAR, hinst, nullptr);
    SetWindowSubclass(win->hwndMenuToolbar, MenuBarToolbarWndProc, 0, 0);
    SendMessageW(win->hwndMenuToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

    if (!UseDarkModeLib() || !DarkMode::isEnabled()) {
        if (!IsCurrentThemeDefault()) {
            SetWindowTheme(win->hwndMenuToolbar, L"", L"");
        }
    }

    HFONT font = GetAppMenuFont();
    HwndSetFont(win->hwndMenuToolbar, font);

    LRESULT tbExStyle = SendMessageW(win->hwndMenuToolbar, TB_GETEXTENDEDSTYLE, 0, 0);
    tbExStyle |= TBSTYLE_EX_MIXEDBUTTONS;
    SendMessageW(win->hwndMenuToolbar, TB_SETEXTENDEDSTYLE, 0, tbExStyle);

    RebuildMenuBarButtons(win);

    RECT rc;
    LRESULT res = SendMessageW(win->hwndMenuToolbar, TB_GETITEMRECT, 0, (LPARAM)&rc);
    if (!res) {
        rc.left = rc.right = rc.top = rc.bottom = 0;
    }

    ShowWindow(win->hwndMenuToolbar, SW_SHOW);

    REBARBANDINFOW rbBand{};
    rbBand.cbSize = sizeof(REBARBANDINFOW);
    rbBand.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE;
    rbBand.fStyle = RBBS_FIXEDSIZE;
    rbBand.hwndChild = win->hwndMenuToolbar;
    rbBand.cxMinChild = 0;
    rbBand.cyMinChild = (rc.bottom - rc.top) + 2 * rc.top;
    rbBand.cx = 0;
    SendMessageW(win->hwndMenuReBar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

    if (UseDarkModeLib()) {
        DarkMode::setWindowNotifyCustomDrawSubclass(win->hwndMenuReBar);
        DarkMode::setChildCtrlsSubclassAndTheme(win->hwndMenuReBar);
    }
}

void ShowMenuBarRebar(MainWindow* win) {
    if (win->hwndMenuReBar) {
        ShowWindow(win->hwndMenuReBar, SW_SHOW);
    }
}

void DestroyMenuBarRebar(MainWindow* win) {
    HwndDestroyWindowSafe(&win->hwndMenuToolbar);
    HwndDestroyWindowSafe(&win->hwndMenuReBar);
}

bool IsShowingMenuBarRebar(MainWindow* win) {
    if (!win->hwndMenuReBar) {
        return false;
    }
    if (win->presentation) {
        return false;
    }
    return true;
}

bool HandleMenuBarCommand(MainWindow* win, int cmdId) {
    if (cmdId < kMenuBarCmdFirst || cmdId >= kMenuBarCmdLast) {
        return false;
    }
    if (!win->hwndMenuToolbar) {
        return false;
    }

    int menuCount = GetMenuItemCount(win->menu);
    int menuIdx = cmdId - kMenuBarCmdFirst;

    // if same button was clicked shortly after dismissing its popup, treat as toggle-close
    DWORD now = GetTickCount();
    if (menuIdx == gMenuBarLastDismissedIdx && (now - gMenuBarLastDismissedTick) < 500) {
        gMenuBarLastDismissedIdx = -1;
        return true;
    }

    UINT flags = TPM_LEFTALIGN | TPM_TOPALIGN;
    if (IsUIRtl()) {
        flags = TPM_RIGHTALIGN | TPM_TOPALIGN;
    }

    for (;;) {
        HMENU subMenu = GetSubMenu(win->menu, menuIdx);
        if (!subMenu) {
            return true;
        }

        // get button rect in screen coordinates
        RECT btnRect;
        int btnCmdId = kMenuBarCmdFirst + menuIdx;
        int btnIdx = (int)SendMessageW(win->hwndMenuToolbar, TB_COMMANDTOINDEX, btnCmdId, 0);
        SendMessageW(win->hwndMenuToolbar, TB_GETITEMRECT, btnIdx, (LPARAM)&btnRect);
        MapWindowPoints(win->hwndMenuToolbar, HWND_DESKTOP, (POINT*)&btnRect, 2);

        gMenuBarPopupNav.win = win;
        gMenuBarPopupNav.rootMenu = subMenu;
        gMenuBarPopupNav.currentMenu = subMenu;
        gMenuBarPopupNav.currentFlags = 0;
        gMenuBarPopupNav.nextMenuIdx = menuIdx;

        HHOOK hook = SetWindowsHookExW(WH_MSGFILTER, MenuBarMsgFilterHook, nullptr, GetCurrentThreadId());
        TrackPopupMenu(subMenu, flags, btnRect.left, btnRect.bottom, 0, win->hwndFrame, nullptr);
        if (hook) {
            UnhookWindowsHookEx(hook);
        }

        int nextMenuIdx = gMenuBarPopupNav.nextMenuIdx;
        gMenuBarPopupNav = {};
        if (nextMenuIdx == menuIdx || menuCount <= 1) {
            gMenuBarLastDismissedIdx = menuIdx;
            gMenuBarLastDismissedTick = GetTickCount();
            break;
        }
        menuIdx = nextMenuIdx;
    }

    return true;
}

// Activate a menu bar button by accelerator key (Alt+letter).
// If accel is 0, activate the first menu item.
// Returns true if handled.
bool ActivateMenuBarByAccel(MainWindow* win, WCHAR accel) {
    if (!win->hwndMenuToolbar || !win->menu) {
        return false;
    }

    int count = GetMenuItemCount(win->menu);
    if (count <= 0) {
        return false;
    }

    // if accel is 0 (bare Alt press), open the first menu
    if (accel == 0) {
        return HandleMenuBarCommand(win, kMenuBarCmdFirst);
    }

    // normalize to uppercase for matching
    if (accel >= 'a' && accel <= 'z') {
        accel -= 'a' - 'A';
    }

    // find the menu item whose text has &<accel>
    MENUITEMINFOW mii{};
    mii.cbSize = sizeof(MENUITEMINFOW);
    mii.fMask = MIIM_STRING;

    for (int i = 0; i < count && i < (kMenuBarCmdLast - kMenuBarCmdFirst); i++) {
        mii.dwTypeData = nullptr;
        mii.cch = 0;
        GetMenuItemInfoW(win->menu, i, TRUE, &mii);
        if (!mii.cch) {
            continue;
        }
        mii.cch++;
        AutoFreeWStr name(AllocArray<WCHAR>(mii.cch));
        mii.dwTypeData = name;
        GetMenuItemInfoW(win->menu, i, TRUE, &mii);

        // look for &X where X matches accel
        for (WCHAR* p = name.Get(); *p; p++) {
            if (*p == '&' && p[1]) {
                WCHAR ch = p[1];
                if (ch >= 'a' && ch <= 'z') {
                    ch -= 'a' - 'A';
                }
                if (ch == accel) {
                    return HandleMenuBarCommand(win, kMenuBarCmdFirst + i);
                }
                break;
            }
        }
    }

    return false;
}
