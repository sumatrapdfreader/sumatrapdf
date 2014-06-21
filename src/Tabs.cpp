/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "Tabs.h"

#include "AppPrefs.h"
#include "ChmModel.h"
#include "Controller.h"
#include "FileWatcher.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "resource.h"
#include "SumatraPDF.h"
#include "TableOfContents.h"
#include "UITask.h"
#include "WindowInfo.h"
#include "WinUtil.h"


// Comment this for default drawing.
#define OWN_TAB_DRAWING

#ifdef OWN_TAB_DRAWING
#define TAB_COLOR_BG                        COLOR_BTNFACE
#define TAB_COLOR_TEXT                      COLOR_BTNTEXT
#define TAB_COLOR_ACTIVEBACKGROUND          COLOR_GRADIENTACTIVECAPTION
#define TAB_COLOR_INACTIVEBACKGROUND        COLOR_GRADIENTINACTIVECAPTION
#define TAB_COLOR_ACTIVECAPTIONELEMENT      COLOR_CAPTIONTEXT
#define TAB_COLOR_INACTIVECAPTIONELEMENT    COLOR_INACTIVECAPTIONTEXT

#define T_CLOSING   (TCN_LAST + 1)
#define T_CLOSE     (TCN_LAST + 2)
#define T_DRAG      (TCN_LAST + 3)

#define BTN_STATE_DEF    0x00
#define BTN_STATE_HIGH   0x01
#define BTN_STATE_CLICK  0x02

class TabPainter
{
    WStrVec text;
    PathData *data;
    int width, height, buttonSize;
    HWND hwnd;
    struct {
        COLORREF background, highlight, select, outline, bar, text, x_highlight, x_click, x_line, element;
    } color;

public:
    int current, highlighted, xClicked, xHighlighted;
    bool isMouseInClientArea, isDragging;
    LPARAM mouseCoordinates;
    int nextTab;
    bool inTitlebar;
    WORD buttonsState;

    TabPainter(HWND wnd, int tabWidth, int tabHeight) :
        hwnd(wnd), data(NULL), width(0), height(0), buttonSize(0),
        current(-1), highlighted(-1), xClicked(-1), xHighlighted(-1), nextTab(-1),
        isMouseInClientArea(false), isDragging(false), inTitlebar(false) {
        Reshape(tabWidth, tabHeight);
        EvaluateColors();
        memset(&color, 0, sizeof(color));
        buttonsState = MAKEWORD(-1, BTN_STATE_DEF);
    }

    ~TabPainter() {
        delete data;
        DeleteAll();
    }

    // Calculates tab's elements, based on its width and height.
    // Generates a GraphicsPath, which is used for painting the tab, etc.
    bool Reshape(int dx, int dy) {
        dx--; dy--;
        if (width == dx && height == dy)
            return false;
        width = dx; height = dy;
        buttonSize = dy;

        GraphicsPath shape;
        // define tab's body
        int c = int((float)height * 0.6f + 0.5f); // size of bounding square for the arc
        shape.AddArc(0, 0, c, c, 180.0f, 90.0f);
        shape.AddArc(width - c, 0, c, c, 270.0f, 90.0f);
        shape.AddLine(width, height, 0, height);
        shape.CloseFigure();
        shape.SetMarker();
        // define "x"'s circle
        c = height > 17 ? 14 : int((float)height * 0.78f + 0.5f); // size of bounding square for the circle
        Point p(width - c - 3, (height - c) / 2); // circle's position
        shape.AddEllipse(p.X, p.Y, c, c);
        shape.SetMarker();
        // define "x"
        int o = int((float)c * 0.286f + 0.5f); // "x"'s offset
        shape.AddLine(p.X+o, p.Y+o, p.X+c-o, p.Y+c-o);
        shape.StartFigure();
        shape.AddLine(p.X+c-o, p.Y+o, p.X+o, p.Y+c-o);
        shape.SetMarker();

        // define button's body
        Rect r1(0, 0, buttonSize, buttonSize);
        shape.AddRectangle(r1);
        shape.SetMarker();
        c = int((float)buttonSize * 0.3f + 0.5f); // amount of deflation for the rectangle
        r1.Inflate(-c, -c);
        r1.Width++;
        r1.Height++;
        // define "minimize" button
        shape.AddLine(r1.GetLeft(), r1.GetBottom(), r1.GetRight(), r1.GetBottom());
        shape.SetMarker();
        // define "maximize" button
        shape.AddRectangle(r1);
        shape.SetMarker();
        // define "restore" button
        Rect r2(r1);
        r2.Inflate(-1, -1);
        r2.Offset(1, -1);
        shape.AddLine(r2.GetLeft(), r2.GetTop(), r2.GetRight(), r2.GetTop());
        shape.AddLine(r2.GetRight(), r2.GetTop(), r2.GetRight(), r2.GetBottom());
        r2.Offset(-2, 2);
        shape.AddRectangle(r2);
        shape.SetMarker();
        // define "close" button
        shape.AddLine(r1.GetLeft(), r1.GetTop(), r1.GetRight(), r1.GetBottom());
        shape.StartFigure();
        shape.AddLine(r1.GetLeft(), r1.GetBottom(), r1.GetRight(), r1.GetTop());
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
        graphics.TranslateTransform(1.0f, REAL(rClient.dy - height - 1));
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

    // Finds the index of the button, which contains the given point.
    int ButtonFromPoint(int x, int y) {
        if (!inTitlebar)
            return -1;
        Point point(x, y);
        Graphics graphics(hwnd);
        GraphicsPath shapes(data->Points, data->Types, data->Count);
        GraphicsPath shape;
        GraphicsPathIterator iterator(&shapes);
        iterator.NextMarker(&shape); iterator.NextMarker(&shape);
        iterator.NextMarker(&shape); iterator.NextMarker(&shape);

        ClientRect rClient(hwnd);
        graphics.TranslateTransform((REAL)rClient.dx, 0.0f);
        for (int i = 0; i < 3; i++) {
            graphics.TranslateTransform(REAL(-buttonSize - 1), 0.0f);
            Point pt(point);
            graphics.TransformPoints( CoordinateSpaceWorld, CoordinateSpaceDevice, &pt, 1);
            if (shape.IsVisible(pt, &graphics))
                return i;
        }
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
        graphics.TranslateTransform(REAL((width + 1) * index) + 1.0f, REAL(rClient.dy - height - 1));
        HRGN hRgn = region.GetHRGN(&graphics);
        InvalidateRgn(hwnd, hRgn, FALSE);
        DeleteObject(hRgn);
    }

    // Invalidates the buttons' region in the client area.
    void InvalidateButtons() {
        if (!inTitlebar)
            return;
        ClientRect rClient(hwnd);
        Graphics graphics(hwnd);
        graphics.TranslateTransform(REAL(rClient.dx - 3*(buttonSize + 1)), 0.0f);
        Region region(Rect(0, 0, 3*(buttonSize + 1) + 1, buttonSize + 1));
        HRGN hRgn = region.GetHRGN(&graphics);
        InvalidateRgn(hwnd, hRgn, FALSE);
        DeleteObject(hRgn);
    }

    // Paints the tabs that intersect the window's update rectangle.
    void Paint(HDC hdc, RECT &rc) {
        ClientRect rClient(hwnd);
        HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, rClient.dx, rClient.dy);
        HDC memDC = CreateCompatibleDC(hdc);
        DeleteObject(SelectObject(memDC, hMemBmp));
        IntersectClipRect(memDC, rc.left, rc.top, rc.right, rc.bottom);

        // paint the background
        HBRUSH brush = CreateSolidBrush(color.bar);
        FillRect(memDC, &rc, brush);
        DeleteObject(brush);

        Graphics graphics(memDC);
        graphics.SetCompositingMode(CompositingModeSourceOver);
        graphics.SetCompositingQuality(CompositingQualityHighQuality);
        graphics.SetSmoothingMode(SmoothingModeHighQuality);
        graphics.SetPageUnit(UnitPixel);
        GraphicsPath shapes(data->Points, data->Types, data->Count);
        GraphicsPath shape;
        GraphicsPathIterator iterator(&shapes);

        SolidBrush br(Color(0, 0, 0));
        Pen pen(&br);

        Font f(memDC, gDefaultGuiFont);
        RectF layout(3.0f, 0.0f, REAL(width - 20), (REAL)height);
        StringFormat sf(StringFormat::GenericDefault());
        sf.SetFormatFlags(StringFormatFlagsNoWrap);
        sf.SetLineAlignment(StringAlignmentCenter);
        sf.SetTrimming(StringTrimmingEllipsisCharacter);

        graphics.TranslateTransform(1.0f, REAL(rClient.dy - height - 1));
        for (int i = 0; i < Count(); i++) {
            if (graphics.IsVisible(0, 0, width + 1, height + 1)) {
                // paint tab's body
                iterator.NextMarker(&shape);
                if (current == (int)i)
                    graphics.FillPath(LoadBrush(br, color.select), &shape);
                else if (highlighted == (int)i)
                    graphics.FillPath(LoadBrush(br, color.highlight), &shape);
                else
                    graphics.FillPath(LoadBrush(br, color.background), &shape);
                graphics.DrawPath(LoadPen(pen, color.outline, 1.0f), &shape);

                // draw tab's text
                graphics.DrawString(text.At(i), -1, &f, layout, &sf, LoadBrush(br, color.text));

                // paint "x"'s circle
                iterator.NextMarker(&shape);
                if (xClicked == (int)i)
                    graphics.FillPath(LoadBrush(br, color.x_click), &shape);
                else if (xHighlighted == (int)i)
                    graphics.FillPath(LoadBrush(br, color.x_highlight), &shape);

                // paint "x"
                iterator.NextMarker(&shape);
                if (xClicked == (int)i || xHighlighted == (int)i)
                    LoadPen(pen, color.x_line, 2.0f);
                else
                    LoadPen(pen, color.outline, 2.0f);
                graphics.DrawPath(&pen, &shape);

                iterator.Rewind();
            }
            graphics.TranslateTransform(REAL(width + 1), 0.0f);
        }
        // draw the line at the bottom of the tab bar
        graphics.DrawLine(LoadPen(pen, color.outline, 1.0f), 0, height, rc.right, height);

        // draw the buttons
        if (inTitlebar) {
            graphics.ResetTransform();
            graphics.TranslateTransform((REAL)rClient.dx, 0.0f);
            if (graphics.IsVisible(0, 0, -3*(buttonSize + 1) - 1, buttonSize + 1)) {
                iterator.NextMarker(&shape); iterator.NextMarker(&shape);
                iterator.NextMarker(&shape); iterator.NextMarker(&shape);
                for (BYTE i = 0; i < 3; i++) {
                    graphics.TranslateTransform(REAL(-buttonSize - 1), 0.0f);
                    if (i == LOBYTE(buttonsState)) {
                        if (BTN_STATE_CLICK & HIBYTE(buttonsState)) {
                            LoadBrush(br, i == 0 ? color.x_click : color.background);
                            graphics.FillPath(&br, &shape);
                        }
                        else if (BTN_STATE_HIGH & HIBYTE(buttonsState)) {
                            LoadBrush(br, i == 0 ? color.x_highlight : color.select);
                            graphics.FillPath(&br, &shape);
                        }
                    }
                }
                graphics.SetSmoothingMode(SmoothingModeNone);
                BOOL isMaximized = IsZoomed(GetParent(hwnd));
                for (BYTE i = 0; i < 3; i++) {
                    iterator.NextMarker(&shape);
                    if (i == 1 && isMaximized || i == 2 && !isMaximized)
                        iterator.NextMarker(&shape);
                    graphics.DrawPath(LoadPen(pen, color.element, 2.0f), &shape);
                    graphics.TranslateTransform(REAL(buttonSize + 1), 0.0f);
                }
            }
        }

        BitBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, memDC, rc.left, rc.top, SRCCOPY);

        DeleteDC(memDC);
        DeleteObject(hMemBmp);
    }

    // Evaluates the colors for the tab's elements.
    void EvaluateColors() {
        COLORREF bg;
        COLORREF txt = GetSysColor(TAB_COLOR_TEXT);
        if (inTitlebar) {
            bool active = GetParent(hwnd) == GetForegroundWindow() ? true : false;
            bg = active ? GetSysColor(TAB_COLOR_ACTIVEBACKGROUND)
                        : GetSysColor(TAB_COLOR_INACTIVEBACKGROUND);
            color.element = active ? GetSysColor(TAB_COLOR_ACTIVECAPTIONELEMENT)
                                   : GetSysColor(TAB_COLOR_INACTIVECAPTIONELEMENT);
        }
        else
            bg = GetSysColor(TAB_COLOR_BG);
        if (bg == color.bar && txt == color.text)
            return;

        color.bar  = bg;
        color.text = txt;

        int sign = 240.0f < GetLightness(color.bar) ? -1 : 1;

        color.select      = AdjustLightness2(color.bar, sign * 25.0f);
        color.highlight   = AdjustLightness2(color.bar, sign * 15.0f);
        color.background  = AdjustLightness2(color.bar, -sign * 15.0f);

        sign = GetLightness(color.text) < GetLightness(color.bar) ? -1 : 1;

        color.outline     = AdjustLightness2(color.bar, sign * 60.0f);
        color.x_line      = COL_CLOSE_X_HOVER;
        color.x_highlight = COL_CLOSE_HOVER_BG;
        color.x_click     = AdjustLightness2(color.x_highlight, -10.0f);
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
            free(text.At(index));
            text.RemoveAt(index);
            return true;
        }
        return false;
    }

    void DeleteAll() {
        text.Reset();
    }

private:
    float GetLightness(COLORREF c) {
        BYTE R = GetRValueSafe(c), G = GetGValueSafe(c), B = GetBValueSafe(c);
        BYTE M = max(max(R, G), B), m = min(min(R, G), B);
        return (M + m) / 2.0f;
    }

    // Adjusts lightness by 1/255 units.
    COLORREF AdjustLightness2(COLORREF c, float units) {
        float lightness = GetLightness(c);
        units = limitValue(units, -lightness, 255.0f - lightness);
        if (0.0f == lightness)
            return RGB(BYTE(units + 0.5f), BYTE(units + 0.5f), BYTE(units + 0.5f));
        return AdjustLightness(c, 1.0f + units / lightness);
    }

    Brush *LoadBrush(SolidBrush &b, COLORREF c) {
        b.SetColor(Color(GetRValueSafe(c), GetGValueSafe(c), GetBValueSafe(c)));
        return &b;
    }

    Pen *LoadPen(Pen &p, COLORREF c, REAL width) {
        p.SetColor(Color(GetRValueSafe(c), GetGValueSafe(c), GetBValueSafe(c)));
        p.SetWidth(width);
        return &p;
    }
};

// TODO: why can't we just call TabsOnNotify directly?
class TabNotification : public UITask {
    WindowInfo *win;
    UINT  code;
    int   index1, index2;

public:
    TabNotification(WindowInfo *win, UINT code, int index1=-1, int index2=-1) :
        win(win), code(code), index1(index1), index2(index2) { }

    virtual void Execute() {
        if (WindowInfoStillValid(win)) {
            NMHDR nmhdr = { NULL, 0, code };
            if (!TabsOnNotify(win, (LPARAM)&nmhdr, index1, index2)) {
                TabPainter *tab = (TabPainter *)GetWindowLongPtr(win->hwndTabBar, GWLP_USERDATA);
                if (T_CLOSING == code) {
                    // if we have permission to close the tab
                    tab->Invalidate(tab->nextTab);
                    tab->xClicked = tab->nextTab;
                }
                else if (TCN_SELCHANGING == code) {
                    // if we have permission to select the tab
                    tab->Invalidate(tab->current);
                    tab->Invalidate(tab->nextTab);
                    tab->current = tab->nextTab;
                    // send notification that the tab is selected
                    nmhdr.code = TCN_SELCHANGE;
                    TabsOnNotify(win, (LPARAM)&nmhdr);
                }
            }
        }
    }
};

WNDPROC DefWndProcTabBar;
LRESULT CALLBACK WndProcTabBar(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;
    int index, xPos, yPos;
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
        return HTCLIENT;

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

            if (tab->inTitlebar && hl == -1) {
                int btn = tab->ButtonFromPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                BYTE state = (BYTE)btn == LOBYTE(tab->buttonsState) ? BTN_STATE_HIGH | HIBYTE(tab->buttonsState)
                                                                    : BTN_STATE_HIGH;
                WORD btnsState = MAKEWORD(btn, state);
                if (tab->buttonsState != btnsState) {
                    tab->InvalidateButtons();
                    tab->buttonsState = btnsState;
                }
            }
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
        else if (tab->inTitlebar) {
            int btn = tab->ButtonFromPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (btn == -1) {
                HWND parent = GetParent(hwnd);
                if (!IsZoomed(parent)) {
                    // Cancel the mouse's tracking and post a message to the parent to move the window.
                    TRACKMOUSEEVENT tme;
                    tme.cbSize = sizeof(TRACKMOUSEEVENT);
                    tme.dwFlags = TME_CANCEL | TME_LEAVE;
                    tme.hwndTrack = hwnd;
                    if (TrackMouseEvent(&tme))
                        tab->isMouseInClientArea = false;
                    PostMessage(parent, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                }
            }
            else {
                WORD btnsState = MAKEWORD(btn, BTN_STATE_HIGH | BTN_STATE_CLICK);
                if (tab->buttonsState != btnsState) {
                    tab->InvalidateButtons();
                    tab->buttonsState = btnsState;
                }
            }
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
        if (tab->inTitlebar && BTN_STATE_CLICK & HIBYTE(tab->buttonsState)) {
            BYTE btn = LOBYTE(tab->buttonsState);
            tab->buttonsState = MAKEWORD(btn, HIBYTE(tab->buttonsState) & ~BTN_STATE_CLICK);
            if (btn < 3) {
                HWND parent = GetParent(hwnd);
                WPARAM wp = 0 == btn ? SC_CLOSE : 2 == btn ? SC_MINIMIZE : IsZoomed(parent) ? SC_RESTORE : SC_MAXIMIZE;
                PostMessage(parent, WM_SYSCOMMAND, wp, 0);
                tab->InvalidateButtons();
            }
        }
        return 0;

    case WM_LBUTTONDBLCLK:
        if (tab->inTitlebar) {
            xPos = GET_X_LPARAM(lParam);
            yPos = GET_Y_LPARAM(lParam);
            if (-1 == tab->IndexFromPoint(xPos, yPos) && -1 == tab->ButtonFromPoint(xPos, yPos)) {
                HWND parent = GetParent(hwnd);
                PostMessage(parent, WM_SYSCOMMAND, IsZoomed(parent) ? SC_RESTORE : SC_MAXIMIZE, 0);
            }
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

    case WM_RBUTTONDOWN:
        if (tab->inTitlebar) {
            xPos = GET_X_LPARAM(lParam);
            yPos = GET_Y_LPARAM(lParam);
            if (-1 == tab->IndexFromPoint(xPos, yPos) && -1 == tab->ButtonFromPoint(xPos, yPos))
                SetCapture(hwnd);
        }
        return 0;

    case WM_RBUTTONUP:
        if (tab->inTitlebar && hwnd == GetCapture()) {
            ReleaseCapture();
            xPos = GET_X_LPARAM(lParam);
            yPos = GET_Y_LPARAM(lParam);
            ClientRect rClient(hwnd);
            if (rClient.Contains(PointI(xPos, yPos)) && -1 == tab->IndexFromPoint(xPos, yPos)
                                                     && -1 == tab->ButtonFromPoint(xPos, yPos)) {
                // Cancel the mouse's tracking and show the menu.
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags = TME_CANCEL | TME_LEAVE;
                tme.hwndTrack = hwnd;
                if (TrackMouseEvent(&tme))
                    tab->isMouseInClientArea = false;
                POINT pt = {xPos, yPos};
                ClientToScreen(hwnd, &pt);
                MenuBarAsPopupMenu(GetParent(hwnd), pt.x, pt.y);
            }
        }
        return 0;

    case WM_ERASEBKGND:
        return TRUE;

    case WM_PAINT:
        RECT rc;
        GetUpdateRect(hwnd, &rc, FALSE);

        hdc = wParam ? (HDC)wParam : BeginPaint(hwnd, &ps);
        ValidateRect(hwnd, NULL);

        tab->EvaluateColors();
        tab->Paint(hdc, rc);

        if (!wParam) EndPaint(hwnd, &ps);
        return 0;
    }
    return CallWindowProc(DefWndProcTabBar, hwnd, msg, wParam, lParam);
}
#endif //OWN_TAB_DRAWING


void CreateTabbar(WindowInfo *win)
{
    win->hwndTabBar = CreateWindow(WC_TABCONTROL, L"", 
        WS_CHILD | WS_CLIPSIBLINGS /*| WS_VISIBLE*/ | 
        TCS_FOCUSNEVER | TCS_FIXEDWIDTH | TCS_FORCELABELLEFT, 
        0, 0, 0, 0, 
        win->hwndFrame, (HMENU)IDC_TABBAR, ghinst, NULL);

#ifdef OWN_TAB_DRAWING
    DefWndProcTabBar = (WNDPROC)SetWindowLongPtr(win->hwndTabBar, GWLP_WNDPROC, (LONG_PTR)WndProcTabBar);
    TabPainter *tp = new TabPainter(win->hwndTabBar, TAB_WIDTH, TAB_HEIGHT);
    SetWindowLongPtr(win->hwndTabBar, GWLP_USERDATA, (LONG_PTR)tp);
#endif //OWN_TAB_DRAWING

    SetWindowFont(win->hwndTabBar, gDefaultGuiFont, FALSE);
    TabCtrl_SetItemSize(win->hwndTabBar, TAB_WIDTH, TAB_HEIGHT);

    win->tabSelectionHistory = new Vec<TabData *>();
}


// Saves some of the document's data from the WindowInfo to the TabData.
void SaveTabData(WindowInfo *win, TabData *tdata)
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
    if (current != -1) {
        TCITEM tcs;
        tcs.mask = TCIF_PARAM;
        if (TabCtrl_GetItem(win->hwndTabBar, current, &tcs)) {
            // we use the lParam member of the TCITEM structure of the tab, to save the TabData pointer in
            PrepareAndSaveTabData(win, (TabData **)&tcs.lParam);
            TabCtrl_SetItem(win->hwndTabBar, current, &tcs);

            // update the selection history
            win->tabSelectionHistory->Remove((TabData *)tcs.lParam);
            win->tabSelectionHistory->Push((TabData *)tcs.lParam);
        }
    }
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


static int FindTabIndex(WindowInfo *win, TabData *tdata)
{
    int count = TabsGetCount(win);
    for (int i = 0; i < count; i++) {
        if (tdata == GetTabData(win, i))
            return i;
    }
    return -1;
}


void DeleteTabData(TabData *tdata, bool deleteModel)
{
    if (tdata) {
        if (deleteModel) {
            delete tdata->ctrl;
            FileWatcherUnsubscribe(tdata->watcher);
        }
        free(tdata->title);
        free(tdata->filePath);
        delete tdata;
    }
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
}


// Called when we're closing a document
void TabsOnCloseDoc(WindowInfo *win)
{
    int count = TabsGetCount(win);
    if (count <= 0)
        return;

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
        }
        break;

#ifdef OWN_TAB_DRAWING
    case T_CLOSING:
        // allow the closure
        return FALSE;

    case T_CLOSE:
        {
            int current = TabCtrl_GetCurSel(win->hwndTabBar);
            if (tab1 == current) {
                CloseWindow(win, false);
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
#endif //OWN_TAB_DRAWING
    }
    return TRUE;
}


static void ShowTabBar(WindowInfo *win, bool show)
{
    if (show == win->tabsVisible)
        return;
    win->tabsVisible = show;
    ShowWindow(win->hwndTabBar, show ? SW_SHOW : SW_HIDE);
    ClientRect rect(win->hwndFrame);
    SendMessage(win->hwndFrame, WM_SIZE, 0, MAKELONG(rect.dx, rect.dy));
}


void UpdateTabWidth(WindowInfo *win)
{
    int count = TabsGetCount(win);
    bool showSingleTab = gGlobalPrefs->useTabs && gGlobalPrefs->showSingleTab;
    if (count > (showSingleTab ? 0 : 1) || win->tabsInTitlebar) {
        ShowTabBar(win, true);
        ClientRect rect(win->hwndFrame);
        int buttonsWidth = win->tabsInTitlebar ? 3*TAB_HEIGHT : 0;
        int tabWidth = count ? (rect.dx - buttonsWidth - 3) / count : TAB_WIDTH;
        TabCtrl_SetItemSize(win->hwndTabBar, TAB_WIDTH < tabWidth ? TAB_WIDTH : tabWidth, TAB_HEIGHT);
    }
    else {
        ShowTabBar(win, false);
    }
}


void SetTabsInTitlebar(WindowInfo *win, bool set)
{
#ifdef OWN_TAB_DRAWING
    if (set == win->tabsInTitlebar)
        return;
    win->tabsInTitlebar = set;
    TabPainter *tab = (TabPainter *)GetWindowLongPtr(win->hwndTabBar, GWLP_USERDATA);
    tab->inTitlebar = set;
    UpdateTabWidth(win);
    SetWindowPos(win->hwndFrame, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE);
#endif //OWN_TAB_DRAWING
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


void SwapTabs(WindowInfo *win, int tab1, int tab2)
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


void MenuBarAsPopupMenu(HWND hwnd, int x, int y)
{
    HMENU menu = GetMenu(hwnd);
    int count = GetMenuItemCount(menu);
    if (count <= 0)
        return;
    HMENU popup = CreatePopupMenu();

    MENUITEMINFO mii;
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_SUBMENU | MIIM_STRING;
    for (int i = 0; i < count; i++) {
        mii.dwTypeData = NULL;
        GetMenuItemInfo(menu, i, TRUE, &mii);
        if (!mii.hSubMenu || !mii.cch)
            continue;
        mii.cch++;
        ScopedMem<WCHAR> subMenuName(AllocArray<WCHAR>(mii.cch));
        mii.dwTypeData = subMenuName;
        GetMenuItemInfo(menu, i, TRUE, &mii);
        AppendMenu(popup, MF_POPUP | MF_STRING, (UINT_PTR)mii.hSubMenu, subMenuName);
    }
    TrackPopupMenu(popup, TPM_LEFTALIGN, x, y, 0, hwnd, NULL);

    while (--count >= 0)
        RemoveMenu(popup, count, MF_BYPOSITION);
    DestroyMenu(popup);
}
