/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

#include "Commands.h"
#include "Settings.h"
#include "GlobalPrefs.h"
#include "DisplayMode.h"
#include "Notifications.h"

#include "utils/Log.h"

// @gen-start cmd-c
// clang-format off
static SeqStrings gCommandNames =
    "CmdOpenFile\0" "CmdClose\0" "CmdCloseCurrentDocument\0"
    "CmdCloseOtherTabs\0" "CmdCloseTabsToTheRight\0" "CmdCloseTabsToTheLeft\0"
    "CmdCloseAllTabs\0" "CmdSaveAs\0" "CmdPrint\0"
    "CmdShowInFolder\0" "CmdRenameFile\0" "CmdDeleteFile\0"
    "CmdExit\0" "CmdReloadDocument\0" "CmdCreateShortcutToFile\0"
    "CmdSendByEmail\0" "CmdProperties\0" "CmdSinglePageView\0"
    "CmdFacingView\0" "CmdBookView\0" "CmdToggleContinuousView\0"
    "CmdToggleMangaMode\0" "CmdRotateLeft\0" "CmdRotateRight\0"
    "CmdToggleBookmarks\0" "CmdToggleTableOfContents\0" "CmdToggleFullscreen\0"
    "CmdPresentationWhiteBackground\0" "CmdPresentationBlackBackground\0" "CmdTogglePresentationMode\0"
    "CmdToggleToolbar\0" "CmdToggleScrollbars\0" "CmdToggleOverlayScrollbar\0"
    "CmdToggleMenuBar\0" "CmdToggleUseTabs\0" "CmdCopySelection\0"
    "CmdTranslateSelectionWithGoogle\0" "CmdTranslateSelectionWithDeepL\0" "CmdSearchSelectionWithGoogle\0"
    "CmdSearchSelectionWithBing\0" "CmdSearchSelectionWithWikipedia\0" "CmdSearchSelectionWithGoogleScholar\0"
    "CmdSelectAll\0" "CmdNewWindow\0" "CmdDuplicateInNewWindow\0"
    "CmdDuplicateInNewTab\0" "CmdCopyImage\0" "CmdCopyLinkTarget\0"
    "CmdCopyComment\0" "CmdCopyFilePath\0" "CmdScrollUp\0"
    "CmdScrollDown\0" "CmdScrollLeft\0" "CmdScrollRight\0"
    "CmdScrollLeftPage\0" "CmdScrollRightPage\0" "CmdScrollUpPage\0"
    "CmdScrollDownPage\0" "CmdScrollDownHalfPage\0" "CmdScrollUpHalfPage\0"
    "CmdGoToNextPage\0" "CmdGoToPrevPage\0" "CmdGoToFirstPage\0"
    "CmdGoToLastPage\0" "CmdGoToPage\0" "CmdFindFirst\0"
    "CmdFindNext\0" "CmdFindPrev\0" "CmdFindNextSel\0"
    "CmdFindPrevSel\0" "CmdFindToggleMatchCase\0" "CmdSaveAnnotations\0"
    "CmdSaveAnnotationsNewFile\0" "CmdEditAnnotations\0" "CmdDeleteAnnotation\0"
    "CmdZoomFitPage\0" "CmdZoomActualSize\0" "CmdZoomFitWidth\0"
    "CmdZoom6400\0" "CmdZoom3200\0" "CmdZoom1600\0"
    "CmdZoom800\0" "CmdZoom400\0" "CmdZoom200\0"
    "CmdZoom150\0" "CmdZoom125\0" "CmdZoom100\0"
    "CmdZoom50\0" "CmdZoom25\0" "CmdZoom12_5\0"
    "CmdZoom8_33\0" "CmdZoomFitContent\0" "CmdZoomCustom\0"
    "CmdZoomIn\0" "CmdZoomOut\0" "CmdZoomFitWidthAndContinuous\0"
    "CmdZoomFitPageAndSinglePage\0" "CmdContributeTranslation\0" "CmdOpenWithKnownExternalViewerFirst\0"
    "CmdOpenWithExplorer\0" "CmdOpenWithDirectoryOpus\0" "CmdOpenWithTotalCommander\0"
    "CmdOpenWithDoubleCommander\0" "CmdOpenWithAcrobat\0" "CmdOpenWithFoxIt\0"
    "CmdOpenWithFoxItPhantom\0" "CmdOpenWithPdfXchange\0" "CmdOpenWithXpsViewer\0"
    "CmdOpenWithHtmlHelp\0" "CmdOpenWithPdfDjvuBookmarker\0" "CmdOpenWithKnownExternalViewerLast\0"
    "CmdOpenSelectedDocument\0" "CmdPinSelectedDocument\0" "CmdForgetSelectedDocument\0"
    "CmdExpandAll\0" "CmdCollapseAll\0" "CmdSaveEmbeddedFile\0"
    "CmdOpenEmbeddedPDF\0" "CmdSaveAttachment\0" "CmdOpenAttachment\0"
    "CmdOptions\0" "CmdAdvancedOptions\0" "CmdAdvancedSettings\0"
    "CmdChangeLanguage\0" "CmdCheckUpdate\0" "CmdHelpOpenManual\0"
    "CmdHelpOpenManualOnWebsite\0" "CmdHelpOpenKeyboardShortcuts\0" "CmdHelpVisitWebsite\0"
    "CmdHelpAbout\0" "CmdMoveFrameFocus\0" "CmdFavoriteAdd\0"
    "CmdFavoriteDel\0" "CmdFavoriteToggle\0" "CmdToggleLinks\0"
    "CmdToggleShowAnnotations\0" "CmdShowAnnotations\0" "CmdHideAnnotations\0"
    "CmdCreateAnnotText\0" "CmdCreateAnnotLink\0" "CmdCreateAnnotFreeText\0"
    "CmdCreateAnnotLine\0" "CmdCreateAnnotSquare\0" "CmdCreateAnnotCircle\0"
    "CmdCreateAnnotPolygon\0" "CmdCreateAnnotPolyLine\0" "CmdCreateAnnotHighlight\0"
    "CmdCreateAnnotUnderline\0" "CmdCreateAnnotSquiggly\0" "CmdCreateAnnotStrikeOut\0"
    "CmdCreateAnnotRedact\0" "CmdCreateAnnotStamp\0" "CmdCreateAnnotCaret\0"
    "CmdCreateAnnotInk\0" "CmdCreateAnnotPopup\0" "CmdCreateAnnotFileAttachment\0"
    "CmdInvertColors\0" "CmdTogglePageInfo\0" "CmdToggleZoom\0"
    "CmdNavigateBack\0" "CmdNavigateForward\0" "CmdToggleCursorPosition\0"
    "CmdOpenNextFileInFolder\0" "CmdOpenPrevFileInFolder\0" "CmdCommandPalette\0"
    "CmdShowLog\0" "CmdShowPdfInfo\0" "CmdShowErrors\0"
    "CmdClearHistory\0" "CmdReopenLastClosedFile\0" "CmdNextTab\0"
    "CmdPrevTab\0" "CmdNextTabSmart\0" "CmdPrevTabSmart\0"
    "CmdMoveTabLeft\0" "CmdMoveTabRight\0" "CmdSelectNextTheme\0"
    "CmdToggleFrequentlyRead\0" "CmdInvokeInverseSearch\0" "CmdExec\0"
    "CmdViewWithExternalViewer\0" "CmdSelectionHandler\0" "CmdSetTheme\0"
    "CmdToggleInverseSearch\0" "CmdDebugCorruptMemory\0" "CmdDebugCrashMe\0"
    "CmdDebugDownloadSymbols\0" "CmdDebugTestApp\0" "CmdDebugShowNotif\0"
    "CmdDebugStartStressTest\0" "CmdDebugTogglePredictiveRender\0" "CmdDebugToggleRtl\0"
    "CmdToggleAntiAlias\0" "CmdToggleSmoothScroll\0" "CmdToggleHideScrollbar\0"
    "CmdToggleScrollbarInSinglePage\0" "CmdToggleLazyLoading\0" "CmdListPrinters\0"
    "CmdToggleWindowsPreviewer\0" "CmdToggleWindowsSearchFilter\0" "CmdScreenshot\0"
    "CmdCropImage\0" "CmdResizeImage\0" "CmdSaveImage\0"
    "CmdPasteClipboardImage\0" "CmdNone\0" "\0";

static i32 gCommandIds[] = {
    CmdOpenFile, CmdClose, CmdCloseCurrentDocument,
    CmdCloseOtherTabs, CmdCloseTabsToTheRight, CmdCloseTabsToTheLeft,
    CmdCloseAllTabs, CmdSaveAs, CmdPrint,
    CmdShowInFolder, CmdRenameFile, CmdDeleteFile,
    CmdExit, CmdReloadDocument, CmdCreateShortcutToFile,
    CmdSendByEmail, CmdProperties, CmdSinglePageView,
    CmdFacingView, CmdBookView, CmdToggleContinuousView,
    CmdToggleMangaMode, CmdRotateLeft, CmdRotateRight,
    CmdToggleBookmarks, CmdToggleTableOfContents, CmdToggleFullscreen,
    CmdPresentationWhiteBackground, CmdPresentationBlackBackground, CmdTogglePresentationMode,
    CmdToggleToolbar, CmdToggleScrollbars, CmdToggleOverlayScrollbar,
    CmdToggleMenuBar, CmdToggleUseTabs, CmdCopySelection,
    CmdTranslateSelectionWithGoogle, CmdTranslateSelectionWithDeepL, CmdSearchSelectionWithGoogle,
    CmdSearchSelectionWithBing, CmdSearchSelectionWithWikipedia, CmdSearchSelectionWithGoogleScholar,
    CmdSelectAll, CmdNewWindow, CmdDuplicateInNewWindow,
    CmdDuplicateInNewTab, CmdCopyImage, CmdCopyLinkTarget,
    CmdCopyComment, CmdCopyFilePath, CmdScrollUp,
    CmdScrollDown, CmdScrollLeft, CmdScrollRight,
    CmdScrollLeftPage, CmdScrollRightPage, CmdScrollUpPage,
    CmdScrollDownPage, CmdScrollDownHalfPage, CmdScrollUpHalfPage,
    CmdGoToNextPage, CmdGoToPrevPage, CmdGoToFirstPage,
    CmdGoToLastPage, CmdGoToPage, CmdFindFirst,
    CmdFindNext, CmdFindPrev, CmdFindNextSel,
    CmdFindPrevSel, CmdFindToggleMatchCase, CmdSaveAnnotations,
    CmdSaveAnnotationsNewFile, CmdEditAnnotations, CmdDeleteAnnotation,
    CmdZoomFitPage, CmdZoomActualSize, CmdZoomFitWidth,
    CmdZoom6400, CmdZoom3200, CmdZoom1600,
    CmdZoom800, CmdZoom400, CmdZoom200,
    CmdZoom150, CmdZoom125, CmdZoom100,
    CmdZoom50, CmdZoom25, CmdZoom12_5,
    CmdZoom8_33, CmdZoomFitContent, CmdZoomCustom,
    CmdZoomIn, CmdZoomOut, CmdZoomFitWidthAndContinuous,
    CmdZoomFitPageAndSinglePage, CmdContributeTranslation, CmdOpenWithKnownExternalViewerFirst,
    CmdOpenWithExplorer, CmdOpenWithDirectoryOpus, CmdOpenWithTotalCommander,
    CmdOpenWithDoubleCommander, CmdOpenWithAcrobat, CmdOpenWithFoxIt,
    CmdOpenWithFoxItPhantom, CmdOpenWithPdfXchange, CmdOpenWithXpsViewer,
    CmdOpenWithHtmlHelp, CmdOpenWithPdfDjvuBookmarker, CmdOpenWithKnownExternalViewerLast,
    CmdOpenSelectedDocument, CmdPinSelectedDocument, CmdForgetSelectedDocument,
    CmdExpandAll, CmdCollapseAll, CmdSaveEmbeddedFile,
    CmdOpenEmbeddedPDF, CmdSaveAttachment, CmdOpenAttachment,
    CmdOptions, CmdAdvancedOptions, CmdAdvancedSettings,
    CmdChangeLanguage, CmdCheckUpdate, CmdHelpOpenManual,
    CmdHelpOpenManualOnWebsite, CmdHelpOpenKeyboardShortcuts, CmdHelpVisitWebsite,
    CmdHelpAbout, CmdMoveFrameFocus, CmdFavoriteAdd,
    CmdFavoriteDel, CmdFavoriteToggle, CmdToggleLinks,
    CmdToggleShowAnnotations, CmdShowAnnotations, CmdHideAnnotations,
    CmdCreateAnnotText, CmdCreateAnnotLink, CmdCreateAnnotFreeText,
    CmdCreateAnnotLine, CmdCreateAnnotSquare, CmdCreateAnnotCircle,
    CmdCreateAnnotPolygon, CmdCreateAnnotPolyLine, CmdCreateAnnotHighlight,
    CmdCreateAnnotUnderline, CmdCreateAnnotSquiggly, CmdCreateAnnotStrikeOut,
    CmdCreateAnnotRedact, CmdCreateAnnotStamp, CmdCreateAnnotCaret,
    CmdCreateAnnotInk, CmdCreateAnnotPopup, CmdCreateAnnotFileAttachment,
    CmdInvertColors, CmdTogglePageInfo, CmdToggleZoom,
    CmdNavigateBack, CmdNavigateForward, CmdToggleCursorPosition,
    CmdOpenNextFileInFolder, CmdOpenPrevFileInFolder, CmdCommandPalette,
    CmdShowLog, CmdShowPdfInfo, CmdShowErrors,
    CmdClearHistory, CmdReopenLastClosedFile, CmdNextTab,
    CmdPrevTab, CmdNextTabSmart, CmdPrevTabSmart,
    CmdMoveTabLeft, CmdMoveTabRight, CmdSelectNextTheme,
    CmdToggleFrequentlyRead, CmdInvokeInverseSearch, CmdExec,
    CmdViewWithExternalViewer, CmdSelectionHandler, CmdSetTheme,
    CmdToggleInverseSearch, CmdDebugCorruptMemory, CmdDebugCrashMe,
    CmdDebugDownloadSymbols, CmdDebugTestApp, CmdDebugShowNotif,
    CmdDebugStartStressTest, CmdDebugTogglePredictiveRender, CmdDebugToggleRtl,
    CmdToggleAntiAlias, CmdToggleSmoothScroll, CmdToggleHideScrollbar,
    CmdToggleScrollbarInSinglePage, CmdToggleLazyLoading, CmdListPrinters,
    CmdToggleWindowsPreviewer, CmdToggleWindowsSearchFilter, CmdScreenshot,
    CmdCropImage, CmdResizeImage, CmdSaveImage,
    CmdPasteClipboardImage, CmdNone,
};

SeqStrings gCommandDescriptions =
    "Open File...\0" "Close Document\0" "Close Current Document\0"
    "Close Other Tabs\0" "Close Tabs To The Right\0" "Close Tabs To The Left\0"
    "Close All Tabs\0" "Save File As...\0" "Print Document...\0"
    "Show File In Folder...\0" "Rename File...\0" "Delete File\0"
    "Exit Application\0" "Reload Document\0" "Create .lnk Shortcut\0"
    "Send Document By Email...\0" "Show Document Properties...\0" "Single Page View\0"
    "Facing View\0" "Book View\0" "Toggle Continuous View\0"
    "Toggle Manga Mode\0" "Rotate Left\0" "Rotate Right\0"
    "Toggle Bookmarks\0" "Toggle Table Of Contents\0" "Toggle Fullscreen\0"
    "Presentation White Background\0" "Presentation Black Background\0" "View: Presentation Mode\0"
    "Toggle Toolbar\0" "Toggle Scrollbars\0" "Toggle Overlay Scrollbar\0"
    "Toggle Menu Bar\0" "Toggle Use Tabs\0" "Copy Selection\0"
    "Translate Selection with Google\0" "Translate Selection With DeepL\0" "Search Selection with Google\0"
    "Search Selection with Bing\0" "Search Selection with Wikipedia\0" "Search Selection with Google Scholar\0"
    "Select All\0" "Open New SumatraPDF Window\0" "Open Current Document In New Window\0"
    "Open Current Document In New Tab\0" "Copy Image\0" "Copy Link Target\0"
    "Copy Comment\0" "Copy File Path\0" "Scroll Up\0"
    "Scroll Down\0" "Scroll Left\0" "Scroll Right\0"
    "Scroll Left By Page\0" "Scroll Right By Page\0" "Scroll Up By Page\0"
    "Scroll Down By Page\0" "Scroll Down By Half Page\0" "Scroll Up By Half Page\0"
    "Next Page\0" "Previous Page\0" "First Page\0"
    "Last Page\0" "Go to Page...\0" "Find\0"
    "Find Next\0" "Find Previous\0" "Find Next Selection\0"
    "Find Previous Selection\0" "Find: Toggle Match Case\0" "Save Annotations to existing PDF\0"
    "Save Annotations to a new PDF\0" "Edit Annotations\0" "Delete Annotation\0"
    "Zoom: Fit Page\0" "Zoom: Actual Size\0" "Zoom: Fit Width\0"
    "Zoom: 6400%\0" "Zoom: 3200%\0" "Zoom: 1600%\0"
    "Zoom: 800%\0" "Zoom: 400%\0" "Zoom: 200%\0"
    "Zoom: 150%\0" "Zoom: 125%\0" "Zoom: 100%\0"
    "Zoom: 50%\0" "Zoom: 25%\0" "Zoom: 12.5%\0"
    "Zoom: 8.33%\0" "Zoom: Fit Content\0" "Zoom: Custom...\0"
    "Zoom In\0" "Zoom Out\0" "Zoom: Fit Width And Continuous\0"
    "Zoom: Fit Page and Single Page\0" "Contribute Translation\0" "don't use\0"
    "Open Directory In Explorer\0" "Open Directory In Directory Opus\0" "Open Directory In Total Commander\0"
    "Open Directory In Double Commander\0" "Open in Adobe Acrobat\0" "Open in Foxit Reader\0"
    "Open in Foxit PhantomPDF\0" "Open in PDF-XChange\0" "Open in Microsoft Xps Viewer\0"
    "Open in Microsoft HTML Help\0" "Open With Pdf&Djvu Bookmarker\0" "don't use\0"
    "Open Selected Document\0" "Pin Selected Document\0" "Remove Selected Document From History\0"
    "Expand All\0" "Collapse All\0" "Save Embedded File...\0"
    "Open Embedded PDF\0" "Save Attachment...\0" "Open Attachment\0"
    "Options...\0" "Advanced Options...\0" "Advanced Settings...\0"
    "Change Language...\0" "Check For Updates\0" "Help: Manual\0"
    "Help: Manual On Website\0" "Help: Keyboard Shortcuts\0" "Help: SumatraPDF Website\0"
    "Help: About SumatraPDF\0" "Move Frame Focus\0" "Add Favorite\0"
    "Delete Favorite\0" "Toggle Favorites\0" "Toggle Show Links\0"
    "Toggle Show Annotations\0" "Show Annotations\0" "Hide Annotations\0"
    "Create Text Annotation\0" "Create Link Annotation\0" "Create Free Text Annotation\0"
    "Create Line Annotation\0" "Create Square Annotation\0" "Create Circle Annotation\0"
    "Create Polygon Annotation\0" "Create Poly Line Annotation\0" "Create Highlight Annotation\0"
    "Create Underline Annotation\0" "Create Squiggly Annotation\0" "Create Strike Out Annotation\0"
    "Create Redact Annotation\0" "Create Stamp Annotation\0" "Create Caret Annotation\0"
    "Create Ink Annotation\0" "Create Popup Annotation\0" "Create File Attachment Annotation\0"
    "Invert Colors\0" "Toggle Page Info\0" "Toggle Zoom\0"
    "Navigate Back\0" "Navigate Forward\0" "Toggle Cursor Position\0"
    "Open Next File In Folder\0" "Open Previous File In Folder\0" "Command Palette\0"
    "Show Logs\0" "Show PDF Info\0" "Show Errors\0"
    "Clear History\0" "Reopen Last Closed\0" "Next Tab\0"
    "Previous Tab\0" "Smart Next Tab\0" "Smart Next Tab\0"
    "Move Tab Left\0" "Move Tab Right\0" "Select next theme\0"
    "Toggle Frequently Read\0" "Invoke Inverse Search\0" "Execute a program\0"
    "View With Custom External Viewer\0" "Launch a browser or run command with selection\0" "Set theme\0"
    "Toggle Inverse Search\0" "Debug: Corrupt Memory\0" "Debug: Crash Me\0"
    "Debug: Download Symbols\0" "Debug: Test App\0" "Debug: Show Notification\0"
    "Debug: Start Stress Test\0" "Debug: Toggle Predictive Rendering\0" "Debug: Toggle Rtl\0"
    "Toggle Anti-Alias Rendering\0" "Toggle Smooth Scroll\0" "Toggle Hide Scrollbar\0"
    "Toggle Scrollbar In Single Page\0" "Toggle Lazy Loading\0" "List Printers\0"
    "Toggle Windows Previewer\0" "Toggle Windows Search Filter\0" "Take Screenshot\0"
    "Crop Image\0" "Resize Image\0" "Save Image\0"
    "Paste Image From Clipboard\0" "Do nothing\0" "\0";
// clang-format on
// @gen-end cmd-c

struct ArgSpec {
    int cmdId;
    const char* name;
    CommandArg::Type type;
};

// arguments for the same command should follow each other
// first argument is default and can be specified without a name
static const ArgSpec argSpecs[] = {
    {CmdSelectionHandler, kCmdArgURL, CommandArg::Type::String}, // default
    {CmdSelectionHandler, kCmdArgExe, CommandArg::Type::String},

    {CmdExec, kCmdArgExe, CommandArg::Type::String}, // default
    {CmdExec, kCmdArgFilter, CommandArg::Type::String},

    // and all CmdCreateAnnot* commands
    {CmdCreateAnnotText, kCmdArgColor, CommandArg::Type::Color}, // default
    {CmdCreateAnnotText, kCmdArgBgColor, CommandArg::Type::Color},
    {CmdCreateAnnotText, kCmdArgOpacity, CommandArg::Type::Int},
    {CmdCreateAnnotText, kCmdArgOpenEdit, CommandArg::Type::Bool},
    {CmdCreateAnnotText, kCmdArgCopyToClipboard, CommandArg::Type::Bool},
    {CmdCreateAnnotText, kCmdArgSetContent, CommandArg::Type::Bool},
    {CmdCreateAnnotText, kCmdArgTextSize, CommandArg::Type::Int},
    {CmdCreateAnnotText, kCmdArgBorderWidth, CommandArg::Type::Int},
    {CmdCreateAnnotText, kCmdArgInteriorColor, CommandArg::Type::Color},
    {CmdCreateAnnotText, kCmdArgFocusEdit, CommandArg::Type::Bool},
    {CmdCreateAnnotText, kCmdArgFocusList, CommandArg::Type::Bool},

    // and  CmdScrollDown, CmdGoToNextPage, CmdGoToPrevPage
    {CmdScrollUp, kCmdArgN, CommandArg::Type::Int}, // default

    {CmdSetTheme, kCmdArgTheme, CommandArg::Type::String}, // default

    {CmdZoomCustom, kCmdArgLevel, CommandArg::Type::String}, // default

    {CmdCommandPalette, kCmdArgMode, CommandArg::Type::String}, // default

    {CmdNone, "", CommandArg::Type::None}, // sentinel
};

CustomCommand* gFirstCustomCommand = nullptr;

// returns -1 if not found
static NO_INLINE int GetCommandIdByNameOrDesc(SeqStrings commands, const char* s) {
    int idx = seqstrings::StrToIdxIS(commands, s);
    if (idx < 0) {
        return -1;
    }
    ReportIf(idx >= dimofi(gCommandIds));
    int cmdId = gCommandIds[idx];
    return (int)cmdId;
}

// cmdName is "CmdOpenFile" etc.
// returns -1 if not found
int GetCommandIdByName(const char* cmdName) {
    int cmdId = GetCommandIdByNameOrDesc(gCommandNames, cmdName);
    if (cmdId >= 0) {
        return cmdId;
    }
    // backwards compatibility for old names
    if (str::EqI(cmdName, "CmdFindMatch")) {
        return CmdFindToggleMatchCase;
    }
    auto curr = gFirstCustomCommand;
    while (curr) {
        if (curr->idStr && str::EqI(cmdName, curr->idStr)) {
            return curr->id;
        }
        curr = curr->next;
    }
    return -1;
}

// returns -1 if not found
int GetCommandIdByDesc(const char* cmdDesc) {
    int cmdId = GetCommandIdByNameOrDesc(gCommandDescriptions, cmdDesc);
    if (cmdId >= 0) {
        return cmdId;
    }
    auto curr = gFirstCustomCommand;
    while (curr) {
        if (curr->name && str::EqI(cmdDesc, curr->name)) {
            return curr->id;
        }
        curr = curr->next;
    }
    return -1;
}

CommandArg::~CommandArg() {
    str::Free(strVal);
    str::Free(name);
}

// arg names are case insensitive
static bool IsArgName(const char* name, const char* argName) {
    if (str::EqI(name, argName)) {
        return true;
    }
    if (!str::StartsWithI(name, argName)) {
        return false;
    }
    char c = name[str::Len(argName)];
    return c == '=';
}

void InsertArg(CommandArg** firstPtr, CommandArg* arg) {
    // for ease of use by callers, we shift null check here
    if (!arg) {
        return;
    }
    arg->next = *firstPtr;
    *firstPtr = arg;
}

void FreeCommandArgs(CommandArg* first) {
    CommandArg* next;
    CommandArg* curr = first;
    while (curr) {
        next = curr->next;
        delete curr;
        curr = next;
    }
}

CommandArg* FindArg(CommandArg* first, const char* name, CommandArg::Type type) {
    CommandArg* curr = first;
    while (curr) {
        if (IsArgName(curr->name, name)) {
            if (curr->type == type) {
                return curr;
            }
            logf("FindArgByName: found arg of name '%s' by different type (wanted: %d, is: %d)\n", name, type,
                 curr->type);
        }
        curr = curr->next;
    }
    return nullptr;
}

static int gNextCustomCommandId = (int)CmdFirstCustom;

CustomCommand::~CustomCommand() {
    FreeCommandArgs(firstArg);
    str::Free(name);
    str::Free(key);
    str::Free(idStr);
    str::Free(definition);
}

CustomCommand* CreateCustomCommand(const char* definition, int origCmdId, CommandArg* args) {
    // if no args we retain original command id
    // only when we have unique args we have to create a new command id
    int id = origCmdId;
    if (args != nullptr) {
        id = gNextCustomCommandId++;
    } else {
#if 0
        auto existingCmd = FindCustomCommand(origCmdId);
        if (existingCmd) {
            return existingCmd;
        }
#endif
    }
    auto cmd = new CustomCommand();
    cmd->id = id;
    cmd->origId = origCmdId;
    cmd->definition = str::Dup(definition);
    cmd->firstArg = args;
    cmd->next = gFirstCustomCommand;
    gFirstCustomCommand = cmd;
    return cmd;
}

CustomCommand* FindCustomCommand(int cmdId) {
    auto cmd = gFirstCustomCommand;
    while (cmd) {
        if (cmd->id == cmdId) {
            return cmd;
        }
        cmd = cmd->next;
    }
    return nullptr;
}

void FreeCustomCommands() {
    CustomCommand* next;
    CustomCommand* curr = gFirstCustomCommand;
    while (curr) {
        next = curr->next;
        delete curr;
        curr = next;
    }
    gFirstCustomCommand = nullptr;
}

void GetCommandsWithOrigId(Vec<CustomCommand*>& commands, int origId) {
    CustomCommand* curr = gFirstCustomCommand;
    while (curr) {
        if (curr->origId == origId) {
            commands.Append(curr);
        }
        curr = curr->next;
    }
    // reverse so that they are returned in the order they were inserted
    commands.Reverse();
}

static CommandArg* NewArg(CommandArg::Type type, const char* name) {
    auto res = new CommandArg();
    res->type = type;
    res->name = str::Dup(name);
    return res;
}

CommandArg* NewStringArg(const char* name, const char* val) {
    auto res = new CommandArg();
    res->type = CommandArg::Type::String;
    res->name = str::Dup(name);
    res->strVal = str::Dup(val);
    return res;
}

CommandArg* NewFloatArg(const char* name, float val) {
    auto res = new CommandArg();
    res->type = CommandArg::Type::Float;
    res->name = str::Dup(name);
    res->floatVal = val;
    return res;
}

static CommandArg* ParseArgOfType(const char* argName, CommandArg::Type type, const char* val) {
    if (type == CommandArg::Type::Color) {
        ParsedColor col;
        ParseColor(col, val);
        if (!col.parsedOk) {
            // invalid value, skip it
            logf("parseArgOfType: invalid color value '%s'\n", val);
            return nullptr;
        }
        auto arg = NewArg(type, argName);
        arg->colorVal = col;
        return arg;
    }

    if (type == CommandArg::Type::Int) {
        auto arg = NewArg(type, argName);
        arg->intVal = ParseInt(val);
        return arg;
    }

    if (type == CommandArg::Type::String) {
        auto arg = NewArg(type, argName);
        arg->strVal = str::Dup(val);
        return arg;
    }

    ReportIf(true);
    return nullptr;
}

CommandArg* TryParseDefaultArg(int defaultArgIdx, const char** argsInOut) {
    // first is default value
    const char* valStart = str::SkipChar(*argsInOut, ' ');
    const char* valEnd = str::FindChar(valStart, ' ');
    const char* argName = argSpecs[defaultArgIdx].name;
    CommandArg::Type type = argSpecs[defaultArgIdx].type;
    if (type == CommandArg::Type::String) {
        // for strings we eat it all to avoid the need for proper quoting
        // creates a problem: all named args must be before default string arg
        valEnd = nullptr;
    }
    TempStr val = nullptr;
    if (valEnd == nullptr) {
        val = str::Dup(valStart);
    } else {
        val = str::Dup(valStart, valEnd - valStart);
        valEnd = str::SkipChar(valEnd, ' ');
    }
    // no matter what, we advance past the value
    *argsInOut = valEnd;

    // we don't support bool because we don't have to yet
    // (no command have default bool value)
    return ParseArgOfType(argName, type, val);
}

// 1  : true
// 0  : false
// -1 : not a known boolean string
static int ParseBool(const char* s) {
    if (str::EqI(s, "1") || str::EqI(s, "true") || str::EqI(s, "yes")) {
        return true;
    }
    if (str::EqI(s, "0") || str::EqI(s, "false") || str::EqI(s, "no")) {
        return true;
    }
    return false;
}

// parse:
//   <name> <value>
//   <name>: <value>
//   <name>=<value>
// for booleans only <name> works as well and represents true
CommandArg* TryParseNamedArg(int firstArgIdx, const char** argsInOut) {
    const char* valStart = nullptr;
    const char* argName = nullptr;
    CommandArg::Type type = CommandArg::Type::None;
    const char* s = *argsInOut;
    int cmdId = argSpecs[firstArgIdx].cmdId;
    for (int i = firstArgIdx;; i++) {
        if (argSpecs[i].cmdId != cmdId) {
            // not a known argument for this command
            return nullptr;
        }
        argName = argSpecs[i].name;
        if (!str::StartsWithI(s, argName)) {
            continue;
        }
        type = argSpecs[i].type;
        break;
    }
    s += str::Len(argName);
    if (s[0] == 0) {
        if (type == CommandArg::Type::Bool) {
            // name of bool arg followed by nothing is true
            *argsInOut = nullptr;
            auto arg = NewArg(type, argName);
            arg->boolVal = true;
            return arg;
        }
    } else if (s[0] == ' ') {
        if (type == CommandArg::Type::Bool) {
            // name of bool arg followed by nothing is true
            s = str::SkipChar(s, ' ');
            *argsInOut = s;
            auto arg = NewArg(type, argName);
            arg->boolVal = true;
            return arg;
        }
        valStart = str::SkipChar(s, ' ');
    } else if (s[0] == ':' && s[1] == ' ') {
        valStart = str::SkipChar(s + 1, ' ');
    } else if (s[0] == '=') {
        valStart = s + 1;
    }
    if (valStart == nullptr) {
        // <args> doesn't start with any of the available commands for this command
        return nullptr;
    }
    const char* valEnd = str::FindChar(valStart, ' ');
    TempStr val = nullptr;
    if (valEnd == nullptr) {
        val = str::DupTemp(valStart);
    } else {
        val = str::DupTemp(valStart, valEnd - valStart);
        valEnd++;
    }
    if (type == CommandArg::Type::Bool) {
        auto bv = ParseBool(val);
        bool b;
        if (bv == 0) {
            b = false;
            *argsInOut = valEnd;
        } else if (bv == 1) {
            b = true;
            *argsInOut = valEnd;
        } else {
            // bv is -1, which means not a recognized bool value, so assume
            // it wasn't given
            // TODO: should apply only if arg doesn't end with ':' or '='
            b = true;
            *argsInOut = valStart;
        }
        auto arg = NewArg(type, argName);
        arg->boolVal = b;
        return arg;
    }

    *argsInOut = valEnd;
    return ParseArgOfType(argName, type, val);
}

// create custom command as defined in Shortcuts section in advanced settings
// or DDE commands
// return null if unkown command
CustomCommand* CreateCommandFromDefinition(const char* definition) {
    // the same command can be sent via DDE many times
    // we don't want to create duplicate CustomCommand
    for (auto cmd = gFirstCustomCommand; cmd; cmd = cmd->next) {
        if (str::Eq(definition, cmd->definition)) {
            return cmd;
        }
    }

    StrVec parts;
    Split(&parts, definition, " ", true, 2);
    const char* cmd = parts[0];
    int cmdId = GetCommandIdByName(cmd);
    if (cmdId < 0) {
        MaybeDelayedWarningNotification("Error parsing Shortcuts in advanced settings. Unknown cmd name '%s'\n",
                                        definition);
        return nullptr;
    }
    if (parts.Size() == 1) {
        return CreateCustomCommand(definition, cmdId, nullptr);
    }

    // some commands share the same arguments, so cannonalize them
    int argCmdId = cmdId;
    switch (cmdId) {
        case CmdCreateAnnotText:
        case CmdCreateAnnotLink:
        case CmdCreateAnnotFreeText:
        case CmdCreateAnnotLine:
        case CmdCreateAnnotSquare:
        case CmdCreateAnnotCircle:
        case CmdCreateAnnotPolygon:
        case CmdCreateAnnotPolyLine:
        case CmdCreateAnnotHighlight:
        case CmdCreateAnnotUnderline:
        case CmdCreateAnnotSquiggly:
        case CmdCreateAnnotStrikeOut:
        case CmdCreateAnnotRedact:
        case CmdCreateAnnotStamp:
        case CmdCreateAnnotCaret:
        case CmdCreateAnnotInk:
        case CmdCreateAnnotPopup:
        case CmdCreateAnnotFileAttachment: {
            argCmdId = CmdCreateAnnotText;
            break;
        }
        case CmdScrollUp:
        case CmdScrollDown:
        case CmdGoToNextPage:
        case CmdGoToPrevPage: {
            argCmdId = CmdScrollUp;
            break;
        }
    }

    // find arguments for this cmdId
    int firstArgIdx = -1;
    for (int i = 0;; i++) {
        int id = argSpecs[i].cmdId;
        if (id == CmdNone) {
            // the command doesn't accept any arguments
            MaybeDelayedWarningNotification("Error parsing Shortcuts: cmd '%s' doesn't accept arguments\n", definition);
            return CreateCustomCommand(definition, cmdId, nullptr);
        }
        if (id != argCmdId) {
            continue;
        }
        firstArgIdx = i;
        break;
    }
    if (firstArgIdx < 0) {
        // shouldn't happen, we already filtered commands without arguments
        logf("CreateCommandFromDefinition: didn't find arguments for: '%s', cmdId: %d, argCmdId: '%d'\n", definition,
             cmdId, argCmdId);
        ReportIf(true);
        return nullptr;
    }

    const char* currArg = parts[1];

    CommandArg* firstArg = nullptr;
    CommandArg* arg;
    for (; currArg;) {
        arg = TryParseNamedArg(firstArgIdx, &currArg);
        if (!arg) {
            arg = TryParseDefaultArg(firstArgIdx, &currArg);
        }
        if (arg) {
            InsertArg(&firstArg, arg);
        }
    }
    if (!firstArg) {
        MaybeDelayedWarningNotification("Error parsing Shortcuts: failed to parse arguments for '%s'\n", definition);
        return nullptr;
    }

    if (cmdId == CmdCommandPalette && firstArg) {
        // validate mode
        const char* s = firstArg->strVal;
        static SeqStrings validModes = ">\0#\0@\0:\0"; // TODO: "@@\0" ?
        if (seqstrings::StrToIdx(validModes, s) < 0) {
            logf("CreateCommandFromDefinition: invalid CmdCommandPalette mode in '%s'\n", definition);
            FreeCommandArgs(firstArg);
            firstArg = nullptr;
        }
    }

    if (cmdId == CmdZoomCustom) {
        // special case: the argument is declared as string but it really is float
        // we convert it in-place here
        float zoomVal = ZoomFromString(firstArg->strVal, 0);
        if (0 == zoomVal) {
            FreeCommandArgs(firstArg);
            MaybeDelayedWarningNotification("CreateCommandFromDefinition: failed to parse arguments in '%s'\n",
                                            definition);
            return nullptr;
        }
        firstArg->type = CommandArg::Type::Float;
        firstArg->floatVal = zoomVal;
    }
    auto res = CreateCustomCommand(definition, cmdId, firstArg);
    return res;
}

CommandArg* GetCommandArg(CustomCommand* cmd, const char* name) {
    if (!cmd) {
        return nullptr;
    }
    CommandArg* curr = cmd->firstArg;
    while (curr) {
        if (str::EqI(curr->name, name)) {
            return curr;
        }
        curr = curr->next;
    }
    return nullptr;
}

int GetCommandIntArg(CustomCommand* cmd, const char* name, int defValue) {
    auto arg = GetCommandArg(cmd, name);
    if (arg) {
        return arg->intVal;
    }
    return defValue;
}

bool GetCommandBoolArg(CustomCommand* cmd, const char* name, bool defValue) {
    auto arg = GetCommandArg(cmd, name);
    if (arg) {
        return arg->boolVal;
    }
    return defValue;
}

const char* GetCommandStringArg(CustomCommand* cmd, const char* name, const char* defValue) {
    auto arg = GetCommandArg(cmd, name);
    if (arg) {
        return arg->strVal;
    }
    return defValue;
}
