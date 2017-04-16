

struct TabsCtrl;
class TabsCtrlState;

typedef std::function<void(TabsCtrl*, std::shared_ptr<TabsCtrlState>, int)> TabSelectedCb;
typedef std::function<void(TabsCtrl*, std::shared_ptr<TabsCtrlState>, int)> TabClosedCb;

class TabItem {
public:
    TabItem(const std::string& title, const std::string& toolTip);

    std::string title;
    std::string toolTip;

    std::string iconSvgPath;
};

class TabsCtrlState {
public:
    std::vector < std::unique_ptr<TabItem>> tabs;
    int selectedItem = 0;
};

class TabsCtrlPrivate;

struct TabsCtrl {
    // creation parameters. must be set before CreateTabsCtrl
    HWND parent;
    RECT initialPos;

    TabSelectedCb onTabSelected;
    TabClosedCb onTabClosed;

    TabsCtrlPrivate *priv;
};

/* Creation sequence:
- AllocTabsCtrl()
- set creation parameters
- CreateTabsCtrl()
*/

TabsCtrl* AllocTabsCtrl(HWND parent, RECT initialPosition);
bool CreateTabsCtrl(TabsCtrl*);
void DeleteTabsCtrl(TabsCtrl*);
void SetState(TabsCtrl*, std::shared_ptr<TabsCtrlState>);
SIZE GetIdealSize(TabsCtrl*);
void SetPos(TabsCtrl*, RECT&);
void SetFont(TabsCtrl*, HFONT);
