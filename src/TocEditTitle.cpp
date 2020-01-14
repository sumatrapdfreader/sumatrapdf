/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/Log.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "wingui/WinGui.h"
#include "wingui/TreeModel.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TreeCtrl.h"
#include "wingui/ButtonCtrl.h"

#include "EngineBase.h"

#include "ParseBKM.h"
#include "TocEditor.h"
#include "TocEditTitle.h"

struct EditTitleWindow {
    Window* mainWindow = nullptr;
    ILayout* layout = nullptr;

    EditTitleWindow() = default;
    ~EditTitleWindow();
};

EditTitleWindow::~EditTitleWindow() {
    delete mainWindow;
    delete layout;
}

static EditTitleWindow* gEditTitleWindow = nullptr;

static void createMainLayout(EditTitleWindow* win) {

}

static EditTitleWindow* createEditTitleWindow() {
    auto win = new EditTitleWindow();
    auto w = new Window();
    w->backgroundColor = MkRgb((u8)0xee, (u8)0xee, (u8)0xee);
    w->SetTitle("Table of content editor");
    w->initialSize = {DpiScale(480), DpiScale(600)};
    //PositionCloseTo(w, args->hwndRelatedTo);
    SIZE winSize = {w->initialSize.Width, w->initialSize.Height};
    //LimitWindowSizeToScreen(nullptr, winSize);
    w->initialSize = {winSize.cx, winSize.cy};
    bool ok = w->Create();
    CrashIf(!ok);

    win->mainWindow = w;
    //win->hwnd = w->hwnd;

    createMainLayout(win);

    return nullptr;
}

bool StartTocEditTitle(HWND hwndOwner, TocItem* ti) {
    CrashIf(gEditTitleWindow);
    EnableWindow(hwndOwner, FALSE);
    gEditTitleWindow = createEditTitleWindow();

    defer {
        EnableWindow(hwndOwner, TRUE);
        delete gEditTitleWindow;
    };

    if (!gEditTitleWindow) {
        return false;
    }

    return true;
}
