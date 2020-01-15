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
#include "wingui/TreeCtrl.h"
#include "wingui/EditCtrl.h"
#include "wingui/CheckboxCtrl.h"
#include "wingui/ButtonCtrl.h"

#include "EngineBase.h"

#include "ParseBKM.h"
#include "TocEditor.h"
#include "TocEditTitle.h"

struct EditTitleWindow {
    Window* mainWindow = nullptr;
    ILayout* mainLayout = nullptr;
    EditCtrl* editTitle = nullptr;

    AutoFree initialTitle;
    bool initialIsBold = false;
    bool initialIsItalic = false;
    AutoFree initialColor;

    EditTitleWindow() = default;
    ~EditTitleWindow();
    void CloseHandler(WindowCloseArgs*);
    void SizeHandler(SizeArgs*);
};

static EditTitleWindow* gEditTitleWindow = nullptr;

EditTitleWindow::~EditTitleWindow() {
    delete mainWindow;
    delete mainLayout;
}

void EditTitleWindow::CloseHandler(WindowCloseArgs* args) {
    WindowBase* w = (WindowBase*)gEditTitleWindow->mainWindow;
    CrashIf(w != args->w);
    delete gEditTitleWindow;
    gEditTitleWindow = nullptr;
}

void EditTitleWindow::SizeHandler(SizeArgs* args) {
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


static void createMainLayout(EditTitleWindow* win) {
    HWND parent = win->mainWindow->hwnd;
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        auto e = new EditCtrl(parent);
        e->SetCueText("Title");
        e->SetText(win->initialTitle.as_view());
        e->Create();
        auto l = NewEditLayout(e);
        vbox->addChild(l);
    }

    // TODO: make this in a hbox
    {
        auto c = new CheckboxCtrl(parent);
        c->SetText("bold");
        c->SetIsChecked(win->initialIsBold);
        c->Create();
        auto l = NewCheckboxLayout(c);
        vbox->addChild(l);
    }

    {
        auto c = new CheckboxCtrl(parent);
        c->SetText("italic");
        c->SetIsChecked(win->initialIsItalic);
        c->Create();
        auto l = NewCheckboxLayout(c);
        vbox->addChild(l);
    }

    {
        auto e = new EditCtrl(parent);
        e->SetCueText("Color");
        e->SetText(win->initialColor.as_view());
        e->Create();
        auto l = NewEditLayout(e);
        vbox->addChild(l);
    }

    // TODO: make in a hbox
    {
        auto b = new ButtonCtrl(parent);
        b->SetText("Cancel");
        b->Create();
        auto l = NewButtonLayout(b);
        vbox->addChild(l);
    }

    {
        auto b = new ButtonCtrl(parent);
        b->SetText("Ok");
        b->Create();
        auto l = NewButtonLayout(b);
        vbox->addChild(l);
    }

    auto* padding = new Padding();
    padding->insets = DefaultInsets();
    padding->child = vbox;

    win->mainLayout = padding;
}

static EditTitleWindow* createEditTitleWindow(TocItem* ti) {
    auto win = new EditTitleWindow();
    win->initialIsBold = bit::IsSet(ti->fontFlags, fontBitBold);
    win->initialIsItalic = bit::IsSet(ti->fontFlags, fontBitItalic);
    win->initialTitle = strconv::WstrToUtf8(ti->title);
    if (ti->color != ColorUnset) {
        str::Str s;
        SerializeColor(ti->color, s);
        win->initialColor = s.StealData();
    }

    auto w = new Window();
    w->backgroundColor = MkRgb((u8)0xee, (u8)0xee, (u8)0xee);
    w->SetTitle("Edit title");
    w->initialSize = {DpiScale(480), DpiScale(600)};
    // PositionCloseTo(w, args->hwndRelatedTo);
    SIZE winSize = {w->initialSize.Width, w->initialSize.Height};
    // LimitWindowSizeToScreen(nullptr, winSize);
    w->initialSize = {winSize.cx, winSize.cy};
    bool ok = w->Create();
    CrashIf(!ok);

    // win->hwnd = w->hwnd;

    using std::placeholders::_1;
    w->onClose = std::bind(&EditTitleWindow::CloseHandler, win, _1);
    w->onSize = std::bind(&EditTitleWindow::SizeHandler, win, _1);

    win->mainWindow = w;
    createMainLayout(win);
    w->SetIsVisible(true);

    return win;
}

bool StartTocEditTitle(HWND hwndOwner, TocItem* ti) {
    CrashIf(gEditTitleWindow);
    EnableWindow(hwndOwner, FALSE);
    gEditTitleWindow = createEditTitleWindow(ti);

    defer {
        EnableWindow(hwndOwner, TRUE);
    };

    if (!gEditTitleWindow) {
        return false;
    }

    return true;
}
