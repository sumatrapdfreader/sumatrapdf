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
#include "AppColors.h"
#include "DisplayModel.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "TabInfo.h"
#include "resource.h"
#include "Commands.h"
#include "Caption.h"
#include "Menu.h"
#include "TableOfContents.h"
#include "Tabs.h"

#include "utils/Log.h"

using Gdiplus::ARGB;
using Gdiplus::Bitmap;
using Gdiplus::Brush;
using Gdiplus::Color;
using Gdiplus::CombineModeReplace;
using Gdiplus::CompositingQualityHighQuality;
using Gdiplus::Font;
using Gdiplus::FontFamily;
using Gdiplus::FontStyle;
using Gdiplus::FontStyleBold;
using Gdiplus::FontStyleItalic;
using Gdiplus::FontStyleRegular;
using Gdiplus::FontStyleStrikeout;
using Gdiplus::FontStyleUnderline;
using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::Image;
using Gdiplus::ImageAttributes;
using Gdiplus::InterpolationModeHighQualityBicubic;
using Gdiplus::LinearGradientBrush;
using Gdiplus::LinearGradientMode;
using Gdiplus::LinearGradientModeVertical;
using Gdiplus::Matrix;
using Gdiplus::MatrixOrderAppend;
using Gdiplus::Ok;
using Gdiplus::OutOfMemory;
using Gdiplus::PathData;
using Gdiplus::Pen;
using Gdiplus::PenAlignmentInset;
using Gdiplus::PropertyItem;
using Gdiplus::Region;
using Gdiplus::SmoothingModeAntiAlias;
using Gdiplus::SolidBrush;
using Gdiplus::Status;
using Gdiplus::StringAlignmentCenter;
using Gdiplus::StringFormat;
using Gdiplus::StringFormatFlagsDirectionRightToLeft;
using Gdiplus::TextRenderingHintClearTypeGridFit;
using Gdiplus::UnitPixel;
using Gdiplus::Win32Error;

#define kTabDefaultBgCol (COLORREF) - 1

#define kTabClosing (TCN_LAST + 1)
#define kTabClose (TCN_LAST + 2)
#define kTabDrag (TCN_LAST + 3)

#define kTabBarDy 24
#define kTabMinDx 100

int GetTabbarHeight(HWND hwnd, float factor) {
    int dy = DpiScale(hwnd, kTabBarDy);
    return (int)(dy * factor);
}

static inline Size GetTabSize(HWND hwnd) {
    int dx = DpiScale(hwnd, std::max(gGlobalPrefs->tabWidth, kTabMinDx));
    int dy = DpiScale(hwnd, kTabBarDy);
    return Size(dx, dy);
}

struct TabPainter {
    TabsCtrl* tabsCtrl = nullptr;
    PathData* data = nullptr;
    int width = -1;
    int height = -1;
    HWND hwnd = nullptr;

    int selectedTabIdx = -1;
    int highlighted = -1;
    int xClicked = -1;
    int xHighlighted = -1;
    int nextTab = -1;
    bool isDragging = false;
    bool inTitlebar = false;
    LPARAM mouseCoordinates = 0;
    COLORREF currBgCol{kTabDefaultBgCol};

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
        COLORREF bgCol = GetAppColor(AppColor::TabBackgroundBg);
        COLORREF textCol = GetAppColor(AppColor::TabBackgroundText);
        COLORREF xColor = GetAppColor(AppColor::TabBackgroundCloseX);
        COLORREF circleColor = GetAppColor(AppColor::TabBackgroundCloseCircle);

        if (selectedTabIdx == i) {
            bgCol = GetAppColor(AppColor::TabSelectedBg);
            textCol = GetAppColor(AppColor::TabSelectedText);
            xColor = GetAppColor(AppColor::TabSelectedCloseX);
            circleColor = GetAppColor(AppColor::TabSelectedCloseCircle);
        } else if (highlighted == i) {
            bgCol = GetAppColor(AppColor::TabHighlightedBg);
            textCol = GetAppColor(AppColor::TabHighlightedText);
            xColor = GetAppColor(AppColor::TabHighlightedCloseX);
            circleColor = GetAppColor(AppColor::TabHighlightedCloseCircle);
        }
        if (xHighlighted == i) {
            xColor = GetAppColor(AppColor::TabHoveredCloseX);
            circleColor = GetAppColor(AppColor::TabHoveredCloseCircle);
        }
        if (xClicked == i) {
            xColor = GetAppColor(AppColor::TabClickedCloseX);
            circleColor = GetAppColor(AppColor::TabClickedCloseCircle);
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

static void SetTabTitle(TabInfo* tab) {
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

static void TabNotification(MainWindow* win, UINT code, int idx1, int idx2) {
    if (!MainWindowStillValid(win)) {
        return;
    }
    NMHDR nmhdr = {nullptr, 0, code};
    if (TabsOnNotify(win, (LPARAM)&nmhdr, idx1, idx2)) {
        return;
    }
    TabPainter* tab = (TabPainter*)GetWindowLongPtr(win->tabsCtrl->hwnd, GWLP_USERDATA);
    if ((UINT)kTabClosing == code) {
        // if we have permission to close the tab
        tab->Invalidate(tab->nextTab);
        tab->xClicked = tab->nextTab;
        return;
    }
    if ((UINT)TCN_SELCHANGING == code) {
        // if we have permission to select the tab
        tab->Invalidate(tab->selectedTabIdx);
        tab->Invalidate(tab->nextTab);
        tab->selectedTabIdx = tab->nextTab;
        // send notification that the tab is selected
        nmhdr.code = (UINT)TCN_SELCHANGE;
        TabsOnNotify(win, (LPARAM)&nmhdr);
    }
}

static LRESULT CALLBACK TabBarParentProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, __unused UINT_PTR uIdSubclass,
                                         DWORD_PTR dwRefData) {
    if (msg == WM_NOTIFY && wp == IDC_TABBAR) {
        MainWindow* win = (MainWindow*)dwRefData;
        if (win) {
            return TabsOnNotify(win, lp);
        }
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK TabBarProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, __unused UINT_PTR uIdSubclass,
                                   __unused DWORD_PTR dwRefData) {
    PAINTSTRUCT ps;
    HDC hdc;
    int index;
    LPTCITEM tcs;

    TabPainter* tab = (TabPainter*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    if (WM_NCDESTROY == msg) {
        RemoveWindowSubclass(GetParent(hwnd), TabBarParentProc, 0);
        RemoveWindowSubclass(hwnd, TabBarProc, 0);
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

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
                    uitask::Post([=] { TabNotification(win, kTabDrag, tabNo, hl); });
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
                // send request to close the tab
                MainWindow* win = FindMainWindowByHwnd(hwnd);
                int next = tab->nextTab;
                uitask::Post([=] { TabNotification(win, (UINT)kTabClosing, next, -1); });
            } else if (tab->nextTab != -1) {
                if (tab->nextTab != tab->selectedTabIdx) {
                    // send request to select tab
                    MainWindow* win = FindMainWindowByHwnd(hwnd);
                    uitask::Post([=] { TabNotification(win, (UINT)TCN_SELCHANGING, -1, -1); });
                }
                tab->isDragging = true;
                SetCapture(hwnd);
            }
            return 0;

        case WM_LBUTTONUP:
            if (tab->xClicked != -1) {
                // send notification that the tab is closed
                MainWindow* win = FindMainWindowByHwnd(hwnd);
                int clicked = tab->xClicked;
                uitask::Post([=] { TabNotification(win, (UINT)kTabClose, clicked, -1); });
                tab->Invalidate(clicked);
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
                // send request to close the tab
                MainWindow* win = FindMainWindowByHwnd(hwnd);
                int next = tab->nextTab;
                uitask::Post([=] { TabNotification(win, (UINT)kTabClosing, next, -1); });
            }
            return 0;

        case WM_MBUTTONUP:
            if (tab->xClicked != -1) {
                // send notification that the tab is closed
                MainWindow* win = FindMainWindowByHwnd(hwnd);
                int clicked = tab->xClicked;
                uitask::Post([=] { TabNotification(win, (UINT)kTabClose, clicked, -1); });
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

        case WM_SIZE: {
            MainWindow* win = FindMainWindowByHwnd(hwnd);
            if (win) {
                UpdateTabWidth(win);
            }
        } break;
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

void CreateTabbar(MainWindow* win) {
    TabsCtrl* tabsCtrl = new TabsCtrl();

    TabsCreateArgs args;
    args.parent = win->hwndFrame;
    args.ctrlID = IDC_TABBAR;
    args.createToolTipsHwnd = true;
    tabsCtrl->Create(args);

    HWND hwndTabBar = tabsCtrl->hwnd;
    SetWindowSubclass(hwndTabBar, TabBarProc, 0, (DWORD_PTR)win);
    SetWindowSubclass(GetParent(hwndTabBar), TabBarParentProc, 0, (DWORD_PTR)win);

    Size tabSize = GetTabSize(win->hwndFrame);
    TabPainter* tp = new TabPainter(tabsCtrl, tabSize);
    SetWindowLongPtr(hwndTabBar, GWLP_USERDATA, (LONG_PTR)tp);
    tabsCtrl->SetItemSize(tabSize);
    win->tabsCtrl = tabsCtrl;

    win->tabSelectionHistory = new Vec<TabInfo*>();
}

// verifies that TabInfo state is consistent with MainWindow state
static NO_INLINE void VerifyTabInfo(MainWindow* win, TabInfo* tdata) {
    CrashIf(tdata->ctrl != win->ctrl);
#if 0
    // disabling this check. best I can tell, external apps can change window
    // title and trigger this
    auto winTitle = win::GetTextTemp(win->hwndFrame);
    if (!str::Eq(winTitle.Get(), tdata->frameTitle.Get())) {
        logf(L"VerifyTabInfo: winTitle: '%s', tdata->frameTitle: '%s'\n", winTitle.Get(), tdata->frameTitle.Get());
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
void SaveCurrentTabInfo(MainWindow* win) {
    if (!win) {
        return;
    }

    int current = win->tabsCtrl->GetSelectedTabIndex();
    if (-1 == current) {
        return;
    }
    CrashIf(win->currentTab != win->tabs.at(current));

    TabInfo* tab = win->currentTab;
    if (win->tocLoaded) {
        TocTree* tocTree = tab->ctrl->GetToc();
        UpdateTocExpansionState(tab->tocState, win->tocTreeView, tocTree);
    }
    VerifyTabInfo(win, tab);

    // update the selection history
    win->tabSelectionHistory->Remove(tab);
    win->tabSelectionHistory->Append(tab);
}

void UpdateCurrentTabBgColor(MainWindow* win) {
    TabPainter* tab = (TabPainter*)GetWindowLongPtr(win->tabsCtrl->hwnd, GWLP_USERDATA);
    // TODO: match either the toolbar (if shown) or background
    tab->currBgCol = kTabDefaultBgCol;
    RepaintNow(win->tabsCtrl->hwnd);
}

// On load of a new document we insert a new tab item in the tab bar.
TabInfo* CreateNewTab(MainWindow* win, const char* filePath) {
    CrashIf(!win);
    if (!win) {
        return nullptr;
    }

    TabInfo* tab = new TabInfo(win, filePath);
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
    TabInfo* tab = win->currentTab;
    CrashIf(!tab != !win->tabs.size());
    if (!tab) {
        return;
    }

    CrashIf(win->tabs.Find(tab) != win->tabsCtrl->GetSelectedTabIndex());
    VerifyTabInfo(win, tab);
    SetTabTitle(tab);
}

static void RemoveTab(MainWindow* win, int idx) {
    TabInfo* tab = win->tabs.at(idx);
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
        TabInfo* tab = win->tabSelectionHistory->Pop();
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

// On tab selection, we save the data for the tab which is losing selection and
// load the data of the selected tab into the MainWindow.
LRESULT TabsOnNotify(MainWindow* win, LPARAM lp, int tab1, int tab2) {
    LPNMHDR data = (LPNMHDR)lp;
    int current;

    switch (data->code) {
        case TCN_SELCHANGING:
            // TODO: Should we allow the switch of the tab if we are in process of printing?
            SaveCurrentTabInfo(win);
            return FALSE;

        case TCN_SELCHANGE:
            current = win->tabsCtrl->GetSelectedTabIndex();
            LoadModelIntoTab(win->tabs.at(current));
            break;

        case kTabClosing:
            // allow the closure
            return FALSE;

        case kTabClose:
            current = win->tabsCtrl->GetSelectedTabIndex();
            if (tab1 == current) {
                CloseCurrentTab(win);
            } else {
                RemoveTab(win, tab1);
            }
            break;

        case kTabDrag:
            SwapTabs(win, tab1, tab2);
            break;
        case TTN_GETDISPINFOA:
        case TTN_GETDISPINFOW:
            logf("TabsOnNotify TTN_GETDISPINFO\n");
            break;
    }
    return TRUE;
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

// Selects the given tab (0-based index).
void TabsSelect(MainWindow* win, int tabIndex) {
    int count = (int)win->tabs.size();
    if (count < 2 || tabIndex < 0 || tabIndex >= count) {
        return;
    }
    NMHDR ntd = {nullptr, 0, (UINT)TCN_SELCHANGING};
    if (TabsOnNotify(win, (LPARAM)&ntd)) {
        return;
    }
    win->currentTab = win->tabs.at(tabIndex);
    char* path = win->currentTab->filePath;
    logf("TabsSelect: tabIndex: %d, new win->currentTab: 0x%p, path: '%s'\n", tabIndex, win->currentTab, path);
    int prevIdx = win->tabsCtrl->SetSelectedTabByIndex(tabIndex);
    if (prevIdx != -1) {
        ntd.code = (UINT)TCN_SELCHANGE;
        TabsOnNotify(win, (LPARAM)&ntd);
    }
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
