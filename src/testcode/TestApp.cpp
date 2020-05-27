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
// in TestLice.cpp
extern int TestLice(HINSTANCE hInstance, int nCmdShow);

HINSTANCE gHinst = nullptr;

static void LaunchTabs() {
    TestTab(gHinst, SW_SHOW);
}

static void LaunchLayout() {
    TestLayout(gHinst, SW_SHOW);
}

static void LaunchLice() {
    TestLice(gHinst, SW_SHOW);
}

static ILayout* CreateMainLayout(HWND hwnd) {
    auto* vbox = new VBox();

    vbox->alignMain = MainAxisAlign::MainCenter;
    vbox->alignCross = CrossAxisAlign::CrossCenter;

    {
        auto b = CreateButton(hwnd, "Tabs test", LaunchTabs);
        vbox->AddChild(b);
    }

    {
        auto b = CreateButton(hwnd, "Layout test", LaunchLayout);
        vbox->AddChild(b);
    }

    {
        auto b = CreateButton(hwnd, "Lice test", LaunchLice);
        vbox->AddChild(b);
    }
    auto padding = new Padding(vbox, DefaultInsets());
    return padding;
}

void TestApp(HINSTANCE hInstance) {
    gHinst = hInstance;

    // return TestDirectDraw(hInstance, nCmdShow);
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
        LayoutToSize(l, {dx, dy});
        InvalidateRect(hwnd, nullptr, false);
    };

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->SetIsVisible(true);

    auto res = RunMessageLoop(nullptr, w->hwnd);
    return;
}
