
struct TabsWnd {
    HWND hwnd;
    HWND hwndParent;
};

TabsWnd *AllocTabsWnd(HWND hwndParent);
bool CreateTabsWnd(TabsWnd*);
void DeleteTabsWnd(TabsWnd*);


