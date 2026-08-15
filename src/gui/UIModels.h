/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
Data models for UI controls.
Don't depend on UI subsystem because they are used in non-gui code e.g. engines.
*/

// The keys the UI reacts to by name, so it doesn't compare against the
// platform's key constants. The values are the control characters those keys
// produce, which is what a character event carries.
enum class Key : int {
    Tab = 9,
    Enter = 13,
    Escape = 27,
};

// The mouse cursors the UI asks for by name. The platform layer maps them to
// its own (on Windows, the IDC_* cursors)
enum class CursorId {
    None = 0,
    Arrow,
    IBeam,
    Hand,
    Cross,
    Move,
    SizeNS,
    SizeWE,
    No,
};

struct ListBoxModel {
    virtual ~ListBoxModel() = default;
    virtual int ItemsCount() = 0;
    virtual Str Item(int) = 0;
};

using DropDownModel = ListBoxModel;

struct ListBoxModelStrings : ListBoxModel {
    StrVec strings;

    ~ListBoxModelStrings() override = default;
    int ItemsCount() override;
    Str Item(int) override;
};

using DropDownModelStrings = ListBoxModelStrings;

// TreeItem represents an item in a TreeView control
typedef UINT_PTR TreeItem;

// TreeModel provides data to TreeCtrl
struct TreeModel {
    static const TreeItem kNullItem = 0;

    virtual ~TreeModel() = default;

    virtual TreeItem Root() = 0;

    virtual Str Text(TreeItem) = 0;
    virtual TreeItem Parent(TreeItem) = 0;
    virtual int ChildCount(TreeItem) = 0;
    virtual TreeItem ChildAt(TreeItem, int idx) = 0;
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
