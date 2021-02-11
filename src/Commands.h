/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
COMMANDS() define commands.
A command is represented by a unique number, defined as
Cmd* enum (e.g. CmdOpen) and a human-readable name (not used yet).
*/
#define COMMANDS(V)                                                       \
    V(CmdOpen, "Open File...")                                            \
    V(CmdOpenFolder, "Open Folder...")                                    \
    V(CmdClose, "Close Document")                                         \
    V(CmdSaveAs, "Save File As...")                                       \
    V(CmdPrint, "Print Document...")                                      \
    V(CmdShowInFolder, "Show File In Folder...")                          \
    V(CmdRenameFile, "Rename File...")                                    \
    V(CmdExit, "Exit Application")                                        \
    V(CmdRefresh, "Reload Document")                                      \
    V(CmdSaveAsBookmark, "Save As Bookmark...")                           \
    V(CmdSendByEmail, "Send Document By Email...")                        \
    V(CmdProperties, "Show Document Properties...")                       \
    V(CmdExitFullScreen, "Exit FullScreen")                               \
    V(CmdViewSinglePage, "View: Single Page")                             \
    V(CmdViewFacing, "View: Facing")                                      \
    V(CmdViewBook, "View: Book")                                          \
    V(CmdViewContinuous, "View: Continuous")                              \
    V(CmdViewMangaMode, "View: Manga Mode")                               \
    V(CmdViewRotateLeft, "View: Rotate Left")                             \
    V(CmdViewRotateRight, "View: Rotate Right")                           \
    V(CmdViewBookmarks, "View: Bookmarks")                                \
    V(CmdViewFullScreen, "View: FullScreen")                              \
    V(CmdViewPresentationMode, "View: Presentation Mode")                 \
    V(CmdViewShowHideToolbar, "View: Toogle Toolbar")                     \
    V(CmdViewShowHideScrollbars, "View: Toogle Scrollbars")               \
    V(CmdViewShowHideMenuBar, "View: Toggle Menu Bar")                    \
    V(CmdCopySelection, "Copy Selection")                                 \
    V(CmdSelectAll, "Select All")                                         \
    V(CmdNewWindow, "Open New Window")                                    \
    V(CmdDuplicateInNewWindow, "Open Document In New Window")             \
    V(CmdCopyImage, "Copy Image")                                         \
    V(CmdCopyLinkTarget, "Copy Link Target")                              \
    V(CmdCopyComment, "Copy Comment")                                     \
    V(CmdGoToNextPage, "Go to Next Page")                                 \
    V(CmdGoToPrevPage, "Go to Previous Page")                             \
    V(CmdGoToFirstPage, "Go to First Page")                               \
    V(CmdGoToLastPage, "Go to Last Page")                                 \
    V(CmdGoToPage, "Go to Page...")                                       \
    V(CmdGoToNavBack, "Navigate: Back")                                   \
    V(CmdGoToNavForward, "Navigate: Forward")                             \
    V(CmdFindFirst, "Find")                                               \
    V(CmdFindNext, "Find: Next")                                          \
    V(CmdFindPrev, "Find: Previous")                                      \
    V(CmdFindMatch, "Find: Match Case")                                   \
    V(CmdFindNextSel, "Find: Next Selection")                             \
    V(CmdFindPrevSel, "Find: Previous Selection")                         \
    V(CmdSaveAnnotations, "Save Annotations")                             \
    V(CmdEditAnnotations, "Edit Annotations")                             \
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
    V(CmdOpenWithFirst, "")                                               \
    V(CmdOpenWithAcrobat, "Open With Adobe Acrobat")                      \
    V(CmdOpenWithAcrobatDC, "Open With Adobe Acrobat DC")                 \
    V(CmdOpenWithFoxIt, "Open With FoxIt")                                \
    V(CmdOpenWithFoxItPhantom, "Open With FoxIt Phantom")                 \
    V(CmdOpenWithPdfXchange, "Open With PdfXchange")                      \
    V(CmdOpenWithXpsViewer, "Open With Xps Viewer")                       \
    V(CmdOpenWithHtmlHelp, "Open With HTML Help")                         \
    V(CmdOpenWithPdfDjvuBookmarker, "Open With Pdf&Djvu Bookmarker")      \
    V(CmdOpenWithLast, "")                                                \
    V(CmdOpenSelectedDocument, "Open Selected Document")                  \
    V(CmdPinSelectedDocument, "Pin Selected Document")                    \
    V(CmdForgetSelectedDocument, "Remove Selected Document From History") \
    V(CmdTocEditorStart, "Table of contents: Start Editing")              \
    V(CmdTocEditorAddSibling, "Add Sibling")                              \
    V(CmdTocEditorAddChild, "Add Child")                                  \
    V(CmdTocEditorRemoveItem, "Remove")                                   \
    V(CmdTocEditorAddPdfChild, "Add PDF Child")                           \
    V(CmdTocEditorAddPdfSibling, "Add PDF Sibling")                       \
    V(CmdExpandAll, "Expand All")                                         \
    V(CmdCollapseAll, "Collapse All")                                     \
    V(CmdExportBookmarks, "Export Bookmarks")                             \
    V(CmdSortTagSmallFirst, "Sort By Tag, Small First")                   \
    V(CmdSortTagBigFirst, "Sort By Tag, Big First")                       \
    V(CmdSortColor, "Sort By Color")                                      \
    V(CmdSaveEmbeddedFile, "Save Embedded File...")                       \
    V(CmdOpenEmbeddedPDF, "Open Embedded PDF")                            \
    V(CmdOptions, "Options...")                                           \
    V(CmdAdvancedOptions, "Advanced Options...")                          \
    V(CmdChangeLanguage, "Change Language...")                            \
    V(CmdCheckUpdate, "Check For Update")                                 \
    V(CmdHelpOpenManualInBrowser, "Help: Manual")                         \
    V(CmdHelpVisitWebsite, "Help: SumatraPDF Website")                    \
    V(CmdHelpAbout, "Help: About SumatraPDF")                             \
    V(CmdMoveFrameFocus, "Move Frame Focus")                              \
    V(CmdFavoriteAdd, "Favorites: Add")                                   \
    V(CmdFavoriteDel, "Favorites: Delete")                                \
    V(CmdFavoriteToggle, "Favorites: Toggle")                             \
    V(CmdFavoriteShow, "Favorites: Show")                                 \
    V(CmdFavoriteHide, "Favorites: Hide")                                 \
    V(CmdDebugShowLinks, "Deubg: Show Links")                             \
    V(CmdDebugCrashMe, "Debug: Crash Me")                                 \
    V(CmdDebugEbookUI, "Debug: Toggle Ebook UI")                          \
    V(CmdDebugAnnotations, "Debug: Annotations")                          \
    V(CmdDebugDownloadSymbols, "Debug: Download Symbols")                 \
    V(CmdDebugTestApp, "Debug: Test App")                                 \
    V(CmdDebugShowNotif, "Debug: Show Notification")                      \
    V(CmdDebugMui, "Debug: Mui")                                          \
    V(CmdNewBookmarks, "New Bookmarks")                                   \
    V(CmdCreateAnnotText, "Create Text Annotation")                       \
    V(CmdCreateAnnotLink, "Create Link Annotation")                       \
    V(CmdCreateAnnotFreeText, "Create  Free Text Annotation")             \
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
    V(CmdLastCommand, "")

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
    CmdSeparatorEmbed,

    COMMANDS(DEF_CMD)

    /* range for "external viewers" setting */
    CmdOpenWithExternalFirst,
    CmdOpenWithExternalLast = CmdOpenWithExternalFirst + 20,

    /* range for file history */
    CmdFileHistoryFirst,
    CmdFileHistoryLast = CmdFileHistoryFirst + 20,

    /* range for favorites */
    CmdFavoriteFirst,
    CmdFavoriteLast = CmdFavoriteFirst + 200,

    /* range for themes. We don't have themes yet. */
    CmdThemeFirst,
    CmdThemeLast = CmdThemeFirst + 20,

    CmdLast = CmdThemeLast,

    // aliases, at the end to not mess ordering
    CmdViewLayoutFirst = CmdViewSinglePage,
    CmdViewLayoutLast = CmdViewMangaMode,

    CmdZoomFirst = CmdZoomFitPage,
    CmdZoomLast = CmdZoomCustom,
};

#undef DEF_CMD
