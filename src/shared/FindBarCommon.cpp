/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "DisplayModel.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Commands.h"
#include "Accelerators.h"
#include "SvgIcons.h"
#include "Toolbar.h"
#include "SearchAndDDE.h"
#include "FindWindow.h"
#include "Translations.h"
#include "FindBar.h"
#include "AppHelpersCommon.h"

#if defined(SUMATRA_NG)
// width of the "n / m" status slot, wide enough for the largest count it will
// show, so the text field next to it doesn't move on every count update
float FindStatusSlotDx(int totalHits, bool capped) {
    int digits = DecimalDigits(std::max(totalHits, 0));
    int nChars = (2 * digits) + 3; // N, " / ", M
    if (capped) {
        nChars++; // the trailing '+' in e.g. "999 / 999+"
    }
    return (float)nChars * 7.f;
}
#endif

// Exercise and report find-UI state for focused integration tests.
TempStr FindUiStateResultTemp(Str action, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](int code) -> Str {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };
    if (len(gWindows) == 0) {
        out.Append(StrL("ERROR no-window\n"));
        return finish(1);
    }
    if (str::Eq(action, StrL("show-all"))) {
        for (MainWindow* w : gWindows) {
            ShowFindBar(w);
        }
    } else if (str::Eq(action, StrL("toggle-first"))) {
        ToggleFloatingFindUI(gWindows[0]);
    } else if (str::Eq(action, StrL("set-first-text"))) {
        FindEditSetText(gWindows[0], StrL("stale-term"));
    } else if (str::Eq(action, StrL("clear-first"))) {
        FindEditSetText(gWindows[0], StrL(""));
    } else if (str::Eq(action, StrL("hide-first"))) {
        HideFindBar(gWindows[0]);
    } else if (str::Eq(action, StrL("theme-recreate-first"))) {
        // RecreateFindBar is the compact bar's theme-change path.
        RecreateFindBar(gWindows[0]);
    } else if (!str::Eq(action, StrL("state"))) {
        out.Append(StrL("ERROR invalid action\n"));
        return finish(1);
    }
    int docs = 0;
    int compact = 0;
    int floating = 0;
    for (MainWindow* w : gWindows) {
        docs += w->IsDocLoaded() ? 1 : 0;
        compact += IsFindBarVisible(w) ? 1 : 0;
        floating += IsFindWindowVisible(w) ? 1 : 0;
    }
    // search state of the first window: highlighted matches, page of the
    // active hit (0: none) and whether a search is still running
    MainWindow* first = gWindows[0];
    int firstTextLen = first->findEdit ? FindEditTextLen(first) : -1;
    int matches = len(first->findMatches);
    int hitPage = 0;
    DisplayModel* dm = first->AsFixed();
    if (dm && dm->textSearch && len(dm->textSearch->result) > 0) {
        hitPage = dm->textSearch->result[0].pageNo;
    }
    bool busy = first->findThread || first->findCountThread || first->findDebouncePending;
    int page = first->ctrl ? first->ctrl->CurrentPageNo() : 0;
    out.Append(
        fmt("OK windows=%d docs=%d pref=%d compact=%d floating=%d firstTextLen=%d matches=%d hitPage=%d "
            "busy=%d page=%d historyOpen=%d\n",
            len(gWindows), docs, gSettings->searchUIFloating ? 1 : 0, compact, floating, firstTextLen, matches, hitPage,
            busy ? 1 : 0, page, IsFindHistoryOpen(first) ? 1 : 0));
    return finish(0);
}
