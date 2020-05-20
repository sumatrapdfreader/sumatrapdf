/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/Log.h"
#include "utils/LogDbg.h"
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

#include "Annotation.h"
#include "EngineBase.h"

#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "SettingsStructs.h"
#include "WindowInfo.h"
#include "ParseBKM.h"
#include "TocEditor.h"
#include "TocEditTitle.h"

using std::placeholders::_1;

struct EditTitleWindow {
    // we own this
    TocEditArgs* args = nullptr;

    HWND hwndOwner = nullptr;
    TreeCtrl* treeCtrl = nullptr;
    TocItem* tocItem = nullptr;

    Window* mainWindow = nullptr;
    ILayout* mainLayout = nullptr;
    EditCtrl* editTitle = nullptr;
    CheckboxCtrl* checkboxItalic = nullptr;
    CheckboxCtrl* checkboxBold = nullptr;
    EditCtrl* editColor = nullptr;
    EditCtrl* editPage = nullptr;
    ButtonCtrl* buttonCancel = nullptr;

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
    delete args;
    delete mainWindow;
    delete mainLayout;
}

void EditTitleWindow::CloseHandler(WindowCloseEvent* ev) {
    WindowBase* w = (WindowBase*)gEditTitleWindow->mainWindow;
    CrashIf(w != ev->w);
    gEditTitleWindow->onFinished(nullptr);
    delete gEditTitleWindow;
    gEditTitleWindow = nullptr;
}

void EditTitleWindow::KeyDownHandler(KeyEvent* ev) {
    UNUSED(ev);
    // TODO: I want Tab to navigate focus between elements
    // dbglogf("KeyDown: %d\n", ev->keyVirtCode);
}

void EditTitleWindow::SizeHandler(SizeEvent* ev) {
    int dx = ev->dx;
    int dy = ev->dy;
    HWND hwnd = ev->hwnd;
    if (dx == 0 || dy == 0) {
        return;
    }
    ev->didHandle = true;
    InvalidateRect(hwnd, nullptr, false);
    if (mainLayout->lastBounds.EqSize(dx, dy)) {
        // avoid un-necessary layout
        return;
    }
    LayoutToSize(mainLayout, {dx, dy});
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

    int nPage = 0;
    int nPages = args->nPages;
    if (nPages > 0) {
        std::string_view pageStr = editPage->GetText();
        str::Parse(pageStr.data(), "%d", &nPage);
        if (nPage < 1 || nPage > nPages) {
            nPage = 0;
        }
    }
    res->page = nPage;

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
        s->SetText("&Title:");
        s->Create();
        auto l = NewLabelLayout(s);
        vbox->AddChild(l);
    }

    {
        auto e = new EditCtrl(parent);
        e->dwStyle |= WS_GROUP;
        win->editTitle = e;
        e->SetCueText("Title");
        e->SetText(win->args->title.as_view());
        e->Create();
        auto l = NewEditLayout(e);
        vbox->AddChild(l);
    }

    int nPages = win->args->nPages;
    if (nPages > 0) {
        {
            auto s = new StaticCtrl(parent);
            if (nPages == 0) {
                s->SetText("&Page");
            } else {
                AutoFreeStr pageStr = str::Format("&Page (1-%d)", nPages);
                s->SetText(pageStr.as_view());
            }
            s->Create();
            auto l = NewLabelLayout(s);
            vbox->AddChild(l);
        }
        {
            auto e = new EditCtrl(parent);
            win->editPage = e;
            e->SetCueText("Page");
            e->Create();

            int nPage = win->args->page;
            if (nPage != 0) {
                AutoFreeStr pageStr = str::Format("%d", nPage);
                e->SetText(pageStr.as_view());
            }
            auto l = NewEditLayout(e);
            vbox->AddChild(l);
        }
    }

    // TODO: make this in a hbox, maybe
    {
        auto c = new CheckboxCtrl(parent);
        win->checkboxBold = c;
        c->SetText("&Bold");
        c->Create();
        c->SetIsChecked(win->args->bold);
        auto l = NewCheckboxLayout(c);
        vbox->AddChild(l);
    }

    {
        auto c = new CheckboxCtrl(parent);
        win->checkboxItalic = c;
        c->SetText("&Italic");
        c->Create();
        c->SetIsChecked(win->args->italic);
        auto l = NewCheckboxLayout(c);
        vbox->AddChild(l);
    }

    {
        auto s = new StaticCtrl(parent);
        s->SetText("&Color:");
        s->Create();
        auto l = NewLabelLayout(s);
        vbox->AddChild(l);
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
        vbox->AddChild(l);
    }

    {
        auto buttons = new HBox();
        buttons->alignMain = MainAxisAlign::SpaceBetween;
        buttons->alignCross = CrossAxisAlign::CrossStart;

        {
            auto b = new ButtonCtrl(parent);
            b->SetText("Cance&l");
            b->onClicked = std::bind(&EditTitleWindow::ButtonCancelHandler, win);
            b->Create();
            win->buttonCancel = b;
            auto l = NewButtonLayout(b);
            buttons->AddChild(l);
        }

        {
            auto b = new ButtonCtrl(parent);
            b->isDefault = true;
            b->SetText("&Ok");
            b->onClicked = std::bind(&EditTitleWindow::ButtonOkHandler, win);
            b->Create();
            auto l = NewButtonLayout(b);
            buttons->AddChild(l);
        }
        vbox->AddChild(buttons);
    }

    auto padding = new Padding(vbox, DpiScaledInsets(parent, 8));
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
    w->isDialog = true;
    w->backgroundColor = MkRgb((u8)0xee, (u8)0xee, (u8)0xee);
    w->SetTitle("Edit title");
    int dx = DpiScale(340);
    int dy = DpiScale(258);
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
    LayoutAndSizeToContent(win->mainLayout, 340, 0, w->hwnd);
    w->SetIsVisible(true);

    win->editTitle->SetFocus();

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
