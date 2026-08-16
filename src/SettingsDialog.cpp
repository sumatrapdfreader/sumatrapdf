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
#include "GlobalPrefs.h"
#include "MainWindow.h"
#include "FileHistory.h"
#include "FileThumbnails.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "AppTools.h"
#include "Translations.h"
#include "DarkMode_win.h"
#include "SettingsDialog.h"

// Section headers, labels and OK/Cancel are VirtCtrl; layout/zoom/command
// combos and the checkboxes are HWNDs. Same WindowBase layout as Inverse Search.
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
    items.Append(_TRA("Automatic"));
    items.Append(_TRA("Single Page"));
    items.Append(_TRA("Facing"));
    items.Append(_TRA("Book View"));
    items.Append(_TRA("Continuous"));
    items.Append(_TRA("Continuous Facing"));
    items.Append(_TRA("Continuous Book View"));
    items.Append(_TRA("Page Aspect"));
    dropLayout->SetItems(items);
    int sel = 0;
    if (gGlobalPrefs && IsPageAspectDisplayMode(gGlobalPrefs->defaultDisplayMode)) {
        sel = len(items) - 1;
    } else if (gGlobalPrefs) {
        sel = (int)gGlobalPrefs->defaultDisplayModeEnum - (int)DisplayMode::Automatic;
    }
    if (sel < 0 || sel >= len(items)) {
        sel = 0;
    }
    dropLayout->SetCurrentSelection(sel);
}

void SettingsWnd::FillZoom() {
    if (!dropZoom) {
        return;
    }
    startZoom = gGlobalPrefs ? gGlobalPrefs->defaultZoomFloat : 0;
    CollectZoomLevels(zoomLevels, false);
    StrVec items;
    for (float z : zoomLevels) {
        items.Append(ZoomLevelStr(z));
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
        dropZoom->SetCurrentSelection(sel);
    } else {
        dropZoom->SetText(fmt("%.0f%%", startZoom));
    }
}

void SettingsWnd::FillInverse() {
    if (!dropInverse) {
        return;
    }
    StrVec items;
    Str cmdLine = gGlobalPrefs ? gGlobalPrefs->inverseSearchCmdLine : Str{};
    CollectInverseSearchCommands(items, cmdLine);
    if (!cmdLine && len(items) > 0) {
        cmdLine = items[0];
    }
    dropInverse->SetItems(items);
    if (!cmdLine) {
        return;
    }
    int idx = items.Find(cmdLine);
    if (idx >= 0) {
        dropInverse->SetCurrentSelection(idx);
    } else {
        dropInverse->SetText(cmdLine);
    }
}

// Selected list entry, or a typed number (empty / non-numeric keeps startZoom).
float SettingsWnd::SelectedZoom() {
    int idx = dropZoom ? dropZoom->GetCurrentSelection() : -1;
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
    if (!gGlobalPrefs) {
        ScheduleDelete();
        return;
    }
    int layoutIdx = dropLayout ? dropLayout->GetCurrentSelection() : -1;
    int nLayout = dropLayout ? len(dropLayout->items) : 0;
    if (layoutIdx >= 0 && nLayout > 0 && layoutIdx == nLayout - 1) {
        str::ReplaceWithCopy(&gGlobalPrefs->defaultDisplayMode, StrL("page aspect"));
        gGlobalPrefs->defaultDisplayModeEnum = DisplayMode::Automatic;
    } else if (layoutIdx >= 0) {
        gGlobalPrefs->defaultDisplayModeEnum = (DisplayMode)(layoutIdx + (int)DisplayMode::Automatic);
        str::ReplaceWithCopy(&gGlobalPrefs->defaultDisplayMode,
                             DisplayModeToString(gGlobalPrefs->defaultDisplayModeEnum));
    }
    gGlobalPrefs->defaultZoomFloat = SelectedZoom();
    if (chkShowToc) {
        gGlobalPrefs->showToc = chkShowToc->IsChecked();
    }
    if (chkRememberState) {
        gGlobalPrefs->rememberStatePerDocument = chkRememberState->IsChecked();
    }
    if (chkUseTabs) {
        gGlobalPrefs->useTabs = chkUseTabs->IsChecked();
    }
    if (chkCheckUpdates) {
        gGlobalPrefs->checkForUpdates = chkCheckUpdates->IsChecked();
    }
    if (chkRememberOpened) {
        gGlobalPrefs->rememberOpenedFiles = chkRememberOpened->IsChecked();
    }
    if (showInverseSearch && dropInverse) {
        TempStr tmp = dropInverse->GetTextTemp();
        str::ReplaceWithCopy(&gGlobalPrefs->inverseSearchCmdLine, tmp);
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
    SaveSettings();
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

static Checkbox* MakeCheckbox(HWND parent, Str text, bool isRtl, bool checked, int topPt) {
    Checkbox::CreateArgs args;
    args.parent = parent;
    args.text = text;
    args.isRtl = isRtl;
    if (checked) {
        args.initialState = Checkbox::State::Checked;
    }
    auto* c = new Checkbox();
    c->SetInsetsPt(topPt, 0, 0, 0);
    c->Create(args);
    return c;
}

static HBox* LabelAndDrop(VirtText* label, DropDown* drop, int topPt) {
    auto* row = new HBox();
    row->alignMain = MainAxisAlign::MainStart;
    row->alignCross = CrossAxisAlign::CrossCenter;
    row->AddChild(label);
    drop->SetInsetsPt(topPt, 0, 0, 8);
    row->AddChild(drop, 1);
    return row;
}

bool SettingsWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    showInverseSearch = gGlobalPrefs && gGlobalPrefs->enableTeXEnhancements && CanAccessDisk();

    {
        CreateCustomArgs args;
        args.title = _TRA("SumatraPDF Options");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = GetFont();
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    bool isRtl = IsUIRtl();

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    //[ ACCESSKEY_GROUP Settings Dialog
    {
        auto* c = NewVirtText({
            .s = _TRA("View"),
            .font = font,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(0, 0, 4, 0),
        });
        labelView = c;
        vbox->AddChild(c);
    }

    {
        auto* lab = NewVirtText({
            .s = _TRA("Default &Layout:"),
            .font = font,
            .isRtl = isRtl,
            .prefix = true,
        });
        labelLayout = lab;
        dropLayout = MakeDropDown(hwnd, GetFont(), isRtl, false);
        vbox->AddChild(LabelAndDrop(lab, dropLayout, 0));
        FillLayout();
    }

    {
        auto* lab = NewVirtText({
            .s = _TRA("Default &Zoom:"),
            .font = font,
            .isRtl = isRtl,
            .prefix = true,
        });
        labelZoom = lab;
        dropZoom = MakeDropDown(hwnd, GetFont(), isRtl, true);
        vbox->AddChild(LabelAndDrop(lab, dropZoom, 4));
        FillZoom();
    }

    chkShowToc = MakeCheckbox(hwnd, _TRA("Show the &bookmarks sidebar when available"), isRtl,
                              gGlobalPrefs && gGlobalPrefs->showToc, 8);
    vbox->AddChild(chkShowToc);

    chkRememberState = MakeCheckbox(hwnd, _TRA("&Remember these settings for each document"), isRtl,
                                    gGlobalPrefs && gGlobalPrefs->rememberStatePerDocument, 4);
    if (gGlobalPrefs && !gGlobalPrefs->rememberOpenedFiles) {
        chkRememberState->SetIsEnabled(false);
    }
    vbox->AddChild(chkRememberState);

    {
        auto* c = NewVirtText({
            .s = _TRA("Advanced"),
            .font = font,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(12, 0, 4, 0),
        });
        labelAdvanced = c;
        vbox->AddChild(c);
    }

    chkUseTabs = MakeCheckbox(hwnd, _TRA("Use &tabs"), isRtl, gGlobalPrefs && gGlobalPrefs->useTabs, 0);
    vbox->AddChild(chkUseTabs);

    chkCheckUpdates = MakeCheckbox(hwnd, _TRA("Automatically check for &updates"), isRtl,
                                   gGlobalPrefs && gGlobalPrefs->checkForUpdates, 4);
    if (!HasPermission(Perm::InternetAccess)) {
        chkCheckUpdates->SetIsEnabled(false);
    }
    vbox->AddChild(chkCheckUpdates);

    chkRememberOpened =
        MakeCheckbox(hwnd, _TRA("Remember &opened files"), isRtl, gGlobalPrefs && gGlobalPrefs->rememberOpenedFiles, 4);
    chkRememberOpened->onStateChanged = MkMethod0<SettingsWnd, &SettingsWnd::OnRememberOpenedChanged>(this);
    vbox->AddChild(chkRememberOpened);

    if (showInverseSearch) {
        auto* hdr = NewVirtText({
            .s = _TRA("Set inverse search command line"),
            .font = font,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(12, 0, 4, 0),
        });
        labelInverse = hdr;
        vbox->AddChild(hdr);

        auto* lab = NewVirtText({
            .s = _TRA("Enter the command line to invoke when you double-click on the PDF document:"),
            .font = font,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(0, 0, 4, 0),
        });
        labelCmdLine = lab;
        vbox->AddChild(lab);

        dropInverse = MakeDropDown(hwnd, GetFont(), isRtl, true);
        vbox->AddChild(dropInverse);
        FillInverse();
    }
    //] ACCESSKEY_GROUP Settings Dialog

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        hbox->gap = font->averageCharWidth;
        auto pad = Insets{4, 0, 4, 0};

        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), font, false);
        btnCancel->onClick = MkMethod1<SettingsWnd, VirtMouseEvent*, &SettingsWnd::OnCancel>(this);
        hbox->AddChild(new Padding(btnCancel, pad));
        btnOk = NewThemedButton(hwnd, _TRA("OK"), font, true);
        btnOk->onClick = MkMethod1<SettingsWnd, VirtMouseEvent*, &SettingsWnd::OnOk>(this);
        hbox->AddChild(new Padding(btnOk, pad));
        vbox->AddChild(hbox);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    int dx = DpiScale(480);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
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
