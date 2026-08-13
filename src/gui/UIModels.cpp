/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/UIModels.h"

int ListBoxModelStrings::ItemsCount() {
    return len(strings);
}

Str ListBoxModelStrings::Item(int i) {
    return strings[i];
}

void FillWithItems(HWND hwnd, ListBoxModel* model) {
    AutoArenaSavepoint scratch;
    SendMessageW(hwnd, WM_SETREDRAW, FALSE, 0);
    LbResetContent(hwnd);
    if (model) {
        int n = model->ItemsCount();
        LbInitStorage(hwnd, n);
        for (int i = 0; i < n; i++) {
            auto sv = model->Item(i);
            LbAddString(hwnd, sv);
        }
    }
    SendMessageW(hwnd, WM_SETREDRAW, TRUE, 0);
    HwndInvalidate(hwnd, true);
}
