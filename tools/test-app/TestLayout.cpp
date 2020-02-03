#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/Log.h"

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

static HINSTANCE hInst;
static const WCHAR* gWindowTitle = L"Test layout";
static const WCHAR* WIN_CLASS = L"LayutWndCls";
static HWND g_hwnd = nullptr;
static VBox* vboxLayout = nullptr;
static ILayout* mainLayout = nullptr;
static int currWinDx = 0;
static int currWinDy = 0;

#define COL_GRAY RGB(0xdd, 0xdd, 0xdd)
#define COL_WHITE RGB(0xff, 0xff, 0xff)
#define COL_BLACK RGB(0, 0, 0)

static void Draw(HWND hwnd, HDC hdc) {
    RECT rc = GetClientRect(hwnd);
    AutoDeleteBrush brush(CreateSolidBrush(COL_GRAY));
    FillRect(hdc, &rc, brush);
}

static void doMainLayout() {
    Size windowSize{currWinDx, currWinDy};
    Constraints constraints = Tight(windowSize);
    auto size = mainLayout->Layout(constraints);
    dbglogf("doLayout: (%d,%d) => (%d, %d)\n", currWinDx, currWinDy, size.Width, size.Height);
    Point min{0, 0};
    Point max{size.Width, size.Height};
    Rect bounds{min, max};
    mainLayout->SetBounds(bounds);
    InvalidateRect(g_hwnd, nullptr, false);
}

static void onCheckboxChanged(CheckState state) {
    const char* name = "";
    switch (state) {
        case CheckState::Unchecked:
            name = "unchecked";
            break;
        case CheckState::Checked:
            name = "checked";
            break;
        case CheckState::Indeterminate:
            name = "indeterminate";
            break;
        default:
            CrashMe();
            break;
    }

    dbglogf("new checkbox state: %s (%d)\n", name, (int)state);
}

static ILayout* CreateCheckboxLayout(HWND parent, std::string_view s) {
    auto b = new CheckboxCtrl(parent);
    b->SetText(s);
    b->OnCheckStateChanged = onCheckboxChanged;
    b->Create();
    return NewCheckboxLayout(b);
}

static void onTextChanged(EditTextChangedArgs* args) {
    std::string_view s = args->text;
    dbglogf("text changed: '%s'\n", s.data());
}

static ILayout* CreateEditLayout(HWND parent, std::string_view s) {
    auto e = new EditCtrl(parent);
    e->SetText(s);
    e->SetCueText("a cue text");
    e->OnTextChanged = onTextChanged;
    e->Create();
    return NewEditLayout(e);
}

static char* ddItems[3] = {"foo", "another one", "bar"};

static void onDropDownSelected(DropDownSelectionChangedArgs* args) {
    int idx = args->idx;
    std::string_view s = args->item;
    dbglogf("drop down selection changed: %d, %s\n", idx, s.data());
}

static ILayout* CreatedDropDownLayout(HWND parent) {
    auto w = new DropDownCtrl(parent);
    for (size_t i = 0; i < dimof(ddItems); i++) {
        char* s = ddItems[i];
        std::string_view sv(s);
        w->items.Append(sv);
    }
    w->onDropDownSelectionChanged = onDropDownSelected;
    w->Create();
    return NewDropDownLayout(w);
}

static ILayout* CreateStaticLayout(HWND parent, std::string_view s) {
    auto w = new StaticCtrl(parent);
    w->SetText(s);
    w->Create();
    return NewStaticLayout(w);
}

static int maxProgress = 8;
static int currProgress = 0;
static ProgressCtrl* gProgress = nullptr;

static std::tuple<ILayout*, ProgressCtrl*> CreateProgressLayout(HWND parent, int maxRange) {
    auto w = new ProgressCtrl(parent, maxRange);
    w->Create();
    return {NewProgressLayout(w), w};
}

static void ToggleMainAxis() {
    u8 n = (u8)vboxLayout->alignMain + 1;
    if (n > (u8)MainAxisAlign::Homogeneous) {
        n = 0;
    }
    vboxLayout->alignMain = (MainAxisAlign)n;
    dbglogf("toggle main axis to %d\n", (int)n);
    doMainLayout();
}

static void ToggleCrossAxis() {
    u8 n = (u8)vboxLayout->alignCross + 1;
    if (n > (u8)CrossAxisAlign::CrossEnd) {
        n = 0;
    }
    vboxLayout->alignCross = (CrossAxisAlign)n;
    dbglogf("toggle cross axis to %d\n", (int)n);
    doMainLayout();
}

static void AdvanceProgress() {
    currProgress++;
    if (currProgress > maxProgress) {
        currProgress = 0;
    }
    gProgress->SetCurrent(currProgress);
    dbglogf("advance progress to %d\n", currProgress);
}

static void CreateMainLayout(HWND hwnd) {
    auto* vbox = new VBox();

    vbox->alignMain = MainAxisAlign::MainEnd;
    vbox->alignCross = CrossAxisAlign::Stretch;
    {
        auto [l, b] = CreateButtonLayout(hwnd, "toggle main axis", ToggleMainAxis);
        vbox->addChild(l);
    }
    {
        auto [l, b] = CreateButtonLayout(hwnd, "advance progress", AdvanceProgress);
        vbox->addChild(l);
    }

    {
        auto l = CreateEditLayout(hwnd, "initial text");
        vbox->addChild(l);
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "toggle cross axis", ToggleCrossAxis);
        vbox->addChild(l);
    }

    {
        auto l = CreateCheckboxLayout(hwnd, "checkbox one");
        auto elInfo = vbox->addChild(l, 0);
    }
    {
        auto l = CreateCheckboxLayout(hwnd, "checkbox two");
        vbox->addChild(l);
    }
    {
        auto l = CreatedDropDownLayout(hwnd);
        vbox->addChild(l);
    }
    {
        auto l = CreateStaticLayout(hwnd, "static control");
        auto l2 = new Align(l);
        l2->HAlign = AlignEnd;
        vbox->addChild(l2);
    }
    {
        auto [l, w] = CreateProgressLayout(hwnd, maxProgress);
        w->idealDy = 32;
        w->idealDx = 128;
        gProgress = w;
        vbox->addChild(l);
        AdvanceProgress();
    }

    vboxLayout = vbox;
    auto* padding = new Padding();
    padding->child = vbox;
    padding->insets = DefaultInsets();
    mainLayout = padding;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // dbglogf("msg: 0x%x, wp: %d, lp: %d\n", msg, (int)wp, (int)lp);

    switch (msg) {
        case WM_CREATE:
            CreateMainLayout(hwnd);
            break;

        case WM_SIZE: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            currWinDx = RectDx(rect);
            currWinDy = RectDy(rect);
            dbglogf("WM_SIZE: wp: %d, (%d,%d)\n", (int)wp, currWinDx, currWinDy);
            doMainLayout();
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
    HWND hwnd = CreateWindowExW(dwExStyle, cls, gWindowTitle, dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, dx, dy, nullptr,
                                nullptr, hInstance, nullptr);

    if (!hwnd)
        return FALSE;

    g_hwnd = hwnd;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return TRUE;
}

int TestLayout(HINSTANCE hInstance, int nCmdShow) {
    RegisterWinClass(hInstance);

    if (!CreateMainWindow(hInstance, nCmdShow)) {
        CrashAlwaysIf(true);
        return FALSE;
    }
    HACCEL accelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_TESTWIN));
    auto res = RunMessageLoop(accelTable);
    return res;
}
