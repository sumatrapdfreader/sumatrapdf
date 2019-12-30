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

struct TocEditorWindow {
    Window* tocEditorWindow = nullptr;
    ILayout* tocEditorLayout = nullptr;
    TreeCtrl* treeCtrl = nullptr;
    TocEditorArgs* tocArgs = nullptr;

    ~TocEditorWindow();
    void OnWindowSize(SizeArgs*);
    void OnTreeItemChanged(TreeItemChangedArgs*);
};

TocEditorWindow::~TocEditorWindow() {
    delete tocEditorLayout;
    delete tocArgs;
    delete tocEditorWindow;
}

static TocEditorWindow* gWindow = nullptr;

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
    HWND hwnd = gWindow->tocEditorWindow->hwnd;
    MessageBoxA(hwnd, "Not yet implemented!", "Information", MB_OK | MB_ICONINFORMATION);
}

static void AddPdf() {
    MessageNYI();
}

// in TableOfContents.cpp
extern void ShowExportedBookmarksMsg(const char* path);

static void SaveVirtual() {
    TocEditorArgs* tocArgs = gWindow->tocArgs;
    char* path = tocArgs->bookmarks[0]->filePath;

    str::WStr pathw = strconv::Utf8ToWchar(path);
    pathw.Append(L".vbkm");
    WCHAR dstFileName[MAX_PATH];
    str::BufSet(&(dstFileName[0]), dimof(dstFileName), pathw.Get());

    HWND hwnd = gWindow->tocEditorWindow->hwnd;

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
    AutoFree patha = strconv::WstrToUtf8(dstFileName);
    ok = ExportBookmarksToFile(tocArgs->bookmarks, patha);
    if (!ok) {
        return;
    }
    ShowExportedBookmarksMsg(patha);
}

static void Exit() {
    gWindow->tocEditorWindow->Close();
}

static ILayout* CreateMainLayout(HWND hwnd, TreeModel* tm) {
    auto* right = new VBox();

    right->alignMain = MainAxisAlign::MainStart;
    right->alignCross = CrossAxisAlign::CrossCenter;
    {
        auto [l, b] = CreateButtonLayout(hwnd, "Add PDF", AddPdf);
        right->addChild(l);
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Save As Virtual PDF", SaveVirtual);
        right->addChild(l);
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Exit", Exit);
        right->addChild(l);
    }

    auto* hbox = new HBox();
    hbox->alignMain = MainAxisAlign::MainStart;
    hbox->alignCross = CrossAxisAlign::Stretch;

    auto* tree = new TreeCtrl(hwnd);
    tree->withCheckboxes = true;
    bool ok = tree->Create(L"tree");
    CrashIf(!ok);
    tree->SetTreeModel(tm);
    // tree->idealSize = {80, 640};

    gWindow->treeCtrl = tree;
    auto treeLayout = NewTreeLayout(tree);

    hbox->addChild(treeLayout, 4);
    hbox->addChild(right, 1);

    auto* padding = new Padding();
    padding->insets = DefaultInsets();
    padding->child = hbox;
    return padding;
}

void TocEditorWindow::OnWindowSize(SizeArgs* args) {
    int dx = args->dx;
    int dy = args->dy;
    HWND hwnd = args->hwnd;
    if (dx == 0 || dy == 0) {
        return;
    }
    Size windowSize{dx, dy};
    auto c = Tight(windowSize);
    auto size = tocEditorLayout->Layout(c);
    Point min{0, 0};
    Point max{size.Width, size.Height};
    Rect bounds{min, max};
    tocEditorLayout->SetBounds(bounds);
    InvalidateRect(hwnd, nullptr, false);
    args->didHandle = true;
}

static void OnWindowDestroyed(WindowDestroyedArgs*) {
    delete gWindow;
    gWindow = nullptr;
}

void TocEditorWindow::OnTreeItemChanged(TreeItemChangedArgs* args) {
    logf("onTreeItemChanged\n");
}

// in TableOfContents.cpp
extern void OnDocTocCustomDraw(TreeItemCustomDrawArgs* args);

void StartTocEditor(TocEditorArgs* args) {
    if (gWindow != nullptr) {
        // TODO: maybe allow multiple windows
        gWindow->tocEditorWindow->onDestroyed = nullptr;
        delete gWindow;
        gWindow = nullptr;
    }

    gWindow = new TocEditorWindow();
    gWindow->tocArgs = args;
    auto w = new Window();
    w->backgroundColor = MkRgb((u8)0xee, (u8)0xee, (u8)0xee);
    w->SetTitle("Table of content editor");
    w->initialPos = {100, 100, 100 + 640, 100 + 800};
    bool ok = w->Create();
    CrashIf(!ok);

    // TODO:  if more than 1, create a combined TreeModel
    auto tm = args->bookmarks[0]->toc;
    gWindow->tocEditorLayout = CreateMainLayout(w->hwnd, tm);

    using namespace std::placeholders;
    w->onSize = std::bind(&TocEditorWindow::OnWindowSize, gWindow, _1);
    w->onDestroyed = OnWindowDestroyed;

    gWindow->treeCtrl->onTreeItemChanged = std::bind(&TocEditorWindow::OnTreeItemChanged, gWindow, _1);
    gWindow->treeCtrl->onTreeItemCustomDraw = OnDocTocCustomDraw;

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->SetIsVisible(true);
    gWindow->tocEditorWindow = w;
}
