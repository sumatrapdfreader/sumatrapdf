/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "wingui/TreeModel.h"

static bool VisitTreeItemRec(TreeItem* ti, const TreeItemVisitor& visitor) {
    bool cont;
    if (ti != nullptr) {
        cont = visitor(ti);
        if (!cont) {
            return false;
        }
    }
    int n = ti->ChildCount();
    for (int i = 0; i < n; i++) {
        auto child = ti->ChildAt(i);
        cont = VisitTreeItemRec(child, visitor);
        if (!cont) {
            return false;
        }
    }
    return true;
}

bool VisitTreeModelItems(TreeModel* tm, const TreeItemVisitor& visitor) {
    int n = tm->RootCount();
    for (int i = 0; i < n; i++) {
        auto* ti = tm->RootAt(i);
        if (!VisitTreeItemRec(ti, visitor)) {
            return false;
        }
    }
    return true;
}
