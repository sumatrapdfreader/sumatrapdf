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
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "FileHistory.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "EbookSettingsDialogCommon.h"

// Font, size and line spacing for reflowable documents (EPUB, MOBI, FB2, ...),
// either globally or for the current document only (issue #4600). The CSS the
// engine generates from those values is shown read-only; "Custom CSS" hands
// the user the pen instead, and then what they type *is* the user CSS.
//
//   Font:   [(default)                      v]
//   Size:   12__   Margin: 1em__   Line spacing: 1.5__
//   ---------------------------------------------- hairline
//   [ ] Ignore the document's own styling
//   [ ] Custom CSS
//   body { ... }                          the CSS, monospaced
//   ---------------------------------------------- hairline
//   (o) This file   ( ) For all ebooks
//   ---------------------------------------------- hairline
//   Reset to defaults                [Cancel] [OK]
constexpr int kEbookDlgDx = 480;
constexpr int kEbookNumChars = 5;
constexpr int kEbookCssLines = 6;
constexpr int kEbookCssFontPx = 13;
// the generated rules are one long line; without a cap the dialog would grow
// as wide as the text
constexpr int kEbookCssMaxDx = 440;

struct EbookSettingsWnd : WindowBase {
    ~EbookSettingsWnd() override;

    MainWindow* win = nullptr;
    Str filePath; // owned

    VirtText* labelFont = nullptr;
    DropDown* ddFont = nullptr;
    VirtText* labelSize = nullptr;
    Edit* editSize = nullptr;
    VirtText* labelMargin = nullptr;
    Edit* editMargin = nullptr;
    VirtText* labelSpacing = nullptr;
    Edit* editSpacing = nullptr;
    Checkbox* cbIgnoreCss = nullptr;
    Checkbox* cbCustomCss = nullptr;
    Edit* editCss = nullptr;
    Checkbox* radioThisFile = nullptr;
    Checkbox* radioAllEbooks = nullptr;
    VirtButton* btnReset = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnOk = nullptr;

    // guards the control -> preview -> control loop
    bool updating = false;
    // what the user typed while "Custom CSS" was on, kept across toggles
    Str customCssText;

    bool Create(MainWindow* win);
    void FillFontList();
    void SetValues(Str fontName, float fontSize, const Vec<float>* margin, float lineSpacing, bool ignoreCss,
                   Str customCss);
    void LoadFromTarget();
    void ReadControls(EbookVals& out);
    void UpdateCssPreview();
    void SetCssEditable(bool);
    void OnControlChanged();
    void OnCustomCssToggled();
    void OnTargetChanged();
    void Apply();
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnReset(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
};

static EbookSettingsWnd* gEbookSettingsWnd = nullptr;

EbookSettingsWnd::~EbookSettingsWnd() {
    str::Free(filePath);
    str::Free(customCssText);
}

static void ClearEbookSettingsWnd() {
    gEbookSettingsWnd = nullptr;
}

// a multi-line edit shows only CRLF as a line break, while our CSS (and what
// we store in the settings) uses LF
static TempStr ToEditTextTemp(Str s) {
    return str::LFToCRLFTemp(s);
}

static TempStr FromEditTextTemp(Str s) {
    return str::ReplaceTemp(s, StrL("\r\n"), StrL("\n"));
}

static float ParseFloatTemp(Edit* e) {
    TempStr s = e->GetTextTemp();
    if (len(s) == 0) {
        return 0;
    }
    return (float)atof(CStrTemp(s));
}

static int CALLBACK EnumFontFamilyCb(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM lp) {
    auto* names = (StrVec*)lp;
    const WCHAR* face = lf->lfFaceName;
    // "@Name" is the vertical-writing variant of a CJK font, not a font to pick
    if (!face[0] || face[0] == L'@') {
        return 1;
    }
    TempStr name = ToUtf8Temp(face);
    if (!IsSafeEbookFontName(name) || names->Contains(name)) {
        return 1;
    }
    names->Append(name);
    return 1;
}

void EbookSettingsWnd::FillFontList() {
    StrVec names;
    names.Append(FontDefaultLabel());
    {
        StrVec fonts;
        HDC hdc = GetDC(nullptr);
        LOGFONTW lf{};
        lf.lfCharSet = DEFAULT_CHARSET;
        EnumFontFamiliesExW(hdc, &lf, EnumFontFamilyCb, (LPARAM)&fonts, 0);
        ReleaseDC(nullptr, hdc);
        SortNoCase(&fonts);
        for (Str s : fonts) {
            names.Append(s);
        }
    }
    ddFont->SetItems(names);
}

// the settings this dialog is editing: the document's own values (falling back
// to the global ones for what it doesn't set), or the global ones
void EbookSettingsWnd::LoadFromTarget() {
    auto* g = &gSettings->eBookUI;
    bool thisFile = radioThisFile && radioThisFile->IsChecked();
    FileState* fs = thisFile ? FileHistoryFindByPath(filePath) : nullptr;
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

void EbookSettingsWnd::SetValues(Str fontName, float fontSize, const Vec<float>* margin, float lineSpacing,
                                 bool ignoreCss, Str customCss) {
    updating = true;
    ddFont->SetText(fontName ? fontName : FontDefaultLabel());
    editSize->SetText(fontSize > 0 ? fmt("%g", fontSize) : StrL(""));
    editMargin->SetText(MarginTextTemp(margin));
    editSpacing->SetText(lineSpacing > 0 ? fmt("%g", lineSpacing) : StrL(""));
    cbIgnoreCss->SetIsChecked(ignoreCss);

    str::ReplaceWithCopy(&customCssText, customCss);
    bool hasCustom = len(customCss) != 0;
    cbCustomCss->SetIsChecked(hasCustom);
    updating = false;

    SetCssEditable(hasCustom);
    if (hasCustom) {
        editCss->SetText(ToEditTextTemp(customCss));
    }
    UpdateCssPreview();
}

void EbookSettingsWnd::ReadControls(EbookVals& out) {
    TempStr font = ddFont->GetTextTemp();
    if (str::Eq(font, FontDefaultLabel())) {
        font = {};
    }
    out.fontName = EbookFontNameFromSetting(font);
    out.fontSize = ParseFloatTemp(editSize);
    ParseMargin(editMargin->GetTextTemp(), out.margin);
    out.lineSpacing = ParseFloatTemp(editSpacing);
    out.ignoreDocumentCSS = cbIgnoreCss->IsChecked();
    out.useCustomCSS = cbCustomCss->IsChecked();
    if (out.useCustomCSS) {
        out.customCSS = FromEditTextTemp(editCss->GetTextTemp());
        // the CSS in the box replaces what we would have generated, so the
        // values those rules came from are no longer ours to apply
        out.fontName = {};
        VecReset(out.margin);
        out.lineSpacing = 0;
    }
}

void EbookSettingsWnd::SetCssEditable(bool editable) {
    if (!editCss) {
        return;
    }
    SendMessageW(editCss->hwnd, EM_SETREADONLY, editable ? FALSE : TRUE, 0);
    // with hand-written CSS these two no longer feed anything
    ddFont->SetIsEnabled(!editable);
    editMargin->SetIsEnabled(!editable);
    editSpacing->SetIsEnabled(!editable);
}

void EbookSettingsWnd::UpdateCssPreview() {
    if (!editCss || cbCustomCss->IsChecked()) {
        return;
    }
    TempStr font = ddFont->GetTextTemp();
    if (str::Eq(font, FontDefaultLabel())) {
        font = {};
    }
    Vec<float> margin;
    ParseMargin(editMargin->GetTextTemp(), margin);
    // the same DPI the engine used: it takes it from DpiGet() when the document
    // is opened (EngineCreate.cpp)
    TempStr css = EbookGeneratedCssTemp(font, &margin, ParseFloatTemp(editSpacing), DpiGet());
    Str text = css ? css : Tr("The document's own styling is used.");
    editCss->SetText(ToEditTextTemp(text));
}

void EbookSettingsWnd::OnControlChanged() {
    if (updating) {
        return;
    }
    UpdateCssPreview();
}

void EbookSettingsWnd::OnCustomCssToggled() {
    if (updating) {
        return;
    }
    bool custom = cbCustomCss->IsChecked();
    if (custom) {
        // start from what was being applied, so nothing is lost by taking over
        if (len(customCssText) == 0) {
            str::ReplaceWithCopy(&customCssText, FromEditTextTemp(editCss->GetTextTemp()));
        }
        editCss->SetText(ToEditTextTemp(customCssText));
    } else {
        str::ReplaceWithCopy(&customCssText, FromEditTextTemp(editCss->GetTextTemp()));
    }
    SetCssEditable(custom);
    UpdateCssPreview();
}

void EbookSettingsWnd::OnTargetChanged() {
    if (updating) {
        return;
    }
    LoadFromTarget();
}

// write the controls to the global section, or to this document's own block
// (where a value equal to the global one is left unset, so it keeps following
// the global setting)
void EbookSettingsWnd::Apply() {
    EbookVals v;
    ReadControls(v);
    auto* g = &gSettings->eBookUI;
    bool thisFile = radioThisFile && radioThisFile->IsChecked();

    if (!thisFile) {
        str::ReplaceWithCopy(&g->fontName, v.fontName);
        g->fontSize = v.fontSize;
        *g->margin = v.margin;
        g->lineSpacing = v.lineSpacing;
        g->ignoreDocumentCSS = v.ignoreDocumentCSS;
        str::ReplaceWithCopy(&g->customCSS, v.customCSS);
        // the global values are what the user wants to see, so this document's
        // overrides have to go (same as Change Background Color -> all files)
        FileState* fs = FileHistoryFindByPath(filePath);
        if (fs && fs->eBookUI) {
            DeleteFileEBookUI(fs->eBookUI);
            fs->eBookUI = nullptr;
        }
        ScheduleSaveSettings();
        return;
    }

    FileState* fs = FileHistoryFindByPath(filePath);
    if (!fs) {
        fs = NewFileState(filePath);
        FileHistoryAppend(fs);
    }
    bool differs = !str::Eq(v.fontName, g->fontName) || v.fontSize != g->fontSize || !MarginEq(v.margin, g->margin) ||
                   v.lineSpacing != g->lineSpacing || v.ignoreDocumentCSS != g->ignoreDocumentCSS ||
                   !str::Eq(v.customCSS, g->customCSS);
    if (!differs) {
        // nothing left that isn't the global setting: drop the block entirely
        // so it stops being written out
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

void EbookSettingsWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

// back to no customization at the current scope: for one document that means
// inheriting everything from the global section (OK then drops its block), for
// all ebooks it means the values SumatraPDF ships with
void EbookSettingsWnd::OnReset(VirtMouseEvent*) {
    bool thisFile = radioThisFile && radioThisFile->IsChecked();
    if (thisFile) {
        auto* g = &gSettings->eBookUI;
        SetValues(g->fontName, g->fontSize, g->margin, g->lineSpacing, g->ignoreDocumentCSS, g->customCSS);
        return;
    }
    SetValues({}, 0, nullptr, 0, false, {});
}

void EbookSettingsWnd::OnOk(VirtMouseEvent*) {
    Apply();
    MainWindow* w = win;
    ScheduleDelete();
    // the settings only reach the text through a fresh layout
    if (IsMainWindowValidAndNotClosing(w) && w->IsDocLoaded()) {
        ReloadDocument(w, false);
    }
}

static void OnClose(WindowBase::CloseEvent*) {
    if (gEbookSettingsWnd) {
        gEbookSettingsWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent*) {
    if (gEbookSettingsWnd) {
        gEbookSettingsWnd->ScheduleDelete();
    }
}

bool EbookSettingsWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    SetFont(fontText);
    bool isRtl = IsUIRtl();
    {
        CreateCustomArgs args;
        args.title = Tr("eBook Settings");
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

    // the font, then its size, the margin and the line spacing
    auto label = [&](Str s) { return NewDlgLabel(s, fontText, colMuted, kToolDlgRowDy); };
    auto numberField = [&]() { return NewDlgEdit(hwnd, fontText, {}, kEbookNumChars); };
    auto* fields = new VBox();
    fields->alignCross = CrossAxisAlign::Stretch;
    {
        HBox* row = NewDlgFieldRow();
        labelFont = label(Tr("Font:"));
        row->AddChild(labelFont);
        DropDown::CreateArgs args;
        args.parent = hwnd;
        args.font = fontText;
        args.isRtl = isRtl;
        args.isEditable = true;
        ddFont = new DropDown();
        ddFont->Create(args);
        ddFont->onTextChanged = MkMethod0<EbookSettingsWnd, &EbookSettingsWnd::OnControlChanged>(this);
        ddFont->onSelectionChanged = MkMethod0<EbookSettingsWnd, &EbookSettingsWnd::OnControlChanged>(this);
        row->AddChild(ddFont, 1);
        fields->AddChild(row);
    }
    {
        HBox* row = NewDlgFieldRow();
        labelSize = label(Tr("Size:"));
        row->AddChild(labelSize);
        editSize = numberField();
        row->AddChild(NewDlgUnderlined(editSize));
        row->AddChild(new Spacer(DpiScale(kToolDlgGapDx), 0));
        labelMargin = label(Tr("Margin:"));
        row->AddChild(labelMargin);
        editMargin = numberField();
        editMargin->onTextChanged = MkMethod0<EbookSettingsWnd, &EbookSettingsWnd::OnControlChanged>(this);
        row->AddChild(NewDlgUnderlined(editMargin));
        row->AddChild(new Spacer(DpiScale(kToolDlgGapDx), 0));
        labelSpacing = label(Tr("Line spacing:"));
        row->AddChild(labelSpacing);
        editSpacing = numberField();
        editSpacing->onTextChanged = MkMethod0<EbookSettingsWnd, &EbookSettingsWnd::OnControlChanged>(this);
        row->AddChild(NewDlgUnderlined(editSpacing));
        fields->AddChild(row);
    }
    Vec<VirtText*> labels;
    VecAppend(labels, labelFont);
    VecAppend(labels, labelSize);
    AlignDlgLabels(labels);
    vbox->AddChild(NewDlgSection(fields));
    vbox->AddChild(new VirtLine());

    // the styling: the document's, the generated CSS or the user's
    auto* styling = new VBox();
    styling->alignCross = CrossAxisAlign::Stretch;
    {
        cbIgnoreCss = NewDlgCheckbox(hwnd, Tr("&Ignore the document's own styling"), fontText, false);
        HBox* row = NewDlgFieldRow();
        row->AddChild(cbIgnoreCss);
        styling->AddChild(row);

        cbCustomCss = NewDlgCheckbox(hwnd, Tr("&Custom CSS"), fontText, false);
        cbCustomCss->onStateChanged = MkMethod0<EbookSettingsWnd, &EbookSettingsWnd::OnCustomCssToggled>(this);
        row = NewDlgFieldRow();
        row->AddChild(cbCustomCss);
        styling->AddChild(row);

        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = GetUserGuiFont(StrL("Consolas"), DpiScale(kEbookCssFontPx));
        args.isMultiLine = true;
        args.withBorder = false;
        args.isRtl = isRtl;
        args.idealSizeLines = kEbookCssLines;
        editCss = new Edit();
        editCss->SetColors(colTxt, colBg);
        editCss->Create(args);
        editCss->maxDx = DpiScale(kEbookCssMaxDx);
        SendMessageW(editCss->hwnd, EM_SETREADONLY, TRUE, 0);
        SendMessageW(editCss->hwnd, EM_SETLIMITTEXT, 0, 0);
        styling->AddChild(new Padding(editCss, DpiScaledInsets(kToolDlgFilesPadDy, 0, 0, 0)), 1);
    }
    vbox->AddChild(NewDlgSection(styling), 1);
    vbox->AddChild(new VirtLine());

    // this file or every ebook
    {
        radioThisFile = NewDlgRadio(hwnd, Tr("&This file"), fontText, true, true);
        radioThisFile->onStateChanged = MkMethod0<EbookSettingsWnd, &EbookSettingsWnd::OnTargetChanged>(this);
        radioAllEbooks = NewDlgRadio(hwnd, Tr("For all &ebooks"), fontText, false, false);
        radioAllEbooks->onStateChanged = MkMethod0<EbookSettingsWnd, &EbookSettingsWnd::OnTargetChanged>(this);
        HBox* row = NewDlgFieldRow();
        row->AddChild(radioThisFile);
        row->AddChild(radioAllEbooks);
        vbox->AddChild(NewDlgSection(row));
    }

    btnReset = NewDlgButton(hwnd, Tr("Reset to defaults"), fontText, DlgButtonStyle::Ghost, false);
    btnReset->onClick = MkMethod1<EbookSettingsWnd, VirtMouseEvent*, &EbookSettingsWnd::OnReset>(this);
    btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<EbookSettingsWnd, VirtMouseEvent*, &EbookSettingsWnd::OnCancel>(this);
    btnOk = NewDlgButton(hwnd, Tr("OK"), fontText, DlgButtonStyle::Filled, true);
    btnOk->onClick = MkMethod1<EbookSettingsWnd, VirtMouseEvent*, &EbookSettingsWnd::OnOk>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnOk, btnReset));
    layout = vbox;

    FillFontList();
    LoadFromTarget();

    LayoutAndSizeToContent(layout, DpiScale(kEbookDlgDx), 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    HwndSetFocus(ddFont->hwnd);
    return true;
}

void ShowEbookSettingsDialog(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->CurrentTab() || !win->CurrentTab()->ctrl) {
        return;
    }
    if (gEbookSettingsWnd) {
        HwndSetFocus(gEbookSettingsWnd->hwnd);
        return;
    }
    auto* wnd = new EbookSettingsWnd();
    wnd->filePath = str::Dup(win->CurrentTab()->filePath);
    // Usually Esc while editing CSS should not throw the edits away. EscToExit
    // is the explicit opt-in to close this window anyway.
    wnd->closeOnEsc = gSettings->escToExit;
    wnd->onBeforeDelete = MkFunc0Void(ClearEbookSettingsWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    if (!wnd->Create(win)) {
        delete wnd;
        return;
    }
    gEbookSettingsWnd = wnd;
}
