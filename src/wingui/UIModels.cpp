/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "wingui/UIModels.h"

int ListBoxModelStrings::ItemsCount() {
    return strings.Size();
}

const char* ListBoxModelStrings::Item(int i) {
    return strings.At(i);
}

void FillWithItems(HWND hwnd, ListBoxModel* model) {
    SendMessageW(hwnd, WM_SETREDRAW, FALSE, 0);
    ListBox_ResetContent(hwnd);
    if (model) {
        int n = model->ItemsCount();
        SendMessageW(hwnd, LB_INITSTORAGE, (WPARAM)n, 0);
        for (int i = 0; i < n; i++) {
            auto sv = model->Item(i);
            auto ws = ToWStrTemp(sv);
            ListBox_AddString(hwnd, ws);
        }
    }
    SendMessageW(hwnd, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwnd, nullptr, TRUE);
}

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
