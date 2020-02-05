/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// very similar to TocItem
struct BkmItem : TreeItem {
    // each engine has a raw representation of the toc item which
    // we want to access. Not (yet) supported by all engines
    // other values come from parsing this value
    Kind kindRaw = nullptr;

    char* rawVal1 = nullptr;
    char* rawVal2 = nullptr;

    // name of the file for this sub-tree of bookmark tree
    char* engineFilePath = nullptr;

    BkmItem* parent = nullptr;

    // the item's visible label
    WCHAR* title = nullptr;

    // in some formats, the document can specify the tree item
    // is expanded by default. We keep track if user toggled
    // expansion state of the tree item
    bool isOpenDefault = false;
    bool isOpenToggled = false;

    bool isUnchecked = false;

    // page this item points to (0 for non-page destinations)
    // if GetLink() returns a destination to a page, the two should match
    int pageNo = 0;

    // auto-calculated page number that tells us a span from
    // pageNo => endPageNo
    // only used by TocEditor and EngineMulti
    int endPageNo = 0;

    // arbitrary number allowing to distinguish this TocItem
    // from any other of the same ToC tree (must be constant
    // between runs so that it can be persisted in FileState::tocState)
    int id = 0;

    int fontFlags = 0; // fontBitBold, fontBitItalic
    COLORREF color = ColorUnset;

    PageDestination* dest = nullptr;

    // first child item
    BkmItem* child = nullptr;
    // next sibling
    BkmItem* next = nullptr;

    BkmItem() = default;

    explicit BkmItem(BkmItem* parent, const WCHAR* title, int pageNo);

    ~BkmItem() override;

    void AddSibling(BkmItem* sibling);

    void OpenSingleNode();

    PageDestination* GetPageDestination();

    WCHAR* Text();

    bool PageNumbersMatch() const;

    // TreeItem
    TreeItem* Parent() override;
    int ChildCount() override;
    TreeItem* ChildAt(int n) override;
    bool IsExpanded() override;
    bool IsChecked() override;
};

// very similar to TocTree
struct BkmTree : TreeModel {
    BkmItem* root = nullptr;

    BkmTree() = default;
    BkmTree(BkmItem* root);
    ~BkmTree() override;

    // TreeModel
    int RootCount() override;
    TreeItem* RootAt(int n) override;
};

// represents bookmarks for a single file
struct VbkmForFile {
    // path of the original file
    AutoFree filePath;
    // TODO: serialize nPages after "file:"
    int nPages = 0;

    TocTree* toc = nullptr;

    EngineBase* engine = nullptr;

    VbkmForFile() = default;
    ~VbkmForFile();
};

// represents a .vbkm file which represents one or more
// physical files
struct VbkmFile {
    AutoFree fileContent;
    AutoFree name;
    Vec<VbkmForFile*> vbkms;

    VbkmFile() = default;
    ~VbkmFile();
};

bool ExportBookmarksToFile(const Vec<VbkmForFile*>&, const char* name, const char* path);

bool LoadAlterenativeBookmarks(std::string_view baseFileName, VbkmFile& vbkm);

bool ParseVbkmFile(std::string_view d, VbkmFile& vbkm);
bool LoadVbkmFile(const char* filePath, VbkmFile& vbkm);
