#include "test-app.h"
#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ListBoxCtrl.h"
#include "wingui/ButtonCtrl.h"
#include "wingui/wingui2.h"

#include "utils/Log.h"

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

/*
static void LaunchLice() {
    TestLice(gHinst, SW_SHOW);
}
*/

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

    /*
    {
        auto b = CreateButton(hwnd, "Lice test", LaunchLice);
        vbox->AddChild(b);
    }
    */
    auto padding = new Padding(vbox, DefaultInsets());
    return padding;
}

void TestApp(HINSTANCE hInstance) {
    gHinst = hInstance;

    // return TestDirectDraw(hInstance, nCmdShow);
    // return TestTab(hInstance, nCmdShow);
    // return TestLayout(hInstance, nCmdShow);

    auto w = new Window();
    w->backgroundColor = MkColor((u8)0xae, (u8)0xae, (u8)0xae);
    w->SetTitle("this is a title");
    w->initialPos = {100, 100};
    w->initialSize = {480, 640};
    bool ok = w->Create(0);
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
    delete w;
    return;
}

using namespace wg;

struct CommandPaletteWnd : Wnd {
    ~CommandPaletteWnd() override {
        delete mainLayout;
    }
    Button* btn = nullptr;
    Edit* editQuery = nullptr;
    ListBox *listBoxResults = nullptr;

    LayoutBase* mainLayout = nullptr;

    void OnDestroy() override;
    bool PreTranslateMessage(MSG&msg) override;

    bool Create();
    void QueryChanged();
    void ListDoubleClick();
    void ButtonClicked();
};

bool CommandPaletteWnd::PreTranslateMessage(MSG&msg) {
    if (msg.message == WM_KEYDOWN) {
        int dir = 0;
        if (msg.wParam == VK_ESCAPE) {
            Close();
            return true;
        }

        if (msg.wParam == VK_RETURN) {
            ListDoubleClick();
            return true;
        }

        if (msg.wParam == VK_UP) {
            dir = -1;
        } else if (msg.wParam == VK_DOWN) {
            dir = 1;
        }
        if (!dir) {
            return false;
        }
        int n = listBoxResults->GetCount();
        if (n == 0) {
            return false;
        }
        int currSel = listBoxResults->GetCurrentSelection();
        int sel = currSel + dir;
        if (sel < 0) {
            sel = n -1;
        }
        if (sel >= n) {
            sel = 0;
        }
        listBoxResults->SetCurrentSelection(sel);
    }
    return false;
}

void CommandPaletteWnd::QueryChanged() {
    logf("query changed\n");
}

void CommandPaletteWnd::ListDoubleClick() {
    int sel = listBoxResults->GetCurrentSelection();
    if (sel >= 0) {
        logf("selected an item %d\n", sel);
        Close();
    }
}

void CommandPaletteWnd::ButtonClicked() {
    Close();
}

void CommandPaletteWnd::OnDestroy() {
    ::PostQuitMessage(0);
}

bool CommandPaletteWnd::Create() {
    {
        CreateCustomArgs args;
        args.title = L"Command Palette";
        args.visible = false;
        args.style = WS_POPUPWINDOW;
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        auto c = new Edit();
        EditCreateArgs args;
        args.parent = hwnd;
        args.isMultiLine = false;
        args.withBorder = true;
        args.cueText = "a cue text";
        HWND ok = c->Create(args);
        CrashIf(!ok);
        c->maxDx = 150;
        c->onTextChanged = std::bind(&CommandPaletteWnd::QueryChanged, this);
        editQuery = c;
        vbox->AddChild(c);
    }

    {
        auto c = new ListBox();
        c->idealSizeLines = 32;
        c->SetInsetsPt(4, 0);
        auto wnd = c->Create(hwnd);
        CrashIf(!wnd);

        auto m = new ListBoxModelStrings();
        m->strings.Append("Hello");
        m->strings.Append("My friend");
        c->SetModel(m);
        c->onDoubleClick = std::bind(&CommandPaletteWnd::ListDoubleClick, this);
        listBoxResults = c;
        vbox->AddChild(c, 1);
    }
    {
        auto c = new Button();
        auto wnd = c->Create(hwnd);
        CrashIf(!wnd);
        c->SetText(L"Close");
        c->onClicked = std::bind(&CommandPaletteWnd::ButtonClicked, this);
        btn = c;
        vbox->AddChild(c);
    }

    auto padding = new Padding(vbox, DpiScaledInsets(hwnd, 4, 8));
    mainLayout = padding;

    LayoutAndSizeToContent(mainLayout, 520, 720, hwnd);
    CenterDialog(hwnd);
    SetIsVisible(true);
    ::SetFocus(editQuery->hwnd);
    return true;
}

void TestWingui() {
    auto w = new CommandPaletteWnd();
    bool ok = w->Create();
    CrashIf(!ok);
    auto res = RunMessageLoop(nullptr, w->hwnd);
    delete w;
    return;
}