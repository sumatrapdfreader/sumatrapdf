#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "test-app.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"

#include "wingui/TabsCtrl.h"

static HINSTANCE hInst;
static const WCHAR* gWindowTitle = L"Test application";
static const WCHAR* WIN_CLASS = L"TabTestWndCls";
static TabsCtrl* g_tabsCtrl = nullptr;
static HWND g_hwnd = nullptr;

#define COL_GRAY RGB(0xdd, 0xdd, 0xdd)
#define COL_WHITE RGB(0xff, 0xff, 0xff)
#define COL_BLACK RGB(0, 0, 0)

static void UpdateTabsSize() {
    SIZE sz = GetIdealSize(g_tabsCtrl);
    RECT tabsPos = MakeRect(10, 10, sz.cx, sz.cy);
    SetPos(g_tabsCtrl, tabsPos);
    TriggerRepaint(g_hwnd);
}

static void Draw(HWND hwnd, HDC hdc) {
    RECT rc = GetClientRect(hwnd);
    AutoDeleteBrush brush(CreateSolidBrush(COL_GRAY));
    FillRect(hdc, &rc, brush);
}

static void OnTabSelected(TabsCtrl* tabsCtrl, TabsCtrlState* currState, int selectedTabIdx) {
    CrashIf(g_tabsCtrl != tabsCtrl);
    CrashIf(currState->selectedItem == selectedTabIdx);
    currState->selectedItem = selectedTabIdx;
    SetState(tabsCtrl, currState);
}

static void OnTabClosed(TabsCtrl* tabsCtrl, TabsCtrlState* currState, int tabIdx) {
    CrashIf(g_tabsCtrl != tabsCtrl);
    currState->tabs.RemoveAt(tabIdx);
    if (currState->selectedItem == tabIdx) {
        if (currState->selectedItem > 0) {
            currState->selectedItem--;
        }
    }
    SetState(tabsCtrl, currState);
    UpdateTabsSize();
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
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

static BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
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
    RECT tabsPos = MakeRect(8, 8, 320, fontDy + 8);
    g_tabsCtrl = AllocTabsCtrl(hwnd, tabsPos);
    g_tabsCtrl->onTabSelected = OnTabSelected;
    g_tabsCtrl->onTabClosed = OnTabClosed;
    auto ok = CreateTabsCtrl(g_tabsCtrl);
    CrashIf(!ok);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return TRUE;
}

int TestTab(HINSTANCE hInstance, int nCmdShow) {
    RegisterWinClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow)) {
        CrashAlwaysIf(true);
        return FALSE;
    }

    auto tabsState = new TabsCtrlState();
    tabsState->selectedItem = 0;

    std::array<TabItem*, 3> tabs = {
        new TabItem{"tab1", "tab 1 tooltip"},
        new TabItem{"tab 2 with a very long name", ""},
        new TabItem{"another tab", "another tab tooltip"},
    };

    for (auto& tab : tabs) {
        tabsState->tabs.push_back(tab);
    }

    SetState(g_tabsCtrl, tabsState);
    UpdateTabsSize();

    HACCEL accelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_TESTWIN));
    auto res = RunMessageLoop(accelTable);
    return res;
}
