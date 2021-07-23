/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "wingui/TreeModel.h"

static bool VisitTreeItemRec(TreeModel* tm, TreeItem ti, const TreeItemVisitor& visitor) {
    if (ti == TreeModel::kNullItem) {
        return true;
    }
    bool cont = visitor(tm, ti);
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
