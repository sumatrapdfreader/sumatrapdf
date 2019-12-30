/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Log.h"

#include "wingui/WinGui.h"
#include "wingui/TreeModel.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TreeCtrl.h"
#include "wingui/ButtonCtrl.h"

#include "EngineBase.h"

#include "TocEditor.h"

static Window* gTocEditorWindow = nullptr;
static ILayout* gTocEditorLayout = nullptr;
static TreeCtrl* gTreeCtrl = nullptr;

static std::tuple<ILayout*, ButtonCtrl*> CreateButtonLayout(HWND parent, std::string_view s, OnClicked onClicked) {
    auto b = new ButtonCtrl(parent);
    b->OnClicked = onClicked;
    b->SetText(s);
    b->Create();
    return {NewButtonLayout(b), b};
}

static void NoOpFunc() {
}

static ILayout* CreateMainLayout(HWND hwnd, TreeModel* tm) {
    auto* vbox = new VBox();

    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::CrossCenter;
    {
        auto [l, b] = CreateButtonLayout(hwnd, "Add PDF", NoOpFunc);
        vbox->addChild(l);
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Save PDF", NoOpFunc);
        vbox->addChild(l);
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Exit", NoOpFunc);
        vbox->addChild(l);
    }

    auto* hbox = new HBox();
    hbox->alignMain = MainAxisAlign::MainStart;
    hbox->alignCross = CrossAxisAlign::Stretch;

    auto* tree = new TreeCtrl(hwnd);
    tree->withCheckboxes = true;
    bool ok = tree->Create(L"tree");
    CrashIf(!ok);
    tree->SetTreeModel(tm);

    gTreeCtrl = tree;
    auto treeLayout = NewTreeLayout(tree);

    hbox->addChild(treeLayout, 3);
    hbox->addChild(vbox, 1);

    auto* padding = new Padding();
    padding->insets = DefaultInsets();
    padding->child = hbox;
    return padding;
}

static void OnWindowSize(SizeArgs* args) {
    int dx = args->dx;
    int dy = args->dy;
    HWND hwnd = args->hwnd;
    if (dx == 0 || dy == 0) {
        return;
    }
    Size windowSize{dx, dy};
    auto c = Tight(windowSize);
    auto size = gTocEditorLayout->Layout(c);
    Point min{0, 0};
    Point max{size.Width, size.Height};
    Rect bounds{min, max};
    gTocEditorLayout->SetBounds(bounds);
    InvalidateRect(hwnd, nullptr, false);
    args->didHandle = true;
}

static void OnWindowDestroyed(WindowDestroyedArgs*) {
    delete gTocEditorWindow;
    gTocEditorWindow = nullptr;
    delete gTocEditorLayout;
    gTocEditorLayout = nullptr;
}

static void onTreeItemChanged(TreeItemChangedArgs* args) {
    logf("onTreeItemChanged\n");
}

// in TableOfContents.cpp
extern void OnDocTocCustomDraw(TreeItemCustomDrawArgs* args);

// TODO: make a copy of tree model
void StartTocEditor(TreeModel* tm) {
    if (gTocEditorWindow != nullptr) {
        gTocEditorWindow->onDestroyed = nullptr;
        delete gTocEditorWindow;
        delete gTocEditorLayout;
    }

    VisitTreeModelItems(tm, [](TreeItem* ti) -> bool {
        auto* docItem = (DocTocItem*)ti;
        docItem->isChecked = true;
        return true;
    });

    auto w = new Window();
    w->backgroundColor = MkRgb((u8)0xee, (u8)0xee, (u8)0xee);
    w->SetTitle("Table of content editor");
    w->initialPos = {100, 100, 100 + 640, 100 + 800};
    bool ok = w->Create();
    CrashIf(!ok);

    gTocEditorLayout = CreateMainLayout(w->hwnd, tm);

    w->onSize = OnWindowSize;
    w->onDestroyed = OnWindowDestroyed;
    gTreeCtrl->onTreeItemChanged = onTreeItemChanged;
    gTreeCtrl->onTreeItemCustomDraw = OnDocTocCustomDraw;

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->SetIsVisible(true);
    w->SetIsVisible(true);
    gTocEditorWindow = w;
}
