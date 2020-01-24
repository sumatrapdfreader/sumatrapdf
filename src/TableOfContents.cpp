/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Dpi.h"
#include "utils/Log.h"
#include "utils/BitManip.h"
#include "utils/FileUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/LabelWithCloseWnd.h"
#include "wingui/SplitterWnd.h"
#include "wingui/TreeModel.h"
#include "wingui/TreeCtrl.h"
#include "wingui/DropDownCtrl.h"

#include "utils/GdiPlusUtil.h"

#include "EngineBase.h"
#include "EngineManager.h"
#include "ParseBKM.h"

#include "SumatraConfig.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "GlobalPrefs.h"
#include "AppColors.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "DisplayModel.h"
#include "Favorites.h"
#include "TabInfo.h"
#include "resource.h"
#include "AppTools.h"
#include "TableOfContents.h"
#include "Translations.h"
#include "Tabs.h"
#include "Menu.h"
#include "TocEditor.h"

/* Define if you want page numbers to be displayed in the ToC sidebar */
// #define DISPLAY_TOC_PAGE_NUMBERS

#ifdef DISPLAY_TOC_PAGE_NUMBERS
#define WM_APP_REPAINT_TOC (WM_APP + 1)
#endif

// set tooltip for this item but only if the text isn't fully shown
// TODO: I might have lost something in translation
static void TocCustomizeTooltip(TreeItmGetTooltipArgs* args) {
    auto* w = args->treeCtrl;
    auto* ti = args->treeItem;
    auto* nm = args->info;
    TocItem* tocItem = (TocItem*)ti;
    PageDestination* link = tocItem->GetPageDestination();
    if (!link) {
        return;
    }
    WCHAR* path = link->GetValue();
    if (!path) {
        path = tocItem->title;
    }
    if (!path) {
        return;
    }
    CrashIf(!link); // /analyze claims that this could happen - it really can't
    auto k = link->Kind();
    // TODO: TocItem from Chm contain other types
    // we probably shouldn't set TocItem::dest there
    if (k == kindDestinationScrollTo) {
        return;
    }
    if (k == kindDestinationNone) {
        return;
    }

    CrashIf(k != kindDestinationLaunchURL && k != kindDestinationLaunchFile && k != kindDestinationLaunchEmbedded);

    str::WStr infotip;

    // Display the item's full label, if it's overlong
    RECT rcLine, rcLabel;
    w->GetItemRect(args->treeItem, false, rcLine);
    w->GetItemRect(args->treeItem, true, rcLabel);

    if (rcLine.right + 2 < rcLabel.right) {
        str::WStr currInfoTip = ti->Text();
        infotip.Append(currInfoTip.data());
        infotip.Append(L"\r\n");
    }

    if (kindDestinationLaunchEmbedded == k) {
        AutoFreeWstr tmp = str::Format(_TR("Attachment: %s"), path);
        infotip.Append(tmp.get());
    } else {
        infotip.Append(path);
    }

    str::BufSet(nm->pszText, nm->cchTextMax, infotip.Get());
    args->didHandle = true;
}

#ifdef DISPLAY_TOC_PAGE_NUMBERS
static void RelayoutTocItem(LPNMTVCUSTOMDRAW ntvcd) {
    // code inspired by http://www.codeguru.com/cpp/controls/treeview/multiview/article.php/c3985/
    LPNMCUSTOMDRAW ncd = &ntvcd->nmcd;
    HWND hTV = ncd->hdr.hwndFrom;
    HTREEITEM hItem = (HTREEITEM)ncd->dwItemSpec;
    RECT rcItem;
    if (0 == ncd->rc.right - ncd->rc.left || 0 == ncd->rc.bottom - ncd->rc.top)
        return;
    if (!TreeView_GetItemRect(hTV, hItem, &rcItem, TRUE))
        return;
    if (rcItem.right > ncd->rc.right)
        rcItem.right = ncd->rc.right;

    // Clear the label
    RECT rcFullWidth = rcItem;
    rcFullWidth.right = ncd->rc.right;
    FillRect(ncd->hdc, &rcFullWidth, GetSysColorBrush(COLOR_WINDOW));

    // Get the label's text
    WCHAR szText[MAX_PATH];
    TVITEM item;
    item.hItem = hItem;
    item.mask = TVIF_TEXT | TVIF_PARAM;
    item.pszText = szText;
    item.cchTextMax = MAX_PATH;
    TreeView_GetItem(hTV, &item);

    // Draw the page number right-aligned (if there is one)
    WindowInfo* win = FindWindowInfoByHwnd(hTV);
    TocItem* tocItem = (TocItem*)item.lParam;
    AutoFreeWstr label;
    if (tocItem->pageNo && win && win->IsDocLoaded()) {
        label.Set(win->ctrl->GetPageLabel(tocItem->pageNo));
        label.Set(str::Join(L"  ", label));
    }
    if (label && str::EndsWith(item.pszText, label)) {
        RECT rcPageNo = rcFullWidth;
        InflateRect(&rcPageNo, -2, -1);

        SIZE txtSize;
        GetTextExtentPoint32(ncd->hdc, label, str::Len(label), &txtSize);
        rcPageNo.left = rcPageNo.right - txtSize.cx;

        SetTextColor(ncd->hdc, GetSysColor(COLOR_WINDOWTEXT));
        SetBkColor(ncd->hdc, GetSysColor(COLOR_WINDOW));
        DrawText(ncd->hdc, label, -1, &rcPageNo, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

        // Reduce the size of the label and cut off the page number
        rcItem.right = std::max(rcItem.right - txtSize.cx, 0);
        szText[str::Len(szText) - str::Len(label)] = '\0';
    }

    SetTextColor(ncd->hdc, ntvcd->clrText);
    SetBkColor(ncd->hdc, ntvcd->clrTextBk);

    // Draw the focus rectangle (including proper background color)
    HBRUSH brushBg = CreateSolidBrush(ntvcd->clrTextBk);
    FillRect(ncd->hdc, &rcItem, brushBg);
    DeleteObject(brushBg);
    if ((ncd->uItemState & CDIS_FOCUS))
        DrawFocusRect(ncd->hdc, &rcItem);

    InflateRect(&rcItem, -2, -1);
    DrawText(ncd->hdc, szText, -1, &rcItem, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_WORD_ELLIPSIS);
}
#endif

static void GoToTocLinkTask(WindowInfo* win, TocItem* tocItem, TabInfo* tab, Controller* ctrl) {
    // tocItem is invalid if the Controller has been replaced
    if (!WindowInfoStillValid(win) || win->currentTab != tab || tab->ctrl != ctrl)
        return;

    // make sure that the tree item that the user selected
    // isn't unselected in UpdateTocSelection right again
    win->tocKeepSelection = true;
    int pageNo = tocItem->pageNo;
    PageDestination* dest = tocItem->GetPageDestination();
    if (dest) {
        win->linkHandler->GotoLink(dest);
    } else if (pageNo) {
        ctrl->GoToPage(pageNo, true);
    }
    win->tocKeepSelection = false;
}

static bool IsScrollToLink(PageDestination* link) {
    if (!link) {
        return false;
    }
    auto kind = link->Kind();
    return kind == kindDestinationScrollTo;
}

static void GoToTocTreeItem(WindowInfo* win, TreeItem* ti, bool allowExternal) {
    if (!ti) {
        return;
    }
    TocItem* tocItem = (TocItem*)ti;
    bool validPage = (tocItem->pageNo > 0);
    bool isScroll = IsScrollToLink(tocItem->GetPageDestination());
    if (validPage || (allowExternal || isScroll)) {
        // delay changing the page until the tree messages have been handled
        TabInfo* tab = win->currentTab;
        Controller* ctrl = win->ctrl;
        uitask::Post([=] { GoToTocLinkTask(win, tocItem, tab, ctrl); });
    }
}

void ClearTocBox(WindowInfo* win) {
    if (!win->tocLoaded) {
        return;
    }

    win->tocTreeCtrl->Clear();

    win->currPageNo = 0;
    win->tocLoaded = false;
}

void ToggleTocBox(WindowInfo* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    if (win->tocVisible) {
        SetSidebarVisibility(win, false, gGlobalPrefs->showFavorites);
        return;
    }
    SetSidebarVisibility(win, true, gGlobalPrefs->showFavorites);
    if (win->tocVisible) {
        SetFocus(win->tocTreeCtrl->hwnd);
    }
}

// find the closest item in tree view to a given page number
static TreeItem* TreeItemForPageNo(TreeCtrl* treeCtrl, int pageNo) {
    TocItem* bestMatch = nullptr;
    int bestMatchPageNo = 0;

    TreeModel* tm = treeCtrl->treeModel;
    if (!tm) {
        return nullptr;
    }
    VisitTreeModelItems(tm, [&](TreeItem* ti) {
        auto* docItem = (TocItem*)ti;
        if (!docItem) {
            return true;
        }
        if (!bestMatch) {
            // if nothing else matches, match the root node
            bestMatch = docItem;
        }

        int page = docItem->pageNo;
        if ((page <= pageNo) && (page >= bestMatchPageNo) && (page >= 1)) {
            bestMatch = docItem;
            bestMatchPageNo = page;
            if (pageNo == bestMatchPageNo) {
                // we can stop earlier if we found the exact match
                return false;
            }
        }
        return true;
    });
    return bestMatch;
}

void UpdateTocSelection(WindowInfo* win, int currPageNo) {
    if (!win->tocLoaded || !win->tocVisible || win->tocKeepSelection) {
        return;
    }

    TreeItem* item = TreeItemForPageNo(win->tocTreeCtrl, currPageNo);
    if (item) {
        win->tocTreeCtrl->SelectItem(item);
    }
}

static void UpdateDocTocExpansionStateRecur(TreeCtrl* treeCtrl, Vec<int>& tocState, TocItem* tocItem) {
    while (tocItem) {
        // items without children cannot be toggled
        if (tocItem->child) {
            // we have to query the state of the tree view item because
            // isOpenToggled is not kept in sync
            // TODO: keep toggle state on TocItem in sync
            // by subscribing to the right notifications
            bool isExpanded = treeCtrl->IsExpanded(tocItem);
            bool wasToggled = isExpanded != tocItem->isOpenDefault;
            if (wasToggled) {
                tocState.Append(tocItem->id);
            }
            UpdateDocTocExpansionStateRecur(treeCtrl, tocState, tocItem->child);
        }
        tocItem = tocItem->next;
    }
}

void UpdateTocExpansionState(Vec<int>& tocState, TreeCtrl* treeCtrl, TocTree* docTree) {
    if (treeCtrl->treeModel != docTree) {
        // CrashMe();
        return;
    }
    tocState.Reset();
    TocItem* tocItem = docTree->root;
    UpdateDocTocExpansionStateRecur(treeCtrl, tocState, tocItem);
}

// copied from mupdf/fitz/dev_text.c
static bool isLeftToRightChar(WCHAR c) {
    return ((0x0041 <= (c) && (c) <= 0x005A) || (0x0061 <= (c) && (c) <= 0x007A) || (0xFB00 <= (c) && (c) <= 0xFB06));
}

static bool isRightToLeftChar(WCHAR c) {
    return ((0x0590 <= (c) && (c) <= 0x05FF) || (0x0600 <= (c) && (c) <= 0x06FF) || (0x0750 <= (c) && (c) <= 0x077F) ||
            (0xFB50 <= (c) && (c) <= 0xFDFF) || (0xFE70 <= (c) && (c) <= 0xFEFE));
}

static void GetLeftRightCounts(TocItem* node, int& l2r, int& r2l) {
    if (!node)
        return;
    if (node->title) {
        for (const WCHAR* c = node->title; *c; c++) {
            if (isLeftToRightChar(*c)) {
                l2r++;
            } else if (isRightToLeftChar(*c)) {
                r2l++;
            }
        }
    }
    GetLeftRightCounts(node->child, l2r, r2l);
    GetLeftRightCounts(node->next, l2r, r2l);
}

static void SetInitialExpandState(TocItem* item, Vec<int>& tocState) {
    while (item) {
        if (tocState.Contains(item->id)) {
            item->isOpenToggled = true;
        }
        SetInitialExpandState(item->child, tocState);
        item = item->next;
    }
}

void ShowExportedBookmarksMsg(const char* path) {
    str::Str msg;
    msg.AppendFmt("Exported bookmarks to file %s", path);
    str::Str caption;
    caption.Append("Exported bookmarks");
    UINT type = MB_OK | MB_ICONINFORMATION | MbRtlReadingMaybe();
    MessageBoxA(nullptr, msg.Get(), caption.Get(), type);
}

static void ExportBookmarksFromTab(TabInfo* tab) {
    auto* tocTree = tab->ctrl->GetToc();
    str::Str path = strconv::WstrToUtf8(tab->filePath);
    path.Append(".bkm");
    Vec<VbkmForFile*> bookmarks;

    VbkmForFile* bkms = new VbkmForFile();
    bkms->filePath = strconv::WstrToUtf8(tab->filePath);
    bkms->toc = CloneTocTree(tocTree, false);
    bkms->nPages = tab->ctrl->PageCount();
    bookmarks.push_back(bkms);
    bool ok = ExportBookmarksToFile(bookmarks, "", path.c_str());
    delete bkms;

    ShowExportedBookmarksMsg(path.c_str());
}

// in Favorites.cpp
extern TreeItem* GetOrSelectTreeItemAtPos(ContextMenuArgs* args, POINT& pt);

// clang-format off
static MenuDef contextMenuDef[] = {
    {_TRN("Expand All"),    IDM_EXPAND_ALL,         0 },
    {_TRN("Collapse All"),  IDM_COLLAPSE_ALL,       0 },
    // note: strings cannot be "" or else items are not there
    {"add",                 IDM_FAV_ADD,            MF_NO_TRANSLATE},
    {"del",                 IDM_FAV_DEL,            MF_NO_TRANSLATE},
    {SEP_ITEM,              IDM_SEPARATOR,          MF_NO_TRANSLATE},
    {"Export Bookmarks",    IDM_EXPORT_BOOKMARKS,   MF_NO_TRANSLATE},
    {"New Bookmarks",       IDM_NEW_BOOKMARKS,      MF_NO_TRANSLATE},
};
// clang-format on      

static void AddFavoriteFromToc(WindowInfo* win, TocItem* dti) {
    int pageNo = 0;
    if (dti->dest) {
        pageNo = dti->dest->GetPageNo();
    }
    AutoFreeWstr name = str::Dup(dti->title);
    AutoFreeWstr pageLabel = win->ctrl->GetPageLabel(pageNo);
    AddFavoriteWithLabelAndName(win, pageNo, pageLabel.Get(), name);
}

static bool IsForVbkm(WindowInfo* win) {
    auto path = win->currentTab->filePath.get();
    bool isVbkm = str::EndsWithI(path, L".vbkm");
    return isVbkm;
}

static void StartTocEditorForWindowInfo(WindowInfo* win) {
    auto* tab = win->currentTab;
    TocEditorArgs* args = new TocEditorArgs();
    // args->filePath = str::Dup(tab->filePath);

    VbkmFile vbkm;
    AutoFreeStr filePath = strconv::WstrToUtf8(tab->filePath);
    if (str::EndsWithI(tab->filePath, L".vbkm")) {
        LoadVbkmFile(filePath, vbkm);
        int n = vbkm.vbkms.isize();
        for (int i = 0; i < n; i++) {
            auto b = vbkm.vbkms[i];
            args->bookmarks.push_back(b);
        }
        vbkm.vbkms.clear();
    } else {
        VbkmForFile* bkms = new VbkmForFile();
        bkms->filePath = filePath.release();
        bkms->nPages = tab->ctrl->PageCount();

        TocTree* tree = (TocTree*)win->tocTreeCtrl->treeModel;
        bkms->toc = CloneTocTree(tree, false);
        args->bookmarks.push_back(bkms);
    }

    args->hwndRelatedTo = win->hwndFrame;
    StartTocEditor(args);
}

static void TocContextMenu(ContextMenuArgs* args) {
    WindowInfo* win = FindWindowInfoByHwnd(args->w->hwnd);
    CrashIf(!win);
    const WCHAR* filePath = win->ctrl->FilePath();

    POINT pt{};
    TreeItem* ti = GetOrSelectTreeItemAtPos(args, pt);
    if (!ti) {
        pt = {args->mouseGlobal.x, args->mouseGlobal.y};
    }
    int pageNo = 0;
    TocItem* dti = (TocItem*)ti;
    if (dti && dti->dest) {
        pageNo = dti->dest->GetPageNo();
    }

    HMENU popup = BuildMenuFromMenuDef(contextMenuDef, dimof(contextMenuDef), CreatePopupMenu());
    if (!gWithTocEditor) {
        win::menu::Remove(popup, IDM_SEPARATOR);
        win::menu::Remove(popup, IDM_EXPORT_BOOKMARKS);
        win::menu::Remove(popup, IDM_NEW_BOOKMARKS);
    }

    if (pageNo > 0) {
        AutoFreeWstr pageLabel = win->ctrl->GetPageLabel(pageNo);
        bool isBookmarked = gFavorites.IsPageInFavorites(filePath, pageNo);
        if (isBookmarked) {
            win::menu::Remove(popup, IDM_FAV_ADD);

            // %s and not %d because re-using translation from RebuildFavMenu()
            auto tr = _TR("Remove page %s from favorites");
            AutoFreeWstr s = str::Format(tr, pageLabel.Get());
            win::menu::SetText(popup, IDM_FAV_DEL, s);
        } else {
            win::menu::Remove(popup, IDM_FAV_DEL);

            // %s and not %d because re-using translation from RebuildFavMenu()
            auto tr = _TR("Add page %s to favorites");
            AutoFreeWstr s = str::Format(tr, pageLabel.Get());
            win::menu::SetText(popup, IDM_FAV_ADD, s);
        }
    } else {
        win::menu::Remove(popup, IDM_FAV_ADD);
        win::menu::Remove(popup, IDM_FAV_DEL);
    }

    if (IsForVbkm(win)) {
        win::menu::SetText(popup, IDM_NEW_BOOKMARKS, L"Edit Bookmarks");
    }

    // MarkMenuOwnerDraw(popup);
    UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    INT cmd = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    // FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);
    switch (cmd) {
        case IDM_EXPORT_BOOKMARKS: {
            auto* tab = win->currentTab;
            ExportBookmarksFromTab(tab);
        } break;
        case IDM_NEW_BOOKMARKS:
            StartTocEditorForWindowInfo(win);
            break;
        case IDM_EXPAND_ALL:
            win->tocTreeCtrl->ExpandAll();
            break;
        case IDM_COLLAPSE_ALL:
            win->tocTreeCtrl->CollapseAll();
            break;
        case IDM_FAV_ADD:
            AddFavoriteFromToc(win, dti);
            break;
        case IDM_FAV_DEL:
            DelFavorite(filePath, pageNo);
            break;
    }
}

static void AltBookmarksChanged(WindowInfo* win, TabInfo* tab, int n, std::string_view s) {
    TocTree* tocTree = nullptr;
    if (n == 0) {
        tocTree = tab->ctrl->GetToc();
    } else {
        auto vbkms = tab->altBookmarks[0]->vbkms;
        tocTree = vbkms.at(n - 1)->toc;
    }
    win->tocTreeCtrl->SetTreeModel(tocTree);
}

// TODO: temporary
static bool LoadAlterenativeBookmarks(const WCHAR* baseFileName, VbkmFile& vbkm) {
    AutoFreeStr tmp = strconv::WstrToUtf8(baseFileName);
    return LoadAlterenativeBookmarks(tmp.as_view(), vbkm);
}

static bool ShouldCustomDraw(WindowInfo* win) {
    // we only want custom drawing for pdf and pdf multi engines
    // as they are the only ones supporting custom colors and fonts
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        return false;
    }
    Kind kind = dm->GetEngineType();
    if (kind == kindEnginePdf || kind == kindEngineMulti) {
        return true;
    }
    return false;
}

void OnTocCustomDraw(TreeItemCustomDrawArgs*);

static void dropDownSelectionChanged(DropDownSelectionChangedArgs* args) {
    WindowInfo* win = FindWindowInfoByHwnd(args->hwnd);
    TabInfo* tab = win->currentTab;
    DebugCrashIf(!tab);
    if (!tab) {
        return;
    }
    AltBookmarksChanged(win, tab, args->idx, args->item);
    args->didHandle = true;
}

void LoadTocTree(WindowInfo* win) {
    TabInfo* tab = win->currentTab;
    CrashIf(!tab);

    if (win->tocLoaded) {
        return;
    }

    win->tocLoaded = true;

    auto* tocTree = tab->ctrl->GetToc();
    if (!tocTree || !tocTree->root) {
        return;
    }

    // TODO: for now just for testing
    // TODO: restore showing alternative bookmarks
    VbkmFile* vbkm = new VbkmFile();
    bool ok = LoadAlterenativeBookmarks(tab->filePath, *vbkm);
    if (ok && vbkm->vbkms.size() > 0) {
        tab->altBookmarks.push_back(vbkm);
        Vec<std::string_view> items;
        size_t n = vbkm->vbkms.size();
        items.Append("Default");
        char* name = vbkm->name.get();
        if (name) {
            items.Append(name);
        }
        win->altBookmarks->SetItems(items);
    } else {
        delete vbkm;
    }

    win->altBookmarks->onDropDownSelectionChanged = dropDownSelectionChanged;

    // consider a ToC tree right-to-left if a more than half of the
    // alphabetic characters are in a right-to-left script
    int l2r = 0, r2l = 0;
    GetLeftRightCounts(tocTree->root, l2r, r2l);
    bool isRTL = r2l > l2r;

    TreeCtrl* treeCtrl = win->tocTreeCtrl;
    HWND hwnd = treeCtrl->hwnd;
    SetRtl(hwnd, isRTL);

    UpdateTreeCtrlColors(win);
    SetInitialExpandState(tocTree->root, tab->tocState);
    tocTree->root->OpenSingleNode();

    treeCtrl->SetTreeModel(tocTree);

    treeCtrl->onTreeItemCustomDraw = nullptr;
    if (ShouldCustomDraw(win)) {
        treeCtrl->onTreeItemCustomDraw = OnTocCustomDraw;
    }
    // UINT fl = RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN;
    // RedrawWindow(hwnd, nullptr, nullptr, fl);
}

// TODO: use https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-getobject?redirectedfrom=MSDN
// to get LOGFONT from existing font and then create a derived font
static void UpdateFont(HDC hdc, int fontFlags) {
    // TODO: this is a bit hacky, in that we use default font
    // and not the font from TreeCtrl. But in this case they are the same
    bool italic = bit::IsSet(fontFlags, fontBitItalic);
    bool bold = bit::IsSet(fontFlags, fontBitBold);
    HFONT hfont = GetDefaultGuiFont(bold, italic);
    SelectObject(hdc, hfont);
}

// https://docs.microsoft.com/en-us/windows/win32/controls/about-custom-draw
// https://docs.microsoft.com/en-us/windows/win32/api/commctrl/ns-commctrl-nmtvcustomdraw
void OnTocCustomDraw(TreeItemCustomDrawArgs* args) {
#if defined(DISPLAY_TOC_PAGE_NUMBERS)
    if (win->AsEbook())
        return CDRF_DODEFAULT;
    switch (((LPNMCUSTOMDRAW)pnmtv)->dwDrawStage) {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT:
            return CDRF_DODEFAULT | CDRF_NOTIFYPOSTPAINT;
        case CDDS_ITEMPOSTPAINT:
            RelayoutTocItem((LPNMTVCUSTOMDRAW)pnmtv);
            // fall through
        default:
            return CDRF_DODEFAULT;
    }
    break;
#endif

    args->result = CDRF_DODEFAULT;
    args->didHandle = true;

    TreeCtrl* w = args->treeCtrl;
    NMTVCUSTOMDRAW* tvcd = args->nm;
    NMCUSTOMDRAW* cd = &(tvcd->nmcd);
    if (cd->dwDrawStage == CDDS_PREPAINT) {
        // ask to be notified about each item
        args->result = CDRF_NOTIFYITEMDRAW;
        return;
    }

    if (cd->dwDrawStage == CDDS_ITEMPREPAINT) {
        // called before drawing each item
        TocItem* tocItem = (TocItem*)args->treeItem;
        ;
        if (!tocItem) {
            return;
        }
        if (tocItem->color != ColorUnset) {
            tvcd->clrText = tocItem->color;
        }
        if (tocItem->fontFlags != 0) {
            UpdateFont(cd->hdc, tocItem->fontFlags);
            args->result = CDRF_NEWFONT;
            return;
        }
        return;
    }
    return;
}

static void TocTreeSelectionChanged(TreeSelectionChangedArgs* args) {
    WindowInfo* win = FindWindowInfoByHwnd(args->w->hwnd);
    CrashIf(!win);

    // When the focus is set to the toc window the first item in the treeview is automatically
    // selected and a TVN_SELCHANGEDW notification message is sent with the special code pnmtv->action ==
    // 0x00001000. We have to ignore this message to prevent the current page to be changed.
    // The case pnmtv->action==TVC_UNKNOWN is ignored because
    // it corresponds to a notification sent by
    // the function TreeView_DeleteAllItems after deletion of the item.
    bool shouldHandle = args->byKeyboard || args->byMouse;
    if (!shouldHandle) {
        return;
    }
    bool allowExternal = args->byMouse;
    GoToTocTreeItem(win, args->selectedItem, allowExternal);
    args->didHandle = true;
}

// also used in Favorites.cpp
void TocTreeKeyDown(TreeKeyDownArgs* args) {
    if (args->keyCode != VK_TAB) {
        return;
    }
    args->didHandle = true;
    args->result = 1;

    WindowInfo* win = FindWindowInfoByHwnd(args->hwnd);
    CrashIf(!win);
    if (win->tabsVisible && IsCtrlPressed()) {
        TabsOnCtrlTab(win, IsShiftPressed());
        return;
    }
    AdvanceFocus(win);
}

#ifdef DISPLAY_TOC_PAGE_NUMBERS
static void TocTreeMsgFilter(WndProcArgs* args) {
    UNUSED(args);
    switch (msg) {
        case WM_SIZE:
        case WM_HSCROLL:
            // Repaint the ToC so that RelayoutTocItem is called for all items
            PostMessage(hwnd, WM_APP_REPAINT_TOC, 0, 0);
            break;
        case WM_APP_REPAINT_TOC:
            InvalidateRect(hwnd, nullptr, TRUE);
            UpdateWindow(hwnd);
            break;
    }
}
#endif

// Position label with close button and tree window within their parent.
// Used for toc and favorites.
void LayoutTreeContainer(LabelWithCloseWnd* l, DropDownCtrl* altBookmarks, HWND hwndTree) {
    HWND hwndContainer = GetParent(hwndTree);
    SizeI labelSize = l->GetIdealSize();
    WindowRect rc(hwndContainer);
    bool altBookmarksVisible = altBookmarks && altBookmarks->items.size() > 0;
    int dy = rc.dy;
    int y = 0;
    MoveWindow(l->hwnd, y, 0, rc.dx, labelSize.dy, TRUE);
    dy -= labelSize.dy;
    y += labelSize.dy;
    if (altBookmarks) {
        altBookmarks->SetIsVisible(altBookmarksVisible);
        if (altBookmarksVisible) {
            SIZE bs = altBookmarks->GetIdealSize();
            int elDy = bs.cy;
            RECT r{0, y, rc.dx, y + elDy};
            altBookmarks->SetBounds(r);
            elDy += 4;
            dy -= elDy;
            y += elDy;
        }
    }
    MoveWindow(hwndTree, 0, y, rc.dx, dy, TRUE);
}

static LRESULT CALLBACK WndProcTocBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId, DWORD_PTR data) {
    WindowInfo* winFromData = reinterpret_cast<WindowInfo*>(data);
    WindowInfo* win = FindWindowInfoByHwnd(hwnd);
    if (!win) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }
    CrashIf(subclassId != win->tocBoxSubclassId);
    CrashIf(win != winFromData);

    switch (msg) {
        case WM_SIZE:
            LayoutTreeContainer(win->tocLabelWithClose, win->altBookmarks, win->tocTreeCtrl->hwnd);
            break;

        case WM_COMMAND:
            if (LOWORD(wp) == IDC_TOC_LABEL_WITH_CLOSE) {
                ToggleTocBox(win);
            }
            break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void SubclassToc(WindowInfo* win) {
    TreeCtrl* tree = win->tocTreeCtrl;
    HWND hwndTocBox = win->hwndTocBox;

    if (win->tocBoxSubclassId == 0) {
        win->tocBoxSubclassId = NextSubclassId();
        BOOL ok = SetWindowSubclass(hwndTocBox, WndProcTocBox, win->tocBoxSubclassId, (DWORD_PTR)win);
        CrashIf(!ok);
    }
}

void UnsubclassToc(WindowInfo* win) {
    if (win->tocBoxSubclassId != 0) {
        RemoveWindowSubclass(win->hwndTocBox, WndProcTocBox, win->tocBoxSubclassId);
    }
}

void TocTreeMouseWheelHandler(MouseWheelArgs* args) {
    WindowInfo* win = FindWindowInfoByHwnd(args->hwnd);
    CrashIf(!win);
    if (!win) {
        return;
    }
    // scroll the canvas if the cursor isn't over the ToC tree
    if (!IsCursorOverWindow(args->hwnd)) {
        args->didHandle = true;
        args->result = SendMessage(win->hwndCanvas, args->msg, args->wparam, args->lparam);
    }
}

void TocTreeCharHandler(CharArgs* args) {
    WindowInfo* win = FindWindowInfoByHwnd(args->hwnd);
    CrashIf(!win);
    if (!win) {
        return;
    }
    if (VK_ESCAPE != args->keyCode) {
        return;
    }
    if (!gGlobalPrefs->escToExit) {
        return;
    }
    if (!MayCloseWindow(win)) {
        return;
    }

    CloseWindow(win, true);
    args->didHandle = true;
}

void CreateToc(WindowInfo* win) {
    HMODULE hmod = GetModuleHandle(nullptr);
    int dx = gGlobalPrefs->sidebarDx;
    DWORD style = WS_CHILD | WS_CLIPCHILDREN;
    HWND parent = win->hwndFrame;
    win->hwndTocBox = CreateWindowExW(0, WC_STATIC, L"", style, 0, 0, dx, 0, parent, 0, hmod, nullptr);

    auto* l = new LabelWithCloseWnd();
    l->Create(win->hwndTocBox, IDC_TOC_LABEL_WITH_CLOSE);
    win->tocLabelWithClose = l;
    l->SetPaddingXY(2, 2);
    l->SetFont(GetDefaultGuiFont());
    // label is set in UpdateToolbarSidebarText()

    win->altBookmarks = new DropDownCtrl(win->hwndTocBox);
    win->altBookmarks->Create();

    auto* treeCtrl = new TreeCtrl(win->hwndTocBox);
    treeCtrl->dwExStyle = WS_EX_STATICEDGE;
    treeCtrl->onGetTooltip = TocCustomizeTooltip;
    treeCtrl->onContextMenu = TocContextMenu;
    treeCtrl->onChar = TocTreeCharHandler;
    treeCtrl->onMouseWheel = TocTreeMouseWheelHandler;
    treeCtrl->onTreeSelectionChanged = TocTreeSelectionChanged;
    treeCtrl->onTreeKeyDown = TocTreeKeyDown;

    bool ok = treeCtrl->Create(L"TOC");
    CrashIf(!ok);
    win->tocTreeCtrl = treeCtrl;
    SubclassToc(win);
}
