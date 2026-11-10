/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Favorites.h"
#include "Translations.h"
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "AppHelpersCommon.h"

// The same window adds a favorite (Ctrl+B) and edits the existing ones
// (CmdEditFavorites); edit mode has no name for a new favorite.
//
//   Name for page 5 (optional):                    (add mode)
//   Chapter 2
//   ______________________________________________ underline
//   ---------------------------------------------- hairline
//   book.pdf
//         Intro (page 1)                           the favorites
//         Page 7
//   Name: Intro_________________  [Rename]
//   [Delete] [Move Up] [Move Down]
//                          [Cancel] [Add Favorite]  ([Close] in edit mode)
constexpr int kAddFavoriteDlgDx = 440;
constexpr int kFavFieldPadTop = 10;
constexpr int kFavFieldPadBottom = 16;
constexpr int kFavListLines = 10;

enum class FavDlgMode {
    Add,
    Edit,
};

struct FavEditListModel : ListBoxModel {
    Vec<FavEditRow> rows;

    int ItemsCount() override { return len(rows); }
    Str Item(int i) override { return FavEditRowTextTemp(rows[i]); }
};

struct AddFavoriteWnd : WindowBase {
    ~AddFavoriteWnd() override;

    FavDlgMode mode = FavDlgMode::Add;
    MainWindow* win = nullptr;
    int pageNo = 0;
    Str pageLabel;
    Str filePath;
    VirtText* label = nullptr;
    Edit* editName = nullptr;
    VirtListBox* listBox = nullptr;
    FavEditListModel* model = nullptr; // owned by listBox
    Edit* editRename = nullptr;
    VirtButton* btnRename = nullptr;
    VirtButton* btnDelete = nullptr;
    VirtButton* btnUp = nullptr;
    VirtButton* btnDown = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnOk = nullptr;

    bool Create(MainWindow* win, FavDlgMode mode, Str path, int page, Str labelIn, Str name);
    void SetTarget(MainWindow* win, Str path, int page, Str labelIn, Str name);

    FavEditRow* SelectedRow();
    void Reload(Favorite* toSelect);
    void OnSelectionChanged();
    void OnRename(VirtMouseEvent* ev = nullptr);
    void OnDelete(VirtMouseEvent* ev = nullptr);
    void OnMoveUp(VirtMouseEvent* ev = nullptr);
    void OnMoveDown(VirtMouseEvent* ev = nullptr);
    void OnKeyDown(KeyEvent* ev);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
};

static AddFavoriteWnd* gAddFavoriteWnd = nullptr;

AddFavoriteWnd::~AddFavoriteWnd() {
    str::Free(pageLabel);
    str::Free(filePath);
}

static void ClearAddFavoriteWnd() {
    gAddFavoriteWnd = nullptr;
}

void AddFavoriteWnd::SetTarget(MainWindow* win, Str path, int page, Str labelIn, Str name) {
    this->win = win;
    pageNo = page;
    str::ReplaceWithCopy(&pageLabel, labelIn);
    str::ReplaceWithCopy(&filePath, path);
    if (label) {
        label->SetText(FavoritePromptTemp(pageLabel));
    }
    if (editName) {
        editName->SetText(name);
        EditSelectAll(editName);
    }
}

FavEditRow* AddFavoriteWnd::SelectedRow() {
    int sel = listBox ? listBox->GetCurrentSelection() : -1;
    if (!model || sel < 0 || sel >= len(model->rows)) {
        return nullptr;
    }
    return &model->rows[sel];
}

// rebuild the rows after a change, keeping `toSelect` selected
void AddFavoriteWnd::Reload(Favorite* toSelect) {
    BuildFavEditRows(model->rows);
    listBox->SetModel(model);
    int sel = -1;
    for (int i = 0; toSelect && i < len(model->rows); i++) {
        if (model->rows[i].fav == toSelect) {
            sel = i;
        }
    }
    listBox->SetCurrentSelection(sel);
    OnSelectionChanged();
}

// the name field and the buttons follow the selected favorite
void AddFavoriteWnd::OnSelectionChanged() {
    FavEditRow* row = SelectedRow();
    Favorite* fav = row ? row->fav : nullptr;
    FileState* fs = row ? row->fs : nullptr;
    editRename->SetText(fav ? fav->name : Str{});
    editRename->SetIsEnabled(fav != nullptr);
    SetDlgButtonEnabled(btnRename, DlgButtonStyle::Ghost, fav != nullptr);
    SetDlgButtonEnabled(btnDelete, DlgButtonStyle::Ghost, fav != nullptr);
    SetDlgButtonEnabled(btnUp, DlgButtonStyle::Ghost, CanMoveFavorite(fs, fav, FavMove::Up));
    SetDlgButtonEnabled(btnDown, DlgButtonStyle::Ghost, CanMoveFavorite(fs, fav, FavMove::Down));
}

void AddFavoriteWnd::OnRename(VirtMouseEvent*) {
    FavEditRow* row = SelectedRow();
    if (!row || !row->fav) {
        return;
    }
    Favorite* fav = row->fav;
    RenameFavorite(row->fs, fav, editRename->GetTextTemp());
    Reload(fav);
}

void AddFavoriteWnd::OnDelete(VirtMouseEvent*) {
    FavEditRow* row = SelectedRow();
    if (!row || !row->fav) {
        return;
    }
    int sel = listBox->GetCurrentSelection();
    DelFavorite(row->fs, row->fav);
    BuildFavEditRows(model->rows);
    listBox->SetModel(model);
    // select the row that took its place, unless that's another file's name
    int n = len(model->rows);
    if (sel >= n || (sel >= 0 && !model->rows[sel].fav)) {
        sel--;
    }
    if (sel >= 0 && sel < n && !model->rows[sel].fav) {
        sel = -1;
    }
    listBox->SetCurrentSelection(sel);
    OnSelectionChanged();
}

void AddFavoriteWnd::OnMoveUp(VirtMouseEvent*) {
    FavEditRow* row = SelectedRow();
    if (!row) {
        return;
    }
    Favorite* fav = row->fav;
    MoveFavorite(row->fs, fav, FavMove::Up);
    Reload(fav);
}

void AddFavoriteWnd::OnMoveDown(VirtMouseEvent*) {
    FavEditRow* row = SelectedRow();
    if (!row) {
        return;
    }
    Favorite* fav = row->fav;
    MoveFavorite(row->fs, fav, FavMove::Down);
    Reload(fav);
}

// Enter in the name field renames instead of pressing the default button
void AddFavoriteWnd::OnKeyDown(KeyEvent* ev) {
    if (ev->vkey == VK_RETURN && editRename && ev->hwnd == editRename->hwnd) {
        OnRename();
        ev->didHandle = true;
    }
}

void AddFavoriteWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

void AddFavoriteWnd::OnOk(VirtMouseEvent*) {
    if (mode == FavDlgMode::Add) {
        TempStr name = editName ? editName->GetTextTemp() : Str{};
        str::TrimWSInPlace(name, str::TrimOpt::Both);
        if (len(name) == 0) {
            name = {};
        }
        ApplyAddFavorite(win, filePath, pageNo, pageLabel, name);
    }
    ScheduleDelete();
}

static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    if (gAddFavoriteWnd) {
        gAddFavoriteWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    if (gAddFavoriteWnd) {
        gAddFavoriteWnd->ScheduleDelete();
    }
}

static VirtButton* NewFavButton(AddFavoriteWnd* w, Str text, PlatformFont* font) {
    return NewDlgButton(w->hwnd, text, font, DlgButtonStyle::Ghost, false);
}

bool AddFavoriteWnd::Create(MainWindow* win, FavDlgMode modeIn, Str path, int page, Str labelIn, Str name) {
    mode = modeIn;
    bool isAdd = mode == FavDlgMode::Add;
    SetTarget(win, path, page, labelIn, name);

    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colMuted = ThemeWindowDarkerTextColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    SetFont(fontText);
    bool isRtl = IsUIRtl();
    if (!CreateToolDialog(this, win ? win->hwndFrame : nullptr, isAdd ? Tr("Add Favorite") : Tr("Edit Favorites"),
                          fontText, colTxt, colBg)) {
        return false;
    }

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;

    if (isAdd) {
        label = NewDlgCaption(FavoritePromptTemp(pageLabel), fontText, colMuted);
        vbox->AddChild(label);

        // the name, in the larger font
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = DlgUiFont(kToolDlgQueryFontPx);
        args.withBorder = false;
        args.selectAllOnFocus = true;
        args.isRtl = isRtl;
        args.text = name;
        editName = new Edit();
        editName->SetColors(colTxt, colBg);
        editName->Create(args);
        Insets insets = DpiScaledInsets(kFavFieldPadTop, kToolDlgPadDx, kFavFieldPadBottom, kToolDlgPadDx);
        vbox->AddChild(new Padding(NewDlgUnderlined(editName), insets));
        vbox->AddChild(new VirtLine());
    }

    listBox = NewDlgList(GetDpi(), fontText);
    listBox->idealSizeLines = kFavListLines;
    listBox->onSelectionChanged = MkMethod0<AddFavoriteWnd, &AddFavoriteWnd::OnSelectionChanged>(this);
    model = new FavEditListModel();
    BuildFavEditRows(model->rows);
    listBox->SetModel(model);
    vbox->AddChild(NewDlgListBox(listBox), 1);

    // the selected favorite's name, and what can be done to it
    {
        HBox* row = NewDlgFieldRow();
        row->AddChild(NewDlgLabel(Tr("Name:"), fontText, colMuted, kToolDlgRowDy));
        editRename = NewDlgEdit(hwnd, fontText, {}, 0);
        row->AddChild(NewDlgUnderlined(editRename), 1);
        btnRename = NewFavButton(this, Tr("Rename"), fontText);
        btnRename->onClick = MkMethod1<AddFavoriteWnd, VirtMouseEvent*, &AddFavoriteWnd::OnRename>(this);
        row->AddChild(btnRename);
        vbox->AddChild(NewDlgSection(row));
    }
    {
        HBox* row = NewDlgRow();
        btnDelete = NewFavButton(this, Tr("Delete"), fontText);
        btnDelete->onClick = MkMethod1<AddFavoriteWnd, VirtMouseEvent*, &AddFavoriteWnd::OnDelete>(this);
        row->AddChild(btnDelete);
        btnUp = NewFavButton(this, Tr("Move Up"), fontText);
        btnUp->onClick = MkMethod1<AddFavoriteWnd, VirtMouseEvent*, &AddFavoriteWnd::OnMoveUp>(this);
        row->AddChild(btnUp);
        btnDown = NewFavButton(this, Tr("Move Down"), fontText);
        btnDown->onClick = MkMethod1<AddFavoriteWnd, VirtMouseEvent*, &AddFavoriteWnd::OnMoveDown>(this);
        row->AddChild(btnDown);
        vbox->AddChild(new Padding(row, DpiScaledInsets(0, kToolDlgPadDx)));
    }

    if (isAdd) {
        btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
        btnCancel->onClick = MkMethod1<AddFavoriteWnd, VirtMouseEvent*, &AddFavoriteWnd::OnCancel>(this);
    }
    Str okText = isAdd ? Tr("Add Favorite") : Tr("Close");
    btnOk = NewDlgButton(hwnd, okText, fontText, DlgButtonStyle::Filled, true);
    btnOk->onClick = MkMethod1<AddFavoriteWnd, VirtMouseEvent*, &AddFavoriteWnd::OnOk>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnOk));
    layout = vbox;

    SizeToLayout(this, layout, DpiScale(kAddFavoriteDlgDx), 0, win ? win->hwndFrame : nullptr);
    UpdateTheme();
    OnSelectionChanged();

    SetIsVisible(true);
    if (editName) {
        EditSelectAll(editName);
        EditSetFocus(editName);
    } else {
        SetFocusTo(listBox);
    }
    return true;
}

static void ShowFavoriteDialog(MainWindow* win, FavDlgMode mode, Str filePath, int pageNo, Str pageLabel, Str name) {
    if (gAddFavoriteWnd && gAddFavoriteWnd->mode == mode) {
        if (mode == FavDlgMode::Add) {
            gAddFavoriteWnd->SetTarget(win, filePath, pageNo, pageLabel, name);
        }
        if (gAddFavoriteWnd->hwnd) {
            gAddFavoriteWnd->DoLayout();
            HwndInvalidate(gAddFavoriteWnd->hwnd);
        }
        HwndSetFocus(gAddFavoriteWnd->hwnd);
        if (gAddFavoriteWnd->editName) {
            EditSetFocus(gAddFavoriteWnd->editName);
        }
        return;
    }
    if (gAddFavoriteWnd) {
        gAddFavoriteWnd->Close();
    }
    auto* wnd = new AddFavoriteWnd();
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearAddFavoriteWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->onKeyDown = MkMethod1<AddFavoriteWnd, KeyEvent*, &AddFavoriteWnd::OnKeyDown>(wnd);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win, mode, filePath, pageNo, pageLabel, name);
    if (!ok) {
        delete wnd;
        return;
    }
    gAddFavoriteWnd = wnd;
    RunModalWindow(wnd->hwnd, win ? win->hwndFrame : nullptr);
}

void ShowAddFavoriteDialog(MainWindow* win, Str filePath, int pageNo, Str pageLabel, Str name) {
    ShowFavoriteDialog(win, FavDlgMode::Add, filePath, pageNo, pageLabel, name);
}

void ShowEditFavoritesDialog(MainWindow* win) {
    ShowFavoriteDialog(win, FavDlgMode::Edit, {}, 0, {}, {});
}
