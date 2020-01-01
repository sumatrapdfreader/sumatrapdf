/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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
