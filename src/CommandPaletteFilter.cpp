/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Accelerators.h"
#include "FilterHighlightDraw.h"
#include "CommandPalette.h"
#include "CommandPaletteInternal.h"

// Return the same effective shortcut text that is painted on the right side
// of a command row, without the menu separator tab.
TempStr CommandPaletteShortcutTemp(i32 cmdId) {
    if (cmdId == 0) {
        return {};
    }
    TempStr withAccel = AppendAccelKeyToMenuStringTemp(StrL(""), cmdId);
    if (!withAccel || withAccel.s[0] != '\t') {
        return {};
    }
    return Str(withAccel.s + 1, len(withAccel) - 1);
}

static void FilterStrings(StrVecCP& strs, const StrVec& words, StrVecCP& matchedOut) {
    int n = len(strs);
    for (int i = 0; i < n; i++) {
        Str s = strs[i];
        if (len(s) == 0) {
            continue;
        }
        bool matches = FilterMatches(s, words);
        ItemDataCP* data = strs.AtData(i);
        if (!matches && data && data->cmdId != 0) {
            TempStr shortcut = CommandPaletteShortcutTemp(data->cmdId);
            matches = FilterMatches(shortcut, words);
        }
        if (!matches && data && data->boolSetting) {
            Str val = *data->boolSetting ? StrL("true") : StrL("false");
            matches = FilterMatches(val, words);
        }
        if (!matches) {
            continue;
        }
        matchedOut.AppendFrom(&strs, i);
    }
}

void CommandPaletteWnd::FilterStringsForQuery(Str filter, StrVecCP& strings) {
    strings.Reset();
    if (!filter) {
        filter = StrL("");
    }

    bool searchTabs = false, searchHistory = false, searchCommands = false, searchToc = false, searchFavorites = false,
         searchBoolSettings = false;
    if (str::TrimPrefix(filter, Str(kPalettePrefixEverything))) {
        searchTabs = searchHistory = searchCommands = true;
    } else if (str::TrimPrefix(filter, Str(kPalettePrefixTabs))) {
        searchTabs = true;
    } else if (str::TrimPrefix(filter, Str(kPalettePrefixFileHistory))) {
        searchHistory = true;
    } else if (str::TrimPrefix(filter, Str(kPalettePrefixTOC)) ||
               str::TrimPrefix(filter, Str(kPalettePrefixTOCLegacy))) {
        searchToc = true;
    } else if (str::TrimPrefix(filter, Str(kPalettePrefixFavorites))) {
        searchFavorites = true;
    } else if (str::TrimPrefix(filter, Str(kPalettePrefixBoolSettings))) {
        searchBoolSettings = true;
    } else {
        str::TrimPrefix(filter, Str(kPalettePrefixCommands));
        searchCommands = true;
    }

    filterWords.Reset();
    SplitFilterToWords(filter, filterWords);

    if (searchTabs) {
        FilterStrings(tabs, filterWords, strings);
    }
    if (searchHistory) {
        FilterStrings(fileHistory, filterWords, strings);
    }
    if (searchCommands) {
        FilterStrings(commands, filterWords, strings);
    }
    if (searchToc) {
        FilterStrings(toc, filterWords, strings);
    }
    if (searchFavorites) {
        FilterStrings(favorites, filterWords, strings);
    }
    if (searchBoolSettings) {
        FilterStrings(boolSettings, filterWords, strings);
    }
}

void CommandPaletteWnd::QueryChanged() {
    Str filter = CommandPaletteSkipWS(Str(editQuery->GetTextTemp()));
    int currSelIdx = 0;
    auto* m = (ListBoxModelCP*)listBox->model;
    int nItemsPrev = m->ItemsCount();
    if (smartTabMode) {
        if (!stickyMode) {
            if (len(filter) > 1) {
                stickyMode = true;
                currSelIdx = listBox->GetCurrentSelection();
            }
        }
    }
    FilterStringsForQuery(filter, m->strings);
    listBox->SetModel(m);
    int nItems = m->ItemsCount();
    if (nItems == 0) {
        return;
    }
    if (stickyMode && nItemsPrev == nItems) {
        CommandPaletteSetCurrentSelection(this, currSelIdx);
        return;
    }
    if ((str::StartsWith(filter, Str(kPalettePrefixTOC)) || str::StartsWith(filter, Str(kPalettePrefixTOCLegacy))) &&
        len(filterWords) == 0) {
        int idx = (currTocIdx >= 0 && currTocIdx < nItems) ? currTocIdx : 0;
        CommandPaletteSetCurrentSelection(this, idx);
        return;
    }
    CommandPaletteSetCurrentSelection(this, 0);
}
