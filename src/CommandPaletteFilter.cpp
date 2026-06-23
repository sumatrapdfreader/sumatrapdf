/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "FilterHighlightDraw.h"
#include "CommandPalette.h"
#include "CommandPaletteInternal.h"

static void FilterStrings(StrVecCP& strs, const StrVec& words, StrVecCP& matchedOut) {
    int n = strs.Size();
    for (int i = 0; i < n; i++) {
        const char* s = strs.At(i);
        if (!FilterMatches(s, words)) {
            continue;
        }
        matchedOut.AppendFrom(&strs, i);
    }
}

void CommandPaletteWnd::FilterStringsForQuery(const char* filter, StrVecCP& strings) {
    strings.Reset();
    if (!filter) {
        filter = "";
    }

    bool searchTabs = false, searchHistory = false, searchCommands = false, searchToc = false, searchFavorites = false;
    if (str::StartsWith(filter, kPalettePrefixEverything)) {
        filter++;
        searchTabs = searchHistory = searchCommands = true;
    } else if (str::StartsWith(filter, kPalettePrefixTabs)) {
        filter++;
        searchTabs = true;
    } else if (str::StartsWith(filter, kPalettePrefixFileHistory)) {
        filter++;
        searchHistory = true;
    } else if (str::StartsWith(filter, kPalettePrefixTOC)) {
        filter++;
        searchToc = true;
    } else if (str::StartsWith(filter, kPalettePrefixFavorites)) {
        filter++;
        searchFavorites = true;
    } else {
        if (str::StartsWith(filter, kPalettePrefixCommands)) {
            filter++;
        }
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
    const char* filter = editQuery->GetTextTemp();
    filter = CommandPaletteSkipWS(filter);
    int currSelIdx = 0;
    auto m = (ListBoxModelCP*)listBox->model;
    int nItemsPrev = m->ItemsCount();
    if (smartTabMode) {
        if (!stickyMode) {
            if (str::Len(filter) > 1) {
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
    if (str::StartsWith(filter, kPalettePrefixTOC) && filterWords.Size() == 0) {
        int idx = (currTocIdx >= 0 && currTocIdx < nItems) ? currTocIdx : 0;
        CommandPaletteSetCurrentSelection(this, idx);
        return;
    }
    CommandPaletteSetCurrentSelection(this, 0);
}