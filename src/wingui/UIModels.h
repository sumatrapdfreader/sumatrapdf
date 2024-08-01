/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
Data models for UI controls.
Don't depend on UI subsystem because they are used in non-gui code e.g. engines.
*/

struct ListBoxModel {
    virtual ~ListBoxModel() = default;
    virtual int ItemsCount() = 0;
    virtual const char* Item(int) = 0;
};

using DropDownModel = ListBoxModel;

struct ListBoxModelStrings : ListBoxModel {
    StrVec strings;

    ~ListBoxModelStrings() override = default;
    int ItemsCount() override;
    const char* Item(int) override;
};

using DropDownModelStrings = ListBoxModelStrings;

void FillWithItems(HWND hwnd, ListBoxModel* model);

// TreeItem represents an item in a TreeView control
typedef UINT_PTR TreeItem;

// TreeModel provides data to TreeCtrl
struct TreeModel {
    static const TreeItem kNullItem = 0;

    virtual ~TreeModel() = default;

    virtual TreeItem Root() = 0;

    virtual char* Text(TreeItem) = 0;
    virtual TreeItem Parent(TreeItem) = 0;
    virtual int ChildCount(TreeItem) = 0;
    virtual TreeItem ChildAt(TreeItem, int index) = 0;
    // true if this tree item should be expanded i.e. showing children
    virtual bool IsExpanded(TreeItem) = 0;
    // when showing checkboxes
    virtual bool IsChecked(TreeItem) = 0;
    virtual void SetHandle(TreeItem, HTREEITEM) = 0;
    virtual HTREEITEM GetHandle(TreeItem) = 0;
};

struct TreeItemVisitorData {
    TreeModel* model = nullptr;
    TreeItem item = 0;
    bool stopTraversal = false;
};

// function called for every item in the TreeModel
// return false to stop iteration
using TreeItemVisitor = Func1<TreeItemVisitorData*>;

bool VisitTreeModelItems(TreeModel*, const TreeItemVisitor& visitor);

struct TreeItemState {
    bool isSelected = false;
    bool isExpanded = false;
    bool isChecked = false;
    int nChildren = 0;
};