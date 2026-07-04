/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Dpi.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

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

constexpr const WCHAR* kTabGroupsWinClassName = L"SUMATRA_PDF_TAB_GROUPS";

constexpr int kButtonAreaDy = 40;
constexpr int kButtonPadding = 8;
constexpr int kEditHeight = 24;
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

struct TabGroupsDialog {
    HWND hwnd = nullptr;
    HWND hwndParent = nullptr;
    HWND hwndEdit = nullptr;
    ListBox* listBox = nullptr;
    TabGroupsListBoxModel* model = nullptr;
    Button* btnOk = nullptr;
    Button* btnDelete = nullptr;
    Button* btnCancel = nullptr;
    TabGroupDialogMode mode = TabGroupDialogMode::Save;
    MainWindow* win = nullptr;
};

static Vec<TabGroupsDialog*> gTabGroupsDialogs;

static int ButtonPadding(HWND hwnd) {
    return DpiScale(hwnd, kButtonPadding);
}

static int ButtonAreaDy(TabGroupsDialog* d) {
    int padding = ButtonPadding(d->hwnd);
    int buttonAreaDy = DpiScale(d->hwnd, kButtonAreaDy);
    if (d->btnOk) {
        buttonAreaDy = std::max(buttonAreaDy, d->btnOk->GetIdealSize().dy + 2 * padding);
    }
    if (d->btnDelete) {
        buttonAreaDy = std::max(buttonAreaDy, d->btnDelete->GetIdealSize().dy + 2 * padding);
    }
    if (d->btnCancel) {
        buttonAreaDy = std::max(buttonAreaDy, d->btnCancel->GetIdealSize().dy + 2 * padding);
    }
    return buttonAreaDy;
}

static TabGroupsDialog* FindDialog(HWND hwnd) {
    for (auto* d : gTabGroupsDialogs) {
        if (d->hwnd == hwnd) {
            return d;
        }
    }
    return nullptr;
}

static void PopulateListBox(TabGroupsDialog* d) {
    d->model->Reload();
    d->listBox->SetModel(d->model);
}

static void LayoutControls(TabGroupsDialog* d) {
    Rect rc = ClientRect(d->hwnd);
    int padding = DpiScale(d->hwnd, kPadding);
    int buttonPadding = ButtonPadding(d->hwnd);
    int buttonAreaDy = ButtonAreaDy(d);
    int y = padding;
    int x = padding;
    int dx = rc.dx - 2 * padding;

    if (d->mode == TabGroupDialogMode::Save && d->hwndEdit) {
        int editHeight = DpiScale(d->hwnd, kEditHeight);
        MoveWindow(d->hwndEdit, x, y, dx, editHeight, TRUE);
        y += editHeight + padding;
    }

    int lbDy = rc.dy - y - buttonAreaDy;
    if (lbDy < 20) {
        lbDy = 20;
    }
    if (d->listBox) {
        MoveWindow(d->listBox->hwnd, x, y, dx, lbDy, TRUE);
    }

    // buttons at the bottom right: [Save/Open] [Delete] [Cancel]
    Size okSize = d->btnOk->GetIdealSize();
    Size cancelSize = d->btnCancel->GetIdealSize();
    int btnY = rc.dy - buttonPadding - okSize.dy;
    int btnX = rc.dx - buttonPadding - cancelSize.dx;
    MoveWindow(d->btnCancel->hwnd, btnX, btnY, cancelSize.dx, cancelSize.dy, TRUE);
    if (d->btnDelete) {
        Size deleteSize = d->btnDelete->GetIdealSize();
        btnX -= buttonPadding + deleteSize.dx;
        MoveWindow(d->btnDelete->hwnd, btnX, btnY, deleteSize.dx, deleteSize.dy, TRUE);
    }
    btnX -= buttonPadding + okSize.dx;
    MoveWindow(d->btnOk->hwnd, btnX, btnY, okSize.dx, okSize.dy, TRUE);
}

static void SaveTabGroup(TabGroupsDialog* d) {
    if (!d->hwndEdit) {
        return;
    }
    int n = GetWindowTextLengthW(d->hwndEdit);
    if (n <= 0) {
        return;
    }
    TempWStr buf = WStr(AllocArrayTemp<WCHAR>(n + 1), (int)n + 1);
    GetWindowTextW(d->hwndEdit, buf.s, n + 1);
    TempStr name = ToUtf8Temp(buf);

    auto* group = AllocStruct<TabGroup>();
    group->name = str::Dup(name);
    group->tabFiles = new Vec<TabFile*>();

    for (WindowTab* tab : d->win->Tabs()) {
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
    PostMessageW(d->hwnd, WM_CLOSE, 0, 0);
}

static void OpenTabGroup(TabGroupsDialog* d) {
    int sel = d->listBox ? d->listBox->GetCurrentSelection() : -1;
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
    MainWindow* win = d->win;
    bool hasFiles = false;
    for (WindowTab* tab : win->Tabs()) {
        if (!tab->IsAboutTab()) {
            hasFiles = true;
            break;
        }
    }
    MainWindow* targetWin = hasFiles ? CreateAndShowMainWindow(nullptr) : win;
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
    PostMessageW(d->hwnd, WM_CLOSE, 0, 0);
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

static void UpdateDeleteButton(TabGroupsDialog* d) {
    if (!d->btnDelete) {
        return;
    }
    int sel = d->listBox ? d->listBox->GetCurrentSelection() : -1;
    EnableWindow(d->btnDelete->hwnd, sel >= 0);
}

static void DeleteTabGroup(TabGroupsDialog* d) {
    int sel = d->listBox ? d->listBox->GetCurrentSelection() : -1;
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
    PopulateListBox(d);
    UpdateDeleteButton(d);
}

static void DrawTabGroupItem(TabGroupsDialog* d, ListBox::DrawItemEvent* ev) {
    if (ev->itemIndex < 0 || ev->itemIndex >= d->model->ItemsCount()) {
        return;
    }

    HDC hdc = ev->hdc;
    RECT rc = ev->itemRect;
    ListBox* lb = ev->listBox;

    COLORREF colBg = IsSpecialColor(lb->bgColor) ? GetSysColor(COLOR_WINDOW) : lb->bgColor;
    COLORREF colText = IsSpecialColor(lb->textColor) ? GetSysColor(COLOR_WINDOWTEXT) : lb->textColor;
    if (ev->selected) {
        colBg = AccentColor(colBg, 30);
    }

    SetBkColor(hdc, colBg);
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, &rc, nullptr, 0, nullptr);

    SetTextColor(hdc, colText);
    SetBkMode(hdc, TRANSPARENT);

    HFONT oldFont = nullptr;
    if (lb->font) {
        oldFont = SelectFont(hdc, lb->font);
    }

    int padX = DpiScale(lb->hwnd, 4);
    rc.left += padX;
    rc.right -= padX;

    // draw group name on the left
    Str name = d->model->Item(ev->itemIndex);
    WCHAR* nameW = CWStrTemp(name);
    uint fmt = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_LEFT;
    DrawTextW(hdc, nameW, -1, &rc, fmt);

    // draw tab count on the right
    int nTabs = d->model->TabCount(ev->itemIndex);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d tabs", nTabs);
    WCHAR* countW = CWStrTemp(buf);
    COLORREF rightCol = AccentColor(colText, 80);
    SetTextColor(hdc, rightCol);
    RECT rcRight = rc;
    fmt = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_RIGHT;
    DrawTextW(hdc, countW, -1, &rcRight, fmt);

    if (oldFont) {
        SelectFont(hdc, oldFont);
    }
}

static void OnListSelectionChanged(TabGroupsDialog* d) {
    UpdateDeleteButton(d);
}

static void OnListDoubleClick(TabGroupsDialog* d) {
    if (d->mode == TabGroupDialogMode::Open) {
        OpenTabGroup(d);
    } else {
        int sel = d->listBox ? d->listBox->GetCurrentSelection() : -1;
        if (sel >= 0 && d->hwndEdit) {
            auto* groups = gGlobalPrefs->tabGroups;
            if (groups && sel < len(*groups)) {
                HwndSetText(d->hwndEdit, (*groups)[sel]->name);
                SendMessageW(d->hwndEdit, EM_SETSEL, 0, -1);
                SetFocus(d->hwndEdit);
            }
        }
    }
}

static LRESULT CALLBACK WndProcTabGroups(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    LRESULT res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res) {
        return res;
    }

    TabGroupsDialog* d = FindDialog(hwnd);

    switch (msg) {
        case WM_SIZE:
            if (d) {
                LayoutControls(d);
            }
            return 0;

        case WM_COMMAND:
            break;

        case WM_CHAR:
            if (VK_ESCAPE == wp) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_DESTROY:
            return 0;

        case WM_NCDESTROY:
            if (d) {
                gTabGroupsDialogs.Remove(d);
                // prevent double-free: ListBox::~ListBox deletes the model
                d->model = nullptr;
                delete d->listBox;
                delete d->btnOk;
                delete d->btnDelete;
                delete d->btnCancel;
                delete d;
            }
            return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

static void OnCancel(TabGroupsDialog* d) {
    PostMessageW(d->hwnd, WM_CLOSE, 0, 0);
}

static void OnOk(TabGroupsDialog* d) {
    if (d->mode == TabGroupDialogMode::Save) {
        SaveTabGroup(d);
    } else {
        OpenTabGroup(d);
    }
}

static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR dwRefData) {
    if (msg == WM_KEYDOWN && wp == VK_RETURN) {
        auto* d = (TabGroupsDialog*)dwRefData;
        if (d->mode == TabGroupDialogMode::Save) {
            int n = GetWindowTextLengthW(d->hwndEdit);
            if (n > 0) {
                SaveTabGroup(d);
                return 0;
            }
        }
    }
    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, EditSubclassProc, 0);
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void ShowTabGroupsDialog(MainWindow* win, TabGroupDialogMode mode) {
    // find existing dialog for this mode and bring to front
    for (auto* d : gTabGroupsDialogs) {
        if (d->win == win && d->mode == mode) {
            BringWindowToTop(d->hwnd);
            return;
        }
    }

    HMODULE h = GetModuleHandleW(nullptr);
    WNDCLASSEX wcex = {};
    FillWndClassEx(wcex, kTabGroupsWinClassName, WndProcTabGroups);
    wcex.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wcex.hIcon = LoadIconW(h, MAKEINTRESOURCEW(GetAppIconID()));
    RegisterClassEx(&wcex);

    bool isRtl = IsUIRtl();

    Str titleStr = (mode == TabGroupDialogMode::Save) ? Str(_TRA("Save Tab Group")) : Str(_TRA("Restore Tab Group"));
    WCHAR* title = CWStrTemp(titleStr);

    DWORD dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    HWND hwnd = CreateWindowExW(0, kTabGroupsWinClassName, title, dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, 400, 350,
                                nullptr, nullptr, h, nullptr);
    if (!hwnd) {
        return;
    }

    auto* d = new TabGroupsDialog();
    d->hwnd = hwnd;
    d->hwndParent = win->hwndFrame;
    d->mode = mode;
    d->win = win;
    gTabGroupsDialogs.Append(d);

    HwndSetRtl(hwnd, isRtl);

    HFONT hFont = GetDefaultGuiFont();

    // edit control (only in save mode)
    if (mode == TabGroupDialogMode::Save) {
        DWORD editStyle = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
        d->hwndEdit =
            CreateWindowExW(WS_EX_CLIENTEDGE, WC_EDITW, L"", editStyle, 0, 0, 0, 0, hwnd, nullptr, h, nullptr);
        SendMessageW(d->hwndEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

        // pre-populate with "group #<n>"
        int groupNum = 1;
        if (gGlobalPrefs->tabGroups) {
            groupNum = len(*gGlobalPrefs->tabGroups) + 1;
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "group #%d", groupNum);
        HwndSetText(d->hwndEdit, buf);
        SendMessageW(d->hwndEdit, EM_SETSEL, 0, -1);
        SetWindowSubclass(d->hwndEdit, EditSubclassProc, 0, (DWORD_PTR)d);
    }

    // listbox
    {
        ListBox::CreateArgs lbArgs;
        lbArgs.parent = hwnd;
        lbArgs.font = hFont;
        lbArgs.isRtl = isRtl;
        auto* lb = new ListBox();
        lb->onDrawItem = MkFunc1(DrawTabGroupItem, d);
        lb->onSelectionChanged = MkFunc0(OnListSelectionChanged, d);
        lb->onDoubleClick = MkFunc0(OnListDoubleClick, d);
        lb->Create(lbArgs);
        d->listBox = lb;
        d->model = new TabGroupsListBoxModel();
        PopulateListBox(d);
    }

    // buttons
    {
        Str okText = (mode == TabGroupDialogMode::Save) ? Str(_TRA("Save")) : Str(_TRA("Restore"));
        Button::CreateArgs args;
        args.parent = hwnd;
        args.text = okText;
        args.isRtl = isRtl;
        auto b = new Button();
        b->Create(args);
        d->btnOk = b;
        b->onClick = MkFunc0(OnOk, d);
    }
    {
        Button::CreateArgs args;
        args.parent = hwnd;
        args.text = _TRA("Delete");
        args.isRtl = isRtl;
        auto b = new Button();
        b->Create(args);
        d->btnDelete = b;
        b->onClick = MkFunc0(DeleteTabGroup, d);
        EnableWindow(b->hwnd, FALSE);
    }
    {
        Button::CreateArgs args;
        args.parent = hwnd;
        args.text = _TRA("Cancel");
        args.isRtl = isRtl;
        auto b = new Button();
        b->Create(args);
        d->btnCancel = b;
        b->onClick = MkFunc0(OnCancel, d);
    }

    LayoutControls(d);
    CenterDialog(hwnd, win->hwndFrame);
    HwndEnsureVisible(hwnd);
    ShowWindow(hwnd, SW_SHOW);

    if (d->hwndEdit) {
        SetFocus(d->hwndEdit);
    }
}

void ShowSaveTabGroupDialog(MainWindow* win) {
    ShowTabGroupsDialog(win, TabGroupDialogMode::Save);
}

void ShowOpenTabGroupDialog(MainWindow* win) {
    ShowTabGroupsDialog(win, TabGroupDialogMode::Open);
}
