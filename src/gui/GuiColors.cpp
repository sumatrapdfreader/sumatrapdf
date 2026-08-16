/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "gui/GuiColors.h"

Color gColsText[kColTextCount];
Color gColsLink[kColTextCount];
Color gColsBtn[kColBtnCount];
Color gColsBtnDefault[kColBtnCount];
Color gColsIconBtn[kColIconBtnCount];
Color gColsCloseBtn[kColCloseCount];
Color gColsListBox[kColListCount];
Color gColsSplitter[kColSplitterCount];
Color gColsFill[kColFillCount];
Color gColsLine[kColLineCount];
Color gColsRichText[kColRichCount];
Color gColsTab[kColTabCount];
Color gColsWin[kColWinCount];
Color gColsEdit[kColEditCount];

// An override wins unless its slot is kColorUnset, which means "I didn't say".
// kColorTransparent is a decision ("don't paint this") so it does not fall
// through to the default.
Color GetCol(const Color* defaults, const Color* overrides, int idx) {
    if (overrides) {
        Color c = overrides[idx];
        if (c != kColorUnset) {
            return c;
        }
    }
    if (!defaults) {
        return kColorUnset;
    }
    return defaults[idx];
}

// The look of the ✕ that closes a tab / panel / notification doesn't come from
// the system palette: a gray glyph that turns white on a red circle when
// hovered, the same in every theme.
constexpr Color kColCloseXDef = MkRgb(0xa0, 0xa0, 0xa0);
constexpr Color kColCloseXHoverDef = MkRgb(0xf9, 0xeb, 0xeb);      // white-ish
constexpr Color kColCloseCircleHoverDef = MkRgb(0xC1, 0x35, 0x35); // red-ish

static bool gDefaultColorsInited = false;

bool gGuiColorsFromSystem = true;

// Every window and control calls this on the way up, so an app that never
// themes anything (the installer, the log viewer) still gets real colors
// without having to know that gui/ has a palette at all.
void GuiColorsInitIfNeeded() {
    if (!gDefaultColorsInited) {
        GuiSetDefaultColorsFromSystem();
    }
}

// The platform UI colors spread over the per-control defaults. On POSIX we use
// a neutral light palette until the native app provides its own theme colors.
void GuiSetDefaultColorsFromSystem() {
    gDefaultColorsInited = true;
    gGuiColorsFromSystem = true;
#if OS_WIN
    Color winBg = GetSysColor(COLOR_WINDOW);
    Color winText = GetSysColor(COLOR_WINDOWTEXT);
    Color ctlBg = GetSysColor(COLOR_BTNFACE);
    Color ctlText = GetSysColor(COLOR_BTNTEXT);
    Color disabledText = GetSysColor(COLOR_GRAYTEXT);
    Color link = GetSysColor(COLOR_HOTLIGHT);
    Color edge = GetSysColor(COLOR_WINDOWFRAME);
#else
    Color winBg = MkGray(0xff);
    Color winText = MkGray(0x00);
    Color ctlBg = MkGray(0xf0);
    Color ctlText = MkGray(0x00);
    Color disabledText = MkGray(0x6d);
    Color link = MkRgb(0x00, 0x66, 0xcc);
    Color edge = MkGray(0xa0);
#endif

    gColsText[kColText] = winText;

    gColsLink[kColText] = link;

    gColsBtn[kColBtnText] = ctlText;
    gColsBtn[kColBtnBg] = AccentColor(ctlBg, 14);
    gColsBtn[kColBtnBgHover] = AccentColor(ctlBg, 28);
    gColsBtn[kColBtnBorder] = AccentColor(ctlBg, 40);
    gColsBtn[kColBtnTextDisabled] = disabledText;

    gColsBtnDefault[kColBtnText] = ctlText;
    gColsBtnDefault[kColBtnBg] = AccentColor(ctlBg, 26);
    gColsBtnDefault[kColBtnBgHover] = AccentColor(ctlBg, 40);
    gColsBtnDefault[kColBtnBorder] = AccentColor(ctlBg, 55);
    gColsBtnDefault[kColBtnTextDisabled] = disabledText;

    gColsIconBtn[kColIconBtnBgHover] = AccentColor(ctlBg, 20);
    gColsIconBtn[kColIconBtnBgSelected] = AccentColor(ctlBg, 36);
    gColsIconBtn[kColIconBtnChevron] = ctlText;

    gColsCloseBtn[kColCloseX] = kColCloseXDef;
    gColsCloseBtn[kColCloseXHover] = kColCloseXHoverDef;
    // an unhovered circle is only painted by a withCircle button, which sits on
    // content it doesn't own (a thumbnail), so it stays a plain light disc
    gColsCloseBtn[kColCloseCircle] = MkGray(0xff);
    gColsCloseBtn[kColCloseCircleHover] = kColCloseCircleHoverDef;

    gColsListBox[kColListText] = winText;
    gColsListBox[kColListBg] = winBg;
    gColsListBox[kColListSel] = AccentColor(winBg, 25);
    gColsListBox[kColListSelFocused] = AccentColor(winBg, 45);
    gColsListBox[kColListScrollbar] = AccentColor(winBg, 60);

    gColsSplitter[kColSplitterBg] = ctlBg;

    gColsFill[kColFillBg] = ctlBg;

    gColsLine[kColLineFg] = edge;

    gColsRichText[kColRichText] = winText;
    gColsRichText[kColRichLink] = link;
    gColsRichText[kColRichBg] = winBg;

    gColsTab[kColTabText] = winText;
    gColsTab[kColTabBg] = ctlBg;

    // a custom top-level window's own surface, like the OS paints a window's
    gColsWin[kColWinText] = winText;
    gColsWin[kColWinBg] = winBg;

    gColsEdit[kColEditBottomBorder] = edge;
}
