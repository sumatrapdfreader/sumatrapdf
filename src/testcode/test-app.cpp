#include "test-app.h"
#include "utils/BaseUtil.h"

#if 0
// in TestTab.cpp
extern int TestTab(HINSTANCE hInstance, int nCmdShow);
// in TestLayout.cpp
extern int TestLayout(HINSTANCE hInstance, int nCmdShow);

static std::tuple<ILayout*, Button*> CreateButtonLayout(HWND parent, const char* s, OnClicked onClicked) {
    auto b = new Button(parent);
    b->OnClicked = onClicked;
    b->SetText(s);
    b->Create();
    return {NewButtonLayout(b), b};
}

HINSTANCE gHinst = nullptr;

static void LaunchTabs() {
    TestTab(gHinst, SW_SHOW);
}

static void LaunchLayout() {
    TestLayout(gHinst, SW_SHOW);
}

static ILayout* CreateMainLayout(HWND hwnd) {
    auto* vbox = new VBox();

    vbox->alignMain = MainAxisAlign::MainCenter;
    vbox->alignCross = CrossAxisAlign::CrossCenter;

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Tabs test", LaunchTabs);
        vbox->addChild(l);
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Layout test", LaunchLayout);
        vbox->addChild(l);
    }

    auto* padding = new Padding();
    padding->child = vbox;
    padding->insets = DefaultInsets();
    return padding;
}
#endif

void _uploadDebugReportIfFunc(__unused bool cond, __unused const char* condStr) {
    // no-op implementation to satisfy SubmitBugReport()
}

int APIENTRY WinMain(HINSTANCE hInstance, __unused HINSTANCE hPrevInstance, __unused LPSTR lpCmdLine,
                     __unused int nCmdShow) {
    // SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
    // SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
#if 0
    gHinst = hInstance;

    INITCOMMONCONTROLSEX cc{};
    cc.dwSize = sizeof(cc);
    cc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&cc);

    // return TestTab(hInstance, nCmdShow);
    // return TestLayout(hInstance, nCmdShow);

    auto w = new Window();
    w->backgroundColor = MkRgb((u8)0xae, (u8)0xae, (u8)0xae);
    w->SetTitle("this is a title");
    w->initialPos = {100, 100};
    w->initialSize = {480, 640};
    bool ok = w->Create();
    ReportIf(!ok);

    auto l = CreateMainLayout(w->hwnd);
    w->onSize = [&](SizeEvent* args) {
        HWND hwnd = args->hwnd;
        int dx = args->dx;
        int dy = args->dy;
        if (dx == 0 || dy == 0) {
            return;
        }
        //auto c = Loose(Size{dx, dy});
        LayoutToSize(l, {dx, dy});
        InvalidateRect(hwnd, nullptr, false);
    };

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->SetIsVisible(true);

    auto res = RunMessageLoop(nullptr);
    return res;
#else
    return 0;
#endif
}
