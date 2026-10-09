/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's SelectionToolbar.cpp - the small floating bar that appears over a
// finished text selection. orig makes it a layered popup window with its own
// virtual controls; here it is an absolutely positioned gpui card inside the
// canvas element, so it follows the selection for free. The button set, the
// debounced show, the "dismissed until the selection changes" rule and the
// placement (above the selection, below it if there is no room) are orig's.

#include "gui/GpuiBridge.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "TextSelection.h"
#include "Commands.h"
#include "CommandAvailability.h"
#include "Translations.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "Selection.h"
#include "SelectionToolbar.h"
#include "SvgIcons.h"
#include "Toolbar.h"

#include "SumatraLog.h"

static Kind kNotifCopiedToClipboard = "notifCopiedToClipboard";
constexpr int kCopiedNotifTimeoutMs = 1500;
// orig's kSelectionToolbarShowDelayInMs (Canvas.h)
constexpr int kShowDelayInMs = 500;

struct SelectionToolbarButton {
    int cmdId = 0;
    Str label; // English literal, translated for the button text
    // a SelectionHandlers entry's own label (SelectToolbarNameOrSvg / its name),
    // shown verbatim. `label` is empty then
    Str userLabel;
    Str svgIcon;
    bool enabled = true;
};

struct SelectionToolbar {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr; // tab the current selection belongs to
    bool visible = false;
    // an action was picked for the current selection: stay hidden until it changes
    bool dismissed = false;
    // ms still to wait before the debounced show
    int showPendingMs = -1;
    Vec<SelectionToolbarButton> buttons;
    // where the card ended up last frame; gpui reports it one frame late, which
    // is enough to center the card on the selection from the second frame on
    gp::Bounds measured;
};

// candidate buttons; per-window visibility/enabled state comes from
// GetCommandVisibility (hidden buttons are dropped)
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

// a selection handler shows its own label; a built-in one the translated one
static Str ButtonLabel(const SelectionToolbarButton& b) {
    return len(b.userLabel) > 0 ? b.userLabel : Tr(b.label);
}

static const SelectionToolbarButton* FindCandidateButton(int cmdId) {
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
static void CollectBuiltInSelectionToolbarCmds(Vec<int>& out) {
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

// selection handlers that asked for a button with SelectToolbarNameOrSvg
static void AppendSelectionHandlerButtons(SelectionToolbar* tb, const AppCommandCtx& ctx) {
    Vec<CustomCommand*> cmds;
    GetCommandsWithOrigId(cmds, CmdSelectionHandler);
    for (CustomCommand* cmd : cmds) {
        Str s = GetCommandStringArg(cmd, kCmdArgSelectToolbar, {});
        if (str::IsEmptyOrWhiteSpace(s)) {
            continue;
        }
        CommandVisibility v = GetCommandVisibility(cmd->id, ctx, CommandSurface::Toolbar);
        if (CommandShouldRemove(v)) {
            continue;
        }
        SelectionToolbarButton b;
        b.cmdId = cmd->id;
        if (str::StartsWithI(s, StrL("<svg"))) {
            b.userLabel = cmd->name;
            b.svgIcon = s;
        } else {
            b.userLabel = s;
        }
        b.enabled = !CommandShouldDisable(v);
        if (len(b.userLabel) == 0) {
            continue;
        }
        VecAppend(tb->buttons, b);
    }
}

// Remove separators that would be leading, trailing, or adjacent after
// unavailable commands have been dropped.
static void NormalizeSelectionToolbarSeparators(Vec<SelectionToolbarButton>& buttons) {
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

static void InitButtons(SelectionToolbar* tb, MainWindow* win) {
    AppCommandCtx ctx = NewAppCommandCtx(win);
    VecReset(tb->buttons);
    Vec<int> ids;
    CollectBuiltInSelectionToolbarCmds(ids);
    for (int i = 0; i < len(ids); i++) {
        if (ids[i] == 0) {
            VecAppend(tb->buttons, {});
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
        VecAppend(tb->buttons, b);
    }
    AppendSelectionHandlerButtons(tb, ctx);
    NormalizeSelectionToolbarSeparators(tb->buttons);
}

static bool IsActivelySelecting(MainWindow* win) {
    MouseAction ma = win->mouseAction;
    return ma == MouseAction::Selecting || ma == MouseAction::SelectingText;
}

static SelectionToolbar* GetOrCreateToolbar(MainWindow* win) {
    if (!win->selectionToolbar) {
        auto* tb = new SelectionToolbar();
        tb->win = win;
        win->selectionToolbar = tb;
    }
    return win->selectionToolbar;
}

// union of the on-screen parts of the selection, in canvas coordinates;
// false if the selection is empty or fully scrolled out of view
static bool GetSelectionBounds(MainWindow* win, Rect& out) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->selectionOnPage) {
        return false;
    }
    Rect canvas(Point(), dm->GetViewPort().Size());
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

bool IsSelectionToolbarVisible(MainWindow* win) {
    SelectionToolbar* tb = win ? win->selectionToolbar : nullptr;
    return tb && tb->visible;
}

// Show the floating selection toolbar for the current text selection. Does
// nothing if the feature is disabled (Annotations.SelectionToolbar) or there
// is no on-screen text selection in a fixed-page document.
static void ShowSelectionToolbarNow(MainWindow* win) {
    if (!win || !gSettings->selectionToolbar) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm || dm->textSelection->result.len <= 0) {
        return;
    }
    Rect sel;
    if (!GetSelectionBounds(win, sel)) {
        return;
    }
    SelectionToolbar* tb = GetOrCreateToolbar(win);
    tb->tab = win->CurrentTab();
    InitButtons(tb, win);
    if (len(tb->buttons) == 0) {
        return;
    }
    tb->visible = true;
    str::Builder labels;
    for (const SelectionToolbarButton& b : tb->buttons) {
        if (len(labels) > 0) {
            labels.Append(StrL(", "));
        }
        labels.Append(b.cmdId == 0 ? StrL("|") : ButtonLabel(b));
    }
    logf("SelectionToolbar: showing %d buttons over %d,%d,%d,%d: %s\n", len(tb->buttons), sel.x, sel.y, sel.dx, sel.dy,
         ToStr(labels));
    AppShellInvalidate(win);
}

// A completed mouse gesture shows immediately. Repaint-driven requests wait
// for the selection to settle so the toolbar does not flash while it changes.
void ShowSelectionToolbar(MainWindow* win, SelToolbarShow when) {
    if (!win || !gSettings->selectionToolbar) {
        return;
    }
    SelectionToolbar* tb = GetOrCreateToolbar(win);
    if (tb->dismissed) {
        return;
    }
    if (when == SelToolbarShow::Now) {
        tb->showPendingMs = -1;
        ShowSelectionToolbarNow(win);
        return;
    }
    if (tb->showPendingMs >= 0) {
        return;
    }
    tb->showPendingMs = kShowDelayInMs;
}

// ng: orig waits out kSelectionToolbarShowTimerID; the shell's tick counts it
// down instead
void SelectionToolbarOnShowTimer(MainWindow* win, int elapsedMs) {
    SelectionToolbar* tb = win ? win->selectionToolbar : nullptr;
    if (!tb || tb->showPendingMs < 0) {
        return;
    }
    tb->showPendingMs -= elapsedMs;
    if (tb->showPendingMs > 0) {
        return;
    }
    tb->showPendingMs = -1;
    // the selection may be gone or still being dragged by now; both self-guard
    if (IsActivelySelecting(win)) {
        return;
    }
    ShowSelectionToolbarNow(win);
}

// cancel a pending debounced show (the selection went away or is being redone)
static void CancelPendingShow(MainWindow* win) {
    SelectionToolbar* tb = win ? win->selectionToolbar : nullptr;
    if (tb) {
        tb->showPendingMs = -1;
    }
}

// Called from the canvas paint: hide the bar while a drag is going on and when
// the selection scrolled out of view, show it again when it comes back.
void UpdateSelectionToolbarPosition(MainWindow* win) {
    if (!win) {
        return;
    }
    // Hide during drag so the bar does not chase the rubber-band selection.
    if (IsActivelySelecting(win)) {
        if (IsSelectionToolbarVisible(win)) {
            HideSelectionToolbar(win);
        }
        return;
    }
    SelectionToolbar* tb = win->selectionToolbar;
    if (!tb || !tb->visible) {
        if (win->showSelection) {
            ShowSelectionToolbar(win, SelToolbarShow::Settled);
        }
        return;
    }
    if (win->CurrentTab() != tb->tab) {
        HideSelectionToolbar(win);
        if (win->showSelection) {
            ShowSelectionToolbar(win, SelToolbarShow::Settled);
        }
        return;
    }
    Rect sel;
    if (!GetSelectionBounds(win, sel)) {
        HideSelectionToolbar(win);
    }
}

// Hide the toolbar; a new selection gets it back.
void HideSelectionToolbar(MainWindow* win) {
    CancelPendingShow(win);
    SelectionToolbar* tb = win ? win->selectionToolbar : nullptr;
    if (!tb || !tb->visible) {
        return;
    }
    tb->visible = false;
    tb->tab = nullptr;
    AppShellInvalidate(win);
}

// The selection changed or went away: a new one gets the toolbar again.
void ResetSelectionToolbarDismissed(MainWindow* win) {
    if (win && win->selectionToolbar) {
        win->selectionToolbar->dismissed = false;
    }
}

void DeleteSelectionToolbar(MainWindow* win) {
    SelectionToolbar* tb = win ? win->selectionToolbar : nullptr;
    if (!tb) {
        return;
    }
    win->selectionToolbar = nullptr;
    delete tb;
}

// The toolbar has done its job once an action is picked, so hide it until the
// selection changes.
static void InvokeSelectionToolbarCommand(MainWindow* win, int cmdId) {
    SelectionToolbar* tb = win ? win->selectionToolbar : nullptr;
    if (!tb || !cmdId) {
        return;
    }
    HideSelectionToolbar(win);
    tb->dismissed = true;

    ExecuteCmd(win, cmdId);
    if (cmdId != CmdCopySelection || !HasPermission(Perm::CopySelection)) {
        return;
    }
    RemoveNotificationsForGroup(win, kNotifCopiedToClipboard);
    NotificationCreateArgs args;
    args.win = win;
    args.groupId = kNotifCopiedToClipboard;
    args.timeoutMs = kCopiedNotifTimeoutMs;
    args.corner = NotifCorner::BottomLeft;
    args.msg = Tr("Copied to clipboard");
    ShowNotification(args);
}

// --- the gpui card ----------------------------------------------------------

struct SelToolbarView {
    MainWindow* win = nullptr;

    static void OnClick(SelToolbarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId);
};

static gp::Entity<SelToolbarView> gSelToolbarView;

void SelToolbarView::OnClick(SelToolbarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId) {
    InvokeSelectionToolbarCommand(self->win, (int)cmdId);
    gp::Notify(cx);
}

// a first guess at the card's width so it can be centered on the selection
// before gpui has laid it out once (BoundsOut reports that a frame later)
static float EstimateCardWidth(SelectionToolbar* tb) {
    float w = 10;
    for (const SelectionToolbarButton& b : tb->buttons) {
        if (b.cmdId == 0) {
            w += 8;
            continue;
        }
        w += len(b.svgIcon) > 0 ? 30.f : (float)len(ButtonLabel(b)) * 7.f + 24.f;
    }
    return w;
}

gp::El* SelectionToolbarBuild(MainWindow* win, gp::Ctx* cx) {
    SelectionToolbar* tb = win->selectionToolbar;
    if (!tb || !tb->visible || len(tb->buttons) == 0) {
        return nullptr;
    }
    Rect sel;
    if (!GetSelectionBounds(win, sel)) {
        return nullptr;
    }
    if (!gSelToolbarView.IsValid()) {
        gSelToolbarView = gp::EntityNewState<SelToolbarView>(cx->app);
    }
    auto* view = (SelToolbarView*)gp::EntityGet(cx->app, gSelToolbarView.id);
    view->win = win;

    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* card = gp::Div(cx->a)
                       ->FlexRow()
                       ->ItemsCenter()
                       ->Gap(2)
                       ->Pad(4)
                       ->Radius(10)
                       ->Bg(th.tokens.popover)
                       ->Border(1, th.border);
    for (const SelectionToolbarButton& b : tb->buttons) {
        if (b.cmdId == 0) {
            card->Child(gp::Div(cx->a)->W(1)->H(18)->Bg(th.tokens.muted));
            continue;
        }
        TempStr id = fmt("sel-tb-%d", b.cmdId);
        auto* button = gpc::Button::New(cx, GpuiDup(cx->a, id))
                           ->Ghost()
                           ->Compact()
                           ->WithSize(gp::UiSize::Small)
                           ->Disabled(!b.enabled)
                           ->OnClick(gp::Listen(cx, &SelToolbarView::OnClick, (intptr_t)b.cmdId));
        if (len(b.svgIcon) > 0) {
            auto* icon = gpc::Icon::Empty(cx)->Data(ToGpui(b.svgIcon));
            button->Icon(gpc::ButtonIcon::New(cx, icon));
            button->Tooltip(GpuiDup(cx->a, ButtonLabel(b)));
        } else {
            button->Label(GpuiDup(cx->a, ButtonLabel(b)));
        }
        card->Child(button->IntoEl());
    }
    card->BoundsOut(&tb->measured);

    // Prefer above the selection, fall back to below; clamp to the canvas.
    float k = CanvasScale(win);
    float w = tb->measured.w > 0 ? tb->measured.w : EstimateCardWidth(tb);
    float h = tb->measured.h > 0 ? tb->measured.h : 32;
    float gap = (float)DpiScale(6);
    float canvasW = (float)win->canvasRc.dx;
    float canvasH = (float)win->canvasRc.dy;

    float x = ((float)sel.x + (float)sel.dx / 2) * k - (w / 2);
    float y = (float)sel.y * k - gap - h;
    if (y < 0) {
        y = ((float)(sel.y + sel.dy)) * k + gap;
    }
    x = std::min(x, canvasW - w);
    x = std::max(x, 0.f);
    y = std::min(y, canvasH - h);
    y = std::max(y, 0.f);
    card->Absolute()->Left(x)->Top(y);
    return card;
}

// Parsed and laid-out selection toolbar state for -dbg-control tests.
TempStr SelectionToolbarLayoutDumpTemp(MainWindow* win) {
    Vec<int> ids;
    CollectBuiltInSelectionToolbarCmds(ids);
    str::Builder out;
    out.Append(fmt("n=%d\n", len(ids)));
    int nSvgIcons = 0;
    for (int i = 0; i < len(ids); i++) {
        out.Append(fmt("cmd=%d\n", ids[i]));
        const SelectionToolbarButton* b = FindCandidateButton(ids[i]);
        if (b && len(b->svgIcon) > 0) {
            nSvgIcons++;
        }
    }
    out.Append(fmt("svgIcons=%d\n", nSvgIcons));

    SelectionToolbar* tb = win ? GetOrCreateToolbar(win) : nullptr;
    out.Append(fmt("visible=%d\n", IsSelectionToolbarVisible(win) ? 1 : 0));
    NotificationWnd* notif = win ? GetNotificationForGroup(win, kNotifCopiedToClipboard) : nullptr;
    out.Append(fmt("notif=%s\n", notif ? NotificationGetMessageTemp(notif) : StrL("")));
    if (tb && tb->visible) {
        gp::Bounds r = tb->measured;
        out.Append(fmt("placed=%d,%d,%d,%d\n", (int)r.x, (int)r.y, (int)r.w, (int)r.h));
    }
    if (!tb) {
        out.Append(StrL("buttons=0\n"));
        return ToStrTemp(out);
    }
    InitButtons(tb, win);
    int nSeparators = 0;
    for (const SelectionToolbarButton& b : tb->buttons) {
        if (b.cmdId == 0) {
            nSeparators++;
        }
    }
    int iconSize = ToolbarIconSize();
    out.Append(fmt("buttons=%d separators=%d toolbarSize=%d,%d mainIconSize=%d\n", len(tb->buttons), nSeparators, 0, 0,
                   iconSize));
    for (int i = 0; i < len(tb->buttons); i++) {
        const SelectionToolbarButton& b = tb->buttons[i];
        Str kind = b.cmdId == 0 ? StrL("separator") : (len(b.svgIcon) > 0 ? StrL("icon") : StrL("text"));
        int iconDx = len(b.svgIcon) > 0 ? iconSize : 0;
        out.Append(
            fmt("button=%d cmd=%d kind=%s icon=%d,%d tooltip=%s\n", i, b.cmdId, kind, iconDx, iconDx, ButtonLabel(b)));
    }
    return ToStrTemp(out);
}

TempStr SelectionToolbarLayoutDumpTemp() {
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    return SelectionToolbarLayoutDumpTemp(win);
}

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
    SelectionToolbar* tb = win ? win->selectionToolbar : nullptr;
    if (!IsSelectionToolbarVisible(win) || !tb) {
        return finish(StrL("ERROR toolbar-not-visible\n"), 1);
    }
    int cmdId = GetCommandIdByName(cmdName);
    if (cmdId <= 0) {
        return finish(fmt("ERROR unknown-cmd %s\n", cmdName), 1);
    }
    bool found = false;
    for (const SelectionToolbarButton& b : tb->buttons) {
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
