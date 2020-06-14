/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define COMMANDS(V)                                               \
    V(Open, "Open File...")                                       \
    V(OpenFolder, "Open Folder...")                               \
    V(Close, "Close Document")                                    \
    V(SaveAs, "Save File As...")                                  \
    V(Print, "Print Document...")                                 \
    V(ShowInFolder, "Show File In Folder...")                     \
    V(RenameFile, "Rename File...")                               \
    V(Exit, "Exit Application")                                   \
    V(Refresh, "Reload Document")                                 \
    V(SaveAsBookmark, "Save As Bookmark...")                      \
    V(SendByEmail, "Send Document By Email...")                   \
    V(Properties, "Show Document Properties...")                  \
    V(ExitFullScreen, "Exit FullScreen")                          \
    V(ViewSinglePage, "View: Single Page")                        \
    V(ViewFacing, "View: Facing")                                 \
    V(ViewBook, "View: Book")                                     \
    V(ViewContinuous, "View: Continuous")                         \
    V(ViewMangaMode, "View: Manga Mode")                          \
    V(ViewRotateLeft, "View: Rotate Left")                        \
    V(ViewRotateRight, "View: Rotate Right")                      \
    V(ViewBookmarks, "View: Bookmarks")                           \
    V(ViewFullScreen, "View: FullScreen")                         \
    V(ViewPresentationMode, "View: Presentation Mode")            \
    V(ViewShowHideToolbar, "View: Toogle Toolbar")                \
    V(ViewShowHideMenuBar, "View: Toggle Menu Bar")               \
    V(CopySelection, "Copy Selection")                            \
    V(SelectAll, "Select All")                                    \
    V(NewWindow, "Open New Window")                               \
    V(DuplicateInNewWindow, "Open Document In New Window")        \
    V(CopyImage, "Copy Image")                                    \
    V(CopyLinkTarget, "Copy Link Target")                         \
    V(CopyComment, "Copy Comment")                                \
    V(GoToNextPage, "Go to Next Page")                            \
    V(GoToPrevPage, "Go to Previous Page")                        \
    V(GoToFirstPage, "Go to First Page")                          \
    V(GoToLastPage, "Go to Last Page")                            \
    V(GoToPage, "Go to Page...")                                  \
    V(GoToNavBack, "Navigate: Back")                              \
    V(GoToNavForward, "Navigate: Forward")                        \
    V(FindFirst, "Find")                                          \
    V(FindNext, "Find: Next")                                     \
    V(FindPrev, "Find: Previous")                                 \
    V(FindMatch, "Find: Match Case")                              \
    V(FindNextSel, "Find: Next Selection")                        \
    V(FindPrevSel, "Find: Previous Selection")                    \
    V(SaveAnnotationsSmx, "Save Annotations As .smx")             \
    V(EditAnnotations, "Edit Annotations")                        \
    V(ZoomFitPage, "Zoom: Fit Page")                              \
    V(ZoomActualSize, "Zoom: Actual Size")                        \
    V(ZoomFitWidth, "Zoom: Fit Width")                            \
    V(Zoom6400, "Zoom: 6400%")                                    \
    V(Zoom3200, "Zoom: 3200%")                                    \
    V(Zoom1600, "Zoom: 1600%")                                    \
    V(Zoom800, "Zoom: 800%")                                      \
    V(Zoom400, "Zoom: 400%")                                      \
    V(Zoom200, "Zoom: 200%")                                      \
    V(Zoom150, "Zoom: 150%")                                      \
    V(Zoom125, "Zoom: 125%")                                      \
    V(Zoom100, "Zoom: 100%")                                      \
    V(Zoom50, "Zoom: 50%")                                        \
    V(Zoom25, "Zoom: 25%")                                        \
    V(Zoom12_5, "Zoom: 12.5%")                                    \
    V(Zoom8_33, "Zoom: 8.33%")                                    \
    V(ZoomFitContent, "Zoom: Fit Content")                        \
    V(ZoomCustom, "Zoom: Custom...")                              \
    V(ZoomIn, "Zoom In")                                        \
    V(ZoomOut, "Zoom Out")                                        \
    V(ZoomFitWidthAndContinous, "Zoom: Fit Width And Continous")  \
    V(ZoomFitPageAndSinglePage, "Zoom: Fit Page and Single Page") \
    V(ContributeTranslation, "Contribute Translation")            \
    V(ViewWithAcrobat, "Open With Adobe Acrobat")                 \
    V(ViewWithFoxIt, "Open With FoxIt")                           \
    V(ViewWithPdfXchange, "Open With PdfXchange")                 \
    V(ViewWithXpsViewer, "Open With Xps Viewer")                  \
    V(ViewWithHtmlHelp, "Open With HTML Help")                    \
    V(OpenSelectedDocument, "Open Selected Document")             \
    V(PinSelectedDocument, "Pin Selected Document")               \
    V(ForgetSelectedDocument, "Forget Selected Document")         \
    V(Edit, "Edit")                                               \
    V(AddSibling, "Add Sibling")                                  \
    V(AddChild, "Add Child")                                      \
    V(Remove, "Remove")                                           \
    V(AddPdfChild, "Add PDF Child")                               \
    V(AddPdfSibling, "Add PDF Sibling")                           \
    V(VisitWebsite, "Help: SumatraPDF Website")                   \
    V(ExpandAll, "Expand All")                                    \
    V(CollapseAll, "Collapse All")                                \
    V(ExportBookmarks, "Export Bookmarks")                        \
    V(SortTagSmallFirst, "Sort By Tag, Small First")              \
    V(SortTagBigFirst, "Sort By Tag, Big First")                  \
    V(SortColor, "Sort By Color")                                 \
    V(SaveEmbedded, "Save Embedded")                              \
    V(OpenEmbedded, "Open Embedde")                               \
    V(About, "Help: About SumatraPDF")                            \
    V(Options, "Options...")                                      \
    V(AdvancedOptions, "Advanced Options...")                     \
    V(ChangeLanguage, "Change Language...")                       \
    V(CheckUpdate, "Check For Update")                            \
    V(OpenManualInBrowser, "Help: Manual")                        \
    V(MoveFrameFocus, "Move Frame Focus")                         \
    V(FavAdd, "Favorites: Add")                                   \
    V(FavDel, "Favorites: Delete")                                \
    V(FavToggle, "Favorites: Toggle")                             \
    V(FavShow, "Favorites: Show")                                 \
    V(FavHide, "Favorites: Hide")                                 \
    V(DebugShowLinks, "Deubg: Show Links")                        \
    V(DebugCrashMe, "Debug: Crash Me")                            \
    V(DebugEbookUI, "Debug: Toggle Ebook UI")                     \
    V(DebugAnnotations, "Debug: Annotations")                     \
    V(DebugDownloadSymbols, "Debug: Download Symbols")            \
    V(DebugTestApp, "Debug: Test App")                            \
    V(DebugShowNotif, "Debug: Show Notification")                 \
    V(DebugMui, "Debug: Mui")                                     \
    V(NewBookmarks, "New Bookmarks")

#define DEF_CMD(id, s) Cmd##id,

enum {
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
    CmdFavFirst,
    CmdFavLast = CmdFavFirst + 200,

    /* range for themes. We don't have themes yet. */
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
