/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TabsCtrl;
class TabsCtrlState;

typedef std::function<void(TabsCtrl*, TabsCtrlState*, int)> TabSelectedCb;
typedef std::function<void(TabsCtrl*, TabsCtrlState*, int)> TabClosedCb;

class TabItem {
  public:
    TabItem(const std::string_view title, const std::string_view toolTip);

    str::Str title;
    str::Str toolTip;

    str::Str iconSvgPath;
};

class TabsCtrlState {
  public:
    Vec<TabItem*> tabs;
    int selectedItem = 0;
};

class TabsCtrlPrivate;

struct TabsCtrl {
    // creation parameters. must be set before CreateTabsCtrl
    HWND parent = nullptr;
    RECT initialPos = {};

    TabSelectedCb onTabSelected = nullptr;
    TabClosedCb onTabClosed = nullptr;

    TabsCtrlPrivate* priv;
};

/* Creation sequence:
- AllocTabsCtrl()
- set creation parameters
- CreateTabsCtrl()
*/

TabsCtrl* AllocTabsCtrl(HWND parent, RECT initialPosition);
bool CreateTabsCtrl(TabsCtrl*);
void DeleteTabsCtrl(TabsCtrl*);
void SetState(TabsCtrl*, TabsCtrlState*);
SIZE GetIdealSize(TabsCtrl*);
void SetPos(TabsCtrl*, RECT&);
void SetFont(TabsCtrl*, HFONT);

struct TabsCtrl2 : WindowBase {
    str::WStr lastTabText;
    bool createToolTipsHwnd{false};

    // for all WM_NOTIFY messages
    WmNotifyHandler onNotify{nullptr};

    TabsCtrl2(HWND parent);
    ~TabsCtrl2() override;
    bool Create() override;

    void WndProc(WndEvent*) override;

    Size GetIdealSize() override;

    int InsertTab(int idx, std::string_view sv);
    int InsertTab(int idx, const WCHAR* ws);

    void RemoveTab(int idx);
    void RemoveAllTabs();

    void SetTabText(int idx, std::string_view sv);
    void SetTabText(int idx, const WCHAR* ws);

    WCHAR* GetTabText(int idx);

    int GetSelectedTabIndex();
    int SetSelectedTabByIndex(int idx);

    void SetItemSize(Size sz);
    int GetTabCount();

    void SetToolTipsHwnd(HWND);
    HWND GetToolTipsHwnd();
};
