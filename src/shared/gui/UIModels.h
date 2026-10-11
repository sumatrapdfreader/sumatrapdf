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

struct ListBoxModelStrings : ListBoxModel {
    StrVec strings;

    int ItemsCount() override;
    Str Item(int) override;
};

// TreeItem represents an item in a TreeView control
typedef uintptr_t TreeItem;

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
    virtual void SetUserData(TreeItem, uintptr_t) = 0;
    virtual uintptr_t GetUserData(TreeItem) = 0;
};

// every item of a TreeModel in pre-order (the root first), for range-for:
//   for (TreeItem ti : TreeModelItems(tm)) { ... }
// break leaves the walk early
struct TreeModelItems {
    TreeModel* tm = nullptr;

    struct iterator {
        TreeModel* tm = nullptr;
        TreeItem curr = TreeModel::kNullItem;
        // an ancestor of curr and the index of its child to go to next
        struct Frame {
            TreeItem item;
            int nextChild;
        };
        Vec<Frame> stack;

        iterator(TreeModel* tm, TreeItem curr);
        TreeItem operator*() const;
        iterator& operator++(); // ++it
        friend bool operator==(const iterator& a, const iterator& b);
        friend bool operator!=(const iterator& a, const iterator& b);
    };
    explicit TreeModelItems(TreeModel* tm);
    iterator begin() const;
    iterator end() const;
};
