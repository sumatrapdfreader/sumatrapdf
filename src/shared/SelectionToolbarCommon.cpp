/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "TextSelection.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Selection.h"
#include "Commands.h"
#include "CommandAvailability.h"
#include "AppSettings.h"
#include "Translations.h"
#include "SvgIcons.h"
#include "Toolbar.h"
#include "Notifications.h"
#include "SelectionToolbar.h"
#include "SelectionToolbarCommon.h"

// candidate buttons; per-window visibility/enabled state comes from
// GetCommandVisibility (hidden buttons are dropped, disabled ones grayed)
static const SelectionToolbarButton gCandidateButtons[] = {
    {CmdCopySelection, TrN("Copy to clipboard"), {}, Str(gIconCopy)},
    {CmdTranslateSelection, StrL("Translate"), {}, Str(gIconTranslate)},
    {CmdReadAloudSelection, StrL("Read Aloud"), {}, Str(gIconSpeak)},
    {CmdCreateAnnotHighlight, StrL("Highlight"), {}, Str(gIconAnnotHighlight)},
    {CmdCreateAnnotUnderline, StrL("Underline"), {}, Str(gIconAnnotUnderline)},
    {CmdCreateAnnotSquiggly, StrL("Squiggly"), {}, Str(gIconAnnotSquiggly)},
    {CmdCreateAnnotStrikeOut, StrL("Strike Out"), {}, Str(gIconAnnotStrikeOut)},
    {CmdCreateAnnotText, TrN("Add text annotation"), {}, Str(gIconAnnotText)},
};

const SelectionToolbarButton* FindCandidateButton(int cmdId) {
    if (cmdId <= 0) {
        return nullptr;
    }
    for (const SelectionToolbarButton& cand : gCandidateButtons) {
        if (cand.cmdId == cmdId) {
            return &cand;
        }
    }
    return nullptr;
}

// Built-in buttons the selection toolbar should offer, in order.
// Empty SelectionToolbarLayout is the standard set; otherwise the setting
// lists command names (discussion #6015).
void CollectBuiltInSelectionToolbarCmds(Vec<int>& out) {
    VecReset(out);
    auto addDefault = [&out]() {
        for (const SelectionToolbarButton& cand : gCandidateButtons) {
            VecAppend(out, cand.cmdId);
        }
    };
    Str setting = gSettings ? gSettings->selectionToolbarLayout : Str{};
    if (str::IsEmptyOrWhiteSpace(setting)) {
        addDefault();
        return;
    }
    TempStr normalized = str::ReplaceTemp(setting, StrL(","), StrL(" "));
    normalized = str::ReplaceTemp(normalized, StrL(";"), StrL(" "));
    StrVec names;
    Split(&names, normalized, StrL(" "), true);
    int nButtons = 0;
    for (Str name : names) {
        Str tok = name;
        str::TrimWSInPlace(tok, str::TrimOpt::Both);
        if (len(tok) == 0) {
            continue;
        }
        if (str::Eq(tok, StrL("|")) || str::EqI(tok, StrL("Separator"))) {
            VecAppend(out, 0);
            continue;
        }
        const SelectionToolbarButton* found = FindCandidateButton(GetCommandIdByName(tok));
        if (!found) {
            logf("SelectionToolbarLayout: no selection-toolbar button for '%s'\n", tok);
            continue;
        }
        bool already = false;
        for (int i = 0; i < len(out); i++) {
            if (out[i] == found->cmdId) {
                already = true;
                break;
            }
        }
        if (!already) {
            VecAppend(out, found->cmdId);
            nButtons++;
        }
    }
    if (nButtons == 0) {
        logf("SelectionToolbarLayout: nothing usable in '%s', using the standard layout\n", setting);
        VecReset(out);
        addDefault();
    }
}

// Remove separators that would be leading, trailing, or adjacent after
// unavailable commands have been dropped. A trailing layout separator is
// retained when a selection-handler button follows it.
void NormalizeSelectionToolbarSeparators(Vec<SelectionToolbarButton>& buttons) {
    int dst = 0;
    bool separatorPending = false;
    for (int i = 0; i < len(buttons); i++) {
        SelectionToolbarButton b = buttons[i];
        if (b.cmdId == 0) {
            separatorPending = dst > 0;
            continue;
        }
        if (separatorPending) {
            buttons[dst++] = {};
            separatorPending = false;
        }
        buttons[dst++] = b;
    }
    buttons.len = dst;
}

bool IsActivelySelecting(MainWindow* win) {
    MouseAction ma = win->mouseAction;
    return ma == MouseAction::Selecting || ma == MouseAction::SelectingText;
}
