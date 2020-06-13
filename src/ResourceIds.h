/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

enum class Cmd {
    Open = 400,
    OpenFolder,
    Close,
    SaveAs,
    Print,
    ShowInFolder,
    Exit,
    Refresh,
    SaveAsBookmark,
    SendByEmail,
    Properties,
    ExitFullScreen,

    ViewSinglePage, // alias: ViewLayoutFirst
    ViewFacing,
    ViewBook,
    ViewContinuous,
    ViewMangaMode, // alias: ViewLayoutLast

    ViewRotateLeft,
    ViewRotateRight,
    ViewBookmarks,
    ViewFullScreen,
    ViewPresentationMode,
    ViewShowHideToolbar,
    ViewShowHideMenuBar,

    CopySelection,
    SelectAll,

    NewWindow,
    DuplicateInNewWindow,
    CopyImage,
    CopyLinkTarget,
    CopyComment,

    GoToNextPage,
    GoToPrevPage,
    GoToFirstPage,
    GoToLastPage,
    GoToPage,
    GoToNavBack,
    GoToNavForward,

    FindFirst,
    FindNext,
    FindPrev,
    FindMatch,
    FindNextSel,
    FindPrevSel,

    SaveAnnotationsSmx,
    EditAnnotations,
    ZoomFitPage, // alias:: ZoomFirst
    ZoomActualSize,
    ZoomFitWidth,
    Zoom6400,
    Zoom3200,
    Zoom1600,
    Zoom800,
    Zoom400,
    Zoom200,
    Zoom150,
    Zoom125,
    Zoom100,
    Zoom50,
    Zoom25,
    Zoom12_5,
    Zoom8_33,
    ZoomFitContent,
    ZoomCustom, // alias: ZoomLast

    ContributeTranslation,

    ViewWithAcrobat,
    ViewWithFoxIt,
    ViewWithPdfXchange,
    ViewWithXpsViewer,
    ViewWithHtmlHelp,

    OpenSelectedDocument,
    PinSelectedDocument,
    ForgetSelectedDocument,

    Edit, // TODO: better name
    AddSibling,
    AddChild,
    Remove,
    AddPdfChild,
    AddPdfSibling,

    VisitWebsite,
    ExpandAll,
    CollapseAll,
    ExportBookmarks,

    SortTagSmallFirst,
    SortTagBigFirst,
    SortColor,
    SaveEmbedded,
    OpenEmbedded,
    EmbedSeparator, // TODO: SeparatorEmbed

    About,
    Options,
    ChangeLanguage,
    CheckUpdate,
    Manual,

    MoveFrameFocus,

    FavAdd,
    FavDel,
    FavToggle,
    FavShow,
    FavHide,
    RenameFile,

    DebugShowLinks,
    DebugCrashMe,
    LoadMobiSample,
    DebugEbookUI,
    DebugAnnotations,
    DebugDownloadSymbols,
    DebugTestApp,
    DebugShowNotif,
    DebugMui,

    AdvancedOptions,
    NewBookmarks,

    ViewZoomIn,
    ViewZoomOut,
    ToolbarViewFitWidth, // TODO: replace with ViewFitWidth,
    ToolbarViewFitPage,  // TODO: replace with ViewFitPage

    Separator,

    /* a range for "external viewers" setting */
    OpenWithExternalFirst,
    OpenWithExternalLast = OpenWithExternalFirst + 20,

    /* a range for file history */
    FileHistoryFirst,
    FileHistoryLast = FileHistoryFirst + 20,

    /* a range for favorites */
    FavFirst,
    FavLast = FavFirst + 200,

    /* a range for themes. We don't have themes yet. */
    ThemeFirst,
    ThemeLast,

    // aliases, at the end to not mess ordering
    ViewLayoutFirst = ViewSinglePage,
    ViewLayoutLast = ViewMangaMode,

    ZoomFirst = ZoomFitPage,
    ZoomLast = ZoomCustom,
};

enum {
    /* a range for "external viewers" setting */
    IDM_OPEN_WITH_EXTERNAL_FIRST,
    IDM_OPEN_WITH_EXTERNAL_LAST = IDM_OPEN_WITH_EXTERNAL_FIRST + 20,
    /* a range for file history */
    IDM_FILE_HISTORY_FIRST,
    IDM_FILE_HISTORY_LAST = IDM_FILE_HISTORY_FIRST + 20,
    IDM_FAV_FIRST,
    IDM_FAV_LAST = IDM_FAV_FIRST + 200,
    IDM_CHANGE_THEME_FIRST,
    IDM_CHANGE_THEME_LAST,
    // TODO: rename to IDM_*
    IDT_VIEW_ZOOMIN,
    IDT_VIEW_ZOOMOUT,
    IDT_VIEW_FIT_WIDTH, // TODO: replace with Cmd::ZoomFIT_WIDTH
    IDT_VIEW_FIT_PAGE,  // TOOD: replace with Cmd::ZoomFIT_PAGE
};
