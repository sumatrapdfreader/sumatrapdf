/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "Tabs.h"

#include "AppPrefs.h"
#include "Caption.h"
#include "ChmModel.h"
#include "Controller.h"
#include "DisplayModel.h"
#include "Dpi.h"
#include "EbookControls.h"
#include "FileUtil.h"
#include "FileWatcher.h"
#include "GdiPlusUtil.h"
#include "Menu.h"
#include "resource.h"
#include "SumatraPDF.h"
#include "TableOfContents.h"
#include "UITask.h"
#include "WindowInfo.h"
#include "WinUtil.h"

static void SwapTabs(WindowInfo *win, int tab1, int tab2);

#define DEFAULT_CURRENT_BG_COL (COLORREF)-1

#define TAB_COLOR_BG      COLOR_BTNFACE
#define TAB_COLOR_TEXT    COLOR_BTNTEXT

#define T_CLOSING   (TCN_LAST + 1)
#define T_CLOSE     (TCN_LAST + 2)
#define T_DRAG      (TCN_LAST + 3)

#define TABBAR_HEIGHT    24
#define TAB_WIDTH        300

static bool g_FirefoxStyle = false;

int GetTabbarHeight(WindowInfo *win, float factor)
{
    return (int)(TABBAR_HEIGHT * win->uiDPIFactor * factor);
}

static inline SizeI GetTabSize(WindowInfo *win)
{
    return SizeI((int)(TAB_WIDTH * win->uiDPIFactor), (int)((TABBAR_HEIGHT) * win->uiDPIFactor));
}

static Brush *LoadBrush(SolidBrush &b, COLORREF c) {
    b.SetColor(Color(GetRValueSafe(c), GetGValueSafe(c), GetBValueSafe(c)));
    return &b;
}

static Pen *LoadPen(Pen &p, COLORREF c, REAL width) {
    p.SetColor(Color(GetRValueSafe(c), GetGValueSafe(c), GetBValueSafe(c)));
    p.SetWidth(width);
    return &p;
}

class TabPainter
{
    WStrVec text;
    PathData *data;
    int width, height;
    HWND hwnd;
public:
    int current, highlighted, xClicked, xHighlighted;
    bool isMouseInClientArea, isDragging;
    LPARAM mouseCoordinates;
    int nextTab;
    bool inTitlebar;
    COLORREF currBgCol;
    struct {
        COLORREF background, highlight, current, outline, bar, text, x_highlight, x_click, x_line;
    } color;

    TabPainter(HWND wnd, SizeI tabSize) :
        hwnd(wnd), data(NULL), width(0), height(0),
        current(-1), highlighted(-1), xClicked(-1), xHighlighted(-1), nextTab(-1),
        isMouseInClientArea(false), isDragging(false), inTitlebar(false), currBgCol(DEFAULT_CURRENT_BG_COL) {
        Reshape(tabSize.dx, tabSize.dy);
        EvaluateColors();
        memset(&color, 0, sizeof(color));
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
        width = dx; height = dy;

        GraphicsPath shape;
        // define tab's body
        int c = int((float)height * 0.6f + 0.5f); // size of bounding square for the arc
        shape.AddArc(0, 0, c, c, 180.0f, 90.0f);
        shape.AddArc(width - c, 0, c, c, 270.0f, 90.0f);
        shape.AddLine(width, height, 0, height);
        shape.CloseFigure();
        shape.SetMarker();

        // define "x"'s circle
        c = int((float)height * 0.78f + 0.5f); // size of bounding square for the circle
        int maxC = DpiScaleX(hwnd, 17);
        if (height > maxC) {
            c = DpiScaleX(hwnd, 17);
        }
        Point p(width - c - DpiScaleX(hwnd, 3), (height - c) / 2); // circle's position
        shape.AddEllipse(p.X, p.Y, c, c);
        shape.SetMarker();
        // define "x"
        int o = int((float)c * 0.286f + 0.5f); // "x"'s offset
        shape.AddLine(p.X+o, p.Y+o, p.X+c-o, p.Y+c-o);
        shape.StartFigure();
        shape.AddLine(p.X+c-o, p.Y+o, p.X+o, p.Y+c-o);
        shape.SetMarker();

        delete data;
        data = new PathData();
        shape.GetPathData(data);
        return true;
    }

    // Finds the index of the tab, which contains the given point.
    int IndexFromPoint(int x, int y, bool *inXbutton=NULL) {
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
            graphics.TransformPoints( CoordinateSpaceWorld, CoordinateSpaceDevice, &pt, 1);
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
        if (index < 0) return;

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
    void Paint(HDC hdc, RECT &rc) {
        IntersectClipRect(hdc, rc.left, rc.top, rc.right, rc.bottom);

        // paint the background
        bool isTranslucentMode = inTitlebar && dwm::IsCompositionEnabled();
        if (isTranslucentMode)
            PaintParentBackground(hwnd, hdc);
        else {
            HBRUSH brush = CreateSolidBrush(color.bar);
            FillRect(hdc, &rc, brush);
            DeleteObject(brush);
        }

        // TODO: GDI+ doesn't seem to cope well with SetWorldTransform
        XFORM ctm = { 1.0, 0, 0, 1.0, 0, 0 };
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
        Pen pen(&br);

        Font f(hdc, GetDefaultGuiFont());
        // TODO: adjust these constant values for DPI?
        RectF layout((REAL)DpiScaleX(hwnd,3), 1.0f, REAL(width - DpiScaleX(hwnd,20)), (REAL)height);
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
                //graphics.SetCompositingMode(CompositingModeSourceOver);
                graphics.DrawString(text.At(i), -1, &f, layout, &sf, LoadBrush(br, color.text));
                graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
                continue;
            }

            COLORREF bgCol = color.background;;
            if (current == i) {
                bgCol = color.current;
            } else if (highlighted == i) {
                bgCol = color.highlight;
            }

            // ensure contrast between text and background color
            // TODO: adjust threshold (and try adjusting both current/background tabs)
            COLORREF textCol = color.text;
            float bgLight = GetLightness(bgCol), textLight = GetLightness(textCol);
            if (textLight < bgLight ? bgLight < 0x70 : bgLight > 0x90)
                textCol = textLight ? AdjustLightness(textCol, 255.0f / textLight - 1.0f) : RGB(255, 255, 255);
            if (fabs(textLight - bgLight) < 0x40)
                textCol = bgLight < 0x80 ? RGB(255, 255, 255) : RGB(0, 0, 0);

            // paint tab's body
            graphics.SetCompositingMode(CompositingModeSourceCopy);
            iterator.NextMarker(&shape);
            LoadBrush(br, bgCol);
            graphics.FillPath(&br, &shape);

            // draw tab's text
            graphics.SetCompositingMode(CompositingModeSourceOver);
            graphics.DrawString(text.At(i), -1, &f, layout, &sf, LoadBrush(br, textCol));

            // paint "x"'s circle
            iterator.NextMarker(&shape);
            if (xClicked == i)
                graphics.FillPath(LoadBrush(br, color.x_click), &shape);
            else if (xHighlighted == i)
                graphics.FillPath(LoadBrush(br, color.x_highlight), &shape);

            // paint "x"
            iterator.NextMarker(&shape);
            if (xClicked == i || xHighlighted == i)
                LoadPen(pen, color.x_line, 2.0f);
            else
                LoadPen(pen, color.outline, 2.0f);
            graphics.DrawPath(&pen, &shape);
            iterator.Rewind();
        }
    }

    // Evaluates the colors for the tab's elements.
    void EvaluateColors(bool force=false) {
        COLORREF bg, txt;
        if (inTitlebar) {
            WindowInfo *win = FindWindowInfoByHwnd(hwnd);
            bg = win->caption->bgColor;
            txt = win->caption->textColor;
        }
        else {
            bg = GetSysColor(TAB_COLOR_BG);
            txt = GetSysColor(TAB_COLOR_TEXT);
        }
        if (!force && bg == color.bar && txt == color.text)
            return;

        color.bar  = bg;
        color.text = txt;

        int sign = GetLightness(color.text) > GetLightness(color.bar) ? -1 : 1;

        color.current      = AdjustLightness2(color.bar, sign * 25.0f);
        color.highlight   = AdjustLightness2(color.bar, sign * 15.0f);
        color.background  = AdjustLightness2(color.bar, -sign * 15.0f);
        color.outline     = AdjustLightness2(color.bar, -sign * 60.0f);
        color.x_line      = COL_CLOSE_X_HOVER;
        color.x_highlight = COL_CLOSE_HOVER_BG;
        color.x_click     = AdjustLightness2(color.x_highlight, -10.0f);
        if (currBgCol != DEFAULT_CURRENT_BG_COL) {
            color.current = currBgCol;
        }
    }

    int Count() {
        return (int)text.Count();
    }

    void Insert(int index, const WCHAR *t) {
        text.InsertAt(index, str::Dup(t));
    }

    bool Set(int index, const WCHAR *t) {
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

    void DeleteAll() {
        text.Reset();
    }
};

class TabNotification : public UITask {
    WindowInfo *win;
    UINT  code;
    int   index1, index2;

public:
    TabNotification(WindowInfo *win, UINT code, int index1=-1, int index2=-1) :
        win(win), code(code), index1(index1), index2(index2) { }

    virtual void Execute() {
        if (!WindowInfoStillValid(win)) {
            return;
        }
        NMHDR nmhdr = { NULL, 0, code };
        if (TabsOnNotify(win, (LPARAM)&nmhdr, index1, index2)) {
            return;
        }
        TabPainter *tab = (TabPainter *)GetWindowLongPtr(win->hwndTabBar, GWLP_USERDATA);
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
};

static WNDPROC DefWndProcTabBar = NULL;
static LRESULT CALLBACK WndProcTabBar(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;
    int index;
    LPTCITEM tcs;

    TabPainter *tab = (TabPainter *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_DESTROY:
        delete tab;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)0);
        break;

    case TCM_INSERTITEM:
        index = (int)wParam;
        tcs = (LPTCITEM)lParam;
        tab->Insert(index, tcs->pszText);
        if ((int)index <= tab->current)
            tab->current++;
        tab->xClicked = -1;
        if (tab->isMouseInClientArea)
            PostMessage(hwnd, WM_MOUSEMOVE, 0, tab->mouseCoordinates);
        InvalidateRgn(hwnd, NULL, FALSE);
        UpdateWindow(hwnd);
        break;

    case TCM_SETITEM:
        index = (int)wParam;
        tcs = (LPTCITEM)lParam;
        if (TCIF_TEXT & tcs->mask) {
            if (tab->Set(index, tcs->pszText))
                tab->Invalidate(index);
        }
        break;

    case TCM_DELETEITEM:
        index = (int)wParam;
        if (tab->Delete(index)) {
            if ((int)index < tab->current)
                tab->current--;
            else if ((int)index == tab->current)
                tab->current = -1;
            tab->xClicked = -1;
            if (tab->isMouseInClientArea)
                PostMessage(hwnd, WM_MOUSEMOVE, 0, tab->mouseCoordinates);
            if (tab->Count()) {
                InvalidateRgn(hwnd, NULL, FALSE);
                UpdateWindow(hwnd);
            }
        }
        break;

    case TCM_DELETEALLITEMS:
        tab->DeleteAll();
        tab->current = tab->highlighted = tab->xClicked = tab->xHighlighted = -1;
        break;

    case TCM_SETITEMSIZE:
        if (tab->Reshape(LOWORD(lParam), HIWORD(lParam))) {
            tab->xClicked = -1;
            if (tab->isMouseInClientArea)
                PostMessage(hwnd, WM_MOUSEMOVE, 0, tab->mouseCoordinates);
            if (tab->Count()) {
                InvalidateRgn(hwnd, NULL, FALSE);
                UpdateWindow(hwnd);
            }
        }
        break;

    case TCM_GETCURSEL:
        return tab->current;

    case TCM_SETCURSEL:
        {
            index = (int)wParam;
            if (index >= tab->Count()) return -1;
            int previous = tab->current;
            if ((int)index != tab->current) {
                tab->Invalidate(tab->current);
                tab->Invalidate(index);
                tab->current = index;
                UpdateWindow(hwnd);
            }
            return previous;
        }

    case WM_NCHITTEST:
        {
            if (!tab->inTitlebar || hwnd == GetCapture())
                return HTCLIENT;
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd, &pt);
            if (-1 != tab->IndexFromPoint(pt.x, pt.y))
                return HTCLIENT;
        }
        return HTTRANSPARENT;

    case WM_MOUSELEAVE:
        PostMessage(hwnd, WM_MOUSEMOVE, 0xFF, 0);
        return 0;

    case WM_MOUSEMOVE:
        {
            tab->mouseCoordinates = lParam;

            if (!tab->isMouseInClientArea) {
                // Track the mouse for leaving the client area.
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                if (TrackMouseEvent(&tme))
                    tab->isMouseInClientArea = true;
            }
            if (wParam == 0xFF)     // The mouse left the client area.
                tab->isMouseInClientArea = false;

            bool inX = false;
            int hl = wParam == 0xFF ? -1 : tab->IndexFromPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &inX);
            if (tab->isDragging && hl == -1)
                // preserve the highlighted tab if it's dragged outside the tabs' area
                hl = tab->highlighted;
            if (tab->highlighted != hl) {
                if (tab->isDragging) {
                    // send notification if the highlighted tab is dragged over another
                    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
                    uitask::Post(new TabNotification(win, T_DRAG, tab->highlighted, hl));
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
        tab->nextTab = tab->IndexFromPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &inX);
        if (inX) {
            // send request to close the tab
            WindowInfo *win = FindWindowInfoByHwnd(hwnd);
            uitask::Post(new TabNotification(win, T_CLOSING, tab->nextTab));
        }
        else if (tab->nextTab != -1) {
            if (tab->nextTab != tab->current) {
                // send request to select tab
                WindowInfo *win = FindWindowInfoByHwnd(hwnd);
                uitask::Post(new TabNotification(win, TCN_SELCHANGING));
            }
            tab->isDragging = true;
            SetCapture(hwnd);
        }
        return 0;

    case WM_LBUTTONUP:
        if (tab->xClicked != -1) {
            // send notification that the tab is closed
            WindowInfo *win = FindWindowInfoByHwnd(hwnd);
            uitask::Post(new TabNotification(win, T_CLOSE, tab->xClicked));
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
            tab->nextTab = tab->IndexFromPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            // send request to close the tab
            WindowInfo *win = FindWindowInfoByHwnd(hwnd);
            uitask::Post(new TabNotification(win, T_CLOSING, tab->nextTab));
        }
        return 0;

    case WM_MBUTTONUP:
        if (tab->xClicked != -1) {
            // send notification that the tab is closed
            WindowInfo *win = FindWindowInfoByHwnd(hwnd);
            uitask::Post(new TabNotification(win, T_CLOSE, tab->xClicked));
            tab->Invalidate(tab->xClicked);
            tab->xClicked = -1;
        }
        return 0;

    case WM_ERASEBKGND:
        return TRUE;

    case WM_PAINT:
        {
            RECT rc;
            GetUpdateRect(hwnd, &rc, FALSE);
            // TODO: when is wParam != NULL?
            hdc = wParam ? (HDC)wParam : BeginPaint(hwnd, &ps);

            DoubleBuffer buffer(hwnd, RectI::FromRECT(rc));
            tab->EvaluateColors();
            tab->Paint(buffer.GetDC(), rc);
            buffer.Flush(hdc);

            ValidateRect(hwnd, NULL);
            if (!wParam)
                EndPaint(hwnd, &ps);
            return 0;
        }

    case WM_SIZE:
        {
            WindowInfo *win = FindWindowInfoByHwnd(hwnd);
            if (win)
                UpdateTabWidth(win);
        }
        break;
    }

    return CallWindowProc(DefWndProcTabBar, hwnd, msg, wParam, lParam);
}

void CreateTabbar(WindowInfo *win)
{
    HWND hwndTabBar = CreateWindow(WC_TABCONTROL, L"", 
        WS_CHILD | WS_CLIPSIBLINGS /*| WS_VISIBLE*/ | 
        TCS_FOCUSNEVER | TCS_FIXEDWIDTH | TCS_FORCELABELLEFT, 
        0, 0, 0, 0, 
        win->hwndFrame, (HMENU)IDC_TABBAR, GetModuleHandle(NULL), NULL);

    if (!DefWndProcTabBar)
        DefWndProcTabBar = (WNDPROC)GetWindowLongPtr(hwndTabBar, GWLP_WNDPROC);
    SetWindowLongPtr(hwndTabBar, GWLP_WNDPROC, (LONG_PTR)WndProcTabBar);

    SizeI tabSize = GetTabSize(win);
    TabPainter *tp = new TabPainter(hwndTabBar, tabSize);
    SetWindowLongPtr(hwndTabBar, GWLP_USERDATA, (LONG_PTR)tp);

    SetWindowFont(hwndTabBar, GetDefaultGuiFont(), FALSE);
    TabCtrl_SetItemSize(hwndTabBar, tabSize.dx, tabSize.dy);

    win->hwndTabBar = hwndTabBar;

    win->tabSelectionHistory = new Vec<TabData *>();
}

// Saves some of the document's data from the WindowInfo to the TabData.
static void SaveTabData(WindowInfo *win, TabData *tdata)
{
    tdata->tocState = win->tocState;
    tdata->showToc = win->isFullScreen || win->presentation != PM_DISABLED ? win->tocBeforeFullScreen : win->tocVisible;
    tdata->ctrl = win->ctrl;
    free(tdata->title);
    tdata->title = win::GetText(win->hwndFrame);
    str::ReplacePtr(&tdata->filePath, win->loadedFilePath);
    tdata->canvasRc = win->canvasRc;
    tdata->watcher = win->watcher;
}

static void PrepareAndSaveTabData(WindowInfo *win, TabData **tdata)
{
    if (*tdata == NULL)
        *tdata = new TabData();

    if (win->tocLoaded) {
        win->tocState.Reset();
        HTREEITEM hRoot = TreeView_GetRoot(win->hwndTocTree);
        if (hRoot)
            UpdateTocExpansionState(win, hRoot);
    }

    if (win->AsChm())
        win->AsChm()->RemoveParentHwnd();

    SaveTabData(win, *tdata);
    // prevent data from being deleted
    win->ctrl = NULL;
    win->watcher = NULL;
}

// Must be called when the active tab is losing selection.
// This happens when a new document is loaded or when another tab is selected.
void SaveCurrentTabData(WindowInfo *win)
{
    if (!win)
        return;

    int current = TabCtrl_GetCurSel(win->hwndTabBar);
    if (current == -1) {
        return;
    }
    TCITEM tcs;
    tcs.mask = TCIF_PARAM;
    if (!TabCtrl_GetItem(win->hwndTabBar, current, &tcs)) {
        return;
    }
    // we use the lParam member of the TCITEM structure of the tab, to save the TabData pointer in
    PrepareAndSaveTabData(win, (TabData **)&tcs.lParam);
    TabCtrl_SetItem(win->hwndTabBar, current, &tcs);

    // update the selection history
    win->tabSelectionHistory->Remove((TabData *)tcs.lParam);
    win->tabSelectionHistory->Push((TabData *)tcs.lParam);
}

static void UpdateCurrentTabBgColForWindow(WindowInfo *win)
{
    COLORREF bgCol = DEFAULT_CURRENT_BG_COL;
    if (win->AsEbook()) {
        GetEbookColors(NULL, &bgCol);
    } else {
        // TODO: match either the toolbar (if shown) or background
    }
    SetCurrentTabBgCol(win, bgCol);
    RepaintNow(win->hwndTabBar);
}

int TabsGetCount(WindowInfo *win)
{
    if (!win)
        return -1;
    return TabCtrl_GetItemCount(win->hwndTabBar);
}

// Gets the TabData pointer from the lParam member of the TCITEM structure of the tab.
TabData *GetTabData(WindowInfo *win, int tabIndex)
{
    TCITEM tcs;
    tcs.mask = TCIF_PARAM;
    if (TabCtrl_GetItem(win->hwndTabBar, tabIndex, &tcs))
        return (TabData *)tcs.lParam;
    return NULL;
}

TabData *GetTabDataByCtrl(WindowInfo *win, Controller *ctrl)
{
    TabData *td;
    for (int i = 0; (td = GetTabData(win, i)) != NULL; i++) {
        if (ctrl == td->ctrl)
            return td;
    }
    return NULL;
}

static int FindTabIndex(WindowInfo *win, TabData *tdata)
{
    int count = TabsGetCount(win);
    for (int i = 0; i < count; i++) {
        if (tdata == GetTabData(win, i))
            return i;
    }
    return -1;
}

static void DeleteTabData(TabData *tdata, bool deleteModel)
{
    if (!tdata) {
        return;
    }
    if (deleteModel) {
        delete tdata->ctrl;
        FileWatcherUnsubscribe(tdata->watcher);
    }
    free(tdata->title);
    free(tdata->filePath);
    delete tdata;
}

// On load of a new document we insert a new tab item in the tab bar.
// Its text is the name of the opened file.
void TabsOnLoadedDoc(WindowInfo *win)
{
    if (!win)
        return;

    TabData *td = new TabData();
    SaveTabData(win, td);

    TCITEM tcs;
    tcs.mask = TCIF_TEXT | TCIF_PARAM;
    tcs.pszText = (WCHAR *)path::GetBaseName(win->loadedFilePath);
    tcs.lParam = (LPARAM)td;

    int count = TabsGetCount(win);
    if (-1 != TabCtrl_InsertItem(win->hwndTabBar, count, &tcs)) {
        TabCtrl_SetCurSel(win->hwndTabBar, count);
        UpdateTabWidth(win);
    }
    else
        DeleteTabData(td, false);
    UpdateCurrentTabBgColForWindow(win);
}

// Refresh the tab's title
void TabsOnChangedDoc(WindowInfo *win)
{
    if (TabsGetCount(win) <= 0)
        return;

    int current = TabCtrl_GetCurSel(win->hwndTabBar);
    TabData *tdata = GetTabData(win, current);
    CrashIf(!tdata);
    SaveTabData(win, tdata);

    TCITEM tcs;
    tcs.mask = TCIF_TEXT;
    tcs.pszText = (WCHAR *)path::GetBaseName(win->loadedFilePath);
    TabCtrl_SetItem(win->hwndTabBar, current, &tcs);
    UpdateCurrentTabBgColForWindow(win);
}

// Called when we're closing a document
void TabsOnCloseDoc(WindowInfo *win)
{
    int count = TabsGetCount(win);
    if (count <= 0)
        return;

    if (win->AsFixed() && win->AsFixed()->userAnnots && win->AsFixed()->userAnnotsModified) {
        // TODO: warn about unsaved changes
    }

    int current = TabCtrl_GetCurSel(win->hwndTabBar);
    TabData *tdata = GetTabData(win, current);
    win->tabSelectionHistory->Remove(tdata);
    UpdateTabFileDisplayStateForWin(win, tdata);
    DeleteTabData(tdata, false);
    TabCtrl_DeleteItem(win->hwndTabBar, current);
    UpdateTabWidth(win);
    if (count > 1) {
        tdata = win->tabSelectionHistory->Pop();
        TabCtrl_SetCurSel(win->hwndTabBar, FindTabIndex(win, tdata));
        LoadModelIntoTab(win, tdata);
        UpdateCurrentTabBgColForWindow(win);
    }
}

// Called when we're closing an entire window (quitting)
void TabsOnCloseWindow(WindowInfo *win)
{
    int count = TabsGetCount(win);
    for (int i = 0; i < count; i++) {
        TabData *tdata = GetTabData(win, i);
        if (tdata) {
            UpdateTabFileDisplayStateForWin(win, tdata);
            DeleteTabData(tdata, win->ctrl != tdata->ctrl);
        }
    }
    win->tabSelectionHistory->Reset();
    TabCtrl_DeleteAllItems(win->hwndTabBar);
}

// On tab selection, we save the data for the tab which is losing selection and
// load the data of the selected tab into the WindowInfo.
LRESULT TabsOnNotify(WindowInfo *win, LPARAM lparam, int tab1, int tab2)
{
    LPNMHDR data = (LPNMHDR)lparam;

    switch(data->code) {
    case TCN_SELCHANGING:
        // TODO: Should we allow the switch of the tab if we are in process of printing?

        SaveCurrentTabData(win);
        return FALSE;

    case TCN_SELCHANGE:
        {
            int current = TabCtrl_GetCurSel(win->hwndTabBar);
            LoadModelIntoTab(win, GetTabData(win, current));
            UpdateCurrentTabBgColForWindow(win);
        }
        break;

    case T_CLOSING:
        // allow the closure
        return FALSE;

    case T_CLOSE:
        {
            int current = TabCtrl_GetCurSel(win->hwndTabBar);
            if (tab1 == current) {
                CloseTab(win);
            }
            else {
                TabData *tdata = GetTabData(win, tab1);
                win->tabSelectionHistory->Remove(tdata);
                UpdateTabFileDisplayStateForWin(win, tdata);
                DeleteTabData(tdata, true);
                TabCtrl_DeleteItem(win->hwndTabBar, tab1);
                UpdateTabWidth(win);
            }
        }
        break;

    case T_DRAG:
        SwapTabs(win, tab1, tab2);
        break;
    }
    return TRUE;
}

static void ShowTabBar(WindowInfo *win, bool show)
{
    if (show == win->tabsVisible)
        return;
    win->tabsVisible = show;
    win::SetVisibility(win->hwndTabBar, show);
    ClientRect rect(win->hwndFrame);
    SendMessage(win->hwndFrame, WM_SIZE, 0, MAKELONG(rect.dx, rect.dy));
}

void UpdateTabWidth(WindowInfo *win)
{
    int count = TabsGetCount(win);
    bool showSingleTab = gGlobalPrefs->useTabs || win->tabsInTitlebar;
    if (count > (showSingleTab ? 0 : 1)) {
        ShowTabBar(win, true);
        ClientRect rect(win->hwndTabBar);
        SizeI tabSize = GetTabSize(win);
        if (tabSize.dx > (rect.dx - 3) / count)
            tabSize.dx = (rect.dx - 3) / count;
        TabCtrl_SetItemSize(win->hwndTabBar, tabSize.dx, tabSize.dy);
    }
    else {
        ShowTabBar(win, false);
    }
}

void SetCurrentTabBgCol(WindowInfo *win, COLORREF bgCol)
{
    TabPainter *tab = (TabPainter *)GetWindowLongPtr(win->hwndTabBar, GWLP_USERDATA);
    tab->currBgCol = bgCol;
    tab->EvaluateColors(true);
}

void SetTabsInTitlebar(WindowInfo *win, bool set)
{
    if (set == win->tabsInTitlebar)
        return;
    if (set) {
        // make sure to load Dwmapi.dll before WndProcFrame -> CustomCaptionFrameProc ->
        // dwm::DefWindowProc_ -> dwm::Initialize might produce an infinite loop
        dwm::Initialize();
    }
    win->tabsInTitlebar = set;
    TabPainter *tab = (TabPainter *)GetWindowLongPtr(win->hwndTabBar, GWLP_USERDATA);
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
    }
    else if (dwm::IsCompositionEnabled()) {
        // remove the extended frame
        MARGINS margins = { 0 };
        dwm::ExtendFrameIntoClientArea(win->hwndFrame, &margins);
        win->extendedFrameHeight = 0;
    }
    SetWindowPos(win->hwndFrame, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE);
}

// Selects the given tab (0-based index).
void TabsSelect(WindowInfo *win, int tabIndex)
{
    int count = TabsGetCount(win);
    if (count < 2 || tabIndex < 0 || tabIndex >= count)
        return;
    NMHDR ntd = { NULL, 0, TCN_SELCHANGING };
    if (TabsOnNotify(win, (LPARAM)&ntd))
        return;
    int prevIndex = TabCtrl_SetCurSel(win->hwndTabBar, tabIndex);
    if (prevIndex != -1) {
        ntd.code = TCN_SELCHANGE;
        TabsOnNotify(win, (LPARAM)&ntd);
    }
}

// Selects the next (or previous) tab.
void TabsOnCtrlTab(WindowInfo *win, bool reverse)
{
    int count = TabsGetCount(win);
    if (count < 2)
        return;

    int next = (TabCtrl_GetCurSel(win->hwndTabBar) + (reverse ? -1 : 1) + count) % count;
    TabsSelect(win, next);
}

static void SwapTabs(WindowInfo *win, int tab1, int tab2)
{
    if (tab1 == tab2 || tab1 < 0 || tab2 < 0)
        return;

    WCHAR buf1[MAX_PATH], buf2[MAX_PATH];
    LPARAM lp;
    TCITEM tcs;
    tcs.mask = TCIF_TEXT | TCIF_PARAM;
    tcs.cchTextMax = MAX_PATH;

    tcs.pszText = buf1;
    if (!TabCtrl_GetItem(win->hwndTabBar, tab1, &tcs))
        return;
    if (tcs.pszText != buf1)
        str::BufSet(buf1, dimof(buf1), tcs.pszText);
    lp = tcs.lParam;

    tcs.pszText = buf2;
    if (!TabCtrl_GetItem(win->hwndTabBar, tab2, &tcs))
        return;
    if (tcs.pszText != buf2)
        str::BufSet(buf2, dimof(buf2), tcs.pszText);

    tcs.pszText = buf2;
    TabCtrl_SetItem(win->hwndTabBar, tab1, &tcs);
    tcs.pszText = buf1;
    tcs.lParam = lp;
    TabCtrl_SetItem(win->hwndTabBar, tab2, &tcs);

    int current = TabCtrl_GetCurSel(win->hwndTabBar);
    if (tab1 == current)
        TabCtrl_SetCurSel(win->hwndTabBar, tab2);
    else if (tab2 == current)
        TabCtrl_SetCurSel(win->hwndTabBar, tab1);
}

// Adjusts lightness by 1/255 units.
COLORREF AdjustLightness2(COLORREF c, float units) {
    float lightness = GetLightness(c);
    units = limitValue(units, -lightness, 255.0f - lightness);
    if (0.0f == lightness)
        return RGB(BYTE(units + 0.5f), BYTE(units + 0.5f), BYTE(units + 0.5f));
    return AdjustLightness(c, 1.0f + units / lightness);
}
