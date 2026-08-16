/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// The colors of the gui/ controls, independent of any app's theming.
//
// Every control class has:
//  - an enum naming its colors, used as indices into an array of Color
//  - a global array of defaults for the class (gCols*), which is what every
//    instance of it paints with unless it was given something else
//  - an optional per-instance array of overrides (VirtCtrl::colors), where
//    kColorUnset in a slot means "use the class default"
//
// GetCol() resolves the two; VirtCtrl::GetColor(idx) is the wrapper that fills
// in both arrays for a control, so painting code reads GetColor(kColBtnBg).
//
// The defaults start out as the colors the OS draws its own UI in
// (GuiSetDefaultColorsFromSystem()). An app with a theme of its own overwrites
// them after a theme change - SumatraPDF does that in SumatraUpdateTheme().
// That is the whole of the app -> gui color coupling: gui/ never asks the app
// for a color, the app pushes its palette into these arrays.
//
// A class that derives from another (VirtButton from VirtText) must keep the
// base's slots at the same indices, because the base's painting code uses them.

Color GetCol(const Color* defaults, const Color* overrides, int idx);

//--- VirtText
enum {
    kColText,
    kColTextCount,
};
extern Color gColsText[kColTextCount];

//--- VirtLink: same slots as VirtText, its own defaults
extern Color gColsLink[kColTextCount];

//--- VirtButton
enum {
    kColBtnText = kColText,
    kColBtnBg,
    kColBtnBgHover,
    kColBtnBorder,
    kColBtnTextDisabled,
    kColBtnCount,
};
extern Color gColsBtn[kColBtnCount];
// a default button ("OK") is a shade stronger, like a native one
extern Color gColsBtnDefault[kColBtnCount];

//--- VirtIconButton
enum {
    kColIconBtnBgHover,
    kColIconBtnBgSelected,
    kColIconBtnChevron,
    kColIconBtnCount,
};
extern Color gColsIconBtn[kColIconBtnCount];

//--- VirtCloseButton
enum {
    kColCloseX,
    kColCloseXHover,
    kColCloseCircle,
    kColCloseCircleHover,
    kColCloseCount,
};
extern Color gColsCloseBtn[kColCloseCount];

//--- VirtListBox
enum {
    kColListText,
    kColListBg,
    // background of the selected row. Derived from kColListBg when unset, and
    // stronger while the list has the keyboard focus, like a win32 listbox
    kColListSel,
    kColListSelFocused,
    // the overlay scrollbar's thumb; derived from kColListBg when unset
    kColListScrollbar,
    kColListCount,
};
extern Color gColsListBox[kColListCount];

//--- VirtSplitter
enum {
    kColSplitterBg,
    kColSplitterCount,
};
extern Color gColsSplitter[kColSplitterCount];

//--- VirtFill
enum {
    kColFillBg,
    kColFillCount,
};
extern Color gColsFill[kColFillCount];

//--- VirtLine
enum {
    kColLineFg,
    kColLineCount,
};
extern Color gColsLine[kColLineCount];

//--- VirtRichText
enum {
    kColRichText,
    kColRichLink,
    // the color the text is painted on; the key-caps are derived from it
    kColRichBg,
    kColRichCount,
};
extern Color gColsRichText[kColRichCount];

//--- TabsCtrl / TabCtrl
enum {
    kColTabText,
    // the selected tab and the strip behind the tabs; the unselected and
    // hovered shades are derived from it
    kColTabBg,
    kColTabCount,
};
extern Color gColsTab[kColTabCount];

//--- win32 WindowBase
// The per-instance overrides are HwndBase::textColor / bgColor (two plain
// fields, not an owned array); WindowBase::GetColor(idx) resolves them
// against these defaults
enum {
    kColWinText,
    kColWinBg,
    kColWinCount,
};
extern Color gColsWin[kColWinCount];

//--- win32 Edit
enum {
    // the 1px underline an Edit created withBottomBorder draws. It is a
    // separator, so it takes the same color as every other border and divider
    kColEditBottomBorder,
    kColEditCount,
};
extern Color gColsEdit[kColEditCount];

// true while the defaults are the ones the OS draws its own UI in. An app that
// pushes a palette of its own into them clears it, so the default
// WindowBase::OnThemeChange() doesn't overwrite the app's colors with the
// system's the next time the system palette changes
extern bool gGuiColorsFromSystem;

void GuiSetDefaultColorsFromSystem();
void GuiColorsInitIfNeeded();
