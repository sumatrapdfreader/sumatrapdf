#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/SimpleLog.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Win32Window.h"
#include "wingui/ButtonCtrl.h"

#include "test-app.h"

static HINSTANCE hInst;
static const WCHAR* gWindowTitle = L"Test layout";
static const WCHAR* WIN_CLASS = L"LayutWndCls";
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

static void CreateMainLayout(HWND hwnd) {
    auto b1 = new ButtonCtrl(hwnd, 0, nullptr);
    b1->Create(L"Button one");
    auto b2 = new ButtonCtrl(hwnd, 0, nullptr);
    b2->Create(L"Button two");
    auto b3 = new ButtonCtrl(hwnd, 0, nullptr);
    b3->Create(L"Button three");
    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::Homogeneous;
    vbox->alignCross = CrossAxisAlign::CrossCenter;
    vbox->addChild(b1);
    vbox->addChild(b2);
    vbox->addChild(b3);
    auto* padding = new Padding();
    padding->child = vbox;
    padding->insets = DefaultInsets();
    mainLayout = padding;
}

static void doLayut(HWND hwnd, int dx, int dy) {
    UNUSED(hwnd);
    Size windowSize{dx, dy};
    Constraints constraints = Tight(windowSize);
    auto size = mainLayout->Layout(constraints);

    Point min{0, 0};
    Point max{size.Width, size.Height};
    Rect bounds{min, max};
    mainLayout->SetBounds(bounds);
    dbglogf("doLayout: dx: %d, dy: %d\n", dx, dy);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            CreateMainLayout(hwnd);
            break;

        case WM_SIZE: {
            int dx = LOWORD(lp);
            int dy = HIWORD(lp);
            doLayut(hwnd, dx, dy);
        }
            break;
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

static BOOL CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
    hInst = hInstance;
    const WCHAR* cls = WIN_CLASS;

    DWORD dwExStyle = 0;
    DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    int dx = 640;
    int dy = 480;
    HWND hwnd = CreateWindowExW(dwExStyle, cls, gWindowTitle, dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, dx, dy, nullptr,
                                nullptr, hInstance, nullptr);

    if (!hwnd)
        return FALSE;

    g_hwnd = hwnd;

    long fontDy = GetDefaultGuiFontSize();

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return TRUE;
}

static int RunMessageLoop() {
    MSG msg;
    HACCEL accelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_TESTWIN));
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, accelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}
int TestLayout(HINSTANCE hInstance, int nCmdShow) {
    RegisterWinClass(hInstance);

    if (!CreateMainWindow(hInstance, nCmdShow)) {
        CrashAlwaysIf(true);
        return FALSE;
    }

    auto res = RunMessageLoop();
    return res;
}
