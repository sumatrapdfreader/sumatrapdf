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
#include "EngineMulti.h"
#include "EngineManager.h"

#include "ParseBKM.h"
#include "TocEditTitle.h"
#include "TocEditor.h"

using std::placeholders::_1;

// in TableOfContents.cpp
extern void OnTocCustomDraw(TreeItemCustomDrawArgs* args);

struct TocEditorWindow {
    HWND hwnd = nullptr;
    Window* mainWindow = nullptr;
    ILayout* mainLayout = nullptr;
    TocEditorArgs* tocArgs = nullptr;

    // not owned by us but by mainLayout

    ButtonCtrl* btnAddPdf = nullptr;
    ButtonCtrl* btnRemovePdf = nullptr;
    ButtonCtrl* btnExit = nullptr;
    ButtonCtrl* btnSaveAsVirtual = nullptr;
    ILayout* layoutButtons = nullptr;

    TreeCtrl* treeCtrl = nullptr;
    TreeModel* treeModel = nullptr;

    bool canRemovePdf = false;

    ~TocEditorWindow();
    void SizeHandler(SizeArgs*);
    void CloseHandler(WindowCloseArgs*);
    void TreeItemChangedHandler(TreeItemChangedArgs*);
    void TreeItemSelectedHandler(TreeSelectionChangedArgs*);
    void TreeClickHandler(TreeClickArgs* args);
    void GetDispInfoHandler(TreeGetDispInfoArgs*);
    void TreeItemDragged(TreeItemDraggeddArgs*);
};

void TocEditorWindow::TreeItemDragged(TreeItemDraggeddArgs* args) {
    TocItem* dragged = (TocItem*)args->draggedItem;
    TocItem* dragTarget = (TocItem*)args->dragTargetItem;
    dbglogf("TreeItemDragged:");
    if (dragged != nullptr) {
        AutoFreeStr s = strconv::WstrToUtf8(dragged->title);
        dbglogf(" dragged: %s", s.get());
    }
    if (dragTarget != nullptr) {
        AutoFreeStr s = strconv::WstrToUtf8(dragTarget->title);
        dbglogf("  on: %s", s.get());
    }
    dbglogf("\n");
}

TocEditorWindow::~TocEditorWindow() {
    // TODO: delete the top but not children, because
    // they are not owned
    // delete treeModel;

    // deletes all controls owned by layout
    delete mainLayout;

    delete tocArgs;
    delete mainWindow;
}

static TocEditorWindow* gWindow = nullptr;

TocEditorArgs::~TocEditorArgs() {
    DeleteVecMembers(bookmarks);
}

void MessageNYI() {
    HWND hwnd = gWindow->mainWindow->hwnd;
    MessageBoxA(hwnd, "Not yet implemented!", "Information", MB_OK | MB_ICONINFORMATION);
}

void ShowErrorMessage(const char* msg) {
    HWND hwnd = gWindow->mainWindow->hwnd;
    MessageBoxA(hwnd, msg, "Error", MB_OK | MB_ICONERROR);
}

static void UpdateTreeModel(TocEditorWindow* w) {
    TreeCtrl* treeCtrl = w->treeCtrl;
    treeCtrl->Clear();

    // TODO: delete the first levels because we keep reference to them
    // delete w->treeModel;
    w->treeModel = nullptr;

    auto& bookmarks = w->tocArgs->bookmarks;

    TocItem* root = nullptr;
    TocItem* curr = nullptr;
    for (auto&& vbkm : bookmarks) {
        TocItem* ti = new TocItem();
        ti->isOpenDefault = true;
        AutoFreeWstr path = strconv::Utf8ToWstr(vbkm->filePath.as_view());
        const WCHAR* name = path::GetBaseNameNoFree(path);
        ti->title = str::Dup(name);
        ti->child = vbkm->toc->root;
        ti->child->parent = ti->child;

        CalcEndPageNo(ti->child, vbkm->nPages);

        if (!root) {
            root = ti;
            curr = root;
        } else {
            curr->next = ti;
            curr = ti;
        }
    }
    w->treeModel = new TocTree(root);
    treeCtrl->SetTreeModel(w->treeModel);
}

static void AddPdf() {
    TocEditorWindow* w = gWindow;
    HWND hwnd = w->mainWindow->hwnd;

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

    EngineBase* engine = EngineManager::CreateEngine(filePath);
    if (!engine) {
        ShowErrorMessage("Failed to open a file!");
        return;
    }
    TocTree* tocTree = engine->GetToc();
    if (nullptr == tocTree) {
        // TODO: maybe add a dummy entry for the first page
        // or make top-level act as first page destination
        ShowErrorMessage("File doesn't have Table of content");
        return;
    }
    tocTree = CloneTocTree(tocTree, false);
    int nPages = engine->PageCount();
    delete engine;
    VbkmForFile* bookmarks = new VbkmForFile();
    bookmarks->toc = tocTree;
    bookmarks->filePath = strconv::WstrToUtf8(filePath).data();
    bookmarks->nPages = nPages;
    w->tocArgs->bookmarks.push_back(bookmarks);

    UpdateTreeModel(w);
}

static void RemovePdf() {
    TocEditorWindow* w = gWindow;
    TreeItem* sel = w->treeCtrl->GetSelection();
    CrashIf(!sel);
    size_t n = w->tocArgs->bookmarks.size();
    CrashIf(n < 2);

    TocItem* di = (TocItem*)sel;
    CrashIf(di->Parent() != nullptr);
    WCHAR* toRemoveTitle = di->title;
    size_t toRemoveIdx = 0;
    VbkmForFile* bkmToRemove = nullptr;
    for (size_t i = 0; i < n; i++) {
        VbkmForFile* bkm = w->tocArgs->bookmarks[i];

        AutoFreeWstr path = strconv::Utf8ToWstr(bkm->filePath.get());
        const WCHAR* name = path::GetBaseNameNoFree(path);
        if (str::Eq(name, toRemoveTitle)) {
            toRemoveIdx = i;
            bkmToRemove = bkm;
            break;
        }
    }
    CrashIf(!bkmToRemove);
    w->tocArgs->bookmarks.RemoveAt(toRemoveIdx);
    delete bkmToRemove;
    UpdateTreeModel(w);
}

static void UpdateRemovePdfButtonStatus(TocEditorWindow* w) {
    TreeItem* sel = w->treeCtrl->GetSelection();
    bool isEnabled = false;
    if (sel) {
        TocItem* di = (TocItem*)sel;
        TreeItem* p = di->Parent();
        isEnabled = (p == nullptr); // enabled if top-level
    }
    // must have at least 2 PDFs to remove
    if (w->tocArgs->bookmarks.size() < 2) {
        isEnabled = 0;
    }
    w->btnRemovePdf->SetIsEnabled(isEnabled);
}

// in TableOfContents.cpp
extern void ShowExportedBookmarksMsg(const char* path);

static void SaveVirtual() {
    TocEditorArgs* tocArgs = gWindow->tocArgs;
    char* path = tocArgs->bookmarks[0]->filePath;

    str::WStr pathw = strconv::Utf8ToWstr(path);
    // if the source was .vbkm file, we over-write it by default
    // any other format, we add .vbkm extension by default
    if (!str::EndsWithI(path, ".vbkm")) {
        pathw.Append(L".vbkm");
    }
    WCHAR dstFileName[MAX_PATH]{0};
    str::BufSet(&(dstFileName[0]), dimof(dstFileName), pathw.Get());

    HWND hwnd = gWindow->mainWindow->hwnd;

    OPENFILENAME ofn{0};
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
    ok = ExportBookmarksToFile(tocArgs->bookmarks, "", patha);
    if (!ok) {
        return;
    }
    // ShowExportedBookmarksMsg(patha);
}

static void Exit() {
    gWindow->mainWindow->Close();
}

static void CreateButtonsLayout(TocEditorWindow* w) {
    HWND hwnd = w->hwnd;
    CrashIf(!hwnd);

    auto* buttons = new HBox();

    buttons->alignMain = MainAxisAlign::SpaceBetween;
    buttons->alignCross = CrossAxisAlign::CrossStart;
    {
        auto [l, b] = CreateButtonLayout(hwnd, "Add PDF", AddPdf);
        buttons->addChild(l);
        w->btnAddPdf = b;
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Remove PDF", RemovePdf);
        buttons->addChild(l);
        w->btnRemovePdf = b;
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Save As Virtual PDF", SaveVirtual);
        buttons->addChild(l);
        w->btnSaveAsVirtual = b;
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Exit", Exit);
        buttons->addChild(l);
        w->btnExit = b;
    }

    w->layoutButtons = buttons;
}

static void CreateMainLayout(TocEditorWindow* win) {
    HWND hwnd = win->hwnd;
    CrashIf(!hwnd);

    CreateButtonsLayout(win);

    auto* main = new VBox();
    main->alignMain = MainAxisAlign::MainStart;
    main->alignCross = CrossAxisAlign::Stretch;

    auto* tree = new TreeCtrl(hwnd);
    tree->supportDragDrop = true;
    int dx = DpiScale(80);
    int dy = DpiScale(120);
    tree->idealSize = {dx, dy};

    tree->withCheckboxes = true;
    tree->onTreeGetDispInfo = std::bind(&TocEditorWindow::GetDispInfoHandler, win, _1);

    bool ok = tree->Create(L"tree");
    CrashIf(!ok);

    tree->onTreeItemChanged = std::bind(&TocEditorWindow::TreeItemChangedHandler, win, _1);
    tree->onTreeItemCustomDraw = OnTocCustomDraw;
    tree->onTreeSelectionChanged = std::bind(&TocEditorWindow::TreeItemSelectedHandler, win, _1);
    tree->onTreeClick = std::bind(&TocEditorWindow::TreeClickHandler, win, _1);
    tree->onTreeItemDragged = std::bind(&TocEditorWindow::TreeItemDragged, win, _1);
    gWindow->treeCtrl = tree;
    auto treeLayout = NewTreeLayout(tree);

    main->addChild(treeLayout, 1);
    main->addChild(win->layoutButtons, 0);

    auto* padding = new Padding();
    padding->insets = DefaultInsets();
    padding->child = main;
    win->mainLayout = padding;
}

void TocEditorWindow::SizeHandler(SizeArgs* args) {
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

void TocEditorWindow::CloseHandler(WindowCloseArgs* args) {
    WindowBase* w = (WindowBase*)gWindow->mainWindow;
    CrashIf(w != args->w);
    delete gWindow;
    gWindow = nullptr;
}

void TocEditorWindow::TreeItemChangedHandler(TreeItemChangedArgs* args) {
    if (!args->checkedChanged) {
        return;
    }
    TocItem* ti = (TocItem*)args->treeItem;
    ti->isUnchecked = !args->newState.isChecked;
}

void TocEditorWindow::TreeItemSelectedHandler(TreeSelectionChangedArgs* args) {
    UNUSED(args);
    UpdateRemovePdfButtonStatus(gWindow);
}

void TocEditorWindow::GetDispInfoHandler(TreeGetDispInfoArgs* args) {
    args->didHandle = true;

    TocItem* ti = (TocItem*)args->treeItem;
    TVITEMEXW* tvitem = &args->dispInfo->item;
    CrashIf(tvitem->mask != TVIF_TEXT);

    size_t cchMax = tvitem->cchTextMax;
    CrashIf(cchMax < 32);

    int sno = ti->pageNo;
    if (sno <= 0) {
        str::BufSet(tvitem->pszText, cchMax, ti->title);
        return;
    }
    int eno = ti->endPageNo;
    WCHAR* s = nullptr;
    if (eno > sno) {
        s = str::Format(L"%s (pages %d-%d)", ti->title, sno, eno);
    } else {
        s = str::Format(L"%s (page %d)", ti->title, sno);
    }
    str::BufSet(tvitem->pszText, cchMax, s);
    str::Free(s);
}

void TocEditorWindow::TreeClickHandler(TreeClickArgs* args) {
    if (!args->isDblClick) {
        return;
    }
    if (!args->treeItem) {
        return;
    }

    args->didHandle = true;
    args->result = 1;

    TocItem* ti = (TocItem*)args->treeItem;
    StartTocEditTitle(mainWindow->hwnd, args->treeCtrl, ti);
}

void StartTocEditor(TocEditorArgs* args) {
    HWND hwndOwner = args->hwndRelatedTo;
    if (gWindow != nullptr) {
        // TODO: maybe allow multiple windows
        gWindow->mainWindow->onDestroy = nullptr;
        delete gWindow;
        gWindow = nullptr;
    }

    auto win = new TocEditorWindow();
    gWindow = win;
    win->tocArgs = args;
    auto w = new Window();
    w->backgroundColor = MkRgb((u8)0xee, (u8)0xee, (u8)0xee);
    w->SetTitle("Table of content editor");
    int dx = DpiScale(hwndOwner, 640);
    int dy = DpiScale(hwndOwner, 800);
    w->initialSize = {dx, dy};
    PositionCloseTo(w, args->hwndRelatedTo);
    SIZE winSize = {w->initialSize.Width, w->initialSize.Height};
    LimitWindowSizeToScreen(args->hwndRelatedTo, winSize);
    w->initialSize = {winSize.cx, winSize.cy};
    bool ok = w->Create();
    CrashIf(!ok);

    win->mainWindow = w;
    win->hwnd = w->hwnd;

    CreateMainLayout(gWindow);

    w->onClose = std::bind(&TocEditorWindow::CloseHandler, win, _1);
    w->onSize = std::bind(&TocEditorWindow::SizeHandler, win, _1);

    UpdateTreeModel(gWindow);
    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->SetIsVisible(true);
    UpdateRemovePdfButtonStatus(gWindow);
}
