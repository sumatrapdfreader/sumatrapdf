/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/Log.h"
#include "utils/WinUtil.h"
#include "utils/BitManip.h"
#include "utils/Dpi.h"

#include "wingui/WinGui.h"
#include "wingui/TreeModel.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/StaticCtrl.h"
#include "wingui/TreeCtrl.h"
#include "wingui/EditCtrl.h"
#include "wingui/CheckboxCtrl.h"
#include "wingui/ButtonCtrl.h"

#include "EngineBase.h"

#include "ParseBKM.h"
#include "TocEditor.h"
#include "TocEditTitle.h"

using std::placeholders::_1;

struct EditTitleWindow {
    HWND hwndOwner = nullptr;
    TreeCtrl* treeCtrl = nullptr;
    TocItem* tocItem = nullptr;

    Window* mainWindow = nullptr;
    ILayout* mainLayout = nullptr;
    EditCtrl* editTitle = nullptr;
    CheckboxCtrl* checkboxItalic = nullptr;
    CheckboxCtrl* checkboxBold = nullptr;
    EditCtrl* editColor = nullptr;

    TocEditArgs* args = nullptr;
    TocEditFinishedHandler onFinished = nullptr;

    EditTitleWindow() = default;
    ~EditTitleWindow();
    void CloseHandler(WindowCloseEvent*);
    void SizeHandler(SizeEvent*);
    void KeyDownHandler(KeyEvent*);
    void ButtonOkHandler();
    void ButtonCancelHandler();
};

static EditTitleWindow* gEditTitleWindow = nullptr;

EditTitleWindow::~EditTitleWindow() {
    EnableWindow(hwndOwner, TRUE);
    delete mainWindow;
    delete mainLayout;
}

void EditTitleWindow::CloseHandler(WindowCloseEvent* args) {
    WindowBase* w = (WindowBase*)gEditTitleWindow->mainWindow;
    CrashIf(w != args->w);
    gEditTitleWindow->onFinished(nullptr);
    delete gEditTitleWindow;
    gEditTitleWindow = nullptr;
}

void EditTitleWindow::KeyDownHandler(KeyEvent* args) {
    // TODO: I want Tab to navigate focus between elements
    // dbglogf("KeyDown: %d\n", args->keyVirtCode);
}

void EditTitleWindow::SizeHandler(SizeEvent* args) {
    int dx = args->dx;
    int dy = args->dy;
    HWND hwnd = args->hwnd;
    if (dx == 0 || dy == 0) {
        return;
    }
    Size windowSize{dx, dy};
    auto c = Tight(windowSize);
    auto size = mainLayout->Layout(c);
    Point min{0, 0};
    Point max{size.Width, size.Height};
    Rect bounds{min, max};
    mainLayout->SetBounds(bounds);
    InvalidateRect(hwnd, nullptr, false);
    args->didHandle = true;
}

void EditTitleWindow::ButtonOkHandler() {
    TocEditArgs* res = new TocEditArgs();
    res->title = str::Dup(editTitle->GetText());
    res->bold = checkboxBold->IsChecked();
    res->italic = checkboxItalic->IsChecked();

    std::string_view colorStr = editColor->GetText();
    // if invalid color value, preserve the orignal
    res->color = args->color;
    ParseColor(&res->color, colorStr);
    gEditTitleWindow->onFinished(res);
    delete res;

    delete gEditTitleWindow;
    gEditTitleWindow = nullptr;
}

void EditTitleWindow::ButtonCancelHandler() {
    gEditTitleWindow->onFinished(nullptr);
    delete gEditTitleWindow;
    gEditTitleWindow = nullptr;
}

static void createMainLayout(EditTitleWindow* win) {
    HWND parent = win->mainWindow->hwnd;
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        auto s = new StaticCtrl(parent);
        s->SetText("Title:");
        s->Create();
        auto l = NewStaticLayout(s);
        vbox->addChild(l);
    }

    {
        auto e = new EditCtrl(parent);
        win->editTitle = e;
        e->SetCueText("Title");
        e->SetText(win->args->title.as_view());
        e->Create();
        auto l = NewEditLayout(e);
        vbox->addChild(l);
    }

    // TODO: make this in a hbox, maybe
    {
        auto c = new CheckboxCtrl(parent);
        win->checkboxBold = c;
        c->SetText("bold");
        c->Create();
        c->SetIsChecked(win->args->bold);
        auto l = NewCheckboxLayout(c);
        vbox->addChild(l);
    }

    {
        auto c = new CheckboxCtrl(parent);
        win->checkboxItalic = c;
        c->SetText("italic");
        c->Create();
        c->SetIsChecked(win->args->italic);
        auto l = NewCheckboxLayout(c);
        vbox->addChild(l);
    }

    {
        auto s = new StaticCtrl(parent);
        s->SetText("Color:");
        s->Create();
        auto l = NewStaticLayout(s);
        vbox->addChild(l);
    }
    {
        auto e = new EditCtrl(parent);
        win->editColor = e;
        e->SetCueText("Color");
        e->Create();

        str::Str colorStr;
        if (win->args->color != ColorUnset) {
            SerializeColor(win->args->color, colorStr);
        }

        e->SetText(colorStr.as_view());
        auto l = NewEditLayout(e);
        vbox->addChild(l);
    }

    // TODO: make in a hbox
    {
        auto buttons = new HBox();
        buttons->alignMain = MainAxisAlign::MainEnd;
        buttons->alignCross = CrossAxisAlign::CrossStart;

        {
            auto b = new ButtonCtrl(parent);
            b->SetText("Cancel");
            b->onClicked = std::bind(&EditTitleWindow::ButtonCancelHandler, win);
            b->Create();
            auto l = NewButtonLayout(b);
            buttons->addChild(l);
        }

        {
            auto b = new ButtonCtrl(parent);
            b->SetText("Ok");
            b->onClicked = std::bind(&EditTitleWindow::ButtonOkHandler, win);
            b->Create();
            auto l = NewButtonLayout(b);
            buttons->addChild(l);
        }
        vbox->addChild(buttons);
    }

    auto* padding = new Padding();
    padding->insets = DefaultInsets();
    padding->child = vbox;

    win->mainLayout = padding;
}

static EditTitleWindow* createEditTitleWindow(HWND hwndOwner, TocEditArgs* args,
                                              const TocEditFinishedHandler& onFinished) {
    auto win = new EditTitleWindow();
    win->hwndOwner = hwndOwner;
    win->args = args;
    win->onFinished = onFinished;

    auto w = new Window();
    // remove minimize / maximize buttons from default style
    w->dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;

    w->backgroundColor = MkRgb((u8)0xee, (u8)0xee, (u8)0xee);
    w->SetTitle("Edit title");
    int dx = DpiScale(320);
    int dy = DpiScale(228);
    w->initialSize = {dx, dy};
    PositionCloseTo(w, hwndOwner);
    // SIZE winSize = {dx, dy};
    // LimitWindowSizeToScreen(nullptr, winSize);
    // w->initialSize = {winSize.cx, winSize.cy};
    bool ok = w->Create();
    CrashIf(!ok);

    // win->hwnd = w->hwnd;

    w->onClose = std::bind(&EditTitleWindow::CloseHandler, win, _1);
    w->onSize = std::bind(&EditTitleWindow::SizeHandler, win, _1);
    w->onKeyDown = std::bind(&EditTitleWindow::KeyDownHandler, win, _1);

    win->mainWindow = w;
    createMainLayout(win);
    win->editTitle->SetFocus();
    w->SetIsVisible(true);

    return win;
}

bool StartTocEditTitle(HWND hwndOwner, TocEditArgs* args, const TocEditFinishedHandler& onFinished) {
    CrashIf(gEditTitleWindow);
    EnableWindow(hwndOwner, FALSE);
    gEditTitleWindow = createEditTitleWindow(hwndOwner, args, onFinished);

    if (!gEditTitleWindow) {
        return false;
    }

    return true;
}
