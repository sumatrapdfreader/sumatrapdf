/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "wingui/UIModels.h"

int ListBoxModelStrings::ItemsCount() {
    return strings.Size();
}

const char* ListBoxModelStrings::Item(int i) {
    return strings.at(i);
}

void FillWithItems(HWND hwnd, ListBoxModel* model) {
    ListBox_ResetContent(hwnd);
    if (model) {
        for (int i = 0; i < model->ItemsCount(); i++) {
            auto sv = model->Item(i);
            auto ws = ToWStrTemp(sv);
            ListBox_AddString(hwnd, ws);
        }
    }
}

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
