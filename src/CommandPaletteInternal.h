/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct MainWindow;
struct WindowTab;
struct TocItem;
struct FileState;
struct Favorite;

struct ItemDataCP {
    i32 cmdId = 0;
    WindowTab* tab = nullptr;
    Str filePath;
    TocItem* tocItem = nullptr;
    int indent = 0;
    int pageNo = 0; // toc entry destination page (0 if none), shown in the list
    FileState* favFs = nullptr;
    Favorite* fav = nullptr;
};

using StrVecCP = StrVecWithData<ItemDataCP>;

struct ListBoxModelCP : ListBoxModel {
    StrVecCP strings;

    ListBoxModelCP() = default;
    ~ListBoxModelCP() override = default;
    int ItemsCount() override { return len(strings); }
    Str Item(int i) override { return strings[i]; }
    ItemDataCP* Data(int i) { return strings.AtData(i); }
};

struct CommandPaletteWnd : Wnd {
    ~CommandPaletteWnd() override = default;
    HFONT font = nullptr;
    MainWindow* win = nullptr;

    Edit* editQuery = nullptr;
    StrVecCP tabs;
    StrVecCP fileHistory;
    StrVecCP commands;
    StrVecCP toc;
    StrVecCP favorites;
    ListBox* listBox = nullptr;
    Static* staticInfo = nullptr;

    StrVec filterWords;
    Vec<u8> highlighted;

    int currTabIdx = 0;
    int currTocIdx = 0;
    bool tocMode = false;
    bool smartTabMode = false;
    bool stickyMode = false;

    bool PreTranslateMessage(MSG&) override;
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;

    void CollectStrings(MainWindow*);
    void CollectTabsRegular(MainWindow*, WindowTab* currTab);
    void CollectTabsMru(MainWindow*, WindowTab* currTab);
    void CollectToc(MainWindow*);
    void CollectFavorites(MainWindow*);
    void FilterStringsForQuery(Str, StrVecCP&);

    bool Create(MainWindow* win, Str prefix, int smartTabAdvance);
    void QueryChanged();

    void ExecuteCurrentSelection();
    bool AdvanceSelection(int dir);
    void SwitchToPrefix(Str prefix);
    void SwitchToCommands();
    void SwitchToTabs();
    void SwitchToEverything();
    void SwitchToFileHistory();
    void SwitchToTOC();
    void SwitchToFavorites();
    void OnSelectionChange();
    void OnListDoubleClick();
    void DrawListBoxItem(ListBox::DrawItemEvent* ev);
};

extern CommandPaletteWnd* gCommandPaletteWnd;
extern HWND gCommandPaletteHwnd;

Str CommandPaletteSkipWS(Str s);
void CommandPaletteSetCurrentSelection(CommandPaletteWnd* wnd, int idx);
void ScheduleDeleteAndExecCommand(i32 cmdId = 0);
void SafeDeleteCommandPaletteWnd();
void PositionCommandPalette(HWND hwnd, HWND hwndRelative);