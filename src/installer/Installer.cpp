/* TODO: those should be set from the makefile */
// Modify the following defines if you have to target a platform prior to the ones specified below.
// Their meaning: http://msdn.microsoft.com/en-us/library/aa383745(VS.85).aspx
// and http://blogs.msdn.com/oldnewthing/archive/2007/04/11/2079137.aspx
// We set the features uniformly to Win 2000 or later.
#ifndef WINVER
#define WINVER 0x0500
#endif

#ifndef _WIN32_WINNT 
#define _WIN32_WINNT 0x0500
// the following is only defined for _WIN32_WINNT >= 0x0600
#define USER_DEFAULT_SCREEN_DPI 96
#endif

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0500
#endif

// Allow use of features specific to IE 6.0 or later.
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <shlobj.h>
#include <gdiplus.h>

#include "Resource.h"
#include "base_util.h"
#include "win_util.h"

using namespace Gdiplus;

// this sucks but I don't know a better way
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define INSATLLER_FRAME_CLASS_NAME  _T("SUMATRA_PDF_INSTALLER_FRAME")
#define ID_BUTTON_INSTALL 1
#define ABOUT_BG_COLOR          RGB(255,242,0)

static HINSTANCE        ghinst;
static HWND             gHwndFrame;
static HWND             gHwndButtonInstall;
static HFONT            gFontDefault;

static ULONG_PTR        gGdiplusToken;

static NONCLIENTMETRICS gNonClientMetrics = { sizeof (NONCLIENTMETRICS) };

int gBallX, gBallY;

inline void SetFont(HWND hwnd, HFONT font)
{
    ::SendMessage(hwnd, WM_SETFONT, WPARAM(font), TRUE);
}

inline int RectDx(RECT *r)
{
    return r->right - r->left;
}

inline int RectDy(RECT *r)
{
    return r->bottom - r->top;
}

#if 0
void ResizeClientArea(HWND hwnd, int dx, int dy)
{
    RECT rwin, rcln;
    ::GetClientRect(hwnd, &rwin);
}
#endif

static HFONT CreateDefaultGuiFont()
{
    if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &gNonClientMetrics, 0))
    {
        // fonts: lfMenuFont, lfStatusFont, lfMessageFont, lfCaptionFont
        return ::CreateFontIndirect(&gNonClientMetrics.lfMessageFont);
    }
    return NULL;
}

void DrawMain(HWND hwnd, HDC hdc, RECT *rect)
{
    HBRUSH brushBg = ::CreateSolidBrush(ABOUT_BG_COLOR);

/*
    HPEN penBorder = CreatePen(PS_SOLID, ABOUT_LINE_OUTER_SIZE, WIN_COL_BLACK);
    HPEN penDivideLine = CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, WIN_COL_BLACK);
    HPEN penLinkLine = CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, COL_BLUE_LINK);

    HFONT fontSumatraTxt = Win32_Font_GetSimple(hdc, SUMATRA_TXT_FONT, SUMATRA_TXT_FONT_SIZE);
    HFONT fontVersionTxt = Win32_Font_GetSimple(hdc, VERSION_TXT_FONT, VERSION_TXT_FONT_SIZE);
    HFONT fontLeftTxt = Win32_Font_GetSimple(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE);
    HFONT fontRightTxt = Win32_Font_GetSimple(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE);

    HGDIOBJ origFont = SelectObject(hdc, fontSumatraTxt);
    */

    ::SetBkMode(hdc, TRANSPARENT);

    RECT rc;
    ::GetClientRect(hwnd, &rc);
    rc.bottom -= 48;
    ::FillRect(hdc, &rc, brushBg);

    Rect ellipseRect(gBallX-5, gBallY-5, 10, 10);
    Graphics graphics(hdc);
    graphics.SetCompositingQuality(CompositingQualityHighQuality);
    graphics.SetSmoothingMode(SmoothingModeHighQuality);
    SolidBrush blackBrush(Color(255, 0, 0, 0));
    graphics.FillEllipse(&blackBrush, ellipseRect);

    ::DeleteObject(brushBg);
}

void OnPaintMain(HWND hwnd)
{
    PAINTSTRUCT ps;
    RECT rc;
    HDC hdc = ::BeginPaint(hwnd, &ps);
    DrawMain(hwnd, hdc, &rc);
    ::EndPaint(hwnd, &ps);
}

void OnButtonInstall()
{
    OutputDebugStringA("OnButtonInstall()");
}

void OnMouseMove(HWND hwnd, int x, int y)
{
    gBallX = x;
    gBallY = y;
    ::InvalidateRect(hwnd, NULL, TRUE);
}

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int         wmId;
    RECT        r;
    int         x, y;

    switch (message)
    {
        case WM_CREATE:
            ::GetClientRect(hwnd, &r);
            x = RectDx(&r) - 120 - 8;
            y = RectDy(&r) - 22 - 8;
            gHwndButtonInstall = ::CreateWindow(WC_BUTTON, _T("Install SumatraPDF"),
                                BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE,
                                x, y, 120, 22, hwnd, (HMENU)ID_BUTTON_INSTALL, ghinst, NULL);
            ::SetFont(gHwndButtonInstall, gFontDefault);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_ERASEBKGND:
            // do nothing, helps to avoid flicker
            return TRUE;

        case WM_PAINT:
            OnPaintMain(hwnd);
            break;

        case WM_COMMAND:
            wmId    = LOWORD(wParam);
            switch (wmId)
            {
                case ID_BUTTON_INSTALL:
                    OnButtonInstall();
                    break;
                default:
                    return DefWindowProc(hwnd, message, wParam, lParam);
            }
            break;
        case WM_MOUSEMOVE:
            x = GET_X_LPARAM(lParam); y = GET_Y_LPARAM(lParam);
            OnMouseMove(hwnd, x, y);
            break;

#if 0
        case WM_SIZE:
            break;


        case WM_CHAR:
            break;

        case WM_KEYDOWN:
            break;

#endif

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

static void FillWndClassEx(WNDCLASSEX &wcex, HINSTANCE hInstance) {
    memzero(&wcex, sizeof(wcex));
    wcex.cbSize         = sizeof(wcex);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.hInstance      = hInstance;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
}

static BOOL RegisterWinClass(HINSTANCE hInstance)
{
    WNDCLASSEX  wcex;
    FillWndClassEx(wcex, hInstance);
    wcex.lpfnWndProc    = WndProcFrame;
    wcex.lpszClassName  = INSATLLER_FRAME_CLASS_NAME;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    ATOM atom = RegisterClassEx(&wcex);
    if (!atom)
        return FALSE;
    return TRUE;
}

static BOOL InstanceInit(HINSTANCE hInstance, int nCmdShow)
{
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gGdiplusToken, &gdiplusStartupInput, NULL);
    
    ghinst = hInstance;
    gFontDefault = CreateDefaultGuiFont();

    gHwndFrame = CreateWindow(
            INSATLLER_FRAME_CLASS_NAME, _T("SumatraPDF Installer"),
            //WS_OVERLAPPEDWINDOW,
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            -1, -1, 320, 480,
            NULL, NULL,
            ghinst, NULL);
    if (!gHwndFrame)
        return FALSE;
    ShowWindow(gHwndFrame, SW_SHOW);

    return TRUE;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG                 msg = {0};

    INITCOMMONCONTROLSEX cex = {0};
    cex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    cex.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_USEREX_CLASSES | ICC_COOL_CLASSES ;
    InitCommonControlsEx(&cex);

    if (!RegisterWinClass(hInstance))
        goto Exit;
    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    CoInitialize(NULL);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

Exit:
    GdiplusShutdown(gGdiplusToken);
    CoUninitialize();

    return msg.wParam;
}
