/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by KeyboardHelpCommon.cpp and each app's KeyboardHelp.cpp ---

// A section is an ordered list of command ids (terminated by 0). The keyboard
// shortcut for each command is looked up from its actual binding, not hard-coded
// here, so re-binding or clearing a shortcut is reflected automatically.
// clang-format off
static const int kSecNav[] = {
    CmdScrollUp, CmdScrollDown, CmdScrollLeft, CmdScrollRight,
    CmdScrollUpPage, CmdScrollDownPage,
    CmdGoToNextPage, CmdGoToPrevPage,
    CmdGoToFirstPage, CmdGoToLastPage, CmdGoToPage,
    CmdNavigateBack, CmdNavigateForward, 0,
};

static const int kSecView[] = {
    CmdZoomIn, CmdZoomOut,
    CmdZoomFitPage, CmdZoomFitWidth, CmdZoomActualSize,
    CmdToggleZoom, CmdSinglePageView, CmdFacingView,
    CmdBookView, CmdToggleContinuousView,
    CmdRotateLeft, CmdRotateRight, CmdToggleFullscreen, 0,
};

static const int kSecDoc[] = {
    CmdOpenFile, CmdSaveAs, CmdPrint, CmdReloadDocument,
    CmdClose, CmdNewWindow, CmdOpenNextFileInFolder,
    CmdOpenPrevFileInFolder, CmdRenameFile, CmdProperties, 0,
};

static const int kSecFind[] = {
    CmdFindFirst, CmdFindNext, CmdFindPrev,
    CmdSelectAll, CmdCopySelection, CmdSelectTextViaKeyboard,
    CmdToggleKeyboardLinkFollowing, 0,
};

static const int kSecTabs[] = {
    CmdNextTabSmart, CmdNextTab, CmdPrevTab,
    CmdMoveTabLeft, CmdMoveTabRight, CmdReopenLastClosedFile, 0,
};

static const int kSecAnnot[] = {
    CmdCreateAnnotHighlight, CmdCreateAnnotUnderline, CmdSaveAnnotations,
    CmdDeleteAnnotation, 0,
};

static const int kSecIface[] = {
    CmdCommandPalette, CmdToggleBookmarks, CmdToggleToolbar, CmdToggleCaption, CmdToggleMenuBar,
    CmdToggleCursorPosition, CmdTogglePageInfo,
    CmdFavoriteAdd, CmdFavoriteToggle, CmdHelpOpenManual,0,
};
// clang-format on

struct KbSectionDef {
    const char* title;
    const int* commands;
    int column; // which of the two columns this section is laid out in
};
// column 0 (left): Navigation, Interface, Find & Select
// column 1 (right): View & Zoom, Document, Tabs, Annotations
static const KbSectionDef kSections[] = {
    {"Navigation", kSecNav, 0},    {"View & Zoom", kSecView, 1},   {"Interface", kSecIface, 0},
    {"Document", kSecDoc, 1},      {"Find & Select", kSecFind, 0}, {"Tabs", kSecTabs, 1},
    {"Annotations", kSecAnnot, 1},
};
bool IsHelpListedCmd(int cmdId);
// fallback shortcuts for platforms without an accelerator table (the Windows
// data source looks up the real bindings instead). {id, ""} means "no default".
// clang-format off
static const struct {
    int id;
    const char* shortcut;
} kFallbackShortcuts[] = {
    {CmdScrollUp, "Up, K"}, {CmdScrollDown, "Down, J"}, {CmdScrollLeft, "Left, H"}, {CmdScrollRight, "Right, L"},
    {CmdScrollUpPage, "Page Up"}, {CmdScrollDownPage, "Page Down"}, {CmdGoToNextPage, "N"}, {CmdGoToPrevPage, "P"},
    {CmdGoToFirstPage, "Home"}, {CmdGoToLastPage, "End"}, {CmdGoToPage, "Ctrl + G"}, {CmdNavigateBack, "Alt + Left"},
    {CmdNavigateForward, "Alt + Right"}, {CmdZoomIn, "Ctrl + +"}, {CmdZoomOut, "Ctrl + -"}, {CmdZoomFitPage, "Ctrl + 0"},
    {CmdZoomFitWidth, "Ctrl + 2"}, {CmdZoomActualSize, "Ctrl + 1"}, {CmdToggleZoom, "Z"}, {CmdSinglePageView, "Ctrl + 6"},
    {CmdFacingView, "Ctrl + 7"}, {CmdBookView, "Ctrl + 8"}, {CmdToggleContinuousView, "C"}, {CmdRotateLeft, "["},
    {CmdRotateRight, "]"}, {CmdToggleFullscreen, "F"}, {CmdToggleAutomaticallyScroll, "Ctrl + Shift + H"},
    {CmdToggleReadingBar, ""}, {CmdToggleReadingBarInvert, ""},
    {CmdOpenFile, "Ctrl + O"}, {CmdSaveAs, "Ctrl + S"},
    {CmdPrint, "Ctrl + P"}, {CmdReloadDocument, "R"}, {CmdClose, "Ctrl + W"}, {CmdNewWindow, "Ctrl + N"},
    {CmdOpenNextFileInFolder, "Ctrl + Shift + Right"}, {CmdOpenPrevFileInFolder, "Ctrl + Shift + Left"},
    {CmdRenameFile, "F2"}, {CmdProperties, "Ctrl + D"}, {CmdFindFirst, "Ctrl + F"}, {CmdFindNext, "F3"},
    {CmdFindPrev, "Shift + F3"}, {CmdSelectAll, "Ctrl + A"}, {CmdCopySelection, "Ctrl + C"},
    {CmdSelectTextViaKeyboard, "F7"}, {CmdToggleKeyboardLinkFollowing, "Shift + F"}, {CmdNextTabSmart, "Ctrl + Tab"},
    {CmdNextTab, "Ctrl + Page Down"}, {CmdPrevTab, "Ctrl + Page Up"}, {CmdMoveTabLeft, "Ctrl + Shift + Page Up"},
    {CmdMoveTabRight, "Ctrl + Shift + Page Down"}, {CmdReopenLastClosedFile, "Ctrl + Shift + T"},
    {CmdCreateAnnotHighlight, "A"}, {CmdCreateAnnotUnderline, "U"}, {CmdSaveAnnotations, "Ctrl + Shift + S"},
    {CmdDeleteAnnotation, "Ctrl + Delete"}, {CmdToggleBookmarks, "F12"}, {CmdToggleToolbar, "F8"},
    {CmdToggleCaption, "Shift + F8"},
    {CmdToggleMenuBar, "F9"}, {CmdToggleCursorPosition, "M"}, {CmdTogglePageInfo, "I"}, {CmdCommandPalette, "Ctrl + K"},
    {CmdFavoriteAdd, "Ctrl + B"}, {CmdHelpOpenManual, "F1"}, {CmdToggleKeyboardHelp, "?"},
};
// clang-format on

struct SumatraKeyboardHelpDataSource : KeyboardHelpDataSource {
    Str Translate(Str s) override { return trans::GetTranslation(s); }

    TempStr CommandDescriptionTemp(int cmdId) override {
        Str description = GetCommandDescription(cmdId);
        if (len(description) == 0) {
            return {};
        }
        return str::DupTemp(trans::GetTranslation(description));
    }

    TempStr CommandShortcutTemp(int cmdId, int maxCount) override { return ShortcutsForCmdTemp(cmdId, maxCount); }
};
