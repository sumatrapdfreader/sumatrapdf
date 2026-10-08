/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the two controls orig's dialogs build out of win32 that gpui does not
// hand us ready-made: a drop-down whose selection we poll (gpui's Select
// reports a change through an entity subscription, which would outlive the
// dialog) and the editable drop-down, which is an Input plus that Select.
// Include after gui/GpuiBridge.h.

struct DialogSelect {
    gpui::App* app = nullptr;
    gpui::Entity<gpui::component::SelectState> state;
    // the Select keeps a pointer to this array, so it lives as long as the
    // dialog does; `titles` owns the strings the items point at
    Vec<gpui::component::SearchableItem> items;
    StrVec titles;
    int sel = -1;

    void Init(gpui::App* app);
    void SetItems(const StrVec& strings, int selected);
    void SetSel(gpui::App* app, int selected);
    void Free();
    // reads the selection back from gpui; true when the user changed it
    bool PollChanged(gpui::App* app);
    Str SelText() const;
    gpui::El* Build(gpui::Ctx* cx, Str id, float w, bool disabled = false);

    // orig's editable combo box (CBS_DROPDOWN): `edit` with a drop-down arrow
    // at its right end, in one box. The list of the items opens under the box
    // and picking a row puts its text into the field; Up / Down in the field
    // step through the items, F4 and Alt + Down open the list.
    // dy: the box's height, for orig's 23 in a dialog window; 0: gpui's
    gpui::El* BuildCombo(gpui::Ctx* cx, Str id, gpui::InputState* edit, float w, bool disabled = false, float dy = 0);
    // set once a row was picked, until asked; lets a dialog react to a pick
    bool TakeComboPicked();
    // the same for a pick with the mouse out of the dropped list only
    bool TakeComboListPick();

    gpui::EntityId comboView = {};
    bool hasComboView = false;
    gpui::InputState* comboEdit = nullptr;
    gpui::Bounds comboBounds{};
    // the window the box was built in last: its root draws the dropped list
    gpui::Window* comboWin = nullptr;
    bool comboPicked = false;
    bool comboListPick = false;
    // the list under the box is dropped; its scroll offset
    bool comboOpen = false;
    float comboScrollY = 0;
};

// ng: a gpui context menu does not take the focus, so the shell cannot tell
// from the key event that one is open. Every context / drop-down menu goes
// through TrackPopup() when built (a DialogSelect tracks itself); while one is
// open the shell leaves the keys to it (win32's modal menu loop) and Esc
// closes only the popup.
gpui::component::PopupMenu* TrackPopup(gpui::Ctx* cx, gpui::component::PopupMenu* menu);
bool IsTrackedPopupOpen(gpui::Ctx* cx);
// for -dbg-control's TestUiState, which has no frame context
bool IsTrackedPopupOpenInApp(gpui::App* app);
// ng: component::ContextMenu opens its popup from its own right-button press.
// orig's context menus open when the button is released, or from the keyboard,
// so the wrapped element stops that press (gpui::WindowStopPropagation) and
// the port opens the popup itself; x / y are relative to the wrapped element
void OpenPopupMenuAt(gpui::Ctx* cx, gpui::Entity<gpui::PopupMenuState> menu, float x, float y);
bool IsContextClick(gpui::MouseButton button, gpui::Modifiers mods);

// A tooltip asked for from an element's hover listener. gpui hides the
// window's tooltip right after it ran the hover listeners of an element
// without a Tip() of its own, which cancelled a gpui::TooltipRequestShow()
// made in the listener; these make the request once gpui is done with the
// mouse move.
void HoverTooltipShow(gpui::Ctx* cx, Str tip, gpui::Bounds at);
// the same in `win`, for an element of a window too small for a tooltip
void HoverTooltipShowIn(gpui::App* app, gpui::Window* win, Str tip, gpui::Bounds at);
void HoverTooltipHideIn(gpui::Window* win);
void HoverTooltipHide(gpui::Ctx* cx);
bool DismissTrackedPopup(gpui::Ctx* cx);

// gpui draws a ScrollY() box at the offset it is handed and reports the wheel
// and the scrollbar through OnScroll, so a list that is to scroll keeps the
// offset and feeds it back. This returns `scrollY` moved just enough to show
// row `row` of `nRows` rows of `rowDy` in a view `viewDy` tall.
float DialogScrollToRow(float scrollY, int row, int nRows, float rowDy, float viewDy);

// the height a FlexCol list of `nRows` rows gets between MinH and MaxH, less
// its border
float DialogListViewDy(int nRows, float rowDy, float minDy, float maxDy, float borderDy = 1);

// orig's dialog access keys (the '&' in a dialog template's labels). A dialog
// passes each label through one of the DlgAccel*() calls while it builds: the
// '&' is stripped, the key is remembered for this frame with what it does, and
// DlgAccelUnderline() underlines the letter under the menus' rule (from an Alt
// press until the dialog is gone, or always with SPI_GETKEYBOARDCUES). The
// shell hands Alt + letter - and the bare letter while no text field has the
// focus, as IsDialogMessage does - to DlgAccelOnKey().

// a button, a checkbox or a radio button: the key clicks it
gpui::Str DlgAccelClick(gpui::Ctx* cx, Str label, gpui::Listener onClick, bool disabled = false);
// the label of an input: the key focuses the input
gpui::Str DlgAccelInput(gpui::Ctx* cx, Str label, gpui::InputState* input);
// the label of a drop-down: the key focuses it
gpui::Str DlgAccelSelect(gpui::Ctx* cx, Str label, DialogSelect* select);
// the label alone: stripped and underlined, the key does nothing
gpui::Str DlgAccelNone(gpui::Ctx* cx, Str label);
// underlines the access key of `text` (what a DlgAccel*() call returned) in
// the text element showing it inside `el`; returns `el`
gpui::El* DlgAccelUnderline(gpui::Ctx* cx, gpui::El* el, gpui::Str text);
// a text element for a DlgAccel*() label, underlined when the cues are on
gpui::El* DlgAccelText(gpui::Ctx* cx, gpui::Str text);

// orig's default button (WindowBase::ActivateOnEnter): what Enter runs in the
// dialog being built when the focus is not on a button of its own.
// fromInputs: also when a text field has the focus and no Enter handler
void DlgSetDefault(gpui::Ctx* cx, gpui::Listener onOk, bool fromInputs = false);
bool DlgDefaultOnEnter(gpui::Ctx* cx, bool editFocused);

// ng: orig's dialogs are windows of their own. A dialog the port builds as a
// gpui Dialog (a card over the frame) can be shown in such a window instead
// where the platform has them (gui/ToolWindow.h), without a layout of its
// own: DlgWindowOpen() makes the window (owned by the main window, which is
// disabled while a modal one is up), the dialog's build function is called
// with the window's context, and DlgIntoEl() - in place of
// Dialog::IntoEl() - then gives the body and the buttons alone. The window is
// as wide as the Dialog says and as tall as that content, centered on the
// main window (orig's HwndCenterDialog).
struct MainWindow;
struct ToolWindow;
struct DlgWindowSpec {
    // how the automation channel names it (TestToolWindow)
    const char* name = nullptr;
    Str (*title)() = nullptr;
    // orig's RunModalWindow; false for one of orig's modeless dialogs, which
    // have no owner
    bool modal = true;
    // a modeless dialog that orig gives an owner: above the main window and
    // closed with it, but the main window stays enabled
    bool owned = false;
    // orig's closeOnEsc
    bool closeOnEsc = true;
    // the dialog's build function; it must build when DlgWindowIsHost(cx)
    gpui::El* (*build)(MainWindow* win, gpui::Ctx* cx) = nullptr;
    // Esc and the close box
    void (*close)() = nullptr;
    // Up / Down for a dialog with a list; true when taken
    bool (*onArrow)(int dir, bool editFocused) = nullptr;
    // Enter outside a text field, for a dialog without a DlgSetDefault()
    void (*onEnter)() = nullptr;
    // every key before anything else sees it, for a dialog that captures a
    // key combination; true when taken. onKeyUp: the same for a key going up
    bool (*onKey)(MainWindow* win, int vk, bool ctrl, bool shift, bool alt) = nullptr;
    void (*onKeyUp)(MainWindow* win, int vk, bool ctrl, bool shift, bool alt) = nullptr;
    // for a dialog without an owner, which orig keeps when its main window
    // closes: the window stays while another main window is left, belongs to
    // `newOwner` from then on, and the dialog is told so
    void (*onOwnerClosed)(MainWindow* newOwner) = nullptr;
    // the client width (dips) when the dialog's build function gives the
    // window orig's layout (DlgWinContent); 0: the Dialog's own width
    float clientDx = 0;
};
// null where the dialog stays in the frame
ToolWindow* DlgWindowOpen(const DlgWindowSpec& spec, MainWindow* win);
// closes it and clears *tw
void DlgWindowClose(ToolWindow** tw);
// true while `cx` is such a window's
bool DlgWindowIsHost(gpui::Ctx* cx);
// the window whose build / close / key callback is running, for a dialog
// that has several windows up at once. Only good for comparing
ToolWindow* DlgWindowCurrent();
gpui::El* DlgIntoEl(gpui::Ctx* cx, gpui::component::Dialog* dlg);

// orig's dialog layout for the content of such a window. Sizes at 96 dpi, as
// orig's layout code has them: the 12 px app font with its 15 px line, a 23
// high edit, 19 high list rows, 25 high buttons (5 / 12 around the label) an
// average character apart
constexpr float kDlgWinFontPx = 12;
constexpr float kDlgWinLineDy = 15;
constexpr float kDlgWinEditDy = 23;
constexpr float kDlgWinRowDy = 19;
constexpr float kDlgWinBtnDy = 25;
constexpr float kDlgWinBtnPadDx = 12;
constexpr float kDlgWinBtnGap = 7;
// sets the window's fonts to orig's and returns the size for Font()
float DlgWinFont(gpui::Ctx* cx);
// the width of `label` (its '&' dropped) in the app font, in dips
float DlgWinTextDx(gpui::Ctx* cx, Str label);
// orig's VirtButton: as wide as its label plus the padding
gpui::El* DlgWinButton(gpui::Ctx* cx, gpui::Str id, Str label, gpui::Listener onClick, bool isDefault,
                       bool disabled = false);
// the selected row of orig's VirtListBox; darker while the list has the focus
gpui::Rgba DlgWinListSelBg(bool listFocused);
// orig's VirtText: one line of `text` (what a DlgAccel*() call returned, or
// plain text) with `padB` under it
gpui::El* DlgWinLabel(gpui::Ctx* cx, gpui::Str text, float font, float padB);
// orig's Edit with a border: 23 high, as wide as its parent
gpui::El* DlgWinEdit(gpui::Ctx* cx, gpui::Str id, gpui::InputState* edit);
// orig's Checkbox: `box` (a gpui Checkbox element) in a 21 high row with
// `padT` above it, the label 2 after the box
constexpr float kDlgWinCheckDy = 21;
gpui::El* DlgWinCheck(gpui::Ctx* cx, gpui::El* box, float padT);
// the row of buttons at the bottom: at the right, `padY` above and below
gpui::El* DlgWinButtonRow(gpui::Ctx* cx, float padY);
// in place of DlgIntoEl(): `col` is the whole content, laid out by the
// dialog; the window gets spec.clientDx and the column's height
gpui::El* DlgWinContent(gpui::Ctx* cx, gpui::El* col);

// the dialog window that has the input of `win`'s program: a modal one, or
// the active one. -dbg-control's TestInput and TestUiState follow it
gpui::Window* DlgWindowInputTarget(MainWindow* win);

// the dropped list of the open editable combo box, if any and if its box is
// in this window. ng: the shell (or a tool window's root) adds it to its
// root, over the dialog, because a dialog clips what is inside it and a combo
// box's list hangs over the dialog's edge
gpui::El* DialogComboListBuild(gpui::Ctx* cx);

// where that list ends (dips from the top of `win`); 0 when none is dropped
// there. A dialog window grows to it, since the list cannot leave the window
float DialogComboListBottom(gpui::Window* win);

// the shell's side
void DlgAccelBeginFrame(gpui::Window* win);
void DlgAccelOnAlt();
bool DlgAccelOnKey(gpui::Ctx* cx, int vk, bool alt, bool ctrl, bool editFocused);

// a gpui Checkbox / Radio / Button with orig's label: sets the stripped label
// and the click handler, registers the access key and underlines it
template <typename T>
gpui::El* DlgAccelEl(gpui::Ctx* cx, T* ctrl, Str label, gpui::Listener onClick, bool disabled = false) {
    gpui::Str text = DlgAccelClick(cx, label, onClick, disabled);
    return DlgAccelUnderline(cx, ctrl->Label(text)->OnClick(onClick)->IntoEl(), text);
}

// gpui's Dialog draws OK / Cancel itself unless the caller gives it a footer,
// and then it draws nothing else. orig's dialogs with an extra button (Help,
// Reset to defaults, Remove, Open Settings File) need the whole row, so this
// puts `extra` on the left and the two standard buttons on the right.
template <typename V>
gpui::El* DialogFooter(gpui::Ctx* cx, gpui::El* extra, gpui::Entity<V> ent, Str okText, Str cancelText,
                       void (*onOk)(V*, gpui::Ctx*, const gpui::ClickEvent*),
                       void (*onCancel)(V*, gpui::Ctx*, const gpui::ClickEvent*)) {
    gpui::El* row = gpui::Div(cx->a)->FlexRow()->W(gpui::kFill)->ItemsCenter()->Gap(8);
    if (extra) {
        row->Child(extra);
    }
    row->Child(gpui::Div(cx->a)->Flex1());
    DlgSetDefault(cx, gpui::ListenTo(ent, onOk), true);
    row->Child(DlgAccelEl(cx, gpui::component::Button::New(cx, GStrL("dlg-cancel"))->WithSize(gpui::UiSize::Small),
                          cancelText, gpui::ListenTo(ent, onCancel)));
    row->Child(DlgAccelEl(cx,
                          gpui::component::Button::New(cx, GStrL("dlg-ok"))->Primary()->WithSize(gpui::UiSize::Small),
                          okText, gpui::ListenTo(ent, onOk)));
    return row;
}
