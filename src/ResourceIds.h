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
