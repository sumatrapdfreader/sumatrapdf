/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "FilterHighlightDraw.h"
#include "CommandPalette.h"
#include "CommandPaletteInternal.h"

static void FilterStrings(StrVecCP& strs, const StrVec& words, StrVecCP& matchedOut) {
    int n = len(strs);
    for (int i = 0; i < n; i++) {
        Str s = strs[i];
        if (len(s) == 0) {
            continue;
        }
        if (!FilterMatches(s, words)) {
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

    bool searchTabs = false, searchHistory = false, searchCommands = false, searchToc = false, searchFavorites = false;
    if (str::TrimPrefix(filter, kPalettePrefixEverything)) {
        searchTabs = searchHistory = searchCommands = true;
    } else if (str::TrimPrefix(filter, kPalettePrefixTabs)) {
        searchTabs = true;
    } else if (str::TrimPrefix(filter, kPalettePrefixFileHistory)) {
        searchHistory = true;
    } else if (str::TrimPrefix(filter, kPalettePrefixTOC)) {
        searchToc = true;
    } else if (str::TrimPrefix(filter, kPalettePrefixFavorites)) {
        searchFavorites = true;
    } else {
        str::TrimPrefix(filter, kPalettePrefixCommands);
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
    if (str::StartsWith(filter, kPalettePrefixTOC) && len(filterWords) == 0) {
        int idx = (currTocIdx >= 0 && currTocIdx < nItems) ? currTocIdx : 0;
        CommandPaletteSetCurrentSelection(this, idx);
        return;
    }
    CommandPaletteSetCurrentSelection(this, 0);
}
