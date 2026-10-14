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
#include "Translations.h"
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"

//   Search
//   ---------------------------------------------- hairline
//   English
//   Deutsch                       languages, the current one selected
//   ---------------------------------------------- hairline
//                                    [Cancel] [OK]
constexpr int kLangDlgDx = 320;
constexpr int kLangListLines = 16;
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
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
    void OnListDoubleClick();
};

static ChangeLanguageWnd* gChangeLanguageWnd = nullptr;

static void ClearChangeLanguageWnd() {
    gChangeLanguageWnd = nullptr;
}

// Rebuild the list from the search box; keep the current UI language selected
// when it still matches the filter.
void ChangeLanguageWnd::FilterList() {
    if (!listBox || !model) {
        return;
    }
    TempStr filter = editSearch ? editSearch->GetTextTemp() : Str{};
    model->strings.Reset();
    VecReset(langIdxByListIdx);
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
        VecAppend(langIdxByListIdx, i);
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
    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    SetFont(fontText);
    bool isRtl = IsUIRtl();
    {
        CreateCustomArgs args;
        args.owner = win ? win->hwndFrame : nullptr;
        args.title = Tr("Change Language");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
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

    // the search, in the larger font, as the palette has it
    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = DlgUiFont(kToolDlgQueryFontPx);
        args.withBorder = false;
        args.isRtl = isRtl;
        editSearch = new Edit();
        editSearch->SetColors(colTxt, colBg);
        editSearch->Create(args);
        // the hint stays while the empty field has the focus, as it always does here
        Edit_SetCueBannerTextFocused(editSearch->hwnd, CWStrTemp(Tr("Search")), TRUE);
        editSearch->onTextChanged = MkMethod0<ChangeLanguageWnd, &ChangeLanguageWnd::FilterList>(this);
        HBox* row = NewDlgRow();
        row->AddChild(new Spacer(0, DpiScale(kToolDlgLineDy)));
        row->AddChild(editSearch, 1);
        vbox->AddChild(new Padding(row, DpiScaledInsets(0, kToolDlgPadDx)));
    }
    vbox->AddChild(new VirtLine());

    listBox = NewDlgList(GetDpi(), fontText);
    listBox->idealSizeLines = kLangListLines;
    model = new ListBoxModelStrings();
    listBox->onDoubleClick = MkMethod0<ChangeLanguageWnd, &ChangeLanguageWnd::OnListDoubleClick>(this);
    vbox->AddChild(NewDlgListBox(listBox));
    FilterList();

    btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<ChangeLanguageWnd, VirtMouseEvent*, &ChangeLanguageWnd::OnCancel>(this);
    btnOk = NewDlgButton(hwnd, Tr("OK"), fontText, DlgButtonStyle::Filled, true);
    btnOk->onClick = MkMethod1<ChangeLanguageWnd, VirtMouseEvent*, &ChangeLanguageWnd::OnOk>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnOk));
    layout = vbox;

    LayoutAndSizeToContent(layout, DpiScale(kLangDlgDx), 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    EditSetFocus(editSearch);
    return true;
}

void ShowChangeLanguageDialog(MainWindow* win) {
    if (gChangeLanguageWnd) {
        HwndSetFocus(gChangeLanguageWnd->hwnd);
        return;
    }
    auto* wnd = new ChangeLanguageWnd();
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearChangeLanguageWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->onKeyDown = MkMethod1<ChangeLanguageWnd, KeyEvent*, &ChangeLanguageWnd::OnKeyDown>(wnd);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gChangeLanguageWnd = wnd;
    RunModalWindow(wnd->hwnd, win ? win->hwndFrame : nullptr);
}
