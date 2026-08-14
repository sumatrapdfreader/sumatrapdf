#include "base/Base.h"

#include "base/Win.h"
#include "gui/UIModels.h"

#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

// in TestTab.cpp
extern int TestTab(int nCmdShow);
// in TestLayout.cpp
extern int TestLayout(int nCmdShow);
// in SumatraPDF.cpp
extern bool IsUIRtl();

static void LaunchTabs() {
    TestTab(SW_SHOW);
}

static void LaunchLayout() {
    TestLayout(SW_SHOW);
}

static ILayout* CreateMainLayout(HWND) {
    auto* vbox = new VBox();

    vbox->alignMain = MainAxisAlign::MainCenter;
    vbox->alignCross = CrossAxisAlign::CrossCenter;
    PlatformFont* font = GetDefaultGuiFont();
    {
        auto* b = new VirtButton("Tabs test", font);
        b->onClick = MkFunc0Void(LaunchTabs);
        vbox->AddChild(b);
    }

    {
        auto* b = new VirtButton("Layout test", font);
        b->onClick = MkFunc0Void(LaunchLayout);
        vbox->AddChild(b);
    }

    auto padding = new Padding(vbox, DefaultInsets());
    return padding;
}

struct TestWnd : WindowBase {};

static void OnDestroy(WindowBase::DestroyEvent*) {
    ::PostQuitMessage(0);
}

void TestApp() {
    auto w = new TestWnd();
    auto fn = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    w->onDestroy = fn;

    // w->backgroundColor = MkRgb((u8)0xae, (u8)0xae, (u8)0xae);
    CreateCustomArgs args;
    args.pos = {CW_USEDEFAULT, CW_USEDEFAULT, 480, 640};
    args.title = "a little test app";
    HWND hwnd = w->CreateCustom(args);
    ReportIf(!hwnd);

    w->layout = CreateMainLayout(w->hwnd);
    LayoutToSize(w->layout, {480, 640});
    HwndInvalidate(hwnd);

#if 0
    w->onSize = [&](SizeEvent* args) {
        HWND hwnd = args->hwnd;
        int dx = args->dx;
        int dy = args->dy;
        if (dx == 0 || dy == 0) {
            return;
        }
        LayoutToSize(l, {dx, dy});
        HwndInvalidate(hwnd);
    };
#endif
    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->SetIsVisible(true);

    RunMessageLoop(nullptr, w->hwnd);
    delete w;
    return;
}
