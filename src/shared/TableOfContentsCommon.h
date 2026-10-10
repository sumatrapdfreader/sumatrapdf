/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by TableOfContentsCommon.cpp and each app's TableOfContents.cpp ---

IPageDestination* SnapshotDestForDeferredNav(IPageDestination* dest, int tocPageNo);
TocItem* FindTocItemByTitlePage(TocItem* item, Str title, int pageNo);
struct GoToTocLinkData {
    WindowTab* tab = nullptr;
    DocController* ctrl = nullptr;
    // owned snapshot; may be null (then pageNo alone is used)
    IPageDestination* dest = nullptr;
    int pageNo = 0;
    // owned; used to re-select in tree after palette-driven nav
    Str title;
    // true when the navigation was driven from outside the tree (e.g. the
    // command palette), so afterwards we must move the tree's selection to the
    // item ourselves. For tree-driven navigation the tree is already selected.
    bool selectInTree = false;

    ~GoToTocLinkData() {
        delete dest;
        str::Free(title);
    }
};
GoToTocLinkData* NewGoToTocLinkData(MainWindow* win, TocItem* tocItem, bool selectInTree);
bool IsScrollToLink(IPageDestination* link);
struct VistorForPageNoData {
    int pageNo = -1;

    TocItem* bestMatch = nullptr;
    int bestMatchPageNo = 0;
    int nItems = 0;
};
void visitTree(VistorForPageNoData* d, TreeItemVisitorData* vd);
struct VisitorForChapterData {
    int chapter = -1;
    int pageNo = 0;
    TocItem* match = nullptr;
    int matchPageNo = 0;
};
void visitTreeForChapter(VisitorForChapterData* d, TreeItemVisitorData* vd);
struct CollectSamePageData {
    int pageNo = 0;
    Vec<TocItem*>* out = nullptr;
};
void visitCollectSamePage(CollectSamePageData* d, TreeItemVisitorData* vd);
bool TocMatchingItemsContains(const Vec<TocItem*>& items, TocItem* item);
bool TocMatchingItemsEq(const Vec<TocItem*>& a, const Vec<TocItem*>& b);
void GetLeftRightCounts(TocItem* node, int& l2r, int& r2l);
void SetInitialExpandState(TocItem* item, Vec<int>& tocState);
void AddFavoriteFromToc(MainWindow* win, TocItem* dti);
void AutoExpandTopLevelItems(TocItem* root);
TocItem* FindTocItemByTitleAndPage(TocItem* item, Str title, int pageNo);
TocItem* FilterTocItemRec(TocItem* item, const StrVec& words);
