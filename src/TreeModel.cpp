/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "TreeModel.h"

static bool VisitTreeItemRec(TreeModel* tm, TreeItem ti, const TreeItemVisitor& visitor) {
    if (ti == TreeModel::kNullItem) {
        return true;
    }
    TreeItemVisitorData d;
    d.model = tm;
    d.item = ti;
    visitor.Call(&d);
    bool cont = !d.stopTraversal;
    if (!cont) {
        return false;
    }
    int n = tm->ChildCount(ti);
    for (int i = 0; i < n; i++) {
        auto child = tm->ChildAt(ti, i);
        cont = VisitTreeItemRec(tm, child, visitor);
        if (!cont) {
            return false;
        }
    }
    return true;
}

bool VisitTreeModelItems(TreeModel* tm, const TreeItemVisitor& visitor) {
    TreeItem root = tm->Root();
    return VisitTreeItemRec(tm, root, visitor);
}
