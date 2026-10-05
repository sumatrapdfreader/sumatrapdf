/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's EbookSettingsDialog.cpp is a WS_POPUPWINDOW with an editable font
// DropDown, three Edits, two Checkboxes, a multi-line CSS Edit and two radio
// buttons. Here it is a gpui Dialog with the same rows in the same order; the
// font drop-down is a Select whose pick writes into the font Input (gpui has no
// editable drop-down) and the CSS box is a Textarea. The model half - what the
// values mean, how they are read back and where they are written - is orig's.

#include "gui/GpuiBridge.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "FileHistory.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SystemFonts.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "SumatraDialogs.h"

#include "SumatraLog.h"

// what the controls hold, ready to be written to a settings struct
struct EbookVals {
    Str fontName; // not owned, points into the controls' temp strings
    float fontSize = 0;
    Vec<float> margin; // 1, 2 or 4 values; empty means unset
    float lineSpacing = 0;
    bool ignoreDocumentCSS = false;
    Str customCSS; // not owned; empty unless useCustomCSS
    bool useCustomCSS = false;
};

struct EbookSettingsDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    Str filePath;
    bool thisFile = true;
    bool ignoreCss = false;
    bool customCss = false;
    // what the user typed while "Custom CSS" was on, kept across toggles
    Str customCssText;
    DialogSelect ddFont;
    gpui::InputState* editFont = nullptr;
    gpui::InputState* editSize = nullptr;
    gpui::InputState* editMargin = nullptr;
    gpui::InputState* editSpacing = nullptr;
    gpui::InputState* editCss = nullptr;
    // orig's window opens with the font drop-down focused
    bool wantFocus = false;
    // the main window it was opened from is gone; the dialog stays, as
    // orig's, but has no document to lay out again
    bool ownerClosed = false;
};

static EbookSettingsDlg gEbook;

// orig's window at 96 dpi: a 460 wide client area, 4 / 8 around. The font
// drop-down after its label; 8 under it the three 54 wide edits, each 8 after
// its label and the labels 16 after the edit before; the two checkboxes 10
// and 8 under what is above; the 106 high CSS box 4 under them; the two radio
// buttons 10 under it, 12 apart; Reset at the left and Cancel / OK at the
// right with 4 above and below
constexpr float kEbookWinDx = 460;
constexpr float kEbookWinPadX = 8;
constexpr float kEbookWinPadY = 4;
constexpr float kEbookWinLabelGap = 8;
constexpr float kEbookWinFieldGap = 16;
constexpr float kEbookWinEditDx = 54;
constexpr float kEbookWinRowGap = 8;
constexpr float kEbookWinCheckGap = 10;
constexpr float kEbookWinCssDy = 106;
// 12 between the radio buttons, and the system's ideal width for one is 19
// more than its box and label
constexpr float kEbookWinRadioGap = 12 + 19;

struct EbookSettingsView {
    static void OnOk(EbookSettingsView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(EbookSettingsView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnReset(EbookSettingsView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnToggle(EbookSettingsView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t which);
    static void OnValueChanged(EbookSettingsView* self, gp::Ctx* cx, const gp::InputEvent* ev);
};

static gp::Entity<EbookSettingsView> gEbookView;

// the dropdown's first entry: no font of our own, whatever the engine picks
static Str FontDefaultLabel() {
    return Tr("(default)");
}

// orig's window (modeless, as orig's), where the platform can have one (DlgWindowOpen); null: a
// dialog in the frame
static ToolWindow* gEbookSettingsTw = nullptr;

// the dialog in the frame; a window of its own is not the frame's business
bool IsEbookSettingsDialogVisible() {
    return gEbook.visible && !gEbookSettingsTw;
}

void CloseEbookSettingsDialog() {
    if (!gEbook.visible) {
        return;
    }
    gEbook.visible = false;
    DlgWindowClose(&gEbookSettingsTw);
    MainWindow* win = gEbook.win;
    gp::InputState* edits[] = {gEbook.editFont, gEbook.editSize, gEbook.editMargin, gEbook.editSpacing, gEbook.editCss};
    for (gp::InputState* e : edits) {
        if (e && win && win->gpuiWin) {
            gp::InputBlur(e, win->gpuiWin->app, win->gpuiWin);
        }
        delete e;
    }
    gEbook.editFont = nullptr;
    gEbook.editSize = nullptr;
    gEbook.editMargin = nullptr;
    gEbook.editSpacing = nullptr;
    gEbook.editCss = nullptr;
    gEbook.ddFont.Free();
    str::Free(gEbook.filePath);
    gEbook.filePath = {};
    str::Free(gEbook.customCssText);
    gEbook.customCssText = {};
    AppShellInvalidate(win);
}

// the margin is typed as CSS writes it: one number for all four sides, two for
// top/bottom and left/right, or four in top-right-bottom-left order. anything
// else (or out of range) is left empty, i.e. unset. orig's ParseMargin
static void ParseMargin(Str s, Vec<float>& out) {
    VecReset(out);
    if (len(s) == 0) {
        return;
    }
    StrVec parts;
    Split(&parts, s, StrL(" "), true);
    bool ok = true;
    for (Str part : parts) {
        if (len(part) == 0) {
            continue;
        }
        const char* cs = CStrTemp(part);
        char* end = nullptr;
        float v = strtof(cs, &end);
        if (end == cs || (end && *end != 0)) {
            ok = false;
            break;
        }
        VecAppend(out, v);
    }
    int n = len(out);
    ok = ok && (n == 1 || n == 2 || n == 4);
    for (int i = 0; ok && i < n; i++) {
        ok = out[i] >= 0 && out[i] <= 200;
    }
    if (!ok) {
        VecReset(out);
    }
}

static TempStr MarginTextTemp(const Vec<float>* margin) {
    int n = margin ? len(*margin) : 0;
    TempStr res;
    for (int i = 0; i < n; i++) {
        TempStr one = fmt("%g", (*margin)[i]);
        res = res ? str::JoinTemp(res, StrL(" "), one) : one;
    }
    return res;
}

static bool MarginEq(const Vec<float>& a, const Vec<float>* b) {
    int n = len(a);
    if (n != (b ? len(*b) : 0)) {
        return false;
    }
    for (int i = 0; i < n; i++) {
        if (a[i] != (*b)[i]) {
            return false;
        }
    }
    return true;
}

static TempStr EditTextTemp(gp::InputState* e) {
    if (!e) {
        return {};
    }
    return str::DupTemp(FromGpui(gp::InputValue(e)));
}

static float ParseFloatTemp(gp::InputState* e) {
    TempStr s = EditTextTemp(e);
    if (len(s) == 0) {
        return 0;
    }
    return (float)atof(CStrTemp(s));
}

static Str FontNameFromControls() {
    TempStr font = EditTextTemp(gEbook.editFont);
    if (str::Eq(font, FontDefaultLabel())) {
        return {};
    }
    return font;
}

// orig's UpdateCssPreview: the rules the engine would generate
static void UpdateCssPreview() {
    if (!gEbook.editCss || gEbook.customCss) {
        return;
    }
    Vec<float> margin;
    ParseMargin(EditTextTemp(gEbook.editMargin), margin);
    // the same DPI the engine used: it takes it from DpiGet() when the document
    // is opened (EngineCreate.cpp)
    TempStr css = EbookGeneratedCssTemp(FontNameFromControls(), &margin, ParseFloatTemp(gEbook.editSpacing), DpiGet());
    Str text = css ? css : Tr("The document's own styling is used.");
    gp::InputSetValue(gEbook.editCss, ToGpui(text));
}

// orig's SetValues
static void SetValues(Str fontName, float fontSize, const Vec<float>* margin, float lineSpacing, bool ignoreCss,
                      Str customCss) {
    gp::InputSetValue(gEbook.editFont, ToGpui(len(fontName) > 0 ? fontName : FontDefaultLabel()));
    gp::InputSetValue(gEbook.editSize, ToGpui(fontSize > 0 ? Str(fmt("%g", fontSize)) : Str{}));
    gp::InputSetValue(gEbook.editMargin, ToGpui(MarginTextTemp(margin)));
    gp::InputSetValue(gEbook.editSpacing, ToGpui(lineSpacing > 0 ? Str(fmt("%g", lineSpacing)) : Str{}));
    gEbook.ignoreCss = ignoreCss;

    str::ReplaceWithCopy(&gEbook.customCssText, customCss);
    bool hasCustom = len(customCss) != 0;
    gEbook.customCss = hasCustom;
    if (hasCustom) {
        gp::InputSetValue(gEbook.editCss, ToGpui(customCss));
    }
    UpdateCssPreview();
}

// orig's LoadFromTarget: the document's own values (falling back to the global
// ones for what it does not set), or the global ones
static void LoadFromTarget() {
    auto* g = &gSettings->eBookUI;
    FileState* fs = gEbook.thisFile ? FileHistoryFindByPath(gEbook.filePath) : nullptr;
    FileEBookUI* f = fs ? fs->eBookUI : nullptr;

    Str fontName = g->fontName;
    float fontSize = g->fontSize;
    const Vec<float>* margin = g->margin;
    float lineSpacing = g->lineSpacing;
    bool ignoreCss = g->ignoreDocumentCSS;
    Str customCss = g->customCSS;
    if (f) {
        if (f->fontName) {
            fontName = f->fontName;
        }
        if (f->fontSize > 0) {
            fontSize = f->fontSize;
        }
        if (f->margin && len(*f->margin) > 0) {
            margin = f->margin;
        }
        if (f->lineSpacing > 0) {
            lineSpacing = f->lineSpacing;
        }
        if (f->ignoreDocumentCSS) {
            ignoreCss = str::EqI(f->ignoreDocumentCSS, StrL("true"));
        }
        if (f->customCSS) {
            customCss = f->customCSS;
        }
    }
    SetValues(fontName, fontSize, margin, lineSpacing, ignoreCss, customCss);
}

// orig's ReadControls
static void ReadControls(EbookVals& out) {
    out.fontName = EbookFontNameFromSetting(FontNameFromControls());
    out.fontSize = ParseFloatTemp(gEbook.editSize);
    ParseMargin(EditTextTemp(gEbook.editMargin), out.margin);
    out.lineSpacing = ParseFloatTemp(gEbook.editSpacing);
    out.ignoreDocumentCSS = gEbook.ignoreCss;
    out.useCustomCSS = gEbook.customCss;
    if (out.useCustomCSS) {
        out.customCSS = EditTextTemp(gEbook.editCss);
        // the CSS in the box replaces what we would have generated, so the
        // values those rules came from are no longer ours to apply
        out.fontName = {};
        VecReset(out.margin);
        out.lineSpacing = 0;
    }
}

// orig's Apply: write the controls to the global section, or to this document's
// own block (where a value equal to the global one is left unset)
static void Apply() {
    EbookVals v;
    ReadControls(v);
    auto* g = &gSettings->eBookUI;

    if (!gEbook.thisFile) {
        str::ReplaceWithCopy(&g->fontName, v.fontName);
        g->fontSize = v.fontSize;
        *g->margin = v.margin;
        g->lineSpacing = v.lineSpacing;
        g->ignoreDocumentCSS = v.ignoreDocumentCSS;
        str::ReplaceWithCopy(&g->customCSS, v.customCSS);
        FileState* fs = FileHistoryFindByPath(gEbook.filePath);
        if (fs && fs->eBookUI) {
            DeleteFileEBookUI(fs->eBookUI);
            fs->eBookUI = nullptr;
        }
        ScheduleSaveSettings();
        return;
    }

    FileState* fs = FileHistoryFindByPath(gEbook.filePath);
    if (!fs) {
        fs = NewFileState(gEbook.filePath);
        FileHistoryAppend(fs);
    }
    bool differs = !str::Eq(v.fontName, g->fontName) || v.fontSize != g->fontSize || !MarginEq(v.margin, g->margin) ||
                   v.lineSpacing != g->lineSpacing || v.ignoreDocumentCSS != g->ignoreDocumentCSS ||
                   !str::Eq(v.customCSS, g->customCSS);
    if (!differs) {
        // nothing left that isn't the global setting: drop the block entirely
        DeleteFileEBookUI(fs->eBookUI);
        fs->eBookUI = nullptr;
        ScheduleSaveSettings();
        return;
    }
    if (!fs->eBookUI) {
        fs->eBookUI = NewFileEBookUI();
    }
    FileEBookUI* f = fs->eBookUI;
    str::ReplaceWithCopy(&f->fontName, str::Eq(v.fontName, g->fontName) ? Str{} : v.fontName);
    f->fontSize = (v.fontSize == g->fontSize) ? 0 : v.fontSize;
    if (MarginEq(v.margin, g->margin)) {
        VecReset(*f->margin);
    } else {
        *f->margin = v.margin;
    }
    f->lineSpacing = (v.lineSpacing == g->lineSpacing) ? 0 : v.lineSpacing;
    Str ignore{};
    if (v.ignoreDocumentCSS != g->ignoreDocumentCSS) {
        ignore = v.ignoreDocumentCSS ? StrL("true") : StrL("false");
    }
    str::ReplaceWithCopy(&f->ignoreDocumentCSS, ignore);
    str::ReplaceWithCopy(&f->customCSS, str::Eq(v.customCSS, g->customCSS) ? Str{} : v.customCSS);
    ScheduleSaveSettings();
}

// orig's FillFontList
static void FillFontList() {
    StrVec names;
    names.Append(FontDefaultLabel());
    StrVec fonts;
    GetInstalledFontNames(fonts);
    for (Str s : fonts) {
        if (IsSafeEbookFontName(s)) {
            names.Append(s);
        }
    }
    gEbook.ddFont.SetItems(names, 0);
}

static Str EbookSettingsDlgTitle() {
    return Tr("eBook Settings");
}

// orig's window has no owner and stays when its main window closes. Its
// settings are still written; there is no window to reload the document in
static void EbookSettingsDlgOnOwnerClosed(MainWindow* newOwner) {
    gEbook.win = newOwner;
    gEbook.ownerClosed = true;
}

void ShowEbookSettingsDialog(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->CurrentTab() || !win->CurrentTab()->ctrl) {
        return;
    }
    CloseEbookSettingsDialog();
    gEbook.win = win;
    gEbook.filePath = str::Dup(win->CurrentTab()->filePath);
    gEbook.thisFile = true;
    gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;
    gp::InputState** edits[] = {&gEbook.editFont, &gEbook.editSize, &gEbook.editMargin, &gEbook.editSpacing,
                                &gEbook.editCss};
    for (gp::InputState** e : edits) {
        *e = new gp::InputState();
        (*e)->focus = gp::FocusHandleNew(app);
    }
    gEbook.ddFont.Init(app);
    FillFontList();
    LoadFromTarget();
    gEbook.visible = true;
    DlgWindowSpec spec;
    spec.name = "ebooksettings";
    spec.title = EbookSettingsDlgTitle;
    spec.modal = false;
    spec.build = EbookSettingsDialogBuild;
    spec.close = CloseEbookSettingsDialog;
    // orig: closeOnEsc = gSettings->escToExit
    spec.closeOnEsc = gSettings->escToExit;
    spec.onOwnerClosed = EbookSettingsDlgOnOwnerClosed;
    gEbook.ownerClosed = false;
    spec.clientDx = kEbookWinDx;
    gEbook.wantFocus = true;
    gEbookSettingsTw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

void EbookSettingsView::OnCancel(EbookSettingsView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseEbookSettingsDialog();
    gp::Notify(cx);
}

// orig's OnOk: the settings only reach the text through a fresh layout
void EbookSettingsView::OnOk(EbookSettingsView*, gp::Ctx* cx, const gp::ClickEvent*) {
    Apply();
    Str target = gEbook.thisFile ? StrL("this file") : StrL("all ebooks");
    logf("EbookSettingsDialog: applied to %s\n", target);
    MainWindow* w = gEbook.win;
    bool ownerClosed = gEbook.ownerClosed;
    CloseEbookSettingsDialog();
    if (!ownerClosed && IsMainWindowValidAndNotClosing(w) && w->IsDocLoaded()) {
        ReloadDocument(w, false);
    }
    gp::Notify(cx);
}

// orig's OnReset: back to no customization at the current scope
void EbookSettingsView::OnReset(EbookSettingsView*, gp::Ctx* cx, const gp::ClickEvent*) {
    if (gEbook.thisFile) {
        auto* g = &gSettings->eBookUI;
        SetValues(g->fontName, g->fontSize, g->margin, g->lineSpacing, g->ignoreDocumentCSS, g->customCSS);
    } else {
        SetValues({}, 0, nullptr, 0, false, {});
    }
    gp::Notify(cx);
    AppShellInvalidate(gEbook.win);
}

void EbookSettingsView::OnToggle(EbookSettingsView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t which) {
    if (which == 0) {
        gEbook.ignoreCss = !gEbook.ignoreCss;
    } else if (which == 1) {
        // orig's OnCustomCssToggled
        gEbook.customCss = !gEbook.customCss;
        if (gEbook.customCss) {
            // start from what was being applied, so nothing is lost
            if (len(gEbook.customCssText) == 0) {
                str::ReplaceWithCopy(&gEbook.customCssText, EditTextTemp(gEbook.editCss));
            }
            gp::InputSetValue(gEbook.editCss, ToGpui(gEbook.customCssText));
        } else {
            str::ReplaceWithCopy(&gEbook.customCssText, EditTextTemp(gEbook.editCss));
        }
        UpdateCssPreview();
    } else {
        // orig's OnTargetChanged (the two radio buttons)
        bool thisFile = which == 2;
        if (thisFile != gEbook.thisFile) {
            gEbook.thisFile = thisFile;
            LoadFromTarget();
        }
    }
    gp::Notify(cx);
    AppShellInvalidate(gEbook.win);
}

void EbookSettingsView::OnValueChanged(EbookSettingsView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::Change) {
        return;
    }
    UpdateCssPreview();
    gp::Notify(cx);
}

static gp::El* EbookSettingsWinBuild(gp::Ctx* cx) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    float font = DlgWinFont(cx);
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->PadX(kEbookWinPadX)->PadY(kEbookWinPadY);
    auto label = [&](Str s, float padL) {
        return gp::Div(cx->a)
            ->W(padL + DlgWinTextDx(cx, s) + kEbookWinLabelGap)
            ->PadL(padL)
            ->Shrink0()
            ->Child(gp::TextEl(cx->a, ToGpui(s))->Font(font)->Fg(th.foreground));
    };
    auto edit = [&](gp::Str id, gp::InputState* state, bool disabled) {
        return gpc::Input::New(cx, id, state)
            ->WithSize(gp::UiSize::Small)
            ->Disabled(disabled)
            ->W(kEbookWinEditDx)
            ->IntoEl()
            ->H(kDlgWinEditDy)
            ->Shrink0();
    };

    gp::El* fontRow = gp::Div(cx->a)->FlexRow()->ItemsCenter()->W(gp::kFill)->H(kDlgWinEditDy)->Shrink0();
    fontRow->Child(label(Tr("Font:"), 0));
    fontRow->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(
        gEbook.ddFont.BuildCombo(cx, StrL("ebook-font"), gEbook.editFont, gp::kFill, gEbook.customCss, kDlgWinEditDy)));
    col->Child(fontRow);

    gp::El* numRow = gp::Div(cx->a)
                         ->FlexRow()
                         ->ItemsCenter()
                         ->W(gp::kFill)
                         ->H(kDlgWinEditDy + kEbookWinRowGap)
                         ->PadT(kEbookWinRowGap)
                         ->Shrink0();
    numRow->Child(label(Tr("Size:"), 0));
    numRow->Child(edit(GStrL("ebook-size"), gEbook.editSize, false));
    numRow->Child(label(Tr("Margin:"), kEbookWinFieldGap));
    numRow->Child(edit(GStrL("ebook-margin"), gEbook.editMargin, gEbook.customCss));
    numRow->Child(label(Tr("Line spacing:"), kEbookWinFieldGap));
    numRow->Child(edit(GStrL("ebook-spacing"), gEbook.editSpacing, gEbook.customCss));
    col->Child(numRow);

    col->Child(DlgWinCheck(cx,
                           DlgAccelEl(cx, gpc::Checkbox::New(cx, GStrL("ebook-ignorecss"))->Checked(gEbook.ignoreCss),
                                      Tr("&Ignore the document's own styling"),
                                      gp::ListenTo(gEbookView, &EbookSettingsView::OnToggle, (intptr_t)0)),
                           kEbookWinCheckGap));
    col->Child(
        DlgWinCheck(cx,
                    DlgAccelEl(cx, gpc::Checkbox::New(cx, GStrL("ebook-customcss"))->Checked(gEbook.customCss),
                               Tr("&Custom CSS"), gp::ListenTo(gEbookView, &EbookSettingsView::OnToggle, (intptr_t)1)),
                    kEbookWinRowGap));
    // read-only until "Custom CSS" is on, as orig's
    col->Child(gp::Div(cx->a)
                   ->W(gp::kFill)
                   ->PadT(kEbookWinPadY)
                   ->Shrink0()
                   ->Child(gpc::Textarea::New(cx, GStrL("ebook-css"), gEbook.editCss)
                               ->H(kEbookWinCssDy)
                               ->Readonly(!gEbook.customCss)
                               ->IntoEl()));

    gp::El* radios = gp::Div(cx->a)
                         ->FlexRow()
                         ->ItemsCenter()
                         ->W(gp::kFill)
                         ->H(kDlgWinCheckDy + kEbookWinCheckGap)
                         ->PadT(kEbookWinCheckGap)
                         ->Gap(kEbookWinRadioGap)
                         ->Shrink0();
    constexpr float kRadioLabelGap = 2;
    radios->Child(DlgAccelEl(cx,
                             gpc::Checkbox::New(cx, GStrL("ebook-thisfile"))
                                 ->Role(gp::AccessibilityRole::RadioButton)
                                 ->Checked(gEbook.thisFile),
                             Tr("&This file"), gp::ListenTo(gEbookView, &EbookSettingsView::OnToggle, (intptr_t)2))
                      ->Gap(kRadioLabelGap));
    radios->Child(DlgAccelEl(cx,
                             gpc::Checkbox::New(cx, GStrL("ebook-allebooks"))
                                 ->Role(gp::AccessibilityRole::RadioButton)
                                 ->Checked(!gEbook.thisFile),
                             Tr("For all &ebooks"), gp::ListenTo(gEbookView, &EbookSettingsView::OnToggle, (intptr_t)3))
                      ->Gap(kRadioLabelGap));
    col->Child(radios);

    gp::Listener onOk = gp::ListenTo(gEbookView, &EbookSettingsView::OnOk);
    DlgSetDefault(cx, onOk, true);
    gp::El* buttons = DlgWinButtonRow(cx, kEbookWinPadY);
    buttons->Child(DlgWinButton(cx, GStrL("ebook-reset"), Tr("Reset to defaults"),
                                gp::ListenTo(gEbookView, &EbookSettingsView::OnReset), false));
    buttons->Child(gp::Div(cx->a)->Flex1());
    buttons->Child(DlgWinButton(cx, GStrL("dlg-cancel"), Tr("Cancel"),
                                gp::ListenTo(gEbookView, &EbookSettingsView::OnCancel), false));
    buttons->Child(DlgWinButton(cx, GStrL("dlg-ok"), Tr("OK"), onOk, true));
    col->Child(buttons);
    return DlgWinContent(cx, col);
}

gp::El* EbookSettingsDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gEbook.visible || gEbook.win != win) {
        return nullptr;
    }
    if (gEbookSettingsTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gEbookView.IsValid()) {
        gEbookView = gp::EntityNewState<EbookSettingsView>(cx->app);
    }
    if (gEbook.ddFont.TakeComboPicked()) {
        UpdateCssPreview();
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::Listener onChange = gp::ListenTo(gEbookView, &EbookSettingsView::OnValueChanged);
    gEbook.editMargin->onChange = onChange;
    gEbook.editSpacing->onChange = onChange;
    gEbook.editFont->onChange = onChange;

    if (DlgWindowIsHost(cx)) {
        gp::El* content = EbookSettingsWinBuild(cx);
        if (gEbook.wantFocus) {
            gp::InputFocus(gEbook.editFont, cx->app, cx->win);
            gp::InputSelectAll(gEbook.editFont, cx->app, cx->win);
            gEbook.wantFocus = cx->win->input != gEbook.editFont;
        }
        return content;
    }

    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(8);

    gp::El* fontRow = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(6);
    fontRow->Child(gp::TextEl(cx->a, ToGpui(Tr("Font:")))->Font(13)->Fg(th.foreground)->Shrink0());
    fontRow->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(
        gEbook.ddFont.BuildCombo(cx, StrL("ebook-font"), gEbook.editFont, gp::kFill, gEbook.customCss)));
    body->Child(fontRow);

    gp::El* numRow = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(6);
    numRow->Child(gp::TextEl(cx->a, ToGpui(Tr("Size:")))->Font(13)->Fg(th.foreground)->Shrink0());
    numRow->Child(
        gpc::Input::New(cx, GStrL("ebook-size"), gEbook.editSize)->WithSize(gp::UiSize::Small)->W(70)->IntoEl());
    numRow->Child(gp::TextEl(cx->a, ToGpui(Tr("Margin:")))->Font(13)->Fg(th.foreground)->Shrink0());
    numRow->Child(gpc::Input::New(cx, GStrL("ebook-margin"), gEbook.editMargin)
                      ->WithSize(gp::UiSize::Small)
                      ->Disabled(gEbook.customCss)
                      ->W(90)
                      ->IntoEl());
    numRow->Child(gp::TextEl(cx->a, ToGpui(Tr("Line spacing:")))->Font(13)->Fg(th.foreground)->Shrink0());
    numRow->Child(gpc::Input::New(cx, GStrL("ebook-spacing"), gEbook.editSpacing)
                      ->WithSize(gp::UiSize::Small)
                      ->Disabled(gEbook.customCss)
                      ->W(70)
                      ->IntoEl());
    body->Child(numRow);

    body->Child(DlgAccelEl(cx, gpc::Checkbox::New(cx, GStrL("ebook-ignorecss"))->Checked(gEbook.ignoreCss),
                           Tr("&Ignore the document's own styling"),
                           gp::ListenTo(gEbookView, &EbookSettingsView::OnToggle, (intptr_t)0)));
    body->Child(DlgAccelEl(cx, gpc::Checkbox::New(cx, GStrL("ebook-customcss"))->Checked(gEbook.customCss),
                           Tr("&Custom CSS"), gp::ListenTo(gEbookView, &EbookSettingsView::OnToggle, (intptr_t)1)));
    body->Child(gpc::Textarea::New(cx, GStrL("ebook-css"), gEbook.editCss)->Rows(6)->IntoEl());

    gp::El* radios = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(16);
    radios->Child(DlgAccelEl(cx,
                             gpc::Checkbox::New(cx, GStrL("ebook-thisfile"))
                                 ->Role(gp::AccessibilityRole::RadioButton)
                                 ->Checked(gEbook.thisFile),
                             Tr("&This file"), gp::ListenTo(gEbookView, &EbookSettingsView::OnToggle, (intptr_t)2)));
    radios->Child(DlgAccelEl(cx,
                             gpc::Checkbox::New(cx, GStrL("ebook-allebooks"))
                                 ->Role(gp::AccessibilityRole::RadioButton)
                                 ->Checked(!gEbook.thisFile),
                             Tr("For all &ebooks"),
                             gp::ListenTo(gEbookView, &EbookSettingsView::OnToggle, (intptr_t)3)));
    body->Child(radios);

    gp::El* reset = gpc::Button::New(cx, GStrL("ebook-reset"))
                        ->Label(ToGpui(Tr("Reset to defaults")))
                        ->WithSize(gp::UiSize::Small)
                        ->OnClick(gp::ListenTo(gEbookView, &EbookSettingsView::OnReset))
                        ->IntoEl();
    gp::El* footer = DialogFooter(cx, reset, gEbookView, Tr("OK"), Tr("Cancel"), &EbookSettingsView::OnOk,
                                  &EbookSettingsView::OnCancel);

    gp::El* dlg = DlgIntoEl(cx, gpc::Dialog::New(cx)
                                    ->Open(true)
                                    ->Title(ToGpui(Tr("eBook Settings")))
                                    ->Body(body)
                                    ->Footer(footer)
                                    ->W(560)
                                    ->OnClose(gp::ListenTo(gEbookView, &EbookSettingsView::OnCancel)));
    if (gEbook.wantFocus) {
        gEbook.wantFocus = false;
        gp::InputFocus(gEbook.editFont, cx->app, cx->win);
        gp::InputSelectAll(gEbook.editFont, cx->app, cx->win);
    }
    return dlg;
}
