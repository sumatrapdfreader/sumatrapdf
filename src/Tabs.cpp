/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"
#include "utils/Log.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TreeModel.h"
#include "wingui/TreeCtrl.h"

#include "EngineBase.h"
#include "EngineManager.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "AppColors.h"
#include "GlobalPrefs.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "resource.h"
#include "Caption.h"
#include "Menu.h"
#include "TableOfContents.h"
#include "Tabs.h"

using namespace Gdiplus;

#define DEFAULT_CURRENT_BG_COL (COLORREF) - 1

#define T_CLOSING (TCN_LAST + 1)
#define T_CLOSE (TCN_LAST + 2)
#define T_DRAG (TCN_LAST + 3)

#define TABBAR_HEIGHT 24
#define MIN_TAB_WIDTH 100

int GetTabbarHeight(HWND hwnd, float factor) {
    int dy = DpiScale(hwnd, TABBAR_HEIGHT);
    return (int)(dy * factor);
}

static inline SizeI GetTabSize(HWND hwnd) {
    int dx = DpiScale(hwnd, std::max(gGlobalPrefs->tabWidth, MIN_TAB_WIDTH));
    int dy = DpiScale(hwnd, TABBAR_HEIGHT);
    return SizeI(dx, dy);
}

class TabPainter {
    WStrVec text;
    PathData* data = nullptr;
    int width = -1;
    int height = -1;
    HWND hwnd;

  public:
    int selectedTabIdx = -1;
    int highlighted = -1;
    int xClicked = -1;
    int xHighlighted = -1;
    int nextTab = -1;
    bool isDragging = false;
    bool inTitlebar = false;
    LPARAM mouseCoordinates = 0;
    COLORREF currBgCol = DEFAULT_CURRENT_BG_COL;

    TabPainter(HWND wnd, SizeI tabSize) {
        hwnd = wnd;
        Reshape(tabSize.dx, tabSize.dy);
    }

    ~TabPainter() {
        delete data;
        DeleteAll();
    }

    // Calculates tab's elements, based on its width and height.
    // Generates a GraphicsPath, which is used for painting the tab, etc.
    bool Reshape(int dx, int dy) {
        dx--;
        if (width == dx && height == dy)
            return false;
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
    int IndexFromPoint(int x, int y, bool* inXbutton = nullptr) {
        Gdiplus::Point point(x, y);
        Graphics gfx(hwnd);
        GraphicsPath shapes(data->Points, data->Types, data->Count);
        GraphicsPath shape;
        GraphicsPathIterator iterator(&shapes);
        iterator.NextMarker(&shape);

        ClientRect rClient(hwnd);
        REAL yPosTab = inTitlebar ? 0.0f : REAL(rClient.dy - height - 1);
        gfx.TranslateTransform(1.0f, yPosTab);
        for (int i = 0; i < Count(); i++) {
            Gdiplus::Point pt(point);
            gfx.TransformPoints(CoordinateSpaceWorld, CoordinateSpaceDevice, &pt, 1);
            if (shape.IsVisible(pt, &gfx)) {
                iterator.NextMarker(&shape);
                if (inXbutton)
                    *inXbutton = shape.IsVisible(pt, &gfx) ? true : false;
                return i;
            }
            gfx.TranslateTransform(REAL(width + 1), 0.0f);
        }
        if (inXbutton)
            *inXbutton = false;
        return -1;
    }

    // Invalidates the tab's region in the client area.
    void Invalidate(int index) {
        if (index < 0)
            return;

        Graphics gfx(hwnd);
        GraphicsPath shapes(data->Points, data->Types, data->Count);
        GraphicsPath shape;
        GraphicsPathIterator iterator(&shapes);
        iterator.NextMarker(&shape);
        Region region(&shape);

        ClientRect rClient(hwnd);
        REAL yPosTab = inTitlebar ? 0.0f : REAL(rClient.dy - height - 1);
        gfx.TranslateTransform(REAL((width + 1) * index) + 1.0f, yPosTab);
        HRGN hRgn = region.GetHRGN(&gfx);
        InvalidateRgn(hwnd, hRgn, FALSE);
        DeleteObject(hRgn);
    }

    // Paints the tabs that intersect the window's update rectangle.
    void Paint(HDC hdc, RECT& rc) {
        IntersectClipRect(hdc, rc.left, rc.top, rc.right, rc.bottom);

// paint the background
#if 0
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
        gfx.SetCompositingMode(CompositingModeSourceCopy);
        gfx.SetCompositingQuality(CompositingQualityHighQuality);
        gfx.SetSmoothingMode(SmoothingModeHighQuality);
        gfx.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        gfx.SetPageUnit(UnitPixel);
        GraphicsPath shapes(data->Points, data->Types, data->Count);
        GraphicsPath shape;
        GraphicsPathIterator iterator(&shapes);

        SolidBrush br(Color(0, 0, 0));
        Pen pen(&br, 2.0f);

        Font f(hdc, GetDefaultGuiFont());
        // TODO: adjust these constant values for DPI?
        RectF layout((REAL)DpiScale(hwnd, 3), 1.0f, REAL(width - DpiScale(hwnd, 20)), (REAL)height);
        StringFormat sf(StringFormat::GenericDefault());
        sf.SetFormatFlags(StringFormatFlagsNoWrap);
        sf.SetLineAlignment(StringAlignmentCenter);
        sf.SetTrimming(StringTrimmingEllipsisCharacter);

        REAL yPosTab = inTitlebar ? 0.0f : REAL(ClientRect(hwnd).dy - height - 1);
        for (int i = 0; i < Count(); i++) {
            gfx.ResetTransform();
            gfx.TranslateTransform(1.f + (REAL)(width + 1) * i - (REAL)rc.left, yPosTab - (REAL)rc.top);

            if (!gfx.IsVisible(0, 0, width + 1, height + 1))
                continue;

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
            gfx.SetCompositingMode(CompositingModeSourceCopy);
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
            gfx.SetCompositingMode(CompositingModeSourceOver);
            br.SetColor(GdiRgbFromCOLORREF(textCol));
            gfx.DrawString(text.at(i), -1, &f, layout, &sf, &br);

            // paint "x"'s circle
            iterator.NextMarker(&shape);
            bool closeCircleEnabled = true;
            if ((xClicked == i || xHighlighted == i) && closeCircleEnabled) {
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

    int Count() {
        return (int)text.size();
    }

    void Insert(int index, const WCHAR* t) {
        text.InsertAt(index, str::Dup(t));
    }

    bool Set(int index, const WCHAR* t) {
        if (index < Count()) {
            str::ReplacePtr(&text.at(index), t);
            return true;
        }
        return false;
    }

    bool Delete(int index) {
        if (index < Count()) {
            free(text.PopAt(index));
            return true;
        }
        return false;
    }

    void DeleteAll() {
        text.Reset();
    }
};

static void SetTabTitle(WindowInfo* win, TabInfo* tab) {
    TCITEM tcs;
    tcs.mask = TCIF_TEXT;
    tcs.pszText = (WCHAR*)tab->GetTabTitle();
    TabCtrl_SetItem(win->hwndTabBar, win->tabs.Find(tab), &tcs);
}

static void SwapTabs(WindowInfo* win, int tab1, int tab2) {
    if (tab1 == tab2 || tab1 < 0 || tab2 < 0)
        return;

    std::swap(win->tabs.at(tab1), win->tabs.at(tab2));
    SetTabTitle(win, win->tabs.at(tab1));
    SetTabTitle(win, win->tabs.at(tab2));

    int current = TabCtrl_GetCurSel(win->hwndTabBar);
    if (tab1 == current)
        TabCtrl_SetCurSel(win->hwndTabBar, tab2);
    else if (tab2 == current)
        TabCtrl_SetCurSel(win->hwndTabBar, tab1);
}

static void TabNotification(WindowInfo* win, UINT code, int idx1, int idx2) {
    if (!WindowInfoStillValid(win)) {
        return;
    }
    NMHDR nmhdr = {nullptr, 0, code};
    if (TabsOnNotify(win, (LPARAM)&nmhdr, idx1, idx2)) {
        return;
    }
    TabPainter* tab = (TabPainter*)GetWindowLongPtr(win->hwndTabBar, GWLP_USERDATA);
    if (T_CLOSING == code) {
        // if we have permission to close the tab
        tab->Invalidate(tab->nextTab);
        tab->xClicked = tab->nextTab;
        return;
    }
    if (TCN_SELCHANGING == code) {
        // if we have permission to select the tab
        tab->Invalidate(tab->selectedTabIdx);
        tab->Invalidate(tab->nextTab);
        tab->selectedTabIdx = tab->nextTab;
        // send notification that the tab is selected
        nmhdr.code = TCN_SELCHANGE;
        TabsOnNotify(win, (LPARAM)&nmhdr);
    }
}

static LRESULT CALLBACK TabBarParentProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass,
                                         DWORD_PTR dwRefData) {
    UNUSED(uIdSubclass);

    if (msg == WM_NOTIFY && wp == IDC_TABBAR) {
        WindowInfo* win = (WindowInfo*)dwRefData;
        if (win)
            return TabsOnNotify(win, lp);
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK TabBarProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass,
                                   DWORD_PTR dwRefData) {
    PAINTSTRUCT ps;
    HDC hdc;
    int index;
    LPTCITEM tcs;

    UNUSED(uIdSubclass);
    UNUSED(dwRefData);

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
            tcs = (LPTCITEM)lp;
            CrashIf(!(TCIF_TEXT & tcs->mask));
            tab->Insert(index, tcs->pszText);
            if (index <= tab->selectedTabIdx)
                tab->selectedTabIdx++;
            tab->xClicked = -1;
            InvalidateRgn(hwnd, nullptr, FALSE);
            UpdateWindow(hwnd);
            break;

        case TCM_SETITEM:
            index = (int)wp;
            tcs = (LPTCITEM)lp;
            if (TCIF_TEXT & tcs->mask) {
                if (tab->Set(index, tcs->pszText)) {
                    tab->Invalidate(index);
                }
            }
            break;

        case TCM_DELETEITEM:
            index = (int)wp;
            if (tab->Delete(index)) {
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
            }
            break;

        case TCM_DELETEALLITEMS:
            tab->DeleteAll();
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
            PostMessage(hwnd, WM_MOUSEMOVE, 0xFF, 0);
            return 0;

        case WM_MOUSEMOVE: {
            tab->mouseCoordinates = lp;

            if (0xff != wp) {
                TrackMouseLeave(hwnd);
            }

            bool inX = false;
            int hl = wp == 0xFF ? -1 : tab->IndexFromPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), &inX);
            if (tab->isDragging && hl == -1) {
                // preserve the highlighted tab if it's dragged outside the tabs' area
                hl = tab->highlighted;
            }
            if (tab->highlighted != hl) {
                if (tab->isDragging) {
                    // send notification if the highlighted tab is dragged over another
                    WindowInfo* win = FindWindowInfoByHwnd(hwnd);
                    int tabNo = tab->highlighted;
                    uitask::Post([=] { TabNotification(win, T_DRAG, tabNo, hl); });
                }

                tab->Invalidate(hl);
                tab->Invalidate(tab->highlighted);
                tab->highlighted = hl;
            }
            int xHl = inX && !tab->isDragging ? hl : -1;
            if (tab->xHighlighted != xHl) {
                tab->Invalidate(xHl);
                tab->Invalidate(tab->xHighlighted);
                tab->xHighlighted = xHl;
            }
            if (!inX)
                tab->xClicked = -1;
        }
            return 0;

        case WM_LBUTTONDOWN:
            bool inX;
            tab->nextTab = tab->IndexFromPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), &inX);
            if (inX) {
                // send request to close the tab
                WindowInfo* win = FindWindowInfoByHwnd(hwnd);
                int next = tab->nextTab;
                uitask::Post([=] { TabNotification(win, T_CLOSING, next, -1); });
            } else if (tab->nextTab != -1) {
                if (tab->nextTab != tab->selectedTabIdx) {
                    // send request to select tab
                    WindowInfo* win = FindWindowInfoByHwnd(hwnd);
                    uitask::Post([=] { TabNotification(win, TCN_SELCHANGING, -1, -1); });
                }
                tab->isDragging = true;
                SetCapture(hwnd);
            }
            return 0;

        case WM_LBUTTONUP:
            if (tab->xClicked != -1) {
                // send notification that the tab is closed
                WindowInfo* win = FindWindowInfoByHwnd(hwnd);
                int clicked = tab->xClicked;
                uitask::Post([=] { TabNotification(win, T_CLOSE, clicked, -1); });
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
                WindowInfo* win = FindWindowInfoByHwnd(hwnd);
                int next = tab->nextTab;
                uitask::Post([=] { TabNotification(win, T_CLOSING, next, -1); });
            }
            return 0;

        case WM_MBUTTONUP:
            if (tab->xClicked != -1) {
                // send notification that the tab is closed
                WindowInfo* win = FindWindowInfoByHwnd(hwnd);
                int clicked = tab->xClicked;
                uitask::Post([=] { TabNotification(win, T_CLOSE, clicked, -1); });
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

            DoubleBuffer buffer(hwnd, RectI::FromRECT(rc));
            tab->Paint(buffer.GetDC(), rc);
            buffer.Flush(hdc);

            ValidateRect(hwnd, nullptr);
            if (!wp)
                EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE: {
            WindowInfo* win = FindWindowInfoByHwnd(hwnd);
            if (win)
                UpdateTabWidth(win);
        } break;
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

void CreateTabbar(WindowInfo* win) {
    DWORD dwStyle = WS_CHILD | WS_CLIPSIBLINGS /*| WS_VISIBLE*/ | TCS_FOCUSNEVER | TCS_FIXEDWIDTH | TCS_FORCELABELLEFT;
    DWORD dwStyleEx = 0;
    auto h = GetModuleHandleW(nullptr);
    HWND hwndTabBar = CreateWindowExW(dwStyleEx, WC_TABCONTROL, L"", dwStyle, 0, 0, 0, 0, win->hwndFrame,
                                      (HMENU)IDC_TABBAR, h, nullptr);

    SetWindowSubclass(hwndTabBar, TabBarProc, 0, (DWORD_PTR)win);
    SetWindowSubclass(GetParent(hwndTabBar), TabBarParentProc, 0, (DWORD_PTR)win);

    SizeI tabSize = GetTabSize(win->hwndFrame);
    TabPainter* tp = new TabPainter(hwndTabBar, tabSize);
    SetWindowLongPtr(hwndTabBar, GWLP_USERDATA, (LONG_PTR)tp);

    SetWindowFont(hwndTabBar, GetDefaultGuiFont(), FALSE);
    TabCtrl_SetItemSize(hwndTabBar, tabSize.dx, tabSize.dy);

    win->hwndTabBar = hwndTabBar;

    win->tabSelectionHistory = new Vec<TabInfo*>();
}

// verifies that TabInfo state is consistent with WindowInfo state
static NO_INLINE void VerifyTabInfo(WindowInfo* win, TabInfo* tdata) {
    CrashIf(tdata->ctrl != win->ctrl);
    AutoFreeWstr winTitle(win::GetText(win->hwndFrame));
    SubmitCrashIf(!str::Eq(winTitle.Get(), tdata->frameTitle));
    bool expectedTocVisibility = tdata->showToc; // if not in presentation mode
    if (PM_DISABLED != win->presentation) {
        expectedTocVisibility = false; // PM_BLACK_SCREEN, PM_WHITE_SCREEN
        if (PM_ENABLED == win->presentation) {
            expectedTocVisibility = tdata->showTocPresentation;
        }
    }
    SubmitCrashIf(win->tocVisible != expectedTocVisibility);
    SubmitCrashIf(tdata->canvasRc != win->canvasRc);
}

// Must be called when the active tab is losing selection.
// This happens when a new document is loaded or when another tab is selected.
void SaveCurrentTabInfo(WindowInfo* win) {
    if (!win)
        return;

    int current = TabCtrl_GetCurSel(win->hwndTabBar);
    if (-1 == current) {
        return;
    }
    CrashIf(win->currentTab != win->tabs.at(current));

    TabInfo* tab = win->currentTab;
    CrashIf(!tab);
    if (win->tocLoaded) {
        TocTree* tocTree = tab->ctrl->GetToc();
        UpdateTocExpansionState(tab->tocState, win->tocTreeCtrl, tocTree);
    }
    VerifyTabInfo(win, tab);

    // update the selection history
    win->tabSelectionHistory->Remove(tab);
    win->tabSelectionHistory->Push(tab);
}

void UpdateCurrentTabBgColor(WindowInfo* win) {
    TabPainter* tab = (TabPainter*)GetWindowLongPtr(win->hwndTabBar, GWLP_USERDATA);
    if (win->AsEbook()) {
        COLORREF txtCol;
        GetEbookUiColors(txtCol, tab->currBgCol);
    } else {
        // TODO: match either the toolbar (if shown) or background
        tab->currBgCol = DEFAULT_CURRENT_BG_COL;
    }
    RepaintNow(win->hwndTabBar);
}

// On load of a new document we insert a new tab item in the tab bar.
TabInfo* CreateNewTab(WindowInfo* win, const WCHAR* filePath) {
    CrashIf(!win);
    if (!win) {
        return nullptr;
    }

    TabInfo* tab = new TabInfo(filePath);
    win->tabs.Append(tab);
    tab->canvasRc = win->canvasRc;

    TCITEMW tcs = {0};
    tcs.mask = TCIF_TEXT;
    tcs.pszText = (WCHAR*)tab->GetTabTitle();

    int idx = (int)win->tabs.size() - 1;
    auto insertedIdx = TabCtrl_InsertItem(win->hwndTabBar, idx, &tcs);
    CrashIf(insertedIdx == -1);
    TabCtrl_SetCurSel(win->hwndTabBar, idx);
    UpdateTabWidth(win);
    return tab;
}

// Refresh the tab's title
void TabsOnChangedDoc(WindowInfo* win) {
    TabInfo* tab = win->currentTab;
    CrashIf(!tab != !win->tabs.size());
    if (!tab)
        return;

    CrashIf(win->tabs.Find(tab) != TabCtrl_GetCurSel(win->hwndTabBar));
    VerifyTabInfo(win, tab);
    SetTabTitle(win, tab);
}

static void RemoveTab(WindowInfo* win, int idx) {
    TabInfo* tab = win->tabs.at(idx);
    UpdateTabFileDisplayStateForWin(win, tab);
    win->tabSelectionHistory->Remove(tab);
    win->tabs.Remove(tab);
    if (tab == win->currentTab) {
        win->ctrl = nullptr;
        win->currentTab = nullptr;
    }
    delete tab;
    TabCtrl_DeleteItem(win->hwndTabBar, idx);
    UpdateTabWidth(win);
}

// Called when we're closing a document
void TabsOnCloseDoc(WindowInfo* win) {
    if (win->tabs.size() == 0)
        return;

    /* if (win->AsFixed() && win->AsFixed()->userAnnots && win->AsFixed()->userAnnotsModified) {
        // TODO: warn about unsaved changes
    } */

    int current = TabCtrl_GetCurSel(win->hwndTabBar);
    RemoveTab(win, current);

    if (win->tabs.size() > 0) {
        TabInfo* tab = win->tabSelectionHistory->Pop();
        TabCtrl_SetCurSel(win->hwndTabBar, win->tabs.Find(tab));
        LoadModelIntoTab(win, tab);
    }
}

// Called when we're closing an entire window (quitting)
void TabsOnCloseWindow(WindowInfo* win) {
    TabCtrl_DeleteAllItems(win->hwndTabBar);
    win->tabSelectionHistory->Reset();
    win->currentTab = nullptr;
    win->ctrl = nullptr;
    DeleteVecMembers(win->tabs);
}

// On tab selection, we save the data for the tab which is losing selection and
// load the data of the selected tab into the WindowInfo.
LRESULT TabsOnNotify(WindowInfo* win, LPARAM lparam, int tab1, int tab2) {
    LPNMHDR data = (LPNMHDR)lparam;
    int current;

    switch (data->code) {
        case TCN_SELCHANGING:
            // TODO: Should we allow the switch of the tab if we are in process of printing?
            SaveCurrentTabInfo(win);
            return FALSE;

        case TCN_SELCHANGE:
            current = TabCtrl_GetCurSel(win->hwndTabBar);
            LoadModelIntoTab(win, win->tabs.at(current));
            break;

        case T_CLOSING:
            // allow the closure
            return FALSE;

        case T_CLOSE:
            current = TabCtrl_GetCurSel(win->hwndTabBar);
            if (tab1 == current) {
                CloseTab(win);
            } else {
                RemoveTab(win, tab1);
            }
            break;

        case T_DRAG:
            SwapTabs(win, tab1, tab2);
            break;
    }
    return TRUE;
}

static void ShowTabBar(WindowInfo* win, bool show) {
    if (show == win->tabsVisible) {
        return;
    }
    win->tabsVisible = show;
    win::SetVisibility(win->hwndTabBar, show);
    RelayoutWindow(win);
}

void UpdateTabWidth(WindowInfo* win) {
    int count = (int)win->tabs.size();
    bool showSingleTab = gGlobalPrefs->useTabs || win->tabsInTitlebar;
    bool showTabs = (count > 1) || (showSingleTab && (count > 0));
    if (!showTabs) {
        ShowTabBar(win, false);
        return;
    }
    ShowTabBar(win, true);
    ClientRect rect(win->hwndTabBar);
    SizeI tabSize = GetTabSize(win->hwndFrame);
    auto maxDx = (rect.dx - 3) / count;
    tabSize.dx = std::min(tabSize.dx, maxDx);
    TabCtrl_SetItemSize(win->hwndTabBar, tabSize.dx, tabSize.dy);
}

void SetTabsInTitlebar(WindowInfo* win, bool set) {
    if (set == win->tabsInTitlebar) {
        return;
    }
    win->tabsInTitlebar = set;
    TabPainter* tab = (TabPainter*)GetWindowLongPtr(win->hwndTabBar, GWLP_USERDATA);
    tab->inTitlebar = set;
    SetParent(win->hwndTabBar, set ? win->hwndCaption : win->hwndFrame);
    ShowWindow(win->hwndCaption, set ? SW_SHOW : SW_HIDE);
    if (set != win->isMenuHidden) {
        ShowHideMenuBar(win);
    }
    if (set) {
        CaptionUpdateUI(win, win->caption);
        RelayoutCaption(win);
    } else if (dwm::IsCompositionEnabled()) {
        // remove the extended frame
        MARGINS margins = {0};
        dwm::ExtendFrameIntoClientArea(win->hwndFrame, &margins);
        win->extendedFrameHeight = 0;
    }
    UINT flags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
    SetWindowPos(win->hwndFrame, nullptr, 0, 0, 0, 0, flags);
}

// Selects the given tab (0-based index).
void TabsSelect(WindowInfo* win, int tabIndex) {
    int count = (int)win->tabs.size();
    if (count < 2 || tabIndex < 0 || tabIndex >= count) {
        return;
    }
    NMHDR ntd = {nullptr, 0, TCN_SELCHANGING};
    if (TabsOnNotify(win, (LPARAM)&ntd)) {
        return;
    }
    win->currentTab = win->tabs.at(tabIndex);
    AutoFree path = strconv::WstrToUtf8(win->currentTab->filePath);
    logf("TabsSelect: tabIndex: %d, new win->currentTab: 0x%p, path: '%s'\n", tabIndex, win->currentTab, path.get());
    int prevIndex = TabCtrl_SetCurSel(win->hwndTabBar, tabIndex);
    if (prevIndex != -1) {
        ntd.code = TCN_SELCHANGE;
        TabsOnNotify(win, (LPARAM)&ntd);
    }
}

// Selects the next (or previous) tab.
void TabsOnCtrlTab(WindowInfo* win, bool reverse) {
    int count = (int)win->tabs.size();
    if (count < 2) {
        return;
    }

    int next = (TabCtrl_GetCurSel(win->hwndTabBar) + (reverse ? -1 : 1) + count) % count;
    TabsSelect(win, next);
}
