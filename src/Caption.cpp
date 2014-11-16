/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include <dwmapi.h>
#include <vssym32.h>
#include "BaseEngine.h"
#include "Caption.h"
#include "DisplayState.h"
#include "Controller.h"
#include "DisplayModel.h"
#include "AppPrefs.h"
#include "SumatraPDF.h"
#include "Tabs.h"
#include "Translations.h"
#include "WindowInfo.h"
#include "WinUtil.h"

using namespace Gdiplus;

#define CUSTOM_CAPTION_CLASS_NAME  L"CustomCaption"
#define UNDOCUMENTED_MENU_CLASS_NAME L"#32768"

#define BTN_ID_FIRST  100

// This will prevent menu reopening, if we click the menu button,
// while the menu is open.
#define DO_NOT_REOPEN_MENU_TIMER_ID       1
#define DO_NOT_REOPEN_MENU_DELAY_IN_MS    1

// undocumented caption buttons state
#define CBS_INACTIVE  5

// undocumented window messages
#define WM_NCUAHDRAWCAPTION  0xAE
#define WM_NCUAHDRAWFRAME    0xAF
#define WM_POPUPSYSTEMMENU   0x313

// When a top level window is maximized the window manager checks whether its client
// area covers the entire screen. If it does, the manager assumes that this is a fullscreen
// window and hides the taskbar and any topmost window. A simple workaround is to
// expand the non-client border at the expense of client area.
#define NON_CLIENT_BAND  1
// This non-client-band hack is only needed for maximized non-fullscreen windows:
static inline bool NeedsNonClientBandHack(HWND hwnd) { return IsZoomed(hwnd) && win::HasCaption(hwnd); }

// http://withinwindows.com/2010/07/01/retrieving-aero-glass-base-color-for-opaque-surface-rendering/
#define REG_DWM  L"Software\\Microsoft\\Windows\\DWM"

// When DWM composition is enabled, this is the ratio between alpha channels of active and inactive caption colors.
#define ACTIVE_INACTIVE_ALPHA_RATIO  2.0f

static void DrawCaptionButton(DRAWITEMSTRUCT *item, WindowInfo *win);
static void PaintCaptionBackground(HDC hdc, WindowInfo *win, bool useDoubleBuffer);
static HMENU GetUpdatedSystemMenu(HWND hwnd);
static void MenuBarAsPopupMenu(WindowInfo *win, int x, int y);

CaptionInfo::CaptionInfo(HWND hwndCaption): hwnd(hwndCaption), theme(NULL), isMenuOpen(false) {
    UpdateTheme();
    UpdateColors(true);
    UpdateBackgroundAlpha();
}

CaptionInfo::~CaptionInfo() {
    if (theme)
        vss::CloseThemeData(theme);
}

void CaptionInfo::UpdateBackgroundAlpha()
{
    bgAlpha = dwm::IsCompositionEnabled() ? 0 : 255;
}

void CaptionInfo::UpdateTheme()
{
    if (theme) {
        vss::CloseThemeData(theme);
        theme = NULL;
    }
    if (vss::IsThemeActive())
        theme = vss::OpenThemeData(hwnd, L"WINDOW");
}

void CaptionInfo::UpdateColors(bool activeWindow)
{
    ARGB colorizationColor;
    if (dwm::IsCompositionEnabled() &&
        // get the color from the Registry and blend it with white background
        ReadRegDWORD(HKEY_CURRENT_USER, REG_DWM, L"ColorizationColor", colorizationColor)) {
            BYTE A, R, G, B, white;
            A = BYTE((colorizationColor >> 24) & 0xff);
            if (!activeWindow)
                A = (BYTE)floor(A / ACTIVE_INACTIVE_ALPHA_RATIO + 0.5f);
            R = BYTE((colorizationColor >> 16) & 0xff);
            G = BYTE((colorizationColor >> 8) & 0xff);
            B = BYTE(colorizationColor & 0xff);
            white = BYTE(255 - A);
            float factor = A / 255.0f;
            R = BYTE((int)floor(R * factor + 0.5f) + white);
            G = BYTE((int)floor(G * factor + 0.5f) + white);
            B = BYTE((int)floor(B * factor + 0.5f) + white);
            bgColor = RGB(R, G, B);
    }
    else if (!theme || !SUCCEEDED(vss::GetThemeColor(theme, WP_CAPTION, 0,
        activeWindow ? TMT_FILLCOLORHINT : TMT_BORDERCOLORHINT, &bgColor))) {
            bgColor = activeWindow ? GetSysColor(COLOR_GRADIENTACTIVECAPTION)
                                   : GetSysColor(COLOR_GRADIENTINACTIVECAPTION);
    }
    if (!theme || !SUCCEEDED(vss::GetThemeColor(theme, WP_CAPTION, 0,
        (activeWindow || dwm::IsCompositionEnabled()) ? TMT_CAPTIONTEXT : TMT_INACTIVECAPTIONTEXT, &textColor))) {
            textColor = (activeWindow || dwm::IsCompositionEnabled()) ? GetSysColor(COLOR_CAPTIONTEXT)
                                                                      : GetSysColor(COLOR_INACTIVECAPTIONTEXT);
    }
}

ButtonInfo::ButtonInfo(): hwnd(NULL), highlighted(false), inactive(false) {
    SetMargins(0, 0, 0, 0);
}

void ButtonInfo::SetMargins(LONG left, LONG top, LONG right, LONG bottom)
{
    margins.left   = left;
    margins.top    = top;
    margins.right  = right;
    margins.bottom = bottom;
}

static LRESULT CALLBACK WndProcCaption(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);

    switch (message)
    {
    case WM_COMMAND:
        if (win && BN_CLICKED == HIWORD(wParam)) {
            WPARAM cmd;
            WORD button = LOWORD(wParam) - BTN_ID_FIRST;
            switch (button)
            {
                case CB_MINIMIZE:  cmd = SC_MINIMIZE; break;
                case CB_MAXIMIZE:  cmd = SC_MAXIMIZE; break;
                case CB_RESTORE:   cmd = SC_RESTORE;  break;
                case CB_CLOSE:     cmd = SC_CLOSE;    break;
                default:           cmd = 0;           break;
            }
            if (cmd)
                PostMessage(win->hwndFrame, WM_SYSCOMMAND, cmd, 0);

            if (button == CB_MENU) {
                if (!KillTimer(hwnd, DO_NOT_REOPEN_MENU_TIMER_ID) && !win->caption->isMenuOpen) {
                    HWND hMenuButton = win->caption->btn[CB_MENU].hwnd;
                    WindowRect wr(hMenuButton);
                    win->caption->isMenuOpen = true;
                    if (!lParam)    // if the WM_COMMAND message was sent as a result of keyboard command
                        InvalidateRgn(hMenuButton, NULL, FALSE);
                    MenuBarAsPopupMenu(win, wr.x, wr.y + wr.dy);
                    win->caption->isMenuOpen = false;
                    if (!lParam)
                        InvalidateRgn(hMenuButton, NULL, FALSE);
                    SetTimer(hwnd, DO_NOT_REOPEN_MENU_TIMER_ID, DO_NOT_REOPEN_MENU_DELAY_IN_MS, NULL);
                }
                SetFocus(win->hwndFrame);
            }
        }
        break;

    case WM_TIMER:
        if (wParam == DO_NOT_REOPEN_MENU_TIMER_ID)
            KillTimer(hwnd, DO_NOT_REOPEN_MENU_TIMER_ID);
        break;

    case WM_SIZE:
        if (win)
            RelayoutCaption(win);
        break;

    case WM_NCHITTEST:
        return HTTRANSPARENT;

    case WM_ERASEBKGND:
        if (win)
            PaintCaptionBackground((HDC)wParam, win, true);
        return TRUE;

    case WM_DRAWITEM:
        if (win) {
            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
            int index = dis->CtlID - BTN_ID_FIRST;
            if (CB_MENU == index && win->caption->isMenuOpen)
                dis->itemState |= ODS_SELECTED;
            if (win->caption->btn[index].highlighted)
                dis->itemState |= ODS_HOTLIGHT;
            else if (win->caption->btn[index].inactive)
                dis->itemState |= ODS_INACTIVE;
            DrawCaptionButton(dis, win);
        }
        return TRUE;

    case WM_THEMECHANGED:
        if (win)
            win->caption->UpdateTheme();
        break;

    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

static WNDPROC DefWndProcButton = NULL;
static LRESULT CALLBACK WndProcButton(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    int index = (int)GetWindowLongPtr(hwnd, GWLP_ID) - BTN_ID_FIRST;

    switch (message)
    {
        case WM_MOUSEMOVE:
            {
                ClientRect rc(hwnd);
                if (!rc.Contains(PointI(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))) {
                    ReleaseCapture();
                    return 0;
                }
                if (win) {
                    TRACKMOUSEEVENT tme;
                    tme.cbSize = sizeof(TRACKMOUSEEVENT);
                    tme.dwFlags = TME_QUERY;
                    tme.hwndTrack = hwnd;
                    TrackMouseEvent(&tme);
                    if (0 == (tme.dwFlags & TME_LEAVE)) {
                        tme.dwFlags = TME_LEAVE;
                        tme.hwndTrack = hwnd;
                        TrackMouseEvent(&tme);
                        win->caption->btn[index].highlighted = true;
                        InvalidateRgn(hwnd, NULL, FALSE);
                    }
                    return 0;
                }
            }
            break;

        case WM_MOUSELEAVE:
            if (win) {
                win->caption->btn[index].highlighted = false;
                InvalidateRgn(hwnd, NULL, FALSE);
                return 0;
            }
            break;

        case WM_ERASEBKGND:
            return TRUE;

        case WM_LBUTTONDOWN:
            if (CB_MENU == index)
                PostMessage(hwnd, WM_LBUTTONUP, 0, lParam);
            return CallWindowProc(DefWndProcButton, hwnd, message, wParam, lParam);

        case WM_KEYDOWN:
            if (CB_MENU == index && win && !win->caption->isMenuOpen &&
                (VK_RETURN == wParam || VK_SPACE == wParam || VK_UP == wParam || VK_DOWN == wParam))
                PostMessage(hwnd, BM_CLICK, 0, 0);
            return CallWindowProc(DefWndProcButton, hwnd, message, wParam, lParam);
    }
    return CallWindowProc(DefWndProcButton, hwnd, message, wParam, lParam);
}

void CreateCaption(WindowInfo *win)
{
    win->hwndCaption = CreateWindow(CUSTOM_CAPTION_CLASS_NAME, L"", WS_CHILDWINDOW | WS_CLIPCHILDREN,
        0, 0, 0, 0, win->hwndFrame, (HMENU)0, GetModuleHandle(NULL), NULL);

    win->caption = new CaptionInfo(win->hwndCaption);

    for (int i = CB_BTN_FIRST; i < CB_BTN_COUNT; i++) {
        HWND btn = CreateWindow(L"BUTTON", L"", WS_CHILDWINDOW | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 0, 0, win->hwndCaption, (HMENU)(BTN_ID_FIRST + i), GetModuleHandle(NULL), NULL);

        if (!DefWndProcButton)
            DefWndProcButton = (WNDPROC)GetWindowLongPtr(btn, GWLP_WNDPROC);
        SetWindowLongPtr(btn, GWLP_WNDPROC, (LONG_PTR)WndProcButton);

        win->caption->btn[i].hwnd = btn;
    }
}

void RegisterCaptionWndClass()
{
    WNDCLASSEX wcex;
    FillWndClassEx(wcex, CUSTOM_CAPTION_CLASS_NAME, WndProcCaption);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassEx(&wcex);
}

void RelayoutCaption(WindowInfo *win)
{
    ClientRect rc(win->hwndCaption);
    CaptionInfo *ci = win->caption;
    ButtonInfo *button;
    DeferWinPosHelper dh;

    if (dwm::IsCompositionEnabled()) {
        // hide the buttons, because DWM paints and serves them, when the composition is enabled
        for (int i = CB_MINIMIZE; i <= CB_CLOSE; i++)
            ShowWindow(ci->btn[i].hwnd, SW_HIDE);
    }
    else {
        int xEdge = GetSystemMetrics(SM_CXEDGE);
        int yEdge = GetSystemMetrics(SM_CYEDGE);
        bool isClassicStyle = win->caption->theme == NULL;
        // Under WIN XP GetSystemMetrics(SM_CXSIZE) returns wrong (previous) value, after theme change
        // or font size change. For this to work, I assume that SM_CXSIZE == SM_CYSIZE.
        int btnDx = GetSystemMetrics(IsVistaOrGreater() ? SM_CXSIZE : SM_CYSIZE) - xEdge * (isClassicStyle ? 1 : 2);
        int btnDy = GetSystemMetrics(SM_CYSIZE) - yEdge * 2;
        bool maximized = IsZoomed(win->hwndFrame);
        int yPosBtn = rc.y + (maximized ? 0 : yEdge);
        int topMargin = maximized ? yEdge : 0;

        button = &ci->btn[CB_CLOSE];
        rc.dx -= btnDx + xEdge;
        int rightMargin = maximized ? xEdge : 0;
        dh.SetWindowPos(button->hwnd, NULL, rc.x + rc.dx, yPosBtn, btnDx + rightMargin, btnDy + topMargin, SWP_NOZORDER | SWP_SHOWWINDOW);
        button->SetMargins(0, topMargin, rightMargin, 0);

        button = &ci->btn[CB_RESTORE];
        rc.dx -= btnDx + xEdge;
        dh.SetWindowPos(button->hwnd, NULL, rc.x + rc.dx, yPosBtn, btnDx, btnDy + topMargin,
                        SWP_NOZORDER | (maximized ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
        button->SetMargins(0, topMargin, 0, 0);

        button = &ci->btn[CB_MAXIMIZE];
        dh.SetWindowPos(button->hwnd, NULL, rc.x + rc.dx, yPosBtn, btnDx, btnDy + topMargin,
                        SWP_NOZORDER | (maximized ? SWP_HIDEWINDOW : SWP_SHOWWINDOW));
        button->SetMargins(0, topMargin, 0, 0);

        button = &ci->btn[CB_MINIMIZE];
        rc.dx -= btnDx + (isClassicStyle ? 0 : xEdge);
        dh.SetWindowPos(button->hwnd, NULL, rc.x + rc.dx, yPosBtn, btnDx, btnDy + topMargin, SWP_NOZORDER | SWP_SHOWWINDOW);
        button->SetMargins(0, topMargin, 0, 0);
    }

    button = &ci->btn[CB_MENU];
    int tabHeight = GetTabbarHeight(win);
    rc.y += rc.dy - tabHeight;
    dh.SetWindowPos(button->hwnd, NULL, rc.x, rc.y, tabHeight, tabHeight, SWP_NOZORDER);
    button->SetMargins(0, 0, 0, 0);

    rc.x += tabHeight;
    rc.dx -= tabHeight;
    dh.SetWindowPos(win->hwndTabBar, NULL, rc.x, rc.y, rc.dx, tabHeight, SWP_NOZORDER);
    dh.End();
}

static void DrawCaptionButton(DRAWITEMSTRUCT *item, WindowInfo *win)
{
    if (!item || item->CtlType != ODT_BUTTON)
        return;

    RectI rButton = RectI::FromRECT(item->rcItem);

    DoubleBuffer buffer(item->hwndItem, rButton);
    HDC memDC = buffer.GetDC();

    UINT button = item->CtlID - BTN_ID_FIRST;
    ButtonInfo *bi = &win->caption->btn[button];
    RectI rc(rButton);
    rc.x += bi->margins.left;
    rc.y += bi->margins.top;
    rc.dx -= bi->margins.left + bi->margins.right;
    rc.dy -= bi->margins.top + bi->margins.bottom;

    int partId = 0, stateId;
    UINT state = (UINT)-1;
    switch (button)
    {
        case CB_MINIMIZE:
            partId = WP_MINBUTTON;
            state = DFCS_CAPTIONMIN;
            break;
        case CB_MAXIMIZE:
            partId = WP_MAXBUTTON;
            state = DFCS_CAPTIONMAX;
            break;
        case CB_RESTORE:
            partId = WP_RESTOREBUTTON;
            state = DFCS_CAPTIONRESTORE;
            break;
        case CB_CLOSE:
            partId = WP_CLOSEBUTTON;
            state = DFCS_CAPTIONCLOSE;
            break;
    }

    if (ODS_SELECTED & item->itemState) {
        stateId = CBS_PUSHED;
        state |= DFCS_PUSHED;
    }
    else if (ODS_HOTLIGHT & item->itemState) {
        stateId = CBS_HOT;
        state |= DFCS_HOT;
    }
    else if (ODS_DISABLED & item->itemState) {
        stateId = CBS_DISABLED;
        state |= DFCS_INACTIVE;
    }
    else if (ODS_INACTIVE & item->itemState)
        stateId = CBS_INACTIVE;
    else
        stateId = CBS_NORMAL;

    // draw system button
    if (partId) {
        if (rc != rButton || vss::IsThemeBackgroundPartiallyTransparent(win->caption->theme, partId, stateId))
            PaintCaptionBackground(memDC, win, false);

        RECT r = rc.ToRECT();
        if (win->caption->theme)
            vss::DrawThemeBackground(win->caption->theme, memDC, partId, stateId, &r, NULL);
        else
            DrawFrameControl(memDC, &r, DFC_CAPTION, state);
    }

    // draw menu's button
    if (button == CB_MENU) {
        PaintCaptionBackground(memDC, win, false);
        Graphics graphics(memDC);

        if (CBS_PUSHED != stateId && ODS_FOCUS & item->itemState)
            stateId = CBS_HOT;

        BYTE buttonRGB = 1;
        if (CBS_PUSHED == stateId)
            buttonRGB = 0;
        else if (CBS_HOT == stateId)
            buttonRGB = 255;

        if (buttonRGB != 1) {
            // paint the background
            if (GetLightness(win->caption->textColor) > GetLightness(win->caption->bgColor))
                buttonRGB ^= 0xff;
            BYTE buttonAlpha = BYTE((255 - abs((int)GetLightness(win->caption->bgColor) - buttonRGB)) / 2);
            SolidBrush br(Color(buttonAlpha, buttonRGB, buttonRGB, buttonRGB));
            graphics.FillRectangle(&br, rc.x, rc.y, rc.dx, rc.dy);
        }
        // draw the three lines
        COLORREF c = win->caption->textColor;
        Pen p(Color(GetRValueSafe(c), GetGValueSafe(c), GetBValueSafe(c)), floor((float)rc.dy / 8.0f));
        rc.Inflate(-int(rc.dx * 0.2f + 0.5f), -int(rc.dy * 0.3f + 0.5f));
        for (int i = 0; i < 3; i++) {
            graphics.DrawLine(&p, rc.x, rc.y + i * rc.dy / 2, rc.x + rc.dx, rc.y + i * rc.dy / 2);
        }
    }

    buffer.Flush(item->hDC);
}

void PaintParentBackground(HWND hwnd, HDC hdc)
{
    HWND parent = GetParent(hwnd);
    POINT pt = {0, 0};
    MapWindowPoints(hwnd, parent, &pt, 1);
    SetViewportOrgEx(hdc, -pt.x, -pt.y, &pt);
    SendMessage(parent, WM_ERASEBKGND, (WPARAM)hdc, 0);
    SetViewportOrgEx(hdc, pt.x, pt.y, NULL);
}

static void PaintCaptionBackground(HDC hdc, WindowInfo *win, bool useDoubleBuffer)
{
    RECT rClip;
    GetClipBox(hdc, &rClip);
    RectI r = RectI::FromRECT(rClip);

    COLORREF c = win->caption->bgColor;

    if (win->caption->bgAlpha == 0) {
        PaintParentBackground(win->hwndCaption, hdc);
    }
    else if (win->caption->bgAlpha == 255) {
        Graphics graphics(hdc);
        SolidBrush br(Color(GetRValueSafe(c), GetGValueSafe(c), GetBValueSafe(c)));
        graphics.FillRectangle(&br, r.x, r.y, r.dx, r.dy);
    }
    else {
        DoubleBuffer buffer(win->hwndCaption, r);
        HDC memDC = useDoubleBuffer ? buffer.GetDC() : hdc;
        PaintParentBackground(win->hwndCaption, memDC);
        Graphics graphics(memDC);
        SolidBrush br(Color(win->caption->bgAlpha, GetRValueSafe(c), GetGValueSafe(c), GetBValueSafe(c)));
        graphics.FillRectangle(&br, r.x, r.y, r.dx, r.dy);
        if (useDoubleBuffer)
            buffer.Flush(hdc);
    }
}

static void DrawFrame(HWND hwnd, COLORREF color, bool drawEdge=true)
{
    HDC hdc = GetWindowDC(hwnd);

    RECT rWindow, rClient;
    GetWindowRect(hwnd, &rWindow);
    GetClientRect(hwnd, &rClient);
    // convert the client rectangle to window coordinates and exclude it from the clipping region
    MapWindowPoints(hwnd, HWND_DESKTOP, (LPPOINT)&rClient, 2);
    OffsetRect(&rClient, -rWindow.left, -rWindow.top);
    ExcludeClipRect(hdc, rClient.left, rClient.top, rClient.right, rClient.bottom);
    // convert the window rectangle, from screen to window coordinates, and draw the frame
    OffsetRect(&rWindow, -rWindow.left, -rWindow.top);
    HBRUSH br = CreateSolidBrush(color);
    FillRect(hdc, &rWindow, br);
    DeleteObject(br);
    if (drawEdge)
        DrawEdge(hdc, &rWindow, EDGE_RAISED, BF_RECT | BF_FLAT);

    ReleaseDC(hwnd, hdc);
}

// accelerator key which was pressed when invoking the "menubar",
// needs to be passed from WM_SYSCOMMAND to WM_INITMENUPOPUP
// (can be static because there can only be one menu active at a time)
static WCHAR gMenuAccelPressed = 0;

LRESULT CustomCaptionFrameProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, bool *callDef, WindowInfo *win)
{
    if (dwm::IsCompositionEnabled()) {
        // Pass the messages to DwmDefWindowProc first. It serves the hit testing for the buttons.
        LRESULT res;
        if (dwm::DefWindowProc_(hwnd, msg, wParam, lParam, &res)) {
            *callDef = false;
            return res;
        }

        switch (msg)
        {
        case WM_NCPAINT:
#if NON_CLIENT_BAND > 0
            if (NeedsNonClientBandHack(hwnd))
                DrawFrame(hwnd, RGB(0,0,0), false);
#endif
            break;

        case WM_ERASEBKGND:
            {
                // Erase the background only under the extended frame.
                *callDef = false;
                if (win->extendedFrameHeight == 0)
                    return TRUE;
                ClientRect rc(hwnd);
                rc.dy = win->extendedFrameHeight;
                HRGN extendedFrameRegion = CreateRectRgn(rc.x, rc.y, rc.x + rc.dx, rc.y + rc.dy);
                int newRegionComplexity = ExtSelectClipRgn((HDC)wParam, extendedFrameRegion, RGN_AND);
                DeleteObject(extendedFrameRegion);
                if (newRegionComplexity == NULLREGION)
                    return TRUE;
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);

        case WM_SIZE:
            // Extend the translucent frame in the client area.
            if (wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED) {
                int frameThickness = 0;
                if (win::HasFrameThickness(hwnd)) {
                    frameThickness = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                }
                int captionHeight = 0;
                if (win::HasCaption(hwnd)) {
                    float tabScale = CAPTION_TABBAR_HEIGHT_FACTOR;
                    if (IsZoomed(hwnd)) {
                        tabScale = 1.f;
                    }
                    captionHeight = GetTabbarHeight(win, tabScale);
                }
                int top = frameThickness + captionHeight;
                int bottom = NeedsNonClientBandHack(hwnd) ? NON_CLIENT_BAND : 0;
                MARGINS margins = { 0, 0, top, bottom };
                dwm::ExtendFrameIntoClientArea(hwnd, &margins);
                win->extendedFrameHeight = frameThickness + captionHeight;
            }
            break;

        case WM_NCACTIVATE:
            win->caption->UpdateColors((bool)wParam);
            if (!IsIconic(hwnd))
                RedrawWindow(win->hwndCaption, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
            break;
        }
    }
    else {
        switch (msg)
        {
        case WM_SETTINGCHANGE:
            if (wParam == SPI_SETNONCLIENTMETRICS)
                RelayoutCaption(win);
            break;

        case WM_NCPAINT:
            DrawFrame(hwnd, win->caption->bgColor);
            *callDef = false;
            return 0;

        case WM_NCACTIVATE:
            win->caption->UpdateColors((bool)wParam);
            for (int i = CB_BTN_FIRST; i < CB_BTN_COUNT; i++)
                win->caption->btn[i].inactive = wParam == FALSE;
            if (!IsIconic(hwnd)) {
                DrawFrame(hwnd, win->caption->bgColor);
                RedrawWindow(win->hwndCaption, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
                *callDef = false;
                return TRUE;
            }
            break;

        case WM_NCUAHDRAWCAPTION:
        case WM_NCUAHDRAWFRAME:
            DrawFrame(hwnd, win->caption->bgColor);
            *callDef = false;
            return TRUE;

        case WM_POPUPSYSTEMMENU:
        case WM_SETCURSOR:
        case WM_SETTEXT:
        case WM_SETICON:
            if (!win->caption->theme && IsWindowVisible(hwnd)) {
                // Remove the WS_VISIBLE style to prevent DefWindowProc from drawing
                // in the caption's area when processing these mesages.
                // TODO: can't such drawing be prevented by handling WM_(NC)PAINT instead?
                ToggleWindowStyle(hwnd, WS_VISIBLE, false);
                LRESULT res = DefWindowProc(hwnd, msg, wParam, lParam);
                ToggleWindowStyle(hwnd, WS_VISIBLE, true);
                *callDef = false;
                return res;
            }
            break;
        }
    }

    // These messages must be handled in both modes - with or without DWM composition.
    switch (msg)
    {
    case WM_NCCALCSIZE:
        {
            // In order to have custom caption, we have to include its area in the client rectangle.
            RECT *r = wParam == TRUE ? &((NCCALCSIZE_PARAMS *)lParam)->rgrc[0] : (RECT *)lParam;
            RECT rWindow = *r;
            // Let DefWindowProc calculate the client rectangle.
            DefWindowProc(hwnd, msg, wParam, lParam);
            RECT rClient = *r;
            // Modify the client rectangle to include the caption's area.
            if (dwm::IsCompositionEnabled())
                rClient.top = rWindow.top;
            else
                rClient.top = rWindow.top + rWindow.bottom - rClient.bottom;
            // prevents the hiding of the topmost windows, when this window is maximized
            if (NeedsNonClientBandHack(hwnd))
                rClient.bottom -= NON_CLIENT_BAND;
            *r = rClient;
            *callDef = false;
        }
        return 0;

    case WM_NCHITTEST:
        {
            // Provide hit testing for the caption.
            PointI pt(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            RectI rClient = MapRectToWindow(ClientRect(hwnd), hwnd, HWND_DESKTOP);
            WindowRect rCaption(win->hwndCaption);
            if (rClient.Contains(pt) && pt.y < rCaption.y + rCaption.dy) {
                *callDef = false;
                if (pt.y < rCaption.y)
                    return HTTOP;
                return HTCAPTION;
            }
        }
        break;

    case WM_NCRBUTTONUP:
        // Prepare and show the system menu.
        if (wParam == HTCAPTION) {
            HMENU menu = GetUpdatedSystemMenu(hwnd);
            UINT flags = TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD;
            if (GetSystemMetrics(SM_MENUDROPALIGNMENT))
                flags |= TPM_RIGHTALIGN;
            WPARAM cmd = TrackPopupMenu(menu, flags, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0, hwnd, NULL);
            if (cmd)
                PostMessage(hwnd, WM_SYSCOMMAND, cmd, 0);
            *callDef = false;
            return 0;
        }
        break;

    case WM_SYSCOMMAND:
        if (wParam == SC_KEYMENU) {
            // Show the "menu bar" (and the desired submenu)
            gMenuAccelPressed = (WCHAR)lParam;
            if (' ' == gMenuAccelPressed) {
                // map space to the accelerator of the Window menu
                if (str::FindChar(_TR("&Window"), '&'))
                    gMenuAccelPressed = *(str::FindChar(_TR("&Window"), '&') + 1);
            }
            PostMessage(win->hwndCaption, WM_COMMAND, MAKELONG(BTN_ID_FIRST + CB_MENU, BN_CLICKED), 0);
            *callDef = false;
            return 0;
        }
        break;

    case WM_INITMENUPOPUP:
        if (gMenuAccelPressed) {
            // poorly documented hack: find the menu window and send it the accelerator key
            HWND hMenu = FindWindow(UNDOCUMENTED_MENU_CLASS_NAME, NULL);
            if (hMenu) {
                if ('a' <= gMenuAccelPressed && gMenuAccelPressed <= 'z')
                    gMenuAccelPressed -= 'a' - 'A';
                if ('A' <= gMenuAccelPressed && gMenuAccelPressed <= 'Z')
                    PostMessage(hMenu, WM_KEYDOWN, gMenuAccelPressed, 0);
                else
                    PostMessage(hMenu, WM_CHAR, gMenuAccelPressed, 0);
            }
            gMenuAccelPressed = 0;
        }
        break;

    case WM_SYSCOLORCHANGE:
        win->caption->UpdateColors(hwnd == GetForegroundWindow());
        break;

    case WM_DWMCOLORIZATIONCOLORCHANGED:
        win->caption->UpdateColors(hwnd == GetForegroundWindow());
        if (!IsIconic(hwnd))
            RedrawWindow(win->hwndCaption, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        break;

    case WM_DWMCOMPOSITIONCHANGED:
        win->caption->UpdateBackgroundAlpha();
        ClientRect cr(hwnd);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE);
        if (ClientRect(hwnd) == cr)
            SendMessage(hwnd, WM_SIZE, 0, MAKELONG(cr.dx, cr.dy));
        *callDef = false;
        return 0;
    }

    *callDef = true;
    return 0;
}

static HMENU GetUpdatedSystemMenu(HWND hwnd)
{
    // don't reset the system menu (in case other applications have added to it)
    HMENU menu = GetSystemMenu(hwnd, FALSE);

    // prevents drawing in the caption's area
    // TODO: how can this even happen?
    ToggleWindowStyle(hwnd, WS_VISIBLE, false);

    bool maximized = IsZoomed(hwnd);
    EnableMenuItem(menu, SC_SIZE, maximized ? MF_GRAYED : MF_ENABLED);
    EnableMenuItem(menu, SC_MOVE, maximized ? MF_GRAYED : MF_ENABLED);
    EnableMenuItem(menu, SC_MINIMIZE, MF_ENABLED);
    EnableMenuItem(menu, SC_MAXIMIZE, maximized ? MF_GRAYED : MF_ENABLED);
    EnableMenuItem(menu, SC_CLOSE, MF_ENABLED);
    EnableMenuItem(menu, SC_RESTORE, maximized ? MF_ENABLED : MF_GRAYED);
    SetMenuDefaultItem(menu, maximized ? SC_RESTORE : SC_MAXIMIZE, FALSE);

    ToggleWindowStyle(hwnd, WS_VISIBLE, true);

    return menu;
}

static void MenuBarAsPopupMenu(WindowInfo *win, int x, int y)
{
    int count = GetMenuItemCount(win->menu);
    if (count <= 0)
        return;
    HMENU popup = CreatePopupMenu();

    MENUITEMINFO mii;
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_SUBMENU | MIIM_STRING;
    for (int i = 0; i < count; i++) {
        mii.dwTypeData = NULL;
        GetMenuItemInfo(win->menu, i, TRUE, &mii);
        if (!mii.hSubMenu || !mii.cch)
            continue;
        mii.cch++;
        ScopedMem<WCHAR> subMenuName(AllocArray<WCHAR>(mii.cch));
        mii.dwTypeData = subMenuName;
        GetMenuItemInfo(win->menu, i, TRUE, &mii);
        AppendMenu(popup, MF_POPUP | MF_STRING, (UINT_PTR)mii.hSubMenu, subMenuName);
    }
    AppendMenu(popup, MF_POPUP | MF_STRING, (UINT_PTR)GetUpdatedSystemMenu(win->hwndFrame), _TR("&Window"));
    count++;

    if (IsUIRightToLeft())
        x += ClientRect(win->caption->btn[CB_MENU].hwnd).dx;
    TrackPopupMenu(popup, TPM_LEFTALIGN, x, y, 0, win->hwndFrame, NULL);

    while (--count >= 0)
        RemoveMenu(popup, count, MF_BYPOSITION);
    DestroyMenu(popup);
}

typedef HTHEME (WINAPI *OpenThemeDataProc)(HWND hwnd, LPCWSTR pszClassList);
typedef HRESULT (WINAPI *CloseThemeDataProc)(HTHEME hTheme);
typedef HRESULT (WINAPI *DrawThemeBackgroundProc)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, LPCRECT pClipRect);
typedef BOOL (WINAPI *IsThemeActiveProc)(VOID);
typedef BOOL (WINAPI *IsThemeBackgroundPartiallyTransparentProc)(HTHEME hTheme, int iPartId, int iStateId);
typedef HRESULT (WINAPI *GetThemeColorProc)(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF *pColor);

namespace vss {

static bool gFuncsLoaded = false;
static OpenThemeDataProc _OpenThemeData = NULL;
static CloseThemeDataProc _CloseThemeData = NULL;
static DrawThemeBackgroundProc _DrawThemeBackground = NULL;
static IsThemeActiveProc _IsThemeActive = NULL;
static IsThemeBackgroundPartiallyTransparentProc _IsThemeBackgroundPartiallyTransparent = NULL;
static GetThemeColorProc _GetThemeColor = NULL;

void Initialize()
{
    if (gFuncsLoaded)
        return;

    HMODULE h = SafeLoadLibrary(L"UxTheme.dll");
#define Load(func) _ ## func = (func ## Proc)GetProcAddress(h, #func)
    Load(OpenThemeData);
    Load(CloseThemeData);
    Load(DrawThemeBackground);
    Load(IsThemeActive);
    Load(IsThemeBackgroundPartiallyTransparent);
    Load(GetThemeColor);
#undef Load

    gFuncsLoaded = true;
}

HTHEME OpenThemeData(HWND hwnd, LPCWSTR pszClassList)
{
    Initialize();
    if (!_OpenThemeData)
        return NULL;
    return _OpenThemeData(hwnd, pszClassList);
}

HRESULT CloseThemeData(HTHEME hTheme)
{
    Initialize();
    if (!_CloseThemeData)
        return E_NOTIMPL;
    return _CloseThemeData(hTheme);
}

HRESULT DrawThemeBackground(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, LPCRECT pClipRect)
{
    Initialize();
    if (!_DrawThemeBackground)
        return E_NOTIMPL;
    return _DrawThemeBackground(hTheme, hdc, iPartId, iStateId, pRect, pClipRect);
}

BOOL IsThemeActive()
{
    Initialize();
    if (!_IsThemeActive)
        return FALSE;
    return _IsThemeActive();
}

BOOL IsThemeBackgroundPartiallyTransparent(HTHEME hTheme, int iPartId, int iStateId)
{
    Initialize();
    if (!_IsThemeBackgroundPartiallyTransparent)
        return FALSE;
    return _IsThemeBackgroundPartiallyTransparent(hTheme, iPartId, iStateId);
}

HRESULT GetThemeColor(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF *pColor)
{
    Initialize();
    if (!_GetThemeColor)
        return E_NOTIMPL;
    return _GetThemeColor(hTheme, iPartId, iStateId, iPropId, pColor);
}

};

typedef HRESULT (WINAPI *DwmIsCompositionEnabledProc)(BOOL *pfEnabled);
typedef HRESULT (WINAPI *DwmExtendFrameIntoClientAreaProc)(HWND hwnd, const MARGINS *pMarInset);
typedef BOOL (WINAPI *DwmDefWindowProcProc)(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *plResult);
typedef HRESULT (WINAPI *DwmGetWindowAttributeProc)(HWND hwnd, DWORD dwAttribute, void *pvAttribute, DWORD cbAttribute);

namespace dwm {

static bool gFuncsLoaded = false;
static DwmIsCompositionEnabledProc _DwmIsCompositionEnabled = NULL;
static DwmExtendFrameIntoClientAreaProc _DwmExtendFrameIntoClientArea = NULL;
static DwmDefWindowProcProc _DwmDefWindowProc = NULL;
static DwmGetWindowAttributeProc _DwmGetWindowAttribute = NULL;

void Initialize()
{
    if (gFuncsLoaded)
        return;

    HMODULE h = SafeLoadLibrary(L"Dwmapi.dll");
#define Load(func) _ ## func = (func ## Proc)GetProcAddress(h, #func)
    Load(DwmIsCompositionEnabled);
    Load(DwmExtendFrameIntoClientArea);
    Load(DwmDefWindowProc);
    Load(DwmGetWindowAttribute);
#undef Load

    gFuncsLoaded = true;
}

BOOL IsCompositionEnabled()
{
    Initialize();
    if (!_DwmIsCompositionEnabled)
        return FALSE;
    BOOL isEnabled;
    if (SUCCEEDED(_DwmIsCompositionEnabled(&isEnabled)))
        return isEnabled;
    return FALSE;
}

HRESULT ExtendFrameIntoClientArea(HWND hwnd, const MARGINS *pMarInset)
{
    Initialize();
    if (!_DwmExtendFrameIntoClientArea)
        return E_NOTIMPL;
    return _DwmExtendFrameIntoClientArea(hwnd, pMarInset);
}

BOOL DefWindowProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *plResult)
{
    Initialize();
    if (!_DwmDefWindowProc)
        return FALSE;
    return _DwmDefWindowProc(hwnd, msg, wParam, lParam, plResult);
}

HRESULT GetWindowAttribute(HWND hwnd, DWORD dwAttribute, void *pvAttribute, DWORD cbAttribute)
{
    Initialize();
    if (!_DwmGetWindowAttribute)
        return E_NOTIMPL;
    return _DwmGetWindowAttribute(hwnd, dwAttribute, pvAttribute, cbAttribute);
}

};
