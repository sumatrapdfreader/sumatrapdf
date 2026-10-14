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
#include "shared/PdfToolsCommon.h"
#include "EbookSettingsDialogCommon.h"

#include "SumatraLog.h"

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

// orig's layout: the font and its numbers, the styling with the CSS, this
// file or every ebook, the footer; hairlines between
constexpr float kEbookDlgDx = 480;
constexpr float kEbookNumDx = 54;
constexpr float kEbookCssDy = 106;
constexpr float kEbookCssFontPx = 13;

struct EbookSettingsView {
    static void OnOk(EbookSettingsView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(EbookSettingsView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnReset(EbookSettingsView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnToggle(EbookSettingsView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t which);
    static void OnValueChanged(EbookSettingsView* self, gp::Ctx* cx, const gp::InputEvent* ev);
};

static gp::Entity<EbookSettingsView> gEbookView;

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
    spec.clientDx = kEbookDlgDx;
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
    gp::Listener onChange = gp::ListenTo(gEbookView, &EbookSettingsView::OnValueChanged);
    gEbook.editMargin->onChange = onChange;
    gEbook.editSpacing->onChange = onChange;
    gEbook.editFont->onChange = onChange;
    gp::Listener onOk = gp::ListenTo(gEbookView, &EbookSettingsView::OnOk);
    gp::Listener onCancel = gp::ListenTo(gEbookView, &EbookSettingsView::OnCancel);
    DlgSetDefault(cx, onOk, true);
    Color colEdge = ThemeEdgeColor();

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);

    // the font, then its size, the margin and the line spacing
    float labelDx = std::max(DlgFieldLabelDx(cx, Tr("Font:")), DlgFieldLabelDx(cx, Tr("Size:")));
    auto number = [&](gp::Str id, gp::InputState* state, bool disabled) {
        return gp::Div(cx->a)
            ->W(kEbookNumDx)
            ->Shrink0()
            ->Child(DlgUnderlineInput(cx, id, state, kToolDlgTextFontPx, colEdge, false, disabled));
    };
    gp::El* fields = DlgSection(cx);
    gp::El* fontRow = DlgFieldRow(cx);
    fontRow->Child(DlgFieldLabel(cx, Tr("Font:"), labelDx));
    fontRow->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(gEbook.ddFont.BuildCombo(
        cx, StrL("ebook-font"), gEbook.editFont, gp::kFill, gEbook.customCss, kDlgWinEditDy, kToolDlgTextFontPx)));
    fields->Child(fontRow);
    gp::El* numRow = DlgFieldRow(cx);
    numRow->Child(DlgFieldLabel(cx, Tr("Size:"), labelDx));
    numRow->Child(number(GStrL("ebook-size"), gEbook.editSize, false));
    numRow->Child(DlgFieldLabel(cx, Tr("Margin:"), 0));
    numRow->Child(number(GStrL("ebook-margin"), gEbook.editMargin, gEbook.customCss));
    numRow->Child(DlgFieldLabel(cx, Tr("Line spacing:"), 0));
    numRow->Child(number(GStrL("ebook-spacing"), gEbook.editSpacing, gEbook.customCss));
    fields->Child(numRow);
    col->Child(fields);
    col->Child(DlgHairline(cx));

    // the styling: the document's, the generated CSS or the user's; the CSS
    // is read-only until "Custom CSS" is on, as orig's
    gp::El* styling = DlgSection(cx);
    styling->Child(DlgFieldRow(cx)->Child(
        DlgCheck(cx, StrL("ebook-ignorecss"), Tr("&Ignore the document's own styling"), gEbook.ignoreCss,
                 gp::ListenTo(gEbookView, &EbookSettingsView::OnToggle, (intptr_t)0))));
    styling->Child(
        DlgFieldRow(cx)->Child(DlgCheck(cx, StrL("ebook-customcss"), Tr("&Custom CSS"), gEbook.customCss,
                                        gp::ListenTo(gEbookView, &EbookSettingsView::OnToggle, (intptr_t)1))));
    styling->Child(gp::Div(cx->a)
                       ->W(gp::kFill)
                       ->Shrink0()
                       ->PadT(kToolDlgFilesPadDy)
                       ->Child(gpc::Textarea::New(cx, GStrL("ebook-css"), gEbook.editCss)
                                   ->H(kEbookCssDy)
                                   ->Readonly(!gEbook.customCss)
                                   ->Appearance(false)
                                   ->Bordered(false)
                                   ->FocusRing(false)
                                   ->IntoEl()
                                   ->Mono()
                                   ->Font(kEbookCssFontPx)));
    col->Child(styling);
    col->Child(DlgHairline(cx));

    // this file or every ebook
    gp::El* target = DlgSection(cx);
    target->Child(
        DlgFieldRow(cx)
            ->Child(DlgCheck(cx, StrL("ebook-thisfile"), Tr("&This file"), gEbook.thisFile,
                             gp::ListenTo(gEbookView, &EbookSettingsView::OnToggle, (intptr_t)2), DlgCheckKind::Radio))
            ->Child(DlgCheck(cx, StrL("ebook-allebooks"), Tr("For all &ebooks"), !gEbook.thisFile,
                             gp::ListenTo(gEbookView, &EbookSettingsView::OnToggle, (intptr_t)3),
                             DlgCheckKind::Radio)));
    col->Child(target);

    gp::El* reset = DlgGhostButton(cx, GStrL("ebook-reset"), Tr("Reset to defaults"),
                                   gp::ListenTo(gEbookView, &EbookSettingsView::OnReset));
    col->Child(DlgFooter(cx, Tr("OK"), onOk, onCancel, reset));
    gp::El* content = DlgToolContent(cx, col, Tr("eBook Settings"), kEbookDlgDx, onCancel);

    if (gEbook.wantFocus) {
        gp::InputFocus(gEbook.editFont, cx->app, cx->win);
        gp::InputSelectAll(gEbook.editFont, cx->app, cx->win);
        gEbook.wantFocus = cx->win->input != gEbook.editFont;
    }
    return content;
}
