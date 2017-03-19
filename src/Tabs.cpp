/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// utils
#include "BaseUtil.h"
#include "WinDynCalls.h"
#include "Dpi.h"
#include "FileUtil.h"
#include "GdiPlusUtil.h"
#include "UITask.h"
#include "WinUtil.h"
// rendering engines
#include "BaseEngine.h"
#include "EngineManager.h"
// layout controllers
#include "SettingsStructs.h"
#include "Controller.h"
#include "Theme.h"
#include "GlobalPrefs.h"
// ui
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "resource.h"
#include "Caption.h"
#include "Menu.h"
#include "TableOfContents.h"
#include "Tabs.h"

static void SwapTabs(WindowInfo* win, int tab1, int tab2);

#define DEFAULT_CURRENT_BG_COL (COLORREF) - 1

#define T_CLOSING (TCN_LAST + 1)
#define T_CLOSE (TCN_LAST + 2)
#define T_DRAG (TCN_LAST + 3)

#define MIN_TAB_WIDTH 100

static bool g_FirefoxStyle = false;

int GetTabbarHeight(HWND hwnd, float factor) {
    int dy = DpiScaleY(hwnd, GetCurrentTheme()->tab.height);
    return (int)(dy * factor);
}

static inline SizeI GetTabSize(HWND hwnd) {
    int dx = DpiScaleX(hwnd, std::max(gGlobalPrefs->prereleaseSettings.tabWidth, MIN_TAB_WIDTH));
    int dy = DpiScaleY(hwnd, GetCurrentTheme()->tab.height);
    return SizeI(dx, dy);
}

static inline Color ToColor(COLORREF c) {
    return Color(GetRValueSafe(c), GetGValueSafe(c), GetBValueSafe(c));
}

class TabPainter {
    WStrVec text;
    PathData* data = nullptr;
    int width = -1;
    int height = -1;
    HWND hwnd;

  public:
    int current = -1;
    int highlighted = -1;
    int xClicked = -1;
    int xHighlighted = -1;
    int nextTab = -1;
    bool isMouseInClientArea = false;
    bool isDragging = false;
    bool inTitlebar = false;
    LPARAM mouseCoordinates = 0;
    COLORREF currBgCol = DEFAULT_CURRENT_BG_COL;
    struct {
        COLORREF background, highlight, current, outline, bar, text, x_highlight, x_click, x_line;
    } colors;

    TabPainter(HWND wnd, SizeI tabSize) {
        ZeroMemory(&colors, sizeof(colors));
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
        shape.AddRectangle(Rect(0, 0, width, height));
        shape.CloseFigure();
        shape.SetMarker();

        // define "x"'s circle
        int c = int((float)height * 0.78f + 0.5f); // size of bounding square for the circle
        int maxC = DpiScaleX(hwnd, 17);
        if (height > maxC) {
            c = DpiScaleX(hwnd, 17);
        }
        Point p(width - c - DpiScaleX(hwnd, 3), (height - c) / 2); // circle's position
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
        Point point(x, y);
        Graphics graphics(hwnd);
        GraphicsPath shapes(data->Points, data->Types, data->Count);
        GraphicsPath shape;
        GraphicsPathIterator iterator(&shapes);
        iterator.NextMarker(&shape);

        ClientRect rClient(hwnd);
        REAL yPosTab = inTitlebar ? 0.0f : REAL(rClient.dy - height - 1);
        graphics.TranslateTransform(1.0f, yPosTab);
        for (int i = 0; i < Count(); i++) {
            Point pt(point);
            graphics.TransformPoints(CoordinateSpaceWorld, CoordinateSpaceDevice, &pt, 1);
            if (shape.IsVisible(pt, &graphics)) {
                iterator.NextMarker(&shape);
                if (inXbutton)
                    *inXbutton = shape.IsVisible(pt, &graphics) ? true : false;
                return i;
            }
            graphics.TranslateTransform(REAL(width + 1), 0.0f);
        }
        if (inXbutton)
            *inXbutton = false;
        return -1;
    }

    // Invalidates the tab's region in the client area.
    void Invalidate(int index) {
        if (index < 0)
            return;

        Graphics graphics(hwnd);
        GraphicsPath shapes(data->Points, data->Types, data->Count);
        GraphicsPath shape;
        GraphicsPathIterator iterator(&shapes);
        iterator.NextMarker(&shape);
        Region region(&shape);

        ClientRect rClient(hwnd);
        REAL yPosTab = inTitlebar ? 0.0f : REAL(rClient.dy - height - 1);
        graphics.TranslateTransform(REAL((width + 1) * index) + 1.0f, yPosTab);
        HRGN hRgn = region.GetHRGN(&graphics);
        InvalidateRgn(hwnd, hRgn, FALSE);
        DeleteObject(hRgn);
    }

    // Paints the tabs that intersect the window's update rectangle.
    void Paint(HDC hdc, RECT& rc) {
        IntersectClipRect(hdc, rc.left, rc.top, rc.right, rc.bottom);

        // paint the background
        bool isTranslucentMode = inTitlebar && dwm::IsCompositionEnabled();
        if (isTranslucentMode) {
            PaintParentBackground(hwnd, hdc);
        } else {
            HBRUSH brush = CreateSolidBrush(colors.bar);
            FillRect(hdc, &rc, brush);
            DeleteObject(brush);
        }

        // TODO: GDI+ doesn't seem to cope well with SetWorldTransform
        XFORM ctm = {1.0, 0, 0, 1.0, 0, 0};
        SetWorldTransform(hdc, &ctm);

        Graphics graphics(hdc);
        graphics.SetCompositingMode(CompositingModeSourceCopy);
        graphics.SetCompositingQuality(CompositingQualityHighQuality);
        graphics.SetSmoothingMode(SmoothingModeHighQuality);
        graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        graphics.SetPageUnit(UnitPixel);
        GraphicsPath shapes(data->Points, data->Types, data->Count);
        GraphicsPath shape;
        GraphicsPathIterator iterator(&shapes);

        SolidBrush br(Color(0, 0, 0));
        Pen pen(&br, GetCurrentTheme()->tab.closePenWidth);

        Font f(hdc, GetDefaultGuiFont());
        // TODO: adjust these constant values for DPI?
        RectF layout((REAL)DpiScaleX(hwnd, 3), 1.0f, REAL(width - DpiScaleX(hwnd, 20)), (REAL)height);
        StringFormat sf(StringFormat::GenericDefault());
        sf.SetFormatFlags(StringFormatFlagsNoWrap);
        sf.SetLineAlignment(StringAlignmentCenter);
        sf.SetTrimming(StringTrimmingEllipsisCharacter);

        REAL yPosTab = inTitlebar ? 0.0f : REAL(ClientRect(hwnd).dy - height - 1);
        for (int i = 0; i < Count(); i++) {
            graphics.ResetTransform();
            graphics.TranslateTransform(1.f + (REAL)(width + 1) * i - (REAL)rc.left, yPosTab - (REAL)rc.top);

            if (!graphics.IsVisible(0, 0, width + 1, height + 1))
                continue;

            // in firefox style we only paint current and highlighed tabs
            // all other tabs only show
            bool onlyText = g_FirefoxStyle && !((current == i) || (highlighted == i));
            if (onlyText) {
#if 0
                // we need to first paint the background with the same color as caption,
                // otherwise the text looks funny (because is transparent?)
                // TODO: what is the damn bg color of caption? bar is too light, outline is too dark
                Color bgColTmp;
                bgColTmp.SetFromCOLORREF(color.bar);
                {
                    SolidBrush bgBr(bgColTmp);
                    graphics.FillRectangle(&bgBr, layout);
                }
                bgColTmp.SetFromCOLORREF(color.outline);
                {
                    SolidBrush bgBr(bgColTmp);
                    graphics.FillRectangle(&bgBr, layout);
                }
#endif
                // TODO: this is a hack. If I use no background and cleartype, the
                // text looks funny (is bold).
                // CompositingModeSourceCopy doesn't work with clear type
                // another option is to draw background before drawing text, but
                // I can't figure out what is the actual color of caption
                graphics.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
                graphics.SetCompositingMode(CompositingModeSourceCopy);
                // graphics.SetCompositingMode(CompositingModeSourceOver);
                br.SetColor(ToColor(colors.text));
                graphics.DrawString(text.At(i), -1, &f, layout, &sf, &br);
                graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
                continue;
            }

            // Get the correct colors based on the state and the current theme
            COLORREF bgCol = GetCurrentTheme()->tab.background.backgroundColor;
            COLORREF textCol = GetCurrentTheme()->tab.background.textColor;
            COLORREF xColor = GetCurrentTheme()->tab.background.close.xColor;
            COLORREF circleColor = GetCurrentTheme()->tab.background.close.circleColor;
            if (current == i) {
                bgCol = GetCurrentTheme()->tab.current.backgroundColor;
                textCol = GetCurrentTheme()->tab.current.textColor;
                xColor = GetCurrentTheme()->tab.current.close.xColor;
                circleColor = GetCurrentTheme()->tab.current.close.circleColor;
            } else if (highlighted == i) {
                bgCol = GetCurrentTheme()->tab.highlighted.backgroundColor;
                textCol = GetCurrentTheme()->tab.highlighted.textColor;
                xColor = GetCurrentTheme()->tab.highlighted.close.xColor;
                circleColor = GetCurrentTheme()->tab.highlighted.close.circleColor;
            }
            if (xHighlighted == i) {
                xColor = GetCurrentTheme()->tab.hoveredClose.xColor;
                circleColor = GetCurrentTheme()->tab.hoveredClose.circleColor;
            }
            if (xClicked == i) {
                xColor = GetCurrentTheme()->tab.clickedClose.xColor;
                circleColor = GetCurrentTheme()->tab.clickedClose.circleColor;
            }

            // paint tab's body
            graphics.SetCompositingMode(CompositingModeSourceCopy);
            iterator.NextMarker(&shape);
            br.SetColor(ToColor(bgCol));
            Point points[4];
            shape.GetPathPoints(points, 4);
            Rect body(points[0].X, points[0].Y, points[2].X - points[0].X, points[2].Y - points[0].Y);
            body.Inflate(0, 0);
            graphics.SetClip(body);
            body.Inflate(5, 5);
            graphics.FillRectangle(&br, body);
            graphics.ResetClip();

            // draw tab's text
            graphics.SetCompositingMode(CompositingModeSourceOver);
            br.SetColor(ToColor(textCol));
            graphics.DrawString(text.At(i), -1, &f, layout, &sf, &br);

            // paint "x"'s circle
            iterator.NextMarker(&shape);
            if ((xClicked == i || xHighlighted == i) && GetCurrentTheme()->tab.closeCircleEnabled) {
                br.SetColor(ToColor(circleColor));
                graphics.FillPath(&br, &shape);
            }

            // paint "x"
            iterator.NextMarker(&shape);
            pen.SetColor(ToColor(xColor));
            graphics.DrawPath(&pen, &shape);
            iterator.Rewind();
        }
    }

    int Count() { return (int)text.Count(); }

    void Insert(int index, const WCHAR* t) { text.InsertAt(index, str::Dup(t)); }

    bool Set(int index, const WCHAR* t) {
        if (index < Count()) {
            str::ReplacePtr(&text.At(index), t);
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

    void DeleteAll() { text.Reset(); }
};

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
        tab->Invalidate(tab->current);
        tab->Invalidate(tab->nextTab);
        tab->current = tab->nextTab;
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
            if ((int)index <= tab->current)
                tab->current++;
            tab->xClicked = -1;
            if (tab->isMouseInClientArea)
                PostMessage(hwnd, WM_MOUSEMOVE, 0, tab->mouseCoordinates);
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
                if ((int)index < tab->current) {
                    tab->current--;
                } else if ((int)index == tab->current) {
                    tab->current = -1;
                }
                tab->xClicked = -1;
                if (tab->isMouseInClientArea) {
                    PostMessage(hwnd, WM_MOUSEMOVE, 0, tab->mouseCoordinates);
                }
                if (tab->Count()) {
                    InvalidateRgn(hwnd, nullptr, FALSE);
                    UpdateWindow(hwnd);
                }
            }
            break;

        case TCM_DELETEALLITEMS:
            tab->DeleteAll();
            tab->current = -1;
            tab->highlighted = -1;
            tab->xClicked = -1;
            tab->xHighlighted = -1;
            break;

        case TCM_SETITEMSIZE:
            if (tab->Reshape(LOWORD(lp), HIWORD(lp))) {
                tab->xClicked = -1;
                if (tab->isMouseInClientArea) {
                    PostMessage(hwnd, WM_MOUSEMOVE, 0, tab->mouseCoordinates);
                }
                if (tab->Count()) {
                    InvalidateRgn(hwnd, nullptr, FALSE);
                    UpdateWindow(hwnd);
                }
            }
            break;

        case TCM_GETCURSEL:
            return tab->current;

        case TCM_SETCURSEL: {
            index = (int)wp;
            if (index >= tab->Count()) {
                return -1;
            }
            int previous = tab->current;
            if ((int)index != tab->current) {
                tab->Invalidate(tab->current);
                tab->Invalidate(index);
                tab->current = index;
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

            if (!tab->isMouseInClientArea) {
                // Track the mouse for leaving the client area.
                TRACKMOUSEEVENT tme = {0};
                tme.cbSize = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                if (TrackMouseEvent(&tme))
                    tab->isMouseInClientArea = true;
            }
            if (wp == 0xFF) // The mouse left the client area.
                tab->isMouseInClientArea = false;

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
                if (tab->nextTab != tab->current) {
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
    ScopedMem<WCHAR> winTitle(win::GetText(win->hwndFrame));
    CrashIf(!str::Eq(winTitle.Get(), tdata->frameTitle));
    bool expectedTocVisibility = tdata->showToc; // if not in presentation mode
    if (PM_DISABLED != win->presentation) {
        expectedTocVisibility = false; // PM_BLACK_SCREEN, PM_WHITE_SCREEN
        if (PM_ENABLED == win->presentation) {
            expectedTocVisibility = tdata->showTocPresentation;
        }
    }
    CrashIf(win->tocVisible != expectedTocVisibility);
    CrashIf(tdata->canvasRc != win->canvasRc);
}

// Must be called when the active tab is losing selection.
// This happens when a new document is loaded or when another tab is selected.
void SaveCurrentTabInfo(WindowInfo* win) {
    if (!win)
        return;

    int current = TabCtrl_GetCurSel(win->hwndTabBar);
    if (-1 == current)
        return;
    CrashIf(win->currentTab != win->tabs.At(current));

    TabInfo* tdata = win->currentTab;
    CrashIf(!tdata);
    if (win->tocLoaded) {
        tdata->tocState.Reset();
        HTREEITEM hRoot = TreeView_GetRoot(win->hwndTocTree);
        if (hRoot)
            UpdateTocExpansionState(tdata, win->hwndTocTree, hRoot);
    }
    VerifyTabInfo(win, tdata);

    // update the selection history
    win->tabSelectionHistory->Remove(tdata);
    win->tabSelectionHistory->Push(tdata);
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

static void SetTabTitle(WindowInfo* win, TabInfo* tab) {
    TCITEM tcs;
    tcs.mask = TCIF_TEXT;
    tcs.pszText = (WCHAR*)tab->GetTabTitle();
    TabCtrl_SetItem(win->hwndTabBar, win->tabs.Find(tab), &tcs);
}

// On load of a new document we insert a new tab item in the tab bar.
TabInfo* CreateNewTab(WindowInfo* win, const WCHAR* filePath) {
    CrashIf(!win);
    if (!win)
        return nullptr;

    TabInfo* tab = new TabInfo(filePath);
    win->tabs.Append(tab);
    tab->canvasRc = win->canvasRc;

    TCITEMW tcs;
    tcs.mask = TCIF_TEXT;
    tcs.pszText = (WCHAR*)tab->GetTabTitle();

    int index = (int)win->tabs.Count() - 1;
    if (-1 != TabCtrl_InsertItem(win->hwndTabBar, index, &tcs)) {
        TabCtrl_SetCurSel(win->hwndTabBar, index);
        UpdateTabWidth(win);
    } else {
        // TODO: what now?
        CrashIf(true);
    }

    return tab;
}

// Refresh the tab's title
void TabsOnChangedDoc(WindowInfo* win) {
    TabInfo* tab = win->currentTab;
    CrashIf(!tab != !win->tabs.Count());
    if (!tab)
        return;

    CrashIf(win->tabs.Find(tab) != TabCtrl_GetCurSel(win->hwndTabBar));
    VerifyTabInfo(win, tab);
    SetTabTitle(win, tab);
}

static void RemoveTab(WindowInfo* win, int idx) {
    TabInfo* tab = win->tabs.At(idx);
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
    if (win->tabs.Count() == 0)
        return;

    /* if (win->AsFixed() && win->AsFixed()->userAnnots && win->AsFixed()->userAnnotsModified) {
        // TODO: warn about unsaved changes
    } */

    int current = TabCtrl_GetCurSel(win->hwndTabBar);
    RemoveTab(win, current);

    if (win->tabs.Count() > 0) {
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
            LoadModelIntoTab(win, win->tabs.At(current));
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
    int count = (int)win->tabs.Count();
    bool showSingleTab = gGlobalPrefs->useTabs || win->tabsInTitlebar;
    if (count > (showSingleTab ? 0 : 1)) {
        ShowTabBar(win, true);
        ClientRect rect(win->hwndTabBar);
        SizeI tabSize = GetTabSize(win->hwndFrame);
        if (tabSize.dx > (rect.dx - 3) / count)
            tabSize.dx = (rect.dx - 3) / count;
        TabCtrl_SetItemSize(win->hwndTabBar, tabSize.dx, tabSize.dy);
    } else {
        ShowTabBar(win, false);
    }
}

void SetTabsInTitlebar(WindowInfo* win, bool set) {
    if (set == win->tabsInTitlebar)
        return;
    win->tabsInTitlebar = set;
    TabPainter* tab = (TabPainter*)GetWindowLongPtr(win->hwndTabBar, GWLP_USERDATA);
    tab->inTitlebar = set;
    SetParent(win->hwndTabBar, set ? win->hwndCaption : win->hwndFrame);
    ShowWindow(win->hwndCaption, set ? SW_SHOW : SW_HIDE);
    if (set != win->isMenuHidden)
        ShowHideMenuBar(win);
    if (set) {
        win->caption->UpdateTheme();
        win->caption->UpdateColors(win->hwndFrame == GetForegroundWindow());
        win->caption->UpdateBackgroundAlpha();
        RelayoutCaption(win);
    } else if (dwm::IsCompositionEnabled()) {
        // remove the extended frame
        MARGINS margins = {0};
        dwm::ExtendFrameIntoClientArea(win->hwndFrame, &margins);
        win->extendedFrameHeight = 0;
    }
    SetWindowPos(win->hwndFrame, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE);
}

// Selects the given tab (0-based index).
void TabsSelect(WindowInfo* win, int tabIndex) {
    int count = (int)win->tabs.Count();
    if (count < 2 || tabIndex < 0 || tabIndex >= count)
        return;
    NMHDR ntd = {nullptr, 0, TCN_SELCHANGING};
    if (TabsOnNotify(win, (LPARAM)&ntd))
        return;
    win->currentTab = win->tabs.At(tabIndex);
    int prevIndex = TabCtrl_SetCurSel(win->hwndTabBar, tabIndex);
    if (prevIndex != -1) {
        ntd.code = TCN_SELCHANGE;
        TabsOnNotify(win, (LPARAM)&ntd);
    }
}

// Selects the next (or previous) tab.
void TabsOnCtrlTab(WindowInfo* win, bool reverse) {
    int count = (int)win->tabs.Count();
    if (count < 2) {
        return;
    }

    int next = (TabCtrl_GetCurSel(win->hwndTabBar) + (reverse ? -1 : 1) + count) % count;
    TabsSelect(win, next);
}

static void SwapTabs(WindowInfo* win, int tab1, int tab2) {
    if (tab1 == tab2 || tab1 < 0 || tab2 < 0)
        return;

    std::swap(win->tabs.At(tab1), win->tabs.At(tab2));
    SetTabTitle(win, win->tabs.At(tab1));
    SetTabTitle(win, win->tabs.At(tab2));

    int current = TabCtrl_GetCurSel(win->hwndTabBar);
    if (tab1 == current)
        TabCtrl_SetCurSel(win->hwndTabBar, tab2);
    else if (tab2 == current)
        TabCtrl_SetCurSel(win->hwndTabBar, tab1);
}
