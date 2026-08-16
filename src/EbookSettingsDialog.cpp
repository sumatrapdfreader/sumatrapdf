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
#include "GlobalPrefs.h"
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
#include "DarkMode_win.h"
#include "EbookSettingsDialog.h"

// Font, size and line spacing for reflowable documents (EPUB, MOBI, FB2, ...),
// either globally or for the current document only (issue #4600). The CSS the
// engine generates from those values is shown read-only; "Custom CSS" hands
// the user the pen instead, and then what they type *is* the user CSS.

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

// the dropdown's first entry: no font of our own, whatever the engine picks
static Str FontDefaultLabel() {
    return _TRA("(default)");
}

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
    return str::ReplaceTemp(s, StrL("\n"), StrL("\r\n"));
}

static TempStr FromEditTextTemp(Str s) {
    return str::ReplaceTemp(s, StrL("\r\n"), StrL("\n"));
}

// the margin is typed as CSS writes it: one number for all four sides, two
// for top/bottom and left/right, or four in top-right-bottom-left order.
// anything else (or out of range) is left empty, i.e. unset
static void ParseMargin(Str s, Vec<float>& out) {
    out.Reset();
    if (!s) {
        return;
    }
    StrVec parts;
    Split(&parts, s, StrL(" "), true);
    bool ok = true;
    for (Str part : parts) {
        if (!part) {
            continue; // Split can hand back an empty piece
        }
        // strtof, not atof: a token that isn't a plain number (or is null,
        // which atof faults on) has to invalidate the whole value, not read
        // as 0 - that would silently mean "no margin at all"
        const char* cs = CStrTemp(part);
        char* end = nullptr;
        float v = strtof(cs, &end);
        if (end == cs || (end && *end != 0)) {
            ok = false;
            break;
        }
        out.Append(v);
    }
    int n = len(out);
    ok = ok && (n == 1 || n == 2 || n == 4);
    for (int i = 0; ok && i < n; i++) {
        ok = out[i] >= 0 && out[i] <= 200;
    }
    if (!ok) {
        out.Reset();
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

static float ParseFloatTemp(Edit* e) {
    TempStr s = e->GetTextTemp();
    if (!s) {
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
    auto* g = &gGlobalPrefs->eBookUI;
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
    bool hasCustom = !!customCss;
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
        font = nullptr;
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
        out.margin.Reset();
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
        font = nullptr;
    }
    Vec<float> margin;
    ParseMargin(editMargin->GetTextTemp(), margin);
    // the same DPI the engine used: it takes it from DpiGet() when the document
    // is opened (EngineCreate.cpp)
    TempStr css = EbookGeneratedCssTemp(font, &margin, ParseFloatTemp(editSpacing), DpiGet());
    Str text = css ? css : _TRA("(the document's own styling is used as-is)");
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
        if (!customCssText) {
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
    auto* g = &gGlobalPrefs->eBookUI;
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
        SaveSettings();
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
        SaveSettings();
        return;
    }
    if (!fs->eBookUI) {
        fs->eBookUI = NewFileEBookUI();
    }
    FileEBookUI* f = fs->eBookUI;
    str::ReplaceWithCopy(&f->fontName, str::Eq(v.fontName, g->fontName) ? Str{} : v.fontName);
    f->fontSize = (v.fontSize == g->fontSize) ? 0 : v.fontSize;
    if (MarginEq(v.margin, g->margin)) {
        f->margin->Reset();
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
    SaveSettings();
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
        auto* g = &gGlobalPrefs->eBookUI;
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
    if (IsMainWindowValid(w) && w->IsDocLoaded()) {
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
    {
        CreateCustomArgs args;
        args.title = _TRA("eBook Settings");
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

    {
        auto* row = new HBox();
        row->alignMain = MainAxisAlign::MainStart;
        row->alignCross = CrossAxisAlign::CrossCenter;

        labelFont = NewVirtText({
            .s = _TRA("Font:"),
            .font = font,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(0, 8, 0, 0),
        });
        row->AddChild(labelFont);

        DropDown::CreateArgs args;
        args.parent = hwnd;
        args.font = GetFont();
        args.isRtl = isRtl;
        args.isEditable = true;
        ddFont = new DropDown();
        ddFont->Create(args);
        ddFont->onTextChanged = MkMethod0<EbookSettingsWnd, &EbookSettingsWnd::OnControlChanged>(this);
        ddFont->onSelectionChanged = MkMethod0<EbookSettingsWnd, &EbookSettingsWnd::OnControlChanged>(this);
        row->AddChild(ddFont, 1);
        vbox->AddChild(row);
    }

    {
        auto* row = new HBox();
        row->alignMain = MainAxisAlign::MainStart;
        row->alignCross = CrossAxisAlign::CrossCenter;

        labelSize = NewVirtText({
            .s = _TRA("Size:"),
            .font = font,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(8, 8, 0, 0),
        });
        row->AddChild(labelSize);

        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = GetFont();
        args.withBorder = true;
        args.isRtl = isRtl;
        args.idealWidthChars = 6;
        editSize = new Edit();
        editSize->SetInsetsPt(8, 0, 0, 0);
        editSize->Create(args);
        row->AddChild(editSize);

        labelMargin = NewVirtText({
            .s = _TRA("Margin:"),
            .font = font,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(8, 8, 0, 16),
        });
        row->AddChild(labelMargin);

        editMargin = new Edit();
        editMargin->SetInsetsPt(8, 0, 0, 0);
        editMargin->Create(args);
        editMargin->onTextChanged = MkMethod0<EbookSettingsWnd, &EbookSettingsWnd::OnControlChanged>(this);
        row->AddChild(editMargin);

        labelSpacing = NewVirtText({
            .s = _TRA("Line spacing:"),
            .font = font,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(8, 8, 0, 16),
        });
        row->AddChild(labelSpacing);

        args.idealWidthChars = 6;
        editSpacing = new Edit();
        editSpacing->SetInsetsPt(8, 0, 0, 0);
        editSpacing->Create(args);
        editSpacing->onTextChanged = MkMethod0<EbookSettingsWnd, &EbookSettingsWnd::OnControlChanged>(this);
        row->AddChild(editSpacing);
        vbox->AddChild(row);
    }

    {
        Checkbox::CreateArgs args;
        args.parent = hwnd;
        args.text = _TRA("&Ignore the document's own styling");
        args.isRtl = isRtl;
        cbIgnoreCss = new Checkbox();
        cbIgnoreCss->SetInsetsPt(10, 0, 0, 0);
        cbIgnoreCss->Create(args);
        vbox->AddChild(cbIgnoreCss);
    }

    {
        Checkbox::CreateArgs args;
        args.parent = hwnd;
        args.text = _TRA("&Custom CSS (edit the rules below)");
        args.isRtl = isRtl;
        cbCustomCss = new Checkbox();
        cbCustomCss->SetInsetsPt(8, 0, 0, 0);
        cbCustomCss->Create(args);
        cbCustomCss->onStateChanged = MkMethod0<EbookSettingsWnd, &EbookSettingsWnd::OnCustomCssToggled>(this);
        vbox->AddChild(cbCustomCss);
    }

    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = GetFont();
        args.isMultiLine = true;
        args.withBorder = true;
        args.isRtl = isRtl;
        args.idealSizeLines = 6;
        args.textPadding = 4;
        editCss = new Edit();
        editCss->SetInsetsPt(4, 0, 0, 0);
        editCss->Create(args);
        // the generated rules are one long line; without this the dialog would
        // grow as wide as the text
        editCss->maxDx = DpiScale(440);
        SendMessageW(editCss->hwnd, EM_SETREADONLY, TRUE, 0);
        SendMessageW(editCss->hwnd, EM_SETLIMITTEXT, 0, 0);
        vbox->AddChild(editCss, 1);
    }

    {
        auto* row = new HBox();
        row->alignMain = MainAxisAlign::MainStart;
        row->alignCross = CrossAxisAlign::CrossCenter;

        Checkbox::CreateArgs args;
        args.parent = hwnd;
        args.text = _TRA("&This file");
        args.isRtl = isRtl;
        args.isRadio = true;
        args.isGroupStart = true;
        args.initialState = Checkbox::State::Checked;
        radioThisFile = new Checkbox();
        radioThisFile->SetInsetsPt(10, 0, 0, 0);
        radioThisFile->Create(args);
        radioThisFile->onStateChanged = MkMethod0<EbookSettingsWnd, &EbookSettingsWnd::OnTargetChanged>(this);
        row->AddChild(radioThisFile);

        args.text = _TRA("For all &ebooks");
        args.isGroupStart = false;
        args.initialState = Checkbox::State::Unchecked;
        radioAllEbooks = new Checkbox();
        radioAllEbooks->SetInsetsPt(10, 0, 0, 12);
        radioAllEbooks->Create(args);
        radioAllEbooks->onStateChanged = MkMethod0<EbookSettingsWnd, &EbookSettingsWnd::OnTargetChanged>(this);
        row->AddChild(radioAllEbooks);
        vbox->AddChild(row);
    }

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::SpaceBetween;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        hbox->gap = font->averageCharWidth;
        auto pad = Insets{4, 0, 4, 0};

        btnReset = NewThemedButton(hwnd, _TRA("Reset to defaults"), font, false);
        btnReset->onClick = MkMethod1<EbookSettingsWnd, VirtMouseEvent*, &EbookSettingsWnd::OnReset>(this);
        hbox->AddChild(new Padding(btnReset, pad));

        auto* right = new HBox();
        right->alignMain = MainAxisAlign::MainEnd;
        right->alignCross = CrossAxisAlign::CrossCenter;
        right->gap = font->averageCharWidth;
        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), font, false);
        btnCancel->onClick = MkMethod1<EbookSettingsWnd, VirtMouseEvent*, &EbookSettingsWnd::OnCancel>(this);
        right->AddChild(new Padding(btnCancel, pad));
        btnOk = NewThemedButton(hwnd, _TRA("OK"), font, true);
        btnOk->onClick = MkMethod1<EbookSettingsWnd, VirtMouseEvent*, &EbookSettingsWnd::OnOk>(this);
        right->AddChild(new Padding(btnOk, pad));
        hbox->AddChild(right);
        vbox->AddChild(hbox);
    }

    layout = new Padding(vbox, DpiScaledInsets(4, 8));

    FillFontList();
    LoadFromTarget();

    int dx = DpiScale(460);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    HwndSetFocus(ddFont->hwnd);
    return true;
}

void ShowEbookSettingsDialog(MainWindow* win) {
    if (!IsMainWindowValid(win) || !win->CurrentTab() || !win->CurrentTab()->ctrl) {
        return;
    }
    if (gEbookSettingsWnd) {
        HwndSetFocus(gEbookSettingsWnd->hwnd);
        return;
    }
    auto* wnd = new EbookSettingsWnd();
    wnd->filePath = str::Dup(win->CurrentTab()->filePath);
    // no closeOnEsc: Esc while editing the CSS would throw the edits away
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
