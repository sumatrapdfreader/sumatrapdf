#include "test-app.h"
#include "utils/BaseUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ButtonCtrl.h"

// in TestTab.cpp
extern int TestTab(HINSTANCE hInstance, int nCmdShow);
// in TestLayout.cpp
extern int TestLayout(HINSTANCE hInstance, int nCmdShow);

static std::tuple<ILayout*, ButtonCtrl*> CreateButtonLayout(HWND parent, std::string_view s, OnClicked onClicked) {
    auto b = new ButtonCtrl(parent);
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

void __cdecl SendCrashReport(char const*) {
    // a dummy implementation
}

int APIENTRY WinMain(HINSTANCE hInstance, [[maybe_unused]]  HINSTANCE hPrevInstance, [[maybe_unused]]  LPSTR lpCmdLine,
                     [[maybe_unused]]  int nCmdShow) {
    // SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
    // SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    gHinst = hInstance;

    INITCOMMONCONTROLSEX cc = {0};
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
    CrashIf(!ok);

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
}
