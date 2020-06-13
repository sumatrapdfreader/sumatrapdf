/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

enum {
    CmdSeparator = 400,
    CmdSeparatorEmbed,

    CmdOpen,
    CmdOpenFolder,
    CmdClose,
    CmdSaveAs,
    CmdPrint,
    CmdShowInFolder,
    CmdExit,
    CmdRefresh,
    CmdSaveAsBookmark,
    CmdSendByEmail,
    CmdProperties,
    CmdExitFullScreen,

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
    CmdChangeLanguage,
    CmdCheckUpdate,
    CmdManual,

    CmdMoveFrameFocus,

    CmdFavAdd,
    CmdFavDel,
    CmdFavToggle,
    CmdFavShow,
    CmdFavHide,
    CmdRenameFile,

    CmdDebugShowLinks,
    CmdDebugCrashMe,
    CmdLoadMobiSample,
    CmdDebugEbookUI,
    CmdDebugAnnotations,
    CmdDebugDownloadSymbols,
    CmdDebugTestApp,
    CmdDebugShowNotif,
    CmdDebugMui,

    CmdAdvancedOptions,
    CmdNewBookmarks,

    CmdViewZoomIn,
    CmdViewZoomOut,
    CmdToolbarViewFitWidth, // TODO: replace with ZoomFitWidth,
    CmdToolbarViewFitPage,  // TODO: replace with ZoomFitPage

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

    // aliases, at the end to not mess ordering
    CmdViewLayoutFirst = CmdViewSinglePage,
    CmdViewLayoutLast = CmdViewMangaMode,

    CmdZoomFirst = CmdZoomFitPage,
    CmdZoomLast = CmdZoomCustom,
};
