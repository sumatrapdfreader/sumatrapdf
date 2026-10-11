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

void InitButtons(Vec<SelectionToolbarButton>& buttons, MainWindow* win) {
    AppCommandCtx ctx = NewAppCommandCtx(win);
    VecReset(buttons);
    Vec<int> ids;
    CollectBuiltInSelectionToolbarCmds(ids);
    for (int i = 0; i < len(ids); i++) {
        if (ids[i] == 0) {
            VecAppend(buttons, {});
            continue;
        }
        const SelectionToolbarButton* cand = FindCandidateButton(ids[i]);
        if (!cand) {
            continue;
        }
        CommandVisibility v = GetCommandVisibility(cand->cmdId, ctx, CommandSurface::Toolbar);
        if (CommandShouldRemove(v)) {
            continue;
        }
        SelectionToolbarButton b = *cand;
        b.enabled = !CommandShouldDisable(v);
        VecAppend(buttons, b);
    }
    AppendSelectionHandlerButtons(buttons, ctx);
    NormalizeSelectionToolbarSeparators(buttons);
}

Str ButtonLabel(const SelectionToolbarButton& b) {
    return len(b.userLabel) > 0 ? b.userLabel : Tr(b.label);
}

// union of the on-screen parts of the selection, in canvas coordinates;
// false if the selection is empty or fully scrolled out of view
bool GetSelectionBounds(MainWindow* win, Rect& out) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->selectionOnPage) {
        return false;
    }
    Rect canvas = SelectionCanvasRect(win, dm);
    Rect bounds;
    bool first = true;
    for (SelectionOnPage& sel : *tab->selectionOnPage) {
        Rect r = sel.GetRect(dm).Intersect(canvas);
        if (r.IsEmpty()) {
            continue;
        }
        if (first) {
            bounds = r;
            first = false;
        } else {
            bounds = bounds.Union(r);
        }
    }
    if (first) {
        return false;
    }
    out = bounds;
    return true;
}

// Fire a selection-toolbar button the same way a click does, for -dbg-control tests.
TempStr SelectionToolbarClickTemp(Str cmdName, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    if (!IsSelectionToolbarVisible(win)) {
        return finish(StrL("ERROR toolbar-not-visible\n"), 1);
    }
    int cmdId = GetCommandIdByName(cmdName);
    if (cmdId <= 0) {
        return finish(fmt("ERROR unknown-cmd %s\n", cmdName), 1);
    }
    bool found = false;
    for (const SelectionToolbarButton& b : *SelectionToolbarButtons(win)) {
        if (b.cmdId == cmdId) {
            found = true;
            break;
        }
    }
    if (!found) {
        return finish(fmt("ERROR no-button %s\n", cmdName), 1);
    }
    InvokeSelectionToolbarCommand(win, cmdId);
    return finish(StrL("OK\n"), 0);
}

// The selection changed or went away: a new one gets the toolbar again.
void ResetSelectionToolbarDismissed(MainWindow* win) {
    if (win && win->selectionToolbar) {
        win->selectionToolbarDismissed = false;
    }
}
