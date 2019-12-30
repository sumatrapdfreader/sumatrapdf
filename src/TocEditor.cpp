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

#include "ParseBKM.h"
#include "TocEditor.h"

// TODO: put state in a TocEditorWindow

static Window* gTocEditorWindow = nullptr;
static ILayout* gTocEditorLayout = nullptr;
static TreeCtrl* gTreeCtrl = nullptr;
static TocEditorArgs* gArgs = nullptr;

TocEditorArgs::~TocEditorArgs() {
    DeleteVecMembers(bookmarks);
}

static std::tuple<ILayout*, ButtonCtrl*> CreateButtonLayout(HWND parent, std::string_view s, OnClicked onClicked) {
    auto b = new ButtonCtrl(parent);
    b->OnClicked = onClicked;
    b->SetText(s);
    b->Create();
    return {NewButtonLayout(b), b};
}

static void MessageNYI() {
    MessageBoxA(gTocEditorWindow->hwnd, "Not yet implemented!", "Information", MB_OK | MB_ICONINFORMATION);
}

static void AddPdf() {
    MessageNYI();
}

static void SaveVirtual() {
    HWND hwnd = gTocEditorWindow->hwnd;

    WCHAR dstFileName[MAX_PATH];

    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = L".vbkm\0\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"vbkm";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    // note: explicitly not setting lpstrInitialDir so that the OS
    // picks a reasonable default (in particular, we don't want this
    // in plugin mode, which is likely the main reason for saving as...)

    bool ok = GetSaveFileName(&ofn);
    if (!ok) {
        return;
    }
}

static void Exit() {
    gTocEditorWindow->Close();
}

static ILayout* CreateMainLayout(HWND hwnd, TreeModel* tm) {
    auto* vbox = new VBox();

    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::CrossCenter;
    {
        auto [l, b] = CreateButtonLayout(hwnd, "Add PDF", AddPdf);
        vbox->addChild(l);
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Save As Virtual PDF", SaveVirtual);
        vbox->addChild(l);
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Exit", Exit);
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

static void DeleteTocEditorWindow() {
    delete gTocEditorWindow;
    gTocEditorWindow = nullptr;
    delete gTocEditorLayout;
    gTocEditorLayout = nullptr;
    delete gArgs;
    gArgs = nullptr;
}

static void OnWindowDestroyed(WindowDestroyedArgs*) {
    DeleteTocEditorWindow();
}

static void onTreeItemChanged(TreeItemChangedArgs* args) {
    logf("onTreeItemChanged\n");
}

// in TableOfContents.cpp
extern void OnDocTocCustomDraw(TreeItemCustomDrawArgs* args);

void StartTocEditor(TocEditorArgs* args) {
    if (gTocEditorWindow != nullptr) {
        // TODO: maybe allow multiple windows
        gTocEditorWindow->onDestroyed = nullptr;
        DeleteTocEditorWindow();
    }

    gArgs = args;

    // TODO: only for now. Maybe rename DocTocItem::isChecked
    // => DocTocItem::isUnchecked so that default state is what we want
    for (auto&& bkm : args->bookmarks) {
        auto tm = bkm->toc;
        VisitTreeModelItems(tm, [](TreeItem* ti) -> bool {
            auto* docItem = (DocTocItem*)ti;
            docItem->isChecked = true;
            return true;
        });
    }

    auto w = new Window();
    w->backgroundColor = MkRgb((u8)0xee, (u8)0xee, (u8)0xee);
    w->SetTitle("Table of content editor");
    w->initialPos = {100, 100, 100 + 640, 100 + 800};
    bool ok = w->Create();
    CrashIf(!ok);

    // TODO:  if more than 1, create a combined TreeModel
    auto tm = args->bookmarks[0]->toc;
    gTocEditorLayout = CreateMainLayout(w->hwnd, tm);

    w->onSize = OnWindowSize;
    w->onDestroyed = OnWindowDestroyed;
    gTreeCtrl->onTreeItemChanged = onTreeItemChanged;
    gTreeCtrl->onTreeItemCustomDraw = OnDocTocCustomDraw;

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->SetIsVisible(true);
    gTocEditorWindow = w;
}
