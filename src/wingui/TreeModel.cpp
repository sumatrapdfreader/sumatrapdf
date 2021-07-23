/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "wingui/TreeModel.h"

static bool VisitTreeItemRec(TreeModel* tm, TreeItem ti, const TreeItemVisitor& visitor) {
    if (ti == tm->ItemNull()) {
        return true;
    }
    bool cont = visitor(tm, ti);
    if (!cont) {
        return false;
    }
    int n = tm->ItemChildCount(ti);
    for (int i = 0; i < n; i++) {
        auto child = tm->ItemChildAt(ti, i);
        cont = VisitTreeItemRec(tm, child, visitor);
        if (!cont) {
            return false;
        }
    }
    return true;
}

bool VisitTreeModelItems(TreeModel* tm, const TreeItemVisitor& visitor) {
    int n = tm->RootCount();
    for (int i = 0; i < n; i++) {
        auto ti = tm->RootAt(i);
        if (!VisitTreeItemRec(tm, ti, visitor)) {
            return false;
        }
    }
    return true;
}
