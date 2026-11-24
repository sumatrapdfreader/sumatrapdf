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
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"

#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "resource.h"
#include "Translations.h"
#include "Theme.h"
#include "DarkMode.h"
#include "base/Pixmap.h"
#include "EngineBase.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"

// http://msdn.microsoft.com/en-us/library/ms645398(v=VS.85).aspx
#pragma pack(push, 1)
struct DLGTEMPLATEEX {
    WORD dlgVer;    // 0x0001
    WORD signature; // 0xFFFF
    DWORD helpID;
    DWORD exStyle;
    DWORD style;
    WORD cDlgItems;
    short x, y, cx, cy;
    /*
    sz_Or_Ord menu;
    sz_Or_Ord windowClass;
    WCHAR     title[titleLen];
    WORD      pointsize;
    WORD      weight;
    BYTE      italic;
    BYTE      charset;
    WCHAR     typeface[stringLen];
    */
};
#pragma pack(pop)

// allocated in arena, or with malloc() if arena is nullptr
static DLGTEMPLATE* DupTemplate(int dlgId, Arena* arena = nullptr) {
    HRSRC dialogRC = FindResourceW(nullptr, MAKEINTRESOURCE(dlgId), RT_DIALOG);
    ReportIf(!dialogRC);
    HGLOBAL dlgTemplate = LoadResource(nullptr, dialogRC);
    ReportIf(!dlgTemplate);
    void* orig = LockResource(dlgTemplate);
    int size = (int)SizeofResource(nullptr, dialogRC);
    ReportIf(size <= 0);
    DLGTEMPLATE* ret = (DLGTEMPLATE*)MemDup(arena, orig, (size_t)size);
    UnlockResource(orig);
    return ret;
}

static bool IsDlgTemplateEx(DLGTEMPLATE* tpl) {
    return tpl->style == MAKELONG(0x0001, 0xFFFF);
}

// gets a dialog template from the resources and sets the RTL flag
// cf. http://www.ureader.com/msg/1484387.aspx
static void SetDlgTemplateRtl(DLGTEMPLATE* tpl) {
    if (IsDlgTemplateEx(tpl)) {
        ((DLGTEMPLATEEX*)tpl)->exStyle |= WS_EX_LAYOUTRTL;
    } else {
        tpl->dwExtendedStyle |= WS_EX_LAYOUTRTL;
    }
}

static DLGTEMPLATE* GetRtLDlgTemplateTemp(int dlgId) {
    DLGTEMPLATE* tpl = DupTemplate(dlgId, GetTempArena());
    SetDlgTemplateRtl(tpl);
    return tpl;
}

void PrintAdvancedInitDlg(HWND hDlg) {
    DarkModeApplyToWindow(hDlg);
    HwndSetDlgItemText(hDlg, IDC_SECTION_PRINT_COMPATIBILITY, Tr("Compatibility"));
}

// the RTL dialog template is allocated in the temp arena so the returned page
// must be used before the temp arena is reset (e.g. inside PrintDlgEx())
void PrintAdvancedSetPage(PROPSHEETPAGE* psp) {
    psp->pszTemplate = MAKEINTRESOURCE(IDD_PROPSHEET_PRINT_ADVANCED);
    if (IsUIRtl()) {
        psp->pResource = GetRtLDlgTemplateTemp(IDD_PROPSHEET_PRINT_ADVANCED);
        psp->dwFlags |= PSP_DLGINDIRECT;
    }
}

// --- the buttons of the redrawn dialogs; here, not in VirtCtrl.cpp, for the
// theme colors (logview links VirtCtrl without Theme) ---

constexpr int kDlgBtnRadius = 4;

VirtButton* NewDlgButton(HWND hwndForDpi, Str text, PlatformFont* font, DlgButtonStyle style, bool isDefault) {
    VirtButton* b = NewThemedButton(hwndForDpi, text, font, isDefault);
    b->cornerRadius = DpiScale(kDlgBtnRadius);
    SetVirtButtonStyle(b, style);
    return b;
}

constexpr int kDlgBtnDisabledShift = 28;

void SetDlgButtonEnabled(VirtButton* b, DlgButtonStyle style, bool enabled) {
    if (enabled == b->HasFlag(vwfEnabled)) {
        return;
    }
    b->SetFlag(vwfEnabled, enabled);
    if (style == DlgButtonStyle::Filled) {
        Color colTxt = ThemeWindowTextColor();
        Color colBg = ThemeWindowControlBackgroundColor();
        b->SetColor(kColBtnBg, enabled ? colTxt : AccentColor(colBg, kDlgBtnDisabledShift));
        b->SetColor(kColBtnBorder, enabled ? colTxt : kColorTransparent);
        b->SetColor(kColBtnTextDisabled, ThemeWindowTextDisabledColor());
    }
    b->Invalidate();
}

PlatformFont* DlgUiFont(int px) {
    return GetUserGuiFont(StrL("Segoe UI"), DpiScale(px));
}

// creates the hidden popup window of a tool dialog, with the app icon and
// the dialog colors; false if the window could not be created
bool CreateToolDialog(WindowBase* w, HWND owner, Str title, PlatformFont* font, Color txt, Color bg) {
    CreateCustomArgs args;
    args.owner = owner;
    args.title = title;
    args.visible = false;
    args.style = WS_POPUPWINDOW | WS_CAPTION;
    args.font = font;
    args.isRtl = IsUIRtl();
    args.bgColor = bg;
    w->CreateCustom(args);
    if (!w->hwnd) {
        return false;
    }
    w->SetColors(txt, bg);
    return true;
}

// sizes the dialog to fit `layout` (at least minDx x minDy), lays it out and
// centers it on `centerOn`
void SizeToLayout(WindowBase* w, ILayout* layout, int minDx, int minDy, HWND centerOn) {
    LayoutAndSizeToContent(layout, minDx, minDy, w->hwnd);
    w->DoLayout();
    HwndCenterDialog(w->hwnd, centerOn);
}

// a label centered in a line `dy` dips high
VirtText* NewDlgLabel(Str s, PlatformFont* font, Color col, int dy, bool prefix) {
    VirtText* t = NewVirtText({.s = s, .font = font, .textColor = col, .isRtl = IsUIRtl(), .prefix = prefix});
    int lineDy = PlatformFontLineHeight(font);
    t->padding.top = std::max(0, (DpiScale(dy) - lineDy) / 2);
    t->padding.bottom = std::max(0, DpiScale(dy) - lineDy - t->padding.top);
    return t;
}

constexpr int kDlgCaptionPadDy = 14;

// muted text over the line it describes, cut with ... when too long
VirtText* NewDlgCaption(Str s, PlatformFont* font, Color col) {
    VirtText* t = NewVirtText({.s = s, .font = font, .textColor = col, .isRtl = IsUIRtl(), .ellipsis = true});
    int padDx = DpiScale(kToolDlgPadDx);
    t->padding = Insets{DpiScale(kDlgCaptionPadDy), padDx, 0, padDx};
    return t;
}

// a bare field over a hairline, so an empty one still reads as a field
ILayout* NewDlgUnderlined(ILayout* field) {
    auto* box = new VBox();
    box->alignCross = CrossAxisAlign::Stretch;
    box->AddChild(field);
    box->AddChild(new VirtLine());
    return box;
}

// controls side by side, centered, the dialogs' gap apart
HBox* NewDlgRow() {
    auto* row = new HBox();
    row->alignCross = CrossAxisAlign::CrossCenter;
    row->rtl = IsUIRtl();
    row->gap = DpiScale(kToolDlgGapDx);
    return row;
}

// `extra` (a quiet button) at the left, Cancel and the one filled action at
// the right
ILayout* NewDlgFooter(VirtButton* cancel, VirtButton* ok, VirtButton* extra) {
    HBox* row = NewDlgRow();
    if (extra) {
        row->AddChild(extra);
    }
    row->AddChild(new Spacer(0, 0), 1);
    if (cancel) {
        row->AddChild(cancel);
    }
    row->AddChild(ok);
    auto* box = new VBox();
    box->alignCross = CrossAxisAlign::Stretch;
    box->AddChild(new Padding(row, DpiScaledInsets(kToolDlgFootPadDy, kToolDlgPadDx)));
    return box;
}

constexpr int kDlgListRowDy = 24;
// the rows' text lines up with the dialog's fields
constexpr int kDlgListInsetDx = 8;
constexpr int kDlgListRadius = 4;
constexpr int kDlgListSelShift = 28;

// a row of a dialog's list: its text, on a rounded selection, and `meta`
// muted at the other end
void DrawDlgListItem(VirtListBox* lb, Gfx* gfx, Rect rc, bool selected, Str text, Str meta) {
    Color colBg = lb->GetColor(kColListBg);
    gfx->FillRect(rc, colBg);
    if (selected) {
        gfx->FillRoundedRect(rc, DpiScaleByDpi(lb->dpi, kDlgListRadius), AccentColor(colBg, kDlgListSelShift));
    }
    int inset = DpiScaleByDpi(lb->dpi, kDlgListInsetDx);
    Rect rcText{rc.x + inset, rc.y, rc.dx - (2 * inset), rc.dy};
    bool isRtl = IsUIRtl();
    if (len(meta) > 0) {
        u32 fmtMeta = gfxTextVCenter | (isRtl ? (gfxTextLeft | gfxTextRtl) : gfxTextRight);
        gfx->DrawText(meta, rcText, fmtMeta, lb->font, ThemeWindowDarkerTextColor());
        int metaDx = gfx->MeasureText(meta, lb->font).dx + DpiScaleByDpi(lb->dpi, kToolDlgGapDx);
        rcText.dx -= metaDx;
        if (isRtl) {
            rcText.x += metaDx;
        }
    }
    u32 fmt = gfxTextEllipsis | gfxTextVCenter;
    fmt |= isRtl ? (gfxTextRight | gfxTextRtl) : gfxTextLeft;
    gfx->DrawText(text, rcText, fmt, lb->font, lb->GetColor(kColListText));
}

static void DrawDlgListRow(VirtListBox::DrawItemEvent* ev) {
    VirtListBox* lb = ev->listBox;
    DrawDlgListItem(lb, ev->gfx, ev->itemRect, ev->selected, lb->model->Item(ev->itemIndex), {});
}

// a list on the dialog's background, its rows drawn by DrawDlgListRow
VirtListBox* NewDlgList(int dpi, PlatformFont* font) {
    auto* lb = new VirtListBox();
    lb->dpi = dpi;
    lb->font = font;
    lb->itemDy = DpiScaleByDpi(dpi, kDlgListRowDy);
    lb->showFocusRing = false;
    SetDlgListColors(lb);
    lb->onDrawItem = MkFunc1Void<VirtListBox::DrawItemEvent*>(DrawDlgListRow);
    return lb;
}

void SetDlgListColors(VirtListBox* lb) {
    lb->SetColor(kColListText, ThemeWindowTextColor());
    lb->SetColor(kColListBg, ThemeWindowControlBackgroundColor());
}

// the list under a hairline, its rows' text in line with the fields
ILayout* NewDlgListBox(VirtListBox* lb) {
    return new Padding(lb, DpiScaledInsets(kToolDlgFilesPadDy, kToolDlgPadDx - kDlgListInsetDx));
}

// a bare field in the dialog's colors; the caller underlines it
Edit* NewDlgEdit(HWND parent, PlatformFont* font, Str text, int idealChars) {
    Edit::CreateArgs args;
    args.parent = parent;
    args.font = font;
    args.withBorder = false;
    args.isRtl = IsUIRtl();
    args.text = text;
    args.idealWidthChars = idealChars;
    auto* e = new Edit();
    e->SetColors(ThemeWindowTextColor(), ThemeWindowControlBackgroundColor());
    e->Create(args);
    return e;
}

static Checkbox* NewDlgCheckboxOrRadio(HWND parent, Str text, PlatformFont* font, bool checked, bool isRadio,
                                       bool groupStart) {
    Checkbox::CreateArgs args;
    args.parent = parent;
    args.text = text;
    args.font = font;
    args.isRtl = IsUIRtl();
    args.isRadio = isRadio;
    args.isGroupStart = groupStart;
    args.initialState = checked ? Checkbox::State::Checked : Checkbox::State::Unchecked;
    auto* c = new Checkbox();
    c->Create(args);
    c->SetColors(ThemeWindowTextColor(), ThemeWindowControlBackgroundColor());
    return c;
}

Checkbox* NewDlgCheckbox(HWND parent, Str text, PlatformFont* font, bool checked) {
    return NewDlgCheckboxOrRadio(parent, text, font, checked, false, false);
}

// groupStart: the first radio of its group
Checkbox* NewDlgRadio(HWND parent, Str text, PlatformFont* font, bool checked, bool groupStart) {
    return NewDlgCheckboxOrRadio(parent, text, font, checked, true, groupStart);
}

// a row of the dialog's controls, as tall as a row of a list
HBox* NewDlgFieldRow() {
    HBox* row = NewDlgRow();
    row->minDy = DpiScale(kToolDlgRowDy);
    return row;
}

// the controls between two hairlines, padded as the tool dialogs pad them
ILayout* NewDlgSection(ILayout* content) {
    return new Padding(content, DpiScaledInsets(kToolDlgFilesPadDy, kToolDlgPadDx));
}

// the labels as wide as the widest, so the fields after them line up
void AlignDlgLabels(const Vec<VirtText*>& labels) {
    int dx = 0;
    for (VirtText* t : labels) {
        dx = std::max(dx, t->GetIdealSize().dx);
    }
    for (VirtText* t : labels) {
        t->padding.right += dx - t->GetIdealSize().dx;
    }
}
