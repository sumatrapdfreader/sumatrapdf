/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/Log.h"
#include "utils/LogDbg.h"
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
#include "wingui/StaticCtrl.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EngineMulti.h"
#include "EngineCreate.h"

#include "resource.h"
#include "Commands.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "DisplayMode.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "Controller.h"
#include "Menu.h"
#include "Translations.h"
#include "SaveAsPdf.h"
#include "SumatraPDF.h"
#include "SumatraConfig.h"

#include "ParseBKM.h"
#include "TocEditTitle.h"
#include "TocEditor.h"

using std::placeholders::_1;

// in TableOfContents.cpp
extern void OnTocCustomDraw(TreeItemCustomDrawEvent*);

struct TocEditorWindow {
    TocEditorArgs* tocArgs = nullptr;
    HWND hwnd = nullptr;

    LayoutBase* mainLayout = nullptr;
    // not owned by us but by mainLayout
    Window* mainWindow = nullptr;
    ButtonCtrl* btnAddPdf = nullptr;
    ButtonCtrl* btnRemoveTocItem = nullptr;
    ButtonCtrl* btnExit = nullptr;
    ButtonCtrl* btnSaveAsVirtual = nullptr;
    ButtonCtrl* btnSaveAsPdf = nullptr;
    StaticCtrl* labelInfo = nullptr;
    ILayout* layoutButtons = nullptr;

    TreeCtrl* treeCtrl = nullptr;

    ~TocEditorWindow();
    void SizeHandler(SizeEvent*);
    void CloseHandler(WindowCloseEvent*);
    void TreeItemChangedHandler(TreeItemChangedEvent*);
    void TreeItemSelectedHandler(TreeSelectionChangedEvent*);
    void TreeClickHandler(TreeClickEvent*);
    void GetDispInfoHandler(TreeGetDispInfoEvent*);
    void TreeItemDragStartEnd(TreeItemDraggeddEvent*);
    void TreeContextMenu(ContextMenuEvent*);
    void DropFilesHandler(DropFilesEvent*);

    void UpdateRemoveTocItemButtonStatus();
    void UpdateTreeModel();
    void SaveAsVirtual();
    void SaveAsPdf();
    void RemoveItem();
    void AddPdf();
    void AddPdfAsSibling(TocItem* ti);
    void AddPdfAsChild(TocItem* ti);
    void RemoveTocItem(TocItem* ti, bool alsoDelete);
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
            nPages = ti->nPages;
            CalcEndPageNo(ti, nPages);
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
    std::string_view newTitle = args->title.AsView();
    str::Free(ti->title);
    ti->title = strconv::Utf8ToWstr(newTitle);
    ti->pageNo = args->page;

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

// find toc item that is a parent of a given ti that represents a pdf file
static TocItem* FindFileParentItem(TocItem* ti) {
    while (ti) {
        if (ti->engineFilePath) {
            return ti;
        }
        ti = ti->parent;
    }
    return nullptr;
}

static void StartEditTocItem(HWND hwnd, TreeCtrl* treeCtrl, TocItem* ti) {
    TocEditArgs* editArgs = new TocEditArgs();
    editArgs->bold = bit::IsSet(ti->fontFlags, fontBitBold);
    editArgs->italic = bit::IsSet(ti->fontFlags, fontBitItalic);
    editArgs->title = strconv::WstrToUtf8(ti->title);
    editArgs->color = ti->color;
    TocItem* fileParent = FindFileParentItem(ti);
    if (fileParent) {
        editArgs->nPages = fileParent->nPages;
        editArgs->page = ti->pageNo;
    }

    StartTocEditTitle(hwnd, editArgs, [=](TocEditArgs* args) {
        if (args == nullptr) {
            // was cancelled
            return;
        }

        SetTocItemFromTocEditArgs(ti, args);
        treeCtrl->UpdateItem(ti);
    });
}

// clang-format off
static MenuDef menuDefContext[] = {
    {_TRN("Edit"),                    CmdTocEditorStart, 0},
    {_TRN("Add sibling"),             CmdTocEditorAddSibling, 0},
    {_TRN("Add child"),               CmdTocEditorAddChild, 0},
    {_TRN("Add PDF as a child"),      CmdTocEditorAddPdfChild, 0},
    {_TRN("Add PDF as a sibling"),    CmdTocEditorAddPdfSibling, 0},
    {_TRN("Remove Item"),             CmdTocEditorRemoveItem, 0},
    { 0, 0, 0 },
};
// clang-format on

static bool RemoveIt(TreeCtrl* treeCtrl, TocItem* ti) {
    SubmitCrashIf(!ti);
    if (!ti) {
        return false;
    }
    TocItem* parent = ti->parent;
    if (parent && parent->child == ti) {
        parent->child = ti->next;
        ti->next = nullptr;
        return true;
    }

    // first sibling for ti
    TocItem* curr = nullptr;
    if (parent) {
        curr = parent->child;
    } else {
        TocTree* tree = (TocTree*)treeCtrl->treeModel;
        curr = tree->root;
        // ti is the first top-level element
        if (curr == ti) {
            tree->root = ti->next;
            return true;
        }
    }
    // remove ti from list of siblings
    while (curr) {
        if (curr->next == ti) {
            curr->next = ti->next;
            ti->next = nullptr;
            return true;
        }
        curr = curr->next;
    }
    // didn't find ti in a list of siblings, shouldn't happen
    CrashMe();
    return false;
}

// ensure is visible i.e. expand all parents of this item
static void EnsureExpanded(TocItem* ti) {
    while (ti) {
        ti->isOpenDefault = false;
        ti->isOpenToggled = false;
        if (ti->child) {
            // isOpenDefault / isOpenToggled is only meaningful for nodes with children
            ti->isOpenDefault = true;
        }
        ti = ti->parent;
    }
}

void TocEditorWindow::RemoveTocItem(TocItem* ti, bool alsoDelete) {
    CrashIf(!ti);
    if (!ti) {
        return;
    }
    EnsureExpanded(ti->parent);

    bool ok = RemoveIt(treeCtrl, ti);
    if (ok && alsoDelete) {
        UpdateTreeModel();
        ti->DeleteJustSelf();
    }
}

static EngineBase* ChooosePdfFile() {
    TocEditorWindow* w = gWindow;
    HWND hwnd = w->mainWindow->hwnd;

    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;

    str::WStr fileFilter(256);
    fileFilter.Append(_TR("PDF documents"));
    fileFilter.Append(L"\1*.pdf\1");
    str::TransChars(fileFilter.Get(), L"\1", L"\0");

    ofn.lpstrFilter = fileFilter.Get();
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

    EngineBase* engine = CreateEngine(filePath);
    if (!engine) {
        ShowErrorMessage("Failed to open a file!");
        return nullptr;
    }
    return engine;
}

void TocEditorWindow::AddPdfAsChild(TocItem* ti) {
    EngineBase* engine = ChooosePdfFile();
    if (!engine) {
        return;
    }
    TocItem* tocWrapper = CreateWrapperItem(engine);
    ti->AddChild(tocWrapper);
    UpdateTreeModel();
    delete engine;
}

void TocEditorWindow::AddPdfAsSibling(TocItem* ti) {
    EngineBase* engine = ChooosePdfFile();
    if (!engine) {
        return;
    }
    TocItem* tocWrapper = CreateWrapperItem(engine);
    ti->AddSibling(tocWrapper);
    UpdateTreeModel();
    delete engine;
}

void TocEditorWindow::AddPdf() {
    EngineBase* engine = ChooosePdfFile();
    if (!engine) {
        return;
    }

    TocItem* tocWrapper = CreateWrapperItem(engine);
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

// TODO: simplify and verify is correct
static bool CanAddPdfAsChild(TocItem* tocItem) {
    bool canAddPdfChild = true;
    TocItem* ti = tocItem;
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
            break;
        }
        ti = ti->parent;
    }
    return canAddPdfChild;
}

// TODO: simplify and verify is correct
static bool CanAddPdfAsSibling(TocItem* tocItem) {
    bool canAddPdfSibling = true;
    TocItem* ti = tocItem;
    while (ti) {
        // if ti is a n-th sibling of a file node, this sets it to file node
        // (i.e. first sibling)
        if (ti->parent) {
            ti = ti->parent->child;
        }
        if (ti->engineFilePath != nullptr) {
            // can't add as sibling if any parent represents PDF file
            canAddPdfSibling = (ti == tocItem);
            break;
        }
        ti = ti->parent;
    }
    return canAddPdfSibling;
}

void TocEditorWindow::DropFilesHandler(DropFilesEvent* ev) {
    int nFiles = DragQueryFile(ev->hdrop, DRAGQUERY_NUMFILES, 0, 0);
    // logf("TocEditorWindow::DropFilesHandler(): %d files\n", nFiles);
    defer {
        DragFinish(ev->hdrop);
    };

    POINT pt{};
    BOOL ok = DragQueryPoint(ev->hdrop, &pt);
    if (!ok) {
        return; // probably shouldn't happen
    }

    TocItem* ti = (TocItem*)treeCtrl->GetItemAt(pt.x, pt.y);

    // TODO: maybe accept more than 1 file?
    if (nFiles != 1) {
        return;
    }

    // we only accept pdf files
    WCHAR filePath[MAX_PATH] = {0};
    bool found = false;
    for (int i = 0; i < nFiles && !found; i++) {
        DragQueryFile(ev->hdrop, i, filePath, dimof(filePath));
        // TODO: maybe resolve .lnk files like OnDropFiles() in Canvas.cpp
        if (str::EndsWithI(filePath, L".pdf")) {
            found = true;
        }
    }

    if (!found) {
        return;
    }

    EngineBase* engine = CreateEngine(filePath, nullptr);
#if 0
    AutoFreeStr path = strconv::WstrToUtf8(filePath);
    logf("Dropped file: '%s' at (%d, %d) on item: 0x%x, engine: 0x%x\n", path.Get(), pt.x, pt.y, ti, engine);
#endif

    if (!engine) {
        return;
    }

    defer {
        delete engine;
    };

    // TocItem* fileToc = (TocItem*)treeCtrl->treeModel->RootAt(0);

    // didn't drop on an existing itme: add as a last sibling
    if (ti == nullptr) {
        TocItem* tocWrapper = CreateWrapperItem(engine);
        tocArgs->bookmarks->tree->root->AddSiblingAtEnd(tocWrapper);
        UpdateTreeModel();
        return;
    }

    bool addAsSibling = !IsShiftPressed();
    if (addAsSibling) {
        if (CanAddPdfAsSibling(ti)) {
            TocItem* tocWrapper = CreateWrapperItem(engine);
            ti->AddSibling(tocWrapper);
            UpdateTreeModel();
        }
        return;
    }

    if (CanAddPdfAsChild(ti)) {
        TocItem* tocWrapper = CreateWrapperItem(engine);
        ti->AddChild(tocWrapper);
        UpdateTreeModel();
    }
}

void TocEditorWindow::TreeContextMenu(ContextMenuEvent* ev) {
    ev->didHandle = true;

    POINT pt{};
    TreeItem* menuTreeItem = GetOrSelectTreeItemAtPos(ev, pt);
    if (!menuTreeItem) {
        return;
    }
    TocItem* selectedTocItem = (TocItem*)menuTreeItem;
    HMENU popup = BuildMenuFromMenuDef(menuDefContext, CreatePopupMenu());

    if (!CanRemoveTocItem(treeCtrl, selectedTocItem)) {
        win::menu::SetEnabled(popup, CmdTocEditorRemoveItem, false);
    }

    bool canAddPdfChild = CanAddPdfAsChild(selectedTocItem);
    bool canAddPdfSibling = CanAddPdfAsSibling(selectedTocItem);

    if (!canAddPdfChild) {
        win::menu::SetEnabled(popup, CmdTocEditorAddPdfChild, false);
    }
    if (!canAddPdfSibling) {
        win::menu::SetEnabled(popup, CmdTocEditorAddPdfSibling, false);
    }

    MarkMenuOwnerDraw(popup);
    uint flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    int cmd = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, hwnd, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);
    switch (cmd) {
        case CmdTocEditorStart:
            StartEditTocItem(mainWindow->hwnd, treeCtrl, selectedTocItem);
            break;
        case CmdTocEditorAddSibling:
        case CmdTocEditorAddChild: {
            TocEditArgs* editArgs = new TocEditArgs();
            TocItem* fileParent = FindFileParentItem(selectedTocItem);
            if (fileParent) {
                editArgs->nPages = fileParent->nPages;
            }
            StartTocEditTitle(hwnd, editArgs, [=](TocEditArgs* args) {
                TocItem* ti = TocItemFromTocEditArgs(args);
                if (ti == nullptr) {
                    // was cancelled or invalid
                    return;
                }
                if (cmd == CmdTocEditorAddSibling) {
                    selectedTocItem->AddSibling(ti);
                } else if (cmd == CmdTocEditorAddChild) {
                    selectedTocItem->AddChild(ti);
                } else {
                    CrashMe();
                }
                EnsureExpanded(selectedTocItem);
                UpdateTreeModel();
            });
        } break;
        case CmdTocEditorAddPdfChild:
            AddPdfAsChild(selectedTocItem);
            break;
        case CmdTocEditorAddPdfSibling:
            AddPdfAsSibling(selectedTocItem);
            break;
        case CmdTocEditorRemoveItem:
            RemoveTocItem(selectedTocItem, true);
            break;
    }
}

static void SetInfoLabelText(StaticCtrl* l, bool forDrag) {
    if (forDrag) {
        l->SetText("Press SHIFT to add as a child, otherwise a ");
    } else {
        l->SetText("Tip: use context menu for more actions");
    }
}

// return true if dst is a child of src
static bool IsItemChildOf(TocItem* src, TocItem* dst) {
    CrashIf(!src);
    CrashIf(!dst);
    TocItem* ti = dst;
    while (ti) {
        if (ti == src) {
            return true;
        }
        ti = ti->parent;
    }
    return false;
}

void TocEditorWindow::TreeItemDragStartEnd(TreeItemDraggeddEvent* ev) {
    if (ev->isStart) {
        SetInfoLabelText(labelInfo, true);
        return;
    }
    SetInfoLabelText(labelInfo, false);

    TocItem* src = (TocItem*)ev->draggedItem;
    TocItem* dst = (TocItem*)ev->dragTargetItem;
    CrashIf(!src);
    if (!src) {
        return;
    }
    if (!dst) {
        // it doesn't seem possible to drop on empty node
        // default code always selects a node
        return;
    }
    // ignore drop on itself
    if (src == dst) {
        return;
    }
    // ignore if dropping on its own child
    if (IsItemChildOf(src, dst)) {
        return;
    }

    // regular drag adds as a sibling, with shift adds as a child
    bool addAsSibling = !IsShiftPressed();
    AutoFreeStr srcTitle = strconv::WstrToUtf8(src->title);
    AutoFreeStr dstTitle = strconv::WstrToUtf8(dst->title);
    dbglogf("TreeItemDragged: dragged: %s on: %s. Add as: %s\n", srcTitle.Get(), dstTitle.Get(),
            addAsSibling ? "sibling" : "child");

    // entries inside a single PDF cannot be moved outside of it
    // entries outside of a PDF cannot be moved inside PDF
    TocItem* srcFileParent = FindFileParentItem(src);
    TocItem* dstFileParent = FindFileParentItem(dst);

    if (srcFileParent != dstFileParent) {
        if (addAsSibling) {
            // allow adding file node as a sibling of another file node
            if (src->engineFilePath && dst->engineFilePath) {
                RemoveTocItem(src, false);
                dst->AddSibling(src);
                UpdateTreeModel();
            }
        }
        // TODO: show a temporary error message that will go away after a while
        return;
    }

    if (addAsSibling && dst->engineFilePath) {
        // - foo.pdf file
        //   - child
        // can't add child as a singling of foo.pdf
        return;
    }

    RemoveTocItem(src, false);
    if (addAsSibling) {
        dst->AddSibling(src);
    } else {
        dst->AddChild(src);
        EnsureExpanded(src);
    }
    UpdateTreeModel();
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
    TocItem* ti = (TocItem*)sel;
    RemoveTocItem(ti, true);
}

void TocEditorWindow::UpdateRemoveTocItemButtonStatus() {
    TreeItem* sel = treeCtrl->GetSelection();
    bool isEnabled = CanRemoveTocItem(treeCtrl, (TocItem*)sel);
    btnRemoveTocItem->SetIsEnabled(isEnabled);
}

// in TableOfContents.cpp
extern void ShowExportedBookmarksMsg(const char* path);

static std::string_view PickSaveName() {
    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    str::WStr fileFilter(256);
    fileFilter.Append(_TR("PDF documents"));
    fileFilter.Append(L"\1*.pdf\1");
    str::TransChars(fileFilter.Get(), L"\1", L"\0");

    WCHAR dstFileName[MAX_PATH] = {};

    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilter.Get();
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = nullptr;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    // note: explicitly not setting lpstrInitialDir so that the OS
    // picks a reasonable default (in particular, we don't want this
    // in plugin mode, which is likely the main reason for saving as...)

    bool ok = GetSaveFileNameW(&ofn);
    if (!ok) {
        return {};
    }
    std::string_view res = strconv::WstrToUtf8(dstFileName);
    return res;
}

static void ShowSavedAsPdfMsg(const char* path) {
    str::Str msg;
    msg.AppendFmt("Saved as PDF file %s", path);
    str::Str caption;
    caption.Append("Saved as PDF");
    uint type = MB_OK | MB_ICONINFORMATION | MbRtlReadingMaybe();
    MessageBoxA(nullptr, msg.Get(), caption.Get(), type);
}

void TocEditorWindow::SaveAsPdf() {
    AutoFreeStr path = PickSaveName();
    if (path.empty()) {
        return;
    }
    TocTree* tree = (TocTree*)treeCtrl->treeModel;
    bool ok = SaveVirtualAsPdf(tree->root, (char*)path.Get());
    if (ok) {
        ShowSavedAsPdfMsg(path.Get());
    }
}

void TocEditorWindow::SaveAsVirtual() {
    str::WStr pathw = tocArgs->filePath.Get();

    bool isVbkm = str::EndsWithI(pathw.Get(), L".vbkm");
    // if the source was .vbkm file, we over-write it by default
    // any other format, we add .vbkm extension by default
    if (!isVbkm) {
        pathw.Append(L".vbkm");
    }

    char* patha = nullptr;
    if (IsShiftPressed() && isVbkm) {
        // when SHIFT is pressed write without asking for a file
        patha = (char*)strconv::WstrToUtf8(pathw.AsView()).data();
    } else {
        WCHAR dstFileName[MAX_PATH]{0};
        str::BufSet(&(dstFileName[0]), dimof(dstFileName), pathw.Get());

        HWND hwnd = gWindow->mainWindow->hwnd;
        OPENFILENAME ofn{0};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFile = dstFileName;
        ofn.nMaxFile = dimof(dstFileName);
        ofn.lpstrFilter = L"VBKM files\0*.vbkm\0\0";
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
        patha = (char*)strconv::WstrToUtf8(dstFileName).data();
    }

    bool ok = ExportBookmarksToFile(tocArgs->bookmarks->tree, "", patha);
    free(patha);
    if (!ok) {
        return;
    }
    // ShowExportedBookmarksMsg(patha);
}

static void CreateButtonsLayout(TocEditorWindow* w) {
    HWND hwnd = w->hwnd;
    CrashIf(!hwnd);

    auto* buttons = new HBox();

    buttons->alignMain = MainAxisAlign::Homogeneous;
    buttons->alignCross = CrossAxisAlign::CrossStart;
    {
        auto b = CreateButton(hwnd, "&Add PDF", std::bind(&TocEditorWindow::AddPdf, w));
        buttons->AddChild(b);
        w->btnAddPdf = b;
    }

    {
        auto b = CreateButton(hwnd, "&Remove Item", std::bind(&TocEditorWindow::RemoveItem, w));
        buttons->AddChild(b);
        w->btnRemoveTocItem = b;
    }

    {
        auto b = CreateButton(hwnd, "Save As PDF", std::bind(&TocEditorWindow::SaveAsPdf, w));
        buttons->AddChild(b);
        w->btnSaveAsPdf = b;
    }

    {
        auto b = CreateButton(hwnd, "Save As Virtual PDF", std::bind(&TocEditorWindow::SaveAsVirtual, w));
        buttons->AddChild(b);
        w->btnSaveAsVirtual = b;
    }

    {
        auto b = CreateButton(hwnd, "E&xit", std::bind(&Window::Close, w->mainWindow));
        buttons->AddChild(b);
        w->btnExit = b;
    }

    w->layoutButtons = buttons;
}

static void CreateMainLayout(TocEditorWindow* win) {
    HWND hwnd = win->hwnd;
    CrashIf(!hwnd);

    CreateButtonsLayout(win);

    auto* tree = new TreeCtrl(hwnd);
    gWindow->treeCtrl = tree;

    int dx = DpiScale(80);
    int dy = DpiScale(120);
    tree->idealSize = {dx, dy};

    tree->supportDragDrop = true;
    tree->withCheckboxes = true;
    tree->onTreeGetDispInfo = std::bind(&TocEditorWindow::GetDispInfoHandler, win, _1);
    tree->onDropFiles = std::bind(&TocEditorWindow::DropFilesHandler, win, _1);
    tree->onTreeItemChanged = std::bind(&TocEditorWindow::TreeItemChangedHandler, win, _1);
    tree->onTreeItemCustomDraw = OnTocCustomDraw;
    tree->onTreeSelectionChanged = std::bind(&TocEditorWindow::TreeItemSelectedHandler, win, _1);
    tree->onTreeClick = std::bind(&TocEditorWindow::TreeClickHandler, win, _1);
    tree->onTreeItemDragStartEnd = std::bind(&TocEditorWindow::TreeItemDragStartEnd, win, _1);
    tree->onContextMenu = std::bind(&TocEditorWindow::TreeContextMenu, win, _1);

    bool ok = tree->Create();
    CrashIf(!ok);

    win->labelInfo = new StaticCtrl(hwnd);
    SetInfoLabelText(win->labelInfo, false);
    COLORREF col = MkGray(0x33);
    win->labelInfo->SetTextColor(col);
    win->labelInfo->Create();

    auto* main = new VBox();
    main->alignMain = MainAxisAlign::MainStart;
    main->alignCross = CrossAxisAlign::Stretch;

    main->AddChild(tree, 1);
    main->AddChild(win->labelInfo, 0);
    main->AddChild(win->layoutButtons, 0);

    auto padding = new Padding(main, DpiScaledInsets(hwnd, 8));
    win->mainLayout = padding;
}

void TocEditorWindow::SizeHandler(SizeEvent* ev) {
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

void TocEditorWindow::CloseHandler(WindowCloseEvent* ev) {
    WindowBase* w = (WindowBase*)gWindow->mainWindow;
    CrashIf(w != ev->w);
    delete gWindow;
    gWindow = nullptr;
}

void TocEditorWindow::TreeItemChangedHandler(TreeItemChangedEvent* ev) {
    if (!ev->checkedChanged) {
        return;
    }
    TocItem* ti = (TocItem*)ev->treeItem;
    ti->isUnchecked = !ev->newState.isChecked;
}

void TocEditorWindow::TreeItemSelectedHandler([[maybe_unused]] TreeSelectionChangedEvent* ev) {
    UpdateRemoveTocItemButtonStatus();
}

void TocEditorWindow::GetDispInfoHandler(TreeGetDispInfoEvent* ev) {
    ev->didHandle = true;

    TocItem* ti = (TocItem*)ev->treeItem;
    TVITEMEXW* tvitem = &ev->dispInfo->item;
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
            s = str::Format(L"%s [file: %s, pages %d-%d]", ti->title, nameW.Get(), sno, eno);
        } else {
            s = str::Format(L"%s [pages %d-%d]", ti->title, sno, eno);
        }
    } else {
        if (ti->engineFilePath) {
            const char* name = path::GetBaseNameNoFree(ti->engineFilePath);
            AutoFreeWstr nameW = strconv::Utf8ToWstr(name);
            s = str::Format(L"%s [file: %s, page %d]", ti->title, nameW.Get(), sno);
        } else {
            s = str::Format(L"%s [page %d]", ti->title, sno);
        }
    }
    str::BufSet(tvitem->pszText, cchMax, s);
    str::Free(s);
}

void TocEditorWindow::TreeClickHandler(TreeClickEvent* ev) {
    if (!ev->isDblClick) {
        return;
    }
    if (!ev->treeItem) {
        return;
    }

    ev->didHandle = true;
    ev->result = 1;

    TocItem* ti = (TocItem*)ev->treeItem;
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
    w->isDialog = true;
    w->backgroundColor = MkRgb((u8)0xee, (u8)0xee, (u8)0xee);
    w->SetTitle("Table of content editor");
    int dx = DpiScale(hwndOwner, 640);
    int dy = DpiScale(hwndOwner, 800);
    w->initialSize = {dx, dy};
    PositionCloseTo(w, args->hwndRelatedTo);
    SIZE winSize = {w->initialSize.dx, w->initialSize.dy};
    LimitWindowSizeToScreen(args->hwndRelatedTo, winSize);
    w->initialSize = {winSize.cx, winSize.cy};
    bool ok = w->Create();
    CrashIf(!ok);

    win->mainWindow = w;
    win->hwnd = w->hwnd;

    w->onClose = std::bind(&TocEditorWindow::CloseHandler, win, _1);
    w->onSize = std::bind(&TocEditorWindow::SizeHandler, win, _1);

    CreateMainLayout(gWindow);
    LayoutAndSizeToContent(win->mainLayout, 720, 800, w->hwnd);

    gWindow->UpdateTreeModel();
    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->SetIsVisible(true);
    gWindow->UpdateRemoveTocItemButtonStatus();
}

void StartTocEditorForWindowInfo(WindowInfo* win) {
    auto* tab = win->currentTab;
    TocEditorArgs* args = new TocEditorArgs();
    args->filePath = str::Dup(tab->filePath);

    VbkmFile* vbkm = new VbkmFile();
    AutoFreeStr filePath = strconv::WstrToUtf8(tab->filePath);
    if (str::EndsWithI(tab->filePath, L".vbkm")) {
        bool ok = LoadVbkmFile(filePath, *vbkm);
        if (!ok) {
            // TODO: show error message box
            delete args;
            return;
        }
    } else {
        TocTree* tree = (TocTree*)win->tocTreeCtrl->treeModel;
        TocItem* rootCopy = nullptr;
        if (tree && tree->root) {
            rootCopy = CloneTocItemRecur(tree->root, false);
        }
        const WCHAR* name = path::GetBaseNameNoFree(tab->filePath);
        TocItem* newRoot = new TocItem(nullptr, name, 0);
        newRoot->isOpenDefault = true;
        newRoot->child = rootCopy;
        newRoot->pageNo = 1; // default to first page in the PDF
        newRoot->nPages = tab->ctrl->PageCount();
        newRoot->engineFilePath = filePath.Release();
        vbkm->tree = new TocTree(newRoot);
    }
    args->bookmarks = vbkm;
    args->hwndRelatedTo = win->hwndFrame;
    StartTocEditor(args);
}

bool IsTocEditorEnabledForWindowInfo(TabInfo* tab) {
    if (!gWithTocEditor) {
        return false;
    }
    auto path = tab->filePath.Get();
    if (str::EndsWithI(path, L".vbkm")) {
        return true;
    }
    if (str::EndsWithI(path, L".pdf")) {
        return true;
    }
    return false;
}
