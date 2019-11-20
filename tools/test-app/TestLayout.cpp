#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "test-app.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Win32Window.h"
#include "wingui/ButtonCtrl.h"

static HINSTANCE hInst;
static const WCHAR *gWindowTitle = L"Test layout";
static const WCHAR *WIN_CLASS = L"TabLayutWndCls";
static HWND g_hwnd = nullptr;
static ILayout* mainLayout = nullptr;

#define COL_GRAY RGB(0xdd, 0xdd, 0xdd)
#define COL_WHITE RGB(0xff, 0xff, 0xff)
#define COL_BLACK RGB(0, 0, 0)

static void Draw(HWND hwnd, HDC hdc) {
    RECT rc = GetClientRect(hwnd);
    ScopedBrush brush(CreateSolidBrush(COL_GRAY));
    FillRect(hdc, &rc, brush);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
    {
    }
    break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wp);
        switch (wmId)
        {
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        default:
            return DefWindowProc(hwnd, msg, wp, lp);
        }
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Draw(hwnd, hdc);
        EndPaint(hwnd, &ps);

        //ValidateRect(hwnd, NULL);
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}
static ATOM RegisterWinClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

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

static BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;
    const WCHAR *cls = WIN_CLASS;

    DWORD dwExStyle = 0;
    DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    int dx = 640;
    int dy = 480;
    HWND hwnd = CreateWindowExW(dwExStyle, cls, gWindowTitle, dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT, dx, dy, nullptr, nullptr, hInstance, nullptr);

    if (!hwnd)
        return FALSE;

    g_hwnd = hwnd;

    long fontDy = GetDefaultGuiFontSize();

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return TRUE;
}

static int RunMessageLoop()
{
    MSG msg;
    HACCEL accelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_TESTWIN));
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, accelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}
int TestLayout(HINSTANCE hInstance, int nCmdShow) {
    RegisterWinClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow)) {
        CrashAlwaysIf(true);
        return FALSE;
    }

    auto res = RunMessageLoop();
    return res;
}
