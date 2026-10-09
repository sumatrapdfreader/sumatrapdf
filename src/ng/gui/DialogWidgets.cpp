/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "gui/GpuiBridge.h"

#include "Menu.h"
#include "gui/DialogWidgets.h"

#include "SumatraLog.h"

float DialogScrollToRow(float scrollY, int row, int nRows, float rowDy, float viewDy) {
    float maxY = std::max(0.f, (float)nRows * rowDy - viewDy);
    if (row >= 0 && row < nRows) {
        float top = (float)row * rowDy;
        if (top < scrollY) {
            scrollY = top;
        } else if (top + rowDy > scrollY + viewDy) {
            scrollY = top + rowDy - viewDy;
        }
    }
    return std::max(0.f, std::min(scrollY, maxY));
}

float DialogListViewDy(int nRows, float rowDy, float minDy, float maxDy, float borderDy) {
    float dy = (float)nRows * rowDy + 2 * borderDy;
    return std::max(minDy, std::min(dy, maxDy)) - 2 * borderDy;
}

static Vec<gp::Entity<gp::PopupMenuState>> gTrackedPopups;

// the menu OpenPopupMenuAt() opened and how many frames it was built for since
static gp::Entity<gp::PopupMenuState> gFitPopup;
static int gFitPopupFrames = 0;

// orig's TrackPopupMenu keeps a menu on the screen: one that doesn't fit right
// of / below the point opens left of / above it. gpui puts a menu where it is
// told to, so one opened near the window's right or bottom edge was cut off.
// Its size is only known once it was painted, so it moves on its second frame.
static void FitPopupInWindow(gp::Ctx* cx, gp::PopupMenuState* st) {
    if (!st->open || !cx->win) {
        gFitPopup = {};
        return;
    }
    gFitPopupFrames++;
    if (gFitPopupFrames < 2) {
        gp::AppInvalidate(cx->win);
        return;
    }
    gFitPopup = {};
    gp::WinSize ws = gp::WindowSize(cx->win);
    gp::Bounds b = st->bounds;
    if (b.w <= 0 || b.h <= 0) {
        return;
    }
    float dx = 0;
    float dy = 0;
    if (b.x + b.w > ws.dipW) {
        dx = -std::min(b.w, b.x);
    }
    if (b.y + b.h > ws.dipH) {
        dy = -std::min(b.h, b.y);
    }
    if (dx == 0 && dy == 0) {
        return;
    }
    st->x += dx;
    st->y += dy;
    gp::AppInvalidate(cx->win);
}

gpc::PopupMenu* TrackPopup(gp::Ctx* cx, gpc::PopupMenu* menu) {
    if (!menu || !menu->state.IsValid()) {
        return menu;
    }
    if (gFitPopup.id == menu->state.id) {
        if (gp::PopupMenuState* st = menu->state.Get(cx)) {
            FitPopupInWindow(cx, st);
        }
    }
    // forget the menus of windows and views that are gone
    for (int i = len(gTrackedPopups) - 1; i >= 0; i--) {
        if (gTrackedPopups[i].id == menu->state.id) {
            return menu;
        }
        if (!gTrackedPopups[i].Get(cx)) {
            VecRemoveAt(gTrackedPopups, i);
        }
    }
    VecAppend(gTrackedPopups, menu->state);
    return menu;
}

static gp::PopupMenuState* OpenTrackedPopup(gp::Ctx* cx) {
    for (auto& e : gTrackedPopups) {
        gp::PopupMenuState* st = e.Get(cx);
        if (st && st->open) {
            return st;
        }
    }
    return nullptr;
}

// the open drop-down of a DialogSelect, which is a popup like the menus
static Vec<gp::Entity<gpc::SelectState>> gTrackedSelects;

static gpc::SelectState* OpenTrackedSelect(gp::Ctx* cx) {
    for (int i = len(gTrackedSelects) - 1; i >= 0; i--) {
        gpc::SelectState* st = gTrackedSelects[i].Get(cx);
        if (!st) {
            VecRemoveAt(gTrackedSelects, i);
            continue;
        }
        if (st->state.open) {
            return st;
        }
    }
    return nullptr;
}

void OpenPopupMenuAt(gp::Ctx* cx, gp::Entity<gp::PopupMenuState> menu, float x, float y) {
    gp::PopupMenuState* st = menu.Get(cx);
    if (!st) {
        return;
    }
    st->x = x;
    st->y = y;
    gp::PopupMenuOpen(st, cx);
    gFitPopup = menu;
    gFitPopupFrames = 0;
}

// macOS control-click is the context click. It arrives as the left button.
bool IsContextClick(gp::MouseButton button, gp::Modifiers mods) {
#if OS_DARWIN
    if (button == gp::MouseButton::Left && mods.control && !mods.shift && !mods.alt && !mods.platform) {
        return true;
    }
#endif
    return button == gp::MouseButton::Right;
}

// the tooltip HoverTooltipShow() was asked for and has not shown yet
static gp::App* gHoverTipApp = nullptr;
static gp::Window* gHoverTipWin = nullptr;
static Str gHoverTipText;
static gp::Bounds gHoverTipAt{};
static bool gHoverTipPending = false;

static void HoverTooltipShowPosted() {
    if (!gHoverTipPending) {
        return;
    }
    gHoverTipPending = false;
    // the window may be gone by now
    bool live = false;
    for (int i = 0; gHoverTipApp && i < gHoverTipApp->windows.len; i++) {
        live = live || (gHoverTipApp->windows[i] == gHoverTipWin);
    }
    if (!live) {
        return;
    }
    gp::TooltipRequestShow(gHoverTipWin, ToGpui(gHoverTipText), gHoverTipAt);
}

void HoverTooltipShow(gp::Ctx* cx, Str tip, gp::Bounds at) {
    HoverTooltipShowIn(cx->app, cx->win, tip, at);
}

void HoverTooltipHideIn(gp::Window* win) {
    if (!win) {
        return;
    }
    if (win == gHoverTipWin) {
        gHoverTipPending = false;
    }
    gp::TooltipRequestHide(win);
}

void HoverTooltipShowIn(gp::App* app, gp::Window* win, Str tip, gp::Bounds at) {
    if (!win || len(tip) == 0) {
        return;
    }
    gHoverTipApp = app;
    gHoverTipWin = win;
    str::ReplaceWithCopy(&gHoverTipText, tip);
    gHoverTipAt = at;
    if (gHoverTipPending) {
        return;
    }
    gHoverTipPending = true;
    gp::ExecPost(gp::MkFunc0Void(HoverTooltipShowPosted));
}

void HoverTooltipHide(gp::Ctx* cx) {
    if (cx->win == gHoverTipWin) {
        gHoverTipPending = false;
    }
    gp::TooltipRequestHide(cx->win);
}

static DialogSelect* OpenCombo();

bool IsTrackedPopupOpen(gp::Ctx* cx) {
    return OpenTrackedPopup(cx) != nullptr || OpenTrackedSelect(cx) != nullptr || OpenCombo() != nullptr;
}

bool IsTrackedPopupOpenInApp(gp::App* app) {
    for (auto& e : gTrackedPopups) {
        auto* st = (gp::PopupMenuState*)gp::EntityGet(app, e.id);
        if (st && st->open) {
            return true;
        }
    }
    return false;
}

bool DismissTrackedPopup(gp::Ctx* cx) {
    if (gp::PopupMenuState* st = OpenTrackedPopup(cx)) {
        gp::PopupMenuDismissAll(st, cx);
        return true;
    }
    if (gpc::SelectState* sel = OpenTrackedSelect(cx)) {
        gpc::SelectToggleOpen(sel, cx);
        return true;
    }
    if (DialogSelect* dd = OpenCombo()) {
        dd->comboOpen = false;
        return true;
    }
    return false;
}

static DialogSelect* gOpenComboPtr();
static void ComboForget(DialogSelect* dd);

static DialogSelect* OpenCombo() {
    DialogSelect* dd = gOpenComboPtr();
    return dd && dd->comboOpen ? dd : nullptr;
}

void DialogSelect::Init(gp::App* a) {
    Free();
    app = a;
    state = gpc::SelectState::New(a);
}

void DialogSelect::Free() {
    if (gOpenComboPtr() == this) {
        ComboForget(this);
    }
    if (app && hasComboView) {
        gp::EntityDrop(app, comboView);
    }
    comboView = {};
    hasComboView = false;
    comboOpen = false;
    comboScrollY = 0;
    comboEdit = nullptr;
    comboWin = nullptr;
    comboPicked = false;
    VecReset(items);
    titles.Reset();
    sel = -1;
    if (app && state.IsValid()) {
        gp::EntityDrop(app, state.id);
    }
    state = {};
    app = nullptr;
}

void DialogSelect::SetItems(const StrVec& strings, int selected) {
    VecReset(items);
    titles.Reset();
    for (Str s : strings) {
        titles.Append(s);
    }
    for (int i = 0; i < len(titles); i++) {
        gpc::SearchableItem it;
        it.title = ToGpui(titles[i]);
        it.value = it.title;
        VecAppend(items, it);
    }
    sel = selected;
}

void DialogSelect::SetSel(gp::App* app, int selected) {
    sel = selected;
    gpc::SelectState* s = state.Get(app);
    if (!s) {
        return;
    }
    s->SetItems(items.els, len(items));
    s->SetSelectedIndex(selected, nullptr);
}

// ng: gpui reports a Select's change through an entity subscription; polling it
// once a frame keeps the dialog's state the single source of truth and needs no
// subscription to unwind when the dialog closes
bool DialogSelect::PollChanged(gp::App* app) {
    gpc::SelectState* s = state.Get(app);
    if (!s) {
        return false;
    }
    int now = s->state.selected.len > 0 ? s->state.selected[0] : -1;
    if (now == sel || now < 0) {
        return false;
    }
    sel = now;
    return true;
}

Str DialogSelect::SelText() const {
    if (sel < 0 || sel >= len(titles)) {
        return {};
    }
    return titles[sel];
}

// ng: component::Select draws the trigger but binds nothing to it - the caller
// owns the open / close. This is the handler that opens the menu.
static void SelectTriggerClicked(gpc::SelectState* s, gp::Ctx* cx, const gp::ClickEvent*) {
    gpc::SelectToggleOpen(s, cx);
}

gp::El* DialogSelect::Build(gp::Ctx* cx, Str id, float w, bool disabled) {
    gpc::SelectState* s = state.Get(cx->app);
    if (s && s->state.selected.len == 0 && sel >= 0) {
        s->SetItems(items.els, len(items));
        s->SetSelectedIndex(sel, nullptr);
    }
    bool tracked = false;
    for (auto& e : gTrackedSelects) {
        tracked = tracked || e.id == state.id;
    }
    if (!tracked) {
        VecAppend(gTrackedSelects, state);
    }
    return gpc::Select::New(cx, GpuiDup(cx->a, id), state)
        ->Items(items.els, len(items))
        ->WithSize(gp::UiSize::Small)
        ->W(w)
        ->Disabled(disabled)
        ->OnToggle(gp::ListenTo(state, &SelectTriggerClicked))
        ->IntoEl();
}

// --- the editable combo box -----------------------------------------------

// ng: the list is the port's own element, not a gpui PopupMenu: a PopupMenu
// opened from code does not take the focus and dispatches a row's action from
// wherever the focus is, which a dialog's combo cannot count on
constexpr float kComboRowDy = 22;
constexpr float kComboListMaxDy = 242;

struct DialogComboView {
    DialogSelect* owner = nullptr;

    static void OnArrow(DialogComboView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnRow(DialogComboView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
    static void OnKey(DialogComboView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnDownOut(DialogComboView* self, gp::Ctx* cx, const gp::MouseDownEvent*);
    static void OnListScroll(DialogComboView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
};

// the one combo whose list is dropped, so Esc closes it before the dialog
static DialogSelect* gOpenCombo = nullptr;

static DialogSelect* gOpenComboPtr() {
    return gOpenCombo;
}

static void ComboForget(DialogSelect* dd) {
    if (gOpenCombo == dd) {
        gOpenCombo = nullptr;
    }
}

static void ComboSetOpen(DialogSelect* dd, bool open) {
    dd->comboOpen = open && len(dd->titles) > 0;
    if (dd->comboOpen) {
        gOpenCombo = dd;
    } else if (gOpenCombo == dd) {
        gOpenCombo = nullptr;
    }
}

static void ComboPick(DialogSelect* dd, gp::Ctx* cx, int idx) {
    if (!dd->comboEdit || idx < 0 || idx >= len(dd->titles)) {
        return;
    }
    dd->sel = idx;
    dd->comboPicked = true;
    gp::InputSetValue(dd->comboEdit, ToGpui(dd->titles[idx]));
    gp::InputSelectAll(dd->comboEdit, cx);
    dd->comboScrollY = DialogScrollToRow(dd->comboScrollY, idx, len(dd->titles), kComboRowDy, kComboListMaxDy - 2);
}

void DialogComboView::OnArrow(DialogComboView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    DialogSelect* dd = self->owner;
    if (!dd) {
        return;
    }
    ComboSetOpen(dd, !dd->comboOpen);
    // win32's combo focuses its edit when the arrow is clicked
    if (dd->comboEdit) {
        gp::InputFocus(dd->comboEdit, cx->app, cx->win);
    }
    if (dd->comboOpen) {
        int cur = dd->titles.Find(FromGpui(gp::InputValue(dd->comboEdit)));
        dd->sel = cur;
        dd->comboScrollY = DialogScrollToRow(0, cur, len(dd->titles), kComboRowDy, kComboListMaxDy - 2);
    }
    gp::Notify(cx);
    gp::AppInvalidate(cx->win);
}

void DialogComboView::OnRow(DialogComboView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx) {
    DialogSelect* dd = self->owner;
    if (!dd) {
        return;
    }
    ComboPick(dd, cx, (int)idx);
    dd->comboListPick = true;
    ComboSetOpen(dd, false);
    if (dd->comboEdit) {
        gp::InputFocus(dd->comboEdit, cx->app, cx->win);
    }
    gp::Notify(cx);
    gp::AppInvalidate(cx->win);
}

void DialogComboView::OnDownOut(DialogComboView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev) {
    DialogSelect* dd = self->owner;
    if (!dd || !dd->comboOpen) {
        return;
    }
    // a press on the box itself is the arrow's (it toggles) or the field's
    const gp::Bounds& b = dd->comboBounds;
    if (ev->x >= b.x && ev->x < b.x + b.w && ev->y >= b.y && ev->y < b.y + b.h) {
        return;
    }
    ComboSetOpen(dd, false);
    gp::Notify(cx);
    gp::AppInvalidate(cx->win);
}

void DialogComboView::OnListScroll(DialogComboView* self, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    if (self->owner) {
        self->owner->comboScrollY = ev->offsetY;
        gp::Notify(cx);
    }
}

// win32's combo box: Up / Down in the edit step through the list, F4 and
// Alt + Down / Up drop or close it, Enter and Esc close a dropped list
void DialogComboView::OnKey(DialogComboView* self, gp::Ctx* cx, const gp::KeyEvent* ev) {
    DialogSelect* dd = self->owner;
    if (!dd || !dd->comboEdit || !gp::FocusHandleIsFocused(cx->win, dd->comboEdit->focus)) {
        return;
    }
    bool isArrow = ev->vk == gp::KeyDown || ev->vk == gp::KeyUp;
    bool toggles = (ev->vk == 0x73 /* F4 */ && !ev->alt && !ev->ctrl) || (isArrow && ev->alt);
    if (toggles) {
        ComboSetOpen(dd, !dd->comboOpen);
    } else if (isArrow && !ev->ctrl && !ev->shift) {
        int n = len(dd->titles);
        if (n == 0) {
            return;
        }
        // from the row the field's text is, or from before the first one
        int cur = dd->titles.Find(FromGpui(gp::InputValue(dd->comboEdit)));
        int idx = ev->vk == gp::KeyDown ? std::min(n - 1, cur + 1) : std::max(0, cur - 1);
        ComboPick(dd, cx, idx);
    } else if (dd->comboOpen && (ev->vk == gp::KeyReturn || ev->vk == 0x1B /* Esc */)) {
        ComboSetOpen(dd, false);
    } else {
        return;
    }
    const_cast<gp::KeyEvent*>(ev)->propagate = false;
    gp::Notify(cx);
    gp::AppInvalidate(cx->win);
}

bool DialogSelect::TakeComboListPick() {
    bool res = comboListPick;
    comboListPick = false;
    return res;
}

bool DialogSelect::TakeComboPicked() {
    bool res = comboPicked;
    comboPicked = false;
    return res;
}

gp::El* DialogSelect::BuildCombo(gp::Ctx* cx, Str id, gp::InputState* edit, float w, bool disabled, float dy,
                                 float fontPx) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    comboEdit = edit;
    comboWin = cx->win;
    gp::Entity<DialogComboView> view{comboView};
    if (!hasComboView || !gp::EntityGet(cx->app, comboView)) {
        view = gp::EntityNewState<DialogComboView>(cx->app);
        comboView = view.id;
        hasComboView = true;
    }
    ((DialogComboView*)gp::EntityGet(cx->app, comboView))->owner = this;
    if (disabled || len(titles) == 0) {
        ComboSetOpen(this, false);
    }

    // one box: the field and the arrow share the edge, as a combo box's do
    gp::El* box = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->BoundsOut(&comboBounds);
    gp::El* field = gpc::Input::New(cx, GpuiDup(cx->a, id), edit)
                        ->WithSize(gp::UiSize::Small)
                        ->Disabled(disabled)
                        ->W(gp::kFill)
                        ->IntoEl();
    if (dy > 0) {
        field->H(dy);
    }
    if (fontPx > 0) {
        field->Font(fontPx);
    }
    box->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(field));
    gpc::Button* arrow = gpc::Button::New(cx, GpuiDup(cx->a, fmt("%s-arrow", id)))
                             ->Icon(gp::IconName::ChevronDown)
                             ->Ghost()
                             ->Compact()
                             ->WithSize(gp::UiSize::Small)
                             ->Disabled(disabled || len(titles) == 0)
                             ->OnClick(gp::ListenTo(view, &DialogComboView::OnArrow));
    // drawn inside the field's right end
    float arrowDy = comboBounds.h > 2 ? comboBounds.h - 2 : 22;
    box->Child(gp::Div(cx->a)
                   ->Absolute()
                   ->Right(1)
                   ->Top(1)
                   ->H(arrowDy)
                   ->FlexRow()
                   ->ItemsCenter()
                   ->ClipY()
                   ->Bg(th.tokens.background)
                   ->Child(arrow->IntoEl()));

    return gp::Div(cx->a)->W(w)->CaptureKeyDown(gp::ListenTo(view, &DialogComboView::OnKey))->Child(box);
}

float DialogComboListBottom(gp::Window* win) {
    DialogSelect* dd = gOpenCombo;
    if (!dd || !dd->comboOpen || dd->comboWin != win || dd->comboBounds.h <= 0) {
        return 0;
    }
    float listDy = std::min((float)len(dd->titles) * kComboRowDy + 2, kComboListMaxDy);
    return dd->comboBounds.y + dd->comboBounds.h + listDy;
}

gp::El* DialogComboListBuild(gp::Ctx* cx) {
    DialogSelect* dd = gOpenCombo;
    if (!dd || !dd->comboOpen || !dd->hasComboView || !gp::EntityGet(cx->app, dd->comboView)) {
        return nullptr;
    }
    // its bounds are in the window that drew the box
    if (dd->comboWin && dd->comboWin != cx->win) {
        return nullptr;
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::Entity<DialogComboView> view{dd->comboView};
    int n = len(dd->titles);
    float listDy = std::min((float)n * kComboRowDy + 2, kComboListMaxDy);
    gp::El* rows = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Bg(th.tokens.background);
    gp::Listener onRow = gp::ListenTo(view, &DialogComboView::OnRow, 0);
    for (int i = 0; i < n; i++) {
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(kComboRowDy)
                          ->Shrink0()
                          ->ItemsCenter()
                          ->PadX(8)
                          ->HoverBg(th.tokens.accent)
                          ->PathClick(GpuiDup(cx->a, fmt("dlg-combo-row-%d", i)))
                          ->OnClick(gp::ListenerArg(onRow, i));
        if (i == dd->sel) {
            row->Bg(th.tokens.accent);
        }
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, dd->titles[i]))->Font(13)->Fg(th.foreground)->Truncate());
        rows->Child(row);
    }
    const gp::Bounds& b = dd->comboBounds;
    return gp::Div(cx->a)
        ->Id(GStrL("dlg-combo-list"))
        ->Absolute()
        ->Left(b.x)
        ->Top(b.y + (b.h > 0 ? b.h : 24))
        ->W(b.w > 0 ? b.w : 200)
        ->H(listDy)
        ->Bg(th.tokens.background)
        ->Border(1, th.border)
        ->ScrollY(dd->comboScrollY)
        ->ScrollFromPath()
        ->OnScroll(gp::ListenTo(view, &DialogComboView::OnListScroll))
        ->OnMouseDownOut(gp::ListenTo(view, &DialogComboView::OnDownOut))
        ->Child(rows)
        // a dialog is drawn in gpui's deferred layer; so is this, after it
        ->Deferred();
}

enum class DlgAccelKind {
    None,
    Click,
    Input,
    Select
};

struct DlgAccel {
    gp::Window* win = nullptr;
    char key = 0;
    DlgAccelKind kind = DlgAccelKind::None;
    bool disabled = false;
    gp::Str text = {};
    int underlineOff = -1;
    int underlineLen = 0;
    gp::Listener onClick = {};
    gp::InputState* input = nullptr;
    DialogSelect* select = nullptr;
};

// the access keys of the dialogs each window built in its last frame
static Vec<DlgAccel> gDlgAccels;
static bool gDlgCues = false;

// Windows underlines the access keys always or only once Alt was pressed
// ("Underline access keys when available", off by default)
static bool DlgCuesAlways() {
#if OS_WIN
    BOOL on = FALSE;
    SystemParametersInfoW(SPI_GETKEYBOARDCUES, 0, &on, 0);
    return on != FALSE;
#else
    return false;
#endif
}

struct DlgDefault {
    gp::Window* win = nullptr;
    gp::Listener onOk;
    bool fromInputs = false;
};

static Vec<DlgDefault> gDlgDefaults;

void DlgSetDefault(gp::Ctx* cx, gp::Listener onOk, bool fromInputs) {
    for (DlgDefault& d : gDlgDefaults) {
        if (d.win == cx->win) {
            d.onOk = onOk;
            d.fromInputs = fromInputs;
            return;
        }
    }
    VecAppend(gDlgDefaults, DlgDefault{cx->win, onOk, fromInputs});
}

static void DlgDefaultsBeginFrame(gp::Window* win) {
    for (int i = len(gDlgDefaults) - 1; i >= 0; i--) {
        if (gDlgDefaults[i].win == win) {
            VecRemoveAt(gDlgDefaults, i);
        }
    }
}

bool DlgDefaultOnEnter(gp::Ctx* cx, bool editFocused) {
    for (const DlgDefault& d : gDlgDefaults) {
        if (d.win != cx->win || (editFocused && !d.fromInputs)) {
            continue;
        }
        gp::ClickEvent ev;
        ev.keyboard = true;
        gp::Listener l = d.onOk;
        gp::ListenerCall(cx->app, cx->win, l, &ev);
        return true;
    }
    return false;
}

void DlgAccelBeginFrame(gp::Window* win) {
    DlgDefaultsBeginFrame(win);
    if (len(gDlgAccels) == 0) {
        // no dialog was up: win32 forgets the cue state with the dialog
        gDlgCues = false;
    }
    for (int i = len(gDlgAccels) - 1; i >= 0; i--) {
        if (gDlgAccels[i].win == win) {
            VecRemoveAt(gDlgAccels, i);
        }
    }
}

void DlgAccelOnAlt() {
    if (len(gDlgAccels) > 0) {
        gDlgCues = true;
    }
}

static gp::Str DlgAccelAdd(gp::Ctx* cx, Str label, DlgAccel acc) {
    MenuAccelText at = ParseMenuAccelTextTemp(label);
    acc.text = GpuiDup(cx->a, at.display);
    acc.underlineOff = at.underlineOff;
    acc.underlineLen = at.underlineLen;
    acc.key = MenuAccessKey(label);
    acc.win = cx->win;
    if (at.underlineOff >= 0) {
        VecAppend(gDlgAccels, acc);
    }
    return acc.text;
}

gp::Str DlgAccelClick(gp::Ctx* cx, Str label, gp::Listener onClick, bool disabled) {
    DlgAccel acc;
    acc.kind = DlgAccelKind::Click;
    acc.onClick = onClick;
    acc.disabled = disabled;
    return DlgAccelAdd(cx, label, acc);
}

gp::Str DlgAccelInput(gp::Ctx* cx, Str label, gp::InputState* input) {
    DlgAccel acc;
    acc.kind = DlgAccelKind::Input;
    acc.input = input;
    return DlgAccelAdd(cx, label, acc);
}

gp::Str DlgAccelSelect(gp::Ctx* cx, Str label, DialogSelect* select) {
    DlgAccel acc;
    acc.kind = DlgAccelKind::Select;
    acc.select = select;
    return DlgAccelAdd(cx, label, acc);
}

gp::Str DlgAccelNone(gp::Ctx* cx, Str label) {
    DlgAccel acc;
    return DlgAccelAdd(cx, label, acc);
}

// the text element showing `text` under `el`; *fg is the color it is drawn in,
// its own or the nearest ancestor's
static gp::El* FindTextEl(gp::El* el, gp::Str text, gp::Rgba* fg) {
    if (!el) {
        return nullptr;
    }
    gp::Rgba inherited = *fg;
    if (el->style.hasColor) {
        *fg = el->style.color;
    }
    if (el->kind == gp::ElKind::Text && el->text.s == text.s) {
        return el;
    }
    for (gp::El* c = el->first; c; c = c->next) {
        if (gp::El* found = FindTextEl(c, text, fg)) {
            return found;
        }
    }
    *fg = inherited;
    return nullptr;
}

// ng: gpui's Button / Checkbox / Radio take a plain label and build the text
// element themselves, so the underline is put on that element after the
// fact, found by the label's address
gp::El* DlgAccelUnderline(gp::Ctx* cx, gp::El* el, gp::Str text) {
    if (!gDlgCues && !DlgCuesAlways()) {
        return el;
    }
    const DlgAccel* acc = nullptr;
    for (const DlgAccel& a : gDlgAccels) {
        if (a.text.s == text.s) {
            acc = &a;
        }
    }
    gp::Rgba fg = gp::ThemeNow(cx->app).foreground;
    gp::El* textEl = acc ? FindTextEl(el, text, &fg) : nullptr;
    if (!textEl) {
        return el;
    }
    auto* span = gp::ArenaNew<gp::TextSpan>(cx->a);
    span->lo = acc->underlineOff;
    span->hi = acc->underlineOff + acc->underlineLen;
    span->color = fg;
    textEl->Underlines(span, 1);
    return el;
}

gp::El* DlgAccelText(gp::Ctx* cx, gp::Str text) {
    return DlgAccelUnderline(cx, gp::TextEl(cx->a, text), text);
}

// Alt + letter, or the letter alone while no text field has the focus
bool DlgAccelOnKey(gp::Ctx* cx, int vk, bool alt, bool ctrl, bool editFocused) {
    if (ctrl || vk < 'A' || vk > 'Z' || len(gDlgAccels) == 0) {
        return false;
    }
    if (!alt && editFocused) {
        return false;
    }
    char key = (char)(vk - 'A' + 'a');
    for (const DlgAccel& acc : gDlgAccels) {
        if (acc.win != cx->win || acc.key != key || acc.disabled) {
            continue;
        }
        switch (acc.kind) {
            case DlgAccelKind::Click: {
                gp::ClickEvent ev;
                ev.keyboard = true;
                gp::Listener l = acc.onClick;
                gp::ListenerCall(cx->app, cx->win, l, &ev);
                break;
            }
            case DlgAccelKind::Input:
                if (acc.input) {
                    gp::FocusHandleFocus(cx->win, acc.input->focus);
                    // win32 selects an edit's text when its label's key focuses it
                    gp::InputSelectAll(acc.input, cx);
                }
                break;
            case DlgAccelKind::Select:
                if (acc.select) {
                    if (gpc::SelectState* s = acc.select->state.Get(cx->app)) {
                        gp::FocusHandleFocus(cx->win, s->state.triggerFocus);
                    }
                }
                break;
            case DlgAccelKind::None:
                break;
        }
        // a letter typed at a dialog never falls through to a command
        return true;
    }
    return false;
}
