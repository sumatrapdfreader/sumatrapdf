/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define COMMANDS(V)                              \
    V(Open, "Open File...")                      \
    V(OpenFolder, "Open Folder...")              \
    V(Close, "Close Document")                   \
    V(SaveAs, "Save File As...")                 \
    V(Print, "Print Document...")                \
    V(ShowInFolder, "Show File In Folder...")    \
    V(Exit, "Exit Application")                  \
    V(Refresh, "Reload Document")                \
    V(SaveAsBookmark, "Save As Bookmark...")     \
    V(SendByEmail, "Send Document By Email...")  \
    V(Properties, "Show Document Properties...") \
    V(ExitFullScreen, "Exit FullScreen")

#define DEF_CMD(id, s) Cmd##id,

enum {
    CmdFirst = 200,
    CmdSeparator = CmdFirst,
    CmdSeparatorEmbed,

    COMMANDS(DEF_CMD)

        CmdViewSinglePage, // alias: ViewLayoutFirst
    CmdViewFacing,
    CmdViewBook,
    CmdViewContinuous,
    CmdViewMangaMode, // alias: ViewLayoutLast

    CmdViewRotateLeft,
    CmdViewRotateRight,
    CmdViewBookmarks,
    CmdViewFullScreen,
    CmdViewPresentationMode,
    CmdViewShowHideToolbar,
    CmdViewShowHideMenuBar,

    CmdCopySelection,
    CmdSelectAll,

    CmdNewWindow,
    CmdDuplicateInNewWindow,
    CmdCopyImage,
    CmdCopyLinkTarget,
    CmdCopyComment,

    CmdGoToNextPage,
    CmdGoToPrevPage,
    CmdGoToFirstPage,
    CmdGoToLastPage,
    CmdGoToPage,
    CmdGoToNavBack,
    CmdGoToNavForward,

    CmdFindFirst,
    CmdFindNext,
    CmdFindPrev,
    CmdFindMatch,
    CmdFindNextSel,
    CmdFindPrevSel,

    CmdSaveAnnotationsSmx,
    CmdEditAnnotations,
    CmdZoomFitPage, // alias:: ZoomFirst
    CmdZoomActualSize,
    CmdZoomFitWidth,
    CmdZoom6400,
    CmdZoom3200,
    CmdZoom1600,
    CmdZoom800,
    CmdZoom400,
    CmdZoom200,
    CmdZoom150,
    CmdZoom125,
    CmdZoom100,
    CmdZoom50,
    CmdZoom25,
    CmdZoom12_5,
    CmdZoom8_33,
    CmdZoomFitContent,
    CmdZoomCustom, // alias: ZoomLast

    CmdZoomIn,
    CmdZoomOut,
    CmdZoomFitWidthAndContinous,
    CmdZoomFitPageAndSinglePage,

    CmdContributeTranslation,

    CmdViewWithAcrobat,
    CmdViewWithFoxIt,
    CmdViewWithPdfXchange,
    CmdViewWithXpsViewer,
    CmdViewWithHtmlHelp,

    CmdOpenSelectedDocument,
    CmdPinSelectedDocument,
    CmdForgetSelectedDocument,

    CmdEdit, // TODO: better name
    CmdAddSibling,
    CmdAddChild,
    CmdRemove,
    CmdAddPdfChild,
    CmdAddPdfSibling,

    CmdVisitWebsite,
    CmdExpandAll,
    CmdCollapseAll,
    CmdExportBookmarks,

    CmdSortTagSmallFirst,
    CmdSortTagBigFirst,
    CmdSortColor,
    CmdSaveEmbedded,
    CmdOpenEmbedded,

    CmdAbout,
    CmdOptions,
    CmdAdvancedOptions,
    CmdChangeLanguage,
    CmdCheckUpdate,
    CmdOpenManualInBrowser,

    CmdMoveFrameFocus,

    CmdFavAdd,
    CmdFavDel,
    CmdFavToggle,
    CmdFavShow,
    CmdFavHide,
    CmdRenameFile,

    CmdDebugShowLinks,
    CmdDebugCrashMe,
    CmdDebugEbookUI,
    CmdDebugAnnotations,
    CmdDebugDownloadSymbols,
    CmdDebugTestApp,
    CmdDebugShowNotif,
    CmdDebugMui,

    CmdNewBookmarks,

    /* a range for "external viewers" setting */
    CmdOpenWithExternalFirst,
    CmdOpenWithExternalLast = CmdOpenWithExternalFirst + 20,

    /* a range for file history */
    CmdFileHistoryFirst,
    CmdFileHistoryLast = CmdFileHistoryFirst + 20,

    /* a range for favorites */
    CmdFavFirst,
    CmdFavLast = CmdFavFirst + 200,

    /* a range for themes. We don't have themes yet. */
    CmdThemeFirst,
    CmdThemeLast,

    CmdLast = CmdThemeLast,

    // aliases, at the end to not mess ordering
    CmdViewLayoutFirst = CmdViewSinglePage,
    CmdViewLayoutLast = CmdViewMangaMode,

    CmdZoomFirst = CmdZoomFitPage,
    CmdZoomLast = CmdZoomCustom,
};

#undef DEF_CMD
