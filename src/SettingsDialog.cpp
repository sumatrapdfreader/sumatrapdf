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
#include "DisplayMode.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "FileHistory.h"
#include "FileThumbnails.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "AppTools.h"
#include "Translations.h"
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"

//   View
//   Default Layout:  [Automatic        v]
//   Default Zoom:    [Fit Page         v]
//   [ ] Show the bookmarks sidebar when available
//   [ ] Remember these settings for each document
//   ---------------------------------------------- hairline
//   Advanced
//   [ ] Use tabs
//   [ ] Automatically check for updates
//   [ ] Remember opened files
//   ---------------------------------------------- hairline
//   Set inverse search command line      (with TeX enhancements)
//   Enter the command line to invoke ...
//   [code -g "%f:%l"                     v]
//   ---------------------------------------------- hairline
//                                    [Cancel] [OK]
constexpr int kSettingsDlgDx = 480;
struct SettingsWnd : WindowBase {
    ~SettingsWnd() override = default;

    MainWindow* win = nullptr;
    Vec<float> zoomLevels;
    float startZoom = 0;
    bool showInverseSearch = false;

    VirtText* labelView = nullptr;
    VirtText* labelLayout = nullptr;
    VirtText* labelZoom = nullptr;
    VirtText* labelAdvanced = nullptr;
    VirtText* labelInverse = nullptr;
    VirtText* labelCmdLine = nullptr;

    DropDown* dropLayout = nullptr;
    DropDown* dropZoom = nullptr;
    DropDown* dropInverse = nullptr;

    Checkbox* chkShowToc = nullptr;
    Checkbox* chkRememberState = nullptr;
    Checkbox* chkUseTabs = nullptr;
    Checkbox* chkCheckUpdates = nullptr;
    Checkbox* chkRememberOpened = nullptr;

    VirtButton* btnCancel = nullptr;
    VirtButton* btnOk = nullptr;

    bool Create(MainWindow* win);
    void FillLayout();
    void FillZoom();
    void FillInverse();
    float SelectedZoom();
    void OnRememberOpenedChanged();

    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
};

static SettingsWnd* gSettingsWnd = nullptr;

static void ClearSettingsWnd() {
    gSettingsWnd = nullptr;
}

void SettingsWnd::FillLayout() {
    if (!dropLayout) {
        return;
    }
    StrVec items;
    items.Append(Tr("Automatic"));
    items.Append(Tr("Single Page"));
    items.Append(Tr("Facing"));
    items.Append(Tr("Book View"));
    items.Append(Tr("Continuous"));
    items.Append(Tr("Continuous Facing"));
    items.Append(Tr("Continuous Book View"));
    items.Append(Tr("Page Aspect"));
    dropLayout->SetItems(items);
    int sel = 0;
    if (gSettings && IsPageAspectDisplayMode(gSettings->defaultDisplayMode)) {
        sel = len(items) - 1;
    } else if (gSettings) {
        sel = (int)gSettings->defaultDisplayModeEnum - (int)DisplayMode::Automatic;
    }
    if (sel < 0 || sel >= len(items)) {
        sel = 0;
    }
    CbSetCurrentSelection(dropLayout, sel);
}

void SettingsWnd::FillZoom() {
    if (!dropZoom) {
        return;
    }
    startZoom = gSettings ? gSettings->defaultZoomFloat : 0;
    CollectZoomLevels(zoomLevels, false);
    StrVec items;
    for (float z : zoomLevels) {
        items.Append(ZoomLevelStrExact(z));
    }
    dropZoom->SetItems(items);
    int sel = -1;
    for (int i = 0; i < len(zoomLevels); i++) {
        if (zoomLevels[i] == startZoom) {
            sel = i;
            break;
        }
    }
    if (sel >= 0) {
        CbSetCurrentSelection(dropZoom, sel);
    } else {
        dropZoom->SetText(fmt("%.0f%%", startZoom));
    }
}

void SettingsWnd::FillInverse() {
    if (!dropInverse) {
        return;
    }
    StrVec items;
    Str cmdLine = gSettings ? gSettings->inverseSearchCmdLine : Str{};
    CollectInverseSearchCommands(items, cmdLine);
    if (len(cmdLine) == 0 && len(items) > 0) {
        cmdLine = items[0];
    }
    dropInverse->SetItems(items);
    if (len(cmdLine) == 0) {
        return;
    }
    int idx = items.Find(cmdLine);
    if (idx >= 0) {
        CbSetCurrentSelection(dropInverse, idx);
    } else {
        dropInverse->SetText(cmdLine);
    }
}

// Selected list entry, or a typed number (empty / non-numeric keeps startZoom).
float SettingsWnd::SelectedZoom() {
    int idx = CbGetCurrentSelection(dropZoom);
    if (idx >= 0 && idx < len(zoomLevels)) {
        float z = zoomLevels[idx];
        return z == 0 ? startZoom : z;
    }
    TempStr text = dropZoom ? dropZoom->GetTextTemp() : Str{};
    if (len(text) == 0) {
        return startZoom;
    }
    float zoom = (float)atof(CStrTemp(text));
    if (zoom == 0) {
        return startZoom;
    }
    return limitValue(zoom, kZoomMin, kZoomMax);
}

void SettingsWnd::OnRememberOpenedChanged() {
    if (!chkRememberState) {
        return;
    }
    bool on = chkRememberOpened && chkRememberOpened->IsChecked();
    chkRememberState->SetIsEnabled(on);
}

void SettingsWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

void SettingsWnd::OnOk(VirtMouseEvent*) {
    if (!gSettings) {
        ScheduleDelete();
        return;
    }
    int layoutIdx = CbGetCurrentSelection(dropLayout);
    int nLayout = dropLayout ? len(dropLayout->items) : 0;
    if (layoutIdx >= 0 && nLayout > 0 && layoutIdx == nLayout - 1) {
        str::ReplaceWithCopy(&gSettings->defaultDisplayMode, StrL("page aspect"));
        gSettings->defaultDisplayModeEnum = DisplayMode::Automatic;
    } else if (layoutIdx >= 0) {
        gSettings->defaultDisplayModeEnum = (DisplayMode)(layoutIdx + (int)DisplayMode::Automatic);
        str::ReplaceWithCopy(&gSettings->defaultDisplayMode, DisplayModeToString(gSettings->defaultDisplayModeEnum));
    }
    gSettings->defaultZoomFloat = SelectedZoom();
    if (chkShowToc) {
        gSettings->showToc = chkShowToc->IsChecked();
    }
    if (chkRememberState) {
        gSettings->rememberStatePerDocument = chkRememberState->IsChecked();
    }
    if (chkUseTabs) {
        gSettings->useTabs = chkUseTabs->IsChecked();
    }
    if (chkCheckUpdates) {
        gSettings->checkForUpdates = chkCheckUpdates->IsChecked();
    }
    if (chkRememberOpened) {
        gSettings->rememberOpenedFiles = chkRememberOpened->IsChecked();
    }
    if (showInverseSearch && dropInverse) {
        TempStr tmp = dropInverse->GetTextTemp();
        str::ReplaceWithCopy(&gSettings->inverseSearchCmdLine, tmp);
    }

    if (!SettingsRememberOpenedFiles()) {
        FileHistoryClear(true);
        EmptyThumbnailCacheDirectory();
    }
    UpdateDocumentColors();
    // note: ideally we would also update state for useTabs changes but that's complicated since
    // to do it right we would have to convert tabs to windows. When moving no tabs -> tabs,
    // there's no problem. When moving tabs -> no tabs, a half solution would be to only
    // call SetTabsInTitlebar() for windows that have only one tab, but that's somewhat inconsistent
    ApplySettingsToOpenWindows();
    ScheduleSaveSettings();
    MaybeRedrawHomePage();
    ScheduleDelete();
}

static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    if (gSettingsWnd) {
        gSettingsWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    if (gSettingsWnd) {
        gSettingsWnd->ScheduleDelete();
    }
}

static DropDown* MakeDropDown(HWND parent, PlatformFont* font, bool isRtl, bool editable) {
    DropDown::CreateArgs args;
    args.parent = parent;
    args.font = font;
    args.isRtl = isRtl;
    args.isEditable = editable;
    auto* c = new DropDown();
    c->Create(args);
    return c;
}

bool SettingsWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    showInverseSearch = gSettings && gSettings->enableTeXEnhancements && CanAccessDisk();
    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    PlatformFont* fontMeta = DlgUiFont(kToolDlgMetaFontPx);
    SetFont(fontText);
    bool isRtl = IsUIRtl();
    {
        CreateCustomArgs args;
        args.title = Tr("Settings");
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
    auto newSection = [&]() {
        auto* box = new VBox();
        box->alignCross = CrossAxisAlign::Stretch;
        return box;
    };
    auto header = [&](Str text) { return NewDlgLabel(text, fontMeta, colMuted, kToolDlgRowDy); };
    auto addCheck = [&](VBox* box, Checkbox* c) {
        HBox* row = NewDlgFieldRow();
        row->AddChild(c);
        box->AddChild(row);
    };
    bool sett = gSettings != nullptr;

    //[ ACCESSKEY_GROUP Settings Dialog
    VBox* view = newSection();
    labelView = header(Tr("View"));
    view->AddChild(labelView);
    {
        HBox* row = NewDlgFieldRow();
        labelLayout = NewDlgLabel(Tr("Default &Layout:"), fontText, colMuted, kToolDlgRowDy, true);
        row->AddChild(labelLayout);
        dropLayout = MakeDropDown(hwnd, fontText, isRtl, false);
        row->AddChild(dropLayout, 1);
        view->AddChild(row);

        row = NewDlgFieldRow();
        labelZoom = NewDlgLabel(Tr("Default &Zoom:"), fontText, colMuted, kToolDlgRowDy, true);
        row->AddChild(labelZoom);
        dropZoom = MakeDropDown(hwnd, fontText, isRtl, true);
        row->AddChild(dropZoom, 1);
        view->AddChild(row);

        Vec<VirtText*> labels;
        VecAppend(labels, labelLayout);
        VecAppend(labels, labelZoom);
        AlignDlgLabels(labels);
        FillLayout();
        FillZoom();
    }
    chkShowToc =
        NewDlgCheckbox(hwnd, Tr("Show the &bookmarks sidebar when available"), fontText, sett && gSettings->showToc);
    addCheck(view, chkShowToc);
    chkRememberState = NewDlgCheckbox(hwnd, Tr("&Remember these settings for each document"), fontText,
                                      sett && gSettings->rememberStatePerDocument);
    if (sett && !gSettings->rememberOpenedFiles) {
        chkRememberState->SetIsEnabled(false);
    }
    addCheck(view, chkRememberState);
    vbox->AddChild(NewDlgSection(view));
    vbox->AddChild(new VirtLine());

    VBox* advanced = newSection();
    labelAdvanced = header(Tr("Advanced"));
    advanced->AddChild(labelAdvanced);
    chkUseTabs = NewDlgCheckbox(hwnd, Tr("Use &tabs"), fontText, sett && gSettings->useTabs);
    addCheck(advanced, chkUseTabs);
    chkCheckUpdates =
        NewDlgCheckbox(hwnd, Tr("Automatically check for &updates"), fontText, sett && gSettings->checkForUpdates);
    if (!HasPermission(Perm::InternetAccess)) {
        chkCheckUpdates->SetIsEnabled(false);
    }
    addCheck(advanced, chkCheckUpdates);
    chkRememberOpened =
        NewDlgCheckbox(hwnd, Tr("Remember &opened files"), fontText, sett && gSettings->rememberOpenedFiles);
    chkRememberOpened->onStateChanged = MkMethod0<SettingsWnd, &SettingsWnd::OnRememberOpenedChanged>(this);
    addCheck(advanced, chkRememberOpened);
    vbox->AddChild(NewDlgSection(advanced));

    if (showInverseSearch) {
        vbox->AddChild(new VirtLine());
        VBox* inverse = newSection();
        labelInverse = header(Tr("Set inverse search command line"));
        inverse->AddChild(labelInverse);
        labelCmdLine = NewDlgLabel(Tr("Enter the command line to invoke when you double-click on the PDF document:"),
                                   fontText, colMuted, kToolDlgRowDy);
        inverse->AddChild(labelCmdLine);
        dropInverse = MakeDropDown(hwnd, fontText, isRtl, true);
        inverse->AddChild(new Padding(dropInverse, DpiScaledInsets(0, 0, kToolDlgFilesPadDy, 0)));
        vbox->AddChild(NewDlgSection(inverse));
        FillInverse();
    }
    //] ACCESSKEY_GROUP Settings Dialog

    btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<SettingsWnd, VirtMouseEvent*, &SettingsWnd::OnCancel>(this);
    btnOk = NewDlgButton(hwnd, Tr("OK"), fontText, DlgButtonStyle::Filled, true);
    btnOk->onClick = MkMethod1<SettingsWnd, VirtMouseEvent*, &SettingsWnd::OnOk>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnOk));
    layout = vbox;

    LayoutAndSizeToContent(layout, DpiScale(kSettingsDlgDx), 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    if (dropLayout) {
        HwndSetFocus(dropLayout->hwnd);
    }
    return true;
}

void ShowSettingsDialog(MainWindow* win) {
    if (!HasPermission(Perm::SavePreferences)) {
        return;
    }
    if (gSettingsWnd) {
        HwndSetFocus(gSettingsWnd->hwnd);
        if (gSettingsWnd->dropLayout) {
            HwndSetFocus(gSettingsWnd->dropLayout->hwnd);
        }
        return;
    }
    auto* wnd = new SettingsWnd();
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearSettingsWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gSettingsWnd = wnd;
}
