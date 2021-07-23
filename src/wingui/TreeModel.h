/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// TreeItem represents an item in a TreeView control
typedef UINT_PTR TreeItem;

constexpr TreeItem kRootItem = (UINT_PTR)-2;

// TreeModel provides data to TreeCtrl
struct TreeModel {
    virtual ~TreeModel() = default;

    virtual int RootCount() = 0;
    virtual TreeItem RootAt(int) = 0;

    // TODO: convert to char*
    virtual WCHAR* ItemText(TreeItem) = 0;
    virtual TreeItem ItemParent(TreeItem) = 0;
    virtual int ItemChildCount(TreeItem) = 0;
    virtual TreeItem ItemChildAt(TreeItem, int index) = 0;
    // true if this tree item should be expanded i.e. showing children
    virtual bool ItemIsExpanded(TreeItem) = 0;
    // when showing checkboxes
    virtual bool ItemIsChecked(TreeItem) = 0;

    // returns a value that represents non-existent (null) item
    virtual TreeItem ItemNull() = 0;

    virtual void SetHandle(TreeItem, HTREEITEM) = 0;
    virtual HTREEITEM GetHandle(TreeItem) = 0;
};

// function called for every item in the TreeModel
// return false to stop iteration
using TreeItemVisitor = std::function<bool(TreeModel*, TreeItem)>;

bool VisitTreeModelItems(TreeModel*, const TreeItemVisitor& visitor);
