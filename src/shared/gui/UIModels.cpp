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

static bool VisitTreeItemRec(TreeModel* tm, TreeItem ti, const TreeItemVisitor& visitor) {
    if (ti == TreeModel::kNullItem) {
        return true;
    }
    TreeItemVisitorData d;
    d.item = ti;
    visitor.Call(&d);
    if (d.stopTraversal) {
        return false;
    }
    int n = tm->ChildCount(ti);
    for (int i = 0; i < n; i++) {
        if (!VisitTreeItemRec(tm, tm->ChildAt(ti, i), visitor)) {
            return false;
        }
    }
    return true;
}

bool VisitTreeModelItems(TreeModel* tm, const TreeItemVisitor& visitor) {
    return VisitTreeItemRec(tm, tm->Root(), visitor);
}
