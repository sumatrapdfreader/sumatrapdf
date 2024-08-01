#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#if 0
#include "wingui/Layout.h"

#include "test-app.h"

#include "utils/Log.h"

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
    LayoutToSize(mainLayout, {currWinDx, currWinDy});
    InvalidateRect(g_hwnd, nullptr, false);
}

static void onCheckboxChanged(Checkbox::State state) {
    const char* name = "";
    switch (state) {
        case Checkbox::State::Unchecked:
            name = "unchecked";
            break;
        case Checkbox::State::Checked:
            name = "checked";
            break;
        case Checkbox::State::Indeterminate:
            name = "indeterminate";
            break;
        default:
            CrashMe();
            break;
    }

    logf("new checkbox state: %s (%d)\n", name, (int)state);
}

static CheckboxCtrl* CreateCheckbox(HWND parent, std::string_view s) {
    auto b = new CheckboxCtrl();
    b->SetText(s);
    b->onStateChanged = onCheckboxChanged;
    b->Create(parent);
    return b;
}

static void onTextChanged(EditTextChangedEvent* args) {
    std::string_view s = args->text;
    logf("text changed: '%s'\n", s.data());
}

static EditCtrl* CreateEdit(HWND parent, std::string_view s) {
    auto w = new EditCtrl();
    w->SetText(s);
    w->SetCueText("a cue text");
    w->onTextChanged = onTextChanged;
    w->Create(parent);
    return w;
}

static const char* ddItems[3] = {"foo", "another one", "bar"};

static void onDropDownSelected(DropDownSelectionChangedEvent* args) {
    int idx = args->idx;
    std::string_view s = args->item;
    logf("drop down selection changed: %d, %s\n", idx, s.data());
}

static DropDownCtrl* CreatedDropDown(HWND parent) {
    auto w = new DropDownCtrl();
    for (size_t i = 0; i < dimof(ddItems); i++) {
        auto s = ddItems[i];
        std::string_view sv(s);
        w->items.Append(sv);
    }
    w->onSelectionChanged = onDropDownSelected;
    w->Create(parent);
    return w;
}

static StaticCtrl* CreateStatic(HWND parent, std::string_view s) {
    auto w = new StaticCtrl();
    w->SetText(s);
    w->Create(parent);
    return w;
}

static int maxProgress = 8;
static int currProgress = 0;
static ProgressCtrl* gProgress = nullptr;

static ProgressCtrl* CreateProgress(HWND parent, int maxRange) {
    auto w = new ProgressCtrl(maxRange);
    w->Create(parent);
    return w;
}

static void ToggleMainAxis(void*) {
    u8 n = (u8)vboxLayout->alignMain + 1;
    if (n > (u8)MainAxisAlign::Homogeneous) {
        n = 0;
    }
    vboxLayout->alignMain = (MainAxisAlign)n;
    logf("toggle main axis to %d\n", (int)n);
    doMainLayout();
}

static void ToggleCrossAxis(void*) {
    u8 n = (u8)vboxLayout->alignCross + 1;
    if (n > (u8)CrossAxisAlign::CrossEnd) {
        n = 0;
    }
    vboxLayout->alignCross = (CrossAxisAlign)n;
    logf("toggle cross axis to %d\n", (int)n);
    doMainLayout();
}

static void AdvanceProgress(void*) {
    currProgress++;
    if (currProgress > maxProgress) {
        currProgress = 0;
    }
    gProgress->SetCurrent(currProgress);
    logf("advance progress to %d\n", currProgress);
}

static void CreateMainLayout(HWND hwnd) {
    auto* vbox = new VBox();

    vbox->alignMain = MainAxisAlign::MainEnd;
    vbox->alignCross = CrossAxisAlign::Stretch;
    {
        auto b = CreateButton(hwnd, "toggle main axis", mkFunc0<void>(ToggleMainAxis, nullptr));
        vbox->AddChild(b);
    }

    {
        auto b = CreateButton(hwnd, "advance progress", mkFunc0<void>(AdvanceProgress, nullptr));
        vbox->AddChild(b);
    }

    {
        auto l = CreateEdit(hwnd, "initial text");
        vbox->AddChild(l);
    }

    {
        auto b = CreateButton(hwnd, "toggle cross axis", mkFunc0<void>(ToggleCrossAxis, nullptr));
        vbox->AddChild(b);
    }

    {
        auto l = CreateCheckbox(hwnd, "checkbox one");
        vbox->AddChild(l, 0);
    }

    {
        auto l = CreateCheckbox(hwnd, "checkbox two");
        vbox->AddChild(l);
    }

    {
        auto w = CreatedDropDown(hwnd);
        vbox->AddChild(w);
    }

    {
        auto l = CreateStatic(hwnd, "static control");
        auto l2 = new Align(l);
        l2->HAlign = AlignEnd;
        vbox->AddChild(l2);
    }

    {
        auto w = CreateProgress(hwnd, maxProgress);
        w->idealDy = 32;
        w->idealDx = 128;
        gProgress = w;
        vbox->AddChild(w);
        AdvanceProgress();
    }

    vboxLayout = vbox;
    auto padding = new Padding(vbox, DefaultInsets());
    mainLayout = padding;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    //DbgLogMsg("tl", hwnd, msg, wp, lp);

    LRESULT res = 0;
    res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res) {
        return res;
    }

    switch (msg) {
        case WM_CREATE:
            CreateMainLayout(hwnd);
            break;

        case WM_SIZE: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            currWinDx = RectDx(rect);
            currWinDy = RectDy(rect);
            //logf("WM_SIZE: wp: %d, (%d,%d)\n", (int)wp, currWinDx, currWinDy);
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
#endif

int TestLayout(HINSTANCE hInstance, int nCmdShow) {
#if 0
    RegisterWinClass(hInstance);

    if (!CreateMainWindow(hInstance, nCmdShow)) {
        ReportIf(true);
        return FALSE;
    }
    HACCEL accelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_TESTWIN));
    auto res = RunMessageLoop(accelTable, g_hwnd);
    return res;
#else
    return 0;
#endif
}
