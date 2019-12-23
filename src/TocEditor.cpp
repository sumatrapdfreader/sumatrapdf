/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

#include "wingui/WinGui.h"
#include "wingui/TreeModel.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TreeCtrl.h"
#include "wingui/ButtonCtrl.h"

#include "TocEditor.h"

static Window* gTocEditorWindow = nullptr;
static ILayout* gTocEditorLayout = nullptr;

static std::tuple<ILayout*, ButtonCtrl*> CreateButtonLayout(HWND parent, std::string_view s, OnClicked onClicked) {
    auto b = new ButtonCtrl(parent);
    b->OnClicked = onClicked;
    b->SetText(s);
    b->Create();
    return {NewButtonLayout(b), b};
}

static void NoOpFunc() {
}

static ILayout* CreateMainLayout(HWND hwnd) {
    auto* vbox = new VBox();

    vbox->alignMain = MainAxisAlign::MainCenter;
    vbox->alignCross = CrossAxisAlign::CrossCenter;
    {
        auto [l, b] = CreateButtonLayout(hwnd, "Button 1", NoOpFunc);
        vbox->addChild(l);
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Button 2", NoOpFunc);
        vbox->addChild(l);
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Button 3", NoOpFunc);
        vbox->addChild(l);
    }

    auto* padding = new Padding();
    padding->child = vbox;
    padding->insets = DefaultInsets();
    return padding;
}

void StartTocEditor(TreeModel* tm) {
    UNUSED(tm);
    if (gTocEditorWindow != nullptr) {
        gTocEditorWindow->onDestroyed = nullptr;
        delete gTocEditorWindow;
        delete gTocEditorLayout;
    }

    auto w = new Window();
    w->backgroundColor = MkRgb((u8)0xae, (u8)0xae, (u8)0xae);
    w->SetTitle("Table of contest editor");
    w->initialPos = {100, 100, 100 + 640, 100 + 800};
    bool ok = w->Create();
    CrashIf(!ok);

    gTocEditorLayout = CreateMainLayout(w->hwnd);
    w->onSize = [](HWND hwnd, int dx, int dy, WPARAM resizeType) {
        UNUSED(hwnd);
        UNUSED(resizeType);
        if (dx == 0 || dy == 0) {
            return;
        }
        // auto c = Loose(Size{dx, dy});
        Size windowSize{dx, dy};
        auto c = Tight(windowSize);
        auto size = gTocEditorLayout->Layout(c);
        Point min{0, 0};
        Point max{size.Width, size.Height};
        Rect bounds{min, max};
        gTocEditorLayout->SetBounds(bounds);
        InvalidateRect(hwnd, nullptr, false);
    };
    w->onDestroyed = [](WindowDestroyedArgs*) {
        delete gTocEditorWindow;
        gTocEditorWindow = nullptr;
        delete gTocEditorLayout;
        gTocEditorLayout = nullptr;
    };

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->SetIsVisible(true);
    gTocEditorWindow = w;
}
