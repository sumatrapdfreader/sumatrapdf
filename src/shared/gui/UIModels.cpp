/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "gui/UIModels.h"

int ListBoxModelStrings::ItemsCount() {
    return len(strings);
}

Str ListBoxModelStrings::Item(int i) {
    return strings[i];
}

TreeModelItems::TreeModelItems(TreeModel* tm) : tm(tm) {}

TreeModelItems::iterator::iterator(TreeModel* tm, TreeItem curr) : tm(tm), curr(curr) {}

TreeItem TreeModelItems::iterator::operator*() const {
    return curr;
}

// pre-order: the current item's first child, else the next sibling of the
// nearest ancestor that still has one
TreeModelItems::iterator& TreeModelItems::iterator::operator++() {
    VecAppend(stack, {curr, 0});
    while (len(stack) > 0) {
        Frame& f = stack[len(stack) - 1];
        if (f.nextChild < tm->ChildCount(f.item)) {
            curr = tm->ChildAt(f.item, f.nextChild++);
            return *this;
        }
        VecPop(stack);
    }
    curr = TreeModel::kNullItem;
    return *this;
}

bool operator==(const TreeModelItems::iterator& a, const TreeModelItems::iterator& b) {
    return a.curr == b.curr;
}

bool operator!=(const TreeModelItems::iterator& a, const TreeModelItems::iterator& b) {
    return a.curr != b.curr;
}

TreeModelItems::iterator TreeModelItems::begin() const {
    return iterator(tm, tm ? tm->Root() : TreeModel::kNullItem);
}

TreeModelItems::iterator TreeModelItems::end() const {
    return iterator(tm, TreeModel::kNullItem);
}
