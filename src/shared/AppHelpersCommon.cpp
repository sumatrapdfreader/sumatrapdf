/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#if defined(SUMATRA_NG)
#include "VirtKeys.h"
#endif

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Translations.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "Annotation.h"
#include "FormFields.h"
#include "Theme.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "AppTools.h"
#include "AnnotFilterToolbar.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "AppHelpersCommon.h"

// One-off helpers that orig and ng both need and that have no shared file of
// their own module: each is a few lines used by one dialog or panel.

void FreeTabGroup(TabGroup* group) {
    if (!group) {
        return;
    }
    str::Free(group->name);
    if (group->tabFiles) {
        for (auto* tf : *group->tabFiles) {
            str::Free(tf->path);
            free(tf);
        }
        delete group->tabFiles;
    }
    free(group);
}

// Makes group's documents those of win, in tab order.
void SetTabGroupFiles(TabGroup* group, MainWindow* win) {
    if (!group->tabFiles) {
        group->tabFiles = new Vec<TabFile*>();
    }
    for (auto* tf : *group->tabFiles) {
        str::Free(tf->path);
        free(tf);
    }
    VecReset(*group->tabFiles);
    for (WindowTab* tab : win->Tabs()) {
        if (tab->IsAboutTab() || len(tab->filePath) == 0) {
            continue;
        }
        auto* tf = AllocStruct<TabFile>();
        str::ReplaceWithCopy(&tf->path, tab->filePath);
        VecAppend(*group->tabFiles, tf);
    }
}

// An AutoSync group follows win's tabs from now on; any other group unlinks it.
void LinkTabGroup(MainWindow* win, TabGroup* group) {
    win->syncedTabGroup = (group && group->autoSync) ? group : nullptr;
}

// Copies the window's tabs into its AutoSync group. The group can be gone
// (deleted, or the settings were reloaded) or no longer AutoSync: then the
// window stops following it.
void SyncTabGroup(MainWindow* win) {
    TabGroup* group = win->syncedTabGroup;
    if (!group) {
        return;
    }
    Vec<TabGroup*>* groups = gSettings->tabGroups;
    if (!groups || VecFind(*groups, group) < 0 || !group->autoSync) {
        win->syncedTabGroup = nullptr;
        return;
    }
    SetTabGroupFiles(group, win);
}

void SyncTabGroups() {
    for (MainWindow* win : gWindows) {
        SyncTabGroup(win);
    }
}

// what the tab group lists show next to a group's name: "4 tabs, synced"
TempStr TabGroupInfoTemp(TabGroup* group) {
    int n = group->tabFiles ? len(*group->tabFiles) : 0;
    if (group->autoSync) {
        return fmt(Tr("%d tabs, synced").s, n);
    }
    return fmt("%d tabs", n);
}

Str ScrollbarModeDisplayName(int idx) {
    if (idx == kScrollbarSmart) {
        return Tr("Smart Overlay");
    }
    if (idx == kScrollbarOverlay) {
        return Tr("Overlay");
    }
    if (idx == kScrollbarHidden) {
        return Tr("Hidden");
    }
    return Tr("Windows");
}

// list index of the match starting at (page, glyph), or -1 if there is none
int FindMatchIndex(MainWindow* win, int page, int glyph) {
    int n = len(win->findMatches);
    for (int i = 0; i < n; i++) {
        const FindMatch& fm = win->findMatches[i];
        if (fm.startPage == page && fm.startGlyph == glyph) {
            return i;
        }
    }
    return -1;
}

// list index of the match the document is currently on (so the selection can
// track the current match), or -1 if it isn't in the list
int FindCurrentMatchIndex(MainWindow* win) {
    if (win->ctrl && win->ctrl->CanFindInPage()) {
        // tracked by the browser (chm / markdown) webview find
        return win->browserFindCurrent;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm || !dm->textSearch || dm->textSearch->result.len == 0) {
        return -1;
    }
    return FindMatchIndex(win, dm->textSearch->startPage, dm->textSearch->startGlyph);
}

// first match at/after the current page, wrapping to the start of the document
// if there is none: the match with the smallest forward page distance from the
// current page (the list itself is in document order)
int FindFirstMatchFromPage(MainWindow* win) {
    int n = len(win->findMatches);
    if (n == 0) {
        return -1;
    }
    int curPage = win->ctrl ? win->ctrl->CurrentPageNo() : 1;
    int nPages = win->ctrl ? win->ctrl->PageCount() : 1;
    int best = 0;
    int bestDist = INT_MAX;
    for (int i = 0; i < n; i++) {
        int dist = win->findMatches[i].startPage - curPage;
        if (dist < 0) {
            dist += nPages;
        }
        if (dist < bestDist) {
            bestDist = dist;
            best = i;
            if (dist == 0) {
                break; // first match on the current page
            }
        }
    }
    return best;
}

// the field's on-screen font height in pixels: the /DA font size (PDF points)
// scaled to the page's current zoom, or a height-derived fallback for
// auto-sized (/DA size 0) fields.
int FieldFontPx(Annotation* widget, Rect rc) {
    float daSize = GetWidgetFontSize(widget);
    float pageDy = widget->bounds.dy; // field height in page (PDF) units
    if (daSize > 0 && pageDy > 0) {
        float scale = (float)rc.dy / pageDy; // screen px per PDF unit
        return std::max(8, (int)(daSize * scale));
    }
    return std::max(8, (int)((float)rc.dy * 0.7f));
}

int DecimalDigits(int n) {
    int digits = 1;
    while (n >= 10) {
        n /= 10;
        digits++;
    }
    return digits;
}

// find existing Shortcut entry for CmdScreenshot, or nullptr
Shortcut* FindScreenshotShortcutEntry() {
    for (Shortcut* sc : *gSettings->shortcuts) {
        if (str::EqI(sc->cmd, StrL("CmdScreenshot"))) {
            return sc;
        }
    }
    return nullptr;
}

TempStr FavoritePromptTemp(Str pageLabel) {
    int chapter = 0, page = 0;
    if (str::Parse(pageLabel, "%d/%d%$", &chapter, &page)) {
        return fmt(Tr("Name for chapter %d page %d (optional):").s, chapter, page);
    }
    return fmt(Tr("Name for page %s (optional):").s, pageLabel);
}

bool AnnotationHasText(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return false;
    }
    return len(Contents(annot)) > 0;
}

// the text stays readable on a tab that carries a color of its own
Color TabTextColorForBackground(Color text, Color tabBg) {
    if (abs((int)GetLightness(text) - (int)GetLightness(tabBg)) >= 80) {
        return text;
    }
    return IsLightColor(tabBg) ? kColBlack : kColWhite;
}

// cameFrom when it is a direct child of dir (so Back / Forward select it), else empty
Str SelectIfChildOf(Str cameFrom, Str dir) {
    if (len(cameFrom) == 0 || len(dir) == 0) {
        return {};
    }
    if (!path::IsSame(path::GetDirTemp(cameFrom), dir)) {
        return {};
    }
    return cameFrom;
}

bool IsListNavKey(int vkey) {
    return vkey == VK_UP || vkey == VK_DOWN || vkey == VK_PRIOR || vkey == VK_NEXT || vkey == VK_HOME || vkey == VK_END;
}

void PostedRefreshAnnots(MainWindow* win) {
    if (IsMainWindowValidAndNotClosing(win)) {
        RefreshAnnotFilterAnnotations(win);
    }
}

Color PopupBg() {
    return ThemeNotificationsBackgroundColor();
}

Color PopupText() {
    return ThemeNotificationsTextColor();
}

// the date is secondary information: same hue, less contrast
Color PopupMutedText() {
    float units = IsLightColor(PopupBg()) ? 55.0f : -55.0f;
    return AdjustLightness2(PopupText(), units);
}

// the rule under the header: a mid-tone that reads on both a light and a dark
// card (the window edge color is nearly invisible on white)
Color PopupRuleColor() {
    float units = IsLightColor(PopupBg()) ? 190.0f : -190.0f;
    return AdjustLightness2(PopupText(), units);
}

// "ShowFindBar" is the entry point used by FindFirst/Ctrl+F; it shows whichever
// find UI the user has chosen (compact overlay or floating window)
void ShowFindBar(MainWindow* win) {
    if (gSettings->searchUIFloating) {
        ShowFindWindow(win);
        return;
    }
    ShowCompactBar(win);
}

// true if either the compact bar or the floating find window is visible
bool IsFindUIVisible(MainWindow* win) {
    return IsFindBarVisible(win) || IsFindWindowVisible(win);
}

Str TranslateStr(Str s) {
    return Tr(s);
}

TempStr GetScreenshotSaveDirTemp() {
    TempStr dataDir = GetAppDataDirTemp();
    return path::JoinTemp(dataDir, StrL("Screenshots"));
}

void ShowChangeThemeDialog(MainWindow* win) {
    ShowThemeDialog(win, false);
}

void ShowSetDocumentColorsFollowThemeDialog(MainWindow* win) {
    ShowThemeDialog(win, true);
}

void ShowSaveTabGroupDialog(MainWindow* win) {
    ShowTabGroupsDialog(win, TabGroupDialogMode::Save);
}

void ShowOpenTabGroupDialog(MainWindow* win) {
    ShowTabGroupsDialog(win, TabGroupDialogMode::Open);
}

// find the saved state for a lazy tab by file path. Because gInitialSessionData
// is kept in sync with the live session, this never matches a closed window;
// per-tab disambiguation (e.g. same file in two windows) comes from the more
// reliable tab->tabState, which RememberSessionState prefers.
TabState* FindSessionTabState(Str fp) {
    if (!gInitialSessionData) {
        return nullptr;
    }
    for (SessionData* psd : *gInitialSessionData) {
        for (TabState* pts : *psd->tabStates) {
            if (str::Eq(pts->filePath, fp)) {
                return pts;
            }
        }
    }
    return nullptr;
}

// lazy tabs borrow tab->tabState from gInitialSessionData. After we replace that
// snapshot, repoint those pointers so the next SaveSettings() does not clone freed
// TabState objects
void RefreshLazyTabStatePointers() {
    int sdIdx = 0;
    for (MainWindow* win : gWindows) {
        bool hasFileTab = false;
        for (WindowTab* tab : win->Tabs()) {
            if (tab->filePath) {
                hasFileTab = true;
                break;
            }
        }
        if (!hasFileTab) {
            continue;
        }
        SessionData* sd = nullptr;
        if (gInitialSessionData && sdIdx < len(*gInitialSessionData)) {
            sd = (*gInitialSessionData)[sdIdx++];
        }
        int tsIdx = 0;
        for (WindowTab* tab : win->Tabs()) {
            if (len(tab->filePath) == 0) {
                continue;
            }
            TabState* ts = nullptr;
            if (sd && tsIdx < len(*sd->tabStates)) {
                ts = (*sd->tabStates)[tsIdx];
            }
            tsIdx++;
            if (!tab->ctrl && tab->tabState) {
                // null when the new snapshot has nothing to borrow: the old one
                // was just freed and must not be left dangling
                tab->tabState = ts;
            }
        }
    }
}
