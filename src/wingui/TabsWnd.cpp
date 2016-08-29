#include "BaseUtil.h"
#include "TabsWnd.h"

TabsWnd *AllocTabsWnd(HWND hwndParent) {
    auto w = AllocStruct<TabsWnd>();
    w->hwndParent = hwndParent;
    return w;
}

bool CreateTabsWnd(TabsWnd* w) {
    DWORD dwStyle = WS_CHILD
        | WS_VISIBLE
        | WS_CLIPSIBLINGS
        | TCS_FOCUSNEVER
        | TCS_FIXEDWIDTH
        | TCS_FORCELABELLEFT;

    w->hwnd = CreateWindowW(WC_TABCONTROL, L"",
        dwStyle,
        0, 0, 0, 0,
        w->hwndParent, (HMENU)nullptr, GetModuleHandle(nullptr), nullptr);
    return w->hwnd != nullptr;
}

void DeleteTabsWnd(TabsWnd* w) {
    if (w) {
        // it's a child window so need to destroy hwnd
        free(w);
    }
}
