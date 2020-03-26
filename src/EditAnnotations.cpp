/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/Log.h"
#include "utils/LogDbg.h"
#include "utils/FileUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "wingui/WinGui.h"
#include "wingui/TreeModel.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/StaticCtrl.h"
#include "wingui/ButtonCtrl.h"
#include "wingui/ListBoxCtrl.h"

#include "EngineBase.h"
#include "EngineMulti.h"
#include "EngineManager.h"
#include "EditAnnotations.h"

using std::placeholders::_1;

struct EditAnnotationsWindow {
    Window* mainWindow = nullptr;
    HWND hwnd = nullptr;
    ILayout* mainLayout = nullptr;
    ListBoxCtrl* listBox = nullptr;
    ButtonCtrl* buttonCancel = nullptr;

    ListBoxModel* lbModel = nullptr;

    ~EditAnnotationsWindow();
    bool Create();
    void CreateMainLayout();
    void CloseHandler(WindowCloseEvent* ev);
    void SizeHandler(SizeEvent* ev);
    void ButtonCancelHandler();
    void OnFinished();
};

static EditAnnotationsWindow* gEditAnnotationsWindow = nullptr;

EditAnnotationsWindow::~EditAnnotationsWindow() {
    delete mainWindow;
    delete mainLayout;
    delete lbModel;
}

void EditAnnotationsWindow::OnFinished() {
    // TODO: write me
}

void EditAnnotationsWindow::CloseHandler(WindowCloseEvent* ev) {
    // CrashIf(w != ev->w);
    OnFinished();
    delete gEditAnnotationsWindow;
    gEditAnnotationsWindow = nullptr;
}

void EditAnnotationsWindow::ButtonCancelHandler() {
    // gEditAnnotationsWindow->onFinished(nullptr);
    OnFinished();
    delete gEditAnnotationsWindow;
    gEditAnnotationsWindow = nullptr;
}

void EditAnnotationsWindow::SizeHandler(SizeEvent* ev) {
    int dx = ev->dx;
    int dy = ev->dy;
    HWND hwnd = ev->hwnd;
    if (dx == 0 || dy == 0) {
        return;
    }
    ev->didHandle = true;
    InvalidateRect(hwnd, nullptr, false);
    if (dx == mainLayout->lastBounds.Dx() && dy == mainLayout->lastBounds.Dy()) {
        // avoid un-necessary layout
        return;
    }
    Size windowSize{dx, dy};
    auto c = Tight(windowSize);
    auto size = mainLayout->Layout(c);
    Point min{0, 0};
    Point max{size.Width, size.Height};
    Rect bounds{min, max};
    mainLayout->SetBounds(bounds);
}

void EditAnnotationsWindow::CreateMainLayout() {
    HWND parent = mainWindow->hwnd;
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        auto w = new ListBoxCtrl(parent);
        bool ok = w->Create();
        CrashIf(!ok);
        listBox = w;
        auto l = NewListBoxLayout(w);
        vbox->addChild(l, 1);
    }

    {
        auto b = new ButtonCtrl(parent);
        b->SetText("Cance&l");
        b->onClicked = std::bind(&EditAnnotationsWindow::ButtonCancelHandler, this);
        bool ok = b->Create();
        CrashIf(!ok);
        buttonCancel = b;
        auto l = NewButtonLayout(b);
        vbox->addChild(l);
    }

    auto lbm = new ListBoxModelStrings();
    lbm->strings.Append("annotations 1");
    lbm->strings.Append("second annotations");
    lbModel = lbm;
    listBox->SetModel(lbModel);
    mainLayout = vbox;
}

bool EditAnnotationsWindow::Create() {
    auto w = new Window();
    // w->isDialog = true;
    w->backgroundColor = MkRgb((u8)0xee, (u8)0xee, (u8)0xee);
    w->SetTitle("Annotations");
    int dx = DpiScale(nullptr, 640);
    int dy = DpiScale(nullptr, 800);
    w->initialSize = {dx, dy};
    // PositionCloseTo(w, args->hwndRelatedTo);
    // SIZE winSize = {w->initialSize.Width, w->initialSize.Height};
    // LimitWindowSizeToScreen(args->hwndRelatedTo, winSize);
    // w->initialSize = {winSize.cx, winSize.cy};
    bool ok = w->Create();
    CrashIf(!ok);

    mainWindow = w;
    hwnd = w->hwnd;

    w->onClose = std::bind(&EditAnnotationsWindow::CloseHandler, this, _1);
    w->onSize = std::bind(&EditAnnotationsWindow::SizeHandler, this, _1);

    CreateMainLayout();
    LayoutAndSizeToContent(mainLayout, 720, 800, w->hwnd);

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->SetIsVisible(true);

    return true;
}

void StartEditAnnotations() {
    if (gEditAnnotationsWindow) {
        return;
    }
    auto win = new EditAnnotationsWindow();
    gEditAnnotationsWindow = win;
    bool ok = win->Create();
    CrashIf(!ok);
}
