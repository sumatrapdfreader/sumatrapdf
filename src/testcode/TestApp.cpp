#include "test-app.h"
#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "utils/Log.h"

// in TestTab.cpp
extern int TestTab(HINSTANCE hInstance, int nCmdShow);
// in TestLayout.cpp
extern int TestLayout(HINSTANCE hInstance, int nCmdShow);

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
        auto b = CreateButton(hwnd, "Tabs test", MkFunc0Void(LaunchTabs));
        vbox->AddChild(b);
    }

    {
        auto b = CreateButton(hwnd, "Layout test", MkFunc0Void(LaunchLayout));
        vbox->AddChild(b);
    }

    auto padding = new Padding(vbox, DefaultInsets());
    return padding;
}

struct TestWnd : Wnd {
};

static void OnDestroy(Wnd::DestroyEvent*) {
    ::PostQuitMessage(0);
}

// in Window.cpp
int RunMessageLoop(HACCEL accelTable, HWND hwndDialog);

void TestApp(HINSTANCE hInstance) {
    gHinst = hInstance;

    auto w = new TestWnd();
    auto fn = MkFunc1Void<Wnd::DestroyEvent*>(OnDestroy);
    w->onDestroy = fn;

    // w->backgroundColor = MkColor((u8)0xae, (u8)0xae, (u8)0xae);
    CreateCustomArgs args;
    args.pos = {CW_USEDEFAULT, CW_USEDEFAULT, 480, 640};
    args.title = "a little test app";
    HWND hwnd = w->CreateCustom(args);
    ReportIf(!hwnd);

    w->layout = CreateMainLayout(w->hwnd);
    LayoutToSize(w->layout, {480, 640});
    InvalidateRect(hwnd, nullptr, false);

#if 0
    w->onSize = [&](SizeEvent* args) {
        HWND hwnd = args->hwnd;
        int dx = args->dx;
        int dy = args->dy;
        if (dx == 0 || dy == 0) {
            return;
        }
        LayoutToSize(l, {dx, dy});
        InvalidateRect(hwnd, nullptr, false);
    };
#endif
    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->SetIsVisible(true);

    RunMessageLoop(nullptr, w->hwnd);
    delete w;
    return;
}
