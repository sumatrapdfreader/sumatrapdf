/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "Commands.h"
#include "CommandAvailability.h"
#include "Translations.h"
#include "Tabs.h"
#include "StripGroups.h"

// Chrome's palette; a new group takes the first color no other group has
const StripGroupColor gStripGroupColors[] = {
    {TrN("Blue"), MkRgb(0x1a, 0x73, 0xe8)},   {TrN("Red"), MkRgb(0xd9, 0x30, 0x25)},
    {TrN("Yellow"), MkRgb(0xf2, 0xa6, 0x00)}, {TrN("Green"), MkRgb(0x18, 0x80, 0x38)},
    {TrN("Pink"), MkRgb(0xd0, 0x18, 0x84)},   {TrN("Purple"), MkRgb(0x8e, 0x44, 0xd8)},
    {TrN("Cyan"), MkRgb(0x00, 0x8b, 0x9a)},   {TrN("Orange"), MkRgb(0xe8, 0x71, 0x0a)},
    {TrN("Grey"), MkRgb(0x5f, 0x63, 0x68)},
};
const int gStripGroupColorsCount = dimofi(gStripGroupColors);

// unique across windows, so a tab moved to another window can't land in a
// group there by accident
static int gNextStripGroupId = 1;

StripGroup::~StripGroup() {
    str::Free(name);
}

// The order that makes every group's tabs neighbours: a group's members gather
// where its first member is. groupIds has one entry per tab (0: no group).
// Returns false if the tabs already are in that order.
//   ids [0 A 0 A B] -> order [0 1 3 2 4]
bool StripGroupOrder(const Vec<int>& groupIds, Vec<int>& order) {
    VecReset(order);
    Vec<int> placed;
    int n = len(groupIds);
    for (int i = 0; i < n; i++) {
        int id = groupIds[i];
        if (id == 0) {
            VecAppend(order, i);
            continue;
        }
        if (VecContains(placed, id)) {
            continue;
        }
        VecAppend(placed, id);
        for (int j = i; j < n; j++) {
            if (groupIds[j] == id) {
                VecAppend(order, j);
            }
        }
    }
    for (int i = 0; i < n; i++) {
        if (order[i] != i) {
            return true;
        }
    }
    return false;
}

StripGroup* StripGroupFind(MainWindow* win, int id) {
    if (!win || id == 0) {
        return nullptr;
    }
    for (StripGroup* g : win->stripGroups) {
        if (g->id == id) {
            return g;
        }
    }
    return nullptr;
}

StripGroup* StripGroupOf(MainWindow* win, WindowTab* tab) {
    return tab ? StripGroupFind(win, tab->stripGroupId) : nullptr;
}

int StripGroupTabCount(MainWindow* win, int id) {
    int n = 0;
    for (WindowTab* tab : win->Tabs()) {
        if (tab->stripGroupId == id) {
            n++;
        }
    }
    return n;
}

WindowTab* StripGroupFirstTab(MainWindow* win, int id) {
    for (WindowTab* tab : win->Tabs()) {
        if (tab->stripGroupId == id) {
            return tab;
        }
    }
    return nullptr;
}

// Home tabs stay out of groups
bool StripGroupCanHold(WindowTab* tab) {
    return tab && !tab->IsAboutTab();
}

bool StripGroupIsHidden(MainWindow* win, WindowTab* tab) {
    StripGroup* g = StripGroupOf(win, tab);
    return g && g->collapsed;
}

// the chip's text: the name, plus how many tabs it hides when collapsed
TempStr StripGroupLabelTemp(MainWindow* win, StripGroup* g) {
    if (!g->collapsed) {
        return str::DupTemp(g->name);
    }
    int n = StripGroupTabCount(win, g->id);
    if (len(g->name) == 0) {
        return fmt("%d", n);
    }
    return fmt("%s (%d)", g->name, n);
}

// Drops group ids the window doesn't know or a tab can't have, makes each
// group's tabs neighbours and forgets groups with no tabs
void StripGroupsFix(MainWindow* win) {
    // nothing to fix, and no need to rebuild the strip for every closed tab
    if (len(win->stripGroups) == 0) {
        return;
    }
    Vec<WindowTab*> tabs = win->Tabs();
    Vec<int> ids;
    for (WindowTab* tab : tabs) {
        if (tab->stripGroupId != 0 && (!StripGroupFind(win, tab->stripGroupId) || !StripGroupCanHold(tab))) {
            tab->stripGroupId = 0;
        }
        VecAppend(ids, tab->stripGroupId);
    }
    Vec<int> order;
    bool moved = StripGroupOrder(ids, order);

    // groups in strip order
    Vec<StripGroup*> groups;
    for (int i : order) {
        StripGroup* g = StripGroupFind(win, ids[i]);
        if (g && !VecContains(groups, g)) {
            VecAppend(groups, g);
        }
    }
    for (StripGroup* g : win->stripGroups) {
        if (!VecContains(groups, g)) {
            delete g;
        }
    }
    win->stripGroups = groups;

    if (!moved) {
        TabsStripGroupsChanged(win);
        return;
    }
    Vec<WindowTab*> ordered;
    for (int i : order) {
        VecAppend(ordered, tabs[i]);
    }
    TabsSetOrder(win, ordered);
}

void StripGroupsDelete(MainWindow* win) {
    DeleteVecMembers(win->stripGroups);
    VecReset(win->stripGroups);
}

static Color NextFreeColor(MainWindow* win) {
    for (int i = 0; i < gStripGroupColorsCount; i++) {
        bool used = false;
        for (StripGroup* g : win->stripGroups) {
            used |= g->color == gStripGroupColors[i].color;
        }
        if (!used) {
            return gStripGroupColors[i].color;
        }
    }
    return gStripGroupColors[len(win->stripGroups) % gStripGroupColorsCount].color;
}

static StripGroup* NewGroup(MainWindow* win, Str name, Color color) {
    auto* g = new StripGroup();
    g->id = gNextStripGroupId++;
    g->name = str::Dup(name);
    g->color = color;
    VecAppend(win->stripGroups, g);
    return g;
}

// Puts the tab into group `id` (0: none), after the group's last tab. A tab
// leaving a group lands right after what's left of it
static void MoveToGroup(MainWindow* win, WindowTab* tab, int id) {
    int oldId = tab->stripGroupId;
    if (oldId == id) {
        return;
    }
    Vec<WindowTab*> tabs = win->Tabs();
    int idx = VecFind(tabs, tab);
    VecRemoveAt(tabs, idx);
    int anchor = id != 0 ? id : oldId;
    int insertAt = idx;
    for (int i = 0; i < len(tabs); i++) {
        if (tabs[i]->stripGroupId == anchor) {
            insertAt = i + 1;
        }
    }
    tab->stripGroupId = id;
    VecInsertAt(tabs, insertAt, tab);
    TabsSetOrder(win, tabs);
    StripGroupsFix(win);
    ScheduleSaveSettings();
}

// The visible tab nearest to idx: idx itself or the first one after it, else
// the last one before it. -1 if every tab is hidden
int StripGroupNextVisible(MainWindow* win, int idx) {
    int n = win->TabCount();
    for (int i = std::max(idx, 0); i < n; i++) {
        if (!StripGroupIsHidden(win, win->GetTab(i))) {
            return i;
        }
    }
    for (int i = std::min(idx, n) - 1; i >= 0; i--) {
        if (!StripGroupIsHidden(win, win->GetTab(i))) {
            return i;
        }
    }
    return -1;
}

// A collapsed group can't hold the selected tab: selection moves to the
// nearest visible tab. With none left the group stays open
void StripGroupSetCollapsed(MainWindow* win, int id, bool collapsed) {
    StripGroup* g = StripGroupFind(win, id);
    if (!g || g->collapsed == collapsed) {
        return;
    }
    g->collapsed = collapsed;
    WindowTab* cur = win->CurrentTab();
    if (collapsed && cur && cur->stripGroupId == id) {
        int idx = StripGroupNextVisible(win, win->GetTabIdx(cur));
        if (idx < 0) {
            g->collapsed = false;
            return;
        }
        TabsSelect(win, idx);
    }
    TabsStripGroupsChanged(win);
    ScheduleSaveSettings();
}

void StripGroupSetName(MainWindow* win, int id, Str name) {
    StripGroup* g = StripGroupFind(win, id);
    if (!g) {
        return;
    }
    str::TrimWsBoth(name);
    str::ReplaceWithCopy(&g->name, name);
    TabsStripGroupsChanged(win);
    ScheduleSaveSettings();
}

static void SetColor(MainWindow* win, StripGroup* g, CustomCommand* cmd) {
    CommandArg* arg = GetCommandArg(cmd, kCmdArgColor);
    if (arg && arg->colorVal.parsedOk) {
        g->color = arg->colorVal.col;
    } else {
        // no color given: the next one in the palette
        int i = 0;
        while (i < gStripGroupColorsCount && gStripGroupColors[i].color != g->color) {
            i++;
        }
        g->color = gStripGroupColors[(i + 1) % gStripGroupColorsCount].color;
    }
    TabsStripGroupsChanged(win);
    ScheduleSaveSettings();
}

static void Ungroup(MainWindow* win, int id) {
    for (WindowTab* tab : win->Tabs()) {
        if (tab->stripGroupId == id) {
            tab->stripGroupId = 0;
        }
    }
    StripGroupsFix(win);
    ScheduleSaveSettings();
}

static void CloseGroup(MainWindow* win, int id) {
    Vec<WindowTab*> toClose;
    for (WindowTab* tab : win->Tabs()) {
        if (tab->stripGroupId == id) {
            VecAppend(toClose, tab);
        }
    }
    CloseCollectedTabs(win, toClose);
}

static bool IsStripGroupCmd(int cmdId) {
    switch (cmdId) {
        case CmdStripGroupNew:
        case CmdStripGroupAddTab:
        case CmdStripGroupRemoveTab:
        case CmdStripGroupToggle:
        case CmdStripGroupRename:
        case CmdStripGroupColor:
        case CmdStripGroupUngroup:
        case CmdStripGroupClose:
            return true;
    }
    return false;
}

// Runs a strip group command on `tab` (the current tab, or the one a menu was
// opened for). Returns false if cmdId is not one of them
bool StripGroupCmd(MainWindow* win, WindowTab* tab, int cmdId) {
    CustomCommand* cmd = FindCustomCommand(cmdId);
    if (cmd) {
        cmdId = cmd->origId;
    }
    if (!IsStripGroupCmd(cmdId)) {
        return false;
    }
    if (!win || !tab || win->GetTabIdx(tab) < 0 || !StripGroupCanHold(tab)) {
        return true;
    }
    StripGroup* g = StripGroupOf(win, tab);
    if (cmdId == CmdStripGroupNew) {
        StripGroup* newGroup = NewGroup(win, {}, NextFreeColor(win));
        MoveToGroup(win, tab, newGroup->id);
        return true;
    }
    if (cmdId == CmdStripGroupAddTab) {
        int n = GetCommandIntArg(cmd, kCmdArgN, 0);
        if (n >= 1 && n <= len(win->stripGroups)) {
            MoveToGroup(win, tab, win->stripGroups[n - 1]->id);
        }
        return true;
    }
    if (!g) {
        return true;
    }
    switch (cmdId) {
        case CmdStripGroupRemoveTab:
            MoveToGroup(win, tab, 0);
            break;
        case CmdStripGroupToggle:
            StripGroupSetCollapsed(win, g->id, !g->collapsed);
            break;
        case CmdStripGroupRename:
            ShowRenameStripGroupDialog(win, g->id);
            break;
        case CmdStripGroupColor:
            SetColor(win, g, cmd);
            break;
        case CmdStripGroupUngroup:
            Ungroup(win, g->id);
            break;
        case CmdStripGroupClose:
            CloseGroup(win, g->id);
            break;
    }
    return true;
}

// A tab dragged to position `to` (before the tab there) joins `groupId`, the
// group of the tab or chip it was dropped on (0: none)
void StripGroupDropTab(MainWindow* win, int from, int to, int groupId) {
    int n = win->TabCount();
    if (from < 0 || from >= n || to < 0 || to > n) {
        return;
    }
    WindowTab* tab = win->GetTab(from);
    if (tab->IsPinnedHomeTab()) {
        return;
    }
    if (!StripGroupCanHold(tab) || !StripGroupFind(win, groupId)) {
        groupId = 0;
    }
    bool inPlace = (to == from) || (to == from + 1);
    if (inPlace && tab->stripGroupId == groupId) {
        return;
    }
    Vec<WindowTab*> tabs = win->Tabs();
    VecRemoveAt(tabs, from);
    if (from < to) {
        to--;
    }
    VecInsertAt(tabs, to, tab);
    tab->stripGroupId = groupId;
    TabsSetOrder(win, tabs);
    StripGroupsFix(win);
    TabsSelect(win, win->GetTabIdx(tab));
    ScheduleSaveSettings();
}

// The current tab one step left (dir -1) or right (dir 1), Chrome's way: at
// the edge of its group it first leaves the group, next to a group it first
// joins it, and it jumps over a collapsed group
void StripGroupMoveTab(MainWindow* win, int dir) {
    int nTabs = win->TabCount();
    WindowTab* tab = win->CurrentTab();
    int idx = win->GetTabIdx(tab);
    int nb = idx + dir;
    if (idx < 0 || nb < 0 || nb >= nTabs) {
        return;
    }
    WindowTab* other = win->GetTab(nb);
    if (tab->IsPinnedHomeTab() || other->IsPinnedHomeTab()) {
        return;
    }
    int id = tab->stripGroupId;
    int otherId = other->stripGroupId;
    if (id != 0 && otherId != id) {
        MoveToGroup(win, tab, 0);
        return;
    }
    Vec<WindowTab*> tabs = win->Tabs();
    StripGroup* otherGroup = StripGroupFind(win, otherId);
    if (id == 0 && otherGroup) {
        if (StripGroupCanHold(tab) && !otherGroup->collapsed) {
            MoveToGroup(win, tab, otherId);
            return;
        }
        // over the whole group
        VecRemoveAt(tabs, idx);
        int first = -1;
        int last = -1;
        for (int i = 0; i < len(tabs); i++) {
            if (tabs[i]->stripGroupId == otherId) {
                first = first < 0 ? i : first;
                last = i;
            }
        }
        VecInsertAt(tabs, dir > 0 ? last + 1 : first, tab);
    } else {
        tabs[idx] = other;
        tabs[nb] = tab;
    }
    TabsSetOrder(win, tabs);
    ScheduleSaveSettings();
}

// a tab can't be selected inside a collapsed group
void StripGroupOnSelected(MainWindow* win, WindowTab* tab) {
    StripGroup* g = StripGroupOf(win, tab);
    if (!g || !g->collapsed) {
        return;
    }
    g->collapsed = false;
    TabsStripGroupsChanged(win);
}

// a tab moved in from another window brings its group id, which means nothing here
void StripGroupOnTabAdded(MainWindow* win, WindowTab* tab) {
    if (tab && !StripGroupFind(win, tab->stripGroupId)) {
        tab->stripGroupId = 0;
    }
}

void StripGroupSetFlags(AppCommandCtx& ctx) {
    MainWindow* win = ctx.win;
    WindowTab* tab = ctx.tab;
    if (!win || !tab) {
        return;
    }
    ctx.tabCanGroup = StripGroupCanHold(tab) && SettingsUseTabs();
    ctx.tabInGroup = StripGroupOf(win, tab) != nullptr;
    ctx.nOtherStripGroups = len(win->stripGroups) - (ctx.tabInGroup ? 1 : 0);
}

// a command with arguments, made once and found again by its definition
static int CmdIdForDefinition(Str definition) {
    CustomCommand* cmd = CreateCommandFromDefinition(definition);
    return cmd ? cmd->id : 0;
}

static TempStr GroupMenuTitleTemp(StripGroup* g, int n) {
    if (len(g->name) > 0) {
        return str::DupTemp(g->name);
    }
    return fmt(Tr("Unnamed group %d").s, n);
}

// "Add Tab to Group": one item per group the tab isn't in
void StripGroupAddToItems(MainWindow* win, WindowTab* tab, Vec<StripGroupMenuItem>& items) {
    int n = len(win->stripGroups);
    for (int i = 0; i < n; i++) {
        StripGroup* g = win->stripGroups[i];
        if (tab && g->id == tab->stripGroupId) {
            continue;
        }
        StripGroupMenuItem it;
        it.title = GroupMenuTitleTemp(g, i + 1);
        it.cmdId = CmdIdForDefinition(fmt("CmdStripGroupAddTab %d", i + 1));
        VecAppend(items, it);
    }
}

// "Group Color": the palette, the group's color checked
void StripGroupColorItems(MainWindow* win, WindowTab* tab, Vec<StripGroupMenuItem>& items) {
    StripGroup* g = StripGroupOf(win, tab);
    for (int i = 0; i < gStripGroupColorsCount; i++) {
        const StripGroupColor& c = gStripGroupColors[i];
        StripGroupMenuItem it;
        it.title = Tr(c.name);
        it.cmdId = CmdIdForDefinition(fmt("CmdStripGroupColor %s", SerializeColorTemp(c.color)));
        it.checked = g && g->color == c.color;
        VecAppend(items, it);
    }
}

// TabState.Group is the group's 1-based position in the window
void StripGroupSaveTab(MainWindow* win, WindowTab* tab, TabState* ts) {
    ts->group = 0;
    str::FreePtr(&ts->groupName);
    str::FreePtr(&ts->groupColor.s);
    ts->groupColor.wasParsed = false;
    ts->groupCollapsed = false;
    StripGroup* g = StripGroupOf(win, tab);
    if (!g) {
        return;
    }
    ts->group = VecFind(win->stripGroups, g) + 1;
    ts->groupName = str::Dup(g->name);
    ts->groupColor.s = str::Dup(SerializeColorTemp(g->color));
    ts->groupCollapsed = g->collapsed;
}

// `tab` was just restored from `ts`; tabs of one saved group join one group
void StripGroupRestoreTab(MainWindow* win, WindowTab* tab, TabState* ts, StripGroupRestore& rs) {
    if (!tab || ts->group <= 0 || !StripGroupCanHold(tab)) {
        return;
    }
    int i = VecFind(rs.keys, ts->group);
    if (i >= 0) {
        tab->stripGroupId = rs.ids[i];
        return;
    }
    Color col = GetParsedColor(ts->groupColor, gStripGroupColors[0].color);
    StripGroup* g = NewGroup(win, ts->groupName, col);
    VecAppend(rs.keys, ts->group);
    VecAppend(rs.ids, g->id);
    if (ts->groupCollapsed) {
        VecAppend(rs.collapsedIds, g->id);
    }
    tab->stripGroupId = g->id;
}

// after the tab to show was selected: collapse what was collapsed, except
// the group of that tab
void StripGroupRestoreDone(MainWindow* win, StripGroupRestore& rs) {
    if (len(rs.ids) == 0) {
        return;
    }
    StripGroupsFix(win);
    WindowTab* cur = win->CurrentTab();
    for (int id : rs.collapsedIds) {
        StripGroup* g = StripGroupFind(win, id);
        if (g && !(cur && cur->stripGroupId == id)) {
            g->collapsed = true;
        }
    }
    TabsStripGroupsChanged(win);
}

// for -dbg-control: the groups, then one line per tab
//   group 1 name=Work color=#1a73e8 collapsed=0
//   tab 2 name=a.pdf group=1 hidden=0 current=1
TempStr StripGroupsStateTemp(MainWindow* win) {
    str::Builder out;
    int nGroups = len(win->stripGroups);
    for (int i = 0; i < nGroups; i++) {
        StripGroup* g = win->stripGroups[i];
        out.Append(fmt("group %d name=%s color=%s collapsed=%d\n", i + 1, g->name, SerializeColorTemp(g->color),
                       (int)g->collapsed));
    }
    WindowTab* cur = win->CurrentTab();
    int nTabs = win->TabCount();
    for (int i = 0; i < nTabs; i++) {
        WindowTab* tab = win->GetTab(i);
        StripGroup* g = StripGroupOf(win, tab);
        int groupPos = g ? VecFind(win->stripGroups, g) + 1 : 0;
        Str name = tab->IsAboutTab() ? StrL("Home") : path::GetBaseNameTemp(tab->filePath);
        out.Append(fmt("tab %d name=%s group=%d hidden=%d current=%d\n", i, name, groupPos,
                       (int)StripGroupIsHidden(win, tab), (int)(tab == cur)));
    }
    return ToStrTemp(out);
}
