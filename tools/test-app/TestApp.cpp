#include "test-app.h"
#include "utils/BaseUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ButtonCtrl.h"

// in TestDirectDraw.cpp
extern int TestDirectDraw(HINSTANCE hInstance, int nCmdShow);
// in TestTab.cpp
extern int TestTab(HINSTANCE hInstance, int nCmdShow);
// in TestLayout.cpp
extern int TestLayout(HINSTANCE hInstance, int nCmdShow);

HINSTANCE gHinst = nullptr;

static void LaunchDirectDraw() {
    TestDirectDraw(gHinst, SW_SHOW);
}

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
        auto [l, b] = CreateButtonLayout(hwnd, "DirectDraw test", LaunchDirectDraw);
        vbox->addChild(l);
    }

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
    w->onSize = [&](SizeArgs* args) {
        HWND hwnd = args->hwnd;
        int dx = args->dx;
        int dy = args->dy;
        if (dx == 0 || dy == 0) {
            return;
        }
        //auto c = Loose(Size{dx, dy});
        Size windowSize{dx, dy};
        auto c = Tight(windowSize);
        auto size = l->Layout(c);
        Point min{0, 0};
        Point max{size.Width, size.Height};
        Rect bounds{min, max};
        l->SetBounds(bounds);
        InvalidateRect(hwnd, nullptr, false);
    };

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->SetIsVisible(true);

    auto res = RunMessageLoop(nullptr);
    return;
}
