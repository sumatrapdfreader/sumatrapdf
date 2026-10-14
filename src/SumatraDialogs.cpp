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

/*
Type: sz_Or_Ord

A variable-length array of 16-bit elements that identifies a menu resource for the dialog box. If the first element of
this array is 0x0000, the dialog box has no menu and the array has no other elements. If the first element is 0xFFFF,
the array has one additional element that specifies the ordinal value of a menu resource in an executable file. If the
first element has any other value, the system treats the array as a null-terminated Unicode string that specifies the
name of a menu resource in an executable file.
*/
static u8* SkipSzOrOrd(u8* d) {
    WORD* pw = (WORD*)d;
    WORD w = *pw++;
    if (w == 0x0000) {
        // no menu
    } else if (w == 0xffff) {
        // menu id followed by another WORD item
        pw++;
    } else {
        // anything else: zero-terminated WCHAR*
        WCHAR* s = (WCHAR*)pw;
        while (*s) {
            s++;
        }
        s++;
        pw = (WORD*)s;
    }
    return (u8*)pw;
}

static u8* SkipSz(u8* d) {
    WCHAR* s = (WCHAR*)d;
    while (*s) {
        s++;
    }
    s++;
    return (u8*)s;
}

static bool IsDlgTemplateEx(DLGTEMPLATE* tpl) {
    return tpl->style == MAKELONG(0x0001, 0xFFFF);
}

static bool HasDlgTemplateExFont(DLGTEMPLATEEX* tpl) {
    DWORD style = tpl->style & (DS_SETFONT | DS_FIXEDSYS);
    return style != 0;
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

static int ToFontPointSize(int fontSize) {
    int res = (fontSize * 72) / 96;
    return res;
}

// https://stackoverflow.com/questions/14370238/can-i-dynamically-change-the-font-size-of-a-dialog-window-created-with-c-in-vi
// TODO: if changing font name would have do more complicated dance of replacing
// variable string in the middle of the struct
static void SetDlgTemplateExFont(DLGTEMPLATE* tmp, bool isRtl, int fontSize) {
    ReportIf(!IsDlgTemplateEx(tmp));
    if (isRtl) {
        SetDlgTemplateRtl(tmp);
    }
    DLGTEMPLATEEX* tpl = (DLGTEMPLATEEX*)tmp;
    ReportIf(!HasDlgTemplateExFont(tpl));
    u8* d = (u8*)tpl;
    d += sizeof(DLGTEMPLATEEX);
    // sz_Or_Ord menu
    d = SkipSzOrOrd(d);
    // sz_Or_Ord windowClass;
    d = SkipSzOrOrd(d);
    // WCHAR[] title
    d = SkipSz(d);
    // WCHAR pointSize;
    WORD* wd = (WORD*)d;
    fontSize = ToFontPointSize(fontSize);
    *wd = (WORD)fontSize;
}

static DLGTEMPLATE* GetRtLDlgTemplateTemp(int dlgId) {
    DLGTEMPLATE* tpl = DupTemplate(dlgId, GetTempArena());
    SetDlgTemplateRtl(tpl);
    return tpl;
}

// creates a dialog box that dynamically gets a right-to-left layout if needed
[[maybe_unused]] static INT_PTR CreateDialogBox(int dlgId, HWND parent, DLGPROC DlgProc, LPARAM data) {
    bool isRtl = IsUIRtl();
    bool isDefaultFont = IsAppFontSizeDefault();
    if (!isRtl && isDefaultFont) {
        return DialogBoxParam(nullptr, MAKEINTRESOURCE(dlgId), parent, DlgProc, data);
    }

    DLGTEMPLATE* tpl = DupTemplate(dlgId);
    int fntSize = GetAppFontSize();
    if (isDefaultFont) {
        SetDlgTemplateRtl(tpl);
    } else {
        SetDlgTemplateExFont(tpl, isRtl, fntSize);
    }

    INT_PTR res = DialogBoxIndirectParamW(nullptr, tpl, parent, DlgProc, data);
    free(tpl);
    return res;
}

#ifndef ID_APPLY_NOW
constexpr int ID_APPLY_NOW = 0x3021;
#endif

static INT_PTR CALLBACK Sheet_Print_Advanced_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    Print_Advanced_Data* data;

    switch (msg) {
        //[ ACCESSKEY_GROUP Advanced Print Tab
        case WM_INITDIALOG:
            data = (Print_Advanced_Data*)((PROPSHEETPAGE*)lp)->lParam;
            SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
            DarkModeApplyToWindow(hDlg);
            HwndSetDlgItemText(hDlg, IDC_SECTION_PRINT_RANGE, Tr("Print range"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_RANGE_ALL, Tr("&All selected pages"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_RANGE_EVEN, Tr("&Even pages only"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_RANGE_ODD, Tr("&Odd pages only"));
            HwndSetDlgItemText(hDlg, IDC_SECTION_PRINT_SCALE, Tr("Page scaling"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_SCALE_SHRINK, Tr("&Shrink pages to printable area"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_SCALE_FIT, Tr("&Fit pages to printable area"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_SCALE_STRETCH, Tr("S&tretch pages to fill paper"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_SCALE_NONE, Tr("A&ctual size (1:1)"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_CENTER_HORIZONTALLY, Tr("Center page hori&zontally on the paper"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_PAPER_SOURCE_BY_SIZE, Tr("Choose &paper source by document page size"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_PER_PAGE_PAPER_SIZE,
                               Tr("Print each page at its &document page size (mixed sizes)"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_ROTATE_LABEL, Tr("&Rotate printout:"));
            {
                HWND hwndCb = GetDlgItem(hDlg, IDC_PRINT_ROTATE);
                CbAddString(hwndCb, Tr("None"));
                CbAddString(hwndCb, StrL("90°"));
                CbAddString(hwndCb, StrL("180°"));
                CbAddString(hwndCb, StrL("270°"));
                int rotIdx = (data->extraRotation / 90) % 4;
                CbSetCurrentSelection(hwndCb, rotIdx);
            }
            HwndSetDlgItemText(hDlg, IDC_SECTION_PRINT_COMPATIBILITY, Tr("Compatibility"));

            {
                int rangeId = IDC_PRINT_RANGE_ALL;
                if (data->range == PrintRangeAdv::Even) {
                    rangeId = IDC_PRINT_RANGE_EVEN;
                } else if (data->range == PrintRangeAdv::Odd) {
                    rangeId = IDC_PRINT_RANGE_ODD;
                }
                CheckRadioButton(hDlg, IDC_PRINT_RANGE_ALL, IDC_PRINT_RANGE_ODD, rangeId);
            }
            {
                int scaleId = IDC_PRINT_SCALE_NONE;
                if (data->scale == PrintScaleAdv::Fit) {
                    scaleId = IDC_PRINT_SCALE_FIT;
                } else if (data->scale == PrintScaleAdv::Stretch) {
                    scaleId = IDC_PRINT_SCALE_STRETCH;
                } else if (data->scale == PrintScaleAdv::Shrink) {
                    scaleId = IDC_PRINT_SCALE_SHRINK;
                }
                CheckRadioButton(hDlg, IDC_PRINT_SCALE_SHRINK, IDC_PRINT_SCALE_STRETCH, scaleId);
            }

            CheckDlgButton(hDlg, IDC_PRINT_CENTER_HORIZONTALLY, data->centerHorizontally ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_PRINT_PAPER_SOURCE_BY_SIZE,
                           data->paperSourceByPageSize ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_PRINT_PER_PAGE_PAPER_SIZE, data->perPagePaperSize ? BST_CHECKED : BST_UNCHECKED);

            return FALSE;
            //] ACCESSKEY_GROUP Advanced Print Tab

        case WM_NOTIFY:
            if (((LPNMHDR)lp)->code == PSN_APPLY) {
                data = (Print_Advanced_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
                if (IsDlgButtonChecked(hDlg, IDC_PRINT_RANGE_EVEN)) {
                    data->range = PrintRangeAdv::Even;
                } else if (IsDlgButtonChecked(hDlg, IDC_PRINT_RANGE_ODD)) {
                    data->range = PrintRangeAdv::Odd;
                } else {
                    data->range = PrintRangeAdv::All;
                }
                if (IsDlgButtonChecked(hDlg, IDC_PRINT_SCALE_FIT)) {
                    data->scale = PrintScaleAdv::Fit;
                } else if (IsDlgButtonChecked(hDlg, IDC_PRINT_SCALE_STRETCH)) {
                    data->scale = PrintScaleAdv::Stretch;
                } else if (IsDlgButtonChecked(hDlg, IDC_PRINT_SCALE_SHRINK)) {
                    data->scale = PrintScaleAdv::Shrink;
                } else {
                    data->scale = PrintScaleAdv::None;
                }
                data->centerHorizontally = IsDlgButtonChecked(hDlg, IDC_PRINT_CENTER_HORIZONTALLY) != 0;
                data->paperSourceByPageSize = IsDlgButtonChecked(hDlg, IDC_PRINT_PAPER_SOURCE_BY_SIZE) != 0;
                data->perPagePaperSize = IsDlgButtonChecked(hDlg, IDC_PRINT_PER_PAGE_PAPER_SIZE) != 0;
                int rotIdx = (int)SendDlgItemMessage(hDlg, IDC_PRINT_ROTATE, CB_GETCURSEL, 0, 0);
                data->extraRotation = rotIdx > 0 ? rotIdx * 90 : 0;
                return TRUE;
            }
            break;

        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDC_PRINT_RANGE_ALL:
                case IDC_PRINT_RANGE_EVEN:
                case IDC_PRINT_RANGE_ODD:
                case IDC_PRINT_SCALE_SHRINK:
                case IDC_PRINT_SCALE_FIT:
                case IDC_PRINT_SCALE_STRETCH:
                case IDC_PRINT_SCALE_NONE:
                case IDC_PRINT_CENTER_HORIZONTALLY:
                case IDC_PRINT_PAPER_SOURCE_BY_SIZE:
                case IDC_PRINT_PER_PAGE_PAPER_SIZE: {
                    HWND hApplyButton = GetDlgItem(GetParent(hDlg), ID_APPLY_NOW);
                    EnableWindow(hApplyButton, TRUE);
                } break;
                case IDC_PRINT_ROTATE:
                    if (HIWORD(wp) == CBN_SELCHANGE) {
                        EnableWindow(GetDlgItem(GetParent(hDlg), ID_APPLY_NOW), TRUE);
                    }
                    break;
            }
    }
    return FALSE;
}

// the RTL dialog template is allocated in the temp arena so the returned page
// must be used before the temp arena is reset (e.g. inside PrintDlgEx())
HPROPSHEETPAGE CreatePrintAdvancedPropSheet(Print_Advanced_Data* data) {
    PROPSHEETPAGE psp{};

    psp.dwSize = sizeof(PROPSHEETPAGE);
    psp.dwFlags = PSP_USETITLE | PSP_PREMATURE;
    psp.pszTemplate = MAKEINTRESOURCE(IDD_PROPSHEET_PRINT_ADVANCED);
    psp.pfnDlgProc = Sheet_Print_Advanced_Proc;
    psp.lParam = (LPARAM)data;
    auto s = Tr("Advanced");
    psp.pszTitle = CWStrTemp(s);

    if (IsUIRtl()) {
        psp.pResource = GetRtLDlgTemplateTemp(IDD_PROPSHEET_PRINT_ADVANCED);
        psp.dwFlags |= PSP_DLGINDIRECT;
    }

    return CreatePropertySheetPage(&psp);
}

// --- the buttons of the redrawn dialogs; here, not in VirtCtrl.cpp, for the
// theme colors (logview links VirtCtrl without Theme) ---

constexpr int kDlgBtnRadius = 4;

VirtButton* NewDlgButton(HWND hwndForDpi, Str text, PlatformFont* font, DlgButtonStyle style, bool isDefault) {
    VirtButton* b = NewThemedButton(hwndForDpi, text, font, isDefault);
    b->cornerRadius = DpiScale(kDlgBtnRadius);
    SetDlgButtonColors(b, style);
    return b;
}

// the style's colors in the current theme
void SetDlgButtonColors(VirtButton* b, DlgButtonStyle style) {
    SetVirtButtonStyle(b, style);
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

// a hairline, then `extra` (a quiet button) at the left, Cancel and the one
// filled action at the right
ILayout* NewDlgFooter(VirtButton* cancel, VirtButton* ok, VirtButton* extra) {
    HBox* row = NewDlgRow();
    if (extra) {
        row->AddChild(extra);
    }
    row->AddChild(new Spacer(0, 0), 1);
    row->AddChild(cancel);
    row->AddChild(ok);
    auto* box = new VBox();
    box->alignCross = CrossAxisAlign::Stretch;
    box->AddChild(new VirtLine());
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
        gfx->DrawText(meta, rcText, fmtMeta, lb->font, ThemeWindowTextDisabledColor());
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
