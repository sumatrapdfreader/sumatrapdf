/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "resource.h"
#include "Commands.h"
#include "Caption.h"
#include "Menu.h"
#include "TableOfContents.h"
#include "Tabs.h"

#include "utils/Log.h"

static void SetTabTitle(WindowTab* tab) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    int idx = win->tabs.Find(tab);
    const char* title = tab->GetTabTitle();
    win->tabsCtrl->SetTabText(idx, title);
    auto tooltip = tab->filePath.Get();
    win->tabsCtrl->SetTooltip(idx, tooltip);
}

static void NO_INLINE SwapTabs(MainWindow* win, int tab1, int tab2) {
    if (tab1 == tab2 || tab1 < 0 || tab2 < 0) {
        return;
    }

    auto&& tabs = win->tabs;
    std::swap(tabs.at(tab1), tabs.at(tab2));
    SetTabTitle(tabs.at(tab1));
    SetTabTitle(tabs.at(tab2));

    int current = win->tabsCtrl->GetSelectedTabIndex();
    int newSelected = tab1;
    if (tab1 == current) {
        newSelected = tab2;
    }
    win->tabsCtrl->SetSelectedTabByIndex(newSelected);
}

static LRESULT CALLBACK TabBarProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, __unused UINT_PTR uIdSubclass,
                                   __unused DWORD_PTR dwRefData) {
    PAINTSTRUCT ps;
    HDC hdc;
    int index;
    LPTCITEM tcs;

    TabPainter* tab = (TabPainter*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    if (WM_NCDESTROY == msg) {
        RemoveWindowSubclass(hwnd, TabBarProc, 0);
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    TabsCtrl* tabs = tab->tabsCtrl;

    switch (msg) {
        case WM_DESTROY:
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)0);
            break;

        case TCM_INSERTITEM:
            index = (int)wp;
            if (index <= tab->selectedTabIdx) {
                tab->selectedTabIdx++;
            }
            tab->xClicked = -1;
            InvalidateRgn(hwnd, nullptr, FALSE);
            UpdateWindow(hwnd);
            break;

        case TCM_SETITEM:
            // TODO: this should not be necessary
            index = (int)wp;
            tcs = (LPTCITEM)lp;
            if (TCIF_TEXT & tcs->mask) {
                tab->Invalidate(index);
            }
            break;

        case TCM_DELETEITEM:
            // TODO: this should not be necessary
            index = (int)wp;
            if (index < tab->selectedTabIdx) {
                tab->selectedTabIdx--;
            } else if (index == tab->selectedTabIdx) {
                tab->selectedTabIdx = -1;
            }
            tab->xClicked = -1;
            if (tab->Count()) {
                InvalidateRgn(hwnd, nullptr, FALSE);
                UpdateWindow(hwnd);
            }
            break;

        case TCM_DELETEALLITEMS:
            tab->selectedTabIdx = -1;
            tab->highlighted = -1;
            tab->xClicked = -1;
            tab->xHighlighted = -1;
            break;

        case TCM_SETITEMSIZE:
            if (tab->Reshape(LOWORD(lp), HIWORD(lp))) {
                tab->xClicked = -1;
                if (tab->Count()) {
                    InvalidateRgn(hwnd, nullptr, FALSE);
                    UpdateWindow(hwnd);
                }
            }
            break;

        case TCM_GETCURSEL:
            return tab->selectedTabIdx;

        case TCM_SETCURSEL: {
            index = (int)wp;
            if (index >= tab->Count()) {
                return -1;
            }
            int previous = tab->selectedTabIdx;
            if (index != tab->selectedTabIdx) {
                tab->Invalidate(tab->selectedTabIdx);
                tab->Invalidate(index);
                tab->selectedTabIdx = index;
                UpdateWindow(hwnd);
            }
            return previous;
        }

        case WM_NCHITTEST: {
            if (!tab->inTitlebar || hwnd == GetCapture()) {
                return HTCLIENT;
            }
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &pt);
            if (-1 != tab->IndexFromPoint(pt.x, pt.y)) {
                return HTCLIENT;
            }
        }
            return HTTRANSPARENT;

        case WM_MOUSELEAVE:
            PostMessageW(hwnd, WM_MOUSEMOVE, 0xFF, 0);
            return 0;

        case WM_MOUSEMOVE: {
            tab->mouseCoordinates = lp;

            if (0xff != wp) {
                TrackMouseLeave(hwnd);
            }

            bool inX = false;
            int hl = wp == 0xFF ? -1 : tab->IndexFromPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), &inX);
            bool didChangeTabs = false;
            if (tab->isDragging && hl == -1) {
                // preserve the highlighted tab if it's dragged outside the tabs' area
                hl = tab->highlighted;
                didChangeTabs = true;
            }
            if (tab->highlighted != hl) {
                if (tab->isDragging) {
                    // send notification if the highlighted tab is dragged over another
                    MainWindow* win = FindMainWindowByHwnd(hwnd);
                    int tabNo = tab->highlighted;
                    if (tabs->onTabDragged) {
                        TabDraggedEvent ev;
                        ev.tabs = win->tabsCtrl;
                        ev.tab1 = tabNo;
                        ev.tab2 = hl;
                        tabs->onTabDragged(&ev);
                    }
                }

                tab->Invalidate(hl);
                tab->Invalidate(tab->highlighted);
                tab->highlighted = hl;
                didChangeTabs = true;
            }
            int xHl = inX && !tab->isDragging ? hl : -1;
            if (tab->xHighlighted != xHl) {
                tab->Invalidate(xHl);
                tab->Invalidate(tab->xHighlighted);
                tab->xHighlighted = xHl;
            }
            if (!inX) {
                tab->xClicked = -1;
            }
            if (didChangeTabs && tab->highlighted >= 0) {
                int idx = tab->highlighted;
                auto tabsCtrl = tab->tabsCtrl;
                tabsCtrl->MaybeUpdateTooltipText(idx);
            }
        }
            return 0;

        case WM_LBUTTONDOWN:
            bool inX;
            tab->nextTab = tab->IndexFromPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), &inX);
            if (inX) {
                tab->Invalidate(tab->nextTab);
                tab->xClicked = tab->nextTab;
            } else if (tab->nextTab != -1) {
                if (tab->nextTab != tab->selectedTabIdx) {
                    // HWND hwndParent = ::GetParent(hwnd);
                    NMHDR nmhdr = {hwnd, IDC_TABBAR, (UINT)TCN_SELCHANGING};
                    BOOL stopChange = (BOOL)tabs->OnNotifyReflect(IDC_TABBAR, (LPARAM)&nmhdr);
                    // TODO: why SendMessage doesn't work?
                    // BOOL stopChange = SendMessageW(hwndParent, WM_NOTIFY, IDC_TABBAR, (LPARAM)&nmhdr);
                    if (!stopChange) {
                        TabCtrl_SetCurSel(hwnd, tab->nextTab);
                        nmhdr = {hwnd, IDC_TABBAR, (UINT)TCN_SELCHANGE};
                        tabs->OnNotifyReflect(IDC_TABBAR, (LPARAM)&nmhdr);
                        return 0;
                    }
                    return 0;
                }
                tab->isDragging = true;
                SetCapture(hwnd);
            }
            return 0;

        case WM_LBUTTONUP:
            if (tab->xClicked != -1) {
                // send notification that the tab is closed
                MainWindow* win = FindMainWindowByHwnd(hwnd);
                if (tabs->onTabClosed) {
                    TabClosedEvent ev;
                    ev.tabs = tabs;
                    ev.tabIdx = tab->xClicked;
                    tabs->onTabClosed(&ev);
                }
                tab->Invalidate(tab->xClicked);
                tab->xClicked = -1;
            }
            if (tab->isDragging) {
                tab->isDragging = false;
                ReleaseCapture();
            }
            return 0;

        case WM_MBUTTONDOWN:
            // middle-clicking unconditionally closes the tab
            {
                tab->nextTab = tab->IndexFromPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
                tab->xClicked = tab->nextTab;
                tab->Invalidate(tab->nextTab);
            }
            return 0;

        case WM_MBUTTONUP:
            if (tab->xClicked != -1) {
                if (tabs->onTabClosed) {
                    TabClosedEvent ev;
                    ev.tabs = tabs;
                    ev.tabIdx = tab->xClicked;
                    tabs->onTabClosed(&ev);
                }
                int clicked = tab->xClicked;
                tab->Invalidate(clicked);
                tab->xClicked = -1;
            }
            return 0;

        case WM_ERASEBKGND:
            return TRUE;

        case WM_PAINT: {
            RECT rc;
            GetUpdateRect(hwnd, &rc, FALSE);
            // TODO: when is wp != nullptr?
            hdc = wp ? (HDC)wp : BeginPaint(hwnd, &ps);

            DoubleBuffer buffer(hwnd, Rect::FromRECT(rc));
            tab->Paint(buffer.GetDC(), rc);
            buffer.Flush(hdc);

            ValidateRect(hwnd, nullptr);
            if (!wp) {
                EndPaint(hwnd, &ps);
            }
            return 0;
        }
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

int GetTabbarHeight(HWND hwnd, float factor) {
    int dy = DpiScale(hwnd, kTabBarDy);
    return (int)(dy * factor);
}

static inline Size GetTabSize(HWND hwnd) {
    int dx = DpiScale(hwnd, std::max(gGlobalPrefs->tabWidth, kTabMinDx));
    int dy = DpiScale(hwnd, kTabBarDy);
    return Size(dx, dy);
}

static void ShowTabBar(MainWindow* win, bool show) {
    if (show == win->tabsVisible) {
        return;
    }
    win->tabsVisible = show;
    win->tabsCtrl->SetIsVisible(show);
    RelayoutWindow(win);
}

void UpdateTabWidth(MainWindow* win) {
    int count = (int)win->tabs.size();
    bool showSingleTab = gGlobalPrefs->useTabs || win->tabsInTitlebar;
    bool showTabs = (count > 1) || (showSingleTab && (count > 0));
    if (!showTabs) {
        ShowTabBar(win, false);
        return;
    }
    ShowTabBar(win, true);
    Rect rect = ClientRect(win->tabsCtrl->hwnd);
    Size tabSize = GetTabSize(win->hwndFrame);
    auto maxDx = (rect.dx - 3) / count;
    tabSize.dx = std::min(tabSize.dx, maxDx);
    win->tabsCtrl->SetItemSize(tabSize);
    win->tabsCtrl->MaybeUpdateTooltip();
}

static void RemoveTab(MainWindow* win, int idx) {
    WindowTab* tab = win->tabs.at(idx);
    UpdateTabFileDisplayStateForTab(tab);
    win->tabSelectionHistory->Remove(tab);
    win->tabs.Remove(tab);
    if (tab == win->currentTab) {
        win->ctrl = nullptr;
        win->currentTab = nullptr;
    }
    delete tab;
    win->tabsCtrl->RemoveTab(idx);
    UpdateTabWidth(win);
}

static void WinTabClosedHandler(MainWindow* win, TabsCtrl* tabs, int closedTabIdx) {
    int current = win->tabsCtrl->GetSelectedTabIndex();
    if (closedTabIdx == current) {
        CloseCurrentTab(win);
    } else {
        RemoveTab(win, closedTabIdx);
    }
}

// Selects the given tab (0-based index)
// TODO: this shouldn't go through the same notifications, just do it
void TabsSelect(MainWindow* win, int tabIndex) {
    auto& tabs = win->tabs;
    int count = tabs.Size();
    if (count < 2 || tabIndex < 0 || tabIndex >= count) {
        return;
    }
    TabsCtrl* tabsCtrl = win->tabsCtrl;
    int currIdx = tabsCtrl->GetSelectedTabIndex();
    if (tabIndex == currIdx) {
        return;
    }

    // same work as in onSelectionChanging and onSelectionChanged
    SaveCurrentWindowTab(win);
    int prevIdx = tabsCtrl->SetSelectedTabByIndex(tabIndex);
    if (prevIdx < 0) {
        return;
    }
    WindowTab* tab = tabs[tabIndex];
    LoadModelIntoTab(tab);
}

void CreateTabbar(MainWindow* win) {
    TabsCtrl* tabsCtrl = new TabsCtrl();
    tabsCtrl->onTabClosed = [win](TabClosedEvent* ev) { WinTabClosedHandler(win, ev->tabs, ev->tabIdx); };
    tabsCtrl->onSelectionChanging = [win](TabsSelectionChangingEvent* ev) -> bool {
        // TODO: Should we allow the switch of the tab if we are in process of printing?
        SaveCurrentWindowTab(win);
        return false;
    };

    tabsCtrl->onSelectionChanged = [win](TabsSelectionChangedEvent* ev) {
        int currentIdx = win->tabsCtrl->GetSelectedTabIndex();
        WindowTab* tab = win->tabs[currentIdx];
        LoadModelIntoTab(tab);
    };
    tabsCtrl->onTabDragged = [win](TabDraggedEvent* ev) {
        int tab1 = ev->tab1;
        int tab2 = ev->tab2;
        SwapTabs(win, tab1, tab2);
    };

    TabsCreateArgs args;
    args.parent = win->hwndFrame;
    args.ctrlID = IDC_TABBAR;
    args.createToolTipsHwnd = true;
    tabsCtrl->Create(args);

    HWND hwndTabBar = tabsCtrl->hwnd;
    SetWindowSubclass(hwndTabBar, TabBarProc, 0, (DWORD_PTR)win);

    Size tabSize = GetTabSize(win->hwndFrame);
    TabPainter* tp = new TabPainter(tabsCtrl, tabSize);
    SetWindowLongPtr(hwndTabBar, GWLP_USERDATA, (LONG_PTR)tp);
    tabsCtrl->SetItemSize(tabSize);
    win->tabsCtrl = tabsCtrl;

    win->tabSelectionHistory = new Vec<WindowTab*>();
}

// verifies that WindowTab state is consistent with MainWindow state
static NO_INLINE void VerifyWindowTab(MainWindow* win, WindowTab* tdata) {
    CrashIf(tdata->ctrl != win->ctrl);
#if 0
    // disabling this check. best I can tell, external apps can change window
    // title and trigger this
    auto winTitle = win::GetTextTemp(win->hwndFrame);
    if (!str::Eq(winTitle.Get(), tdata->frameTitle.Get())) {
        logf(L"VerifyWindowTab: winTitle: '%s', tdata->frameTitle: '%s'\n", winTitle.Get(), tdata->frameTitle.Get());
        ReportIf(!str::Eq(winTitle.Get(), tdata->frameTitle));
    }
#endif
    bool expectedTocVisibility = tdata->showToc; // if not in presentation mode
    if (PM_DISABLED != win->presentation) {
        expectedTocVisibility = false; // PM_BLACK_SCREEN, PM_WHITE_SCREEN
        if (PM_ENABLED == win->presentation) {
            expectedTocVisibility = tdata->showTocPresentation;
        }
    }
    ReportIf(win->tocVisible != expectedTocVisibility);
    ReportIf(tdata->canvasRc != win->canvasRc);
}

// Must be called when the active tab is losing selection.
// This happens when a new document is loaded or when another tab is selected.
void SaveCurrentWindowTab(MainWindow* win) {
    if (!win) {
        return;
    }

    int current = win->tabsCtrl->GetSelectedTabIndex();
    if (-1 == current) {
        return;
    }
    CrashIf(win->currentTab != win->tabs.at(current));

    WindowTab* tab = win->currentTab;
    if (win->tocLoaded) {
        TocTree* tocTree = tab->ctrl->GetToc();
        UpdateTocExpansionState(tab->tocState, win->tocTreeView, tocTree);
    }
    VerifyWindowTab(win, tab);

    // update the selection history
    win->tabSelectionHistory->Remove(tab);
    win->tabSelectionHistory->Append(tab);
}

#include "AppColors.h"

void TabPainterSetColors(TabPainter* p) {
    p->tabBackgroundBg = GetAppColor(AppColor::TabBackgroundBg);
    p->tabBackgroundText = GetAppColor(AppColor::TabBackgroundText);
    p->tabBackgroundCloseX = GetAppColor(AppColor::TabBackgroundCloseX);
    p->tabBackgroundCloseCircle = GetAppColor(AppColor::TabBackgroundCloseCircle);
    p->tabSelectedBg = GetAppColor(AppColor::TabSelectedBg);
    p->tabSelectedText = GetAppColor(AppColor::TabSelectedText);
    p->tabSelectedCloseX = GetAppColor(AppColor::TabSelectedCloseX);
    p->tabSelectedCloseCircle = GetAppColor(AppColor::TabSelectedCloseCircle);
    p->tabHighlightedBg = GetAppColor(AppColor::TabHighlightedBg);
    p->tabHighlightedText = GetAppColor(AppColor::TabHighlightedText);
    p->tabHighlightedCloseX = GetAppColor(AppColor::TabHighlightedCloseX);
    p->tabHighlightedCloseCircle = GetAppColor(AppColor::TabHighlightedCloseCircle);
    p->tabHoveredCloseX = GetAppColor(AppColor::TabHoveredCloseX);
    p->tabHoveredCloseCircle = GetAppColor(AppColor::TabHoveredCloseCircle);
    p->tabClickedCloseX = GetAppColor(AppColor::TabClickedCloseX);
    p->tabClickedCloseCircle = GetAppColor(AppColor::TabClickedCloseCircle);
}

void UpdateCurrentTabBgColor(MainWindow* win) {
    TabPainter* tab = (TabPainter*)GetWindowLongPtr(win->tabsCtrl->hwnd, GWLP_USERDATA);
    // TODO: match either the toolbar (if shown) or background
    tab->currBgCol = kTabDefaultBgCol;
    TabPainterSetColors(tab);
    RepaintNow(win->tabsCtrl->hwnd);
}

// On load of a new document we insert a new tab item in the tab bar.
WindowTab* CreateNewTab(MainWindow* win, const char* filePath) {
    CrashIf(!win);
    if (!win) {
        return nullptr;
    }

    WindowTab* tab = new WindowTab(win, filePath);
    win->tabs.Append(tab);
    tab->canvasRc = win->canvasRc;

    int idx = (int)win->tabs.size() - 1;
    auto tabs = win->tabsCtrl;
    int insertedIdx = tabs->InsertTab(idx, tab->GetTabTitle());
    CrashIf(insertedIdx == -1);
    tabs->SetSelectedTabByIndex(idx);
    UpdateTabWidth(win);
    return tab;
}

// Refresh the tab's title
void TabsOnChangedDoc(MainWindow* win) {
    WindowTab* tab = win->currentTab;
    CrashIf(!tab != !win->tabs.size());
    if (!tab) {
        return;
    }

    CrashIf(win->tabs.Find(tab) != win->tabsCtrl->GetSelectedTabIndex());
    VerifyWindowTab(win, tab);
    SetTabTitle(tab);
}

// Called when we're closing a document
void TabsOnCloseDoc(MainWindow* win) {
    if (win->tabs.size() == 0) {
        return;
    }

    /*
    DisplayModel* dm = win->AsFixed();
    if (dm) {
        EngineBase* engine = dm->GetEngine();
        if (EngineHasUnsavedAnnotations(engine)) {
            // TODO: warn about unsaved annotations
            logf("File has unsaved annotations\n");
        }
    }
    */

    int current = win->tabsCtrl->GetSelectedTabIndex();
    RemoveTab(win, current);

    if (win->tabs.size() > 0) {
        WindowTab* tab = win->tabSelectionHistory->Pop();
        int idx = win->tabs.Find(tab);
        win->tabsCtrl->SetSelectedTabByIndex(idx);
        LoadModelIntoTab(tab);
    }
}

// Called when we're closing an entire window (quitting)
void TabsOnCloseWindow(MainWindow* win) {
    win->tabsCtrl->RemoveAllTabs();
    win->tabSelectionHistory->Reset();
    win->currentTab = nullptr;
    win->ctrl = nullptr;
    DeleteVecMembers(win->tabs);
}

void SetTabsInTitlebar(MainWindow* win, bool inTitlebar) {
    if (inTitlebar == win->tabsInTitlebar) {
        return;
    }
    win->tabsInTitlebar = inTitlebar;
    TabPainter* tab = (TabPainter*)GetWindowLongPtr(win->tabsCtrl->hwnd, GWLP_USERDATA);
    tab->inTitlebar = inTitlebar;
    SetParent(win->tabsCtrl->hwnd, inTitlebar ? win->hwndCaption : win->hwndFrame);
    ShowWindow(win->hwndCaption, inTitlebar ? SW_SHOW : SW_HIDE);
    if (inTitlebar != win->isMenuHidden) {
        ToggleMenuBar(win);
    }
    if (inTitlebar) {
        CaptionUpdateUI(win, win->caption);
        RelayoutCaption(win);
    } else if (dwm::IsCompositionEnabled()) {
        // remove the extended frame
        MARGINS margins{};
        dwm::ExtendFrameIntoClientArea(win->hwndFrame, &margins);
        win->extendedFrameHeight = 0;
    }
    uint flags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
    SetWindowPos(win->hwndFrame, nullptr, 0, 0, 0, 0, flags);
}

// Selects the next (or previous) tab.
void TabsOnCtrlTab(MainWindow* win, bool reverse) {
    if (!win) {
        return;
    }
    int count = (int)win->tabs.size();
    if (count < 2) {
        return;
    }
    int idx = win->tabsCtrl->GetSelectedTabIndex() + 1;
    if (reverse) {
        idx -= 2;
    }
    idx += count; // ensure > 0
    idx = idx % count;
    TabsSelect(win, idx);
}
