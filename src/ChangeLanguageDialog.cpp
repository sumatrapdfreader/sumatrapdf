/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/UITask.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "GlobalPrefs.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkModeSubclass.h"
#include "ChangeLanguageDialog.h"

// Search edit is a real HWND; the language list and buttons are VirtCtrl.
// Same layout / WindowBase pattern as Change Theme.
struct ChangeLanguageWnd : WindowBase {
    ~ChangeLanguageWnd() override = default;

    MainWindow* win = nullptr;
    Edit* editSearch = nullptr;
    VirtListBox* listBox = nullptr;
    ListBoxModelStrings* model = nullptr; // owned by listBox
    VirtButton* btnCancel = nullptr;
    VirtButton* btnOk = nullptr;
    Vec<int> langIdxByListIdx;

    bool Create(MainWindow* win);
    void OnKeyDown(KeyEvent* ev);

    void FilterList();
    void UpdateTheme();
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
    void OnListDoubleClick();
    void ScheduleDelete();
};

static ChangeLanguageWnd* gChangeLanguageWnd = nullptr;

void SafeDeleteChangeLanguageDialog() {
    if (!gChangeLanguageWnd) {
        return;
    }
    auto* tmp = gChangeLanguageWnd;
    gChangeLanguageWnd = nullptr;
    delete tmp;
}

void ChangeLanguageWnd::ScheduleDelete() {
    if (gChangeLanguageWnd != this) {
        return;
    }
    auto fn = MkFunc0Void(SafeDeleteChangeLanguageDialog);
    uitask::Post(fn, "SafeDeleteChangeLanguageDialog");
}

void ChangeLanguageWnd::UpdateTheme() {
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    if (editSearch) {
        editSearch->SetColors(colTxt, colBg);
    }
    if (listBox) {
        listBox->textColor = colTxt;
        listBox->bgColor = colBg;
    }
    if (btnCancel) {
        StyleThemedButton(btnCancel, false);
    }
    if (btnOk) {
        StyleThemedButton(btnOk, true);
    }
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
    }
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

// Rebuild the list from the search box; keep the current UI language selected
// when it still matches the filter.
void ChangeLanguageWnd::FilterList() {
    if (!listBox || !model) {
        return;
    }
    TempStr filter = editSearch ? editSearch->GetTextTemp() : Str{};
    model->strings.Reset();
    langIdxByListIdx.Reset();
    int itemToSelect = 0;
    Str currLangCode = trans::GetCurrentLangCode();
    for (int i = 0; i < trans::GetLangsCount(); i++) {
        TempStr name = trans::GetLangNameByIdxTemp(i);
        if (filter && !str::ContainsI(name, filter)) {
            continue;
        }
        model->strings.Append(name);
        if (str::Eq(trans::GetLangCodeByIdxTemp(i), currLangCode)) {
            itemToSelect = len(langIdxByListIdx);
        }
        langIdxByListIdx.Append(i);
    }
    listBox->SetModel(model);
    if (len(langIdxByListIdx) > 0) {
        listBox->SetCurrentSelection(itemToSelect);
    }
}

void ChangeLanguageWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

void ChangeLanguageWnd::OnListDoubleClick() {
    OnOk();
}

void ChangeLanguageWnd::OnOk(VirtMouseEvent*) {
    int idx = listBox ? listBox->GetCurrentSelection() : -1;
    if (idx >= 0 && idx < len(langIdxByListIdx)) {
        int langIdx = langIdxByListIdx[idx];
        SetCurrentLanguageAndRefreshUI(trans::GetLangCodeByIdxTemp(langIdx));
    }
    ScheduleDelete();
}

void ChangeLanguageWnd::OnKeyDown(KeyEvent* ev) {
    if (ev->vkey == VK_ESCAPE) {
        OnCancel();
        ev->didHandle = true;
        return;
    }
    if (ev->vkey == VK_RETURN) {
        if (btnCancel && vroot && vroot->focused == btnCancel) {
            OnCancel();
        } else {
            OnOk();
        }
        ev->didHandle = true;
        return;
    }
    // Up/Down from the search box move the list, like the command palette
    bool fromSearch = editSearch && ev->hwnd == editSearch->hwnd;
    if (fromSearch && listBox && (ev->vkey == VK_UP || ev->vkey == VK_DOWN)) {
        int n = listBox->ItemsCount();
        if (n > 0) {
            int sel = listBox->GetCurrentSelection();
            if (ev->vkey == VK_UP) {
                sel = sel <= 0 ? n - 1 : sel - 1;
            } else {
                sel = sel < 0 || sel >= n - 1 ? 0 : sel + 1;
            }
            listBox->SetCurrentSelection(sel);
        }
        ev->didHandle = true;
        return;
    }
}

static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    if (gChangeLanguageWnd) {
        gChangeLanguageWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    if (gChangeLanguageWnd) {
        gChangeLanguageWnd->ScheduleDelete();
    }
}

bool ChangeLanguageWnd::Create(MainWindow* mainWin) {
    win = mainWin;

    {
        CreateCustomArgs args;
        args.title = _TRA("Change Language");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = GetHFont();
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = GetHFont();
        args.withBorder = true;
        args.isRtl = IsUIRtl();
        auto* c = new Edit();
        c->Create(args);
        c->onTextChanged = MkMethod0<ChangeLanguageWnd, &ChangeLanguageWnd::FilterList>(this);
        editSearch = c;
        vbox->AddChild(c);
    }

    {
        auto* c = new VirtListBox();
        c->dpi = GetDpi();
        c->font = font;
        c->idealSizeLines = 16;
        listBox = c;
        model = new ListBoxModelStrings();
        c->onDoubleClick = MkMethod0<ChangeLanguageWnd, &ChangeLanguageWnd::OnListDoubleClick>(this);
        vbox->AddChild(c);
        FilterList();
    }

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{4, 8, 4, 8};

        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), font, false);
        btnCancel->onClick = MkMethod1<ChangeLanguageWnd, VirtMouseEvent*, &ChangeLanguageWnd::OnCancel>(this);
        hbox->AddChild(new Padding(btnCancel, pad));
        btnOk = NewThemedButton(hwnd, _TRA("OK"), font, true);
        btnOk->onClick = MkMethod1<ChangeLanguageWnd, VirtMouseEvent*, &ChangeLanguageWnd::OnOk>(this);
        hbox->AddChild(new Padding(btnOk, pad));
        vbox->AddChild(hbox);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    int dx = DpiScale(280);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    if (editSearch) {
        HwndSetFocus(editSearch->hwnd);
    }
    return true;
}

void ShowChangeLanguageDialog(MainWindow* win) {
    if (gChangeLanguageWnd) {
        HwndSetFocus(gChangeLanguageWnd->hwnd);
        return;
    }
    auto* wnd = new ChangeLanguageWnd();
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->onKeyDown = MkMethod1<ChangeLanguageWnd, KeyEvent*, &ChangeLanguageWnd::OnKeyDown>(wnd);
    wnd->SetFont(GetPlatformFont(GetAppFont()));
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gChangeLanguageWnd = wnd;
}
