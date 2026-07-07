/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "wingui/UIModels.h"

int ListBoxModelStrings::ItemsCount() {
    return len(strings);
}

Str ListBoxModelStrings::Item(int i) {
    return strings[i];
}

void FillWithItems(HWND hwnd, ListBoxModel* model) {
    SendMessageW(hwnd, WM_SETREDRAW, FALSE, 0);
    ListBox_ResetContent(hwnd);
    if (model) {
        int n = model->ItemsCount();
        SendMessageW(hwnd, LB_INITSTORAGE, (WPARAM)n, 0);
        for (int i = 0; i < n; i++) {
            auto sv = model->Item(i);
            WCHAR* ws = CWStrTemp(sv);
            ListBox_AddString(hwnd, ws);
        }
    }
    SendMessageW(hwnd, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwnd, nullptr, TRUE);
}
