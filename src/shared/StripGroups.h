/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// A strip group is a named, colored run of neighbouring tabs in the tab strip,
// like Chrome's tab groups. Not to be confused with saved Tab Groups
// (Settings.TabGroups, TabGroupsManage.cpp), which are sets of files.
//
//   [Home] [a.pdf] [ Work ][b.pdf][c.pdf] [ Misc (2) ] [d.pdf] [+]
//                   chip    members        collapsed

struct StripGroup {
    int id = 0;
    Str name;
    Color color = 0;
    bool collapsed = false;
    ~StripGroup();
};

struct StripGroupColor {
    Str name;
    Color color;
};
extern const StripGroupColor gStripGroupColors[];
extern const int gStripGroupColorsCount;

// what a session restore has created so far, keyed by TabState.Group
struct StripGroupRestore {
    Vec<int> keys;
    Vec<int> ids;
    Vec<int> collapsedIds;
};

struct AppCommandCtx;
struct TabState;
struct MainWindow;
struct WindowTab;

bool StripGroupOrder(const Vec<int>& groupIds, Vec<int>& order);
StripGroup* StripGroupFind(MainWindow*, int id);
StripGroup* StripGroupOf(MainWindow*, WindowTab*);
int StripGroupTabCount(MainWindow*, int id);
WindowTab* StripGroupFirstTab(MainWindow*, int id);
bool StripGroupCanHold(WindowTab*);
bool StripGroupIsHidden(MainWindow*, WindowTab*);
TempStr StripGroupLabelTemp(MainWindow*, StripGroup*);
void StripGroupsFix(MainWindow*);
void StripGroupsDelete(MainWindow*);
void StripGroupSetName(MainWindow*, int id, Str name);
void StripGroupSetCollapsed(MainWindow*, int id, bool collapsed);
bool StripGroupCmd(MainWindow*, WindowTab*, int cmdId);
void StripGroupDropTab(MainWindow*, int from, int to, int groupId);
void StripGroupMoveTab(MainWindow*, int dir);
void StripGroupOnSelected(MainWindow*, WindowTab*);
void StripGroupOnTabAdded(MainWindow*, WindowTab*);
int StripGroupNextVisible(MainWindow*, int idx);
void StripGroupSetFlags(AppCommandCtx&);
TempStr StripGroupsStateTemp(MainWindow*);

struct StripGroupMenuItem {
    Str title;
    int cmdId = 0;
    bool checked = false;
};
void StripGroupAddToItems(MainWindow*, WindowTab*, Vec<StripGroupMenuItem>&);
void StripGroupColorItems(MainWindow*, WindowTab*, Vec<StripGroupMenuItem>&);

void StripGroupSaveTab(MainWindow*, WindowTab*, TabState*);
void StripGroupRestoreTab(MainWindow*, WindowTab*, TabState*, StripGroupRestore&);
void StripGroupRestoreDone(MainWindow*, StripGroupRestore&);

// implemented by each app
void TabsSetOrder(MainWindow*, const Vec<WindowTab*>&);
void TabsStripGroupsChanged(MainWindow*);
void ShowRenameStripGroupDialog(MainWindow*, int groupId);
