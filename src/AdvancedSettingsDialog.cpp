/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// A dialog for editing advanced settings, driven by the settings metadata
// (gSettingsInfo). Shows a filterable list of settings; clicking a bool
// toggles it, clicking an enum cycles its allowed values, clicking a string /
// color / number setting edits it in-place (Enter confirms, Esc cancels).
// Save writes the settings file and reloads it (so all derived state is
// re-computed); Cancel abandons the changes.

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/UITask.h"
#include "base/SettingsUtil.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "AppTools.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "MainWindow.h"
#include "PdfDarkMode.h"
#include "Theme.h"
#include "DarkMode.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "FilterHighlightDraw.h"
#include "SumatraDialogs.h"
#include "RestrictedUseConfig.h"
#include "AdvancedSettingsDialogCommon.h"

// the dialog is drawn like the command palette: a bare filter line, 28 px
// rows on an 8 px inset with a rounded selection, 16 px side padding
constexpr int kAdvQueryFontPx = 15;
constexpr int kAdvTextFontPx = 13;
constexpr int kAdvMetaFontPx = 12;
constexpr int kAdvHintFontPx = 11;
constexpr int kAdvRowDy = 24;
constexpr int kAdvInsetDx = 8;
constexpr int kAdvPadDx = 16;
constexpr int kAdvGapDx = 8;
constexpr int kAdvQueryPadDy = 6;
constexpr int kAdvListPadDy = 4;
constexpr int kAdvHelpPadDy = 8;
constexpr int kAdvHelpGapDy = 3;
constexpr int kAdvHelpLines = 4;
constexpr int kAdvFootPadDy = 5;
constexpr int kAdvRadius = 4;
constexpr int kAdvSelShift = 28;
constexpr int kAdvFaintShift = 25;
constexpr int kAdvHeaderShiftDy = 4;
constexpr int kAdvSwatchDx = 12;
constexpr int kAdvDotDx = 6;

static PlatformFont* UiFont(int px) {
    return DlgUiFont(px);
}

struct ListBoxModelSettings : ListBoxModel {
    Vec<SettingItem*>* items = nullptr;       // not owned
    Vec<SettingSection*>* sections = nullptr; // not owned
    Vec<SettingRow> rows;

    ~ListBoxModelSettings() override = default;
    int ItemsCount() override { return len(rows); }
    Str Item(int i) override {
        const SettingRow& row = rows[i];
        if (row.itemIdx < 0) {
            return (*sections)[row.sectionIdx]->name;
        }
        return SettingRowName(*items, row);
    }
    // null for a section header
    SettingItem* ItemAt(int i) {
        if (i < 0 || i >= len(rows) || rows[i].itemIdx < 0) {
            return nullptr;
        }
        return (*items)[rows[i].itemIdx];
    }
};

// a header row takes no click
struct AdvSettingsListBox : VirtListBox {
    void OnAdvMouseDown(VirtMouseEvent* ev);
};

void AdvSettingsListBox::OnAdvMouseDown(VirtMouseEvent* ev) {
    int idx = ItemFromPoint(ev->pt);
    auto* m = (ListBoxModelSettings*)model;
    if (idx >= 0 && m && !m->ItemAt(idx)) {
        ev->didHandle = true;
        return;
    }
    VirtListBox::OnMouseDown(ev);
}

struct AdvancedSettingsWnd : WindowBase {
    ~AdvancedSettingsWnd() override;

    PlatformFont* fontBold = nullptr;
    MainWindow* win = nullptr;

    // the list, the help strip, the hints and the buttons are virtual
    // controls; the filter and the in-place editors are real HWNDs
    Edit* editFilter = nullptr;
    VirtText* countText = nullptr; // "212 settings" at the right end of the filter line
    VirtListBox* listBox = nullptr;
    ListBoxModelSettings* model = nullptr; // owned by listBox
    int prevSel = -1;                      // where the selection came from, to step over a header
    // the help strip: the selected setting's name, its default and its doc comment
    VirtText* helpName = nullptr;
    VirtText* helpDefault = nullptr;
    VirtFixedLinesText* commentText = nullptr;
    // the footer: the key hints for the edit state, "Unsaved changes: n" (collapsed at 0)
    HBox* hintRow = nullptr;
    int hintState = -1;
    ILayout* unsavedRow = nullptr;
    VirtCustom* unsavedDot = nullptr;
    VirtText* unsavedText = nullptr;
    Edit* editValue = nullptr;         // in-place value editor, created on demand
    DropDown* dropDownValue = nullptr; // in-place enum editor, created on demand
    Str dropDownOrigVal;               // value before the drop-down opened (owned), for Esc
    int editItemIdx = -1;              // index into items of the setting being edited
    // Create/show/focus of the in-place editor can re-enter CancelEditValue
    // (filter EN_CHANGE, selection change) while the local editor pointer is
    // still in use — that was a heap UAF (CloseEnumEdit delete). While set,
    // CancelEditValue / CloseEnumEdit only mark pendingCancelEdit and return.
    bool editCreating = false;
    bool pendingCancelEdit = false;

    // bottom buttons (layout-owned; Tab order Save → Cancel → Open → Help)
    VirtButton* btnSave = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnOpenSettingsFile = nullptr;
    VirtButton* btnHelp = nullptr;

    // filter tokens for list matching + match underlay paint (command-palette style)
    StrVec filterWords;
    Vec<u8> highlighted; // scratch for DrawMaybeHighlightedText

    Vec<SettingItem*> items;
    Vec<SettingSection*> sections;

    bool Create(MainWindow* win);
    void OnKeyDown(KeyEvent* ev);
    bool HandleEscapeKey(bool isEditingValue, bool isEditingEnum);
    bool HandleTabKey(bool isEditingValue, bool isEditingEnum);
    bool HandleEnterKey(bool isEditingValue, bool isEditingEnum);
    bool HandleUpDownKey(const KeyEvent& ev);
    void OnSize(WindowBase::SizeEvent* ev);
    void OnGetMinMaxInfo(WindowBase::GetMinMaxInfoEvent* ev);

    void QueryChanged();
    int CountChangedSettings() const;
    void UpdateUnsavedRow();
    void UpdateHints();
    void NoteItemEdited(SettingItem* item);
    void DrawListBoxItem(VirtListBox::DrawItemEvent* ev);
    void ActivateItem(int lbIdx);
    void OnItemDoubleClicked();
    void OnSelectionChanged();

    Rect ValueRectForItem(int idx); // in listBox client coords
    void BeginEditValue(int idx);
    bool RestoreSelectedDefault(bool isEditingValue, bool isEditingEnum);
    void CommitEditValue();
    void CancelEditValue();

    void BeginEditEnum(int idx);
    void OnEnumSelectionChanged();
    void OnEnumDropDownClosed();
    void CheckDropDownClosed();
    void CloseEnumEdit(bool keepValue);
    void KeepFocus();
    void RelayoutInPlaceEditor();
    void OnEditValueKillFocus();
    void PostedRefocusEditValue();

    void OnOpenSettingsFile(VirtMouseEvent* ev = nullptr);
    void OnHelp(VirtMouseEvent* ev = nullptr);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnSave(VirtMouseEvent* ev = nullptr);

    void ApplyChangesAndSave();
};

static AdvancedSettingsWnd* gAdvancedSettingsWnd = nullptr;

AdvancedSettingsWnd::~AdvancedSettingsWnd() {
    // editFilter, listBox and commentText are added to the layout; VBox owns
    // and deletes its children, and the base WindowBase::~WindowBase() deletes `layout`, so
    // they must not be deleted here (doing so is a double-free). editValue and
    // dropDownValue are created on demand and are not part of the layout, so
    // they are freed explicitly.
    // covers every way the dialog goes away (Save, Cancel, Esc, the close box).
    // On Save the value is in gSettings by now, so dropping the preview
    // renders the same thing; on every other path it undoes the preview.
    EndPreviewSettingChange();
    delete editValue;
    delete dropDownValue;
    DeleteVecMembers(items);
    DeleteVecMembers(sections);
    str::Free(dropDownOrigVal);
}

static void ClearAdvancedSettingsWnd() {
    gAdvancedSettingsWnd = nullptr;
}

void AdvancedSettingsWnd::QueryChanged() {
    CancelEditValue();
    Str filter = editFilter->GetTextTemp();
    // split trims leading/trailing/internal runs of whitespace into words
    filterWords.Reset();
    SplitFilterToWords(filter, filterWords);
    CollectFilteredSettings(items, filterWords, model->rows);
    listBox->SetModel(model); // resets selection to -1
    prevSel = -1;
    if (countText) {
        countText->SetText(AdvSettingsCountTemp(CountSettingRows(model->rows), len(items)));
        if (layout) {
            DoLayout();
        }
    }
    OnSelectionChanged();
}

// show the selected setting's name, default and doc comment in the help strip
void AdvancedSettingsWnd::OnSelectionChanged() {
    int lbSel = listBox->GetCurrentSelection();
    // a header is not a row to land on: step over it the way the selection was going
    if (lbSel >= 0 && !model->ItemAt(lbSel)) {
        int dir = lbSel >= prevSel ? 1 : -1;
        int next = NextSettingRow(model->rows, lbSel, dir);
        if (next < 0) {
            next = NextSettingRow(model->rows, lbSel, -dir);
        }
        listBox->SetCurrentSelection(next);
        if (next >= 0) {
            listBox->EnsureVisible(next);
        }
        lbSel = next;
    }
    prevSel = lbSel;
    int selItemIdx = lbSel >= 0 ? model->rows[lbSel].itemIdx : -1;
    // an in-place editor stays keyed to editItemIdx; when the selection moves to
    // a different setting, dismiss the editor (keeping the value). Guarding on
    // the index (rather than dismissing unconditionally) is important: creating
    // an editor can synchronously re-enter here (focus change / CB_SHOWDROPDOWN)
    // while the selection is still on the item being edited, and dismissing then
    // would free the editor mid-construction.
    if (editItemIdx >= 0 && editItemIdx != selItemIdx) {
        CommitEditValue();
        CloseEnumEdit(true);
    }
    if (!commentText) {
        return;
    }
    SettingItem* item = model->ItemAt(lbSel);
    commentText->SetText(item ? item->comment : Str(""));
    helpName->SetText(item ? item->name : Str(""));
    helpDefault->SetText(item ? Str(AdvSettingsDefaultTemp(item)) : Str(""));
    if (layout) {
        DoLayout(); // the default's width changed
    }
    HwndInvalidate(hwnd);
}

// Split a list-item row into non-overlapping name (left) and value (right)
// columns so long names and long values (e.g. InverseSearchCmdLine) don't
// draw on top of each other (#5804).
static void AdvSettingsItemColumns(const Rect& rc, Rect& rcName, Rect& rcVal) {
    int pad = DpiScale(kAdvInsetDx);
    int gap = DpiScale(10);
    int totalW = rc.dx - (2 * pad);
    if (totalW < 1) {
        rcName = rc;
        rcVal = rc;
        return;
    }
    // Value column ~45% (min 100px); name gets the rest. Both are clipped with
    // ellipsis when the dialog is narrow.
    int minVal = DpiScale(100);
    int valW = std::max(totalW * 45 / 100, minVal);
    valW = std::min(valW, totalW * 3 / 5);
    int nameW = totalW - valW - gap;
    if (nameW < DpiScale(72)) {
        nameW = (totalW / 2) - (gap / 2);
        valW = totalW - nameW - gap;
    }
    nameW = std::max(nameW, 1);
    valW = std::max(valW, 1);

    rcName = rc;
    rcName.x += pad;
    rcName.dx = nameW;

    rcVal = rc;
    rcVal.x += rcVal.dx - pad - valW;
    rcVal.dx = valW;
}

// a header row: the section's name with its comment fainter after it, set
// low in the row so it reads as the title of the rows under it
static void DrawSectionRow(Gfx* gfx, Rect rc, SettingSection* sec, Color colMuted, bool isRtl) {
    PlatformFont* font = UiFont(kAdvMetaFontPx);
    int shift = DpiScale(kAdvHeaderShiftDy);
    Rect r = rc;
    r.SubLR(DpiScale(kAdvInsetDx), DpiScale(kAdvInsetDx));
    r.y += shift;
    r.dy -= shift;
    u32 flags = gfxTextVCenter | gfxTextEllipsis | (isRtl ? (gfxTextRight | gfxTextRtl) : gfxTextLeft);
    int nameDx = std::min(gfx->MeasureText(sec->name, font).dx, r.dx);
    Rect rName = r;
    rName.dx = nameDx;
    if (isRtl) {
        rName.x = r.Right() - nameDx;
    }
    gfx->DrawText(sec->name, rName, flags, font, colMuted);
    int gap = DpiScale(kAdvGapDx);
    Rect rComment = r;
    rComment.dx = r.dx - nameDx - gap;
    if (!isRtl) {
        rComment.x = rName.Right() + gap;
    }
    if (len(sec->comment) > 0 && rComment.dx > 0) {
        gfx->DrawText(sec->comment, rComment, flags, font, AccentColor(colMuted, kAdvFaintShift));
    }
}

void AdvancedSettingsWnd::DrawListBoxItem(VirtListBox::DrawItemEvent* ev) {
    VirtListBox* lb = ev->listBox;
    auto* m = (ListBoxModelSettings*)lb->model;
    if (ev->itemIndex < 0 || ev->itemIndex >= len(m->rows)) {
        return;
    }
    const SettingRow& row = m->rows[ev->itemIndex];

    Gfx* gfx = ev->gfx;
    Rect rc = ev->itemRect;

    Color colBg = lb->GetColor(kColListBg);
    Color colText = lb->GetColor(kColListText);
    if (IsSpecialColor(colBg)) {
        colBg = GetSysColor(COLOR_WINDOW);
    }
    if (IsSpecialColor(colText)) {
        colText = GetSysColor(COLOR_WINDOWTEXT);
    }
    Color colMuted = ThemeWindowTextDisabledColor();
    gfx->FillRect(rc, colBg);
    bool isRtl = HwndIsRtl(lb->GetHwnd());

    if (row.itemIdx < 0) {
        DrawSectionRow(gfx, rc, (*m->sections)[row.sectionIdx], colMuted, isRtl);
        return;
    }
    SettingItem* item = (*m->items)[row.itemIdx];
    if (ev->selected) {
        colBg = AccentColor(colBg, kAdvSelShift);
        gfx->FillRoundedRect(rc, DpiScale(kAdvRadius), colBg);
    }

    PlatformFont* font = lb->font;
    Rect rcName{};
    Rect rcVal{};
    AdvSettingsItemColumns(rc, rcName, rcVal);

    // yellow/accent underlays on matched filter tokens (same as command palette)
    u32 nameFmt = gfxTextEllipsis | gfxTextVCenter;
    nameFmt |= isRtl ? (gfxTextRight | gfxTextRtl) : gfxTextLeft;
    DrawMaybeHighlightedText(gfx, rcName, SettingRowName(*m->items, row), filterWords, highlighted, colBg, isRtl, false,
                             nameFmt, font, colText);

    // an in-place editor covers the value; only its underline shows
    if (row.itemIdx == editItemIdx) {
        gfx->FillRect({rcVal.x, rcVal.Bottom() - 1, rcVal.dx, 1}, colText);
        return;
    }

    // the value: muted when it is the default, the text color when not. A
    // color setting gets a swatch, an unsaved edit a yellow dot, before it
    TempStr val = FormatSettingValueTemp(item);
    Color valCol = SettingDiffersFromDefault(item) ? colText : colMuted;
    int valDx = std::min(gfx->MeasureText(val, font).dx, rcVal.dx);
    Rect r = rcVal;
    r.dx = valDx;
    if (!isRtl) {
        r.x = rcVal.Right() - valDx;
    }
    gfx->DrawText(val, r, gfxTextEllipsis | gfxTextVCenter | (isRtl ? gfxTextLeft : gfxTextRight), font, valCol);

    // the marks go on the name's side of the value
    int gap = DpiScale(kAdvGapDx);
    int x = isRtl ? r.Right() : r.x;
    auto markRect = [&](int dx) -> Rect {
        x += isRtl ? gap : -(gap + dx);
        Rect res{x, rc.y + (rc.dy - dx) / 2, dx, dx};
        if (isRtl) {
            x += dx;
        }
        return res;
    };
    Color swatch;
    if (item->type == SettingType::Color && ParseColor(&swatch, item->strVal)) {
        Rect rs = markRect(DpiScale(kAdvSwatchDx));
        gfx->FillRect(rs, swatch);
        gfx->DrawRect(rs, ThemeEdgeColor());
    }
    if (item->changed) {
        gfx->FillEllipse(markRect(DpiScale(kAdvDotDx)), ThemeAccentColor());
    }
}

// in-place editor sits in the value column (same split as DrawListBoxItem)
// in window (dialog client) coords, which is where the in-place editors go
Rect AdvancedSettingsWnd::ValueRectForItem(int idx) {
    int lbIdx = FindSettingRow(model->rows, idx);
    if (lbIdx < 0) {
        return {};
    }
    Rect rc = listBox->ItemRect(lbIdx);
    if (rc.IsEmpty()) {
        return {};
    }
    Rect rcName{};
    Rect rcVal{};
    AdvSettingsItemColumns(rc, rcName, rcVal);
    return rcVal;
}

void AdvancedSettingsWnd::BeginEditValue(int idx) {
    CancelEditValue();
    SettingItem* item = items[idx];
    Rect r = ValueRectForItem(idx);
    if (r.IsEmpty()) {
        return;
    }

    // the row draws the underline in the strip the edit leaves free
    r.dy -= DpiScale(2);
    Edit::CreateArgs args;
    args.parent = hwnd;
    args.isMultiLine = false;
    args.withBorder = false;
    args.font = listBox->font;
    args.text = FormatSettingValueTemp(item);
    auto* c = new Edit();
    c->SetColors(ThemeWindowTextColor(), AccentColor(ThemeWindowControlBackgroundColor(), kAdvSelShift));
    // Suppress re-entrant CancelEditValue while Create/show/focus pump messages.
    editCreating = true;
    pendingCancelEdit = false;
    HWND ok = c->Create(args);
    if (!ok) {
        editCreating = false;
        delete c;
        if (pendingCancelEdit) {
            pendingCancelEdit = false;
            CancelEditValue();
        }
        return;
    }
    editValue = c;
    editItemIdx = idx;
    c->onKillFocus = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnEditValueKillFocus>(this);
    SetWindowPos(c->hwnd, HWND_TOP, r.x, r.y, r.dx, r.dy, SWP_SHOWWINDOW);
    EditSelectAll(c);
    EditSetFocus(c);
    listBox->Invalidate();
    UpdateHints();
    editCreating = false;
    if (pendingCancelEdit) {
        pendingCancelEdit = false;
        CancelEditValue();
    }
}

void AdvancedSettingsWnd::CancelEditValue() {
    if (editCreating) {
        // Called re-entrantly from Create/show/focus of BeginEdit*; finish
        // construction first, then cancel (avoids UAF on the editor being built).
        pendingCancelEdit = true;
        return;
    }
    CloseEnumEdit(true);
    if (!editValue) {
        return;
    }
    auto* tmp = editValue;
    editValue = nullptr;
    editItemIdx = -1;
    delete tmp;
    listBox->Invalidate();
    UpdateHints();
    SetFocusTo(listBox);
}

// show a drop-down with the allowed values over the item's value rect
void AdvancedSettingsWnd::BeginEditEnum(int idx) {
    CancelEditValue();
    SettingItem* item = items[idx];
    Rect r = ValueRectForItem(idx);
    if (r.IsEmpty()) {
        return;
    }
    DropDown::CreateArgs args;
    args.parent = hwnd;
    args.font = GetFont();
    auto* c = new DropDown();
    // Suppress re-entrant CancelEditValue / CloseEnumEdit while Create and
    // CB_SHOWDROPDOWN pump messages (filter EN_CHANGE was freeing c mid-flight).
    editCreating = true;
    pendingCancelEdit = false;
    HWND ok = c->Create(args);
    if (!ok) {
        editCreating = false;
        delete c;
        if (pendingCancelEdit) {
            pendingCancelEdit = false;
            CancelEditValue();
        }
        return;
    }
    StrVec vals;
    int currSel = 0;
    for (int i = 0; item->enumValues[i]; i++) {
        vals.Append(Str(item->enumValues[i]));
        if (str::EqI(item->strVal, Str(item->enumValues[i]))) {
            currSel = i;
        }
    }
    c->SetItems(vals);
    CbSetCurrentSelection(c, currSel);
    c->onSelectionChanged = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnEnumSelectionChanged>(this);
    c->onCloseUp = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnEnumDropDownClosed>(this);
    dropDownValue = c;
    editItemIdx = idx;
    str::ReplaceWithCopy(&dropDownOrigVal, item->strVal);
    SetWindowPos(c->hwnd, HWND_TOP, r.x, r.y, r.dx, r.dy, SWP_SHOWWINDOW);
    HwndSetFocus(c->hwnd);
    SendMessageW(c->hwnd, CB_SHOWDROPDOWN, TRUE, 0);
    UpdateHints();
    editCreating = false;
    if (pendingCancelEdit) {
        pendingCancelEdit = false;
        CancelEditValue();
    }
}

// Previewing a value (or the combo list closing) can activate the main
// window. Put the user back in this dialog; if the list is still open,
// leave the caret on the combo.
void AdvancedSettingsWnd::KeepFocus() {
    if (gAdvancedSettingsWnd != this || !hwnd) {
        return;
    }
    HwndToForeground(hwnd);
    HWND focused = ::GetFocus();
    if (focused == hwnd || ::IsChild(hwnd, focused)) {
        return;
    }
    if (dropDownValue && dropDownValue->hwnd) {
        HwndSetFocus(dropDownValue->hwnd);
        return;
    }
    HwndSetFocus(hwnd);
}

void AdvancedSettingsWnd::RelayoutInPlaceEditor() {
    if (editItemIdx < 0) {
        return;
    }
    Rect r = ValueRectForItem(editItemIdx);
    if (r.IsEmpty()) {
        return;
    }
    HWND h = nullptr;
    if (editValue) {
        h = editValue->hwnd;
    } else if (dropDownValue) {
        h = dropDownValue->hwnd;
    }
    if (h) {
        SetWindowPos(h, HWND_TOP, r.x, r.y, r.dx, r.dy, SWP_NOACTIVATE);
    }
}

void AdvancedSettingsWnd::PostedRefocusEditValue() {
    if (gAdvancedSettingsWnd != this || !editValue || !editValue->hwnd) {
        return;
    }
    if (GetFocus() == editValue->hwnd) {
        return;
    }
    HwndSetFocus(editValue->hwnd);
}

void AdvancedSettingsWnd::OnEditValueKillFocus() {
    if (!editValue) {
        return;
    }
    HWND next = GetFocus();
    if (next == editValue->hwnd) {
        return;
    }
    if (!next || HwndIsOnScreenKeyboard(next)) {
        auto fn = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::PostedRefocusEditValue>(this);
        uitask::Post(fn, "AdvSettingsRefocusEdit");
        return;
    }
    if (hwnd && (next == hwnd || IsChild(hwnd, next))) {
        return;
    }
    CommitEditValue();
}

void AdvancedSettingsWnd::OnEnumSelectionChanged() {
    if (!dropDownValue) {
        return;
    }
    SettingItem* item = items[editItemIdx];
    int sel = CbGetCurrentSelection(dropDownValue);
    if (sel >= 0) {
        str::ReplaceWithCopy(&item->strVal, Str(item->enumValues[sel]));
        NoteItemEdited(item);
        // browsing the list with the arrow keys previews each value in turn
        PreviewSettingChange(item);
        listBox->Invalidate();
    }
    KeepFocus();
    // selecting with the mouse closes the list: dispose of the control then.
    // can't do it here (we're inside its notification), so check afterwards;
    // during keyboard browsing the list stays open and the control stays up
    auto fn = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::CheckDropDownClosed>(this);
    uitask::Post(fn, "AdvSettingsCheckDropDownClosed");
}

void AdvancedSettingsWnd::OnEnumDropDownClosed() {
    // CBN_CLOSEUP is when Windows hands activation to whoever it thinks
    // owned the list (often the main window). Take it back now, not after
    // a posted task that runs too late.
    KeepFocus();
    auto fn = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::CheckDropDownClosed>(this);
    uitask::Post(fn, "AdvSettingsCheckDropDownClosed");
}

void AdvancedSettingsWnd::CheckDropDownClosed() {
    if (gAdvancedSettingsWnd != this || !dropDownValue) {
        return;
    }
    bool droppedDown = SendMessageW(dropDownValue->hwnd, CB_GETDROPPEDSTATE, 0, 0) != 0;
    if (!droppedDown) {
        CloseEnumEdit(true);
    }
}

void AdvancedSettingsWnd::CloseEnumEdit(bool keepValue) {
    if (editCreating) {
        pendingCancelEdit = true;
        return;
    }
    if (!dropDownValue) {
        return;
    }
    auto* tmp = dropDownValue;
    dropDownValue = nullptr;
    if (!keepValue) {
        SettingItem* item = items[editItemIdx];
        str::ReplaceWithCopy(&item->strVal, dropDownOrigVal);
        NoteItemEdited(item);
        PreviewSettingChange(item);
    }
    editItemIdx = -1;
    delete tmp;
    listBox->Invalidate();
    UpdateHints();
    KeepFocus();
    SetFocusTo(listBox);
    // destroying the combo can activate the main window after we return
    auto fn = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::KeepFocus>(this);
    uitask::Post(fn, "AdvSettingsKeepFocus");
}

void AdvancedSettingsWnd::CommitEditValue() {
    if (!editValue) {
        return;
    }
    SettingItem* item = items[editItemIdx];
    Str s = editValue->GetTextTemp();
    switch (item->type) {
        case SettingType::Int:
            item->intVal = ParseInt(s);
            break;
        case SettingType::Float: {
            float f = item->floatVal;
            str::Parse(s, "%f", &f);
            item->floatVal = f;
            break;
        }
        default:
            str::ReplaceWithCopy(&item->strVal, s);
            break;
    }
    NoteItemEdited(item);
    PreviewSettingChange(item);
    CancelEditValue();
    listBox->Invalidate();
}

// activate a setting: toggle a bool, or begin editing an enum / value. A single
// click only selects (so a stray click doesn't change anything); activation
// happens on double-click or Enter.
void AdvancedSettingsWnd::ActivateItem(int lbIdx) {
    SettingItem* item = model->ItemAt(lbIdx);
    if (!item) {
        return;
    }
    if (item->type == SettingType::Bool) {
        item->boolVal = !item->boolVal;
        NoteItemEdited(item);
        listBox->Invalidate();
        return;
    }
    int idx = model->rows[lbIdx].itemIdx;
    if (item->enumValues) {
        BeginEditEnum(idx);
        return;
    }
    BeginEditValue(idx);
}

void AdvancedSettingsWnd::OnItemDoubleClicked() {
    ActivateItem(listBox->GetCurrentSelection());
}

void AdvancedSettingsWnd::ApplyChangesAndSave() {
    // snapshot settings that need explicit apply (tabs, menu bar ...) before we
    // overwrite them, so we can act on what actually changed after the reload
    SettingsApplyState before = GetSettingsApplyState();
    bool didChange = false;
    for (SettingItem* item : items) {
        if (!item->changed) {
            continue;
        }
        didChange = true;
        u8* p = SettingFieldPtr(item->fieldOffset);
        switch (item->type) {
            case SettingType::Bool:
                *(bool*)p = item->boolVal;
                break;
            case SettingType::Int:
                *(int*)p = item->intVal;
                break;
            case SettingType::Float:
                *(float*)p = item->floatVal;
                break;
            case SettingType::Compact:
                ApplyCompactInts(item->compactInfo, p, item->strVal);
                break;
            default:
                str::ReplaceWithCopy((Str*)p, item->strVal);
                break;
        }
        // SaveSettings() re-generates these strings from their parsed
        // representations, which would clobber the edit unless the parsed
        // representation is updated as well
        if (str::EqI(item->name, StrL("DefaultDisplayMode"))) {
            gSettings->defaultDisplayModeEnum = DisplayModeFromString(item->strVal, DisplayMode::Automatic);
        } else if (str::EqI(item->name, StrL("DefaultZoom"))) {
            gSettings->defaultZoomFloat = ZoomFromString(item->strVal, kZoomActualSize);
        } else if (str::EqI(item->name, StrL("ImageUI.DefaultZoom"))) {
            gSettings->imageUI.defaultZoomFloat = ZoomFromString(item->strVal, 0);
        } else if (str::EqI(item->name, StrL("ComicBookUI.DefaultZoom"))) {
            gSettings->comicBookUI.defaultZoomFloat = ZoomFromString(item->strVal, 0);
        }
    }
    if (!didChange) {
        return;
    }
    ScheduleSaveSettings();
    // reload so that all state derived from settings (theme, fonts, parsed
    // colors, custom commands, accelerators ...) is re-computed and applied
    ForceReloadSettings();
    // apply changes that a reload doesn't pick up on its own (menu bar, tabs,
    // anti-alias ...) and re-layout the open windows
    ApplyChangedSettingsAndRelayout(before);
}

// opens the settings file in the editor associated with .txt files
void OpenSettingsFileInEditor() {
    if (!CanAccessDisk()) {
        return;
    }
    TempStr path = GetSettingsPathTemp();
    LaunchFileIfExists(path);
}

void AdvancedSettingsWnd::OnOpenSettingsFile(VirtMouseEvent*) {
    OpenSettingsFileInEditor();
}

void AdvancedSettingsWnd::OnHelp(VirtMouseEvent*) {
    SumatraLaunchBrowser(Str(kSettingsDocsUrl));
}

void AdvancedSettingsWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

void AdvancedSettingsWnd::OnSave(VirtMouseEvent*) {
    CommitEditValue();
    // queue the dialog teardown first: ApplyChangesAndSave() may post a tabs
    // transition that closes/recreates windows (including this dialog's owner),
    // and uitask runs FIFO, so the delete must be enqueued before it
    ScheduleDelete();
    ApplyChangesAndSave();
}

// Esc cancels an in-place edit; it only closes the dialog when nothing is unsaved.
bool AdvancedSettingsWnd::HandleEscapeKey(bool isEditingValue, bool isEditingEnum) {
    if (isEditingValue) {
        CancelEditValue();
    } else if (isEditingEnum) {
        CloseEnumEdit(false);
    } else if (CountChangedSettings() == 0) {
        ScheduleDelete();
    }
    return true;
}

bool AdvancedSettingsWnd::HandleTabKey(bool isEditingValue, bool isEditingEnum) {
    // leave in-place editors before moving focus (Enter would commit too)
    if (isEditingValue) {
        CommitEditValue();
    } else if (isEditingEnum) {
        CloseEnumEdit(true);
    }
    // the ring is the layout order: filter → list → Open → Help → Cancel → Save
    TabNavigate(IsShiftPressed());
    return true;
}

bool AdvancedSettingsWnd::HandleEnterKey(bool isEditingValue, bool isEditingEnum) {
    if (isEditingValue) {
        CommitEditValue();
        return true;
    }
    if (isEditingEnum) {
        CloseEnumEdit(true);
        return true;
    }
    // focused button: Enter = click (OnKeyDown would otherwise steal
    // Enter for list-item activation)
    LRESULT res = 0;
    if (vroot && vroot->focused && VirtTreeOnMessage(hwnd, vroot, WM_KEYDOWN, VK_RETURN, 0, res)) {
        return true;
    }
    // Activate the selected row (bool: toggle true↔false; else edit).
    // Do not require list focus: after filter/arrow navigation the
    // selection can be valid while focus is still on the filter.
    int lbIdx = listBox->GetCurrentSelection();
    if (lbIdx < 0) {
        return false;
    }
    ActivateItem(lbIdx);
    return true;
}

// Delete puts the selected setting's default back; while a value is being
// typed it edits that text instead
bool AdvancedSettingsWnd::RestoreSelectedDefault(bool isEditingValue, bool isEditingEnum) {
    if (isEditingValue) {
        return false;
    }
    int lbIdx = listBox->GetCurrentSelection();
    SettingItem* item = model->ItemAt(lbIdx);
    if (!item) {
        return false;
    }
    if (isEditingEnum) {
        CloseEnumEdit(false);
    }
    RestoreSettingDefault(item);
    NoteItemEdited(item);
    listBox->Invalidate();
    return true;
}

// up/down in the filter edit moves the list selection (wrapping)
bool AdvancedSettingsWnd::HandleUpDownKey(const KeyEvent& ev) {
    int dir = 0;
    if (ev.vkey == VK_UP) {
        dir = -1;
    } else if (ev.vkey == VK_DOWN) {
        dir = 1;
    }
    if (dir == 0 || ev.hwnd != editFilter->hwnd) {
        return false;
    }
    int n = listBox->ItemsCount();
    if (n > 0) {
        int sel = (listBox->GetCurrentSelection() + dir + n) % n;
        listBox->SetCurrentSelection(sel);
        OnSelectionChanged(); // programmatic selection: no LBN_SELCHANGE
    }
    return true;
}

void AdvancedSettingsWnd::OnKeyDown(KeyEvent* ev) {
    // a single click only selects; activation (toggle / edit) is on double-click
    // (OnItemDoubleClicked) or Enter
    bool isEditingValue = editValue && ev->hwnd == editValue->hwnd;
    bool isEditingEnum = dropDownValue && ev->hwnd == dropDownValue->hwnd;
    if (ev->vkey == VK_ESCAPE) {
        ev->didHandle = HandleEscapeKey(isEditingValue, isEditingEnum);
        return;
    }
    if (ev->vkey == VK_TAB) {
        ev->didHandle = HandleTabKey(isEditingValue, isEditingEnum);
        return;
    }
    if (ev->vkey == VK_RETURN) {
        ev->didHandle = HandleEnterKey(isEditingValue, isEditingEnum);
        return;
    }
    if (ev->vkey == VK_DELETE) {
        ev->didHandle = RestoreSelectedDefault(isEditingValue, isEditingEnum);
        return;
    }
    if (HandleUpDownKey(*ev)) {
        ev->didHandle = true;
    }
}

// clicking the window's close box sends WM_CLOSE. We must schedule our own
// teardown here: the framework's default WM_CLOSE handler calls WindowBase::Destroy(),
// which removes the WindowBase from the hwnd->WindowBase list *before* DestroyWindow(), so the
// following WM_DESTROY never reaches onDestroy - leaving gAdvancedSettingsWnd
// dangling and blocking reopen. CloseEvent::didHandle defaults to true, so
// returning from here skips that default Destroy().
static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    if (gAdvancedSettingsWnd) {
        gAdvancedSettingsWnd->ScheduleDelete();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    if (gAdvancedSettingsWnd) {
        gAdvancedSettingsWnd->ScheduleDelete();
    }
}

// last client size chosen by the user (or initial layout); reused when the
// dialog is reopened in the same process so a widened window sticks (#5804)
static int gAdvSettingsLastClientDx = 0;
static int gAdvSettingsLastClientDy = 0;

// re-layout the controls when the (resizable) window is resized
void AdvancedSettingsWnd::OnSize(WindowBase::SizeEvent* ev) {
    // a WS_CAPTION/WS_THICKFRAME window gets WM_SIZE during CreateCustom,
    // before the child controls exist; ignore layout until they're created
    if (!layout || !listBox) {
        return;
    }
    int dx = ev->size.dx;
    int dy = ev->size.dy;
    if (dx == 0 || dy == 0) {
        return;
    }
    gAdvSettingsLastClientDx = dx;
    gAdvSettingsLastClientDy = dy;
    DoLayout({dx, dy});
    RelayoutInPlaceEditor();
    HwndInvalidate(hwnd);
}

void AdvancedSettingsWnd::OnGetMinMaxInfo(WindowBase::GetMinMaxInfoEvent* ev) {
    if (!ev->mmi) {
        return;
    }
    int minClient = DpiScale(kAdvSettingsMinClientDx);
    int chromeX = 0;
    if (hwnd) {
        Rect wr = HwndWindowRect(hwnd);
        Rect cr = HwndClientRect(hwnd);
        if (cr.dx > 0 && wr.dx > cr.dx) {
            chromeX = wr.dx - cr.dx;
        }
    }
    ev->mmi->ptMinTrackSize.x = minClient + chromeX;
}

int AdvancedSettingsWnd::CountChangedSettings() const {
    int n = 0;
    for (SettingItem* item : items) {
        if (item->changed) {
            n++;
        }
    }
    return n;
}

// "Unsaved changes: n" with a yellow dot sits in the footer before Cancel,
// hidden until at least one value has been edited since the dialog opened
void AdvancedSettingsWnd::UpdateUnsavedRow() {
    if (!unsavedRow || !unsavedText) {
        return;
    }
    int n = CountChangedSettings();
    bool show = n > 0;
    if (show) {
        unsavedText->SetText(fmt(Tr("Unsaved changes: %d").s, n));
    }
    unsavedRow->SetVisibility(show ? Visibility::Visible : Visibility::Collapse);
    unsavedText->SetIsVisible(show);
    unsavedDot->SetIsVisible(show);
    if (hwnd && layout) {
        DoLayout();
        HwndRepaintNow(hwnd);
    }
}

// the footer's key hints follow what the keys do: change a value, confirm a
// typed one, pick from the enum's list
void AdvancedSettingsWnd::UpdateHints() {
    if (!hintRow) {
        return;
    }
    int state = editValue ? kAdvEditValue : (dropDownValue ? kAdvEditEnum : kAdvEditNone);
    if (state == hintState) {
        return;
    }
    hintState = state;
    for (auto& c : hintRow->children) {
        delete c.layout;
    }
    VecReset(hintRow->children);
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    AdvHint hints[kAdvMaxHints];
    int n = AdvSettingsHints(state, hints);
    for (int i = 0; i < n; i++) {
        hintRow->AddChild(NewKeyHint(UiFont(kAdvHintFontPx), hints[i].key, hints[i].desc, colMuted, colBg));
    }
    if (hwnd && layout) {
        DoLayout();
        HwndInvalidate(hwnd);
    }
}

void AdvancedSettingsWnd::NoteItemEdited(SettingItem* item) {
    SetItemChanged(item);
    UpdateUnsavedRow();
}

// center the dialog over the main window frame
static void PositionDialog(HWND hwnd, HWND hwndRelative) {
    Rect rRelative = HwndWindowRect(hwndRelative);
    Rect r = HwndWindowRect(hwnd);
    int x = rRelative.x + (rRelative.dx / 2) - (r.dx / 2);
    int y = rRelative.y + (rRelative.dy / 2) - (r.dy / 2);
    r = {x, y, r.dx, r.dy};
    Rect r2 = ShiftRectToWorkArea(r, hwndRelative, true);
    SetWindowPos(hwnd, nullptr, r2.x, r2.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

static void PaintUnsavedDot(VirtCustom*, VirtPaintCtx* ctx) {
    ctx->gfx->FillEllipse(ctx->bounds, ThemeAccentColor());
}

bool AdvancedSettingsWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    // OnSize repositions in-place editors after DoLayout; skip the generic path
    autoLayout = false;
    CollectSettings(items, sections);

    {
        CreateCustomArgs args;
        args.title = Tr("Advanced Settings");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION | WS_THICKFRAME;
        args.font = GetFont();
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    fontBold = GetBoldPlatformFont(UiFont(kAdvTextFontPx));

    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();
    auto colMuted = ThemeWindowTextDisabledColor();
    SetColors(colTxt, colBg);
    bool isRtl = IsUIRtl();

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    // the filter line: a bare edit with the count at its right end, over a hairline
    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.isMultiLine = false;
        args.withBorder = false;
        args.cueText = Tr("Filter settings");
        args.font = UiFont(kAdvQueryFontPx);
        args.isRtl = isRtl;
        auto* c = new Edit();
        c->SetColors(colTxt, colBg);
        c->maxDx = 150;
        HWND ok = c->Create(args);
        ReportIf(!ok);
        c->onTextChanged = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::QueryChanged>(this);
        editFilter = c;

        auto* row = new HBox();
        row->alignMain = MainAxisAlign::MainStart;
        row->alignCross = CrossAxisAlign::CrossCenter;
        row->gap = DpiScale(kAdvGapDx);
        row->rtl = isRtl;
        row->AddChild(c, 1);
        countText = NewVirtText({.s = {}, .font = UiFont(kAdvMetaFontPx), .textColor = colMuted, .isRtl = isRtl});
        row->AddChild(countText);
        vbox->AddChild(new Padding(row, DpiScaledInsets(kAdvQueryPadDy, kAdvPadDx)));
        vbox->AddChild(new VirtLine());
    }

    {
        auto* c = new AdvSettingsListBox();
        c->dpi = GetDpi();
        c->font = UiFont(kAdvTextFontPx);
        c->itemDy = DpiScale(kAdvRowDy);
        c->SetColor(kColListText, colTxt);
        c->SetColor(kColListBg, colBg);
        c->padding = DpiScaledInsets(kAdvListPadDy, kAdvInsetDx);
        c->onDrawItem =
            MkMethod1<AdvancedSettingsWnd, VirtListBox::DrawItemEvent*, &AdvancedSettingsWnd::DrawListBoxItem>(this);
        c->onMouseDown = MkMethod1<AdvSettingsListBox, VirtMouseEvent*, &AdvSettingsListBox::OnAdvMouseDown>(c);
        listBox = c;
        model = new ListBoxModelSettings();
        model->items = &items;
        model->sections = &sections;
        CollectFilteredSettings(items, filterWords, model->rows);
        c->onSelectionChanged = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnSelectionChanged>(this);
        c->onDoubleClick = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnItemDoubleClicked>(this);
        c->SetModel(model);
        vbox->AddChild(c, 1);
    }
    countText->SetText(AdvSettingsCountTemp(CountSettingRows(model->rows), len(items)));

    // the help strip: the selected setting's name and default over its doc comment
    {
        vbox->AddChild(new VirtLine());
        auto* head = new HBox();
        head->alignMain = MainAxisAlign::MainStart;
        head->alignCross = CrossAxisAlign::CrossCenter;
        head->gap = DpiScale(kAdvPadDx);
        head->rtl = isRtl;
        helpName = NewVirtText({.s = {}, .font = fontBold, .textColor = colTxt, .isRtl = isRtl, .ellipsis = true});
        head->AddChild(helpName, 1);
        helpDefault = NewVirtText({.s = {}, .font = UiFont(kAdvMetaFontPx), .textColor = colMuted, .isRtl = isRtl});
        head->AddChild(helpDefault);

        auto* c = new VirtFixedLinesText();
        c->font = UiFont(kAdvTextFontPx);
        c->nLines = kAdvHelpLines;
        c->SetColor(kColRichText, colMuted);
        c->SetColor(kColRichLink, colMuted);
        c->SetColor(kColRichBg, colBg);
        commentText = c;

        auto* box = new VBox();
        box->alignMain = MainAxisAlign::MainStart;
        box->alignCross = CrossAxisAlign::Stretch;
        box->gap = DpiScale(kAdvHelpGapDy);
        box->AddChild(head);
        box->AddChild(c);
        vbox->AddChild(new Padding(box, DpiScaledInsets(kAdvHelpPadDy, kAdvPadDx)));
    }

    // the footer: the key hints and the quiet actions on the left, the unsaved
    // count, Cancel and Save on the right
    {
        auto* row = new HBox();
        row->alignMain = MainAxisAlign::MainStart;
        row->alignCross = CrossAxisAlign::CrossCenter;
        row->gap = DpiScale(kAdvGapDx);
        row->rtl = isRtl;

        hintRow = new HBox();
        hintRow->alignMain = MainAxisAlign::MainStart;
        hintRow->alignCross = CrossAxisAlign::CrossCenter;
        hintRow->rtl = isRtl;
        row->AddChild(hintRow);
        UpdateHints();

        PlatformFont* btnFont = UiFont(kAdvTextFontPx);
        btnOpenSettingsFile = NewDlgButton(hwnd, Tr("Open Settings File"), btnFont, DlgButtonStyle::Ghost, false);
        btnOpenSettingsFile->onClick =
            MkMethod1<AdvancedSettingsWnd, VirtMouseEvent*, &AdvancedSettingsWnd::OnOpenSettingsFile>(this);
        row->AddChild(btnOpenSettingsFile);
        btnHelp = NewDlgButton(hwnd, Tr("Help"), btnFont, DlgButtonStyle::Ghost, false);
        btnHelp->onClick = MkMethod1<AdvancedSettingsWnd, VirtMouseEvent*, &AdvancedSettingsWnd::OnHelp>(this);
        row->AddChild(btnHelp);
        row->AddChild(new Spacer(0, 0), 1);

        auto* unsaved = new HBox();
        unsaved->alignMain = MainAxisAlign::MainStart;
        unsaved->alignCross = CrossAxisAlign::CrossCenter;
        unsaved->gap = DpiScale(kAdvGapDx);
        unsaved->rtl = isRtl;
        unsavedDot = new VirtCustom();
        unsavedDot->idealSize = {DpiScale(kAdvDotDx), DpiScale(kAdvDotDx)};
        unsavedDot->onPaint = MkFunc1(PaintUnsavedDot, unsavedDot);
        unsavedDot->SetFlag(vwfNoHitTest, true);
        unsavedDot->SetIsVisible(false);
        unsaved->AddChild(unsavedDot);
        unsavedText = NewVirtText({.s = {}, .font = UiFont(kAdvMetaFontPx), .textColor = colTxt, .isRtl = isRtl});
        unsavedText->SetIsVisible(false);
        unsaved->AddChild(unsavedText);
        unsaved->SetVisibility(Visibility::Collapse);
        unsavedRow = unsaved;
        row->AddChild(new Padding(unsaved, DpiScaledInsets(0, kAdvGapDx)));

        btnCancel = NewDlgButton(hwnd, Tr("Cancel"), btnFont, DlgButtonStyle::Outline, false);
        btnCancel->onClick = MkMethod1<AdvancedSettingsWnd, VirtMouseEvent*, &AdvancedSettingsWnd::OnCancel>(this);
        row->AddChild(btnCancel);
        btnSave = NewDlgButton(hwnd, Tr("Save"), btnFont, DlgButtonStyle::Filled, true);
        btnSave->onClick = MkMethod1<AdvancedSettingsWnd, VirtMouseEvent*, &AdvancedSettingsWnd::OnSave>(this);
        row->AddChild(new Padding(btnSave, DpiScaledInsets(0, kAdvGapDx, 0, 0)));
        // the hints pad themselves by 8, so the row starts 8 in and the
        // padding on Save evens the right side out
        vbox->AddChild(new Padding(row, DpiScaledInsets(kAdvFootPadDy, kAdvInsetDx)));
    }

    DarkModeApplyToPopupWindow(hwnd);

    layout = vbox;

    auto rc = HwndClientRect(win->hwndFrame);
    // Default is wide enough that long setting names (e.g. InverseSearchCmdLine)
    // and long values don't crowd each other; reuse last size if the user
    // resized earlier this session (#5804), but never below kAdvSettingsMinClientDx.
    int minDx = DpiScale(kAdvSettingsMinClientDx);
    int dy = gAdvSettingsLastClientDy > 0 ? gAdvSettingsLastClientDy : limitValue(rc.dy - 72, 480, 900);
    int dx = gAdvSettingsLastClientDx > 0 ? gAdvSettingsLastClientDx : limitValue(rc.dx - 128, 760, 1100);
    dx = std::max(dx, minDx);
    LayoutAndSizeToContent(layout, dx, dy, hwnd);
    // pick up the virtual controls so we paint them and they get their input
    DoLayout(HwndClientRect(hwnd).Size());
    gAdvSettingsLastClientDx = HwndClientRect(hwnd).dx;
    gAdvSettingsLastClientDy = HwndClientRect(hwnd).dy;
    PositionDialog(hwnd, win->hwndFrame);

    SetIsVisible(true);
    EditSetFocus(editFilter);
    return true;
}

struct AdvSettingsRowRec {
    int idx;
    Rect rect;
};

// set only while the probe below is painting into its scratch bitmap
static Vec<AdvSettingsRowRec>* gAdvSettingsRowRec = nullptr;

static void RecordAdvSettingsRow(VirtListBox::DrawItemEvent* ev) {
    if (gAdvSettingsRowRec) {
        VecAppend(*gAdvSettingsRowRec, {ev->itemIndex, ev->itemRect});
    }
}

// The rects the list hands its row drawer. Asking VirtListBox for them
// (ItemRect) would report what the layout intends, not what the paint does --
// and the bug this is here for was a paint that disagreed with the layout. So
// the real tree is painted into a scratch bitmap with the row drawer swapped
// for a recorder.
static void RecordAdvSettingsRows(AdvancedSettingsWnd* wnd, Vec<AdvSettingsRowRec>& out) {
    Rect client = HwndClientRect(wnd->hwnd);
    if (client.IsEmpty() || !wnd->vroot) {
        return;
    }
    HDC hdcWin = GetDC(wnd->hwnd);
    if (!hdcWin) {
        return;
    }
    HDC memDC = CreateCompatibleDC(hdcWin);
    HBITMAP bmp = CreateCompatibleBitmap(hdcWin, client.dx, client.dy);
    if (memDC && bmp) {
        HGDIOBJ oldBmp = SelectObject(memDC, bmp);
        auto prevDrawItem = wnd->listBox->onDrawItem;
        gAdvSettingsRowRec = &out;
        wnd->listBox->onDrawItem = MkFunc1Void<VirtListBox::DrawItemEvent*>(RecordAdvSettingsRow);
        {
            Gfx* gfx = GfxCreate(memDC);
            wnd->vroot->Paint(gfx, client);
            delete gfx;
        }
        wnd->listBox->onDrawItem = prevDrawItem;
        gAdvSettingsRowRec = nullptr;
        SelectObject(memDC, oldBmp);
    }
    if (bmp) {
        DeleteObject(bmp);
    }
    if (memDC) {
        DeleteDC(memDC);
    }
    ReleaseDC(wnd->hwnd, hdcWin);
}

// What the settings list actually draws: the list's content rect in dialog
// client coords, the row height, and the rect of every row the paint touched.
// `action` is "geom" to report, or "scroll" to first scroll by `arg` rows. Rows
// are always whole and always tile the usable viewport; a test can check that
// on the real pixels without hard-coding a layout that depends on window size,
// dpi and theme. Used by tests/issue-5882.ts.
TempStr AdvSettingsRowsResultTemp(Str action, int arg, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    AdvancedSettingsWnd* wnd = gAdvancedSettingsWnd;
    if (!wnd || !wnd->hwnd) {
        out.Append(StrL("NOTREADY no-dialog\n"));
        return finish(2);
    }
    if (str::Eq(action, StrL("names"))) {
        for (SettingItem* item : wnd->items) {
            out.Append(item->name);
            out.AppendChar('\n');
        }
        return finish(0);
    }
    if (str::Eq(action, StrL("nondefault"))) {
        int n = 0;
        for (SettingItem* item : wnd->items) {
            if (!SettingDiffersFromDefault(item)) {
                continue;
            }
            n++;
            out.Append(
                fmt("%s=%s default=%s\n", item->name, FormatSettingValueTemp(item), FormatSettingDefaultTemp(item)));
        }
        out.Append(fmt("count=%d\n", n));
        return finish(0);
    }
    if (str::Eq(action, StrL("changed"))) {
        int n = wnd->CountChangedSettings();
        bool banner = wnd->unsavedRow && wnd->unsavedRow->GetVisibility() == Visibility::Visible;
        out.Append(fmt("changed=%d banner=%d\n", n, banner ? 1 : 0));
        return finish(0);
    }
    if (str::Eq(action, StrL("toggle"))) {
        int nBool = 0;
        SettingItem* target = nullptr;
        for (SettingItem* item : wnd->items) {
            if (item->type != SettingType::Bool) {
                continue;
            }
            if (nBool == arg) {
                target = item;
                break;
            }
            nBool++;
        }
        if (!target) {
            out.Append(StrL("ERROR no-bool\n"));
            return finish(1);
        }
        target->boolVal = !target->boolVal;
        wnd->NoteItemEdited(target);
        if (wnd->listBox) {
            wnd->listBox->Invalidate();
        }
        int n = wnd->CountChangedSettings();
        bool banner = wnd->unsavedRow && wnd->unsavedRow->GetVisibility() == Visibility::Visible;
        out.Append(fmt("toggled=%s changed=%d banner=%d\n", target->name, n, banner ? 1 : 0));
        return finish(0);
    }
    // select the row of the arg-th bool setting (as "toggle" counts them) and
    // press Delete on it
    if (str::Eq(action, StrL("restore"))) {
        int nBool = 0;
        int lbIdx = -1;
        for (int i = 0; i < len(wnd->items) && lbIdx < 0; i++) {
            if (wnd->items[i]->type != SettingType::Bool || nBool++ != arg) {
                continue;
            }
            lbIdx = wnd->model ? FindSettingRow(wnd->model->rows, i) : -1;
        }
        if (lbIdx < 0) {
            out.Append(StrL("ERROR no-bool\n"));
            return finish(1);
        }
        wnd->listBox->SetCurrentSelection(lbIdx);
        wnd->OnSelectionChanged();
        bool restored = wnd->RestoreSelectedDefault(wnd->editValue != nullptr, wnd->dropDownValue != nullptr);
        out.Append(fmt("restored=%s changed=%d\n", restored ? wnd->model->ItemAt(lbIdx)->name : StrL(""),
                       wnd->CountChangedSettings()));
        return finish(0);
    }
    if (str::Eq(action, StrL("esc"))) {
        bool editingValue = wnd->editValue != nullptr;
        bool editingEnum = wnd->dropDownValue != nullptr;
        wnd->HandleEscapeKey(editingValue, editingEnum);
        bool closing = !gAdvancedSettingsWnd || gAdvancedSettingsWnd->deleteScheduled;
        out.Append(fmt("closed=%d\n", closing ? 1 : 0));
        return finish(0);
    }
    if (str::Eq(action, StrL("save"))) {
        wnd->OnSave(nullptr);
        out.Append(StrL("saved=1\n"));
        return finish(0);
    }
    if (str::Eq(action, StrL("state"))) {
        out.Append(fmt("editing=%d\n", wnd->editValue ? 1 : 0));
        return finish(0);
    }
    if (str::Eq(action, StrL("edit"))) {
        int idx = -1;
        if (wnd->model) {
            int n = wnd->model->ItemsCount();
            for (int i = 0; i < n; i++) {
                SettingItem* item = wnd->model->ItemAt(i);
                if (item && !item->enumValues && item->type != SettingType::Bool) {
                    idx = wnd->model->rows[i].itemIdx;
                    break;
                }
            }
        }
        if (idx < 0) {
            out.Append(StrL("ERROR no-edit-item\n"));
            return finish(1);
        }
        wnd->BeginEditValue(idx);
        out.Append(fmt("editing=%d\n", wnd->editValue ? 1 : 0));
        return finish(0);
    }
    if (str::Eq(action, StrL("killfocus"))) {
        if (wnd->editValue && wnd->editValue->hwnd) {
            HWND edit = wnd->editValue->hwnd;
            int id = GetDlgCtrlID(edit);
            SendMessageW(wnd->hwnd, WM_COMMAND, MAKEWPARAM(id, EN_KILLFOCUS), (LPARAM)edit);
        }
        out.Append(fmt("editing=%d\n", wnd->editValue ? 1 : 0));
        return finish(0);
    }
    if (str::Eq(action, StrL("resize"))) {
        WindowBase::SizeEvent ev;
        ev.size = HwndClientRect(wnd->hwnd).Size();
        wnd->OnSize(&ev);
        out.Append(fmt("editing=%d\n", wnd->editValue ? 1 : 0));
        return finish(0);
    }
    if (!wnd->listBox) {
        out.Append(StrL("NOTREADY no-dialog\n"));
        return finish(2);
    }
    VirtListBox* lb = wnd->listBox;
    if (lb->bounds.IsEmpty()) {
        out.Append(StrL("NOTREADY no-layout\n"));
        return finish(2);
    }
    int itemDy = lb->GetItemHeight();
    if (str::Eq(action, StrL("scroll"))) {
        lb->ScrollBy(arg * itemDy);
        HwndRepaintNow(wnd->hwnd);
    } else if (!str::Eq(action, StrL("geom"))) {
        out.Append(fmt("ERROR unknown-action action=%s\n", action));
        return finish(1);
    }

    // the content rect is where the rows live; UsableDy() rounds it down to
    // whole rows, so the strip below the last one is background and must stay
    // that way however far the list is scrolled
    Rect cr = lb->ContentRectInWindow();
    out.Append(fmt("OK content=%d,%d,%d,%d itemDy=%d usableDy=%d scrollY=%d maxScrollY=%d items=%d\n", cr.x, cr.y,
                   cr.dx, cr.dy, itemDy, lb->UsableDy(), lb->scrollY, lb->MaxScrollY(), lb->ItemsCount()));
    Vec<AdvSettingsRowRec> drawn;
    RecordAdvSettingsRows(wnd, drawn);
    for (const AdvSettingsRowRec& rec : drawn) {
        Rect r = rec.rect;
        out.Append(fmt("row=%d rect=%d,%d,%d,%d\n", rec.idx, r.x, r.y, r.dx, r.dy));
    }
    return finish(0);
}

void ShowAdvancedSettingsDialog(MainWindow* win) {
    if (!HasPermission(Perm::SavePreferences)) {
        return;
    }
    if (gAdvancedSettingsWnd) {
        HwndSetFocus(gAdvancedSettingsWnd->hwnd);
        return;
    }
    auto* wnd = new AdvancedSettingsWnd();
    wnd->onBeforeDelete = MkFunc0Void(ClearAdvancedSettingsWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->onSize = MkMethod1<AdvancedSettingsWnd, WindowBase::SizeEvent*, &AdvancedSettingsWnd::OnSize>(wnd);
    wnd->onGetMinMaxInfo =
        MkMethod1<AdvancedSettingsWnd, WindowBase::GetMinMaxInfoEvent*, &AdvancedSettingsWnd::OnGetMinMaxInfo>(wnd);
    wnd->onKeyDown = MkMethod1<AdvancedSettingsWnd, KeyEvent*, &AdvancedSettingsWnd::OnKeyDown>(wnd);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gAdvancedSettingsWnd = wnd;
}

struct RestrictedUseWnd;

struct RestrictedUseListBox : VirtListBox {
    RestrictedUseWnd* owner = nullptr;
    void OnRestrictedMouseDown(VirtMouseEvent* ev);
};

struct RestrictedUseWnd : WindowBase {
    MainWindow* win = nullptr;
    RestrictedUseConfig config;
    RestrictedUseListBox* listBox = nullptr;
    VirtFixedLinesText* commentText = nullptr;
    VirtButton* btnSave = nullptr;
    ILayout* saveLayout = nullptr;
    ILayout* deleteLayout = nullptr;
    PlatformFont* fontBold = nullptr;

    bool Create(MainWindow* mainWin);
    void ToggleSelected();
    void UpdateSelection();
    void UpdateButtons();
    void DrawItem(VirtListBox::DrawItemEvent* ev);
    void OnKeyDown(KeyEvent* ev);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnSave(VirtMouseEvent* ev = nullptr);
    void OnDelete(VirtMouseEvent* ev = nullptr);
    void OnHelp(VirtMouseEvent* ev = nullptr);
};

static RestrictedUseWnd* gRestrictedUseWnd = nullptr;

void RestrictedUseListBox::OnRestrictedMouseDown(VirtMouseEvent* ev) {
    int idx = ItemFromPoint(ev->pt);
    VirtListBox::OnMouseDown(ev);
    if (idx >= 0 && owner) {
        owner->ToggleSelected();
    }
}

void RestrictedUseWnd::UpdateButtons() {
    saveLayout->SetVisibility(RestrictedUseConfigChanged(config) ? Visibility::Visible : Visibility::Collapse);
    deleteLayout->SetVisibility(config.fileExists ? Visibility::Visible : Visibility::Collapse);
    if (vroot) {
        vroot->RequestLayout();
    }
}

void RestrictedUseWnd::UpdateSelection() {
    int idx = listBox->GetCurrentSelection();
    commentText->SetText(idx >= 0 ? RestrictedUseFeatureDescription(idx) : Str());
}

void RestrictedUseWnd::ToggleSelected() {
    int idx = listBox->GetCurrentSelection();
    if (idx < 0 || idx >= RestrictedUseFeatureCount()) {
        return;
    }
    config.items[idx].enabled = !config.items[idx].enabled;
    UpdateRestrictedUseChanged(config);
    UpdateButtons();
    listBox->Invalidate();
}

void RestrictedUseWnd::DrawItem(VirtListBox::DrawItemEvent* ev) {
    int idx = ev->itemIndex;
    if (idx < 0 || idx >= RestrictedUseFeatureCount()) {
        return;
    }
    Gfx* gfx = ev->gfx;
    Rect row = ev->itemRect;
    Color bg = listBox->GetColor(kColListBg);
    Color text = listBox->GetColor(kColListText);
    if (IsSpecialColor(bg)) {
        bg = GetSysColor(COLOR_WINDOW);
    }
    if (IsSpecialColor(text)) {
        text = GetSysColor(COLOR_WINDOWTEXT);
    }
    if (ev->selected) {
        bg = AccentColor(bg, 30);
    }
    gfx->FillRect(row, bg);

    int boxDx = DpiScale(13);
    int pad = DpiScale(5);
    Rect box{row.Right() - pad - boxDx, row.y + (row.dy - boxDx) / 2, boxDx, boxDx};
    Rect name = row;
    name.SubLR(pad, pad + boxDx + pad);
    PlatformFont* itemFont = config.items[idx].changed ? fontBold : GetFont();
    gfx->DrawText(RestrictedUseFeatureName(idx), name, gfxTextLeft | gfxTextVCenter | gfxTextEllipsis, itemFont, text);
    gfx->DrawRect(box, text);
    if (config.items[idx].enabled) {
        Point p1{box.x + 2, box.y + box.dy / 2};
        Point p2{box.x + box.dx / 2, box.y + box.dy - 3};
        Point p3{box.x + box.dx - 2, box.y + 3};
        gfx->DrawLineAA(p1, p2, text, 2);
        gfx->DrawLineAA(p2, p3, text, 2);
    }
}

void RestrictedUseWnd::OnKeyDown(KeyEvent* ev) {
    if (ev->vkey == VK_RETURN && vroot && vroot->focused == listBox) {
        ToggleSelected();
        ev->didHandle = true;
    }
}

void RestrictedUseWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

void RestrictedUseWnd::OnSave(VirtMouseEvent*) {
    if (!SaveRestrictedUseConfig(config)) {
        MsgBox(hwnd, Tr("Failed to save sumatrapdfrestrict.ini."), Tr("Error"), MB_OK | MB_ICONERROR);
        return;
    }
    ScheduleDelete();
}

void RestrictedUseWnd::OnDelete(VirtMouseEvent*) {
    int res =
        MsgBox(hwnd, Tr("Delete sumatrapdfrestrict.ini?"), Tr("Configure Restricted Use"), MB_YESNO | MB_ICONQUESTION);
    if (res != IDYES) {
        return;
    }
    if (!DeleteRestrictedUseConfig()) {
        MsgBox(hwnd, Tr("Failed to delete sumatrapdfrestrict.ini."), Tr("Error"), MB_OK | MB_ICONERROR);
        return;
    }
    ScheduleDelete();
}

void RestrictedUseWnd::OnHelp(VirtMouseEvent*) {
    SumatraLaunchBrowser(Str(kRestrictedUseDocsUrl));
}

bool RestrictedUseWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    TempStr path = GetPathInExeDirTemp(Str(kRestrictionsFileName));
    bool exists = file::Exists(path);
    LoadRestrictedUseConfig(config, exists ? file::ReadFile(path) : Str(),
                            exists ? RestrictedUseFile::Exists : RestrictedUseFile::Missing);

    CreateCustomArgs args;
    args.owner = win ? win->hwndFrame : nullptr;
    args.title = Tr("Configure Restricted Use");
    args.visible = false;
    args.style = WS_POPUPWINDOW | WS_CAPTION;
    args.font = GetFont();
    args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
    CreateCustom(args);
    if (!hwnd) {
        return false;
    }
    fontBold = GetBoldPlatformFont(GetFont());

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;
    auto* list = new RestrictedUseListBox();
    list->owner = this;
    list->dpi = GetDpi();
    list->font = GetFont();
    list->idealSizeLines = RestrictedUseFeatureCount();
    auto* model = new ListBoxModelStrings();
    for (int i = 0; i < RestrictedUseFeatureCount(); i++) {
        model->strings.Append(RestrictedUseFeatureName(i));
    }
    list->SetModel(model);
    list->SetCurrentSelection(0);
    list->onMouseDown =
        MkMethod1<RestrictedUseListBox, VirtMouseEvent*, &RestrictedUseListBox::OnRestrictedMouseDown>(list);
    list->onSelectionChanged = MkMethod0<RestrictedUseWnd, &RestrictedUseWnd::UpdateSelection>(this);
    list->onDrawItem = MkMethod1<RestrictedUseWnd, VirtListBox::DrawItemEvent*, &RestrictedUseWnd::DrawItem>(this);
    listBox = list;
    vbox->AddChild(list);

    auto* help = new VirtFixedLinesText();
    help->font = GetFont();
    help->borderCol = ThemeEdgeColor();
    help->padding = DpiScaledInsets(4);
    commentText = help;
    vbox->AddChild(new Padding(help, DpiScaledInsets(4, 2)));
    UpdateSelection();

    auto* buttons = new HBox();
    buttons->alignMain = MainAxisAlign::SpaceBetween;
    buttons->alignCross = CrossAxisAlign::CrossCenter;
    auto* left = new HBox();
    left->gap = GetFont()->averageCharWidth;
    auto pad = Insets{4, 0, 4, 0};
    btnSave = NewThemedButton(hwnd, Tr("Save"), GetFont(), true);
    btnSave->onClick = MkMethod1<RestrictedUseWnd, VirtMouseEvent*, &RestrictedUseWnd::OnSave>(this);
    saveLayout = new Padding(btnSave, pad);
    left->AddChild(saveLayout);
    auto* btnDelete = NewThemedButton(hwnd, Tr("Delete File"), GetFont(), false);
    btnDelete->onClick = MkMethod1<RestrictedUseWnd, VirtMouseEvent*, &RestrictedUseWnd::OnDelete>(this);
    deleteLayout = new Padding(btnDelete, pad);
    left->AddChild(deleteLayout);
    auto* btnCancel = NewThemedButton(hwnd, Tr("Cancel"), GetFont(), false);
    btnCancel->onClick = MkMethod1<RestrictedUseWnd, VirtMouseEvent*, &RestrictedUseWnd::OnCancel>(this);
    left->AddChild(new Padding(btnCancel, pad));
    buttons->AddChild(left);
    auto* btnHelp = NewThemedButton(hwnd, Tr("Help"), GetFont(), false);
    btnHelp->onClick = MkMethod1<RestrictedUseWnd, VirtMouseEvent*, &RestrictedUseWnd::OnHelp>(this);
    buttons->AddChild(new Padding(btnHelp, pad));
    vbox->AddChild(buttons);

    layout = new Padding(vbox, DpiScaledInsets(4, 8));
    UpdateButtons();
    LayoutAndSizeToContent(layout, DpiScale(520), 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();
    SetIsVisible(true);
    vroot->SetFocus(list);
    return true;
}

static void ClearRestrictedUseWnd() {
    gRestrictedUseWnd = nullptr;
}

static void CloseRestrictedUseWnd(WindowBase::CloseEvent*) {
    if (gRestrictedUseWnd) {
        gRestrictedUseWnd->ScheduleDelete();
    }
}

void ShowConfigureRestrictedUseDialog(MainWindow* win) {
    if (!CanWriteRestrictedUseConfig()) {
        Str msg = Tr("SumatraPDF cannot create files in its installation folder.");
        if (!IsProcessRunningElevated()) {
            int res = MsgBox(win ? win->hwndFrame : nullptr, Tr("Administrator access is required. Restart elevated?"),
                             Tr("Configure Restricted Use"), MB_YESNO | MB_ICONERROR);
            if (res == IDYES) {
                LaunchElevated(GetSelfExePathTemp(), StrL("-configure-restricted-use"));
            }
        } else {
            MsgBox(win ? win->hwndFrame : nullptr, msg, Tr("Configure Restricted Use"), MB_OK | MB_ICONERROR);
        }
        return;
    }
    if (gRestrictedUseWnd) {
        HwndSetFocus(gRestrictedUseWnd->hwnd);
        return;
    }
    auto* wnd = new RestrictedUseWnd();
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearRestrictedUseWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(CloseRestrictedUseWnd);
    wnd->onKeyDown = MkMethod1<RestrictedUseWnd, KeyEvent*, &RestrictedUseWnd::OnKeyDown>(wnd);
    wnd->SetFont(GetAppFont());
    if (!wnd->Create(win)) {
        delete wnd;
        return;
    }
    gRestrictedUseWnd = wnd;
    RunModalWindow(wnd->hwnd, win ? win->hwndFrame : nullptr);
}
