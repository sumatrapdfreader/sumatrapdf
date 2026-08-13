/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/win/DialogSizer.h"
#include "base/Win.h"

#include "Settings.h"
#include "AppSettings.h"

#include "GlobalPrefs.h"

#include "SumatraPDF.h"
#include "resource.h"
#include "AppTools.h"
#include "SumatraDialogs.h"
#include "Translations.h"
#include "Theme.h"
#include "DarkModeSubclass.h"

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

static DLGTEMPLATE* DupTemplate(int dlgId) {
    HRSRC dialogRC = FindResourceW(nullptr, MAKEINTRESOURCE(dlgId), RT_DIALOG);
    ReportIf(!dialogRC);
    HGLOBAL dlgTemplate = LoadResource(nullptr, dialogRC);
    ReportIf(!dlgTemplate);
    void* orig = LockResource(dlgTemplate);
    int size = (int)SizeofResource(nullptr, dialogRC);
    ReportIf(size <= 0);
    DLGTEMPLATE* ret = (DLGTEMPLATE*)memdup(orig, size);
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

static DLGTEMPLATE* GetRtLDlgTemplate(int dlgId) {
    DLGTEMPLATE* tpl = DupTemplate(dlgId);
    SetDlgTemplateRtl(tpl);
    return tpl;
}

// creates a dialog box that dynamically gets a right-to-left layout if needed
static INT_PTR CreateDialogBox(int dlgId, HWND parent, DLGPROC DlgProc, LPARAM data) {
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

TempStr ZoomLevelStr(float zoom) {
    if (zoom == kZoomFitPage) {
        return _TRA("Fit Page");
    }
    if (zoom == kZoomFitWidth) {
        return _TRA("Fit Width");
    }
    if (zoom == kZoomFitHeight) {
        return _TRA("Fit Height");
    }
    if (zoom == kZoomFitContent) {
        return _TRA("Fit Content");
    }
    if (zoom == kZoomShrinkToFit) {
        return _TRA("Shrink To Fit");
    }
    if (zoom == kZoomFitByOrientation) {
        return _TRA("Fit by Orientation");
    }
    if (zoom == 0) {
        return "-";
    }
    TempStr res = fmt("%.f%%", zoom);
    return res;
}

// clang-format off
static float gZoomLevels[] = {
    kZoomFitPage,
    kZoomFitWidth,
    kZoomFitHeight,
    kZoomFitByOrientation,
    kZoomFitContent,
    kZoomShrinkToFit,
    0,
    6400.0,
    3200.0,
    1600.0,
    800.0,
    400.0,
    200.0,
    150.0,
    125.0,
    100.0,
    50.0,
    25.0,
    12.5,
    8.33f
};
static float gZoomLevelsChm[] = {
    800.0,
    400.0,
    200.0,
    150.0,
    125.0,
    100.0,
    50.0,
    25.0,
};
// clang-format on

// Fit/preset zoom values for the zoom combo (Settings) and Custom Zoom dialog.
void CollectZoomLevels(Vec<float>& out, bool forChm) {
    out.Reset();
    auto* customZoomLevels = gGlobalPrefs->zoomLevels;
    int n = customZoomLevels ? len(*customZoomLevels) : 0;
    if (n > 0) {
        if (!forChm) {
            for (int i = 0; i < 4; i++) {
                out.Append(gZoomLevels[i]);
            }
        }
        float maxZoom = forChm ? 800 : kZoomMax;
        float minZoom = forChm ? 16 : kZoomMin;
        for (int i = 0; i < n; i++) {
            float zl = (*customZoomLevels)[n - i - 1]; // largest first
            if (zl >= minZoom && zl <= maxZoom) {
                out.Append(zl);
            }
        }
        return;
    }
    float* zoomLevels = forChm ? gZoomLevelsChm : gZoomLevels;
    n = forChm ? dimofi(gZoomLevelsChm) : dimofi(gZoomLevels);
    for (int i = 0; i < n; i++) {
        out.Append(zoomLevels[i]);
    }
}

// Detected text-editor command lines plus the current setting, if any.
void CollectInverseSearchCommands(StrVec& out, Str cmdLine) {
    out.Reset();
    Vec<TextEditor*> textEditors;
    DetectTextEditors(textEditors);
    for (auto* e : textEditors) {
        AppendIfNotExists(&out, e->openFileCmd);
    }
    if (cmdLine) {
        AppendIfNotExists(&out, cmdLine);
    }
}

#ifndef ID_APPLY_NOW
#define ID_APPLY_NOW 0x3021
#endif

static INT_PTR CALLBACK Sheet_Print_Advanced_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    Print_Advanced_Data* data;

    switch (msg) {
        //[ ACCESSKEY_GROUP Advanced Print Tab
        case WM_INITDIALOG:
            data = (Print_Advanced_Data*)((PROPSHEETPAGE*)lp)->lParam;
            SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
            if (UseDarkModeLib()) {
                DarkMode::setDarkWndSafe(hDlg);
            }
            HwndSetDlgItemText(hDlg, IDC_SECTION_PRINT_RANGE, _TRA("Print range"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_RANGE_ALL, _TRA("&All selected pages"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_RANGE_EVEN, _TRA("&Even pages only"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_RANGE_ODD, _TRA("&Odd pages only"));
            HwndSetDlgItemText(hDlg, IDC_SECTION_PRINT_SCALE, _TRA("Page scaling"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_SCALE_SHRINK, _TRA("&Shrink pages to printable area (if necessary)"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_SCALE_FIT, _TRA("&Fit pages to printable area"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_SCALE_STRETCH,
                               _TRA("S&tretch pages to fill paper (ignore aspect ratio)"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_SCALE_NONE, _TRA("&Use original page sizes"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_CENTER_HORIZONTALLY, _TRA("Center page hori&zontally on the paper"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_PAPER_SOURCE_BY_SIZE,
                               _TRA("Choose &paper source by document page size"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_PER_PAGE_PAPER_SIZE,
                               _TRA("Print each page at its &document page size (mixed sizes)"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_ROTATE_LABEL, _TRA("&Rotate printout:"));
            {
                HWND hwndCb = GetDlgItem(hDlg, IDC_PRINT_ROTATE);
                CbAddString(hwndCb, _TRA("None"));
                CbAddString(hwndCb, "90°");
                CbAddString(hwndCb, "180°");
                CbAddString(hwndCb, "270°");
                int rotIdx = (data->extraRotation / 90) % 4;
                CbSetCurrentSelection(hwndCb, rotIdx);
            }
            HwndSetDlgItemText(hDlg, IDC_SECTION_PRINT_COMPATIBILITY, _TRA("Compatibility"));

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

HPROPSHEETPAGE CreatePrintAdvancedPropSheet(Print_Advanced_Data* data, ScopedMem<DLGTEMPLATE>& dlgTemplate) {
    PROPSHEETPAGE psp{};

    psp.dwSize = sizeof(PROPSHEETPAGE);
    psp.dwFlags = PSP_USETITLE | PSP_PREMATURE;
    psp.pszTemplate = MAKEINTRESOURCE(IDD_PROPSHEET_PRINT_ADVANCED);
    psp.pfnDlgProc = Sheet_Print_Advanced_Proc;
    psp.lParam = (LPARAM)data;
    auto s = _TRA("Advanced");
    psp.pszTitle = CWStrTemp(s);

    if (IsUIRtl()) {
        dlgTemplate.Set(GetRtLDlgTemplate(IDD_PROPSHEET_PRINT_ADVANCED));
        psp.pResource = dlgTemplate.Get();
        psp.dwFlags |= PSP_DLGINDIRECT;
    }

    return CreatePropertySheetPage(&psp);
}

// --- Change Background Color dialog ---

static const int kMaxCustomColors = 13;

struct BgColorDlgData {
    Color currentColor; // current selected color
    bool isCheckered;   // true if "checkered" is selected
    bool applyToAll;    // radio: all files like this
    Color customColors[kMaxCustomColors];
    bool customColorSet[kMaxCustomColors]; // true if slot has a color
    bool customColorsChanged;
    int selectedCustomIdx; // -1 = no custom button selected
    bool previewSelected;  // true if preview button is selected
    Str title;             // dialog title (empty = default)
    bool showRadioButtons; // show "this file" / "all files" radio buttons
    Str allFilesLabel;     // label for "all files" radio button (empty = default)
};

// fixed preset colors: checkered, black, white
static const Color kBgPresetColors[] = {
    kColorUnset, // checkered
    kColBlack,
    kColWhite,
};
static const int kNumPresets = 3;

static void ParseCustomColors(BgColorDlgData* data) {
    for (int i = 0; i < kMaxCustomColors; i++) {
        data->customColorSet[i] = false;
        data->customColors[i] = 0;
    }
    data->customColorsChanged = false;
    Str s = gGlobalPrefs->customColors;
    if (len(s) == 0) {
        return;
    }
    int idx = 0;
    int i = 0;
    while (i < s.len && idx < kMaxCustomColors) {
        while (i < s.len && s.s[i] == ' ') {
            i++;
        }
        if (i >= s.len) {
            break;
        }
        int start = i;
        while (i < s.len && s.s[i] != ' ') {
            i++;
        }
        ParsedColor parsed;
        ParseColor(parsed, Str(s.s + start, i - start));
        if (parsed.parsedOk) {
            data->customColors[idx] = parsed.col;
            data->customColorSet[idx] = true;
            idx++;
        }
    }
}

static void SaveCustomColors(BgColorDlgData* data) {
    str::Builder buf;
    for (int i = 0; i < kMaxCustomColors; i++) {
        if (!data->customColorSet[i]) {
            continue;
        }
        if (len(buf) > 0) {
            buf.AppendChar(' ');
        }
        TempStr cs = SerializeColorTemp(data->customColors[i]);
        buf.Append(cs);
    }
    str::ReplaceWithCopy(&gGlobalPrefs->customColors, ToStr(buf));
    SaveSettings();
}

static void HsvToRgb(float h, float s, float v, u8& r, u8& g, u8& b) {
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float rf, gf, bf;
    if (h < 60) {
        rf = c;
        gf = x;
        bf = 0;
    } else if (h < 120) {
        rf = x;
        gf = c;
        bf = 0;
    } else if (h < 180) {
        rf = 0;
        gf = c;
        bf = x;
    } else if (h < 240) {
        rf = 0;
        gf = x;
        bf = c;
    } else if (h < 300) {
        rf = x;
        gf = 0;
        bf = c;
    } else {
        rf = c;
        gf = 0;
        bf = x;
    }
    r = (u8)((rf + m) * 255.0f);
    g = (u8)((gf + m) * 255.0f);
    b = (u8)((bf + m) * 255.0f);
}

static void PaintColorArea(HDC hdc, RECT* rc) {
    int w = rc->right - rc->left;
    int h = rc->bottom - rc->top;
    if (w <= 0 || h <= 0) {
        return;
    }
    // rows must be DWORD-aligned; each pixel is 3 bytes (BGR)
    int stride = ((w * 3) + 3) & ~3;
    // cast before multiply so the product cannot overflow int→size_t
    u8* bits = (u8*)malloc((size_t)stride * (size_t)h);
    if (!bits) {
        return;
    }
    for (int y = 0; y < h; y++) {
        float val = 1.0f - ((float)y / (float)h);
        u8* row = bits + ((size_t)y * stride);
        for (int x = 0; x < w; x++) {
            float hue = (float)x / (float)w * 360.0f;
            u8 r, g, b;
            HsvToRgb(hue, 1.0f, val, r, g, b);
            row[(size_t)x * 3] = b;
            row[(x * 3) + 1] = g;
            row[(x * 3) + 2] = r;
        }
    }
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(hdc, rc->left, rc->top, w, h, 0, 0, 0, h, bits, &bmi, DIB_RGB_COLORS);
    free(bits);
}

static void SelectPreviewButton(HWND hDlg, BgColorDlgData* data) {
    int prevCustom = data->selectedCustomIdx;
    bool wasPreview = data->previewSelected;
    data->selectedCustomIdx = -1;
    data->previewSelected = true;
    if (prevCustom >= 0) {
        HwndInvalidate(GetDlgItem(hDlg, IDC_BGCOL_CUSTOM_FIRST + prevCustom), true);
    }
    if (!wasPreview) {
        HwndInvalidate(GetDlgItem(hDlg, IDC_BGCOL_PREVIEW), true);
    }
}

static void SelectCustomButton(HWND hDlg, BgColorDlgData* data, int idx) {
    int prevCustom = data->selectedCustomIdx;
    bool wasPreview = data->previewSelected;
    data->selectedCustomIdx = idx;
    data->previewSelected = false;
    if (prevCustom >= 0 && prevCustom != idx) {
        HwndInvalidate(GetDlgItem(hDlg, IDC_BGCOL_CUSTOM_FIRST + prevCustom), true);
    }
    if (wasPreview) {
        HwndInvalidate(GetDlgItem(hDlg, IDC_BGCOL_PREVIEW), true);
    }
    HwndInvalidate(GetDlgItem(hDlg, IDC_BGCOL_CUSTOM_FIRST + idx), true);
}

static void InvalidatePreview(HWND hDlg, BgColorDlgData* data) {
    if (data->selectedCustomIdx >= 0) {
        HwndInvalidate(GetDlgItem(hDlg, IDC_BGCOL_CUSTOM_FIRST + data->selectedCustomIdx), true);
    }
    if (data->previewSelected) {
        HwndInvalidate(GetDlgItem(hDlg, IDC_BGCOL_PREVIEW), true);
    }
}

static void UpdateBgColorEditFromColor(HWND hDlg, BgColorDlgData* data) {
    if (data->isCheckered) {
        HwndSetDlgItemText(hDlg, IDC_BGCOL_EDIT, data->showRadioButtons ? "checkered" : "unset");
    } else {
        TempStr s = SerializeColorTemp(data->currentColor);
        HwndSetDlgItemText(hDlg, IDC_BGCOL_EDIT, s);
    }
    // update selected custom button color and refresh preview
    if (data->selectedCustomIdx >= 0 && !data->isCheckered) {
        data->customColors[data->selectedCustomIdx] = data->currentColor;
        data->customColorSet[data->selectedCustomIdx] = true;
        data->customColorsChanged = true;
    }
    InvalidatePreview(hDlg, data);
}

static bool TryParseBgColorEdit(HWND hDlg, BgColorDlgData* data) {
    TempStr text = HwndGetTextTemp(GetDlgItem(hDlg, IDC_BGCOL_EDIT));
    if (!text || !text.s[0]) {
        return false;
    }
    ParsedColor parsed;
    ParseColor(parsed, text);
    if (!parsed.parsedOk) {
        return false;
    }
    if (parsed.col == kColorUnset) {
        data->isCheckered = true;
    } else {
        data->isCheckered = false;
        data->currentColor = parsed.col;
    }
    return true;
}

static void PickColorFromArea(HWND hwndCA, BgColorDlgData* data, HWND hDlg) {
    Point pt = HwndGetCursorPos(hwndCA);
    HDC hdcCA = GetDC(hwndCA);
    Color picked = GetPixel(hdcCA, pt.x, pt.y);
    ReleaseDC(hwndCA, hdcCA);
    if (picked != CLR_INVALID) {
        data->isCheckered = false;
        data->currentColor = picked;
        UpdateBgColorEditFromColor(hDlg, data);
    }
}

static WNDPROC gOrigColorAreaProc = nullptr;

static LRESULT CALLBACK ColorAreaSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    HWND hDlg = GetParent(hwnd);
    BgColorDlgData* data = (BgColorDlgData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);

    switch (msg) {
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            PickColorFromArea(hwnd, data, hDlg);
            return 0;
        case WM_MOUSEMOVE:
            if (wp & MK_LBUTTON) {
                PickColorFromArea(hwnd, data, hDlg);
            }
            return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            return 0;
    }
    return CallWindowProcW(gOrigColorAreaProc, hwnd, msg, wp, lp);
}

static INT_PTR CALLBACK Dialog_ChangeBgColor_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    BgColorDlgData* data;
    if (msg == WM_INITDIALOG) {
        data = (BgColorDlgData*)lp;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)data);
    } else {
        data = (BgColorDlgData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
    }

    switch (msg) {
        case WM_INITDIALOG: {
            if (UseDarkModeLib()) {
                DarkMode::setDarkWndSafe(hDlg);
            }
            HwndSetText(hDlg, data->title ? data->title : _TRA("Change Background Color"));
            HwndSetDlgItemText(hDlg, IDOK, _TRA("OK"));
            HwndSetDlgItemText(hDlg, IDCANCEL, _TRA("Cancel"));
            if (data->showRadioButtons) {
                if (data->allFilesLabel) {
                    HwndSetDlgItemText(hDlg, IDC_BGCOL_ALL_FILES, data->allFilesLabel);
                }
                CheckRadioButton(hDlg, IDC_BGCOL_THIS_FILE, IDC_BGCOL_ALL_FILES,
                                 data->applyToAll ? IDC_BGCOL_ALL_FILES : IDC_BGCOL_THIS_FILE);
            } else {
                ShowWindow(GetDlgItem(hDlg, IDC_BGCOL_THIS_FILE), SW_HIDE);
                ShowWindow(GetDlgItem(hDlg, IDC_BGCOL_ALL_FILES), SW_HIDE);
            }
            ParseCustomColors(data);
            UpdateBgColorEditFromColor(hDlg, data);
            // subclass color area for mouse drag tracking
            HWND hwndCA = GetDlgItem(hDlg, IDC_BGCOL_COLORAREA);
            gOrigColorAreaProc = (WNDPROC)SetWindowLongPtrW(hwndCA, GWLP_WNDPROC, (LONG_PTR)ColorAreaSubclassProc);
            HwndCenterDialog(hDlg);
            return TRUE;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
            int ctlId = (int)dis->CtlID;
            if (ctlId == IDC_BGCOL_COLORAREA) {
                PaintColorArea(dis->hDC, &dis->rcItem);
                return TRUE;
            }
            // preview button shows the currently selected color
            if (ctlId == IDC_BGCOL_PREVIEW) {
                Rect rc = ToRect(dis->rcItem);
                if (data->previewSelected) {
                    HdcFillRect(dis->hDC, rc, (HBRUSH)(COLOR_HIGHLIGHT + 1));
                    rc.Inflate(-3, -3);
                }
                if (data->isCheckered) {
                    HdcPaintCheckerboard(dis->hDC, rc.x, rc.y, rc.dx, rc.dy);
                } else {
                    HBRUSH br = CreateSolidBrush(data->currentColor);
                    HdcFillRect(dis->hDC, rc, br);
                    DeleteObject(br);
                }
                return TRUE;
            }
            // preset color buttons
            if (ctlId >= IDC_BGCOL_PRESET_FIRST && ctlId < IDC_BGCOL_PRESET_FIRST + kNumPresets) {
                int idx = ctlId - IDC_BGCOL_PRESET_FIRST;
                Color col = kBgPresetColors[idx];
                if (col == kColorUnset) {
                    HdcPaintCheckerboard(dis->hDC, dis->rcItem.left, dis->rcItem.top,
                                         dis->rcItem.right - dis->rcItem.left, dis->rcItem.bottom - dis->rcItem.top);
                } else {
                    HBRUSH br = CreateSolidBrush(col);
                    HdcFillRect(dis->hDC, ToRect(dis->rcItem), br);
                    DeleteObject(br);
                }
                // draw focus rect if focused
                if (dis->itemState & ODS_FOCUS) {
                    DrawFocusRect(dis->hDC, &dis->rcItem);
                }
                return TRUE;
            }
            // custom color buttons
            if (ctlId >= IDC_BGCOL_CUSTOM_FIRST && ctlId < IDC_BGCOL_CUSTOM_FIRST + kMaxCustomColors) {
                int idx = ctlId - IDC_BGCOL_CUSTOM_FIRST;
                Rect rc = ToRect(dis->rcItem);
                bool isSelected = (idx == data->selectedCustomIdx);
                if (isSelected) {
                    // draw selection outline: fill background, then inset for 2px gap
                    HdcFillRect(dis->hDC, rc, (HBRUSH)(COLOR_HIGHLIGHT + 1));
                    rc.Inflate(-3, -3);
                }
                if (data->customColorSet[idx]) {
                    HBRUSH br = CreateSolidBrush(data->customColors[idx]);
                    HdcFillRect(dis->hDC, rc, br);
                    DeleteObject(br);
                } else {
                    // empty slot: window background with accent border and diagonal X
                    HdcFillRect(dis->hDC, rc, (HBRUSH)(COLOR_WINDOW + 1));
                    HPEN pen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
                    HPEN oldPen = (HPEN)SelectObject(dis->hDC, pen);
                    int right = rc.x + rc.dx;
                    int bottom = rc.y + rc.dy;
                    // border
                    MoveToEx(dis->hDC, rc.x, rc.y, nullptr);
                    LineTo(dis->hDC, right - 1, rc.y);
                    LineTo(dis->hDC, right - 1, bottom - 1);
                    LineTo(dis->hDC, rc.x, bottom - 1);
                    LineTo(dis->hDC, rc.x, rc.y);
                    // diagonal lines
                    MoveToEx(dis->hDC, rc.x, rc.y, nullptr);
                    LineTo(dis->hDC, right - 1, bottom - 1);
                    MoveToEx(dis->hDC, right - 1, rc.y, nullptr);
                    LineTo(dis->hDC, rc.x, bottom - 1);
                    SelectObject(dis->hDC, oldPen);
                    DeleteObject(pen);
                }
                return TRUE;
            }
            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDOK:
                    TryParseBgColorEdit(hDlg, data);
                    data->applyToAll = IsDlgButtonChecked(hDlg, IDC_BGCOL_ALL_FILES) == BST_CHECKED;
                    if (data->customColorsChanged) {
                        SaveCustomColors(data);
                    }
                    EndDialog(hDlg, IDOK);
                    return TRUE;
                case IDCANCEL:
                    if (data->customColorsChanged) {
                        SaveCustomColors(data);
                    }
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
                case IDC_BGCOL_EDIT:
                    if (HIWORD(wp) == EN_CHANGE) {
                        if (TryParseBgColorEdit(hDlg, data)) {
                            // update selected button color
                            if (data->selectedCustomIdx >= 0 && !data->isCheckered) {
                                data->customColors[data->selectedCustomIdx] = data->currentColor;
                                data->customColorSet[data->selectedCustomIdx] = true;
                                data->customColorsChanged = true;
                            }
                            InvalidatePreview(hDlg, data);
                        }
                    }
                    break;
                case IDC_BGCOL_PREVIEW:
                    SelectPreviewButton(hDlg, data);
                    break;
                default: {
                    int id = LOWORD(wp);
                    // preset buttons: select preview button
                    if (id >= IDC_BGCOL_PRESET_FIRST && id < IDC_BGCOL_PRESET_FIRST + kNumPresets) {
                        int idx = id - IDC_BGCOL_PRESET_FIRST;
                        Color col = kBgPresetColors[idx];
                        if (col == kColorUnset) {
                            data->isCheckered = true;
                        } else {
                            data->isCheckered = false;
                            data->currentColor = col;
                        }
                        SelectPreviewButton(hDlg, data);
                        UpdateBgColorEditFromColor(hDlg, data);
                    }
                    // custom color buttons: select this button
                    if (id >= IDC_BGCOL_CUSTOM_FIRST && id < IDC_BGCOL_CUSTOM_FIRST + kMaxCustomColors) {
                        int idx = id - IDC_BGCOL_CUSTOM_FIRST;
                        if (data->selectedCustomIdx == idx) {
                            // clicking selected button deselects it
                            SelectPreviewButton(hDlg, data);
                        } else {
                            SelectCustomButton(hDlg, data, idx);
                            // load the button's color as current selection
                            if (data->customColorSet[idx]) {
                                data->isCheckered = false;
                                data->currentColor = data->customColors[idx];
                                UpdateBgColorEditFromColor(hDlg, data);
                            }
                        }
                    }
                } break;
            }
            break;

        case WM_CONTEXTMENU: {
            HWND hwndClicked = (HWND)wp;
            int ctlId = GetDlgCtrlID(hwndClicked);
            if (ctlId >= IDC_BGCOL_CUSTOM_FIRST && ctlId < IDC_BGCOL_CUSTOM_FIRST + kMaxCustomColors) {
                int idx = ctlId - IDC_BGCOL_CUSTOM_FIRST;
                if (data->customColorSet[idx]) {
                    data->customColorSet[idx] = false;
                    data->customColorsChanged = true;
                    if (data->selectedCustomIdx == idx) {
                        SelectPreviewButton(hDlg, data);
                    }
                    HwndInvalidate(hwndClicked, true);
                }
                return TRUE;
            }
            break;
        }
    }
    return FALSE;
}

bool Dialog_ChangeBackgroundColor(HWND hwnd, Color currentColor, bool isCheckered, Str allFilesLabel,
                                  BgColorResult& result) {
    BgColorDlgData data;
    data.currentColor = currentColor;
    data.isCheckered = isCheckered;
    data.applyToAll = false;
    data.selectedCustomIdx = -1;
    data.previewSelected = true;
    data.showRadioButtons = true;
    data.allFilesLabel = allFilesLabel;

    INT_PTR res = CreateDialogBox(IDD_DIALOG_CHANGE_BG_COLOR, hwnd, Dialog_ChangeBgColor_Proc, (LPARAM)&data);
    if (res != IDOK) {
        return false;
    }

    result.color = data.currentColor;
    result.isCheckered = data.isCheckered;
    result.applyToAllFiles = data.applyToAll;
    return true;
}

bool Dialog_SetTabColor(HWND hwnd, Color currentColor, bool isUnset, Color& resultColor, bool& resultIsUnset) {
    BgColorDlgData data;
    data.currentColor = currentColor;
    data.isCheckered = isUnset;
    data.applyToAll = false;
    data.selectedCustomIdx = -1;
    data.previewSelected = true;
    data.title = _TRA("Change Tab Color");
    data.showRadioButtons = false;

    INT_PTR res = CreateDialogBox(IDD_DIALOG_CHANGE_BG_COLOR, hwnd, Dialog_ChangeBgColor_Proc, (LPARAM)&data);
    if (res != IDOK) {
        return false;
    }

    resultColor = data.currentColor;
    resultIsUnset = data.isCheckered;
    return true;
}
