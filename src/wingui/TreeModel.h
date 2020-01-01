/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// TreeItem represents an item in a TreeView control
struct TreeItem {
    virtual ~TreeItem(){};

    // TODO: convert to char*
    virtual WCHAR* Text() = 0;
    virtual TreeItem* Parent() = 0;
    virtual int ChildCount() = 0;
    virtual TreeItem* ChildAt(int index) = 0;
    // true if this tree item should be expanded i.e. showing children
    virtual bool IsExpanded() = 0;
    // when showing checkboxes
    virtual bool IsChecked() = 0;
};

// TreeModel provides data to TreeCtrl
struct TreeModel {
    virtual ~TreeModel(){};

    virtual int RootCount() = 0;
    virtual TreeItem* RootAt(int) = 0;
};

// function called for every item in the TreeModel
// return false to stop iteration
typedef std::function<bool(TreeItem*)> TreeItemVisitor;

bool VisitTreeModelItems(TreeModel* tm, const TreeItemVisitor& visitor);
