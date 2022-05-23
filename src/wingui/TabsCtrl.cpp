/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Dpi.h"
#include "utils/WinUtil.h"

#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TabsCtrl.h"

#include "utils/Log.h"

Kind kindTabs = "tabs";

TabsCtrl2::TabsCtrl2() {
    dwStyle = WS_CHILD | WS_CLIPSIBLINGS | TCS_FOCUSNEVER | TCS_FIXEDWIDTH | TCS_FORCELABELLEFT | WS_VISIBLE;
    winClass = WC_TABCONTROLW;
    kind = kindTabs;
}

TabsCtrl2::~TabsCtrl2() = default;

static void Handle_WM_NOTIFY(void* user, WndEvent* ev) {
    CrashIf(ev->msg != WM_NOTIFY);
    TabsCtrl2* w = (TabsCtrl2*)user;
    ev->w = w; // TODO: is this needed?
    CrashIf(GetParent(w->hwnd) != (HWND)ev->hwnd);
    LPNMHDR hdr = (LPNMHDR)ev->lp;
    if (hdr->code == TTN_GETDISPINFOA) {
        logf("Handle_WM_NOTIFY TTN_GETDISPINFOA\n");
    } else if (hdr->code == TTN_GETDISPINFOW) {
        logf("Handle_WM_NOTIFY TTN_GETDISPINFOW\n");
    }
}

bool TabsCtrl2::Create(HWND parent) {
    if (createToolTipsHwnd) {
        dwStyle |= TCS_TOOLTIPS;
    }
    bool ok = WindowBase::Create(parent);
    if (!ok) {
        return false;
    }

    void* user = this;
    RegisterHandlerForMessage(hwnd, WM_NOTIFY, Handle_WM_NOTIFY, user);
    if (createToolTipsHwnd) {
        HWND ttHwnd = GetToolTipsHwnd();
        TOOLINFO ti{0};
        ti.cbSize = sizeof(ti);
        ti.hwnd = hwnd;
        ti.uId = 0;
        ti.uFlags = TTF_SUBCLASS;
        ti.lpszText = (WCHAR*)L"placeholder tooltip";
        SetRectEmpty(&ti.rect);
        RECT r = ti.rect;
        SendMessage(ttHwnd, TTM_ADDTOOL, 0, (LPARAM)&ti);
    }
    return true;
}

void TabsCtrl2::WndProc(WndEvent* ev) {
    HWND hwnd = ev->hwnd;
#if 0
    UINT msg = ev->msg;
    WPARAM wp = ev->wp;
    LPARAM lp = ev->lp;
    DbgLogMsg("tree:", hwnd, msg, wp, ev->lp);
#endif

    TabsCtrl2* w = this;
    CrashIf(w->hwnd != (HWND)hwnd);
}

Size TabsCtrl2::GetIdealSize() {
    Size sz{32, 128};
    return sz;
}

int TabsCtrl2::GetTabCount() {
    int n = TabCtrl_GetItemCount(hwnd);
    return n;
}

int TabsCtrl2::InsertTab(int idx, const char* s) {
    CrashIf(idx < 0);

    TCITEMW item{0};
    item.mask = TCIF_TEXT;
    item.pszText = ToWstrTemp(s);
    int insertedIdx = TabCtrl_InsertItem(hwnd, idx, &item);
    tooltips.InsertAt(idx, "");
    return insertedIdx;
}

void TabsCtrl2::RemoveTab(int idx) {
    CrashIf(idx < 0);
    CrashIf(idx >= GetTabCount());
    BOOL ok = TabCtrl_DeleteItem(hwnd, idx);
    CrashIf(!ok);
    tooltips.RemoveAt(idx);
}

void TabsCtrl2::RemoveAllTabs() {
    TabCtrl_DeleteAllItems(hwnd);
    tooltips.Reset();
}

void TabsCtrl2::SetTabText(int idx, const char* s) {
    CrashIf(idx < 0);
    CrashIf(idx >= GetTabCount());

    TCITEMW item{0};
    item.mask = TCIF_TEXT;
    item.pszText = ToWstrTemp(s);
    TabCtrl_SetItem(hwnd, idx, &item);
}

// result is valid until next call to GetTabText()
char* TabsCtrl2::GetTabText(int idx) {
    CrashIf(idx < 0);
    CrashIf(idx >= GetTabCount());

    WCHAR buf[512]{};
    TCITEMW item{0};
    item.mask = TCIF_TEXT;
    item.pszText = buf;
    item.cchTextMax = dimof(buf) - 1; // -1 just in case
    TabCtrl_GetItem(hwnd, idx, &item);
    char* s = ToUtf8Temp(buf);
    lastTabText.Set(s);
    return lastTabText.Get();
}

int TabsCtrl2::GetSelectedTabIndex() {
    int idx = TabCtrl_GetCurSel(hwnd);
    return idx;
}

int TabsCtrl2::SetSelectedTabByIndex(int idx) {
    int prevSelectedIdx = TabCtrl_SetCurSel(hwnd, idx);
    return prevSelectedIdx;
}

void TabsCtrl2::SetItemSize(Size sz) {
    TabCtrl_SetItemSize(hwnd, sz.dx, sz.dy);
}

void TabsCtrl2::SetToolTipsHwnd(HWND hwndTooltip) {
    TabCtrl_SetToolTips(hwnd, hwndTooltip);
}

HWND TabsCtrl2::GetToolTipsHwnd() {
    HWND res = TabCtrl_GetToolTips(hwnd);
    return res;
}

// TODO: this is a nasty implementation
// should probably TTM_ADDTOOL for each tab item
// we could re-calculate it in SetItemSize()
void TabsCtrl2::MaybeUpdateTooltip() {
    // logf("MaybeUpdateTooltip() start\n");
    HWND ttHwnd = GetToolTipsHwnd();
    if (!ttHwnd) {
        return;
    }

    {
        TOOLINFO ti{0};
        ti.cbSize = sizeof(ti);
        ti.hwnd = hwnd;
        ti.uId = 0;
        SendMessage(ttHwnd, TTM_DELTOOL, 0, (LPARAM)&ti);
    }

    {
        TOOLINFO ti{0};
        ti.cbSize = sizeof(ti);
        ti.hwnd = hwnd;
        ti.uFlags = TTF_SUBCLASS;
        // ti.lpszText = LPSTR_TEXTCALLBACK;
        WCHAR* ws = ToWstrTemp(currTooltipText.Get());
        ti.lpszText = ws;
        ti.uId = 0;
        GetClientRect(hwnd, &ti.rect);
        SendMessage(ttHwnd, TTM_ADDTOOL, 0, (LPARAM)&ti);
    }
}

void TabsCtrl2::MaybeUpdateTooltipText(int idx) {
    HWND ttHwnd = GetToolTipsHwnd();
    if (!ttHwnd) {
        return;
    }
    const char* tooltip = GetTooltip(idx);
    if (!tooltip) {
        // TODO: remove tooltip
        return;
    }
    currTooltipText.Set(tooltip);
#if 1
    MaybeUpdateTooltip();
#else
    // TODO: why this doesn't work?
    TOOLINFO ti{0};
    ti.cbSize = sizeof(ti);
    ti.hwnd = hwnd;
    ti.uFlags = TTF_SUBCLASS;
    ti.lpszText = currTooltipText.Get();
    ti.uId = 0;
    GetClientRect(hwnd, &ti.rect);
    SendMessage(ttHwnd, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
#endif
    // SendMessage(ttHwnd, TTM_UPDATE, 0, 0);

    SendMessage(ttHwnd, TTM_POP, 0, 0);
    SendMessage(ttHwnd, TTM_POPUP, 0, 0);
    // logf(L"MaybeUpdateTooltipText: %s\n", tooltip);
}

void TabsCtrl2::SetTooltip(int idx, const char* s) {
    tooltips.SetAt(idx, s);
}

const char* TabsCtrl2::GetTooltip(int idx) {
    if (idx >= tooltips.Size()) {
        return nullptr;
    }
    char* res = tooltips.at(idx);
    return res;
}
