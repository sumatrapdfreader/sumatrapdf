/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Translations.h"
#include "SumatraConfig.h"
#include "Theme.h"
#include "DarkModeSubclass.h"

constexpr int kPadding = 8;

enum class TabGroupDialogMode {
    Save,
    Open,
};

struct TabGroupsListBoxModel : ListBoxModel {
    Vec<TabGroup*> groups;

    void Reload() {
        groups.Reset();
        auto* g = gGlobalPrefs->tabGroups;
        if (g) {
            for (auto* tg : *g) {
                groups.Append(tg);
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
    void LayoutToClient();
    void UpdateTheme();
    void SaveTabGroup();
    void OpenTabGroup();
    void DeleteTabGroup(VirtMouseEvent* ev = nullptr);
    void UpdateDeleteButton();
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
    void OnSize(WindowBase::SizeEvent* ev);
    void OnKeyDown(KeyEvent* ev);
    void ScheduleDelete();
};

static Vec<TabGroupsWnd*> gTabGroupsWnds;

TabGroupsWnd::~TabGroupsWnd() = default;

static void DeleteTabGroupsWndInstance(TabGroupsWnd* w) {
    delete w;
}

void TabGroupsWnd::ScheduleDelete() {
    auto fn = MkFunc0<TabGroupsWnd>(DeleteTabGroupsWndInstance, this);
    uitask::Post(fn, "SafeDeleteTabGroupsWnd");
}

static void PopulateListBox(TabGroupsWnd* w) {
    w->model->Reload();
    w->listBox->SetModel(w->model);
}

void TabGroupsWnd::LayoutToClient() {
    if (!layout || !hwnd) {
        return;
    }
    // also picks up the virtual controls so we paint them and they get input
    DoLayout(HwndClientRect(hwnd).Size());
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
        if (!tab->filePath) {
            continue;
        }
        auto* tf = AllocStruct<TabFile>();
        str::ReplaceWithCopy(&tf->path, tab->filePath);
        group->tabFiles->Append(tf);
    }

    if (!gGlobalPrefs->tabGroups) {
        gGlobalPrefs->tabGroups = new Vec<TabGroup*>();
    }
    gGlobalPrefs->tabGroups->Append(group);
    SaveSettings();
    Close();
}

void TabGroupsWnd::OpenTabGroup() {
    int sel = listBox ? listBox->GetCurrentSelection() : -1;
    if (sel < 0) {
        return;
    }
    auto* groups = gGlobalPrefs->tabGroups;
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

static void FreeTabGroup(TabGroup* group) {
    if (!group) {
        return;
    }
    str::Free(group->name);
    if (group->tabFiles) {
        for (auto* tf : *group->tabFiles) {
            str::Free(tf->path);
            free(tf);
        }
        delete group->tabFiles;
    }
    free(group);
}

void TabGroupsWnd::UpdateDeleteButton() {
    if (!btnDelete) {
        return;
    }
    int sel = listBox ? listBox->GetCurrentSelection() : -1;
    btnDelete->SetIsEnabled(sel >= 0);
}

void TabGroupsWnd::DeleteTabGroup(VirtMouseEvent*) {
    int sel = listBox ? listBox->GetCurrentSelection() : -1;
    if (sel < 0) {
        return;
    }
    auto* groups = gGlobalPrefs->tabGroups;
    if (!groups || sel >= len(*groups)) {
        return;
    }
    TabGroup* group = (*groups)[sel];
    groups->Remove(group);
    FreeTabGroup(group);
    SaveSettings();
    PopulateListBox(this);
    UpdateDeleteButton();
}

static void DrawTabGroupItem(TabGroupsWnd* w, VirtListBox::DrawItemEvent* ev) {
    if (ev->itemIndex < 0 || ev->itemIndex >= w->model->ItemsCount()) {
        return;
    }

    VirtListBox* lb = ev->listBox;
    Gfx* gfx = ev->gfx;
    Rect rc = ev->itemRect;

    Color colBg = IsSpecialColor(lb->bgColor) ? GetSysColor(COLOR_WINDOW) : lb->bgColor;
    Color colText = IsSpecialColor(lb->textColor) ? GetSysColor(COLOR_WINDOWTEXT) : lb->textColor;
    if (ev->selected) {
        colBg = AccentColor(colBg, 30);
    }

    gfx->FillRect(rc, colBg);

    int padX = DpiScale(4);
    rc.x += padX;
    rc.dx -= 2 * padX;

    // draw group name on the left
    Str name = w->model->Item(ev->itemIndex);
    gfx->DrawText(name, rc, gfxTextVCenter | gfxTextLeft, lb->font, colText);

    // draw tab count on the right
    int nTabs = w->model->TabCount(ev->itemIndex);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d tabs", nTabs);
    gfx->DrawText(Str(buf), rc, gfxTextVCenter | gfxTextRight, lb->font, AccentColor(colText, 80));
}

static void OnListDoubleClick(TabGroupsWnd* w) {
    if (w->mode == TabGroupDialogMode::Open) {
        w->OpenTabGroup();
    } else {
        int sel = w->listBox ? w->listBox->GetCurrentSelection() : -1;
        if (sel >= 0 && w->editName) {
            auto* groups = gGlobalPrefs->tabGroups;
            if (groups && sel < len(*groups)) {
                w->editName->SetText((*groups)[sel]->name);
                w->editName->SelectAll();
                HwndSetFocus(w->editName->hwnd);
            }
        }
    }
}

void TabGroupsWnd::UpdateTheme() {
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    auto setColors = [&](ControlBase* c) {
        if (c) {
            c->SetColors(colTxt, colBg);
        }
    };
    setColors(editName);
    if (listBox) {
        listBox->textColor = colTxt;
        listBox->bgColor = colBg;
    }
    if (btnOk) {
        StyleThemedButton(btnOk, true);
        StyleThemedButton(btnDelete, false);
        StyleThemedButton(btnCancel, false);
    }
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
        DarkMode::setWindowEraseBgSubclass(hwnd);
    }
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
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

void TabGroupsWnd::OnSize(WindowBase::SizeEvent* ev) {
    if (ev->msg != WM_SIZE) {
        return;
    }
    LayoutToClient();
    HwndInvalidate(hwnd);
}

void TabGroupsWnd::OnKeyDown(KeyEvent* ev) {
    if (!hwnd) {
        return;
    }
    if (ev->hwnd != hwnd && !IsChild(hwnd, ev->hwnd)) {
        return;
    }
    if (ev->vkey == VK_RETURN && editName && ev->hwnd == editName->hwnd && mode == TabGroupDialogMode::Save) {
        TempStr name = editName->GetTextTemp();
        if (!str::IsEmptyOrWhiteSpace(name)) {
            SaveTabGroup();
            ev->didHandle = true;
            return;
        }
    }
    if (ev->vkey == VK_ESCAPE) {
        OnCancel();
        ev->didHandle = true;
    }
}

static void TeardownTabGroupsWnd(TabGroupsWnd* w) {
    if (!w || gTabGroupsWnds.Find(w) < 0) {
        return;
    }
    gTabGroupsWnds.Remove(w);
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

    Str titleStr = (mode == TabGroupDialogMode::Save) ? Str(_TRA("Save Tab Group")) : Str(_TRA("Restore Tab Group"));
    {
        CreateCustomArgs args;
        args.title = titleStr;
        args.visible = false;
        args.style = WS_OVERLAPPEDWINDOW;
        args.font = GetHFont();
        args.isRtl = isRtl;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    if (mode == TabGroupDialogMode::Save) {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = GetHFont();
        args.withBorder = true;
        args.isRtl = isRtl;
        int groupNum = 1;
        if (gGlobalPrefs->tabGroups) {
            groupNum = len(*gGlobalPrefs->tabGroups) + 1;
        }
        TempStr defaultName = fmt("group #%d", groupNum);
        args.text = defaultName;
        editName = new Edit();
        editName->Create(args);
        auto* editPad = new Padding(editName, DpiScaledInsets(0, 0, kPadding, 0));
        vbox->AddChild(editPad);
    }

    {
        listBox = new VirtListBox();
        listBox->dpi = GetDpi();
        listBox->font = font;
        listBox->onDrawItem = MkFunc1(DrawTabGroupItem, this);
        listBox->onSelectionChanged = MkMethod0<TabGroupsWnd, &TabGroupsWnd::UpdateDeleteButton>(this);
        listBox->onDoubleClick = MkFunc0(OnListDoubleClick, this);
        model = new TabGroupsListBoxModel();
        PopulateListBox(this);
        vbox->AddChild(listBox, 1);
    }

    {
        auto* btnRow = new HBox();
        btnRow->alignMain = MainAxisAlign::MainEnd;
        btnRow->alignCross = CrossAxisAlign::CrossCenter;

        Insets gap = DpiScaledInsets(0, 0, 0, 4);

        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), font, false);
        btnCancel->onClick = MkMethod1<TabGroupsWnd, VirtMouseEvent*, &TabGroupsWnd::OnCancel>(this);
        btnRow->AddChild(btnCancel);
        btnDelete = NewThemedButton(hwnd, _TRA("Delete"), font, false);
        btnDelete->onClick = MkMethod1<TabGroupsWnd, VirtMouseEvent*, &TabGroupsWnd::DeleteTabGroup>(this);
        btnDelete->SetIsEnabled(false);
        btnRow->AddChild(new Padding(btnDelete, gap));
        Str okText = (mode == TabGroupDialogMode::Save) ? Str(_TRA("Save")) : Str(_TRA("Restore"));
        btnOk = NewThemedButton(hwnd, okText, font, true);
        btnOk->onClick = MkMethod1<TabGroupsWnd, VirtMouseEvent*, &TabGroupsWnd::OnOk>(this);
        btnRow->AddChild(new Padding(btnOk, gap));
        vbox->AddChild(new Padding(btnRow, DpiScaledInsets(kPadding, 0, 0, 0)));
    }

    layout = new Padding(vbox, DpiScaledInsets(kPadding, kPadding));

    int winW = DpiScale(400);
    int winH = DpiScale(350);
    SetWindowPos(hwnd, nullptr, 0, 0, winW, winH, SWP_NOMOVE | SWP_NOZORDER);
    LayoutToClient();
    HwndCenterDialog(hwnd, hwndParent);
    HwndEnsureOnScreen(hwnd);
    UpdateTheme();
    UpdateDeleteButton();
    SetIsVisible(true);
    if (editName) {
        editName->SelectAll();
        HwndSetFocus(editName->hwnd);
    } else {
        // no name to type in when restoring: the list owns the keyboard
        SetFocusTo(listBox);
    }
    return true;
}

static void ShowTabGroupsDialog(MainWindow* win, TabGroupDialogMode mode) {
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
    wnd->SetFont(GetPlatformFont(GetAppFont()));
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnTabGroupsClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnTabGroupsDestroy);
    wnd->onSize = MkMethod1<TabGroupsWnd, WindowBase::SizeEvent*, &TabGroupsWnd::OnSize>(wnd);
    wnd->onKeyDown = MkMethod1<TabGroupsWnd, KeyEvent*, &TabGroupsWnd::OnKeyDown>(wnd);
    if (!wnd->Create(win, mode)) {
        delete wnd;
        return;
    }
    gTabGroupsWnds.Append(wnd);
}

void ShowSaveTabGroupDialog(MainWindow* win) {
    ShowTabGroupsDialog(win, TabGroupDialogMode::Save);
}

void ShowOpenTabGroupDialog(MainWindow* win) {
    ShowTabGroupsDialog(win, TabGroupDialogMode::Open);
}
