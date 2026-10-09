/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig draws the sidebar with two child windows (CreateToc / CreateFavorites),
// each holding a VirtText header with a close button, an Edit and a win32
// TreeView it custom-draws. Here both panes are gpui elements over the same
// models: the bookmarks pane walks the document's TocItem tree (expansion lives
// on the items, as in orig) and the favorites pane walks the FavTreeModel that
// Favorites.cpp builds. Rows are virtualized against the pane's measured height.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"
#include "base/BitManip.h"
#include "base/Pixmap.h"
#include "base/UITask.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Theme.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "FileHistory.h"
#include "Commands.h"
#include "Translations.h"
#include "Menu.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Favorites.h"
#include "TableOfContents.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "FilterHighlightDraw.h"
#include "gui/DialogWidgets.h"
#include "gui/Sidebar.h"

#include "SumatraLog.h"

// orig's tree rows are one text line high; the indent is a tree-view level
// measured on orig's Explorer-themed tree view at 100% (Windows 11): 18 px
// rows, 19 px a level, the label 25 px in, the glyph a chevron in the text
// color centered 12 px in
constexpr float kRowDy = 18;
constexpr float kIndentDx = 19;
constexpr float kChevronDx = 19;
constexpr float kRowPadL = 4;
constexpr float kTreeFontSize = 12;
constexpr float kPaneHeaderDy = 24;
constexpr int kThumbDx = 120;
constexpr int kThumbDy = 170;
constexpr int kThumbGap = 16;
constexpr int kThumbMaxCols = 6;

struct SidebarThumb {
    Pixmap* bitmap = nullptr;
    gp::RenderImage* img = nullptr;
    bool failed = false;
};

struct SidebarThumbCache {
    Vec<SidebarThumb> thumbs;
    EngineBase* renderEngine = nullptr;
    DisplayModel* owner = nullptr;
    MainWindow* win = nullptr;
    AtomicInt cancelRendering = 0;
    int rotation = 0;
    bool workerRunning = false;
    bool deleteWhenWorkerFinishes = false;
};

// one visible row of a tree
struct TocRow {
    TocItem* item;
    int depth;
};

struct FavRow {
    FavTreeItem* item;
    int depth;
};

struct SidebarView;

struct SidebarUI {
    // ng: one view entity per window, so a second window's listeners dispatch
    // into its own sidebar state (step 10b)
    gp::Entity<SidebarView> view;
    gpui::InputState* tocFilter = nullptr;
    gpui::InputState* favFilter = nullptr;
    gp::FocusHandle tocFocus;
    gp::FocusHandle favFocus;
    float tocScrollY = 0;
    float favScrollY = 0;
    // measured last frame; the row virtualization needs a viewport height
    gpui::Bounds tocView{};
    gpui::Bounds favView{};
    gpui::Bounds thumbView{};
    // one entry per page; only the cells built this frame have a size
    Vec<gpui::Bounds> thumbBounds;
    float thumbScrollY = 0;
    int thumbRevealPage = 0;
    int thumbSelectedPage = 0;
    bool thumbSelectionPinned = false;
    // view icons, Bookmarks / Thumbnails / Favorites, for each panel
    gpui::Bounds viewIconBounds[2][3]{};
    SidebarThumbCache* thumbCache = nullptr;

    TocItem* tocSel = nullptr;
    TocItem* tocRightClick = nullptr;
    TocItem* tocReveal = nullptr;
    bool tocRtl = false;
    Vec<TocRow> tocRows;
    MenuModel* tocMenu = nullptr;
    TocItem* tocMenuFor = nullptr;
    bool tocMenuValid = false;

    FavTreeModel* favModel = nullptr;
    FavTreeItem* favSel = nullptr;
    FavTreeItem* favRightClick = nullptr;
    Vec<FavRow> favRows;
    MenuModel* favMenu = nullptr;
    FavTreeItem* favMenuFor = nullptr;
    bool favMenuValid = false;
    bool favWantFocus = false;
    gp::Entity<gp::PopupMenuState> tocPopup;
    gp::Entity<gp::PopupMenuState> favPopup;
    // a right press landed on a row; the pane's own handler for the same press
    // runs after the row's and clears the target when it did not
    bool rdownOnRow = false;

    // the native tree's incremental search: what was typed and when
    char typeAhead[64]{};
    int typeAheadLen = 0;
    double typeAheadTime = 0;
};

struct SidebarView {
    MainWindow* win = nullptr;

    static void OnTocRowClick(SidebarView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t rowIdx);
    static void OnTocRowDown(SidebarView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev, int64_t rowIdx);
    static void OnTocScroll(SidebarView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
    static void OnTocFilter(SidebarView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnFilterKey(SidebarView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnTreeDown(SidebarView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev, int64_t isToc);
    static void OnTreeUp(SidebarView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev, int64_t isToc);
    static void OnTocRowHover(SidebarView* self, gp::Ctx* cx, const gp::HoverEvent* ev, int64_t rowIdx);
    static void OnFavRowHover(SidebarView* self, gp::Ctx* cx, const gp::HoverEvent* ev, int64_t rowIdx);
    static void OnTocClose(SidebarView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnTocAction(SidebarView* self, gp::Ctx* cx, const gp::ActionEvent* ev);

    static void OnFavRowClick(SidebarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t rowIdx);
    static void OnFavRowDown(SidebarView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev, int64_t rowIdx);
    static void OnFavScroll(SidebarView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
    static void OnFavFilter(SidebarView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnFavClose(SidebarView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnFavAction(SidebarView* self, gp::Ctx* cx, const gp::ActionEvent* ev);
    static void OnFavSplitter(SidebarView* self, gp::Ctx* cx, const gp::DragMoveEvent* ev);
    static void OnFavSplitterDone(SidebarView* self, gp::Ctx* cx, const gp::MouseUpEvent*);
    static void OnThumbClick(SidebarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t pageNo);
    static void OnThumbScroll(SidebarView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
    static void OnThumbClose(SidebarView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnPanelView(SidebarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t arg);
};

// one action per pane, carrying the context-menu command id
static uint32_t ActTocMenu() {
    static uint32_t id = gp::ActionOf(GStrL("sumatra::TocMenu"));
    return id;
}

static uint32_t ActFavMenu() {
    static uint32_t id = gp::ActionOf(GStrL("sumatra::FavMenu"));
    return id;
}

// --- state ------------------------------------------------------------------

static SidebarUI* Ui(MainWindow* win);

static void EnsureView(MainWindow* win, gp::Ctx* cx) {
    SidebarUI* ui = Ui(win);
    if (!ui->view.IsValid()) {
        ui->view = gp::EntityNewState<SidebarView>(cx->app);
        ui->tocFocus = gp::FocusHandleNew(cx);
        ui->favFocus = gp::FocusHandleNew(cx);
    }
    auto* view = (SidebarView*)gp::EntityGet(cx->app, ui->view.id);
    view->win = win;
}

static SidebarUI* Ui(MainWindow* win) {
    if (!win->sidebar) {
        win->sidebar = new SidebarUI();
    }
    return win->sidebar;
}

static gp::App* WinApp(MainWindow* win) {
    return win && win->gpuiWin ? win->gpuiWin->app : nullptr;
}

static gp::InputState* EnsureInput(MainWindow* win, gpui::InputState** slot, Str placeholder) {
    if (*slot) {
        return *slot;
    }
    auto* s = new gp::InputState();
    s->focus = gp::FocusHandleNew(WinApp(win));
    gp::InputSetPlaceholder(s, ToGpui(placeholder));
    *slot = s;
    return s;
}

static void DeleteThumbCache(SidebarThumbCache* cache) {
    for (SidebarThumb& thumb : cache->thumbs) {
        FreePixmap(thumb.bitmap);
        if (thumb.img) {
            gp::RenderImageRelease(thumb.img);
        }
    }
    VecReset(cache->thumbs);
    if (cache->renderEngine) {
        cache->renderEngine->Release();
    }
    delete cache;
}

static void DetachThumbCache(SidebarUI* ui) {
    SidebarThumbCache* cache = ui->thumbCache;
    ui->thumbCache = nullptr;
    if (!cache) {
        return;
    }
    AtomicIntSet(&cache->cancelRendering, 1);
    if (cache->workerRunning) {
        cache->deleteWhenWorkerFinishes = true;
    } else {
        DeleteThumbCache(cache);
    }
}

void SidebarDelete(MainWindow* win) {
    SidebarUI* ui = win->sidebar;
    if (!ui) {
        return;
    }
    delete ui->tocFilter;
    delete ui->favFilter;
    delete ui->favModel;
    DeleteMenuModel(ui->tocMenu);
    DeleteMenuModel(ui->favMenu);
    DetachThumbCache(ui);
    VecReset(ui->tocRows);
    VecReset(ui->favRows);
    delete ui;
    win->sidebar = nullptr;
}

void SidebarSetTocSelection(MainWindow* win, TocItem* item) {
    SidebarUI* ui = Ui(win);
    if (ui->tocSel == item) {
        return;
    }
    ui->tocSel = item;
    ui->tocReveal = item;
    logf("SidebarSetTocSelection: '%s'\n", item ? item->title : Str{});
    AppShellInvalidate(win);
}

TocItem* SidebarTocSelection(MainWindow* win) {
    return win->sidebar ? win->sidebar->tocSel : nullptr;
}

void SidebarRevealTocItem(MainWindow* win, TocItem* item) {
    if (!item) {
        return;
    }
    SidebarUI* ui = Ui(win);
    for (TocItem* p = item->parent; p; p = p->parent) {
        if (p->child && !p->IsExpanded()) {
            p->isOpenToggled = !p->isOpenToggled;
        }
    }
    ui->tocSel = item;
    ui->tocReveal = item;
    AppShellInvalidate(win);
}

void SidebarSetTocFilterText(MainWindow* win, Str s) {
    SidebarUI* ui = Ui(win);
    if (!ui->tocFilter) {
        return;
    }
    gp::InputSetValue(ui->tocFilter, ToGpui(s));
}

TempStr SidebarTocFilterTextTemp(MainWindow* win) {
    SidebarUI* ui = win->sidebar;
    if (!ui || !ui->tocFilter) {
        return {};
    }
    return str::DupTemp(FromGpui(gp::InputValue(ui->tocFilter)));
}

void SidebarSetTocRtl(MainWindow* win, bool isRtl) {
    Ui(win)->tocRtl = isRtl;
}

void SidebarSetFavModel(MainWindow* win, FavTreeModel* model) {
    SidebarUI* ui = Ui(win);
    if (ui->favModel == model) {
        return;
    }
    delete ui->favModel;
    ui->favModel = model;
    ui->favSel = nullptr;
    ui->favRightClick = nullptr;
    ui->favMenuValid = false;
    VecReset(ui->favRows);
}

FavTreeModel* SidebarFavModel(MainWindow* win) {
    return win->sidebar ? win->sidebar->favModel : nullptr;
}

TempStr SidebarFavFilterTextTemp(MainWindow* win) {
    SidebarUI* ui = win->sidebar;
    if (!ui || !ui->favFilter) {
        return {};
    }
    return str::DupTemp(FromGpui(gp::InputValue(ui->favFilter)));
}

void SidebarFocusFavorites(MainWindow* win) {
    Ui(win)->favWantFocus = true;
    AppShellInvalidate(win);
}

bool SidebarPanelVisible(MainWindow* win, bool top) {
    if (!win || !win->sidebar) {
        return false;
    }
    return top ? (win->uiState.tocVisible && win->CurrentTab() != nullptr) : win->uiState.favVisible;
}

bool SidebarPanelHasFocus(MainWindow* win, bool top) {
    if (!win || !win->gpuiWin || !win->sidebar) {
        return false;
    }
    gp::FocusHandle focus = top ? win->sidebar->tocFocus : win->sidebar->favFocus;
    return focus.IsValid() && gp::FocusHandleIsFocused(win->gpuiWin, focus);
}

// orig's FocusSidebarPanel: the keyboard goes to the panel's tree (or to the
// panel itself for thumbnails), not to its search field
void SidebarFocusPanel(MainWindow* win, bool top) {
    if (!SidebarPanelVisible(win, top) || !win->gpuiWin) {
        return;
    }
    gp::FocusHandle focus = top ? win->sidebar->tocFocus : win->sidebar->favFocus;
    if (!focus.IsValid()) {
        return;
    }
    gp::Window* gw = win->gpuiWin;
    if (gw->input) {
        gp::InputBlur(gw->input, gw->app, gw);
    }
    gp::FocusHandleFocus(gw, focus);
    AppShellInvalidate(win);
}

// F6 focuses the tree in the upper pane, not its search field.
void SidebarFocusTop(MainWindow* win) {
    if (!win || !win->sidebar) {
        return;
    }
    // ng: the handles belong to the panels, not to what they show
    SidebarFocusPanel(win, SidebarPanelVisible(win, true));
}

SidebarContent SidebarContentFromStr(Str s, SidebarContent fallback) {
    if (str::EqI(s, StrL("bookmarks"))) {
        return SidebarContent::Bookmarks;
    }
    if (str::EqI(s, StrL("thumbnails"))) {
        return SidebarContent::Thumbnails;
    }
    if (str::EqI(s, StrL("favorites"))) {
        return SidebarContent::Favorites;
    }
    return fallback;
}

Str SidebarContentToStr(SidebarContent content) {
    if (content == SidebarContent::Thumbnails) {
        return StrL("thumbnails");
    }
    if (content == SidebarContent::Favorites) {
        return StrL("favorites");
    }
    return StrL("bookmarks");
}

static bool ContentAvailable(MainWindow* win, SidebarContent content) {
    if (content == SidebarContent::Thumbnails) {
        return win && win->AsFixed();
    }
    if (content == SidebarContent::Favorites) {
        return CanAccessDisk();
    }
    return win && win->IsDocLoaded() && win->ctrl && win->ctrl->HasToc();
}

bool SidebarContentVisible(MainWindow* win, SidebarContent content) {
    if (!win) {
        return false;
    }
    WindowTab* tab = win->CurrentTab();
    bool top = tab && win->uiState.tocVisible && tab->sidebarContent == content;
    bool bottom = win->uiState.favVisible && win->sidebarBottomContent == content;
    return top || bottom;
}

static void SaveBottomContent(MainWindow* win) {
    str::ReplaceWithCopy(&gSettings->sidebarBottomView, SidebarContentToStr(win->sidebarBottomContent));
}

void SidebarResolveContents(MainWindow* win) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    if (!tab || tab->sidebarContent != win->sidebarBottomContent) {
        return;
    }
    SidebarContent order[] = {SidebarContent::Favorites, SidebarContent::Bookmarks, SidebarContent::Thumbnails};
    for (SidebarContent content : order) {
        if (content != tab->sidebarContent) {
            win->sidebarBottomContent = content;
            SaveBottomContent(win);
            return;
        }
    }
}

static void SetPanelContent(MainWindow* win, bool top, SidebarContent content) {
    WindowTab* tab = win->CurrentTab();
    SidebarContent old = top && tab ? tab->sidebarContent : win->sidebarBottomContent;
    SidebarContent other = top ? win->sidebarBottomContent : (tab ? tab->sidebarContent : SidebarContent::Bookmarks);
    if (content == other) {
        if (top) {
            win->sidebarBottomContent = old;
        } else if (tab) {
            tab->sidebarContent = old;
        }
    }
    if (top && tab) {
        tab->sidebarContent = content;
    } else {
        win->sidebarBottomContent = content;
    }
    SaveBottomContent(win);
}

static void ToggleContent(MainWindow* win, SidebarContent content) {
    if (!ContentAvailable(win, content)) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    bool topShows = tab && win->uiState.tocVisible && tab->sidebarContent == content;
    bool bottomShows = win->uiState.favVisible && win->sidebarBottomContent == content;
    if (topShows) {
        SetSidebarVisibility(win, false, win->uiState.favVisible);
        ScheduleSaveSettings();
        return;
    }
    if (bottomShows) {
        SetSidebarVisibility(win, win->uiState.tocVisible, false);
        ScheduleSaveSettings();
        return;
    }
    bool useTop = tab && !win->uiState.tocVisible;
    if (win->uiState.tocVisible && !win->uiState.favVisible) {
        useTop = false;
    } else if (win->uiState.tocVisible && win->uiState.favVisible) {
        useTop = tab != nullptr;
    }
    SetPanelContent(win, useTop, content);
    SetSidebarVisibility(win, useTop || win->uiState.tocVisible, !useTop || win->uiState.favVisible);
    if (content == SidebarContent::Favorites) {
        SidebarFocusFavorites(win);
    } else if (content == SidebarContent::Thumbnails) {
        // showing the pane puts the keyboard there, same as the favorites tree
        SidebarFocusPanel(win, useTop);
    }
    ScheduleSaveSettings();
}

void SidebarToggleBookmarks(MainWindow* win) {
    ToggleContent(win, SidebarContent::Bookmarks);
}

void SidebarToggleThumbnails(MainWindow* win) {
    ToggleContent(win, SidebarContent::Thumbnails);
}

void SidebarToggleFavorites(MainWindow* win) {
    ToggleContent(win, SidebarContent::Favorites);
}

static float ScrollToRow(float scrollY, float viewH, int idx, int nRows);

// orig's TocTreeKeyDown / FavTreeKeyDown: Esc clears the search and focuses
// the filter, Up on the first row goes back to the search box
static bool TreeKeyToFilter(MainWindow* win, gp::InputState* filter, int vkey, bool onFirstRow) {
    if (!filter || !win->gpuiWin) {
        return false;
    }
    if (vkey == VK_ESCAPE) {
        gp::InputSetValue(filter, GStrL(""));
    } else if (vkey != VK_UP || !onFirstRow) {
        return false;
    }
    gp::InputFocus(filter, win->gpuiWin->app, win->gpuiWin);
    AppShellInvalidate(win);
    return true;
}

static void TocSetExpanded(TocItem* item, bool expand) {
    if (item->child && item->IsExpanded() != expand) {
        item->isOpenToggled = !item->isOpenToggled;
    }
}

// orig's TreeViewExpandRecursively: the item's subtree only, or the item, the
// siblings after it and everything below them
static void TocExpandRecursively(TocItem* item, bool expand, bool subtree) {
    while (item) {
        TocSetExpanded(item, expand);
        TocExpandRecursively(item->child, expand, false);
        if (subtree) {
            return;
        }
        item = item->next;
    }
}

static void FavExpandRecursively(FavTreeItem* item, bool expand) {
    if (len(item->children) > 0) {
        item->isExpanded = expand;
    }
    for (FavTreeItem* c : item->children) {
        FavExpandRecursively(c, expand);
    }
}

// the native tree moves the selection to the node that swallowed it
static TocItem* TocVisibleAncestor(TocItem* root, TocItem* item) {
    TocItem* res = item;
    for (TocItem* p = item ? item->parent : nullptr; p && p != root; p = p->parent) {
        if (!p->IsExpanded()) {
            res = p;
        }
    }
    return res;
}

static FavTreeItem* FavVisibleAncestor(FavTreeItem* root, FavTreeItem* item) {
    FavTreeItem* res = item;
    for (FavTreeItem* p = item ? item->parent : nullptr; p && p != root; p = p->parent) {
        if (!p->isExpanded) {
            res = p;
        }
    }
    return res;
}

static void FavSelectAndReveal(MainWindow* win, SidebarUI* ui, FavTreeItem* item) {
    ui->favSel = item;
    int n = len(ui->favRows);
    for (int i = 0; i < n; i++) {
        if (ui->favRows[i].item == item) {
            ui->favScrollY = ScrollToRow(ui->favScrollY, ui->favView.h, i, n);
            break;
        }
    }
    AppShellInvalidate(win);
}

// orig's HandleKey (gui/win/TreeView.cpp): numpad * and / expand / collapse
// the selected subtree, the whole tree with Shift; numpad + and - are the
// native tree's own expand / collapse of the node
static bool TocTreeExpandKey(MainWindow* win, SidebarUI* ui, TocItem* item, int vkey, bool shift) {
    TocItem* root = ui->tocRows[0].item;
    if (vkey == VK_MULTIPLY) {
        TocExpandRecursively(shift ? root : item, true, !shift);
    } else if (vkey == VK_DIVIDE) {
        if (shift) {
            if (!root->next) {
                root = root->child;
            }
            TocExpandRecursively(root, false, false);
        } else {
            TocExpandRecursively(item, false, true);
        }
    } else if (vkey == VK_ADD) {
        TocSetExpanded(item, true);
    } else if (vkey == VK_SUBTRACT) {
        TocSetExpanded(item, false);
    } else {
        return false;
    }
    TocTree* tree = CurrentTocTree(win);
    ui->tocSel = TocVisibleAncestor(tree ? tree->root : nullptr, ui->tocSel);
    ui->tocReveal = ui->tocSel;
    return true;
}

static bool FavTreeExpandKey(SidebarUI* ui, FavTreeItem* item, int vkey, bool shift) {
    FavTreeItem* root = ui->favModel->root;
    if (vkey == VK_MULTIPLY) {
        if (shift) {
            for (FavTreeItem* c : root->children) {
                FavExpandRecursively(c, true);
            }
        } else {
            FavExpandRecursively(item, true);
        }
    } else if (vkey == VK_DIVIDE) {
        if (shift) {
            FavTreeItem* from = len(root->children) == 1 ? root->children[0] : root;
            for (FavTreeItem* c : from->children) {
                FavExpandRecursively(c, false);
            }
        } else {
            FavExpandRecursively(item, false);
        }
    } else if (vkey == VK_ADD) {
        item->isExpanded = len(item->children) > 0;
    } else if (vkey == VK_SUBTRACT) {
        item->isExpanded = false;
    } else {
        return false;
    }
    ui->favSel = FavVisibleAncestor(root, ui->favSel);
    return true;
}

// the keys the focused tree keeps (orig's tree accelerator table leaves the
// navigation keys without Ctrl / Alt to the control, Shift or not)
bool SidebarOnKeyDown(MainWindow* win, int vkey, bool ctrl, bool shift, bool alt) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    SidebarUI* ui = win ? win->sidebar : nullptr;
    if (!tab || !ui || ctrl || alt) {
        return false;
    }
    bool topFocused = ui->tocFocus.IsValid() && gp::FocusHandleIsFocused(win->gpuiWin, ui->tocFocus);
    bool bottomFocused = ui->favFocus.IsValid() && gp::FocusHandleIsFocused(win->gpuiWin, ui->favFocus);
    if (!topFocused && !bottomFocused) {
        return false;
    }
    SidebarContent content = topFocused ? tab->sidebarContent : win->sidebarBottomContent;
    if (content == SidebarContent::Bookmarks) {
        int n = len(ui->tocRows);
        if (n == 0) {
            return false;
        }
        int idx = 0;
        for (int i = 0; i < n; i++) {
            if (ui->tocRows[i].item == ui->tocSel) {
                idx = i;
                break;
            }
        }
        TocItem* item = ui->tocRows[idx].item;
        if (!shift && TreeKeyToFilter(win, ui->tocFilter, vkey, idx == 0)) {
            return true;
        }
        // the native tree's Enter expands / collapses the node, the whole
        // subtree with Shift (orig's TreeViewToggle)
        if (vkey == VK_RETURN) {
            if (item->child) {
                bool expand = !item->IsExpanded();
                if (shift) {
                    TocExpandRecursively(item, expand, true);
                } else {
                    TocSetExpanded(item, expand);
                }
                AppShellInvalidate(win);
            }
            return true;
        }
        if (TocTreeExpandKey(win, ui, item, vkey, shift)) {
            AppShellInvalidate(win);
            return true;
        }
        // the native tree has no use for Space, but keeps it
        if (vkey == VK_SPACE) {
            return true;
        }
        if (vkey == VK_LEFT && item->child && item->IsExpanded()) {
            item->isOpenToggled = !item->isOpenToggled;
            AppShellInvalidate(win);
            return true;
        }
        if (vkey == VK_RIGHT && item->child && !item->IsExpanded()) {
            item->isOpenToggled = !item->isOpenToggled;
            AppShellInvalidate(win);
            return true;
        }
        TocTree* tree = CurrentTocTree(win);
        // a top-level row's parent is the tree's invisible root, or nothing
        if (vkey == VK_LEFT && item->parent && item->parent != (tree ? tree->root : nullptr)) {
            item = item->parent;
        } else if (vkey == VK_RIGHT && item->child) {
            item = item->child;
        } else if (vkey == VK_UP) {
            item = ui->tocRows[std::max(0, idx - 1)].item;
        } else if (vkey == VK_DOWN) {
            item = ui->tocRows[std::min(n - 1, idx + 1)].item;
        } else if (vkey == VK_HOME) {
            item = ui->tocRows[0].item;
        } else if (vkey == VK_END) {
            item = ui->tocRows[n - 1].item;
        } else if (vkey == VK_LEFT || vkey == VK_RIGHT) {
            // nowhere to go: the key stays with the tree
            return true;
        } else {
            return false;
        }
        TocTreeItemSelectedByKey(win, item);
        return true;
    }
    if (content == SidebarContent::Favorites) {
        int n = len(ui->favRows);
        if (n == 0) {
            return false;
        }
        int idx = 0;
        for (int i = 0; i < n; i++) {
            if (ui->favRows[i].item == ui->favSel) {
                idx = i;
                break;
            }
        }
        FavTreeItem* item = ui->favRows[idx].item;
        bool hasChildren = len(item->children) > 0;
        if (!shift && TreeKeyToFilter(win, ui->favFilter, vkey, idx == 0)) {
            return true;
        }
        // Enter opens the selected favorite (FavTreeKeyDown)
        if (vkey == VK_RETURN) {
            GoToFavForTreeItem(win, item);
            AppShellInvalidate(win);
            return true;
        }
        if (FavTreeExpandKey(ui, item, vkey, shift)) {
            AppShellInvalidate(win);
            return true;
        }
        if (vkey == VK_SPACE) {
            return true;
        }
        if (vkey == VK_LEFT && hasChildren && item->isExpanded) {
            item->isExpanded = false;
            AppShellInvalidate(win);
            return true;
        }
        if (vkey == VK_RIGHT && hasChildren && !item->isExpanded) {
            item->isExpanded = true;
            AppShellInvalidate(win);
            return true;
        }
        if (vkey == VK_LEFT && item->parent && item->parent != ui->favModel->root) {
            item = item->parent;
        } else if (vkey == VK_RIGHT && hasChildren) {
            item = item->children[0];
        } else if (vkey == VK_UP) {
            item = ui->favRows[std::max(0, idx - 1)].item;
        } else if (vkey == VK_DOWN) {
            item = ui->favRows[std::min(n - 1, idx + 1)].item;
        } else if (vkey == VK_HOME) {
            item = ui->favRows[0].item;
        } else if (vkey == VK_END) {
            item = ui->favRows[n - 1].item;
        } else if (vkey == VK_LEFT || vkey == VK_RIGHT) {
            return true;
        } else {
            return false;
        }
        // moving the selection does not open the favorite; Enter does
        FavSelectAndReveal(win, ui, item);
        return true;
    }
    // orig's ThumbnailsTakeKey: Up / Down, with or without Shift
    if (content != SidebarContent::Thumbnails || (vkey != VK_UP && vkey != VK_DOWN)) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }
    int cols = (win->sidebarDx - kThumbGap) / (kThumbDx + kThumbGap);
    cols = limitValue(cols, 1, kThumbMaxCols);
    int delta = vkey == VK_UP ? -cols : cols;
    int pageNo = limitValue(ui->thumbSelectedPage + delta, 1, dm->PageCount());
    ui->thumbSelectedPage = pageNo;
    ui->thumbSelectionPinned = true;
    ui->thumbRevealPage = pageNo;
    dm->GoToPage(pageNo, 0, true);
    AppShellInvalidate(win);
    return true;
}

// ng: what the native tree view does for a typed character (its incremental
// search): the characters typed in quick succession are a prefix, the next
// visible row whose label starts with it is selected, and one character typed
// again steps through the rows that start with it. The time-out is comctl32's
// (four double-click times; not documented).
static int TypeAheadTimeoutMs() {
#if OS_WIN
    return (int)GetDoubleClickTime() * 4;
#else
    return 2000;
#endif
}

// cap is the allocated count; len alone can point past it
template <typename T>
static int LiveRowCount(const Vec<T>& rows) {
    int cap = rows.cap < 0 ? -rows.cap : rows.cap;
    if (!rows.els || rows.len <= 0 || cap <= 0) {
        return 0;
    }
    return rows.len < cap ? rows.len : cap;
}

static TocRow* TocRowPtr(SidebarUI* ui, int i) {
    if (i < 0 || i >= LiveRowCount(ui->tocRows)) {
        return nullptr;
    }
    return &ui->tocRows.els[i];
}

static FavRow* FavRowPtr(SidebarUI* ui, int i) {
    if (i < 0 || i >= LiveRowCount(ui->favRows)) {
        return nullptr;
    }
    return &ui->favRows.els[i];
}

bool SidebarOnChar(MainWindow* win, u32 ch) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    SidebarUI* ui = win ? win->sidebar : nullptr;
    if (!tab || !ui || ch < 0x20 || ch == 0x7f) {
        return false;
    }
    bool topFocused = SidebarPanelHasFocus(win, true);
    if (!topFocused && !SidebarPanelHasFocus(win, false)) {
        return false;
    }
    SidebarContent content = topFocused ? tab->sidebarContent : win->sidebarBottomContent;
    bool isToc = content == SidebarContent::Bookmarks;
    if (!isToc && content != SidebarContent::Favorites) {
        return false;
    }
    int n = isToc ? LiveRowCount(ui->tocRows) : LiveRowCount(ui->favRows);
    if (n <= 0) {
        return true;
    }
    double now = gp::TimeNow();
    if ((now - ui->typeAheadTime) * 1000.0 > TypeAheadTimeoutMs()) {
        ui->typeAheadLen = 0;
    }
    ui->typeAheadTime = now;
    char utf8[8];
    int nUtf8 = 0;
    if (ch < 0x80) {
        utf8[nUtf8++] = (char)ch;
    } else if (ch < 0x800) {
        utf8[nUtf8++] = (char)(0xc0 | (ch >> 6));
        utf8[nUtf8++] = (char)(0x80 | (ch & 0x3f));
    } else if (ch < 0x10000) {
        utf8[nUtf8++] = (char)(0xe0 | (ch >> 12));
        utf8[nUtf8++] = (char)(0x80 | ((ch >> 6) & 0x3f));
        utf8[nUtf8++] = (char)(0x80 | (ch & 0x3f));
    } else {
        utf8[nUtf8++] = (char)(0xf0 | (ch >> 18));
        utf8[nUtf8++] = (char)(0x80 | ((ch >> 12) & 0x3f));
        utf8[nUtf8++] = (char)(0x80 | ((ch >> 6) & 0x3f));
        utf8[nUtf8++] = (char)(0x80 | (ch & 0x3f));
    }
    // the same character again steps on instead of growing the prefix
    bool sameChar = ui->typeAheadLen == nUtf8 && memcmp(ui->typeAhead, utf8, (size_t)nUtf8) == 0;
    if (!sameChar && ui->typeAheadLen + nUtf8 <= (int)sizeof(ui->typeAhead)) {
        memcpy(ui->typeAhead + ui->typeAheadLen, utf8, (size_t)nUtf8);
        ui->typeAheadLen += nUtf8;
    }
    Str prefix{ui->typeAhead, ui->typeAheadLen};
    int idx = 0;
    for (int i = 0; i < n; i++) {
        if (isToc) {
            TocRow* row = TocRowPtr(ui, i);
            if (row && row->item == ui->tocSel) {
                idx = i;
                break;
            }
            continue;
        }
        FavRow* row = FavRowPtr(ui, i);
        if (row && row->item == ui->favSel) {
            idx = i;
            break;
        }
    }
    // a longer prefix may still match the selected row; a first or repeated
    // character moves on from it
    int start = ui->typeAheadLen > nUtf8 ? idx : idx + 1;
    for (int k = 0; k < n; k++) {
        int i = (start + k) % n;
        if (i < 0) {
            i += n;
        }
        if (isToc) {
            TocRow* row = TocRowPtr(ui, i);
            TocItem* item = row ? row->item : nullptr;
            if (!item || !str::StartsWithI(item->title, prefix)) {
                continue;
            }
            TocTreeItemSelectedByKey(win, item);
            break;
        }
        FavRow* row = FavRowPtr(ui, i);
        FavTreeItem* item = row ? row->item : nullptr;
        if (!item || !str::StartsWithI(item->text, prefix)) {
            continue;
        }
        FavSelectAndReveal(win, ui, item);
        break;
    }
    return true;
}

static int CountVisibleToc(TocItem* item) {
    int n = 0;
    for (; item; item = item->next) {
        n++;
        if (item->child && item->IsExpanded()) {
            n += CountVisibleToc(item->child);
        }
    }
    return n;
}

static TocItem* VisibleTocAt(TocItem* item, int& idx) {
    for (; item; item = item->next) {
        if (idx == 0) {
            return item;
        }
        idx--;
        if (item->child && item->IsExpanded()) {
            TocItem* found = VisibleTocAt(item->child, idx);
            if (found) {
                return found;
            }
        }
    }
    return nullptr;
}

// visible rows, and whether a bookmark is selected (the native tree's caret)
static TempStr TocProbeRows(MainWindow* win, TocItem* root) {
    SidebarUI* ui = win ? win->sidebar : nullptr;
    int sel = ui && ui->tocSel ? 1 : 0;
    return fmt("tocRows=%d tocSel=%d", root ? CountVisibleToc(root) : 0, sel);
}

// count / select / expand the bookmarks the native tree drives with TVM_*
TempStr SidebarTestToc(MainWindow* win, Str op, int arg) {
    // the bookmarks tree has no window of its own; tests focus it from here
    if (str::Eq(op, StrL("focus"))) {
        SidebarFocusPanel(win, true);
        return SidebarPanelHasFocus(win, true) ? StrL("ok") : StrL("ERR no-focus");
    }
    TocTree* tree = win ? CurrentTocTree(win) : nullptr;
    TocItem* root = tree && tree->root ? tree->root->child : nullptr;
    if (!root) {
        if (str::Eq(op, StrL("count"))) {
            return TocProbeRows(win, nullptr);
        }
        return StrL("ERR no-toc");
    }
    if (str::Eq(op, StrL("count"))) {
        return TocProbeRows(win, root);
    }
    // TVE_COLLAPSE on each top-level item; children stay expanded underneath
    if (str::Eq(op, StrL("collapse-roots"))) {
        for (TocItem* item = root; item; item = item->next) {
            TocSetExpanded(item, false);
        }
        AppShellInvalidate(win);
        return TocProbeRows(win, root);
    }
    SidebarUI* ui = Ui(win);
    if (str::Eq(op, StrL("sel"))) {
        int idx = arg;
        TocItem* item = VisibleTocAt(root, idx);
        if (!item) {
            return StrL("ERR no-row");
        }
        SidebarSetTocSelection(win, item);
        return StrL("ok");
    }
    // the same navigation a click on the row runs; ng has no SysTreeView32
    if (str::Eq(op, StrL("go"))) {
        int idx = arg;
        TocItem* item = VisibleTocAt(root, idx);
        if (!item) {
            return StrL("ERR no-row");
        }
        TocTreeItemClicked(win, item);
        return StrL("ok");
    }
    TocItem* sel = ui->tocSel;
    if (!sel) {
        return StrL("ERR no-sel");
    }
    if (str::Eq(op, StrL("expand"))) {
        TocSetExpanded(sel, true);
    } else if (str::Eq(op, StrL("collapse"))) {
        TocSetExpanded(sel, false);
    } else {
        return StrL("ERR op");
    }
    AppShellInvalidate(win);
    return fmt("tocRows=%d", CountVisibleToc(root));
}

TempStr SidebarStateTemp(MainWindow* win) {
    SidebarUI* ui = win ? win->sidebar : nullptr;
    if (!ui) {
        return StrL("sidebar=none");
    }
    gp::Bounds fv = ui->favView;
    WindowTab* tab = win->CurrentTab();
    return fmt(
        "tocRows=%d tocSel='%s' tocFilter='%s' favRows=%d favSel='%s' panelFocus=%d%d top=%s bottom=%s "
        "favView=%d,%d,%d,%d",
        len(ui->tocRows), ui->tocSel ? ui->tocSel->title : Str{}, ui->tocFilter ? SidebarTocFilterTextTemp(win) : Str{},
        len(ui->favRows), ui->favSel ? ui->favSel->text : Str{}, SidebarPanelHasFocus(win, true) ? 1 : 0,
        SidebarPanelHasFocus(win, false) ? 1 : 0,
        SidebarContentToStr(tab ? tab->sidebarContent : SidebarContent::Bookmarks),
        SidebarContentToStr(win->sidebarBottomContent), (int)fv.x, (int)fv.y, (int)fv.w, (int)fv.h);
}

// orig's WndProcTocFilterEdit / WndProcFavFilterEdit: Down moves into the
// tree, Esc clears the text or, when it is empty already, gives the tree the
// keyboard. ng: a capture listener, so the field does not take the key first
void SidebarView::OnFilterKey(SidebarView* self, gp::Ctx* cx, const gp::KeyEvent* ev) {
    MainWindow* win = self->win;
    SidebarUI* ui = win ? win->sidebar : nullptr;
    if (!ui || (ev->vk != VK_DOWN && ev->vk != VK_ESCAPE)) {
        return;
    }
    bool isToc = ui->tocFilter && gp::FocusHandleIsFocused(cx->win, ui->tocFilter->focus);
    bool isFav = ui->favFilter && gp::FocusHandleIsFocused(cx->win, ui->favFilter->focus);
    if (!isToc && !isFav) {
        return;
    }
    const_cast<gp::KeyEvent*>(ev)->propagate = false;
    gp::Notify(cx);
    AppShellInvalidate(win);
    WindowTab* tab = win->CurrentTab();
    SidebarContent content = isToc ? SidebarContent::Bookmarks : SidebarContent::Favorites;
    bool top = tab && win->uiState.tocVisible && tab->sidebarContent == content;
    bool inFavTab = isFav && tab && tab->IsFavoritesTab();
    gp::InputState* filter = isToc ? ui->tocFilter : ui->favFilter;
    if (ev->vk == VK_ESCAPE) {
        if (gp::InputValue(filter).len > 0) {
            gp::InputSetValue(filter, GStrL(""));
            // orig's onTextChanged restores the full tree
            if (isToc) {
                TocFilterChanged(win);
            } else {
                FavFilterChanged(win);
            }
            return;
        }
        // empty: stay in the edit (Favorites tab) or move the focus to the tree
        if (!inFavTab) {
            SidebarFocusPanel(win, top);
        }
        return;
    }
    if (isToc) {
        // move into the tree: first top-level bookmark
        if (len(ui->tocRows) > 0) {
            SidebarSetTocSelection(win, ui->tocRows[0].item);
        }
    } else if (ui->favModel && len(ui->favModel->root->children) > 0) {
        // move into the tree: first child of the first file node (or first row)
        FavTreeItem* first = ui->favModel->root->children[0];
        FavTreeItem* sel = first;
        if (len(first->children) > 0) {
            first->isExpanded = true;
            sel = first->children[0];
        }
        ui->favSel = sel;
        ui->favScrollY = 0;
    }
    if (inFavTab) {
        gp::InputBlur(filter, cx->app, cx->win);
        gp::FocusHandleFocus(cx->win, ui->favFocus);
        return;
    }
    SidebarFocusPanel(win, top);
}

// --- flattening -------------------------------------------------------------

static void FlattenToc(TocItem* item, int depth, Vec<TocRow>& out) {
    while (item) {
        VecAppend(out, TocRow{item, depth});
        if (item->child && item->IsExpanded()) {
            FlattenToc(item->child, depth + 1, out);
        }
        item = item->next;
    }
}

static void FlattenFav(FavTreeItem* item, int depth, Vec<FavRow>& out) {
    VecAppend(out, FavRow{item, depth});
    if (!item->isExpanded) {
        return;
    }
    for (FavTreeItem* c : item->children) {
        FlattenFav(c, depth + 1, out);
    }
}

// --- handlers ---------------------------------------------------------------

static TocItem* TocRowAt(SidebarUI* ui, intptr_t idx) {
    if (idx < 0 || idx >= ui->tocRows.len) {
        return nullptr;
    }
    return ui->tocRows[(int)idx].item;
}

static FavTreeItem* FavRowAt(SidebarUI* ui, intptr_t idx) {
    if (idx < 0 || idx >= ui->favRows.len) {
        return nullptr;
    }
    return ui->favRows[(int)idx].item;
}

// orig's tree toggles a node only when the click lands on its +/- box; a click
// anywhere else on the row navigates. gpui has no way to stop a click from
// bubbling out of the box, so the row decides from where it was clicked.
static bool ClickOnChevron(const gp::ClickEvent* ev, int depth) {
    float localX = ev->x - ev->el.x;
    return localX < kRowPadL + (float)depth * kIndentDx + kChevronDx;
}

void SidebarView::OnTocRowClick(SidebarView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t rowIdx) {
    MainWindow* win = self->win;
    SidebarUI* ui = Ui(win);
    if (rowIdx < 0 || rowIdx >= ui->tocRows.len) {
        return;
    }
    TocItem* item = ui->tocRows[(int)rowIdx].item;
    if (item->child && ClickOnChevron(ev, ui->tocRows[(int)rowIdx].depth)) {
        item->isOpenToggled = !item->isOpenToggled;
        logf("TocToggle: '%s' expanded %d\n", item->title, (int)item->IsExpanded());
        gp::Notify(cx);
        return;
    }
    // the native tree toggles a node on a double-click, and orig's TocTreeClick
    // ignores isDblClick: the first click of the pair already navigated
    if (ev->clickCount > 0 && (ev->clickCount % 2) == 0) {
        if (item->child) {
            item->isOpenToggled = !item->isOpenToggled;
        }
        gp::Notify(cx);
        return;
    }
    TocTreeItemClicked(win, item);
    gp::Notify(cx);
}

void SidebarView::OnTocRowDown(SidebarView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev, int64_t rowIdx) {
    if (!IsContextClick(ev->button, ev->modifiers)) {
        return;
    }
    SidebarUI* ui = Ui(self->win);
    ui->tocRightClick = TocRowAt(ui, rowIdx);
    ui->rdownOnRow = true;
    gp::Notify(cx);
}

// ng: gpui's ContextMenu would open from the press; orig's tree shows its menu
// from WM_CONTEXTMENU, when the button comes up
void SidebarView::OnTreeDown(SidebarView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev, int64_t isToc) {
    if (!IsContextClick(ev->button, ev->modifiers)) {
        return;
    }
    gp::WindowStopPropagation(cx);
    SidebarUI* ui = Ui(self->win);
    if (!ui->rdownOnRow) {
        // the empty area under the rows: a menu without the item rows
        if (isToc) {
            ui->tocRightClick = nullptr;
        } else {
            ui->favRightClick = nullptr;
        }
    }
    ui->rdownOnRow = false;
}

// orig's TocContextMenu / FavTreeContextMenu via GetOrSelectTreeItemAtPos: the
// context menu acts on this item so select it for better visual feedback
void SidebarView::OnTreeUp(SidebarView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev, int64_t isToc) {
    if (!IsContextClick(ev->button, ev->modifiers)) {
        return;
    }
    MainWindow* win = self->win;
    SidebarUI* ui = Ui(win);
    if (isToc) {
        if (ui->tocRightClick) {
            SidebarSetTocSelection(win, ui->tocRightClick);
            ui->tocReveal = nullptr;
        }
        ui->tocMenuValid = false;
        OpenPopupMenuAt(cx, ui->tocPopup, ev->x - ui->tocView.x, ev->y - ui->tocView.y);
    } else {
        if (ui->favRightClick) {
            ui->favSel = ui->favRightClick;
        }
        ui->favMenuValid = false;
        OpenPopupMenuAt(cx, ui->favPopup, ev->x - ui->favView.x, ev->y - ui->favView.y);
    }
    AppShellInvalidate(win);
    gp::Notify(cx);
}

bool SidebarContextMenuFromKey(MainWindow* win, gp::Ctx* cx) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    SidebarUI* ui = win ? win->sidebar : nullptr;
    if (!tab || !ui) {
        return false;
    }
    bool topFocused = SidebarPanelHasFocus(win, true);
    if (!topFocused && !SidebarPanelHasFocus(win, false)) {
        return false;
    }
    SidebarContent content = topFocused ? tab->sidebarContent : win->sidebarBottomContent;
    // no mouse position when launched via keyboard shortcut
    // use position of selected item to show menu
    if (content == SidebarContent::Bookmarks) {
        for (int i = 0; i < len(ui->tocRows); i++) {
            if (ui->tocRows[i].item != ui->tocSel) {
                continue;
            }
            ui->tocRightClick = ui->tocSel;
            ui->tocMenuValid = false;
            float x = kRowPadL + (float)ui->tocRows[i].depth * kIndentDx + kChevronDx;
            OpenPopupMenuAt(cx, ui->tocPopup, x, (float)(i + 1) * kRowDy - ui->tocScrollY);
            break;
        }
    } else if (content == SidebarContent::Favorites) {
        for (int i = 0; i < len(ui->favRows); i++) {
            if (ui->favRows[i].item != ui->favSel) {
                continue;
            }
            ui->favRightClick = ui->favSel;
            ui->favMenuValid = false;
            float x = kRowPadL + (float)ui->favRows[i].depth * kIndentDx + kChevronDx;
            OpenPopupMenuAt(cx, ui->favPopup, x, (float)(i + 1) * kRowDy - ui->favScrollY);
            break;
        }
    }
    AppShellInvalidate(win);
    return true;
}

// ng: gpui measures text only while painting, so whether a label is cut off
// is estimated from its length (13 px UI font, about 6.5 px a character)
static bool RowLabelTruncated(Str text, int depth, float viewDx, float extraDx) {
    float avail = viewDx - (kRowPadL + (float)depth * kIndentDx + kChevronDx) - 6 - extraDx;
    return (float)len(text) * 6.5f > avail;
}

static gp::Bounds RowBounds(const gp::Bounds& view, float scrollY, int idx) {
    gp::Bounds b = view;
    b.y = view.y + (float)idx * kRowDy - scrollY;
    b.h = kRowDy;
    return b;
}

// orig's TocCustomizeTooltip: where a link out of the document goes; and the
// native tree's own tip, the full label of a row that does not fit
void SidebarView::OnTocRowHover(SidebarView* self, gp::Ctx* cx, const gp::HoverEvent* ev, int64_t rowIdx) {
    MainWindow* win = self->win;
    SidebarUI* ui = Ui(win);
    TocItem* item = ev->hovered ? TocRowAt(ui, rowIdx) : nullptr;
    if (!item) {
        HoverTooltipHide(cx);
        return;
    }
    TempStr tip;
    IPageDestination* link = item->dest;
    Kind k = link ? link->GetKind() : nullptr;
    if (link && k != kindDestinationScrollTo && k != kindDestinationNone) {
        Str path = link->GetValue();
        if (len(path) == 0) {
            path = item->title;
        }
        if (len(path) > 0) {
            if (kindDestinationLaunchEmbedded == k || kindDestinationAttachment == k) {
                tip = fmt(Tr("Attachment: %s").s, path);
            } else {
                tip = str::DupTemp(path);
            }
        }
    }
    float pageDx = gSettings->showTocPageNumbers && item->pageNo > 0 ? 40.f : 0.f;
    if (len(tip) == 0 && RowLabelTruncated(item->title, ui->tocRows[(int)rowIdx].depth, ui->tocView.w, pageDx)) {
        tip = str::DupTemp(item->title);
    }
    if (len(tip) == 0) {
        HoverTooltipHide(cx);
        return;
    }
    HoverTooltipShow(cx, tip, RowBounds(ui->tocView, ui->tocScrollY, (int)rowIdx));
}

void SidebarView::OnFavRowHover(SidebarView* self, gp::Ctx* cx, const gp::HoverEvent* ev, int64_t rowIdx) {
    SidebarUI* ui = Ui(self->win);
    FavTreeItem* item = ev->hovered ? FavRowAt(ui, rowIdx) : nullptr;
    if (!item || !RowLabelTruncated(item->text, ui->favRows[(int)rowIdx].depth, ui->favView.w, 0)) {
        HoverTooltipHide(cx);
        return;
    }
    HoverTooltipShow(cx, item->text, RowBounds(ui->favView, ui->favScrollY, (int)rowIdx));
}

void SidebarView::OnTocScroll(SidebarView* self, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    Ui(self->win)->tocScrollY = ev->offsetY;
    gp::Notify(cx);
}

void SidebarView::OnTocFilter(SidebarView* self, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::Change) {
        return;
    }
    TocFilterChanged(self->win);
    gp::Notify(cx);
}

void SidebarView::OnTocClose(SidebarView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    ToggleTocBox(self->win);
    gp::Notify(cx);
}

void SidebarView::OnTocAction(SidebarView* self, gp::Ctx* cx, const gp::ActionEvent* ev) {
    MainWindow* win = self->win;
    TocContextMenuCommand(win, Ui(win)->tocRightClick, (int)ev->arg);
    gp::Notify(cx);
}

void SidebarView::OnFavRowClick(SidebarView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t rowIdx) {
    MainWindow* win = self->win;
    SidebarUI* ui = Ui(win);
    FavTreeItem* item = FavRowAt(ui, rowIdx);
    if (!item) {
        return;
    }
    ui->favSel = item;
    // Parent rows with children: leave expand/collapse to the tree; only
    // navigate when the click is a leaf (or a single-favorite file row).
    // The tree toggles from its +/- box and on a double-click
    if (len(item->children) > 0) {
        bool isDblClick = ev->clickCount > 0 && (ev->clickCount % 2) == 0;
        if (isDblClick || ClickOnChevron(ev, ui->favRows[(int)rowIdx].depth)) {
            item->isExpanded = !item->isExpanded;
        }
        gp::Notify(cx);
        return;
    }
    GoToFavForTreeItem(win, item);
    gp::Notify(cx);
}

void SidebarView::OnFavRowDown(SidebarView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev, int64_t rowIdx) {
    if (!IsContextClick(ev->button, ev->modifiers)) {
        return;
    }
    SidebarUI* ui = Ui(self->win);
    ui->favRightClick = FavRowAt(ui, rowIdx);
    ui->rdownOnRow = true;
    gp::Notify(cx);
}

void SidebarView::OnFavScroll(SidebarView* self, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    Ui(self->win)->favScrollY = ev->offsetY;
    gp::Notify(cx);
}

void SidebarView::OnFavFilter(SidebarView* self, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::Change) {
        return;
    }
    FavFilterChanged(self->win);
    gp::Notify(cx);
}

// orig's OnCloseClick: hides the panel; on the Favorites tab, closes the tab
void SidebarView::OnFavClose(SidebarView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    MainWindow* win = self->win;
    WindowTab* tab = win->CurrentTab();
    if (tab && tab->IsFavoritesTab()) {
        if (WindowTab* favTab = FindFavoritesTab(win)) {
            CloseTab(favTab, false);
        }
        gp::Notify(cx);
        return;
    }
    ToggleFavorites(win);
    gp::Notify(cx);
}

void SidebarView::OnFavAction(SidebarView* self, gp::Ctx* cx, const gp::ActionEvent* ev) {
    MainWindow* win = self->win;
    FavContextMenuCommand(win, Ui(win)->favRightClick, (int)ev->arg);
    gp::Notify(cx);
}

// orig's OnFavSplitterMove: the bookmarks pane ends where the cursor is
void SidebarView::OnFavSplitter(SidebarView* self, gp::Ctx* cx, const gp::DragMoveEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    int contentDy = win->canvasRc.dy;
    int tocDy = (int)ev->event.y - win->canvasRc.y;
    tocDy = limitValue(tocDy, kTocMinDy, std::max(kTocMinDy, contentDy - kTocMinDy));
    if (tocDy == gSettings->tocDy) {
        return;
    }
    logf("OnFavSplitter: tocDy %d\n", tocDy);
    gSettings->tocDy = tocDy;
    gp::Notify(cx);
}

void SidebarView::OnFavSplitterDone(SidebarView*, gp::Ctx* cx, const gp::MouseUpEvent*) {
    logf("OnFavSplitterDone: tocDy %d\n", gSettings->tocDy);
    ScheduleSaveSettings();
    gp::Notify(cx);
}

void SidebarView::OnThumbClick(SidebarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t pageNo) {
    DisplayModel* dm = self->win->AsFixed();
    if (!dm || pageNo < 1 || pageNo > dm->PageCount()) {
        return;
    }
    dm->GoToPage((int)pageNo, 0, true);
    SidebarUI* ui = Ui(self->win);
    ui->thumbSelectedPage = (int)pageNo;
    ui->thumbSelectionPinned = true;
    ui->thumbRevealPage = (int)pageNo;
    gp::Notify(cx);
}

void SidebarView::OnThumbScroll(SidebarView* self, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    Ui(self->win)->thumbScrollY = ev->offsetY;
    gp::Notify(cx);
}

void SidebarView::OnThumbClose(SidebarView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    SidebarToggleThumbnails(self->win);
    gp::Notify(cx);
}

void SidebarView::OnPanelView(SidebarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t arg) {
    bool top = (arg & 4) == 0;
    SidebarContent content = (SidebarContent)(arg & 3);
    if (!ContentAvailable(self->win, content)) {
        return;
    }
    SetPanelContent(self->win, top, content);
    if (content == SidebarContent::Bookmarks) {
        LoadTocTree(self->win);
    } else if (content == SidebarContent::Favorites) {
        PopulateFavTreeIfNeeded(self->win);
    }
    ScheduleSaveSettings();
    gp::Notify(cx);
}

// --- building ---------------------------------------------------------------

static gpc::PopupMenu* PopupFromModel(gp::Ctx* cx, MenuModel* model, Str id, uint32_t action) {
    gpc::PopupMenu* menu = gpc::PopupMenu::New(cx, GpuiDup(cx->a, id))->MinW(200);
    for (const MenuItemModel& it : model->items) {
        if (it.separator) {
            menu->Separator();
            continue;
        }
        menu->MenuWithAction(GpuiDup(cx->a, it.title), action, (intptr_t)it.cmdId);
        menu->Disabled(it.disabled);
        menu->Checked(it.checked);
    }
    return menu;
}

static gp::El* PaneHeader(MainWindow* win, gp::Ctx* cx, bool selectors, bool top, Str title, Str closeId,
                          gp::Listener onClose) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* row = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->H(kPaneHeaderDy)->Shrink0()->ItemsCenter()->PadX(6)->Gap(4);
    if (selectors) {
        SidebarContent selected =
            top && win->CurrentTab() ? win->CurrentTab()->sidebarContent : win->sidebarBottomContent;
        SidebarContent contents[] = {SidebarContent::Bookmarks, SidebarContent::Thumbnails, SidebarContent::Favorites};
        gp::IconName icons[] = {gp::IconName::BookOpen, gp::IconName::GalleryVerticalEnd, gp::IconName::Star};
        Str tips[] = {Tr("Bookmarks"), Tr("Thumbnails"), Tr("Favorites")};
        for (int i = 0; i < dimofi(contents); i++) {
            int64_t arg = (top ? 0 : 4) | (int)contents[i];
            auto* button = gpc::Button::New(cx, GpuiDup(cx->a, fmt("sidebar-view-%d-%d", top ? 0 : 1, i)))
                               ->Icon(icons[i])
                               ->Ghost()
                               ->Compact()
                               ->WithSize(gp::UiSize::XSmall)
                               ->Selected(selected == contents[i])
                               ->Disabled(!ContentAvailable(win, contents[i]))
                               ->Tooltip(ToGpui(tips[i]))
                               ->OnClick(gp::ListenTo(Ui(win)->view, &SidebarView::OnPanelView, arg));
            row->Child(button->IntoEl()->BoundsOut(&Ui(win)->viewIconBounds[top ? 0 : 1][i]));
        }
        row->Child(gp::Div(cx->a)->Flex1());
    } else {
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, title))->Font(12)->Bold()->Fg(th.mutedFg)->Flex1()->Truncate());
    }
    row->Child(gpc::Button::New(cx, GpuiDup(cx->a, closeId))
                   ->Icon(gp::IconName::Close)
                   ->Ghost()
                   ->Compact()
                   ->WithSize(gp::UiSize::XSmall)
                   ->Tooltip(ToGpui(Tr("Close")))
                   ->OnClick(onClose)
                   ->IntoEl());
    return row;
}

// scroll `scrollY` so that row `idx` is inside the viewport
static float ScrollToRow(float scrollY, float viewH, int idx, int nRows) {
    if (idx < 0 || viewH <= 0) {
        return scrollY;
    }
    float top = (float)idx * kRowDy;
    float maxScroll = std::max(0.f, (float)nRows * kRowDy - viewH);
    if (top < scrollY) {
        scrollY = top;
    } else if (top + kRowDy > scrollY + viewH) {
        scrollY = top + kRowDy - viewH;
    }
    return std::max(0.f, std::min(scrollY, maxScroll));
}

// a row's expand / collapse marker; the row's own click handler decides whether
// the click landed on it (see ClickOnChevron)
// orig's ResolveTreeFilterItemColors: the selected row of a focused tree is
// the system highlight; selected but unfocused (and a multi-match "current
// page" row) is a subtle accent of the tree background
static void TreeRowColors(bool isSelected, bool hasFocus, Color* bgOut, Color* txtOut) {
    if (isSelected && hasFocus) {
        *bgOut = SysHighlightBgColor();
        *txtOut = SysHighlightTextColor();
        return;
    }
    *txtOut = ThemeWindowTextColor();
    if (isSelected) {
        *bgOut = AccentColor(ThemeControlBackgroundColor(), 40);
        return;
    }
    *bgOut = kColorUnset;
}

// the Explorer theme's hot-tracked item (TVS_TRACKSELECT). Its light blue is
// a fixed color of the visual style; other themes get an accent of their own
// background instead of a light bar under light text
static Color TreeRowHotColor() {
    if (IsCurrentThemeDefault()) {
        return MkRgb(0xe5, 0xf3, 0xff);
    }
    return AccentColor(ThemeControlBackgroundColor(), 20);
}

// the part of a row orig's custom draw fills: from the label to the right
// edge, not the indent and the expand glyph
// orig fills the whole row of a selected / current-page item
// (ResolveTreeFilterItemColors on the item rect); the hot color is the label's
static gp::El* TreeRowLabelBox(gp::Ctx* cx, gp::El* row, Color bg) {
    gp::El* box = gp::Div(cx->a)->FlexRow()->Flex1()->MinW(0)->H(gp::kFill)->ItemsCenter()->PadX(2);
    if (bg != kColorUnset) {
        row->Bg(ToGpui(bg));
    } else {
        box->HoverBg(ToGpui(TreeRowHotColor()));
    }
    return box;
}

// orig's tree has the Explorer visual style (SetWindowTheme(hwnd, L"Explorer")),
// whose expand glyph on Windows 10 / 11 is a chevron in the text color: ">"
// collapsed, "v" expanded. gpui's chevron icons at 16 px are that size
static gp::El* RowChevron(gp::Ctx* cx, bool hasChildren, bool expanded, Color col) {
    gp::El* box = gp::Div(cx->a)->W(kChevronDx)->H(kRowDy)->Shrink0()->ItemsCenter()->PadL(1);
    if (!hasChildren) {
        return box;
    }
    gp::IconName ic = expanded ? gp::IconName::ChevronDown : gp::IconName::ChevronRight;
    box->Child(gp::IconEl(cx->a, ic, 16)->Fg(ToGpui(col)));
    return box;
}

struct SidebarThumbTask {
    SidebarThumbCache* cache = nullptr;
    int pageNo = 0;
    Pixmap* bitmap = nullptr;
};

struct SidebarThumbWorker {
    SidebarThumbCache* cache = nullptr;
    Vec<int> pages;
    Vec<Location> locs;
};

static Pixmap* RenderSidebarThumb(EngineBase* engine, int pageNo, Location loc, int rotation) {
    int boxPage = loc.IsValid() && engine->isReflowable ? 1 : pageNo;
    RectF pageRect = engine->PageMediabox(boxPage);
    if (pageRect.IsEmpty()) {
        return nullptr;
    }
    pageRect = engine->Transform(pageRect, boxPage, 1.0f, rotation);
    if (pageRect.dx <= 0 || pageRect.dy <= 0) {
        return nullptr;
    }
    float zoom = (float)kThumbDx / pageRect.dx;
    pageRect.dy = std::min(pageRect.dy, (float)kThumbDy / zoom);
    pageRect = engine->Transform(pageRect, boxPage, 1.0f, rotation, true);
    RenderPageArgs args(pageNo, zoom, rotation, &pageRect, RenderTarget::View);
    args.loc = loc;
    return PixmapToBgr(engine->RenderPage(args));
}

static void FinishSidebarThumb(SidebarThumbTask* task) {
    SidebarThumbCache* cache = task->cache;
    int idx = task->pageNo - 1;
    if (!cache->deleteWhenWorkerFinishes && idx >= 0 && idx < len(cache->thumbs)) {
        SidebarThumb& thumb = cache->thumbs[idx];
        if (task->bitmap) {
            FreePixmap(thumb.bitmap);
            thumb.bitmap = task->bitmap;
            task->bitmap = nullptr;
            if (thumb.img) {
                gp::RenderImageRelease(thumb.img);
                thumb.img = nullptr;
            }
        } else {
            thumb.failed = true;
        }
        AppShellInvalidate(cache->win);
    }
    FreePixmap(task->bitmap);
    delete task;
}

static void FinishSidebarWorker(SidebarThumbCache* cache) {
    cache->workerRunning = false;
    if (cache->deleteWhenWorkerFinishes) {
        DeleteThumbCache(cache);
        return;
    }
    AppShellInvalidate(cache->win);
}

static void RenderSidebarThumbs(SidebarThumbWorker* worker) {
    SidebarThumbCache* cache = worker->cache;
    if (cache->renderEngine) {
        for (int i = 0; i < len(worker->pages); i++) {
            if (AtomicIntGet(&cache->cancelRendering) != 0) {
                break;
            }
            auto* task = new SidebarThumbTask;
            task->cache = cache;
            task->pageNo = worker->pages[i];
            task->bitmap = RenderSidebarThumb(cache->renderEngine, task->pageNo, worker->locs[i], cache->rotation);
            uitask::Post(MkFunc0<SidebarThumbTask>(FinishSidebarThumb, task));
        }
    }
    uitask::Post(MkFunc0<SidebarThumbCache>(FinishSidebarWorker, cache));
    delete worker;
}

static SidebarThumbCache* EnsureThumbCache(MainWindow* win) {
    SidebarUI* ui = Ui(win);
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        DetachThumbCache(ui);
        return nullptr;
    }
    int rotation = dm->GetRotation();
    int pageCount = dm->PageCount();
    SidebarThumbCache* cache = ui->thumbCache;
    if (cache && (cache->owner != dm || cache->rotation != rotation || len(cache->thumbs) != pageCount)) {
        DetachThumbCache(ui);
        cache = nullptr;
    }
    if (cache) {
        return cache;
    }
    cache = new SidebarThumbCache;
    cache->owner = dm;
    cache->win = win;
    cache->rotation = rotation;
    cache->renderEngine = dm->GetEngine();
    cache->renderEngine->AddRef();
    for (int i = 0; i < pageCount; i++) {
        VecAppend(cache->thumbs, SidebarThumb{});
    }
    ui->thumbCache = cache;
    return cache;
}

static void StartSidebarThumbs(MainWindow* win, int firstPage, int lastPage) {
    SidebarThumbCache* cache = EnsureThumbCache(win);
    DisplayModel* dm = win->AsFixed();
    if (!cache || !dm || cache->workerRunning) {
        return;
    }
    auto* worker = new SidebarThumbWorker;
    EngineBase* engine = dm->GetEngine();
    firstPage = limitValue(firstPage, 1, dm->PageCount());
    lastPage = limitValue(lastPage, firstPage, dm->PageCount());
    for (int pageNo = firstPage; pageNo <= lastPage; pageNo++) {
        SidebarThumb& thumb = cache->thumbs[pageNo - 1];
        if (thumb.bitmap || thumb.failed) {
            continue;
        }
        VecAppend(worker->pages, pageNo);
        VecAppend(worker->locs, engine->LocationFromPageNo(pageNo));
    }
    if (len(worker->pages) == 0) {
        delete worker;
        return;
    }
    worker->cache = cache;
    cache->workerRunning = true;
    AtomicIntSet(&cache->cancelRendering, 0);
    RunAsync(MkFunc0<SidebarThumbWorker>(RenderSidebarThumbs, worker), StrL("SidebarThumbnailRender"));
}

void SidebarRefreshThumbnailPage(MainWindow* win, int pageNo) {
    SidebarUI* ui = win ? win->sidebar : nullptr;
    SidebarThumbCache* cache = ui ? ui->thumbCache : nullptr;
    int idx = pageNo - 1;
    if (!cache || idx < 0 || idx >= len(cache->thumbs)) {
        return;
    }
    if (cache->workerRunning) {
        DetachThumbCache(ui);
        return;
    }
    SidebarThumb& thumb = cache->thumbs[idx];
    FreePixmap(thumb.bitmap);
    thumb.bitmap = nullptr;
    if (thumb.img) {
        gp::RenderImageRelease(thumb.img);
        thumb.img = nullptr;
    }
    thumb.failed = false;
    StartSidebarThumbs(win, pageNo, pageNo);
}

static gp::ImageLoadState SidebarThumbLoad(gp::PaintApp* pa, void* user, gp::RenderImage** imgOut) {
    auto* thumb = (SidebarThumb*)user;
    if (!thumb->img && thumb->bitmap) {
        thumb->img = RenderImageFromPixmap(pa, thumb->bitmap);
    }
    *imgOut = thumb->img;
    return thumb->img ? gp::ImageLoadState::Ready : gp::ImageLoadState::Loading;
}

static gp::El* BuildThumbPane(MainWindow* win, gp::Ctx* cx, bool top) {
    SidebarUI* ui = Ui(win);
    DisplayModel* dm = win->AsFixed();
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* pane = gp::Div(cx->a)->FlexCol()->SizeFull()->Bg(th.tokens.sidebar);
    pane->Child(PaneHeader(win, cx, true, top, Tr("Thumbnails"), StrL("thumb-close"),
                           gp::ListenTo(ui->view, &SidebarView::OnThumbClose)));
    if (!dm) {
        return pane;
    }
    SidebarThumbCache* cache = EnsureThumbCache(win);
    int pageCount = dm->PageCount();
    VecResize(ui->thumbBounds, pageCount);
    for (int i = 0; i < pageCount; i++) {
        ui->thumbBounds[i] = {};
    }
    int currentPage = dm->CurrentPageNo();
    int cols = (win->sidebarDx - kThumbGap) / (kThumbDx + kThumbGap);
    cols = limitValue(cols, 1, kThumbMaxCols);
    int rows = (pageCount + cols - 1) / cols;
    float rowDy = (float)(kThumbDy + kThumbGap);
    bool keepSelected = ui->thumbSelectionPinned && dm->PageVisible(ui->thumbSelectedPage);
    if (!keepSelected && ui->thumbSelectedPage != currentPage) {
        ui->thumbSelectedPage = currentPage;
        ui->thumbRevealPage = currentPage;
        ui->thumbSelectionPinned = false;
    }
    float viewH = ui->thumbView.h > 0 ? ui->thumbView.h : 400;
    if (ui->thumbRevealPage > 0) {
        int row = (ui->thumbRevealPage - 1) / cols;
        float maxScroll = std::max(0.f, (float)rows * rowDy - viewH);
        ui->thumbScrollY = std::max(0.f, std::min((float)row * rowDy - (viewH - rowDy) / 2, maxScroll));
        ui->thumbRevealPage = 0;
    }
    int firstRow = std::max(0, (int)(ui->thumbScrollY / rowDy) - 1);
    int endRow = std::min(rows, (int)((ui->thumbScrollY + viewH) / rowDy) + 2);
    gp::El* grid = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Gap((float)kThumbGap)->Pad((float)kThumbGap);
    if (firstRow > 0) {
        grid->Child(gp::Div(cx->a)->H((float)firstRow * rowDy)->Shrink0());
    }
    for (int row = firstRow; row < endRow; row++) {
        gp::El* rowEl = gp::Div(cx->a)->FlexRow()->Gap((float)kThumbGap)->JustifyCenter()->Shrink0();
        for (int col = 0; col < cols; col++) {
            int pageNo = row * cols + col + 1;
            if (pageNo > pageCount) {
                break;
            }
            SidebarThumb* thumb = cache ? &cache->thumbs[pageNo - 1] : nullptr;
            gp::El* cell = gp::Div(cx->a)
                               ->FlexCol()
                               ->W((float)kThumbDx)
                               ->H((float)kThumbDy)
                               ->Shrink0()
                               ->ItemsCenter()
                               ->JustifyCenter()
                               ->Bg(gp::Rgba{0xff, 0xff, 0xff, 0xff})
                               ->PathClick(GpuiDup(cx->a, fmt("sidebar-thumb-%d", pageNo)))
                               ->BoundsOut(&ui->thumbBounds[pageNo - 1])
                               ->OnClick(gp::ListenTo(ui->view, &SidebarView::OnThumbClick, (intptr_t)pageNo));
            if (pageNo == ui->thumbSelectedPage) {
                cell->Border(3, gp::Rgba{0, 120, 215, 0xff});
            }
            if (thumb && thumb->bitmap) {
                gp::ImageSource src = gp::ImageSource::FromCustom(SidebarThumbLoad, thumb);
                cell->Child(gp::ImageEl(cx->a, src, GStrL(""))->SizeFull()->ObjectFitMode(gp::ObjectFit::Contain));
            }
            cell->Child(gp::TextEl(cx->a, GpuiDup(cx->a, fmt("%d", pageNo)))
                            ->Font(12)
                            ->Fg(th.foreground)
                            ->Bg(th.tokens.background)
                            ->Radius(8)
                            ->PadX(6)
                            ->Absolute()
                            ->Bottom(4));
            rowEl->Child(cell);
        }
        grid->Child(rowEl);
    }
    if (endRow < rows) {
        grid->Child(gp::Div(cx->a)->H((float)(rows - endRow) * rowDy)->Shrink0());
    }
    gp::El* scroll = gp::Div(cx->a)
                         ->Id(GStrL("page-thumbnails"))
                         ->TrackFocus(top ? ui->tocFocus : ui->favFocus)
                         ->FocusOnPress()
                         ->TabStop(false)
                         ->FocusRing()
                         ->FlexCol()
                         ->Flex1()
                         ->W(gp::kFill)
                         ->MinH(0)
                         ->ScrollY(ui->thumbScrollY)
                         ->ScrollFromPath()
                         ->OnScroll(gp::ListenTo(ui->view, &SidebarView::OnThumbScroll))
                         ->BoundsOut(&ui->thumbView)
                         ->Child(grid);
    pane->Child(scroll);
    int firstPage = firstRow * cols + 1;
    int lastPage = std::min(pageCount, endRow * cols);
    StartSidebarThumbs(win, firstPage, lastPage);
    return pane;
}

// The thumbnail pane as orig's TestSidebarThumbnails line: frame hwnd, whether
// thumbnails show, the highlighted page, and each cell in frame-client pixels.
TempStr SidebarThumbnailsResultTemp(int* exitCodeOut) {
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    if (!win) {
        if (exitCodeOut) {
            *exitCodeOut = 2;
        }
        return StrL("NOTREADY no-window");
    }
    DisplayModel* dm = win->AsFixed();
    SidebarUI* ui = win->sidebar;
    int pageCount = dm ? dm->PageCount() : 0;
    int current = ui ? ui->thumbSelectedPage : 0;
    int rendered = 0;
    if (ui && ui->thumbCache) {
        for (SidebarThumb& thumb : ui->thumbCache->thumbs) {
            if (thumb.bitmap) {
                rendered++;
            }
        }
    }
    bool showing = SidebarContentVisible(win, SidebarContent::Thumbnails);
    float s = CanvasScale(win);
    if (s <= 0.f) {
        s = 1.f;
    }
    WindowTab* tab = win->CurrentTab();
    bool thumbsTop = tab && win->uiState.tocVisible && tab->sidebarContent == SidebarContent::Thumbnails;
    bool thumbsBottom = win->uiState.favVisible && win->sidebarBottomContent == SidebarContent::Thumbnails;
    bool ring = (thumbsTop && SidebarPanelHasFocus(win, true)) || (thumbsBottom && SidebarPanelHasFocus(win, false));
    int hwnd = (int)(intptr_t)AppShellNativeHwnd(win);
    str::Builder sb;
    sb.Append(fmt("hwnd=%d thumbnails=%d count=%d current=%d rendered=%d ring=%d", hwnd, showing ? 1 : 0, pageCount,
                  current, rendered, ring ? 1 : 0));
    SidebarContent icons[] = {SidebarContent::Bookmarks, SidebarContent::Thumbnails, SidebarContent::Favorites};
    bool panelVis[] = {win->uiState.tocVisible, win->uiState.favVisible};
    SidebarContent panelContent[] = {tab ? tab->sidebarContent : SidebarContent::Bookmarks, win->sidebarBottomContent};
    Str panelName[] = {StrL("top"), StrL("bottom")};
    for (int p = 0; p < 2; p++) {
        sb.Append(fmt(" %s=%d,%d,%s,", panelName[p], hwnd, panelVis[p] ? 1 : 0, SidebarContentToStr(panelContent[p])));
        for (int i = 0; i < 3; i++) {
            sb.Append(fmt("%d", ContentAvailable(win, icons[i]) ? 1 : 0));
        }
        sb.Append(StrL(","));
        for (int i = 0; i < 3; i++) {
            sb.Append(fmt("%d", panelContent[p] == icons[i] ? 1 : 0));
        }
        sb.Append(StrL(":"));
        for (int i = 0; i < 3; i++) {
            gp::Bounds b = panelVis[p] && ui ? ui->viewIconBounds[p][i] : gp::Bounds{};
            int x = (int)(b.x / s + 0.5f);
            int y = (int)(b.y / s + 0.5f);
            int dx = (int)((b.x + b.w) / s + 0.5f) - x;
            int dy = (int)((b.y + b.h) / s + 0.5f) - y;
            sb.Append(fmt(i == 0 ? "%d,%d,%d,%d" : ";%d,%d,%d,%d", x, y, dx, dy));
        }
    }
    sb.Append(StrL(" rects="));
    int n = (showing && ui) ? std::min(pageCount, len(ui->thumbBounds)) : 0;
    for (int pageNo = 1; pageNo <= n; pageNo++) {
        gp::Bounds b = ui->thumbBounds[pageNo - 1];
        int x = (int)(b.x / s + 0.5f);
        int y = (int)(b.y / s + 0.5f);
        int dx = (int)((b.x + b.w) / s + 0.5f) - x;
        int dy = (int)((b.y + b.h) / s + 0.5f) - y;
        sb.Append(fmt("%d:%d,%d,%d,%d;", pageNo, x, y, dx, dy));
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(sb);
}

static gp::El* BuildTocPane(MainWindow* win, gp::Ctx* cx, bool top) {
    SidebarUI* ui = Ui(win);
    const gp::Theme& th = gp::ThemeNow(cx->app);

    VecReset(ui->tocRows);
    TocTree* tree = CurrentTocTree(win);
    if (tree && tree->root) {
        FlattenToc(tree->root->child, 0, ui->tocRows);
    }
    int nRows = ui->tocRows.len;

    if (ui->tocReveal) {
        int idx = -1;
        for (int i = 0; i < nRows; i++) {
            if (ui->tocRows[i].item == ui->tocReveal) {
                idx = i;
                break;
            }
        }
        ui->tocScrollY = ScrollToRow(ui->tocScrollY, ui->tocView.h, idx, nRows);
        ui->tocReveal = nullptr;
    }

    // ng: the pane is inset on the right so nothing it draws sits under the
    // splitter's grab strip (orig's SplitterCtrl is a window of its own)
    gp::El* pane = gp::Div(cx->a)->FlexCol()->SizeFull()->Bg(th.tokens.sidebar);
    pane->Child(PaneHeader(win, cx, true, top, Tr("Bookmarks"), StrL("toc-close"),
                           gp::ListenTo(Ui(win)->view, &SidebarView::OnTocClose)));

    gp::InputState* filter = EnsureInput(win, &ui->tocFilter, Tr("Search Bookmarks"));
    filter->onChange = gp::ListenTo(Ui(win)->view, &SidebarView::OnTocFilter);
    pane->CaptureKeyDown(gp::ListenTo(Ui(win)->view, &SidebarView::OnFilterKey));
    pane->Child(gp::Div(cx->a)->W(gp::kFill)->Shrink0()->PadX(6)->PadB(4)->Child(
        gpc::Input::New(cx, GStrL("toc-filter"), filter)->WithSize(gp::UiSize::Small)->W(gp::kFill)->IntoEl()));

    // the rows that fit the pane, plus spacers for the ones above and below
    float viewH = ui->tocView.h > 0 ? ui->tocView.h : 400;
    int first = std::max(0, (int)(ui->tocScrollY / kRowDy) - 2);
    int end = std::min(nRows, (int)((ui->tocScrollY + viewH) / kRowDy) + 3);
    gp::El* list = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    if (first > 0) {
        list->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)first * kRowDy));
    }
    gp::Listener click = gp::ListenTo(Ui(win)->view, &SidebarView::OnTocRowClick, 0);
    gp::Listener down = gp::ListenTo(Ui(win)->view, &SidebarView::OnTocRowDown, 0);
    gp::Listener hover = gp::ListenTo(Ui(win)->view, &SidebarView::OnTocRowHover, 0);
    StrVec filterWords;
    SplitFilterToWords(SidebarTocFilterTextTemp(win), filterWords);
    gp::FocusHandle treeFocus = top ? ui->tocFocus : ui->favFocus;
    bool hasFocus = treeFocus.IsValid() && gp::FocusHandleIsFocused(cx->win, treeFocus);
    for (int i = first; i < end; i++) {
        TocItem* item = ui->tocRows[i].item;
        int depth = ui->tocRows[i].depth;
        bool isSel = item == ui->tocSel;
        bool isMatch = TocItemIsMultiHighlight(win, item);
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(kRowDy)
                          ->ItemsCenter()
                          ->PadR(6)
                          ->PadL(kRowPadL + (float)depth * kIndentDx)
                          ->PathClick(GpuiDup(cx->a, fmt("toc-row-%d", i)))
                          // orig's TVS_TRACKSELECT: a hand over the rows
                          ->Cursor(gp::CursorKind::Pointer)
                          ->OnClick(gp::ListenerArg(click, i))
                          ->OnMouseDown(gp::ListenerArg(down, i))
                          ->OnHover(gp::ListenerArg(hover, i));
        // orig's OnTocCustomDraw / DrawTocItemPostPaint
        Color bgCol, txtCol;
        TreeRowColors(isSel || isMatch, isSel && hasFocus, &bgCol, &txtCol);
        if (!(isSel && hasFocus) && item->color != kColorUnset) {
            txtCol = item->color;
        }
        row->Child(RowChevron(cx, item->child != nullptr, item->IsExpanded(), txtCol));
        gp::El* labelBox = TreeRowLabelBox(cx, row, bgCol);
        gp::Rgba fg = ToGpui(txtCol);
        gp::El* label;
        if (len(filterWords) > 0) {
            // orig's DrawTreeItemFilterHighlight: the matched letters get an
            // underlay while the bookmark filter is on
            label = FilterHighlightText(cx, item->title, filterWords, fg, 13)->Flex1()->MinW(0)->ClipX();
        } else {
            label = gp::TextEl(cx->a, GpuiDup(cx->a, item->title))
                        ->Font(kTreeFontSize)
                        ->Fg(fg)
                        ->Flex1()
                        ->MinW(0)
                        ->Truncate();
            if (bit::IsSet(item->fontFlags, kFontBitBold)) {
                label->Bold();
            }
            if (bit::IsSet(item->fontFlags, kFontBitItalic)) {
                label->Italic();
            }
        }
        labelBox->Child(label);
        if (gSettings->showTocPageNumbers && win->ctrl && item->pageNo > 0) {
            TempStr pageLabel = win->ctrl->GetPageLabeTemp(item->pageNo);
            if (len(pageLabel) > 0) {
                // Slightly muted vs title when not selected (keeps numbers secondary).
                Color pageCol = txtCol;
                if (!(isSel && hasFocus)) {
                    Color bg = bgCol != kColorUnset ? bgCol : ThemeControlBackgroundColor();
                    pageCol = MkRgb((GetRed(txtCol) * 2 + GetRed(bg)) / 3, (GetGreen(txtCol) * 2 + GetGreen(bg)) / 3,
                                    (GetBlue(txtCol) * 2 + GetBlue(bg)) / 3);
                }
                labelBox->Child(gp::TextEl(cx->a, GpuiDup(cx->a, pageLabel))
                                    ->Font(kTreeFontSize)
                                    ->Fg(ToGpui(pageCol))
                                    ->PadL(8)
                                    ->Shrink0());
            }
        }
        row->Child(labelBox);
        list->Child(row);
    }
    if (end < nRows) {
        list->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)(nRows - end) * kRowDy));
    }

    gp::El* scroll = gp::Div(cx->a)
                         ->Id(GStrL("toc-tree"))
                         ->TrackFocus(top ? ui->tocFocus : ui->favFocus)
                         ->FocusOnPress()
                         ->TabStop(false)
                         ->FocusRing()
                         ->FlexCol()
                         ->Flex1()
                         ->W(gp::kFill)
                         ->MinH(0)
                         ->ScrollY(ui->tocScrollY)
                         ->ScrollFromPath()
                         ->OnScroll(gp::ListenTo(Ui(win)->view, &SidebarView::OnTocScroll))
                         ->BoundsOut(&ui->tocView)
                         ->OnMouseDown(gp::ListenTo(Ui(win)->view, &SidebarView::OnTreeDown, 1))
                         ->OnMouseUp(gp::ListenTo(Ui(win)->view, &SidebarView::OnTreeUp, 1))
                         ->Child(list);

    if (!ui->tocMenuValid || ui->tocMenuFor != ui->tocRightClick) {
        DeleteMenuModel(ui->tocMenu);
        ui->tocMenu = BuildTocContextMenu(win, ui->tocRightClick);
        ui->tocMenuFor = ui->tocRightClick;
        ui->tocMenuValid = true;
    }
    // ng: the ContextMenu wraps a box around the tree, so the tree's own
    // right-press listener can keep the press from it
    gpc::PopupMenu* tocPopup = TrackPopup(cx, PopupFromModel(cx, ui->tocMenu, StrL("toc-ctx-menu"), ActTocMenu()));
    ui->tocPopup = tocPopup->state;
    gp::El* treeBox = gp::Div(cx->a)->FlexCol()->Flex1()->W(gp::kFill)->MinH(0)->Child(scroll);
    gp::El* body = gpc::ContextMenu::New(cx, GStrL("toc-ctx"))->Child(treeBox)->Menu(tocPopup)->IntoEl();
    body->OnAction(ActTocMenu(), gp::ListenTo(Ui(win)->view, &SidebarView::OnTocAction));
    pane->Child(body);
    return pane;
}

static gp::El* BuildFavPane(MainWindow* win, gp::Ctx* cx, bool top, bool selectors = true) {
    SidebarUI* ui = Ui(win);
    const gp::Theme& th = gp::ThemeNow(cx->app);

    VecReset(ui->favRows);
    if (ui->favModel && ui->favModel->root) {
        for (FavTreeItem* c : ui->favModel->root->children) {
            FlattenFav(c, 0, ui->favRows);
        }
    }
    int nRows = ui->favRows.len;

    // ng: the pane is inset on the right so nothing it draws sits under the
    // splitter's grab strip (orig's SplitterCtrl is a window of its own)
    gp::El* pane = gp::Div(cx->a)->FlexCol()->SizeFull()->Bg(th.tokens.sidebar);
    pane->Child(PaneHeader(win, cx, selectors, top, Tr("Favorites"), StrL("fav-close"),
                           gp::ListenTo(Ui(win)->view, &SidebarView::OnFavClose)));

    gp::InputState* filter = EnsureInput(win, &ui->favFilter, Tr("Search Favorites"));
    filter->onChange = gp::ListenTo(Ui(win)->view, &SidebarView::OnFavFilter);
    pane->CaptureKeyDown(gp::ListenTo(Ui(win)->view, &SidebarView::OnFilterKey));
    pane->Child(gp::Div(cx->a)->W(gp::kFill)->Shrink0()->PadX(6)->PadB(4)->Child(
        gpc::Input::New(cx, GStrL("fav-filter"), filter)->WithSize(gp::UiSize::Small)->W(gp::kFill)->IntoEl()));

    float viewH = ui->favView.h > 0 ? ui->favView.h : 200;
    int first = std::max(0, (int)(ui->favScrollY / kRowDy) - 2);
    int end = std::min(nRows, (int)((ui->favScrollY + viewH) / kRowDy) + 3);
    gp::El* list = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    if (first > 0) {
        list->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)first * kRowDy));
    }
    gp::Listener click = gp::ListenTo(Ui(win)->view, &SidebarView::OnFavRowClick, 0);
    gp::Listener down = gp::ListenTo(Ui(win)->view, &SidebarView::OnFavRowDown, 0);
    gp::Listener hover = gp::ListenTo(Ui(win)->view, &SidebarView::OnFavRowHover, 0);
    StrVec filterWords;
    SplitFilterToWords(SidebarFavFilterTextTemp(win), filterWords);
    gp::FocusHandle treeFocus = top ? ui->tocFocus : ui->favFocus;
    bool hasFocus = treeFocus.IsValid() && gp::FocusHandleIsFocused(cx->win, treeFocus);
    for (int i = first; i < end; i++) {
        FavTreeItem* item = ui->favRows[i].item;
        int depth = ui->favRows[i].depth;
        bool isSel = item == ui->favSel;
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(kRowDy)
                          ->ItemsCenter()
                          ->PadR(6)
                          ->PadL(kRowPadL + (float)depth * kIndentDx)
                          ->PathClick(GpuiDup(cx->a, fmt("fav-row-%d", i)))
                          ->Cursor(gp::CursorKind::Pointer)
                          ->OnClick(gp::ListenerArg(click, i))
                          ->OnMouseDown(gp::ListenerArg(down, i))
                          ->OnHover(gp::ListenerArg(hover, i));
        // orig's DrawFavItemText
        Color bgCol, txtCol;
        TreeRowColors(isSel, hasFocus, &bgCol, &txtCol);
        gp::Rgba fg = ToGpui(txtCol);
        bool hasChildren = len(item->children) > 0;
        row->Child(RowChevron(cx, hasChildren, item->isExpanded, txtCol));
        gp::El* labelBox = TreeRowLabelBox(cx, row, bgCol);
        // orig bolds the file-name span of a row that leads with one and
        // underlays the filter's matched letters (DrawTreeItemFilterHighlight)
        Str text = item->text;
        int nameLen = item->fileNameLen;
        bool hasName = item->fileNameOffset == 0 && nameLen > 0 && nameLen < len(text);
        if (len(filterWords) > 0) {
            int boldLen = hasName ? nameLen : 0;
            labelBox->Child(FilterHighlightText(cx, text, filterWords, fg, 13, 0, boldLen)->Flex1()->MinW(0)->ClipX());
        } else if (hasName) {
            Str name = Str(text.s, nameLen);
            Str rest = Str(text.s + nameLen, len(text) - nameLen);
            labelBox->Child(gp::TextEl(cx->a, GpuiDup(cx->a, name))->Font(kTreeFontSize)->Bold()->Fg(fg)->Shrink0());
            labelBox->Child(
                gp::TextEl(cx->a, GpuiDup(cx->a, rest))->Font(kTreeFontSize)->Fg(fg)->Flex1()->MinW(0)->Truncate());
        } else {
            labelBox->Child(
                gp::TextEl(cx->a, GpuiDup(cx->a, text))->Font(kTreeFontSize)->Fg(fg)->Flex1()->MinW(0)->Truncate());
        }
        row->Child(labelBox);
        list->Child(row);
    }
    if (end < nRows) {
        list->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)(nRows - end) * kRowDy));
    }

    gp::El* scroll = gp::Div(cx->a)
                         ->Id(GStrL("fav-tree"))
                         ->TrackFocus(top ? ui->tocFocus : ui->favFocus)
                         ->FocusOnPress()
                         ->TabStop(false)
                         ->FocusRing()
                         ->FlexCol()
                         ->Flex1()
                         ->W(gp::kFill)
                         ->MinH(0)
                         ->ScrollY(ui->favScrollY)
                         ->ScrollFromPath()
                         ->OnScroll(gp::ListenTo(Ui(win)->view, &SidebarView::OnFavScroll))
                         ->BoundsOut(&ui->favView)
                         ->OnMouseDown(gp::ListenTo(Ui(win)->view, &SidebarView::OnTreeDown, 0))
                         ->OnMouseUp(gp::ListenTo(Ui(win)->view, &SidebarView::OnTreeUp, 0))
                         ->Child(list);

    if (!ui->favMenuValid || ui->favMenuFor != ui->favRightClick) {
        DeleteMenuModel(ui->favMenu);
        ui->favMenu = BuildFavContextMenu(win, ui->favRightClick);
        ui->favMenuFor = ui->favRightClick;
        ui->favMenuValid = true;
    }
    gpc::PopupMenu* favPopup = TrackPopup(cx, PopupFromModel(cx, ui->favMenu, StrL("fav-ctx-menu"), ActFavMenu()));
    ui->favPopup = favPopup->state;
    gp::El* treeBox = gp::Div(cx->a)->FlexCol()->Flex1()->W(gp::kFill)->MinH(0)->Child(scroll);
    gp::El* body = gpc::ContextMenu::New(cx, GStrL("fav-ctx"))->Child(treeBox)->Menu(favPopup)->IntoEl();
    body->OnAction(ActFavMenu(), gp::ListenTo(Ui(win)->view, &SidebarView::OnFavAction));
    pane->Child(body);

    if (ui->favWantFocus) {
        ui->favWantFocus = false;
        gp::InputFocus(filter, cx->app, cx->win);
    }
    return pane;
}

gp::El* SidebarBuild(MainWindow* win, gp::Ctx* cx) {
    bool tocVisible = win->uiState.tocVisible;
    bool favVisible = win->uiState.favVisible;
    if (!tocVisible && !favVisible) {
        return nullptr;
    }
    EnsureView(win, cx);
    SidebarResolveContents(win);
    WindowTab* tab = win->CurrentTab();
    SidebarContent topContent = tab ? tab->sidebarContent : SidebarContent::Bookmarks;
    SidebarContent bottomContent = win->sidebarBottomContent;
    auto build = [&](SidebarContent content, bool top) -> gp::El* {
        if (content == SidebarContent::Thumbnails) {
            return BuildThumbPane(win, cx, top);
        }
        if (content == SidebarContent::Favorites) {
            return BuildFavPane(win, cx, top);
        }
        return BuildTocPane(win, cx, top);
    };

    if (tocVisible && !favVisible) {
        return build(topContent, true);
    }
    if (!tocVisible && favVisible) {
        return build(bottomContent, false);
    }
    // both panes: orig's favSplitter between them. TocDy is the height of the
    // bookmarks pane.
    const gp::Theme& th = gp::ThemeNow(cx->app);
    int contentDy = win->canvasRc.dy;
    int tocDy = gSettings->tocDy > 0 ? gSettings->tocDy : contentDy / 2;
    tocDy = limitValue(tocDy, kTocMinDy, std::max(kTocMinDy, contentDy - kTocMinDy));
    gp::El* col = gp::Div(cx->a)->FlexCol()->SizeFull();
    col->Child(gp::Div(cx->a)->FlexCol()->W(gp::kFill)->H((float)tocDy)->Shrink0()->Child(build(topContent, true)));
    col->Child(gp::Div(cx->a)->H((float)kSplitterDy)->W(gp::kFill)->Shrink0()->Bg(th.border));
    col->Child(gp::Div(cx->a)->FlexCol()->Flex1()->MinH(0)->W(gp::kFill)->Child(build(bottomContent, false)));
    // ng: the grab strip goes last and absolute, so it is the topmost hit rect
    // over the splitter's row (a later sibling's rect would win otherwise)
    col->Child(gp::Div(cx->a)
                   ->Absolute()
                   ->Left(0)
                   ->Top((float)tocDy)
                   ->H((float)kSplitterDy)
                   ->W(gp::kFill)
                   ->Cursor(gp::CursorKind::RowResize)
                   ->PathClick(GStrL("fav-splitter"))
                   ->OnDrag(GStrL("sumatra-fav-splitter"))
                   ->OnDragMove(gp::ListenTo(Ui(win)->view, &SidebarView::OnFavSplitter))
                   ->OnMouseUp(gp::ListenTo(Ui(win)->view, &SidebarView::OnFavSplitterDone))
                   ->OnMouseUpOut(gp::ListenTo(Ui(win)->view, &SidebarView::OnFavSplitterDone)));
    return col;
}

// ng: the Favorites tab (CmdFavoriteShowInTab) is the favorites pane filling
// the canvas instead of the sidebar. orig reparents the same child window.
gp::El* SidebarBuildFavTab(MainWindow* win, gp::Ctx* cx) {
    EnsureView(win, cx);
    return BuildFavPane(win, cx, false, false);
}
