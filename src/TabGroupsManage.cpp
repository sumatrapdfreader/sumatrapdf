/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Translations.h"
#include "SumatraConfig.h"
#include "Theme.h"
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "TabGroupsManage.h"
#include "AppHelpersCommon.h"

//   group #3                       the name, in the larger font (Save only)
//   ---------------------------------------------- hairline
//   group #1                        4 tabs
//   group #2                        2 tabs     the saved groups
//   Delete                         [Cancel] [Save]
constexpr int kTabGroupsDlgDx = 400;
constexpr int kTabGroupsDlgDy = 350;

struct TabGroupsListBoxModel : ListBoxModel {
    Vec<TabGroup*> groups;

    void Reload() {
        VecReset(groups);
        auto* g = gSettings->tabGroups;
        if (g) {
            for (auto* tg : *g) {
                VecAppend(groups, tg);
            }
        }
    }

    int ItemsCount() override { return len(groups); }

    Str Item(int i) override { return groups[i]->name; }

    int TabCount(int i) {
        auto* tf = groups[i]->tabFiles;
        return tf ? len(*tf) : 0;
    }
};

struct TabGroupsWnd : WindowBase {
    ~TabGroupsWnd() override;

    HWND hwndParent = nullptr;
    Edit* editName = nullptr;
    VirtListBox* listBox = nullptr;
    TabGroupsListBoxModel* model = nullptr;
    VirtButton* btnOk = nullptr;
    VirtButton* btnDelete = nullptr;
    VirtButton* btnCancel = nullptr;
    TabGroupDialogMode mode = TabGroupDialogMode::Save;
    MainWindow* win = nullptr;

    bool Create(MainWindow* winIn, TabGroupDialogMode modeIn);
    void ApplyDarkMode() override;
    void SaveTabGroup();
    void OpenTabGroup();
    void DeleteTabGroup(VirtMouseEvent* ev = nullptr);
    void UpdateDeleteButton();
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
};

static Vec<TabGroupsWnd*> gTabGroupsWnds;

TabGroupsWnd::~TabGroupsWnd() = default;

static void PopulateListBox(TabGroupsWnd* w) {
    w->model->Reload();
    w->listBox->SetModel(w->model);
}

void TabGroupsWnd::SaveTabGroup() {
    if (!editName) {
        return;
    }
    TempStr name = editName->GetTextTemp();
    if (str::IsEmptyOrWhiteSpace(name)) {
        return;
    }

    auto* group = AllocStruct<TabGroup>();
    group->name = str::Dup(name);
    group->tabFiles = new Vec<TabFile*>();

    for (WindowTab* tab : win->Tabs()) {
        if (tab->IsAboutTab()) {
            continue;
        }
        if (len(tab->filePath) == 0) {
            continue;
        }
        auto* tf = AllocStruct<TabFile>();
        str::ReplaceWithCopy(&tf->path, tab->filePath);
        VecAppend(*group->tabFiles, tf);
    }

    if (!gSettings->tabGroups) {
        gSettings->tabGroups = new Vec<TabGroup*>();
    }
    VecAppend(*gSettings->tabGroups, group);
    ScheduleSaveSettings();
    Close();
}

void TabGroupsWnd::OpenTabGroup() {
    int sel = listBox ? listBox->GetCurrentSelection() : -1;
    if (sel < 0) {
        return;
    }
    auto* groups = gSettings->tabGroups;
    if (!groups || sel >= len(*groups)) {
        return;
    }
    TabGroup* group = (*groups)[sel];
    if (!group->tabFiles || len(*group->tabFiles) == 0) {
        return;
    }

    // reuse current window if it has no files open (only about tab or empty)
    MainWindow* targetMain = win;
    bool hasFiles = false;
    for (WindowTab* tab : targetMain->Tabs()) {
        if (!tab->IsAboutTab()) {
            hasFiles = true;
            break;
        }
    }
    MainWindow* targetWin = hasFiles ? CreateAndShowMainWindow(nullptr) : targetMain;
    if (!targetWin) {
        return;
    }
    bool first = true;
    for (TabFile* tf : *group->tabFiles) {
        if (!tf->path.s) {
            continue;
        }
        LoadArgs args(tf->path, targetWin);
        if (!first) {
            args.forceReuse = false;
        }
        LoadDocument(&args);
        first = false;
    }
    // post WM_CLOSE instead of DestroyWindow so we return from the
    // listbox double-click callback before the dialog is torn down
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

void TabGroupsWnd::UpdateDeleteButton() {
    if (!btnDelete) {
        return;
    }
    int sel = listBox ? listBox->GetCurrentSelection() : -1;
    SetDlgButtonEnabled(btnDelete, DlgButtonStyle::Ghost, sel >= 0);
}

void TabGroupsWnd::DeleteTabGroup(VirtMouseEvent*) {
    int sel = listBox ? listBox->GetCurrentSelection() : -1;
    if (sel < 0) {
        return;
    }
    auto* groups = gSettings->tabGroups;
    if (!groups || sel >= len(*groups)) {
        return;
    }
    TabGroup* group = (*groups)[sel];
    VecRemove(*groups, group);
    FreeTabGroup(group);
    ScheduleSaveSettings();
    PopulateListBox(this);
    UpdateDeleteButton();
}

static void DrawTabGroupItem(TabGroupsWnd* w, VirtListBox::DrawItemEvent* ev) {
    if (ev->itemIndex < 0 || ev->itemIndex >= w->model->ItemsCount()) {
        return;
    }
    TempStr nTabs = fmt("%d tabs", w->model->TabCount(ev->itemIndex));
    DrawDlgListItem(ev->listBox, ev->gfx, ev->itemRect, ev->selected, w->model->Item(ev->itemIndex), nTabs);
}

static void OnListDoubleClick(TabGroupsWnd* w) {
    if (w->mode == TabGroupDialogMode::Open) {
        w->OpenTabGroup();
    } else {
        int sel = w->listBox ? w->listBox->GetCurrentSelection() : -1;
        if (sel >= 0 && w->editName) {
            auto* groups = gSettings->tabGroups;
            if (groups && sel < len(*groups)) {
                w->editName->SetText((*groups)[sel]->name);
                EditSelectAll(w->editName);
                EditSetFocus(w->editName);
            }
        }
    }
}

void TabGroupsWnd::ApplyDarkMode() {
    DarkModeApplyToWindowAndEraseBg(hwnd);
}

void TabGroupsWnd::OnCancel(VirtMouseEvent*) {
    Close();
}

void TabGroupsWnd::OnOk(VirtMouseEvent*) {
    if (mode == TabGroupDialogMode::Save) {
        SaveTabGroup();
    } else {
        OpenTabGroup();
    }
}

static void TeardownTabGroupsWnd(TabGroupsWnd* w) {
    if (!w || VecFind(gTabGroupsWnds, w) < 0) {
        return;
    }
    VecRemove(gTabGroupsWnds, w);
    w->model = nullptr;
    w->ScheduleDelete();
}

static void OnTabGroupsClose(WindowBase::CloseEvent* ev) {
    TeardownTabGroupsWnd((TabGroupsWnd*)ev->e->self);
}

static void OnTabGroupsDestroy(WindowBase::DestroyEvent* ev) {
    TeardownTabGroupsWnd((TabGroupsWnd*)ev->e->self);
}

bool TabGroupsWnd::Create(MainWindow* winIn, TabGroupDialogMode modeIn) {
    win = winIn;
    mode = modeIn;
    hwndParent = win->hwndFrame;
    bool isRtl = IsUIRtl();

    Str titleStr = (mode == TabGroupDialogMode::Save) ? Str(Tr("Save Tab Group")) : Str(Tr("Restore Tab Group"));
    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    SetFont(fontText);
    {
        CreateCustomArgs args;
        args.title = titleStr;
        args.visible = false;
        args.style = WS_OVERLAPPEDWINDOW;
        args.font = fontText;
        args.isRtl = isRtl;
        args.bgColor = colBg;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    SetColors(colTxt, colBg);

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;

    if (mode == TabGroupDialogMode::Save) {
        int groupNum = 1;
        if (gSettings->tabGroups) {
            groupNum = len(*gSettings->tabGroups) + 1;
        }
        editName = NewDlgEdit(hwnd, DlgUiFont(kToolDlgQueryFontPx), fmt("group #%d", groupNum), 0);
        HBox* row = NewDlgRow();
        row->minDy = DpiScale(kToolDlgLineDy);
        row->AddChild(editName, 1);
        vbox->AddChild(new Padding(row, DpiScaledInsets(0, kToolDlgPadDx)));
        vbox->AddChild(new VirtLine());
    }

    listBox = NewDlgList(GetDpi(), fontText);
    listBox->onDrawItem = MkFunc1(DrawTabGroupItem, this);
    listBox->onSelectionChanged = MkMethod0<TabGroupsWnd, &TabGroupsWnd::UpdateDeleteButton>(this);
    listBox->onDoubleClick = MkFunc0(OnListDoubleClick, this);
    model = new TabGroupsListBoxModel();
    PopulateListBox(this);
    vbox->AddChild(NewDlgListBox(listBox), 1);

    btnDelete = NewDlgButton(hwnd, Tr("Delete"), fontText, DlgButtonStyle::Ghost, false);
    btnDelete->onClick = MkMethod1<TabGroupsWnd, VirtMouseEvent*, &TabGroupsWnd::DeleteTabGroup>(this);
    btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<TabGroupsWnd, VirtMouseEvent*, &TabGroupsWnd::OnCancel>(this);
    Str okText = (mode == TabGroupDialogMode::Save) ? Str(Tr("Save")) : Str(Tr("Restore"));
    btnOk = NewDlgButton(hwnd, okText, fontText, DlgButtonStyle::Filled, true);
    btnOk->onClick = MkMethod1<TabGroupsWnd, VirtMouseEvent*, &TabGroupsWnd::OnOk>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnOk, btnDelete));
    layout = vbox;

    SetWindowPos(hwnd, nullptr, 0, 0, DpiScale(kTabGroupsDlgDx), DpiScale(kTabGroupsDlgDy), SWP_NOMOVE | SWP_NOZORDER);
    DoLayout();
    HwndCenterDialog(hwnd, hwndParent);
    HwndEnsureOnScreen(hwnd);
    UpdateTheme();
    UpdateDeleteButton();
    SetIsVisible(true);
    if (editName) {
        EditSelectAll(editName);
        EditSetFocus(editName);
    } else {
        // no name to type in when restoring: the list owns the keyboard
        SetFocusTo(listBox);
    }
    return true;
}

void ShowTabGroupsDialog(MainWindow* win, TabGroupDialogMode mode) {
    for (auto* w : gTabGroupsWnds) {
        if (w->win == win && w->mode == mode) {
            if (w->hwnd && IsWindow(w->hwnd)) {
                BringWindowToTop(w->hwnd);
                return;
            }
            TeardownTabGroupsWnd(w);
            break;
        }
    }

    auto* wnd = new TabGroupsWnd();
    wnd->SetFont(GetAppFont());
    wnd->closeOnEsc = true;
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnTabGroupsClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnTabGroupsDestroy);
    if (!wnd->Create(win, mode)) {
        delete wnd;
        return;
    }
    VecAppend(gTabGroupsWnds, wnd);
}
