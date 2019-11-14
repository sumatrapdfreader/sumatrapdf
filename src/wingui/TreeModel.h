/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// TreeItem represents an item in a TreeView control
struct TreeItem {
    // TODO: convert to char*
    virtual WCHAR* Text() = 0;
    virtual TreeItem* Parent() = 0;
    virtual int ChildCount() = 0;
    virtual TreeItem* ChildAt(int index) = 0;
    virtual bool IsOpened() = 0;
};

// TreeModel provides data to TreeCtrl
struct TreeModel {
    virtual int RootCount() = 0;
    virtual TreeItem* RootAt(int) = 0;
};
