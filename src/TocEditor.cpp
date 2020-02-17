/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/Log.h"
#include "utils/FileUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "wingui/WinGui.h"
#include "wingui/TreeModel.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TreeCtrl.h"
#include "wingui/ButtonCtrl.h"

#include "resource.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "SettingsStructs.h"
#include "WindowInfo.h"
#include "Menu.h"

#include "EngineBase.h"
#include "EngineMulti.h"
#include "EngineManager.h"

#include "ParseBKM.h"
#include "TocEditTitle.h"
#include "TocEditor.h"

using std::placeholders::_1;

// in TableOfContents.cpp
extern void OnTocCustomDraw(TreeItemCustomDrawEvent* args);

struct TocEditorWindow {
    HWND hwnd = nullptr;
    Window* mainWindow = nullptr;
    ILayout* mainLayout = nullptr;
    TocEditorArgs* tocArgs = nullptr;

    // not owned by us but by mainLayout

    ButtonCtrl* btnAddPdf = nullptr;
    ButtonCtrl* btnRemoveTocItem = nullptr;
    ButtonCtrl* btnExit = nullptr;
    ButtonCtrl* btnSaveAsVirtual = nullptr;
    ButtonCtrl* btnSaveAsPdf = nullptr;
    ILayout* layoutButtons = nullptr;

    TreeCtrl* treeCtrl = nullptr;

    ~TocEditorWindow();
    void SizeHandler(SizeEvent*);
    void CloseHandler(WindowCloseEvent*);
    void TreeItemChangedHandler(TreeItemChangedEvent*);
    void TreeItemSelectedHandler(TreeSelectionChangedEvent*);
    void TreeClickHandler(TreeClickEvent* args);
    void GetDispInfoHandler(TreeGetDispInfoEvent*);
    void TreeItemDragged(TreeItemDraggeddArgs*);
    void TreeContextMenu(ContextMenuEvent*);

    void UpdateRemoveTocItemButtonStatus();
    void UpdateTreeModel();
    void SaveAsVirtual();
    void SaveAsPdf();
    void RemoveItem();
    void AddPdf();
};

static TocEditorWindow* gWindow = nullptr;

void MessageNYI() {
    HWND hwnd = gWindow->mainWindow->hwnd;
    MessageBoxA(hwnd, "Not yet implemented!", "Information", MB_OK | MB_ICONINFORMATION);
}

void ShowErrorMessage(const char* msg) {
    HWND hwnd = gWindow->mainWindow->hwnd;
    MessageBoxA(hwnd, msg, "Error", MB_OK | MB_ICONERROR);
}

void CalcEndPageNo2(TocItem* ti, int& nPages) {
    while (ti) {
        // this marks a root node for a document
        if (ti->nPages > 0) {
            CalcEndPageNo(ti, nPages);
            nPages += ti->nPages;
        } else {
            CalcEndPageNo2(ti->child, nPages);
        }
        ti = ti->next;
    }
}

void TocEditorWindow::UpdateTreeModel() {
    treeCtrl->Clear();

    VbkmFile* bookmarks = tocArgs->bookmarks;
    TocTree* tree = bookmarks->tree;
    int nPages = 0;
    CalcEndPageNo2(tree->root, nPages);
    SetTocTreeParents(tree->root);
    treeCtrl->SetTreeModel(tree);
}

static void SetTocItemFromTocEditArgs(TocItem* ti, TocEditArgs* args) {
    std::string_view newTitle = args->title.as_view();
    str::Free(ti->title);
    ti->title = strconv::Utf8ToWstr(newTitle);

    int fontFlags = 0;
    if (args->bold) {
        bit::Set(fontFlags, fontBitBold);
    }
    if (args->italic) {
        bit::Set(fontFlags, fontBitItalic);
    }
    ti->fontFlags = fontFlags;
    ti->color = args->color;
}

static TocItem* TocItemFromTocEditArgs(TocEditArgs* args) {
    if (args == nullptr) {
        return nullptr;
    }
    // we don't allow empty titles
    if (args->title.empty()) {
        return nullptr;
    }
    TocItem* ti = new TocItem();
    SetTocItemFromTocEditArgs(ti, args);
    return ti;
}

static void StartEditTocItem(HWND hwnd, TreeCtrl* treeCtrl, TocItem* ti) {
    TocEditArgs* editArgs = new TocEditArgs();
    editArgs->bold = bit::IsSet(ti->fontFlags, fontBitBold);
    editArgs->italic = bit::IsSet(ti->fontFlags, fontBitItalic);
    editArgs->title = strconv::WstrToUtf8(ti->title);

    StartTocEditTitle(hwnd, editArgs, [=](TocEditArgs* args) {
        delete editArgs;
        if (args == nullptr) {
            // was cancelled
            return;
        }

        SetTocItemFromTocEditArgs(ti, args);
        treeCtrl->UpdateItem(ti);
    });
}

// clang-format off
#define IDM_EDIT            100
#define IDM_ADD_SIBLING     101
#define IDM_ADD_CHILD       102
#define IDM_REMOVE          103
#define IDM_ADD_PDF_CHILD   104
#define IDM_ADD_PDF_SIBLING 105

static MenuDef menuDefContext[] = {
    {"Edit",                    IDM_EDIT, 0},
    {"Add sibling",             IDM_ADD_SIBLING, 0},
    {"Add child",               IDM_ADD_CHILD, 0},
    {"Add PDF as a child",      IDM_ADD_PDF_CHILD, 0},
    {"Add PDF as a sibling",    IDM_ADD_PDF_SIBLING, 0},
    {"Remove",                  IDM_REMOVE, 0},
    { 0, 0, 0},
};
// clang-format on

static bool RemoveTocItem(TocItem* ti) {
    TocItem* parent = ti->parent;
    if (parent->child == ti) {
        parent->child = ti->next;
        ti->next = nullptr;
        return true;
    }
    TocItem* curr = parent->child;
    while (curr) {
        if (curr->next == ti) {
            curr->next = ti->next;
            ti->next = nullptr;
            return true;
        }
        curr = curr->next;
    }
    CrashMe();
    return false;
}

static EngineBase* ChooosePdfFile() {
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
        return nullptr;
    }
    WCHAR* filePath = ofn.lpstrFile;

    EngineBase* engine = EngineManager::CreateEngine(filePath);
    if (!engine) {
        ShowErrorMessage("Failed to open a file!");
        return nullptr;
    }
    return engine;
}

static bool AddPdfAsChild(TocItem* ti) {
    EngineBase* engine = ChooosePdfFile();
    if (!engine) {
        return false;
    }
    // TODO: allow PDFs with no toc
    TocTree* tocTree = engine->GetToc();
    if (nullptr == tocTree) {
        // TODO: maybe add a dummy entry for the first page
        // or make top-level act as first page destination
        ShowErrorMessage("File doesn't have Table of content");
        delete engine;
        return false;
    }
    TocItem* tocRoot = CloneTocItemRecur(tocTree->root, false);
    int nPages = engine->PageCount();
    char* filePath = (char*)strconv::WstrToUtf8(engine->FileName()).data();
    tocRoot->engineFilePath = filePath;
    tocRoot->nPages = nPages;
    ti->AddChild(tocRoot);
    delete engine;
    return true;
}

static bool AddPdfAsSibling(TocItem* ti) {
    EngineBase* engine = ChooosePdfFile();
    if (!engine) {
        return false;
    }
    TocTree* tocTree = engine->GetToc();
    if (nullptr == tocTree) {
        // TODO: maybe add a dummy entry for the first page
        // or make top-level act as first page destination
        ShowErrorMessage("File doesn't have Table of content");
        delete engine;
        return false;
    }

    TocItem* tocRoot = CloneTocItemRecur(tocTree->root, false);
    int nPages = engine->PageCount();
    char* filePath = (char*)strconv::WstrToUtf8(engine->FileName()).data();
    tocRoot->engineFilePath = filePath;
    tocRoot->nPages = nPages;
    ti->AddSibling(tocRoot);
    delete engine;
    return true;
}

void TocEditorWindow::AddPdf() {
    EngineBase* engine = ChooosePdfFile();
    if (!engine) {
        return;
    }
    TocTree* tocTree = engine->GetToc();
    TocItem* tocRoot = CloneTocItemRecur(tocTree->root, false);

    int nPages = engine->PageCount();
    char* filePath = (char*)strconv::WstrToUtf8(engine->FileName()).data();
    const WCHAR* title = path::GetBaseNameNoFree(engine->FileName());
    TocItem* tocWrapper = new TocItem(tocRoot, title, 0);
    tocWrapper->isOpenDefault = true;
    tocWrapper->child = tocRoot;
    tocWrapper->engineFilePath = filePath;
    tocWrapper->nPages = nPages;
    tocWrapper->pageNo = 0;

    tocArgs->bookmarks->tree->root->AddSiblingAtEnd(tocWrapper);
    UpdateTreeModel();

    delete engine;
}

static bool CanRemoveTocItem(TreeCtrl* treeCtrl, TocItem* ti) {
    if (!ti) {
        return false;
    }
    TocTree* tree = (TocTree*)treeCtrl->treeModel;
    if (tree->RootCount() == 1 && tree->root == ti) {
        // don't allow removing only remaining root node
        return false;
    }
    return true;
}

void TocEditorWindow::TreeContextMenu(ContextMenuEvent* args) {
    args->didHandle = true;

    POINT pt{};
    TreeItem* menuTreeItem = GetOrSelectTreeItemAtPos(args, pt);
    if (!menuTreeItem) {
        return;
    }
    TocItem* selectedTocItem = (TocItem*)menuTreeItem;
    HMENU popup = BuildMenuFromMenuDef(menuDefContext, CreatePopupMenu());

    if (!CanRemoveTocItem(treeCtrl, selectedTocItem)) {
        win::menu::SetEnabled(popup, IDM_REMOVE, false);
    }

    // TODO: this is still not good enough to prevent all invalid cases
    bool canAddPdfChild = true;
    bool canAddPdfSibling = true;
    TocItem* ti = selectedTocItem;
    while (ti) {
        // if ti is a n-th sibling of a file node, this sets it to file node
        // (i.e. first sibling)
        if (ti->parent) {
            ti = ti->parent->child;
        }
        if (ti->engineFilePath != nullptr) {
            // can't add as a child if this node or any parent
            // represents PDF file
            canAddPdfChild = false;
            // can't add as sibling if any parent represents PDF file
            canAddPdfSibling = (ti == selectedTocItem);
            break;
        }
        ti = ti->parent;
    }

    if (!canAddPdfChild) {
        win::menu::SetEnabled(popup, IDM_ADD_PDF_CHILD, false);
    }
    if (!canAddPdfSibling) {
        win::menu::SetEnabled(popup, IDM_ADD_PDF_SIBLING, false);
    }

    MarkMenuOwnerDraw(popup);
    UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    INT cmd = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, hwnd, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);
    bool ok;
    switch (cmd) {
        case IDM_EDIT:
            StartEditTocItem(mainWindow->hwnd, treeCtrl, selectedTocItem);
            break;
        case IDM_ADD_SIBLING:
        case IDM_ADD_CHILD: {
            TocEditArgs* editArgs = new TocEditArgs();
            StartTocEditTitle(hwnd, editArgs, [=](TocEditArgs* args) {
                delete editArgs;
                TocItem* ti = TocItemFromTocEditArgs(args);
                if (ti == nullptr) {
                    // was cancelled or invalid
                    return;
                }
                if (cmd == IDM_ADD_SIBLING) {
                    selectedTocItem->AddSibling(ti);
                } else if (cmd == IDM_ADD_CHILD) {
                    selectedTocItem->AddChild(ti);
                } else {
                    CrashMe();
                }
                // ensure is visible i.e. expand all parents of this item
                TocItem* curr = selectedTocItem;
                while (curr) {
                    curr->isOpenDefault = true;
                    curr->isOpenToggled = false;
                    curr = curr->parent;
                }
                UpdateTreeModel();
            });
        } break;
        case IDM_ADD_PDF_CHILD:
            ok = AddPdfAsChild(selectedTocItem);
            if (ok) {
                UpdateTreeModel();
            }
            break;
        case IDM_ADD_PDF_SIBLING:
            ok = AddPdfAsSibling(selectedTocItem);
            if (ok) {
                UpdateTreeModel();
            }
            break;
        case IDM_REMOVE:
            // ensure is visible i.e. expand all parents of this item
            TocItem* curr = selectedTocItem->parent;
            while (curr) {
                curr->isOpenDefault = true;
                curr->isOpenToggled = false;
                curr = curr->parent;
            }
            ok = RemoveTocItem(selectedTocItem);
            if (ok) {
                UpdateTreeModel();
                delete selectedTocItem;
            }
            break;
    }
}

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

TocEditorArgs::~TocEditorArgs() {
    delete bookmarks;
}

void TocEditorWindow::RemoveItem() {
    TreeItem* sel = treeCtrl->GetSelection();
    CrashIf(!sel);
    TocItem* di = (TocItem*)sel;
    TreeItem* parent = di->Parent();
    if (parent != nullptr) {
        CrashIf(parent != di->Parent());
    }
    TocItem* parent2 = (TocItem*)parent;
    parent2->child = nullptr;
    delete di;
    UpdateTreeModel();
}

void TocEditorWindow::UpdateRemoveTocItemButtonStatus() {
    TreeItem* sel = treeCtrl->GetSelection();
    bool isEnabled = CanRemoveTocItem(treeCtrl, (TocItem*)sel);
    btnRemoveTocItem->SetIsEnabled(isEnabled);
}

// in TableOfContents.cpp
extern void ShowExportedBookmarksMsg(const char* path);

void TocEditorWindow::SaveAsPdf() {
    MessageNYI();
}

void TocEditorWindow::SaveAsVirtual() {
    str::WStr pathw = tocArgs->filePath.get();

    // if the source was .vbkm file, we over-write it by default
    // any other format, we add .vbkm extension by default
    if (!str::EndsWithI(pathw.Get(), L".vbkm")) {
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
    ok = ExportBookmarksToFile(tocArgs->bookmarks->tree, "", patha);
    if (!ok) {
        return;
    }
    // ShowExportedBookmarksMsg(patha);
}

static void CreateButtonsLayout(TocEditorWindow* w) {
    HWND hwnd = w->hwnd;
    CrashIf(!hwnd);

    auto* buttons = new HBox();

    buttons->alignMain = MainAxisAlign::SpaceBetween;
    buttons->alignCross = CrossAxisAlign::CrossStart;
    {
        auto [l, b] = CreateButtonLayout(hwnd, "Add PDF", std::bind(&TocEditorWindow::AddPdf, w));
        buttons->addChild(l);
        w->btnAddPdf = b;
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Remove Item", std::bind(&TocEditorWindow::RemoveItem, w));
        buttons->addChild(l);
        w->btnRemoveTocItem = b;
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Save As PDF", std::bind(&TocEditorWindow::SaveAsPdf, w));
        buttons->addChild(l);
        w->btnSaveAsPdf = b;
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Save As Virtual PDF", std::bind(&TocEditorWindow::SaveAsVirtual, w));
        buttons->addChild(l);
        w->btnSaveAsVirtual = b;
    }

    {
        auto [l, b] = CreateButtonLayout(hwnd, "Exit", std::bind(&Window::Close, w->mainWindow));
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
    tree->onContextMenu = std::bind(&TocEditorWindow::TreeContextMenu, win, _1);
    gWindow->treeCtrl = tree;
    auto treeLayout = NewTreeLayout(tree);

    main->addChild(treeLayout, 1);
    main->addChild(win->layoutButtons, 0);

    auto* padding = new Padding();
    padding->insets = DefaultInsets();
    padding->child = main;
    win->mainLayout = padding;
}

void TocEditorWindow::SizeHandler(SizeEvent* args) {
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

void TocEditorWindow::CloseHandler(WindowCloseEvent* args) {
    WindowBase* w = (WindowBase*)gWindow->mainWindow;
    CrashIf(w != args->w);
    delete gWindow;
    gWindow = nullptr;
}

void TocEditorWindow::TreeItemChangedHandler(TreeItemChangedEvent* args) {
    if (!args->checkedChanged) {
        return;
    }
    TocItem* ti = (TocItem*)args->treeItem;
    ti->isUnchecked = !args->newState.isChecked;
}

void TocEditorWindow::TreeItemSelectedHandler(TreeSelectionChangedEvent* args) {
    UNUSED(args);
    UpdateRemoveTocItemButtonStatus();
}

void TocEditorWindow::GetDispInfoHandler(TreeGetDispInfoEvent* args) {
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
        if (ti->engineFilePath) {
            const char* name = path::GetBaseNameNoFree(ti->engineFilePath);
            AutoFreeWstr nameW = strconv::Utf8ToWstr(name);
            s = str::Format(L"%s [file: %s, pages %d-%d]", ti->title, nameW.get(), sno, eno);
        } else {
            s = str::Format(L"%s [pages %d-%d]", ti->title, sno, eno);
        }
    } else {
        if (ti->engineFilePath) {
            const char* name = path::GetBaseNameNoFree(ti->engineFilePath);
            AutoFreeWstr nameW = strconv::Utf8ToWstr(name);
            s = str::Format(L"%s [file: %s, page %d]", ti->title, nameW.get(), sno);
        } else {
            s = str::Format(L"%s [page %d]", ti->title, sno);
        }
    }
    str::BufSet(tvitem->pszText, cchMax, s);
    str::Free(s);
}

void TocEditorWindow::TreeClickHandler(TreeClickEvent* args) {
    if (!args->isDblClick) {
        return;
    }
    if (!args->treeItem) {
        return;
    }

    args->didHandle = true;
    args->result = 1;

    TocItem* ti = (TocItem*)args->treeItem;
    StartEditTocItem(mainWindow->hwnd, treeCtrl, ti);
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

    gWindow->UpdateTreeModel();
    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->SetIsVisible(true);
    gWindow->UpdateRemoveTocItemButtonStatus();
}
