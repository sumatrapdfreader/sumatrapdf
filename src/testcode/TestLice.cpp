#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/Log.h"
#include "utils/LogDbg.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ButtonCtrl.h"
#include "wingui/CheckboxCtrl.h"
#include "wingui/EditCtrl.h"
#include "wingui/DropDownCtrl.h"
#include "wingui/StaticCtrl.h"
#include "wingui/ProgressCtrl.h"

#include "test-app.h"

#include "lice/lice.h"

static LICE_IBitmap *framebuffer;

static HINSTANCE hInst;
static const WCHAR* WIN_CLASS = L"LiceWndCls";
static HWND g_hwnd = nullptr;

#define COL_GRAY RGB(0xdd, 0xdd, 0xdd)
#define COL_WHITE RGB(0xff, 0xff, 0xff)
#define COL_BLACK RGB(0, 0, 0)

static void Draw(HWND hwnd, HDC hdc) {
    RECT rc = GetClientRect(hwnd);
  
    int dx= RectDx(rc);
    int dy = RectDy(rc);
    if (!framebuffer->resize(dx, dy)) {
      dbglog("framebuffer->resize failed\n");
      AutoDeleteBrush brush(CreateSolidBrush(COL_GRAY));
      FillRect(hdc, &rc, brush);
      return;
    }
    auto bgCol = LICE_RGBA(63,63,63,255);
    LICE_Clear(framebuffer, bgCol);

    int x = rc.left;
    int y = rc.top;
    BitBlt(hdc, x, y, dx,dy, framebuffer->getDC(), 0, 0, SRCCOPY);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // dbglogf("msg: 0x%x, wp: %d, lp: %d\n", msg, (int)wp, (int)lp);

    switch (msg) {
        case WM_CREATE:
            //CreateMainLayout(hwnd);
            break;

        case WM_SIZE: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            int currWinDx = RectDx(rect);
            int currWinDy = RectDy(rect);
            dbglogf("WM_SIZE: wp: %d, (%d,%d)\n", (int)wp, currWinDx, currWinDy);
            //doMainLayout();
            return 0;
            // return DefWindowProc(hwnd, msg, wp, lp);
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wp);
            switch (wmId) {
                case IDM_EXIT:
                    DestroyWindow(hwnd);
                    break;
                default:
                    return DefWindowProc(hwnd, msg, wp, lp);
            }
        } break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            Draw(hwnd, hdc);
            EndPaint(hwnd, &ps);
            // ValidateRect(hwnd, NULL);
        } break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}
static ATOM RegisterWinClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex{};

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TESTWIN));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_TESTWIN);
    wcex.lpszClassName = WIN_CLASS;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    return RegisterClassExW(&wcex);
}

static BOOL CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
    hInst = hInstance;
    const WCHAR* cls = WIN_CLASS;

    DWORD dwExStyle = 0;
    DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    int dx = 640;
    int dy = 480;
    HWND hwnd = CreateWindowExW(dwExStyle, cls, L"Test lice", dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, dx, dy, nullptr,
                                nullptr, hInstance, nullptr);

    if (!hwnd) {
        return FALSE;
    }

    g_hwnd = hwnd;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return TRUE;
}

int TestLice(HINSTANCE hInstance, int nCmdShow) {
    RegisterWinClass(hInstance);

    framebuffer = new LICE_SysBitmap(0,0);
    if (!CreateMainWindow(hInstance, nCmdShow)) {
        CrashAlwaysIf(true);
        return FALSE;
    }
    HACCEL accelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_TESTWIN));
    auto res = RunMessageLoop(accelTable);
    return res;
}
