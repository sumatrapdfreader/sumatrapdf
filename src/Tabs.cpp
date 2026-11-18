/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/File.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"
#include "gui/win/TabsCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "SumatraPDF.h"
#include "DocumentProperties.h"
#include "MainWindow.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "Commands.h"
#include "CommandAvailability.h"
#include "FindBar.h"
#include "PagePosition.h"
#include "ReadingAutoScroll.h"
#include "ReadingBar.h"
#include "SelectionToolbar.h"
#include "Menu.h"
#include "TableOfContents.h"
#include "FileHistory.h"
#include "Theme.h"
#include "Translations.h"
#include "HomePage.h"
#include "Tabs.h"
#include "MenuDefs.h"
#include "TabsCommon.h"

void UpdateTabPageText(WindowTab* tab) {
    if (!tab || !tab->win || !tab->win->tabsCtrl) {
        return;
    }
    int idx = tab->win->GetTabIdx(tab);
    tab->win->tabsCtrl->SetPageText(idx, TabPageSuffixTemp(tab));
}

static void UpdateTabTitle(WindowTab* tab) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    int idx = win->GetTabIdx(tab);
    Str title = tab->GetTabTitle();
    bool dirty = false;
    if (tab->AsFixed()) {
        dirty = EngineHasUnsavedAnnotations(tab->AsFixed()->GetEngine());
    }
    TempStr tooltip = MakeTabTooltipTemp(tab->filePath, dirty);
    // a "+" Home tab a document was opened in is sized like the other documents
    TabInfo* ti = win->tabsCtrl->GetTab(idx);
    if (ti) {
        ti->fitToLabel = tab->IsAboutTab();
    }
    win->tabsCtrl->SetTextAndTooltip(idx, title, tooltip);
    UpdateTabPageText(tab);
}

int GetTabbarHeight(HWND hwnd, float factor) {
    DpiSetFromHwnd(hwnd);
    PlatformFont* font = GetAppFont();
    int tabDy = DpiScale(kTabBarDy);
    int fontDyWithPadding = PlatformFontLineHeight(font) + DpiScale(2);
    tabDy = std::max(fontDyWithPadding, tabDy);
    return (int)((float)tabDy * factor);
}

static void ShowTabBar(MainWindow* win, bool show) {
    if (show == win->tabsVisible) {
        return;
    }
    win->tabsVisible = show;
    if (win->tabsCtrl) {
        win->tabsCtrl->SetIsVisible(show);
    }
    ScheduleUiUpdate(win);
}

// also shows/hides the tabbar when necessary
void UpdateTabWidth(MainWindow* win) {
    int nTabs = win->TabCount();
    // Hide a lone Home tab. Keeping its tab bar open after the last document
    // closed fought RelayoutCaption and caused a continuous repaint storm
    // (issue #5861). Favorites is a closable tab, so it must remain visible
    // even when it is the only tab left.
    bool onlyHomeTab = nTabs == 1 && win->GetTab(0)->IsAboutTab();
    bool showSingleTab = SettingsUseTabs() || win->tabsInTitlebar;
    bool showTabs = !onlyHomeTab && !win->isQuickLook && ((nTabs > 1) || (showSingleTab && (nTabs > 0)));
    // TabWidth is stored in logical (96-DPI) units, same as other layout
    // settings; convert to physical pixels so HiDPI monitors honor the value
    // (issue #3850). Height already uses DpiScale via GetTabbarHeight.
    if (win->tabsCtrl) {
        HWND hwnd = win->tabsCtrl->hwnd ? win->tabsCtrl->hwnd : win->hwndFrame;
        win->tabsCtrl->tabDefaultDx = DpiScale(gSettings->tabWidth);
        win->tabsCtrl->equalWidth = gSettings->tabsEqualWidth;
    }
    // Lay out only when the bar stays visible. Hiding it right after
    // TabCtrl_SetItemSize invalidated the control leaves a pending WM_PAINT for
    // a window we're about to hide (issue #5861). It's laid out when shown again.
    if (!showTabs) {
        ShowTabBar(win, false);
        return;
    }
    ShowTabBar(win, true);
    if (win->tabsCtrl) {
        win->tabsCtrl->LayoutTabs();
    }
}

// posted: the click comes from the button that adding a tab rebuilds
static void OnTabsPlusClick(MainWindow* win) {
    HwndPostCommand(win->hwndFrame, CmdNewTab);
}

void RemoveTab(WindowTab* tab) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    if (!win || !win->tabsCtrl) {
        return;
    }
    int idx = win->GetTabIdx(tab);
    if (idx < 0) {
        // nested close already took this tab out of the strip (DDE CloseAllTabs)
        return;
    }
    UpdateTabFileDisplayStateForTab(tab);
    VecRemove(*win->tabSelectionHistory, tab);
    // ask before removing: afterwards CurrentTab() follows the strip's new
    // selection unless currentTabTemp happens to be set
    bool closedCurrentTab = IsCurrentTab(win, tab);
    WindowTab* tab2 = win->tabsCtrl->RemoveTab<WindowTab*>(idx);
    ReportIf(tab != tab2);
    if (closedCurrentTab) {
        win->ctrl = nullptr;
        win->currentTabTemp = nullptr;
    }
    // Hide the bar before selecting Home so we don't flash Home's close button
    // when the last document closes (formerly a special case; now UpdateTabWidth
    // itself treats Home-only as "no tabs").
    UpdateTabWidth(win);

    int nTabs = win->TabCount();
    if (nTabs < 1) {
        return;
    }
    if (!closedCurrentTab) {
        return;
    }
    // if the removed tab was the current one, select another
    // select tab to the right or to the left if nothing to the right
    int newIdx = idx;
    int lastIdx = nTabs - 1;
    newIdx = std::min(newIdx, lastIdx);
    win->tabsCtrl->SetSelected(newIdx);
    tab = win->CurrentTab();
    // seen in crash report that tab was WindowTab::Type::None
    // TODO: don't know how it could have happened
    if (tab && tab->type != WindowTab::Type::None) {
        LoadModelIntoTab(tab);
    }
}

static void MaybeMigrateTab(WindowTab* tab, MainWindow* newWin, Point releasePt) {
    MainWindow* oldWin = tab->win;

    // Home / Favorites tabs stay in their window
    if (tab->IsNonDocumentTab()) {
        return;
    }

    // don't migrate if it's only one document tab and not
    // dragging over a window
    int nDocTabs = 0;
    for (auto& t : oldWin->Tabs()) {
        if (t->IsNonDocumentTab()) continue;
        nDocTabs++;
    }
    if (nDocTabs == 1 && !newWin) return;

    // migration reloads by path; a document opened from memory (an embedded
    // attachment) has none
    if (len(tab->filePath) == 0) {
        return;
    }

    auto* engine = tab->GetEngine();
    if (EngineHasUnsavedAnnotations(engine)) {
        return;
    }

    RemoveTab(tab);

    if (!newWin) {
        if (IsZoomed(oldWin->hwndFrame)) {
            // dragging a tab out of a maximized window: like Chrome, create a
            // normal (non-maximized) window with the size the source window
            // would have when restored, positioned at the cursor so it lands
            // on the new window's tab strip
            WINDOWPLACEMENT wp{};
            wp.length = sizeof(wp);
            GetWindowPlacement(oldWin->hwndFrame, &wp);
            int dx = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
            int dy = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
            int x = releasePt.x - DpiScale(100);
            int y = releasePt.y - (GetTabbarHeight(oldWin->hwndFrame) / 2);
            Rect rect = ShiftRectToWorkArea(Rect(x, y, dx, dy), oldWin->hwndFrame, true);
            newWin = CreateAndShowMainWindow(nullptr, false);
            if (!newWin) {
                return;
            }
            HwndMoveWindow(newWin->hwndFrame, &rect);
            ShowMainWindow(newWin, WIN_STATE_NORMAL);
        } else {
            newWin = CreateAndShowMainWindow(nullptr);
        }
        if (!newWin) {
            return;
        }
    }

    // TODO: we should be able to just slide the existing tab
    // into the new window, preserving its controller and
    // the entire state, but this crashes/renders badly etc.
    // More work needed. The code that should work but doesn't:
    // tab->win = newWin;
    // newWin->currentTabTemp = AddTabToWindow(newWin, tab);
    // newWin->ctrl = tab->ctrl;
    // UpdateUiForCurrentTab(newWin);
    // newWin->showSelection = tab->selectionOnPage != nullptr;
    // HwndSetFocus(newWin->hwndFrame);
    // newWin->RedrawAll(true);
    // TabsOnChangedDoc(newWin);
    WindowTab* newTab = new WindowTab(newWin);
    newTab->SetFilePath(tab->filePath);
    newTab->SetDisplayName(tab->displayName);
    newWin->currentTabTemp = AddTabToWindow(newWin, newTab);
    newWin->ctrl = nullptr;
    LoadArgs args(tab->filePath, newWin);
    args.SetDisplayName(tab->displayName);
    args.forceReuse = true;
    args.noSavePrefs = true;
    LoadDocument(&args);
    // newTab owns the file now: don't let ~WindowTab delete an open-cache copy
    str::FreePtr(&tab->filePath);
    delete tab;

    CloseWindowIfNoDocuments(oldWin);
}

void MaybeMigrateTab(WindowTab* tab, MainWindow* newWin) {
    MaybeMigrateTab(tab, newWin, Point{});
}

// Selects the given tab (0-based index)
// tabIndex can come from settings file so must be sanitized
void TabsSelect(MainWindow* win, int tabIndex) {
    auto tabs = win->Tabs();
    int nTabs = len(tabs);
    logf("TabsSelect: tabIndex: %d, nTabs: %d\n", tabIndex, nTabs);
    if (nTabs == 0) {
        logf("TabsSelect: skipping because nTabs = %d\n", nTabs);
        return;
    }
    if (tabIndex < 0 || tabIndex >= nTabs) {
        tabIndex = 0;
        logf("TabsSelect: fixing tabIndex to 0\n");
    }
    TabsCtrl* tabsCtrl = win->tabsCtrl;
    int currIdx = tabsCtrl->GetSelected();
    if (tabIndex == currIdx) {
        return;
    }

    // same work as in onSelectionChanging and onSelectionChanged
    SaveCurrentWindowTab(win);
    int prevIdx = tabsCtrl->SetSelected(tabIndex);
    if (prevIdx < 0) {
        return;
    }
    WindowTab* tab = tabs[tabIndex];
    // page-info tip is restored via MainWindow::pageInfoWanted in LoadModelIntoTab
    LoadModelIntoTab(tab);
}

void CloseAllTabs(MainWindow* win) {
    if (!win) {
        return;
    }
    if (win->isBeingClosed || win->inCloseAllTabs) {
        logf("CloseAllTabs: skip isBeingClosed=%d inCloseAllTabs=%d\n", (int)win->isBeingClosed,
             (int)win->inCloseAllTabs);
        return;
    }
    win->inCloseAllTabs = true;
    defer {
        if (IsMainWindowValid(win)) {
            win->inCloseAllTabs = false;
        }
    };
    // can't close while iterating over the tabs so collect them first
    Vec<WindowTab*> toClose;
    CollectClosableTabs(win, toClose);
    CloseCollectedTabs(win, toClose);
}

//--- vertical tabs

bool UseVerticalTabs() {
    return gSettings && gSettings->verticalTabs && SettingsUseTabs();
}

// physical pixels; the setting is in logical (96 DPI) units like TabWidth
int VerticalTabsDx() {
    return DpiScale(limitValue(gSettings->verticalTabsWidth, 120, 600));
}

void ToggleVerticalTabs(MainWindow* win) {
    gSettings->verticalTabs = !gSettings->verticalTabs;
    for (MainWindow* w : gWindows) {
        if (w->tabsCtrl) {
            w->tabsCtrl->SetVertical(UseVerticalTabs());
        }
        ScheduleUiUpdate(w, kUiForceRelayout | kUiToolbarDirty | kUiTabsDirty);
    }
    ScheduleSaveSettings();
    (void)win;
}

//--- tab groups

struct TabGroupColor {
    const char* name;
    Color color;
};

// Chrome's palette
static const TabGroupColor kTabGroupColors[] = {
    {"Blue", MkRgb(0x1a, 0x73, 0xe8)},   {"Red", MkRgb(0xd9, 0x30, 0x25)},    {"Yellow", MkRgb(0xf2, 0xa6, 0x00)},
    {"Green", MkRgb(0x18, 0x80, 0x38)},  {"Pink", MkRgb(0xd0, 0x18, 0x84)},   {"Purple", MkRgb(0x8e, 0x44, 0xd8)},
    {"Cyan", MkRgb(0x00, 0x8b, 0x9a)},   {"Orange", MkRgb(0xe8, 0x71, 0x0a)}, {"Grey", MkRgb(0x5f, 0x63, 0x68)},
};
constexpr int kNTabGroupColors = dimofi(kTabGroupColors);

// the first color no group has, so that groups next to each other look different
static Color NextGroupColor(TabsCtrl* tabs) {
    for (int i = 0; i < kNTabGroupColors; i++) {
        bool used = false;
        for (TabGroupInfo* g : tabs->groups) {
            if (g->color == kTabGroupColors[i].color) {
                used = true;
                break;
            }
        }
        if (!used) {
            return kTabGroupColors[i].color;
        }
    }
    return kTabGroupColors[len(tabs->groups) % kNTabGroupColors].color;
}

// group names are what a restored session finds its groups by, so they are unique
static TempStr UniqueGroupNameTemp(TabsCtrl* tabs, Str wanted, int exceptGroupId) {
    TempStr name = str::DupTemp(wanted);
    for (int n = 2; n < 1000; n++) {
        TabGroupInfo* g = tabs->FindGroupByName(name);
        if (!g || g->id == exceptGroupId) {
            return name;
        }
        name = fmt("%s %d", wanted, n);
    }
    return name;
}

static TempStr NewGroupNameTemp(TabsCtrl* tabs) {
    for (int n = 1; n < 1000; n++) {
        TempStr name = fmt("Group %d", n);
        if (!tabs->FindGroupByName(name)) {
            return name;
        }
    }
    return str::DupTemp(StrL("Group"));
}

void TabGroupNewFromTab(MainWindow* win, WindowTab* tab) {
    if (!win || !win->tabsCtrl || !tab || tab->IsAboutTab()) {
        return;
    }
    TabsCtrl* tabs = win->tabsCtrl;
    int idx = win->GetTabIdx(tab);
    if (idx < 0) {
        return;
    }
    int gid = tabs->NewGroup(NewGroupNameTemp(tabs), NextGroupColor(tabs));
    tabs->SetTabGroup(idx, gid);
    // nothing joined (a tab that can't be grouped): don't keep an empty group
    tabs->FixGroups();
    tabs->RebuildTabCtrls();
    tabs->LayoutTabs();
    ScheduleSaveSettings();
}

void TabGroupRemoveTab(MainWindow* win, WindowTab* tab) {
    if (!win || !win->tabsCtrl || !tab) {
        return;
    }
    win->tabsCtrl->SetTabGroup(win->GetTabIdx(tab), 0);
    ScheduleSaveSettings();
}

void TabGroupToggleCollapsed(MainWindow* win, WindowTab* tab) {
    if (!win || !win->tabsCtrl || !tab) {
        return;
    }
    TabGroupInfo* g = win->tabsCtrl->GroupOfTab(win->GetTabIdx(tab));
    if (g) {
        win->tabsCtrl->ToggleGroupCollapsed(g->id);
    }
}

//--- a minimal modal "type a name" window

struct PromptState {
    HWND hwndEdit = nullptr;
    bool done = false;
    bool ok = false;
};

constexpr const WCHAR* kPromptClassName = L"SumatraTabGroupPrompt";
constexpr int kPromptEditId = 100;

static LRESULT CALLBACK PromptWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PromptState* st = (PromptState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lp;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            break;
        }
        case WM_COMMAND:
            if (!st) {
                break;
            }
            if (LOWORD(wp) == IDOK) {
                st->ok = true;
                st->done = true;
            } else if (LOWORD(wp) == IDCANCEL) {
                st->done = true;
            }
            return 0;
        case WM_CLOSE:
            if (st) {
                st->done = true;
            }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// returns the text the user typed, or {} when they cancelled
static TempStr PromptForTextTemp(HWND hwndParent, Str title, Str initial) {
    static ATOM atom = 0;
    if (!atom) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = PromptWndProc;
        wc.hInstance = GetInstance();
        wc.hCursor = GetCachedCursor(IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = kPromptClassName;
        atom = RegisterClassExW(&wc);
    }
    PromptState st;
    int dx = DpiScale(340);
    int dy = DpiScale(118);
    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, kPromptClassName, CWStrTemp(title), style, CW_USEDEFAULT,
                                CW_USEDEFAULT, dx, dy, hwndParent, nullptr, GetInstance(), &st);
    if (!hwnd) {
        return {};
    }
    HFONT font = GetAppFont() ? GetAppFont()->GetHFont() : nullptr;
    int pad = DpiScale(12);
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int cw = rcClient.right - rcClient.left;
    int editDy = DpiScale(24);
    int btnDx = DpiScale(80);
    int btnDy = DpiScale(26);
    st.hwndEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", CWStrTemp(initial),
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, pad, pad, cw - 2 * pad, editDy,
                                  hwnd, (HMENU)(INT_PTR)kPromptEditId, GetInstance(), nullptr);
    int btnY = pad + editDy + pad;
    HWND ok = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                              cw - 2 * btnDx - pad - DpiScale(8), btnY, btnDx, btnDy, hwnd, (HMENU)(INT_PTR)IDOK,
                              GetInstance(), nullptr);
    HWND cancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                  cw - btnDx - pad, btnY, btnDx, btnDy, hwnd, (HMENU)(INT_PTR)IDCANCEL, GetInstance(),
                                  nullptr);
    if (font) {
        HwndSetFont(st.hwndEdit, font);
        HwndSetFont(ok, font);
        HwndSetFont(cancel, font);
    }
    HwndCenterDialog(hwnd, hwndParent);
    if (hwndParent) {
        EnableWindow(hwndParent, FALSE);
    }
    ShowWindow(hwnd, SW_SHOW);
    SetFocus(st.hwndEdit);
    SendMessageW(st.hwndEdit, EM_SETSEL, 0, -1);

    MSG m;
    while (!st.done && GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (m.message == WM_KEYDOWN && (m.wParam == VK_RETURN || m.wParam == VK_ESCAPE) &&
            (m.hwnd == st.hwndEdit || m.hwnd == hwnd)) {
            st.ok = m.wParam == VK_RETURN;
            st.done = true;
            break;
        }
        if (IsDialogMessageW(hwnd, &m)) {
            continue;
        }
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    TempStr res = {};
    if (st.ok) {
        res = str::DupTemp(HwndGetTextTemp(st.hwndEdit));
    }
    if (hwndParent) {
        EnableWindow(hwndParent, TRUE);
    }
    DestroyWindow(hwnd);
    if (hwndParent) {
        SetForegroundWindow(hwndParent);
    }
    return res;
}

static void TabGroupRename(MainWindow* win, int groupId) {
    TabsCtrl* tabs = win->tabsCtrl;
    TabGroupInfo* g = tabs->FindGroup(groupId);
    if (!g) {
        return;
    }
    TempStr cur = str::DupTemp(g->name);
    TempStr name = PromptForTextTemp(win->hwndFrame, StrL("Rename Tab Group"), cur);
    if (!IsMainWindowValidAndNotClosing(win) || !win->tabsCtrl->FindGroup(groupId)) {
        return;
    }
    // trim, and a group needs a name to be found again when a session is restored
    name = str::DupTemp(name);
    while (len(name) > 0 && (name.s[0] == ' ' || name.s[0] == '\t')) {
        name = Str(name.s + 1, name.len - 1);
    }
    while (len(name) > 0 && (name.s[len(name) - 1] == ' ' || name.s[len(name) - 1] == '\t')) {
        name = Str(name.s, name.len - 1);
    }
    if (len(name) == 0) {
        return;
    }
    name = UniqueGroupNameTemp(tabs, name, groupId);
    tabs->SetGroupNameAndColor(groupId, name, g->color);
    ScheduleSaveSettings();
}

void TabGroupRenameOfTab(MainWindow* win, WindowTab* tab) {
    if (!win || !win->tabsCtrl || !tab) {
        return;
    }
    TabGroupInfo* g = win->tabsCtrl->GroupOfTab(win->GetTabIdx(tab));
    if (g) {
        TabGroupRename(win, g->id);
    }
}

// a tab and its group in the right-click menus
enum {
    kMenuGrpNew = 41000,
    kMenuGrpRemove,
    kMenuGrpRename,
    kMenuGrpCollapse,
    kMenuGrpUngroup,
    kMenuGrpCloseAll,
    kMenuVerticalTabs,
    kMenuGrpAddBase = 41100,   // + index in TabsCtrl::groups
    kMenuGrpColorBase = 41300, // + index in kTabGroupColors
};

static void AppendGroupMenuItems(HMENU popup, TabsCtrl* tabs, bool forTab, TabGroupInfo* group) {
    AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
    if (forTab) {
        AppendMenuW(popup, MF_STRING, kMenuGrpNew, L"Add Tab to New Group");
        HMENU sub = CreatePopupMenu();
        int nGroups = len(tabs->groups);
        int nAdded = 0;
        for (int i = 0; i < nGroups; i++) {
            TabGroupInfo* g = tabs->groups[i];
            if (g == group) {
                continue;
            }
            AppendMenuW(sub, MF_STRING, kMenuGrpAddBase + i, CWStrTemp(g->name));
            nAdded++;
        }
        if (nAdded > 0) {
            AppendMenuW(popup, MF_POPUP, (UINT_PTR)sub, L"Add Tab to Group");
        } else {
            DestroyMenu(sub);
        }
        if (group) {
            AppendMenuW(popup, MF_STRING, kMenuGrpRemove, L"Remove from Group");
        }
    }
    if (group) {
        AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(popup, MF_STRING, kMenuGrpRename, L"Rename Group...");
        AppendMenuW(popup, MF_STRING, kMenuGrpCollapse, group->collapsed ? L"Expand Group" : L"Collapse Group");
        HMENU colors = CreatePopupMenu();
        for (int i = 0; i < kNTabGroupColors; i++) {
            UINT flags = MF_STRING | (kTabGroupColors[i].color == group->color ? MF_CHECKED : 0);
            AppendMenuW(colors, flags, kMenuGrpColorBase + i, CWStrTemp(Str(kTabGroupColors[i].name)));
        }
        AppendMenuW(popup, MF_POPUP, (UINT_PTR)colors, L"Group Color");
        AppendMenuW(popup, MF_STRING, kMenuGrpUngroup, L"Ungroup");
        AppendMenuW(popup, MF_STRING, kMenuGrpCloseAll, L"Close Group");
    }
}

static void AppendVerticalTabsItem(HMENU popup) {
    AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
    UINT flags = MF_STRING | (UseVerticalTabs() ? MF_CHECKED : 0);
    AppendMenuW(popup, flags, kMenuVerticalTabs, L"Vertical Tabs");
}

// Runs the group / vertical tabs commands of the menus built above.
// `tab` is the tab that was right-clicked (null for a group's chip or the bar)
static bool HandleGroupMenuCmd(MainWindow* win, WindowTab* tab, int groupId, int cmdId) {
    TabsCtrl* tabs = win->tabsCtrl;
    if (cmdId == kMenuVerticalTabs) {
        ToggleVerticalTabs(win);
        return true;
    }
    if (cmdId >= kMenuGrpAddBase && cmdId < kMenuGrpAddBase + 200) {
        int i = cmdId - kMenuGrpAddBase;
        if (tab && i < len(tabs->groups)) {
            tabs->SetTabGroup(win->GetTabIdx(tab), tabs->groups[i]->id);
            ScheduleSaveSettings();
        }
        return true;
    }
    if (cmdId >= kMenuGrpColorBase && cmdId < kMenuGrpColorBase + kNTabGroupColors) {
        TabGroupInfo* g = tabs->FindGroup(groupId);
        if (g) {
            tabs->SetGroupNameAndColor(groupId, g->name, kTabGroupColors[cmdId - kMenuGrpColorBase].color);
            ScheduleSaveSettings();
        }
        return true;
    }
    switch (cmdId) {
        case kMenuGrpNew:
            TabGroupNewFromTab(win, tab);
            return true;
        case kMenuGrpRemove:
            TabGroupRemoveTab(win, tab);
            return true;
        case kMenuGrpRename:
            TabGroupRename(win, groupId);
            return true;
        case kMenuGrpCollapse:
            tabs->ToggleGroupCollapsed(groupId);
            return true;
        case kMenuGrpUngroup:
            tabs->RemoveGroup(groupId);
            ScheduleSaveSettings();
            return true;
        case kMenuGrpCloseAll: {
            Vec<WindowTab*> toClose;
            for (int i = 0; i < tabs->TabCount(); i++) {
                if (tabs->GetTab(i)->groupId == groupId) {
                    VecAppend(toClose, win->GetTab(i));
                }
            }
            CloseCollectedTabs(win, toClose);
            return true;
        }
    }
    return false;
}

// right-click on a group's chip
static void GroupHeaderContextMenu(MainWindow* win, TabsCtrl* tabs, int groupId, VirtMouseEvent* ev) {
    TabGroupInfo* g = tabs->FindGroup(groupId);
    if (!g) {
        return;
    }
    HMENU popup = CreatePopupMenu();
    AppendGroupMenuItems(popup, tabs, false, g);
    AppendVerticalTabsItem(popup);
    Point pt = HwndClientToScreen(tabs->hwnd, ev->ptWindow);
    ev->didHandle = true;
    int cmdId = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    DestroyMenu(popup);
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    HandleGroupMenuCmd(win, nullptr, groupId, cmdId);
}

// right-click on the strip, not on a tab
static void StripContextMenu(MainWindow* win, TabsCtrl* tabs, VirtMouseEvent* ev) {
    HMENU popup = CreatePopupMenu();
    UINT flags = MF_STRING | (UseVerticalTabs() ? MF_CHECKED : 0);
    AppendMenuW(popup, flags, kMenuVerticalTabs, L"Vertical Tabs");
    Point pt = HwndClientToScreen(tabs->hwnd, ev->ptWindow);
    ev->didHandle = true;
    int cmdId = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    DestroyMenu(popup);
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    HandleGroupMenuCmd(win, nullptr, 0, cmdId);
}

//--- groups in saved sessions

// the group of the tab goes into the session, so that it comes back
void TabGroupSaveToState(WindowTab* tab, TabState* ts) {
    str::Free(ts->groupName);
    str::Free(ts->groupColor);
    ts->groupName = {};
    ts->groupColor = {};
    ts->groupCollapsed = false;
    MainWindow* win = tab ? tab->win : nullptr;
    if (!win || !win->tabsCtrl) {
        return;
    }
    TabGroupInfo* g = win->tabsCtrl->GroupOfTab(win->GetTabIdx(tab));
    if (!g || len(g->name) == 0) {
        return;
    }
    ts->groupName = str::Dup(g->name);
    ts->groupColor = str::Dup(SerializeColorTemp(g->color));
    ts->groupCollapsed = g->collapsed;
}

// Recreates the groups of a restored window. The tabs are in the order of the
// session's TabStates (some may be missing: a file that is gone)
void TabGroupsRestore(MainWindow* win, SessionData* data) {
    if (!win || !win->tabsCtrl || !data) {
        return;
    }
    bool any = false;
    for (TabState* st : *data->tabStates) {
        if (len(st->groupName) > 0) {
            any = true;
            break;
        }
    }
    if (!any) {
        return;
    }
    TabsCtrl* tabs = win->tabsCtrl;
    Vec<WindowTab*> wtabs = win->Tabs();
    int nTabs = len(wtabs);
    int next = 0;
    Vec<int> collapsed;
    for (TabState* st : *data->tabStates) {
        if (len(st->filePath) == 0) {
            continue;
        }
        WindowTab* tab = nullptr;
        for (int k = next; k < nTabs; k++) {
            WindowTab* t = wtabs[k];
            if (t->IsAboutTab() || len(t->filePath) == 0) {
                continue;
            }
            if (str::EqI(t->filePath, st->filePath)) {
                tab = t;
                next = k + 1;
                break;
            }
        }
        if (!tab || len(st->groupName) == 0) {
            continue;
        }
        TabGroupInfo* g = tabs->FindGroupByName(st->groupName);
        int gid = g ? g->id : 0;
        if (!g) {
            Color col = kTabGroupColors[0].color;
            ParseColor(&col, st->groupColor);
            gid = tabs->NewGroup(st->groupName, col);
        }
        tabs->SetTabGroup(win->GetTabIdx(tab), gid);
        if (st->groupCollapsed) {
            VecAppend(collapsed, gid);
        }
    }
    // the group of the selected tab stays open
    TabGroupInfo* selectedGroup = tabs->GroupOfTab(tabs->GetSelected());
    for (int gid : collapsed) {
        if (!selectedGroup || selectedGroup->id != gid) {
            tabs->SetGroupCollapsed(gid, true);
        }
    }
}

// TODO: add "Move to another window" sub-menu
static void TabsContextMenu(TabsCtrl* tabsCtrl, VirtMouseEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(tabsCtrl->hwnd);
    if (!win) {
        return;
    }
    TabsCtrl::MouseState tabState = tabsCtrl->TabStateFromMousePosition(ev->ptWindow);
    int tabIdx = tabState.tabIdx;
    if (tabState.headerGroupId != 0) {
        GroupHeaderContextMenu(win, tabsCtrl, tabState.headerGroupId, ev);
        return;
    }
    if (tabIdx < 0) {
        StripContextMenu(win, tabsCtrl, ev);
        return;
    }

    WindowTab* tabUnderMouse = win->Tabs()[tabIdx];
    if (tabUnderMouse->IsAboutTab()) {
        return;
    }
    Point pt = HwndClientToScreen(tabsCtrl->hwnd, ev->ptWindow);

    Vec<WindowTab*> toCloseOther;
    Vec<WindowTab*> toCloseRight;
    Vec<WindowTab*> toCloseLeft;
    CollectTabsToClose(win, tabUnderMouse, toCloseOther, toCloseRight, toCloseLeft);

    DisplayModel* dmTab = tabUnderMouse->AsFixed();
    EngineBase* tabEngine = dmTab ? dmTab->GetEngine() : nullptr;

    // Build the command context for the tab under the mouse, which may differ
    // from the current tab that NewAppCommandCtx() keys off. Without a context
    // command availability is evaluated against an empty (no-document) state,
    // which removes almost every item (leaving only "Restore Tab Group").
    BuildMenuCtx* ctx = NewBuildMenuCtx(tabUnderMouse, Point{0, 0});
    ctx->tab = tabUnderMouse;
    ctx->isDocLoaded = true; // tabUnderMouse is a real (non-about) document tab
    ctx->filePath = tabUnderMouse->filePath;
    ctx->supportsAnnots = EngineSupportsAnnotations(tabEngine);
    ctx->hasUnsavedAnnotations = EngineHasUnsavedAnnotations(tabEngine);
    ctx->canCloseOtherTabs = len(toCloseOther) > 0;
    ctx->canCloseTabsToRight = len(toCloseRight) > 0;
    ctx->canCloseTabsToLeft = len(toCloseLeft) > 0;

    HMENU popup = BuildMenuFromDef(menuDefContextTab, CreatePopupMenu(), ctx);
    DeleteBuildMenuCtx(ctx);

    if (!tabUnderMouse->ctrl) {
        MenuSetEnabled(popup, CmdSetTabColor, false);
    }
    // the save/discard items only make sense when the document has unsaved
    // changes (e.g. filled form fields, added annotations); otherwise remove
    // them, then clean up the separator they leave behind
    if (!EngineHasUnsavedAnnotations(tabEngine)) {
        DeleteMenu(popup, CmdSaveAnnotations, MF_BYCOMMAND);
        DeleteMenu(popup, CmdSaveAnnotationsNewFile, MF_BYCOMMAND);
        DeleteMenu(popup, CmdDiscardChanges, MF_BYCOMMAND);
        RemoveBadMenuSeparators(popup);
    }
    TabGroupInfo* tabGroup = tabsCtrl->GroupOfTab(tabIdx);
    int tabGroupId = tabGroup ? tabGroup->id : 0;
    AppendGroupMenuItems(popup, tabsCtrl, true, tabGroup);
    AppendVerticalTabsItem(popup);
    // handled: the menu loop can close the window and free tabsCtrl, so the
    // event must not bubble up to it
    ev->didHandle = true;
    uint flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    int cmdId = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    DestroyMenu(popup);
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (HandleGroupMenuCmd(win, tabUnderMouse, tabGroupId, cmdId)) {
        return;
    }
    switch (cmdId) {
        case CmdClose: {
            CloseTab(tabUnderMouse, false);
            return;
        }

        case CmdCloseAllTabs: {
            CloseAllTabs(win);
            return;
        }
        case CmdCloseOtherTabs: {
            CloseCollectedTabs(win, toCloseOther);
            return;
        }
        case CmdCloseTabsToTheRight: {
            CloseCollectedTabs(win, toCloseRight);
            return;
        }
        case CmdCloseTabsToTheLeft: {
            CloseCollectedTabs(win, toCloseLeft);
            return;
        }
        case CmdShowInFolder: {
            ShowFileInFolder(win, tabUnderMouse->filePath);
            return;
        }
        case CmdCopyFilePath: {
            CopyFilePath(tabUnderMouse);
            return;
        }
        case CmdDuplicateInNewWindow: {
            DuplicateTabInNewWindow(tabUnderMouse);
            return;
        }
        case CmdDuplicateInNewTab: {
            DuplicateTabInNewTab(tabUnderMouse);
            return;
        }
        case CmdProperties: {
            ShowProperties(win->hwndFrame, tabUnderMouse->ctrl);
            return;
        }
        case CmdSetTabColor: {
            // handled in FrameOnCommand; forward the tab under the mouse encoded
            // in lp (the command palette invokes it on the current tab, lp == 0)
            HwndSendCommand(win->hwndFrame, CmdSetTabColor, (LPARAM)tabUnderMouse);
            return;
        }
        case CmdSaveAnnotations: {
            SaveAnnotationsToExistingFile(tabUnderMouse);
            return;
        }
        case CmdSaveAnnotationsNewFile: {
            SaveAnnotationsToMaybeNewPdfFile(tabUnderMouse);
            return;
        }
        case CmdDiscardChanges: {
            // revert to the on-disk version, discarding unsaved changes
            TabsSelect(win, tabIdx);
            ReloadDocument(win, false);
            return;
        }
    }
    // everything we forward to main window
    HwndSendCommand(win->hwndFrame, cmdId);
}

static void MainWindowTabClosed(MainWindow* win, TabsCtrl::ClosedEvent* ev) {
    int closedTabIdx = ev->tabIdx;
    WindowTab* tab = win->GetTab(closedTabIdx);
    CloseTab(tab, false);
}

static void MainWindowTabSelectionChanging(MainWindow* win, TabsCtrl::SelectionChangingEvent* ev) {
    // TODO: Should we allow the switch of the tab if we are in process of printing?
    SaveCurrentWindowTab(win);
    ev->preventChanging = false;
}

static void MainWindowTabSelectionChanged(MainWindow* win, TabsCtrl::SelectionChangedEvent* /*ev*/) {
    int currentIdx = win->tabsCtrl->GetSelected();
    WindowTab* tab = win->Tabs()[currentIdx];
    // page-info tip is restored via MainWindow::pageInfoWanted in LoadModelIntoTab
    LoadModelIntoTab(tab);
}

static void MainWindowTabMigration(MainWindow* win, TabsCtrl::MigrationEvent* ev) {
    WindowTab* tab = win->GetTab(ev->tabIdx);
    MainWindow* releaseWnd = nullptr;
    HWND hwnd = HwndWindowFromPoint(ev->releasePoint);
    if (hwnd != nullptr) {
        releaseWnd = FindMainWindowByHwnd(hwnd);
    }
    if (releaseWnd == win) {
        // don't re-add to the same window
        releaseWnd = nullptr;
    }
    MaybeMigrateTab(tab, releaseWnd, ev->releasePoint);
}

void CreateTabbar(MainWindow* win) {
    if (win->frameDpi > 0) {
        DpiSet(win->frameDpi, win->frameDpi);
    }
    TabsCtrl::CreateArgs args;
    args.parent = win->hwndFrame;
    args.withToolTips = true;
    args.font = GetAppFont();
    // logical TabWidth → physical (see UpdateTabWidth / issue #3850)
    args.tabDefaultDx = DpiScale(gSettings->tabWidth);
    args.equalWidth = gSettings->tabsEqualWidth;
    args.isRtl = false; // LTR hwnd; RTL tab order follows parent frame (see UpdateWindowRtlLayout)

    TabsCtrl* tabsCtrl = new TabsCtrl();
    tabsCtrl->onTabClosed = MkFunc1(MainWindowTabClosed, win);
    tabsCtrl->onSelectionChanging = MkFunc1(MainWindowTabSelectionChanging, win);
    tabsCtrl->onSelectionChanged = MkFunc1(MainWindowTabSelectionChanged, win);
    tabsCtrl->onContextMenu = MkFunc1(TabsContextMenu, tabsCtrl);
    tabsCtrl->onTabMigration = MkFunc1(MainWindowTabMigration, win);
    tabsCtrl->onPlusClick = MkFunc0<MainWindow>(OnTabsPlusClick, win);
    tabsCtrl->Create(args);
    win->tabsCtrl = tabsCtrl;
    win->tabSelectionHistory = new Vec<WindowTab*>();
}

// verifies that WindowTab state is consistent with MainWindow state
static NO_INLINE void VerifyWindowTab(MainWindow* win, WindowTab* tdata) {
    ReportIf(tdata->ctrl != win->ctrl);
    // Home / Favorites tabs have no document controller. Favorites layout
    // intentionally leaves canvas geometry alone (hidden), so canvasRc is not
    // kept in lockstep with win->canvasRc the way document tabs are.
    if (tdata->IsNonDocumentTab()) {
        return;
    }
    bool expectedTocVisibility = tdata->showToc; // if not in presentation mode
    if (win->InPresentation()) {
        expectedTocVisibility = false; // WindowMode::PresentationBlack, WindowMode::PresentationWhite
        if (win->windowMode == WindowMode::Presentation) {
            expectedTocVisibility = tdata->showTocPresentation;
        }
    }
    // Heading TOC is generated after the document is shown. Until that finishes
    // the sidebar stays hidden (uiState.sidebarTopVisible) but the tab keeps the
    // caller's showToc preference so we can open it when headings arrive.
    bool topVisible = win->uiState.sidebarTopVisible;
    if (topVisible != expectedTocVisibility) {
        bool headingPending = EngineMupdfHeadingTocPending(tdata->GetEngine());
        bool okPendingHide = headingPending && expectedTocVisibility && !topVisible;
        ReportDebugIf(!okPendingHide);
    }
    ReportIf(tdata->canvasRc != win->canvasRc);
}

// Must be called when the active tab is losing selection.
// This happens when a new document is loaded or when another tab is selected.
void SaveCurrentWindowTab(MainWindow* win) {
    if (!win) {
        return;
    }
    if (!win->tabsCtrl) {
        return;
    }
    // the find UI (compact bar or floating window) belongs to the previous tab's
    // search; close it when leaving the tab (HideFindBar also drops the cached
    // results so the next tab can't show or navigate into the old document's
    // matches)
    HideFindBar(win);
    HideSelectionToolbar(win);
    ReadingAutoScrollHideBar(win);
    ReadingBarCancelDrag(win);

    int current = win->tabsCtrl->GetSelected();
    if (-1 == current) {
        return;
    }
    if (!IsCurrentTab(win, win->Tabs()[current])) {
        return; // TODO: restore ReportIf() ?
    }

    WindowTab* tab = win->CurrentTab();
    if (win->tocLoaded && tab->ctrl) {
        TocTree* tocTree = tab->ctrl->GetToc();
        UpdateTocExpansionState(tab->tocState, win->tocTreeView, tocTree);
    }
    VerifyWindowTab(win, tab);

    // update the selection history
    VecRemove(*win->tabSelectionHistory, tab);
    VecAppend(*win->tabSelectionHistory, tab);
}

static WindowTab* InsertHomeTab(MainWindow* win, int idx, HomeTabKind kind, bool deferUpdate) {
    WindowTab* homeTab = NewHomeTab(win, kind);
    bool isPinned = kind == HomeTabKind::Pinned;
    TabInfo* newTab = new TabInfo();
    newTab->text = str::Dup(StrL("Home"));
    newTab->tooltip = {};
    newTab->isPinned = isPinned;
    newTab->canClose = true;
    // every Home tab is sized to its label; the extra ones keep room for a ✕ (#6469)
    newTab->fitToLabel = true;
    newTab->userData = (UINT_PTR)homeTab;
    int insertedIdx = win->tabsCtrl->InsertTab(idx, newTab, !deferUpdate);
    ReportIf(insertedIdx != idx);
    return homeTab;
}

// select the Home tab, creating it when NoHomeTab left the window without one
void GoToHomeTab(MainWindow* win) {
    if (!win || !win->tabsCtrl || !SettingsUseTabs()) {
        return;
    }
    WindowTab* homeTab = FindHomeTab(win);
    if (homeTab) {
        TabsSelect(win, win->GetTabIdx(homeTab));
        HomePageFocusSearch(win);
        return;
    }

    // InsertTab selects without TCN_SELCHANGE, so save / load the models here
    SaveCurrentWindowTab(win);
    homeTab = InsertHomeTab(win, 0, HomeTabKind::Pinned, false);
    UpdateTabWidth(win);
    LoadModelIntoTab(homeTab);
    HomePageFocusSearch(win);
}

// the "+" button: another Home tab after the last one. It stays out of the
// session, which only stores documents
void AddExtraHomeTab(MainWindow* win) {
    if (!win || !win->tabsCtrl || !SettingsUseTabs()) {
        return;
    }
    // the first Home tab is pinned, unless NoHomeTab: a file opened from it must
    // not leave a Home tab behind
    if (win->TabCount() == 0 && !gSettings->noHomeTab) {
        GoToHomeTab(win);
        return;
    }
    SaveCurrentWindowTab(win);
    WindowTab* homeTab = InsertHomeTab(win, win->TabCount(), HomeTabKind::Extra, false);
    UpdateTabWidth(win);
    LoadModelIntoTab(homeTab);
    HomePageFocusSearch(win);
}

WindowTab* AddTabToWindow(MainWindow* win, WindowTab* tab, bool deferUpdate) {
    ReportIf(!win);
    if (!win) {
        return nullptr;
    }
    if (!win->tabsCtrl) {
        return nullptr;
    }

    auto* tabs = win->tabsCtrl;
    int idx = win->TabCount();
    bool useTabs = SettingsUseTabs();
    bool noHomeTab = gSettings->noHomeTab;
    bool createHomeTab = useTabs && !noHomeTab && (idx == 0);
    if (createHomeTab) {
        InsertHomeTab(win, 0, HomeTabKind::Pinned, deferUpdate);
        idx++;
    }

    tab->canvasRc = win->canvasRc;
    TabInfo* newTab = new TabInfo();
    newTab->text = str::Dup(tab->GetTabTitle());
    newTab->pageText = str::Dup(TabPageSuffixTemp(tab));
    // same full path + size as UpdateTabTitle (was path-only, so size missing until
    // TabsOnChangedDoc for the active tab)
    newTab->tooltip = str::Dup(MakeTabTooltipTemp(tab->filePath, false));
    newTab->userData = (UINT_PTR)tab;
    newTab->tabColor = tab->tabColor;
    // a failed load can arrive in a tab created for it (ShowLoadErrorInTab)
    newTab->isError = tab->loadState == WindowTab::LoadState::Error;

    int insertedIdx = tabs->InsertTab(idx, newTab, !deferUpdate);
    ReportIf(insertedIdx == -1);
    if (!deferUpdate) {
        tabs->SetSelected(insertedIdx);
        UpdateTabWidth(win);
    }
    return tab;
}

// The tab control paints from TabInfo::tabColor, so WindowTab::tabColor has to
// be pushed into it whenever it changes. AddTabToWindow() does it at insert
// time, but a document's saved color is only known once it has loaded, well
// after that (issue #5884).
// copy tab->tabColor into the tab control's TabInfo (what it paints from)
void SetTabInfoColor(WindowTab* tab) {
    if (!tab || !tab->win || !tab->win->tabsCtrl) {
        return;
    }
    MainWindow* win = tab->win;
    TabInfo* ti = win->tabsCtrl->GetTab(win->GetTabIdx(tab));
    if (!ti || ti->tabColor == tab->tabColor) {
        return;
    }
    ti->tabColor = tab->tabColor;
    win->tabsCtrl->ScheduleRepaint();
}

// The tab control paints from TabInfo::isError, so it has to be re-synced from
// WindowTab::loadState when a load fails or a reload succeeds
void UpdateTabIsError(WindowTab* tab) {
    if (!tab || !tab->win || !tab->win->tabsCtrl) {
        return;
    }
    MainWindow* win = tab->win;
    TabInfo* ti = win->tabsCtrl->GetTab(win->GetTabIdx(tab));
    bool isError = tab->loadState == WindowTab::LoadState::Error;
    if (!ti || ti->isError == isError) {
        return;
    }
    ti->isError = isError;
    win->tabsCtrl->ScheduleRepaint();
}

// Refresh the tab's title
void TabsOnChangedDoc(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    ReportIf(!tab != !win->TabCount());
    if (!tab) {
        return;
    }

    int tabIdx = win->GetTabIdx(tab);
    int selectedIdx = win->tabsCtrl->GetSelected();
    if (tabIdx != selectedIdx) {
        logf("TabsonChangeDoc: tabIdx (%d) != selectedIdx (%d)\n", tabIdx, selectedIdx);
        ReportDebugIf(tabIdx != selectedIdx);
    }
    VerifyWindowTab(win, tab);
    UpdateTabTitle(tab);
}

// Called when we're closing an entire window (quitting)
void TabsOnCloseWindow(MainWindow* win) {
    // TODO: I've seen a crash here where it seems like we've deleted the only main window
    // but somehow we still process Esc and we get here. this might not be enough
    if (!win->tabsCtrl) {
        return;
    }
    // Clear these BEFORE destroying tabs. Deleting a Markdown/CHM tab destroys
    // its WebView2, which pumps messages; canvas WndProc then called AsFixed()
    // on win->ctrl after another tab's DisplayModel had already been freed.
    win->ctrl = nullptr;
    win->currentTabTemp = nullptr;
    auto tabs = win->Tabs();
    // drop TabsCtrl userData first so CurrentTab() is null while WindowTab
    // dtors run (they refresh the annotation filter via CurrentTab)
    win->tabsCtrl->RemoveAllTabs();
    DeleteVecMembers(tabs);
    VecReset(*win->tabSelectionHistory);
}

void SetTabsInTitlebar(MainWindow* win, bool inTitleBar) {
    if (inTitleBar == win->tabsInTitlebar) {
        return;
    }
    win->tabsInTitlebar = inTitleBar;
    win->tabsCtrl->inTitleBar = inTitleBar;
    if (inTitleBar) {
        RelayoutCaption(win);
    }
    uint flags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
    SetWindowPos(win->hwndFrame, nullptr, 0, 0, 0, 0, flags);
}

void MoveTab(MainWindow* win, int dir) {
    if (!win) {
        return;
    }
    int nTabs = win->TabCount();
    int idx = win->tabsCtrl->GetSelected();
    int newIdx = idx + dir;
    if (newIdx < 0) {
        return;
    }
    if (newIdx >= nTabs) {
        return;
    }
    if (win->GetTab(idx)->IsPinnedHomeTab() || win->GetTab(newIdx)->IsPinnedHomeTab()) {
        return;
    }
    TabsCtrl* tabsCtrl = win->tabsCtrl;
    TabInfo* moved = tabsCtrl->GetTab(idx);
    int otherGroupId = tabsCtrl->GetTab(newIdx)->groupId;
    tabsCtrl->SwapTabs(idx, newIdx);
    // a tab moved past the edge of a group joins it, or leaves its own
    if (!moved->fitToLabel) {
        moved->groupId = otherGroupId;
    }
    tabsCtrl->FixGroups();
    tabsCtrl->RebuildTabCtrls();
    tabsCtrl->SetSelected(VecFind(tabsCtrl->tabs, moved));
    tabsCtrl->LayoutTabs();
}
