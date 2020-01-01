/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Dpi.h"
#include "utils/WinUtil.h"

#include "TabsCtrl.h"

#define COL_BLACK RGB(0x00, 0x00, 0x00)
#define COL_WHITE RGB(0xff, 0xff, 0xff)
#define COL_RED RGB(0xff, 0x00, 0x00)
#define COL_LIGHT_GRAY RGB(0xde, 0xde, 0xde)
#define COL_LIGHTER_GRAY RGB(0xee, 0xee, 0xee)
#define COL_DARK_GRAY RGB(0x42, 0x42, 0x42)

// desired space between top of the text in tab and top of the tab
#define PADDING_TOP 4
// desired space between bottom of the text in tab and bottom of the tab
#define PADDING_BOTTOM 4

// space to the left of tab label
#define PADDING_LEFT 8
// empty space to the righ of tab label
#define PADDING_RIGHT 8

// TODO: implement a max width for the tab

enum class Tab {
    Selected = 0,
    Background = 1,
    Highlighted = 2,
};

static str::WStr wstrFromUtf8(const str::Str& str) {
    AutoFreeWstr s = strconv::Utf8ToWstr(str.c_str());
    return str::WStr(s.as_view());
}

TabItem::TabItem(const std::string_view title, const std::string_view toolTip) {
    this->title = title;
    this->toolTip = toolTip;
}

class TabItemInfo {
  public:
    str::WStr title;
    str::WStr toolTip;

    SIZE titleSize;
    // area for this tab item inside the tab window
    RECT tabRect;
    POINT titlePos;
    RECT closeRect;
    HWND hwndTooltip;
};

class TabsCtrlPrivate {
  public:
    TabsCtrlPrivate(HWND hwnd) {
        this->hwnd = hwnd;
    }
    ~TabsCtrlPrivate() {
        DeleteObject(font);
    }

    HWND hwnd = nullptr;
    HFONT font = nullptr;
    // TODO: logFont is not used anymore, keep it for debugging?
    LOGFONTW logFont{}; // info that corresponds to font
    TEXTMETRIC fontMetrics{};
    int fontDy = 0;
    SIZE size{};                // current size of the control's window
    SIZE idealSize{};           // ideal size as calculated during layout
    int tabIdxUnderCursor = -1; // -1 if none under cursor
    bool isCursorOverClose = false;

    TabsCtrlState* state = nullptr;

    // each TabItemInfo orresponds to TabItem from state->tabs, same order
    Vec<TabItemInfo*> tabInfos;
};

static long GetIdealDy(TabsCtrl* ctrl) {
    auto priv = ctrl->priv;
    int padTop = PADDING_TOP;
    int padBottom = PADDING_BOTTOM;
    DpiScale(priv->hwnd, padTop, padBottom);
    return priv->fontDy + padTop + padBottom;
}

static HWND CreateTooltipForRect(HWND parent, const WCHAR* s, RECT& r) {
    HMODULE h = GetModuleHandleW(nullptr);
    DWORD dwStyleEx = WS_EX_TOPMOST;
    DWORD dwStyle = WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP;
    HWND hwnd = CreateWindowExW(dwStyleEx, TOOLTIPS_CLASSW, NULL, dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                CW_USEDEFAULT, parent, NULL, h, NULL);

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    TOOLINFOW ti = {0};
    ti.cbSize = sizeof(TOOLINFO);
    ti.uFlags = TTF_SUBCLASS;
    ti.hwnd = parent;
    ti.hinst = h;
    ti.lpszText = (WCHAR*)s;
    ti.rect = r;
    SendMessage(hwnd, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    SendMessage(hwnd, TTM_ACTIVATE, TRUE, 0);
    return hwnd;
}

void LayoutTabs(TabsCtrl* ctrl) {
    auto priv = ctrl->priv;
    long x = 0;
    long dy = priv->size.cy;
    auto idealDy = GetIdealDy(ctrl);

    int padLeft = PADDING_LEFT;
    int padRight = PADDING_RIGHT;
    DpiScale(priv->hwnd, padLeft, padRight);

    // position y of title text and 'x' circle
    long titleY = 0;
    if (dy > priv->fontDy) {
        titleY = (dy - priv->fontDy) / 2;
    }

    long closeButtonDy = (priv->fontMetrics.tmAscent / 2) + DpiScale(priv->hwnd, 1);
    long closeButtonY = (dy - closeButtonDy) / 2;
    if (closeButtonY < 0) {
        closeButtonDy = dy - 2;
        closeButtonY = 2;
    }

    for (auto& ti : priv->tabInfos) {
        long xStart = x;
        x += padLeft;

        auto sz = ti->titleSize;
        titleY = 0;
        if (dy > sz.cy) {
            titleY = (dy - sz.cy) / 2;
        }
        ti->titlePos = MakePoint(x, titleY);

        // TODO: implement max dx of the tab
        x += sz.cx;
        x += padRight;
        ti->closeRect = MakeRect(x, closeButtonY, closeButtonDy, closeButtonDy);
        x += closeButtonDy;
        x += padRight;
        long dx = (x - xStart);
        ti->tabRect = MakeRect(xStart, 0, dx, dy);
        if (!ti->toolTip.empty()) {
            if (ti->hwndTooltip) {
                DestroyWindow(ti->hwndTooltip);
            }
            ti->hwndTooltip = CreateTooltipForRect(priv->hwnd, ti->toolTip.c_str(), ti->tabRect);
        }
    }
    priv->idealSize = MakeSize(x, idealDy);
    // TODO: if dx > size of the tab, we should shrink the tabs
    TriggerRepaint(priv->hwnd);
}

static LRESULT CALLBACK TabsParentProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass,
                                       DWORD_PTR dwRefData) {
    UNUSED(uIdSubclass);
    UNUSED(dwRefData);

    // TabsCtrl *w = (TabsCtrl *)dwRefData;
    // CrashIf(GetParent(ctrl->hwnd) != (HWND)lp);

    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void PaintClose(HWND hwnd, HDC hdc, RECT& r, bool isHighlighted) {
    auto x = r.left;
    auto y = r.top;
    auto dx = RectDx(r);
    auto dy = RectDy(r);

    COLORREF lineCol = COL_BLACK;
    if (isHighlighted) {
        int p = 3;
        DpiScale(hwnd, p);
        AutoDeleteBrush brush(CreateSolidBrush(COL_RED));
        RECT r2 = r;
        r2.left -= p;
        r2.right += p;
        r2.top -= p;
        r2.bottom += p;
        FillRect(hdc, &r2, brush);
        lineCol = COL_WHITE;
    }

    AutoDeletePen pen(CreatePen(PS_SOLID, 2, lineCol));
    ScopedSelectPen p(hdc, pen);
    MoveToEx(hdc, x, y, nullptr);
    LineTo(hdc, x + dx, y + dy);

    MoveToEx(hdc, x + dx, y, nullptr);
    LineTo(hdc, x, y + dy);
}

static void Paint(TabsCtrl* ctrl) {
    auto priv = ctrl->priv;
    HWND hwnd = priv->hwnd;

    PAINTSTRUCT ps;
    RECT rc = GetClientRect(hwnd);
    HDC hdc = BeginPaint(hwnd, &ps);

    AutoDeleteBrush brush(CreateSolidBrush(COL_LIGHTER_GRAY));
    FillRect(hdc, &rc, brush);

    ScopedSelectFont f(hdc, priv->font);
    UINT opts = ETO_OPAQUE;

    int padLeft = PADDING_LEFT;
    DpiScale(priv->hwnd, padLeft);

    int tabIdx = 0;
    for (const auto& ti : priv->tabInfos) {
        if (ti->title.empty()) {
            continue;
        }
        auto tabType = Tab::Background;
        if (tabIdx == priv->state->selectedItem) {
            tabType = Tab::Selected;
        } else if (tabIdx == priv->tabIdxUnderCursor) {
            tabType = Tab::Highlighted;
        }
        COLORREF bgCol = COL_LIGHTER_GRAY;
        COLORREF txtCol = COL_DARK_GRAY;

        bool paintClose = false;
        switch (tabType) {
            case Tab::Background:
                bgCol = COL_LIGHTER_GRAY;
                txtCol = COL_DARK_GRAY;
                break;
            case Tab::Selected:
                bgCol = COL_WHITE;
                txtCol = COL_DARK_GRAY;
                paintClose = true;
                break;
            case Tab::Highlighted:
                bgCol = COL_LIGHT_GRAY;
                txtCol = COL_BLACK;
                paintClose = true;
                break;
        }

        SetTextColor(hdc, txtCol);
        SetBkColor(hdc, bgCol);

        auto tabRect = ti->tabRect;
        AutoDeleteBrush brush2(CreateSolidBrush(bgCol));
        FillRect(hdc, &tabRect, brush2);

        auto pos = ti->titlePos;
        int x = pos.x;
        int y = pos.y;
        const WCHAR* s = ti->title.c_str();
        UINT sLen = (UINT)ti->title.size();
        ExtTextOutW(hdc, x, y, opts, nullptr, s, sLen, nullptr);

        if (paintClose) {
            bool isCursorOverClose = priv->isCursorOverClose && (tabIdx == priv->tabIdxUnderCursor);
            PaintClose(hwnd, hdc, ti->closeRect, isCursorOverClose);
        }

        tabIdx++;
    }

    EndPaint(hwnd, &ps);
}

static void SetTabUnderCursor(TabsCtrl* ctrl, int tabUnderCursor, bool isMouseOverClose) {
    auto priv = ctrl->priv;
    if (priv->tabIdxUnderCursor == tabUnderCursor && priv->isCursorOverClose == isMouseOverClose) {
        return;
    }
    priv->tabIdxUnderCursor = tabUnderCursor;
    priv->isCursorOverClose = isMouseOverClose;
    TriggerRepaint(priv->hwnd);
}

static int TabFromMousePos(TabsCtrl* ctrl, int x, int y, bool& isMouseOverClose) {
    POINT mousePos = {x, y};
    for (size_t i = 0; i < ctrl->priv->tabInfos.size(); i++) {
        auto& ti = ctrl->priv->tabInfos[i];
        if (PtInRect(&ti->tabRect, mousePos)) {
            isMouseOverClose = PtInRect(&ti->closeRect, mousePos);
            return (int)i;
        }
    }
    return -1;
}

static void OnMouseMove(TabsCtrl* ctrl) {
    auto priv = ctrl->priv;
    auto mousePos = GetCursorPosInHwnd(priv->hwnd);
    bool isMouseOverClose = false;
    auto tabIdx = TabFromMousePos(ctrl, mousePos.x, mousePos.y, isMouseOverClose);

    SetTabUnderCursor(ctrl, tabIdx, isMouseOverClose);
    TrackMouseLeave(priv->hwnd);
}

static void OnLeftButtonUp(TabsCtrl* ctrl) {
    auto priv = ctrl->priv;
    auto mousePos = GetCursorPosInHwnd(priv->hwnd);
    bool isMouseOverClose;
    auto tabIdx = TabFromMousePos(ctrl, mousePos.x, mousePos.y, isMouseOverClose);
    if (tabIdx == -1) {
        return;
    }
    if (isMouseOverClose) {
        if (ctrl->onTabClosed) {
            ctrl->onTabClosed(ctrl, priv->state, tabIdx);
        }
        return;
    }
    if (tabIdx == priv->state->selectedItem) {
        return;
    }
    if (ctrl->onTabSelected) {
        ctrl->onTabSelected(ctrl, priv->state, tabIdx);
    }
}

static LRESULT CALLBACK TabsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    UNUSED(uIdSubclass);
    TabsCtrl* ctrl = (TabsCtrl*)dwRefData;
    TabsCtrlPrivate* priv = ctrl->priv;
    // CrashIf(ctrl->hwnd != (HWND)lp);

    // TraceMsg(msg);

    if (WM_ERASEBKGND == msg) {
        return TRUE; // tells Windows we handle background erasing so it doesn't do it
    }

    // This is needed in order to receive WM_MOUSEMOVE messages
    if (WM_NCHITTEST == msg) {
        // TODO: or just return HTCLIENT always?
        if (hwnd == GetCapture()) {
            return HTCLIENT;
        }
        auto mousePos = GetCursorPosInHwnd(ctrl->priv->hwnd);
        bool isMouseOverClose;
        auto tabIdx = TabFromMousePos(ctrl, mousePos.x, mousePos.y, isMouseOverClose);
        if (-1 == tabIdx) {
            return HTTRANSPARENT;
        }
        return HTCLIENT;
    }

    if (WM_NCDESTROY == msg) {
        RemoveWindowSubclass(GetParent(priv->hwnd), TabsParentProc, 0);
        RemoveWindowSubclass(priv->hwnd, TabsProc, 0);
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    if (WM_PAINT == msg) {
        CrashIf(priv->hwnd != hwnd);
        Paint(ctrl);
        return 0;
    }

    if (WM_LBUTTONDOWN == msg) {
        return 0;
    }

    if (WM_LBUTTONUP == msg) {
        OnLeftButtonUp(ctrl);
        return 0;
    }

    if (WM_MOUSELEAVE == msg) {
        SetTabUnderCursor(ctrl, -1, false);
        return 0;
    }

    if (WM_MOUSEMOVE == msg) {
        OnMouseMove(ctrl);
        return 0;
    }

    if (WM_SIZE == msg) {
        long dx = LOWORD(lp);
        long dy = HIWORD(lp);
        priv->size = MakeSize(dx, dy);
        LayoutTabs(ctrl);
        return 0;
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

TabsCtrl* AllocTabsCtrl(HWND parent, RECT initialPosition) {
    auto w = new TabsCtrl;
    w->parent = parent;
    w->initialPos = initialPosition;
    return w;
}

void SetFont(TabsCtrl* ctrl, HFONT font) {
    auto priv = ctrl->priv;
    priv->font = font;
    GetObject(font, sizeof(LOGFONTW), &priv->logFont);

    ScopedGetDC hdc(priv->hwnd);
    ScopedSelectFont prevFont(hdc, priv->font);
    GetTextMetrics(hdc, &priv->fontMetrics);
    priv->fontDy = priv->fontMetrics.tmHeight;
}

bool CreateTabsCtrl(TabsCtrl* ctrl) {
    auto r = ctrl->initialPos;
    auto x = r.left;
    auto y = r.top;
    auto dx = RectDx(r);
    auto dy = RectDy(r);
    DWORD dwExStyle = 0;
    DWORD dwStyle = WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | TCS_FOCUSNEVER | TCS_FIXEDWIDTH | TCS_FORCELABELLEFT;

    auto hwnd = CreateWindowExW(dwExStyle, WC_TABCONTROL, L"", dwStyle, x, y, dx, dy, ctrl->parent, nullptr,
                                GetModuleHandle(nullptr), ctrl);

    if (hwnd == nullptr) {
        return false;
    }

    auto priv = new TabsCtrlPrivate(hwnd);
    r = ctrl->initialPos;
    priv->size = MakeSize(RectDx(r), RectDy(r));
    priv->hwnd = hwnd;
    ctrl->priv = priv;
    SetFont(ctrl, GetDefaultGuiFont());

    SetWindowSubclass(hwnd, TabsProc, 0, (DWORD_PTR)ctrl);
    // SetWindowSubclass(GetParent(hwnd), TabsParentProc, 0, (DWORD_PTR)ctrl);
    return true;
}

void DeleteTabsCtrl(TabsCtrl* ctrl) {
    if (ctrl) {
        DeleteObject(ctrl->priv->font);
        delete ctrl->priv;
    }
    delete ctrl;
}

void SetState(TabsCtrl* ctrl, TabsCtrlState* state) {
    auto priv = ctrl->priv;
    priv->state = state;
    priv->tabInfos.clear();

    // measure size of tab's title
    auto& tabInfos = priv->tabInfos;
    for (auto& tab : state->tabs) {
        auto ti = new TabItemInfo();
        tabInfos.push_back(ti);
        ti->titleSize = MakeSize(0, 0);
        if (!tab->title.empty()) {
            ti->title = wstrFromUtf8(tab->title);
            const WCHAR* s = ti->title.c_str();
            ti->titleSize = TextSizeInHwnd2(priv->hwnd, s, priv->font);
        }
        if (!tab->toolTip.empty()) {
            ti->toolTip = wstrFromUtf8(tab->toolTip);
        }
    }
    LayoutTabs(ctrl);
    // TODO: should use mouse position to determine this
    // TODO: calculate isHighlighted
    priv->isCursorOverClose = false;
}

SIZE GetIdealSize(TabsCtrl* ctrl) {
    return ctrl->priv->idealSize;
}

void SetPos(TabsCtrl* ctrl, RECT& r) {
    MoveWindow(ctrl->priv->hwnd, &r);
}
