/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
COMMANDS() define commands.
A command is represented by a unique number, defined as
Cmd* enum (e.g. CmdOpen) and a human-readable name (not used yet).
*/
#define COMMANDS(V)                                                       \
    V(CmdOpenFile, "Open File...")                                        \
    V(CmdOpenFolder, "Open Folder...")                                    \
    V(CmdClose, "Close Document")                                         \
    V(CmdCloseCurrentDocument, "Close Current Document")                  \
    V(CmdCloseOtherTabs, "Close Other Tabs")                              \
    V(CmdCloseTabsToTheRight, "Close Tabs To The Right")                  \
    V(CmdSaveAs, "Save File As...")                                       \
    V(CmdPrint, "Print Document...")                                      \
    V(CmdShowInFolder, "Show File In Folder...")                          \
    V(CmdRenameFile, "Rename File...")                                    \
    V(CmdExit, "Exit Application")                                        \
    V(CmdReloadDocument, "Reload Document")                               \
    V(CmdCreateShortcutToFile, "Create .lnk Shortcut")                    \
    V(CmdSendByEmail, "Send Document By Email...")                        \
    V(CmdProperties, "Show Document Properties...")                       \
    V(CmdSinglePageView, "Single Page View")                              \
    V(CmdFacingView, "Facing View")                                       \
    V(CmdBookView, "Book View")                                           \
    V(CmdToggleContinuousView, "Toggle Continuous View")                  \
    V(CmdToggleMangaMode, "Toggle Manga Mode")                            \
    V(CmdRotateLeft, "Rotate Left")                                       \
    V(CmdRotateRight, "Rotate Right")                                     \
    V(CmdToggleBookmarks, "Toggle Bookmarks")                             \
    V(CmdToggleTableOfContents, "Toggle Table Of Contents")               \
    V(CmdToggleFullscreen, "Toggle Fullscreen")                           \
    V(CmdPresentationWhiteBackground, "Presentation White Background")    \
    V(CmdPresentationBlackBackground, "Presentation Black Background")    \
    V(CmdTogglePresentationMode, "View: Presentation Mode")               \
    V(CmdToggleToolbar, "Toggle Toolbar")                                 \
    V(CmdToggleScrollbars, "Toggle Scrollbars")                           \
    V(CmdToggleMenuBar, "Toggle Menu Bar")                                \
    V(CmdCopySelection, "Copy Selection")                                 \
    V(CmdTranslateSelectionWithGoogle, "Translate Selection with Google") \
    V(CmdTranslateSelectionWithDeepL, "Translate Selection With DeepL")   \
    V(CmdSearchSelectionWithGoogle, "Search Selection with Google")       \
    V(CmdSearchSelectionWithBing, "Search Selection with Bing")           \
    V(CmdSelectAll, "Select All")                                         \
    V(CmdNewWindow, "Open New SumatraPDF Window")                         \
    V(CmdDuplicateInNewWindow, "Open Current Document In New Window")     \
    V(CmdCopyImage, "Copy Image")                                         \
    V(CmdCopyLinkTarget, "Copy Link Target")                              \
    V(CmdCopyComment, "Copy Comment")                                     \
    V(CmdCopyFilePath, "Copy File Path")                                  \
    V(CmdScrollUp, "Scroll Up")                                           \
    V(CmdScrollDown, "Scroll Down")                                       \
    V(CmdScrollLeft, "Scroll Left")                                       \
    V(CmdScrollRight, "Scroll Right")                                     \
    V(CmdScrollLeftPage, "Scroll Left By Page")                           \
    V(CmdScrollRightPage, "Scroll Right By Page")                         \
    V(CmdScrollUpPage, "Scroll Up By Page")                               \
    V(CmdScrollDownPage, "Scroll Down By Page")                           \
    V(CmdScrollDownHalfPage, "Scroll Down By Half Page")                  \
    V(CmdScrollUpHalfPage, "Scroll Up By Half Page")                      \
    V(CmdGoToNextPage, "Next Page")                                       \
    V(CmdGoToPrevPage, "Previous Page")                                   \
    V(CmdGoToFirstPage, "First Page")                                     \
    V(CmdGoToLastPage, "Last Page")                                       \
    V(CmdGoToPage, "Go to Page...")                                       \
    V(CmdFindFirst, "Find")                                               \
    V(CmdFindNext, "Find Next")                                           \
    V(CmdFindPrev, "Find Previous")                                       \
    V(CmdFindNextSel, "Find Next Selection")                              \
    V(CmdFindPrevSel, "Find Previous Selection")                          \
    V(CmdFindMatch, "Find: Match Case")                                   \
    V(CmdSaveAnnotations, "Save Annotations to existing PDF")             \
    V(CmdSaveAnnotationsNewFile, "Save Annotations to a new PDF")         \
    V(CmdEditAnnotations, "Edit Annotations")                             \
    V(CmdDeleteAnnotation, "Delete Annotation")                           \
    V(CmdZoomFitPage, "Zoom: Fit Page")                                   \
    V(CmdZoomActualSize, "Zoom: Actual Size")                             \
    V(CmdZoomFitWidth, "Zoom: Fit Width")                                 \
    V(CmdZoom6400, "Zoom: 6400%")                                         \
    V(CmdZoom3200, "Zoom: 3200%")                                         \
    V(CmdZoom1600, "Zoom: 1600%")                                         \
    V(CmdZoom800, "Zoom: 800%")                                           \
    V(CmdZoom400, "Zoom: 400%")                                           \
    V(CmdZoom200, "Zoom: 200%")                                           \
    V(CmdZoom150, "Zoom: 150%")                                           \
    V(CmdZoom125, "Zoom: 125%")                                           \
    V(CmdZoom100, "Zoom: 100%")                                           \
    V(CmdZoom50, "Zoom: 50%")                                             \
    V(CmdZoom25, "Zoom: 25%")                                             \
    V(CmdZoom12_5, "Zoom: 12.5%")                                         \
    V(CmdZoom8_33, "Zoom: 8.33%")                                         \
    V(CmdZoomFitContent, "Zoom: Fit Content")                             \
    V(CmdZoomCustom, "Zoom: Custom...")                                   \
    V(CmdZoomIn, "Zoom In")                                               \
    V(CmdZoomOut, "Zoom Out")                                             \
    V(CmdZoomFitWidthAndContinuous, "Zoom: Fit Width And Continuous")     \
    V(CmdZoomFitPageAndSinglePage, "Zoom: Fit Page and Single Page")      \
    V(CmdContributeTranslation, "Contribute Translation")                 \
    V(CmdOpenWithFirst, "don't use")                                      \
    V(CmdOpenWithExplorer, "Open Directory In Explorer")                  \
    V(CmdOpenWithDirectoryOpus, "Open Directory In Directory Opus")       \
    V(CmdOpenWithTotalCommander, "Open Directory In Total Commander")     \
    V(CmdOpenWithDoubleCommander, "Open Directory In Double Commander")   \
    V(CmdOpenWithAcrobat, "Open With Adobe Acrobat")                      \
    V(CmdOpenWithFoxIt, "Open With FoxIt")                                \
    V(CmdOpenWithFoxItPhantom, "Open With FoxIt Phantom")                 \
    V(CmdOpenWithPdfXchange, "Open With PdfXchange")                      \
    V(CmdOpenWithXpsViewer, "Open With Xps Viewer")                       \
    V(CmdOpenWithHtmlHelp, "Open With HTML Help")                         \
    V(CmdOpenWithPdfDjvuBookmarker, "Open With Pdf&Djvu Bookmarker")      \
    V(CmdOpenWithLast, "don't use")                                       \
    V(CmdOpenSelectedDocument, "Open Selected Document")                  \
    V(CmdPinSelectedDocument, "Pin Selected Document")                    \
    V(CmdForgetSelectedDocument, "Remove Selected Document From History") \
    V(CmdExpandAll, "Expand All")                                         \
    V(CmdCollapseAll, "Collapse All")                                     \
    V(CmdSaveEmbeddedFile, "Save Embedded File...")                       \
    V(CmdOpenEmbeddedPDF, "Open Embedded PDF")                            \
    V(CmdSaveAttachment, "Save Attachment...")                            \
    V(CmdOpenAttachment, "Open Attachment")                               \
    V(CmdOptions, "Options...")                                           \
    V(CmdAdvancedOptions, "Advanced Options...")                          \
    V(CmdAdvancedSettings, "Advanced Settings...")                        \
    V(CmdChangeLanguage, "Change Language...")                            \
    V(CmdCheckUpdate, "Check For Updates")                                \
    V(CmdHelpOpenManualInBrowser, "Help: Manual")                         \
    V(CmdHelpOpenKeyboardShortcutsInBrowser, "Help: Keyboard Shortcuts")  \
    V(CmdHelpVisitWebsite, "Help: SumatraPDF Website")                    \
    V(CmdHelpAbout, "Help: About SumatraPDF")                             \
    V(CmdMoveFrameFocus, "Move Frame Focus")                              \
    V(CmdFavoriteAdd, "Add Favorite")                                     \
    V(CmdFavoriteDel, "Delete Favorite")                                  \
    V(CmdFavoriteToggle, "Toggle Favorites")                              \
    V(CmdToggleLinks, "Toggle Show Links")                                \
    V(CmdDebugCrashMe, "Debug: Crash Me")                                 \
    V(CmdDebugCorruptMemory, "Debug: Corrupt Memory")                     \
    V(CmdDebugDownloadSymbols, "Debug: Download Symbols")                 \
    V(CmdDebugTestApp, "Debug: Test App")                                 \
    V(CmdDebugShowNotif, "Debug: Show Notification")                      \
    V(CmdDebugStartStressTest, "Debug: Start Stress Test")                \
    V(CmdCreateAnnotText, "Create Text Annotation")                       \
    V(CmdCreateAnnotLink, "Create Link Annotation")                       \
    V(CmdCreateAnnotFreeText, "Create Free Text Annotation")              \
    V(CmdCreateAnnotLine, "Create Line Annotation")                       \
    V(CmdCreateAnnotSquare, "Create Square Annotation")                   \
    V(CmdCreateAnnotCircle, "Create Circle Annotation")                   \
    V(CmdCreateAnnotPolygon, "Create Polygon Annotation")                 \
    V(CmdCreateAnnotPolyLine, "Create Poly Line Annotation")              \
    V(CmdCreateAnnotHighlight, "Create Highlight Annotation")             \
    V(CmdCreateAnnotUnderline, "Create Underline Annotation")             \
    V(CmdCreateAnnotSquiggly, "Create Squiggly Annotation")               \
    V(CmdCreateAnnotStrikeOut, "Create Strike Out Annotation")            \
    V(CmdCreateAnnotRedact, "Create Redact Annotation")                   \
    V(CmdCreateAnnotStamp, "Create Stamp Annotation")                     \
    V(CmdCreateAnnotCaret, "Create Caret Annotation")                     \
    V(CmdCreateAnnotInk, "Create Ink Annotation")                         \
    V(CmdCreateAnnotPopup, "Create Popup Annotation")                     \
    V(CmdCreateAnnotFileAttachment, "Create File Attachment Annotation")  \
    V(CmdInvertColors, "Invert Colors")                                   \
    V(CmdTogglePageInfo, "Toggle Page Info")                              \
    V(CmdToggleZoom, "Toggle Zoom")                                       \
    V(CmdNavigateBack, "Navigate Back")                                   \
    V(CmdNavigateForward, "Navigate Forward")                             \
    V(CmdToggleCursorPosition, "Toggle Cursor Position")                  \
    V(CmdOpenNextFileInFolder, "Open Next File In Folder")                \
    V(CmdOpenPrevFileInFolder, "Open Previous File In Folder")            \
    V(CmdCommandPalette, "Command Palette")                               \
    V(CmdCommandPaletteNoFiles, "Command Palette No Files")               \
    V(CmdCommandPaletteOnlyTabs, "Command Palette Only Tabs")             \
    V(CmdShowLog, "Show Log")                                             \
    V(CmdClearHistory, "Clear History")                                   \
    V(CmdReopenLastClosedFile, "Reopen Last Closed")                      \
    V(CmdNextTab, "Next Tab")                                             \
    V(CmdPrevTab, "Previous Tab")                                         \
    V(CmdSelectNextTheme, "Select next theme")                            \
    V(CmdToggleFrequentlyRead, "Toggle Frequently Read")                  \
    V(CmdNone, "Do nothing")

//V(CmdSelectAnnotation, "Select Annotation")                           \

// order of CreateAnnot* must be the same as enum AnnotationType
/*
    TOOD: maybe add commands for those annotations
    Sound,
    Movie,
    Widget,
    Screen,
    PrinterMark,
    TrapNet,
    Watermark,
    ThreeD,
*/

#define DEF_CMD(id, s) id,

enum {
    // commands are integers sent with WM_COMMAND so start them
    // at some number higher than 0
    CmdFirst = 200,
    CmdSeparator = CmdFirst,

    COMMANDS(DEF_CMD)

        CmdLastCommand,

    /* range for "external viewers" setting */
    CmdOpenWithExternalFirst,
    CmdOpenWithExternalLast = CmdOpenWithExternalFirst + 32,

    /* range for "SelectionHandlers" setting */
    CmdSelectionHandlerFirst,
    CmdSelectionHandlerLast = CmdSelectionHandlerFirst + 32,

    /* range for file history */
    CmdFileHistoryFirst,
    CmdFileHistoryLast = CmdFileHistoryFirst + 32,

    /* range for favorites */
    CmdFavoriteFirst,
    CmdFavoriteLast = CmdFavoriteFirst + 256,

    /* range for themes. We don't have themes yet. */
    CmdThemeFirst,
    CmdThemeLast = CmdThemeFirst + 20,

    CmdLast = CmdThemeLast,

    // aliases, at the end to not mess ordering
    CmdViewLayoutFirst = CmdSinglePageView,
    CmdViewLayoutLast = CmdToggleMangaMode,

    CmdZoomFirst = CmdZoomFitPage,
    CmdZoomLast = CmdZoomCustom,

    CmdCreateAnnotFirst = CmdCreateAnnotText,
    CmdCreateAnnotLast = CmdCreateAnnotFileAttachment,
};

#undef DEF_CMD

int GetCommandIdByName(const char*);
int GetCommandIdByDesc(const char*);

extern SeqStrings gCommandDescriptions;
