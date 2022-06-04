/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "resource.h"
#include "Commands.h"
#include "Caption.h"
#include "Menu.h"
#include "TableOfContents.h"
#include "Tabs.h"

#include "utils/Log.h"

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

#define kTabDefaultBgCol (COLORREF) - 1

struct TabPainter {
    TabsCtrl* tabsCtrl = nullptr;
    PathData* data = nullptr;
    int width = -1;
    int height = -1;
    HWND hwnd = nullptr;

    // TODO: set those to reasonable defaults
    COLORREF currBgCol = kTabDefaultBgCol;
    COLORREF tabBackgroundBg = 0;
    COLORREF tabBackgroundText = 0;
    COLORREF tabBackgroundCloseX = 0;
    COLORREF tabBackgroundCloseCircle = 0;
    COLORREF tabSelectedBg = 0;
    COLORREF tabSelectedText = 0;
    COLORREF tabSelectedCloseX = 0;
    COLORREF tabSelectedCloseCircle = 0;
    COLORREF tabHighlightedBg = 0;
    COLORREF tabHighlightedText = 0;
    COLORREF tabHighlightedCloseX = 0;
    COLORREF tabHighlightedCloseCircle = 0;
    COLORREF tabHoveredCloseX = 0;
    COLORREF tabHoveredCloseCircle = 0;
    COLORREF tabClickedCloseX = 0;
    COLORREF tabClickedCloseCircle = 0;

    int selectedTabIdx = -1;
    int highlighted = -1;
    int xClicked = -1;
    int xHighlighted = -1;
    int nextTab = -1;
    bool isDragging = false;
    bool inTitlebar = false;
    LPARAM mouseCoordinates = 0;

    TabPainter(TabsCtrl* ctrl, Size tabSize);
    ~TabPainter();
    bool Reshape(int dx, int dy);
    int IndexFromPoint(int x, int y, bool* inXbutton = nullptr) const;
    void Invalidate(int index) const;
    void Paint(HDC hdc, RECT& rc) const;
    int Count() const;
};

TabPainter::TabPainter(TabsCtrl* ctrl, Size tabSize) {
    tabsCtrl = ctrl;
    hwnd = tabsCtrl->hwnd;
    Reshape(tabSize.dx, tabSize.dy);
}

TabPainter::~TabPainter() {
    delete data;
}

// Calculates tab's elements, based on its width and height.
// Generates a GraphicsPath, which is used for painting the tab, etc.
bool TabPainter::Reshape(int dx, int dy) {
    dx--;
    if (width == dx && height == dy) {
        return false;
    }
    width = dx;
    height = dy;

    GraphicsPath shape;
    // define tab's body
    shape.AddRectangle(Gdiplus::Rect(0, 0, width, height));
    shape.SetMarker();

    // define "x"'s circle
    int c = int((float)height * 0.78f + 0.5f); // size of bounding square for the circle
    int maxC = DpiScale(hwnd, 17);
    if (height > maxC) {
        c = DpiScale(hwnd, 17);
    }
    Gdiplus::Point p(width - c - DpiScale(hwnd, 3), (height - c) / 2); // circle's position
    shape.AddEllipse(p.X, p.Y, c, c);
    shape.SetMarker();
    // define "x"
    int o = int((float)c * 0.286f + 0.5f); // "x"'s offset
    shape.AddLine(p.X + o, p.Y + o, p.X + c - o, p.Y + c - o);
    shape.StartFigure();
    shape.AddLine(p.X + c - o, p.Y + o, p.X + o, p.Y + c - o);
    shape.SetMarker();

    delete data;
    data = new PathData();
    shape.GetPathData(data);
    return true;
}

// Finds the index of the tab, which contains the given point.
int TabPainter::IndexFromPoint(int x, int y, bool* inXbutton) const {
    Gdiplus::Point point(x, y);
    Graphics gfx(hwnd);
    GraphicsPath shapes(data->Points, data->Types, data->Count);
    GraphicsPath shape;
    Gdiplus::GraphicsPathIterator iterator(&shapes);
    iterator.NextMarker(&shape);

    Rect rClient = ClientRect(hwnd);
    float yPosTab = inTitlebar ? 0.0f : float(rClient.dy - height - 1);
    gfx.TranslateTransform(1.0f, yPosTab);
    for (int i = 0; i < Count(); i++) {
        Gdiplus::Point pt(point);
        gfx.TransformPoints(Gdiplus::CoordinateSpaceWorld, Gdiplus::CoordinateSpaceDevice, &pt, 1);
        if (shape.IsVisible(pt, &gfx)) {
            iterator.NextMarker(&shape);
            if (inXbutton) {
                *inXbutton = shape.IsVisible(pt, &gfx) != 0;
            }
            return i;
        }
        gfx.TranslateTransform(float(width + 1), 0.0f);
    }
    if (inXbutton) {
        *inXbutton = false;
    }
    return -1;
}

// Invalidates the tab's region in the client area.
void TabPainter::Invalidate(int index) const {
    if (index < 0) {
        return;
    }

    Graphics gfx(hwnd);
    GraphicsPath shapes(data->Points, data->Types, data->Count);
    GraphicsPath shape;
    Gdiplus::GraphicsPathIterator iterator(&shapes);
    iterator.NextMarker(&shape);
    Region region(&shape);

    Rect rClient = ClientRect(hwnd);
    float yPosTab = inTitlebar ? 0.0f : float(rClient.dy - height - 1);
    gfx.TranslateTransform(float((width + 1) * index) + 1.0f, yPosTab);
    HRGN hRgn = region.GetHRGN(&gfx);
    InvalidateRgn(hwnd, hRgn, FALSE);
    DeleteObject(hRgn);
}

// Paints the tabs that intersect the window's update rectangle.
void TabPainter::Paint(HDC hdc, RECT& rc) const {
    IntersectClipRect(hdc, rc.left, rc.top, rc.right, rc.bottom);
#if 0
        // paint the background
        bool isTranslucentMode = inTitlebar && dwm::IsCompositionEnabled();
        if (isTranslucentMode) {
            PaintParentBackground(hwnd, hdc);
        } else {
            // note: not sure what color should be used here and painting
            // background works fine
            /*HBRUSH brush = CreateSolidBrush(colors.bar);
            FillRect(hdc, &rc, brush);
            DeleteObject(brush);*/
        }
#else
    PaintParentBackground(hwnd, hdc);
#endif
    // TODO: GDI+ doesn't seem to cope well with SetWorldTransform
    XFORM ctm = {1.0, 0, 0, 1.0, 0, 0};
    SetWorldTransform(hdc, &ctm);

    Graphics gfx(hdc);
    gfx.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    gfx.SetCompositingQuality(CompositingQualityHighQuality);
    gfx.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    gfx.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    gfx.SetPageUnit(UnitPixel);
    GraphicsPath shapes(data->Points, data->Types, data->Count);
    GraphicsPath shape;
    Gdiplus::GraphicsPathIterator iterator(&shapes);

    SolidBrush br(Color(0, 0, 0));
    Pen pen(&br, 2.0f);

    Font f(hdc, GetDefaultGuiFont());
    // TODO: adjust these constant values for DPI?
    Gdiplus::RectF layout((float)DpiScale(hwnd, 3), 1.0f, float(width - DpiScale(hwnd, 20)), (float)height);
    StringFormat sf(StringFormat::GenericDefault());
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    sf.SetLineAlignment(StringAlignmentCenter);
    sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

    float yPosTab = inTitlebar ? 0.0f : float(ClientRect(hwnd).dy - height - 1);
    for (int i = 0; i < Count(); i++) {
        gfx.ResetTransform();
        gfx.TranslateTransform(1.f + (float)(width + 1) * i - (float)rc.left, yPosTab - (float)rc.top);

        if (!gfx.IsVisible(0, 0, width + 1, height + 1)) {
            continue;
        }

        // Get the correct colors based on the state and the current theme
        COLORREF bgCol = tabBackgroundBg;
        COLORREF textCol = tabBackgroundText;
        COLORREF xColor = tabBackgroundCloseX;
        COLORREF circleColor = tabBackgroundCloseCircle;

        if (selectedTabIdx == i) {
            bgCol = tabSelectedBg;
            textCol = tabSelectedText;
            xColor = tabSelectedCloseX;
            circleColor = tabSelectedCloseCircle;
        } else if (highlighted == i) {
            bgCol = tabHighlightedBg;
            textCol = tabHighlightedText;
            xColor = tabHighlightedCloseX;
            circleColor = tabHighlightedCloseCircle;
        }
        if (xHighlighted == i) {
            xColor = tabHoveredCloseX;
            circleColor = tabHoveredCloseCircle;
        }
        if (xClicked == i) {
            xColor = tabClickedCloseX;
            circleColor = tabClickedCloseCircle;
        }

        // paint tab's body
        gfx.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        iterator.NextMarker(&shape);
        br.SetColor(GdiRgbFromCOLORREF(bgCol));
        Gdiplus::Point points[4];
        shape.GetPathPoints(points, 4);
        Gdiplus::Rect body(points[0].X, points[0].Y, points[2].X - points[0].X, points[2].Y - points[0].Y);
        body.Inflate(0, 0);
        gfx.SetClip(body);
        body.Inflate(5, 5);
        gfx.FillRectangle(&br, body);
        gfx.ResetClip();

        // draw tab's text
        gfx.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
        br.SetColor(GdiRgbFromCOLORREF(textCol));
        char* text = tabsCtrl->GetTabText(i);
        gfx.DrawString(ToWstrTemp(text), -1, &f, layout, &sf, &br);

        // paint "x"'s circle
        iterator.NextMarker(&shape);
        // bool closeCircleEnabled = true;
        if ((xClicked == i || xHighlighted == i) /*&& closeCircleEnabled*/) {
            br.SetColor(GdiRgbFromCOLORREF(circleColor));
            gfx.FillPath(&br, &shape);
        }

        // paint "x"
        iterator.NextMarker(&shape);
        pen.SetColor(GdiRgbFromCOLORREF(xColor));
        gfx.DrawPath(&pen, &shape);
        iterator.Rewind();
    }
}

int TabPainter::Count() const {
    int n = tabsCtrl->GetTabCount();
    return n;
}

static void SetTabTitle(WindowTab* tab) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    int idx = win->tabs.Find(tab);
    const char* title = tab->GetTabTitle();
    win->tabsCtrl->SetTabText(idx, title);
    auto tooltip = tab->filePath.Get();
    win->tabsCtrl->SetTooltip(idx, tooltip);
}

static void NO_INLINE SwapTabs(MainWindow* win, int tab1, int tab2) {
    if (tab1 == tab2 || tab1 < 0 || tab2 < 0) {
        return;
    }

    auto&& tabs = win->tabs;
    std::swap(tabs.at(tab1), tabs.at(tab2));
    SetTabTitle(tabs.at(tab1));
    SetTabTitle(tabs.at(tab2));

    int current = win->tabsCtrl->GetSelectedTabIndex();
    int newSelected = tab1;
    if (tab1 == current) {
        newSelected = tab2;
    }
    win->tabsCtrl->SetSelectedTabByIndex(newSelected);
}

static LRESULT CALLBACK TabBarProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, __unused UINT_PTR uIdSubclass,
                                   __unused DWORD_PTR dwRefData) {
    PAINTSTRUCT ps;
    HDC hdc;
    int index;
    LPTCITEM tcs;

    TabPainter* tab = (TabPainter*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    if (WM_NCDESTROY == msg) {
        RemoveWindowSubclass(hwnd, TabBarProc, 0);
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    TabsCtrl* tabs = tab->tabsCtrl;

    switch (msg) {
        case WM_DESTROY:
            delete tab;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)0);
            break;

        case TCM_INSERTITEM:
            index = (int)wp;
            if (index <= tab->selectedTabIdx) {
                tab->selectedTabIdx++;
            }
            tab->xClicked = -1;
            InvalidateRgn(hwnd, nullptr, FALSE);
            UpdateWindow(hwnd);
            break;

        case TCM_SETITEM:
            // TODO: this should not be necessary
            index = (int)wp;
            tcs = (LPTCITEM)lp;
            if (TCIF_TEXT & tcs->mask) {
                tab->Invalidate(index);
            }
            break;

        case TCM_DELETEITEM:
            // TODO: this should not be necessary
            index = (int)wp;
            if (index < tab->selectedTabIdx) {
                tab->selectedTabIdx--;
            } else if (index == tab->selectedTabIdx) {
                tab->selectedTabIdx = -1;
            }
            tab->xClicked = -1;
            if (tab->Count()) {
                InvalidateRgn(hwnd, nullptr, FALSE);
                UpdateWindow(hwnd);
            }
            break;

        case TCM_DELETEALLITEMS:
            tab->selectedTabIdx = -1;
            tab->highlighted = -1;
            tab->xClicked = -1;
            tab->xHighlighted = -1;
            break;

        case TCM_SETITEMSIZE:
            if (tab->Reshape(LOWORD(lp), HIWORD(lp))) {
                tab->xClicked = -1;
                if (tab->Count()) {
                    InvalidateRgn(hwnd, nullptr, FALSE);
                    UpdateWindow(hwnd);
                }
            }
            break;

        case TCM_GETCURSEL:
            return tab->selectedTabIdx;

        case TCM_SETCURSEL: {
            index = (int)wp;
            if (index >= tab->Count()) {
                return -1;
            }
            int previous = tab->selectedTabIdx;
            if (index != tab->selectedTabIdx) {
                tab->Invalidate(tab->selectedTabIdx);
                tab->Invalidate(index);
                tab->selectedTabIdx = index;
                UpdateWindow(hwnd);
            }
            return previous;
        }

        case WM_NCHITTEST: {
            if (!tab->inTitlebar || hwnd == GetCapture()) {
                return HTCLIENT;
            }
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &pt);
            if (-1 != tab->IndexFromPoint(pt.x, pt.y)) {
                return HTCLIENT;
            }
        }
            return HTTRANSPARENT;

        case WM_MOUSELEAVE:
            PostMessageW(hwnd, WM_MOUSEMOVE, 0xFF, 0);
            return 0;

        case WM_MOUSEMOVE: {
            tab->mouseCoordinates = lp;

            if (0xff != wp) {
                TrackMouseLeave(hwnd);
            }

            bool inX = false;
            int hl = wp == 0xFF ? -1 : tab->IndexFromPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), &inX);
            bool didChangeTabs = false;
            if (tab->isDragging && hl == -1) {
                // preserve the highlighted tab if it's dragged outside the tabs' area
                hl = tab->highlighted;
                didChangeTabs = true;
            }
            if (tab->highlighted != hl) {
                if (tab->isDragging) {
                    // send notification if the highlighted tab is dragged over another
                    MainWindow* win = FindMainWindowByHwnd(hwnd);
                    int tabNo = tab->highlighted;
                    if (tabs->onTabDragged) {
                        TabDraggedEvent ev;
                        ev.tabs = win->tabsCtrl;
                        ev.tab1 = tabNo;
                        ev.tab2 = hl;
                        tabs->onTabDragged(&ev);
                    }
                }

                tab->Invalidate(hl);
                tab->Invalidate(tab->highlighted);
                tab->highlighted = hl;
                didChangeTabs = true;
            }
            int xHl = inX && !tab->isDragging ? hl : -1;
            if (tab->xHighlighted != xHl) {
                tab->Invalidate(xHl);
                tab->Invalidate(tab->xHighlighted);
                tab->xHighlighted = xHl;
            }
            if (!inX) {
                tab->xClicked = -1;
            }
            if (didChangeTabs && tab->highlighted >= 0) {
                int idx = tab->highlighted;
                auto tabsCtrl = tab->tabsCtrl;
                tabsCtrl->MaybeUpdateTooltipText(idx);
            }
        }
            return 0;

        case WM_LBUTTONDOWN:
            bool inX;
            tab->nextTab = tab->IndexFromPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), &inX);
            if (inX) {
                tab->Invalidate(tab->nextTab);
                tab->xClicked = tab->nextTab;
            } else if (tab->nextTab != -1) {
                if (tab->nextTab != tab->selectedTabIdx) {
                    // HWND hwndParent = ::GetParent(hwnd);
                    NMHDR nmhdr = {hwnd, IDC_TABBAR, (UINT)TCN_SELCHANGING};
                    BOOL stopChange = (BOOL)tabs->OnNotifyReflect(IDC_TABBAR, (LPARAM)&nmhdr);
                    // TODO: why SendMessage doesn't work?
                    // BOOL stopChange = SendMessageW(hwndParent, WM_NOTIFY, IDC_TABBAR, (LPARAM)&nmhdr);
                    if (!stopChange) {
                        TabCtrl_SetCurSel(hwnd, tab->nextTab);
                        nmhdr = {hwnd, IDC_TABBAR, (UINT)TCN_SELCHANGE};
                        tabs->OnNotifyReflect(IDC_TABBAR, (LPARAM)&nmhdr);
                        return 0;
                    }
                    return 0;
                }
                tab->isDragging = true;
                SetCapture(hwnd);
            }
            return 0;

        case WM_LBUTTONUP:
            if (tab->xClicked != -1) {
                // send notification that the tab is closed
                MainWindow* win = FindMainWindowByHwnd(hwnd);
                if (tabs->onTabClosed) {
                    TabClosedEvent ev;
                    ev.tabs = tabs;
                    ev.tabIdx = tab->xClicked;
                    tabs->onTabClosed(&ev);
                }
                tab->Invalidate(tab->xClicked);
                tab->xClicked = -1;
            }
            if (tab->isDragging) {
                tab->isDragging = false;
                ReleaseCapture();
            }
            return 0;

        case WM_MBUTTONDOWN:
            // middle-clicking unconditionally closes the tab
            {
                tab->nextTab = tab->IndexFromPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
                tab->xClicked = tab->nextTab;
                tab->Invalidate(tab->nextTab);
            }
            return 0;

        case WM_MBUTTONUP:
            if (tab->xClicked != -1) {
                if (tabs->onTabClosed) {
                    TabClosedEvent ev;
                    ev.tabs = tabs;
                    ev.tabIdx = tab->xClicked;
                    tabs->onTabClosed(&ev);
                }
                int clicked = tab->xClicked;
                tab->Invalidate(clicked);
                tab->xClicked = -1;
            }
            return 0;

        case WM_ERASEBKGND:
            return TRUE;

        case WM_PAINT: {
            RECT rc;
            GetUpdateRect(hwnd, &rc, FALSE);
            // TODO: when is wp != nullptr?
            hdc = wp ? (HDC)wp : BeginPaint(hwnd, &ps);

            DoubleBuffer buffer(hwnd, Rect::FromRECT(rc));
            tab->Paint(buffer.GetDC(), rc);
            buffer.Flush(hdc);

            ValidateRect(hwnd, nullptr);
            if (!wp) {
                EndPaint(hwnd, &ps);
            }
            return 0;
        }
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

#define kTabMinDx 100
#define kTabBarDy 24

int GetTabbarHeight(HWND hwnd, float factor) {
    int dy = DpiScale(hwnd, kTabBarDy);
    return (int)(dy * factor);
}

static inline Size GetTabSize(HWND hwnd) {
    int dx = DpiScale(hwnd, std::max(gGlobalPrefs->tabWidth, kTabMinDx));
    int dy = DpiScale(hwnd, kTabBarDy);
    return Size(dx, dy);
}

static void ShowTabBar(MainWindow* win, bool show) {
    if (show == win->tabsVisible) {
        return;
    }
    win->tabsVisible = show;
    win->tabsCtrl->SetIsVisible(show);
    RelayoutWindow(win);
}

void UpdateTabWidth(MainWindow* win) {
    int count = (int)win->tabs.size();
    bool showSingleTab = gGlobalPrefs->useTabs || win->tabsInTitlebar;
    bool showTabs = (count > 1) || (showSingleTab && (count > 0));
    if (!showTabs) {
        ShowTabBar(win, false);
        return;
    }
    ShowTabBar(win, true);
    Rect rect = ClientRect(win->tabsCtrl->hwnd);
    Size tabSize = GetTabSize(win->hwndFrame);
    auto maxDx = (rect.dx - 3) / count;
    tabSize.dx = std::min(tabSize.dx, maxDx);
    win->tabsCtrl->SetItemSize(tabSize);
    win->tabsCtrl->MaybeUpdateTooltip();
}

static void RemoveTab(MainWindow* win, int idx) {
    WindowTab* tab = win->tabs.at(idx);
    UpdateTabFileDisplayStateForTab(tab);
    win->tabSelectionHistory->Remove(tab);
    win->tabs.Remove(tab);
    if (tab == win->currentTab) {
        win->ctrl = nullptr;
        win->currentTab = nullptr;
    }
    delete tab;
    win->tabsCtrl->RemoveTab(idx);
    UpdateTabWidth(win);
}

static void WinTabClosedHandler(MainWindow* win, TabsCtrl* tabs, int closedTabIdx) {
    int current = win->tabsCtrl->GetSelectedTabIndex();
    if (closedTabIdx == current) {
        CloseCurrentTab(win);
    } else {
        RemoveTab(win, closedTabIdx);
    }
}

// Selects the given tab (0-based index)
// TODO: this shouldn't go through the same notifications, just do it
void TabsSelect(MainWindow* win, int tabIndex) {
    auto& tabs = win->tabs;
    int count = tabs.Size();
    if (count < 2 || tabIndex < 0 || tabIndex >= count) {
        return;
    }
    TabsCtrl* tabsCtrl = win->tabsCtrl;
    int currIdx = tabsCtrl->GetSelectedTabIndex();
    if (tabIndex == currIdx) {
        return;
    }

    // same work as in onSelectionChanging and onSelectionChanged
    SaveCurrentWindowTab(win);
    int prevIdx = tabsCtrl->SetSelectedTabByIndex(tabIndex);
    if (prevIdx < 0) {
        return;
    }
    WindowTab* tab = tabs[tabIndex];
    LoadModelIntoTab(tab);
}

void CreateTabbar(MainWindow* win) {
    TabsCtrl* tabsCtrl = new TabsCtrl();
    tabsCtrl->onTabClosed = [win](TabClosedEvent* ev) { WinTabClosedHandler(win, ev->tabs, ev->tabIdx); };
    tabsCtrl->onSelectionChanging = [win](TabsSelectionChangingEvent* ev) -> bool {
        // TODO: Should we allow the switch of the tab if we are in process of printing?
        SaveCurrentWindowTab(win);
        return false;
    };

    tabsCtrl->onSelectionChanged = [win](TabsSelectionChangedEvent* ev) {
        int currentIdx = win->tabsCtrl->GetSelectedTabIndex();
        WindowTab* tab = win->tabs[currentIdx];
        LoadModelIntoTab(tab);
    };
    tabsCtrl->onTabDragged = [win](TabDraggedEvent* ev) {
        int tab1 = ev->tab1;
        int tab2 = ev->tab2;
        SwapTabs(win, tab1, tab2);
    };

    TabsCreateArgs args;
    args.parent = win->hwndFrame;
    args.ctrlID = IDC_TABBAR;
    args.createToolTipsHwnd = true;
    tabsCtrl->Create(args);

    HWND hwndTabBar = tabsCtrl->hwnd;
    SetWindowSubclass(hwndTabBar, TabBarProc, 0, (DWORD_PTR)win);

    Size tabSize = GetTabSize(win->hwndFrame);
    TabPainter* tp = new TabPainter(tabsCtrl, tabSize);
    SetWindowLongPtr(hwndTabBar, GWLP_USERDATA, (LONG_PTR)tp);
    tabsCtrl->SetItemSize(tabSize);
    win->tabsCtrl = tabsCtrl;

    win->tabSelectionHistory = new Vec<WindowTab*>();
}

// verifies that WindowTab state is consistent with MainWindow state
static NO_INLINE void VerifyWindowTab(MainWindow* win, WindowTab* tdata) {
    CrashIf(tdata->ctrl != win->ctrl);
#if 0
    // disabling this check. best I can tell, external apps can change window
    // title and trigger this
    auto winTitle = win::GetTextTemp(win->hwndFrame);
    if (!str::Eq(winTitle.Get(), tdata->frameTitle.Get())) {
        logf(L"VerifyWindowTab: winTitle: '%s', tdata->frameTitle: '%s'\n", winTitle.Get(), tdata->frameTitle.Get());
        ReportIf(!str::Eq(winTitle.Get(), tdata->frameTitle));
    }
#endif
    bool expectedTocVisibility = tdata->showToc; // if not in presentation mode
    if (PM_DISABLED != win->presentation) {
        expectedTocVisibility = false; // PM_BLACK_SCREEN, PM_WHITE_SCREEN
        if (PM_ENABLED == win->presentation) {
            expectedTocVisibility = tdata->showTocPresentation;
        }
    }
    ReportIf(win->tocVisible != expectedTocVisibility);
    ReportIf(tdata->canvasRc != win->canvasRc);
}

// Must be called when the active tab is losing selection.
// This happens when a new document is loaded or when another tab is selected.
void SaveCurrentWindowTab(MainWindow* win) {
    if (!win) {
        return;
    }

    int current = win->tabsCtrl->GetSelectedTabIndex();
    if (-1 == current) {
        return;
    }
    CrashIf(win->currentTab != win->tabs.at(current));

    WindowTab* tab = win->currentTab;
    if (win->tocLoaded) {
        TocTree* tocTree = tab->ctrl->GetToc();
        UpdateTocExpansionState(tab->tocState, win->tocTreeView, tocTree);
    }
    VerifyWindowTab(win, tab);

    // update the selection history
    win->tabSelectionHistory->Remove(tab);
    win->tabSelectionHistory->Append(tab);
}

#include "AppColors.h"

void TabPainterSetColors(TabPainter* p) {
    p->tabBackgroundBg = GetAppColor(AppColor::TabBackgroundBg);
    p->tabBackgroundText = GetAppColor(AppColor::TabBackgroundText);
    p->tabBackgroundCloseX = GetAppColor(AppColor::TabBackgroundCloseX);
    p->tabBackgroundCloseCircle = GetAppColor(AppColor::TabBackgroundCloseCircle);
    p->tabSelectedBg = GetAppColor(AppColor::TabSelectedBg);
    p->tabSelectedText = GetAppColor(AppColor::TabSelectedText);
    p->tabSelectedCloseX = GetAppColor(AppColor::TabSelectedCloseX);
    p->tabSelectedCloseCircle = GetAppColor(AppColor::TabSelectedCloseCircle);
    p->tabHighlightedBg = GetAppColor(AppColor::TabHighlightedBg);
    p->tabHighlightedText = GetAppColor(AppColor::TabHighlightedText);
    p->tabHighlightedCloseX = GetAppColor(AppColor::TabHighlightedCloseX);
    p->tabHighlightedCloseCircle = GetAppColor(AppColor::TabHighlightedCloseCircle);
    p->tabHoveredCloseX = GetAppColor(AppColor::TabHoveredCloseX);
    p->tabHoveredCloseCircle = GetAppColor(AppColor::TabHoveredCloseCircle);
    p->tabClickedCloseX = GetAppColor(AppColor::TabClickedCloseX);
    p->tabClickedCloseCircle = GetAppColor(AppColor::TabClickedCloseCircle);
}

void UpdateCurrentTabBgColor(MainWindow* win) {
    TabPainter* tab = (TabPainter*)GetWindowLongPtr(win->tabsCtrl->hwnd, GWLP_USERDATA);
    // TODO: match either the toolbar (if shown) or background
    tab->currBgCol = kTabDefaultBgCol;
    TabPainterSetColors(tab);
    RepaintNow(win->tabsCtrl->hwnd);
}

// On load of a new document we insert a new tab item in the tab bar.
WindowTab* CreateNewTab(MainWindow* win, const char* filePath) {
    CrashIf(!win);
    if (!win) {
        return nullptr;
    }

    WindowTab* tab = new WindowTab(win, filePath);
    win->tabs.Append(tab);
    tab->canvasRc = win->canvasRc;

    int idx = (int)win->tabs.size() - 1;
    auto tabs = win->tabsCtrl;
    int insertedIdx = tabs->InsertTab(idx, tab->GetTabTitle());
    CrashIf(insertedIdx == -1);
    tabs->SetSelectedTabByIndex(idx);
    UpdateTabWidth(win);
    return tab;
}

// Refresh the tab's title
void TabsOnChangedDoc(MainWindow* win) {
    WindowTab* tab = win->currentTab;
    CrashIf(!tab != !win->tabs.size());
    if (!tab) {
        return;
    }

    CrashIf(win->tabs.Find(tab) != win->tabsCtrl->GetSelectedTabIndex());
    VerifyWindowTab(win, tab);
    SetTabTitle(tab);
}

// Called when we're closing a document
void TabsOnCloseDoc(MainWindow* win) {
    if (win->tabs.size() == 0) {
        return;
    }

    /*
    DisplayModel* dm = win->AsFixed();
    if (dm) {
        EngineBase* engine = dm->GetEngine();
        if (EngineHasUnsavedAnnotations(engine)) {
            // TODO: warn about unsaved annotations
            logf("File has unsaved annotations\n");
        }
    }
    */

    int current = win->tabsCtrl->GetSelectedTabIndex();
    RemoveTab(win, current);

    if (win->tabs.size() > 0) {
        WindowTab* tab = win->tabSelectionHistory->Pop();
        int idx = win->tabs.Find(tab);
        win->tabsCtrl->SetSelectedTabByIndex(idx);
        LoadModelIntoTab(tab);
    }
}

// Called when we're closing an entire window (quitting)
void TabsOnCloseWindow(MainWindow* win) {
    win->tabsCtrl->RemoveAllTabs();
    win->tabSelectionHistory->Reset();
    win->currentTab = nullptr;
    win->ctrl = nullptr;
    DeleteVecMembers(win->tabs);
}

void SetTabsInTitlebar(MainWindow* win, bool inTitlebar) {
    if (inTitlebar == win->tabsInTitlebar) {
        return;
    }
    win->tabsInTitlebar = inTitlebar;
    TabPainter* tab = (TabPainter*)GetWindowLongPtr(win->tabsCtrl->hwnd, GWLP_USERDATA);
    tab->inTitlebar = inTitlebar;
    SetParent(win->tabsCtrl->hwnd, inTitlebar ? win->hwndCaption : win->hwndFrame);
    ShowWindow(win->hwndCaption, inTitlebar ? SW_SHOW : SW_HIDE);
    if (inTitlebar != win->isMenuHidden) {
        ToggleMenuBar(win);
    }
    if (inTitlebar) {
        CaptionUpdateUI(win, win->caption);
        RelayoutCaption(win);
    } else if (dwm::IsCompositionEnabled()) {
        // remove the extended frame
        MARGINS margins{};
        dwm::ExtendFrameIntoClientArea(win->hwndFrame, &margins);
        win->extendedFrameHeight = 0;
    }
    uint flags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
    SetWindowPos(win->hwndFrame, nullptr, 0, 0, 0, 0, flags);
}

// Selects the next (or previous) tab.
void TabsOnCtrlTab(MainWindow* win, bool reverse) {
    if (!win) {
        return;
    }
    int count = (int)win->tabs.size();
    if (count < 2) {
        return;
    }
    int idx = win->tabsCtrl->GetSelectedTabIndex() + 1;
    if (reverse) {
        idx -= 2;
    }
    idx += count; // ensure > 0
    idx = idx % count;
    TabsSelect(win, idx);
}
