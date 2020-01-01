/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/Log.h"

#include "wingui/WinGui.h"
#include "wingui/TreeModel.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TreeCtrl.h"
#include "wingui/ButtonCtrl.h"

#include "EngineBase.h"
#include "EngineManager.h"

#include "ParseBKM.h"
#include "TocEditor.h"

struct TocEditorWindow {
    Window* tocEditorWindow = nullptr;
    ILayout* tocEditorLayout = nullptr;
    TocEditorArgs* tocArgs = nullptr;

    // not owned by us but by tocEditorLayout
    TreeCtrl* treeCtrl = nullptr;

    ~TocEditorWindow();
    void OnWindowSize(SizeArgs*);
    void OnTreeItemChanged(TreeItemChangedArgs*);
};

TocEditorWindow::~TocEditorWindow() {
    delete treeCtrl->treeModel;

    // deletes all windows owned by layout
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

void MessageNYI() {
    HWND hwnd = gWindow->tocEditorWindow->hwnd;
    MessageBoxA(hwnd, "Not yet implemented!", "Information", MB_OK | MB_ICONINFORMATION);
}

void ShowErrorMessage(const char* msg) {
    HWND hwnd = gWindow->tocEditorWindow->hwnd;
    MessageBoxA(hwnd, msg, "Error", MB_OK | MB_ICONERROR);
}

static void SetTreeModel() {
    // TODO:  if more than 1, create a combined TreeModel
    TreeCtrl* treeCtrl = gWindow->treeCtrl;
    auto& bookmarks = gWindow->tocArgs->bookmarks;
    delete treeCtrl->treeModel;
    treeCtrl->treeModel = nullptr;
    if (bookmarks.size() == 1) {
        auto tm = bookmarks[0]->toc;
        auto tmCopy = CloneDocTocTree(tm);
        treeCtrl->SetTreeModel(tmCopy);
        return;
    }
    DocTocItem* root = nullptr;
    DocTocItem* curr = nullptr;
    for (auto&& bkm : bookmarks) {
        DocTocItem* i = new DocTocItem();
        i->child = CloneDocTocItemRecur(bkm->toc->root);
        AutoFreeWstr path = strconv::Utf8ToWstr(bkm->filePath.get());
        const WCHAR* name = path::GetBaseNameNoFree(path);
        i->title = str::Dup(name);
        if (root == nullptr) {
            root = i;
            curr = root;
        } else {
            curr->next = i;
            curr = i;
        }
    }
    DocTocTree* tm = new DocTocTree();
    tm->root = root;
    treeCtrl->SetTreeModel(tm);
}

static void AddPdf() {
    HWND hwnd = gWindow->tocEditorWindow->hwnd;

    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;

    ofn.lpstrFilter = L".pdf\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;

    // OFN_ENABLEHOOK disables the new Open File dialog under Windows Vista
    // and later, so don't use it and just allocate enough memory to contain
    // several dozen file paths and hope that this is enough
    // TODO: Use IFileOpenDialog instead (requires a Vista SDK, though)
    ofn.nMaxFile = MAX_PATH * 2;
    AutoFreeWstr file = AllocArray<WCHAR>(ofn.nMaxFile);
    ofn.lpstrFile = file;

    if (!GetOpenFileNameW(&ofn)) {
        return;
    }
    WCHAR* filePath = ofn.lpstrFile;
    logf(L"fileName: %s\n", filePath);
    EngineBase* engine = EngineManager::CreateEngine(filePath);
    if (!engine) {
        ShowErrorMessage("Failed to open a file!");
        return;
    }
    DocTocTree* tocTree = engine->GetTocTree();
    if (nullptr == tocTree) {
        ShowErrorMessage("File doesn't have Table of content");
        return;
    }
    tocTree = CloneDocTocTree(tocTree);
    delete engine;
    Bookmarks* bookmarks = new Bookmarks();
    bookmarks->toc = tocTree;
    bookmarks->filePath = str::Dup(tocTree->filePath);
    gWindow->tocArgs->bookmarks.push_back(bookmarks);
    SetTreeModel();
}

// in TableOfContents.cpp
extern void ShowExportedBookmarksMsg(const char* path);

static void SaveVirtual() {
    TocEditorArgs* tocArgs = gWindow->tocArgs;
    char* path = tocArgs->bookmarks[0]->filePath;

    str::WStr pathw = strconv::Utf8ToWstr(path);
    pathw.Append(L".vbkm");
    WCHAR dstFileName[MAX_PATH];
    str::BufSet(&(dstFileName[0]), dimof(dstFileName), pathw.Get());

    HWND hwnd = gWindow->tocEditorWindow->hwnd;

    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = L".vbkm\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"vbkm";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    // note: explicitly not setting lpstrInitialDir so that the OS
    // picks a reasonable default (in particular, we don't want this
    // in plugin mode, which is likely the main reason for saving as...)

    bool ok = GetSaveFileNameW(&ofn);
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

static ILayout* CreateMainLayout(HWND hwnd) {
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

    gWindow->tocEditorLayout = CreateMainLayout(w->hwnd);

    using namespace std::placeholders;
    w->onSize = std::bind(&TocEditorWindow::OnWindowSize, gWindow, _1);
    w->onDestroyed = OnWindowDestroyed;

    gWindow->treeCtrl->onTreeItemChanged = std::bind(&TocEditorWindow::OnTreeItemChanged, gWindow, _1);
    gWindow->treeCtrl->onTreeItemCustomDraw = OnDocTocCustomDraw;

    SetTreeModel();
    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->SetIsVisible(true);
    gWindow->tocEditorWindow = w;
}
