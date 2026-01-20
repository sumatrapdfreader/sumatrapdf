/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/BitManip.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"
#include "utils/WinDynCalls.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Theme.h"

#include "utils/Log.h"

//--- Tabs

Kind kindTabs = "tabs";

using Gdiplus::Bitmap;
using Gdiplus::Color;
using Gdiplus::CompositingQualityHighQuality;
using Gdiplus::Font;
using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::Ok;
using Gdiplus::PathData;
using Gdiplus::Pen;
using Gdiplus::Region;
using Gdiplus::SolidBrush;
using Gdiplus::Status;
using Gdiplus::StringAlignmentCenter;
using Gdiplus::StringFormat;
using Gdiplus::TextRenderingHintClearTypeGridFit;
using Gdiplus::UnitPixel;

static void HwndTabsSetItemSize(HWND hwnd, Size sz) {
    TabCtrl_SetItemSize(hwnd, sz.dx, sz.dy);
}

TabInfo::~TabInfo() {
    str::Free(text);
    str::Free(tooltip);
}

void TooltipRemoveAll(HWND hwnd) {
    int n = TooltipGetCount(hwnd);
    if (n == 0) {
        return;
    }
}

void TabsCtrl::ScheduleRepaint() {
    HwndScheduleRepaint(hwnd);
}

// Calculates tab's elements, based on its width and height.
// Generates a GraphicsPath, which is used for painting the tab, etc.
void TabsCtrl::LayoutTabs() {
    Rect rect = ClientRect(hwnd);
    int dy = rect.dy;
    int nTabs = TabCount();
    if (nTabs == 0) {
        // logfa("TabsCtrl::Layout size: (%d, %d), no tabs\n", rect.dx, rect.dy);
        HwndScheduleRepaint(hwnd);
        return;
    }
    auto maxDx = (rect.dx - 5) / nTabs;
    int dx = std::min(tabDefaultDx, maxDx);
    tabSize = {dx, dy};
    // logfa("TabsCtrl::Layout size: (%d, %d), tab size: (%d, %d)\n", rect.dx, rect.dy, tabSize.dx, tabSize.dy);

    int closeDy = DpiScale(hwnd, 8);
    int closeDx = closeDy;
    int closeY = (dy - closeDy) / 2;
    // logfa("  closeDx: %d, closeDy: %d\n", closeDx, closeDy);

    HFONT hfont = GetFont();
    int x = 0;
    int xEnd;
    TooltipInfo* tools = AllocArray<TooltipInfo>(nTabs);
    for (int i = 0; i < nTabs; i++) {
        TabInfo* ti = GetTab(i);
        xEnd = x + dx;
        ti->r = {x, 0, dx, dy};
        ti->rClose = {xEnd - closeDx - 8, closeY, closeDx, closeDy};
        ti->titleSize = HwndMeasureText(hwnd, ti->text, hfont);
        int y = (dy - ti->titleSize.dy) / 2;
        // logfa("  ti->titleSize.dy: %d\n", ti->titleSize.dy);
        if (y < 0) {
            y = 0;
        }
        ti->titlePos = {x + 2, y};
        if (withToolTips) {
            tools[i].s = ti->tooltip;
            tools[i].id = i;
            tools[i].r = ti->r;
        }
        x = xEnd;
    }
    if (withToolTips) {
        HWND ttHwnd = GetToolTipsHwnd();
        TooltipAddTools(ttHwnd, hwnd, tools, nTabs);
    }
    free(tools);

    HwndTabsSetItemSize(hwnd, tabSize);
}

// Finds the index of the tab, which contains the given point.
TabsCtrl::MouseState TabsCtrl::TabStateFromMousePosition(const Point& p) {
    TabsCtrl::MouseState res;
    if (p.x < 0 || p.y < 0) {
        return res;
    }
    int nTabs = TabCount();
    for (int i = 0; i < nTabs; i++) {
        TabInfo* ti = tabs[i];
        Rect r = ti->r;
        // logfa("testing i=%d rect: %d %d %d %d pt: %d %d\n", i, ti->r.x, ti->r.y, ti->r.dx, ti->r.dy, p.x, p.y);
        if (!r.Contains(p)) {
            continue;
        }
        res.tabIdx = i;
        res.overClose = ti->rClose.Contains(p);
        res.tabInfo = ti;
        Rect rightHalf = r;
        int halfDx = r.dx / 2;
        rightHalf.x = r.x + halfDx;
        rightHalf.dx = halfDx;
        res.inRightHalf = rightHalf.Contains(p);
        return res;
    }

    return res;
}

// TODO: duplicated in Caption.cpp
static void PaintParentBackground(HWND hwnd, HDC hdc) {
    HWND parent = GetParent(hwnd);
    POINT pt = {0, 0};
    MapWindowPoints(hwnd, parent, &pt, 1);
    SetViewportOrgEx(hdc, -pt.x, -pt.y, &pt);
    SendMessageW(parent, WM_ERASEBKGND, (WPARAM)hdc, 0);
    SetViewportOrgEx(hdc, pt.x, pt.y, nullptr);

    // TODO: needed to force repaint of tab area after closing a window
    InvalidateRect(parent, nullptr, TRUE);
}

Gdiplus::Color GdipCol(COLORREF c) {
    return GdiRgbFromCOLORREF(c);
}

// if true, on hover we paint the background of tab close (X) button
constexpr bool closeCircleEnabled = true;
constexpr float closePenWidth = 1.0f;
constexpr COLORREF circleColor = RgbToCOLORREF(0xC13535);

bool TabsCtrl::IsValidIdx(int idx) {
    return idx >= 0 && idx < TabCount();
}

void TabsCtrl::Paint(HDC hdc, const RECT& rc) {
    TabsCtrl::MouseState tabState = TabStateFromMousePosition(lastMousePos);
    int tabUnderMouse = tabState.tabIdx;
    bool overClose = tabState.overClose && tabState.tabInfo->canClose;
    int selectedIdx = GetSelected();
    if (IsValidIdx(tabForceShowSelected)) {
        selectedIdx = tabForceShowSelected;
    }

    // logfa("TabsCtrl::Paint, underMouse: %d, overClose: %d, selected: %d, rc: pos: (%d, %d), size: (%d, %d)\n",
    //  tabUnderMouse, (int)overClose, selectedIdx, rc.left, rc.top, RectDx(rc), RectDy(rc));

    bool isTranslucentMode = inTitleBar && dwm::IsCompositionEnabled();
    if (isTranslucentMode) {
        PaintParentBackground(hwnd, hdc);
    }

    Graphics gfx(hdc);
    gfx.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    gfx.SetCompositingQuality(CompositingQualityHighQuality);
    gfx.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    gfx.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    gfx.SetPageUnit(UnitPixel);

    SolidBrush br(GdipCol(ThemeControlBackgroundColor()));

    Font f(hdc, GetFont());

    Gdiplus::Rect gr = ToGdipRect(rc);
    gfx.FillRectangle(&br, gr);

    StringFormat sf(StringFormat::GenericDefault());
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    sf.SetLineAlignment(StringAlignmentCenter);
    sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

    TabInfo* ti;
    int n = TabCount();
    Rect r;
    Gdiplus::RectF rTxt;

    COLORREF textColor = ThemeWindowTextColor();
    COLORREF tabBgSelected = ThemeControlBackgroundColor();
    COLORREF tabBgHighlight;
    COLORREF tabBgBackground;
    if (IsLightColor(tabBgSelected)) {
        tabBgBackground = AdjustLightness2(tabBgSelected, -25);
        tabBgHighlight = AdjustLightness2(tabBgSelected, -35);
    } else {
        tabBgBackground = AdjustLightness2(tabBgSelected, 25);
        tabBgHighlight = AdjustLightness2(tabBgSelected, 35);
    }

    COLORREF tabBgCol;
    for (int i = 0; i < n; i++) {
        // Get the correct colors based on the state and the current theme
        tabBgCol = tabBgBackground;
        if (selectedIdx == i) {
            tabBgCol = tabBgSelected;
        } else if (tabUnderMouse == i) {
            tabBgCol = tabBgHighlight;
        }

        ti = GetTab(i);
        // logfa("rClose: pos: (%d, %d) size: (%d, %d)\n", r.x, r.y, r.dx, r.dy);

        gfx.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);

        // draw background
        br.SetColor(GdipCol(tabBgCol));
        gr = ToGdipRect(ti->r);
        gfx.FillRectangle(&br, gr);

        if (ti->canClose) {
            r = ti->rClose;
            if (i == tabUnderMouse && overClose && closeCircleEnabled) {
                // draw bacground of X
                Rect cr = r;
                cr.Inflate(3, 3);
                gr = ToGdipRect(cr);
                br.SetColor(GdipCol(circleColor));
                gfx.FillRectangle(&br, gr);
            }

            // draw X
            br.SetColor(GdipCol(textColor));
            Pen penX(&br, closePenWidth);
            Gdiplus::Point p1(r.x, r.y);
            Gdiplus::Point p2(r.x + r.dx, r.y + r.dy);
            gfx.DrawLine(&penX, p1, p2);
            p1 = {r.x + r.dx, r.y};
            p2 = {r.x, r.y + r.dy};
            gfx.DrawLine(&penX, p1, p2);
        }

        // draw text
        gfx.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
        rTxt = ToGdipRectF(ti->r);
        rTxt.X += 8;
        rTxt.Width -= (8 + r.dx + 8);
        br.SetColor(GdipCol(textColor));
        TempWStr ws = ToWStrTemp(ti->text);
        gfx.DrawString(ws, -1, &f, rTxt, &sf, &br);
    }
}

HBITMAP TabsCtrl::RenderForDragging(int idx) {
    TabInfo* ti = GetTab(idx);
    if (!ti) {
        return nullptr;
    }
    Bitmap bitmap(ti->r.dx, ti->r.dy);
    Graphics* gfx = Graphics::FromImage(&bitmap);
    // DrawString() on a bitmap does not work with CompositingModeSourceCopy - obscure bug.
    gfx->SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    gfx->SetCompositingQuality(CompositingQualityHighQuality);
    gfx->SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    gfx->SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    gfx->SetPageUnit(UnitPixel);

    StringFormat sf(StringFormat::GenericDefault());
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    sf.SetLineAlignment(StringAlignmentCenter);
    sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

    COLORREF bgCol = tabSelectedBg;
    COLORREF textCol = tabSelectedText;

    SolidBrush br(GdipCol(bgCol));
    Gdiplus::Rect gr(0, 0, ti->r.dx, ti->r.dy);
    gfx->FillRectangle(&br, gr);

    HDC hdc = GetDC(hwnd);
    Font f(hdc, GetFont());
    ReleaseDC(hwnd, hdc);

    Gdiplus::RectF rTxt(0, 0, ti->r.dx, ti->r.dy);
    rTxt.X += 8;
    rTxt.Width -= (8 + 8);
    br.SetColor(GdipCol(textCol));
    TempWStr ws = ToWStrTemp(ti->text);
    gfx->DrawString(ws, -1, &f, rTxt, &sf, &br);

    HBITMAP ret;
    bitmap.GetHBITMAP(Color(255, 255, 255), &ret);
    delete gfx;
    return ret;
}

TabsCtrl::TabsCtrl() {
    kind = kindTabs;
}

// must be called after LayoutTabs()
static void TabsCtrlUpdateAfterChangingTabsCount(TabsCtrl* tabs) {
    HWND hwnd = tabs->hwnd;
    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }
    tabs->tabBeingClosed = -1;
    Point mousePos = HwndGetCursorPos(hwnd);
    auto tabState = tabs->TabStateFromMousePosition(mousePos);
    bool canClose = tabState.tabInfo && tabState.tabInfo->canClose;
    bool overClose = tabState.overClose && canClose;
    int tabUnderMouse = tabState.tabIdx;
    tabs->tabHighlighted = tabUnderMouse;
    tabs->tabHighlightedClose = overClose ? tabUnderMouse : -1;
    if (tabs->draggingTab) {
        tabs->draggingTab = false;
        ImageList_EndDrag();
    }
}

TabsCtrl::~TabsCtrl() {
}

static void TriggerSelectionChanged(TabsCtrl* tabs) {
    if (!tabs->onSelectionChanged.IsValid()) {
        return;
    }
    TabsCtrl::SelectionChangedEvent ev;
    ev.tabs = tabs;
    tabs->onSelectionChanged.Call(&ev);
}

static bool TriggerSelectionChanging(TabsCtrl* tabs) {
    if (!tabs->onSelectionChanging.IsValid()) {
        // allow changing
        return false;
    }

    TabsCtrl::SelectionChangingEvent ev;
    tabs->onSelectionChanging.Call(&ev);
    return (LRESULT)ev.preventChanging;
}

static void TriggerTabMigration(TabsCtrl* tabs, int tabIdx, Point p) {
    if (!tabs->onTabMigration.IsValid()) {
        return;
    }
    TabsCtrl::MigrationEvent ev;
    ev.tabs = tabs;
    ev.tabIdx = tabIdx;
    ev.releasePoint = p;
    tabs->onTabMigration.Call(&ev);
}

static void TriggerTabClosed(TabsCtrl* tabs, int tabIdx) {
    if ((tabIdx < 0) || !tabs->onTabClosed.IsValid()) {
        return;
    }
    TabsCtrl::ClosedEvent ev;
    ev.tabs = tabs;
    ev.tabIdx = tabIdx;
    tabs->onTabClosed.Call(&ev);
}

static void TriggerTabDragged(TabsCtrl* tabs, int tab1, int tab2) {
    if (!tabs->onTabDragged.IsValid()) {
        return;
    }
    TabsCtrl::DraggedEvent ev;
    ev.tabs = tabs;
    ev.tab1 = tab1;
    ev.tab2 = tab2;
    tabs->onTabDragged.Call(&ev);
}

static void UpdateAfterDrag(TabsCtrl* tabsCtrl, int tabIdxFrom, int tabIdxTo) {
    int nTabs = tabsCtrl->TabCount();
    bool badState =
        (tabIdxFrom == tabIdxTo) || (tabIdxFrom < 0) || (tabIdxTo < 0) || (tabIdxFrom >= nTabs) || (tabIdxTo > nTabs);
    if (badState) {
        logfa("tabIdxFrom: %d, tabIdxTo: %d, nTabs: %d\n", tabIdxFrom, tabIdxTo, nTabs);
        ReportDebugIf(true);
        return;
    }

    auto&& tabs = tabsCtrl->tabs;
    TabInfo* moved = tabs.At(tabIdxFrom);
    tabs.RemoveAt(tabIdxFrom);
    if (tabIdxFrom < tabIdxTo) {
        // we moved from left to right e.g. from 1 to 3
        // after removing 1 we insert not at 3 but 2
        tabIdxTo -= 1;
    }
    tabs.InsertAt(tabIdxTo, moved);
    tabsCtrl->SetSelected(tabIdxTo);
    tabsCtrl->LayoutTabs();
    TabsCtrlUpdateAfterChangingTabsCount(tabsCtrl);
}

LRESULT TabsCtrl::OnNotifyReflect(WPARAM wp, LPARAM lp) {
    NMHDR* hdr = (NMHDR*)lp;
    switch (hdr->code) {
        case TCN_SELCHANGING:
            return (LRESULT)TriggerSelectionChanging(this);

        case TCN_SELCHANGE:
            TriggerSelectionChanged(this);
            break;

        case TTN_GETDISPINFOA:
        case TTN_GETDISPINFOW:
            break;
    }
    return 0;
}

LRESULT TabsCtrl::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // TCITEMW* tcs = nullptr;

    // I'm seeing crashes when ending a drag in WM_LBUTTONUP
    // because the MainWindow has been destroyed
    if (!IsWindow(hwnd)) {
        return 0;
    }

    Point mousePos = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    if (WM_MOUSELEAVE == msg) {
        mousePos = HwndGetCursorPos(hwnd);
    }

    TabsCtrl::MouseState tabState;

    bool overClose = false;
    bool canClose = true;
    int tabUnderMouse = -1;

    if ((msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || (msg == WM_MOUSELEAVE)) {
        tabState = TabStateFromMousePosition(mousePos);
        tabUnderMouse = tabState.tabIdx;
        canClose = tabState.tabInfo && tabState.tabInfo->canClose;
        overClose = tabState.overClose && canClose;
        lastMousePos = mousePos;
        // TempStr msgName = WinMsgNameTemp(msg);
        //  logfa("msg; %s, tabUnderMouse: %d, overClose: %d\n", msgName, tabUnderMouse, (int)overClose);
    }

    if (draggingTab && msg == WM_MOUSEMOVE) {
        POINT p;
        p.x = mousePos.x;
        p.y = mousePos.y;
        MapWindowPoints(hwnd, NULL, &p, 1);
        // logfa("%s moving to: %d %d\n", WinMsgNameTemp(msg), p.x, p.y);
        ImageList_DragMove(p.x, p.y);
        return 0;
    }

    switch (msg) {
        case WM_NCHITTEST: {
            if (false) {
                return HTCLIENT;
            }
            // parts that are HTTRANSPARENT are used to move the window
            if (!inTitleBar || hwnd == GetCapture()) {
                return HTCLIENT;
            }
            HwndScreenToClient(hwnd, mousePos);
            tabState = TabStateFromMousePosition(mousePos);
            if (tabState.tabIdx >= 0) {
                return HTCLIENT;
            }
            return HTTRANSPARENT;
        }

        case WM_SIZE:
            LayoutTabs();
            break;

        case WM_MOUSELEAVE:
            if (tabHighlighted != tabUnderMouse) {
                tabHighlighted = tabUnderMouse;
                HwndScheduleRepaint(hwnd);
            }
            break;

        case WM_MOUSEMOVE: {
            TrackMouseLeave(hwnd);
            bool isDragging = (GetCapture() == hwnd);
            int hl = tabHighlighted;
            if (isDragging) {
                // move the tab out: draw it as a image and drag around the screen
                draggingTab = true;
                TabInfo* thl = GetTab(hl);
                HBITMAP hbmp = RenderForDragging(hl);
                if (!hbmp) {
                    logfa("TabsCtrl::WndProc: RenderForDragging failed for tab %d\n", hl);
                    return 0;
                }
                HIMAGELIST himl = ImageList_Create(thl->r.dx, thl->r.dy, 0, 1, 0);
                ImageList_Add(himl, hbmp, NULL);
                ImageList_BeginDrag(himl, 0, grabLocation.x, grabLocation.y);
                DeleteObject(hbmp);
                DeleteObject(himl);
                POINT p(mousePos.x, mousePos.y);
                MapWindowPoints(hwnd, NULL, &p, 1);
                ImageList_DragEnter(NULL, p.x, p.y);
                return 0;
            }

            if (hl != tabUnderMouse) {
                tabHighlighted = tabUnderMouse;
                // logf("tab: WM_MOUSEMOVE: tabHighlighted = tabUnderMouse: %d\n", tabHighlighted);
                // note: hl == -1 possible repro: we start drag, a file gets loaded via DDE etc.
                // which re-layouts tabs and mouse is no longer over a tab
                if (isDragging && hl != -1) {
                    // send notification if the highlighted tab is dragged over another
                    if (!GetTab(tabUnderMouse)->isPinned) {
                        TriggerTabDragged(this, hl, tabUnderMouse);
                        UpdateAfterDrag(this, hl, tabUnderMouse);
                    }
                } else {
                    // highlight a different tab
                    HwndScheduleRepaint(hwnd);
                }
                return 0;
            }
            int xHl = -1;
            if (overClose && !isDragging) {
                xHl = hl;
            }
            // logfa("inX=%d, hl=%d, xHl=%d, xHighlighted=%d\n", (int)inX, hl, xHl, tab->xHighlighted);
            if (tabHighlightedClose != xHl) {
                // logfa("before invalidate, xHl=%d, xHighlited=%d\n", xHl, tab->xHighlighted);
                tabHighlightedClose = xHl;
                HwndScheduleRepaint(hwnd);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            tabHighlighted = tabUnderMouse;
            if (overClose) {
                HwndScheduleRepaint(hwnd);
                tabBeingClosed = tabUnderMouse;
                return 0;
            }
            if (tabUnderMouse < 0) {
                return 0;
            }

            int selectedTab = GetSelected();
            if (tabUnderMouse != selectedTab) {
                bool stopChange = TriggerSelectionChanging(this);
                if (stopChange) {
                    return 0;
                }
                SetSelected(tabUnderMouse);
                TriggerSelectionChanged(this);
            }
            TabInfo* ti = GetTab(GetSelected());
            if (ti->isPinned) {
                return 0;
            }

            grabLocation.x = mousePos.x - ti->r.x;
            grabLocation.y = mousePos.y - ti->r.y;
            SetCapture(hwnd);
            return 0;
        }

        case WM_LBUTTONUP: {
            bool isDragging = (GetCapture() == hwnd);
            if (isDragging) {
                ReleaseCapture();
            }
            if (tabBeingClosed != -1 && tabUnderMouse == tabBeingClosed && overClose) {
                // send notification that the tab is closed
                TriggerTabClosed(this, tabBeingClosed);
                HwndScheduleRepaint(hwnd);
                tabBeingClosed = -1;
                return 0;
            }
            // we don't always get WM_MOUSEMOVE before WM_LBUTTONUP so
            // update tabHighlighted
            tabHighlighted = tabUnderMouse;

            if (!draggingTab) {
                return 0;
            }
            draggingTab = false;
            ImageList_EndDrag();
            int selectedTab = GetSelected();
            if (tabUnderMouse < 0) {
                // migrate to new/different window
                POINT p(mousePos.x, mousePos.y);
                ClientToScreen(hwnd, &p);
                Point scPoint(p.x, p.y);
                TriggerTabMigration(this, selectedTab, scPoint);
                return 0;
            }
            int dstIdx = tabUnderMouse;
            if (tabState.inRightHalf) {
                dstIdx++;
            }
            if (dstIdx == selectedTab) {
                return 0;
            }
            if ((dstIdx < TabCount()) && GetTab(dstIdx)->isPinned) {
                return 0;
            }
            TriggerTabDragged(this, selectedTab, dstIdx);
            UpdateAfterDrag(this, selectedTab, dstIdx);
            HwndScheduleRepaint(hwnd);
            return 0;
        }

        case WM_MBUTTONDOWN: {
            // middle-clicking unconditionally closes the tab

            tabBeingClosed = tabUnderMouse;
            if (tabBeingClosed < 0 || !canClose) {
                return 0;
            }
            TriggerTabClosed(this, tabBeingClosed);
            HwndScheduleRepaint(hwnd);
            return 0;
        }

        case WM_ERASEBKGND:
            return TRUE; // we handled it so don't erase

        case WM_PAINT: {
            PAINTSTRUCT ps;
            RECT rc = ClientRECT(hwnd);
            HDC hdc = BeginPaint(hwnd, &ps);
            DoubleBuffer buffer(hwnd, ToRect(rc));
            Paint(buffer.GetDC(), rc);
            buffer.Flush(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

#if 0
        case WM_PAINT: {
            RECT rc;
            GetUpdateRect(hwnd, &rc, FALSE);
            // TODO: when is wp != nullptr?
            hdc = wp ? (HDC)wp : BeginPaint(hwnd, &ps);
#if 1
            DoubleBuffer buffer(hwnd, ToRect(rc));
            Paint(buffer.GetDC(), rc);
            buffer.Flush(hdc);
#else
            Paint(hdc, rc);
#endif
            ValidateRect(hwnd, nullptr);
            if (!wp) {
                EndPaint(hwnd, &ps);
            }
            return 0;
        }
#endif
    }

    return WndProcDefault(hwnd, msg, wp, lp);
}

HWND TabsCtrl::Create(TabsCtrl::CreateArgs& argsIn) {
    withToolTips = argsIn.withToolTips;
    tabDefaultDx = argsIn.tabDefaultDx;

    CreateControlArgs args;
    args.parent = argsIn.parent;
    args.font = argsIn.font;
    args.className = WC_TABCONTROLW;
    args.style = WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | TCS_FOCUSNEVER | TCS_FIXEDWIDTH | TCS_FORCELABELLEFT;
    if (withToolTips) {
        args.style |= TCS_TOOLTIPS;
    }

    HWND hwnd = CreateControl(args);
    if (!hwnd) {
        return nullptr;
    }

    if (withToolTips) {
        HWND ttHwnd = GetToolTipsHwnd();
        SetWindowStyle(ttHwnd, TTS_NOPREFIX, true);
        TOOLINFO ti{};
        ti.cbSize = sizeof(ti);
        ti.hwnd = hwnd;
        ti.uId = 0;
        ti.uFlags = TTF_SUBCLASS;
        ti.lpszText = (WCHAR*)L"placeholder tooltip";
        SetRectEmpty(&ti.rect);
        RECT r = ti.rect;
        SendMessageW(ttHwnd, TTM_ADDTOOL, 0, (LPARAM)&ti);
    }
    return hwnd;
}

Size TabsCtrl::GetIdealSize() {
    Size sz{32, 128};
    return sz;
}

int TabsCtrl::TabCount() {
    int n = TabCtrl_GetItemCount(hwnd);
    return n;
}

// takes ownership of tab
int TabsCtrl::InsertTab(int idx, TabInfo* tab) {
    ReportIf(idx < 0);
    TCITEMW item{};
    item.mask = TCIF_TEXT;
    item.pszText = ToWStrTemp(tab->text);
    int res = TabCtrl_InsertItem(hwnd, idx, &item);
    if (res < 0) {
        return res;
    }
    tabs.InsertAt(idx, tab);
    // LayoutTabs() must be before SetSelected() because SetSelected()
    // triggers sync repaint which paints tab texts in wrong positions
    // because we didn't position them yet in layout.
    LayoutTabs();
    SetSelected(idx);
    TabsCtrlUpdateAfterChangingTabsCount(this);
    return idx;
}

void TabsCtrl::SetTextAndTooltip(int idx, const char* text, const char* tooltip) {
    TabInfo* tab = GetTab(idx);
    str::ReplaceWithCopy(&tab->text, text);
    str::ReplaceWithCopy(&tab->tooltip, tooltip);
    LayoutTabs();
}

// returns userData because it's not owned by TabsCtrl
UINT_PTR TabsCtrl::RemoveTab(int idx) {
    ReportIf(idx < 0);
    ReportIf(idx >= TabCount());
    BOOL ok = TabCtrl_DeleteItem(hwnd, idx);
    ReportIf(!ok);
    TabInfo* tab = tabs[idx];
    UINT_PTR userData = tab->userData;
    tabs.RemoveAt(idx);
    delete tab;
    int selectedTab = GetSelected();
    if (idx < selectedTab) {
        SetSelected(selectedTab - 1);
    } else if (idx == selectedTab) {
        SetSelected(0);
    }
    LayoutTabs();
    TabsCtrlUpdateAfterChangingTabsCount(this);
    return userData;
}

void TabsCtrl::SwapTabs(int idx1, int idx2) {
    TabInfo* tmp = tabs[idx1];
    tabs[idx1] = tabs[idx2];
    tabs[idx2] = tmp;
}

// Note: the caller should take care of deleting userData
void TabsCtrl::RemoveAllTabs() {
    TabCtrl_DeleteAllItems(hwnd);
    DeleteVecMembers(tabs);
    tabs.Reset();
    LayoutTabs();
    TabsCtrlUpdateAfterChangingTabsCount(this);
}

TabInfo* TabsCtrl::GetTab(int idx) {
    return tabs[idx];
}

int TabsCtrl::GetSelected() {
    int idx = TabCtrl_GetCurSel(hwnd);
    return idx;
}

int TabsCtrl::SetSelected(int idx) {
    int nTabs = TabCount();
    if (idx < 0 || idx >= nTabs) {
        logf("TabsCtrl::SetSelected(): idx: %d, TabsCount(): %d\n", idx, nTabs);
    }
    ReportIf(idx < 0 || idx >= nTabs);
    int prevSelectedIdx = TabCtrl_SetCurSel(hwnd, idx);
    return prevSelectedIdx;
}

void TabsCtrl::SetHighlighted(int idx) {
    tabForceShowSelected = idx;
    HwndRepaintNow(hwnd);
}

HWND TabsCtrl::GetToolTipsHwnd() {
    HWND res = TabCtrl_GetToolTips(hwnd);
    return res;
}
