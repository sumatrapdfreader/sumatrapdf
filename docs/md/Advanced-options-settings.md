# Advanced options / settings

SumatraPDF has many [advanced settings](https://www.sumatrapdfreader.org/settings/settings) to customize look and behavior.

To open advanced settings file:

- menu `Settings` / `Advanced options...`
- or with Command Palette: `Ctrl + K`, type `adv` to narrow down to command, press `Enter` to select `Advanced Options...` command

This opens a settings text file in default .txt editor. Make changes and save the file.

To reset to default settings, delete settings file. SumatraPDF will re-create it with default values.

Most settings take effect immediately after saving the settings file. Some settings (e.g. `UseTabs`) require closing and re-starting SumatraPDF.

Documentation for all settings is at [https://www.sumatrapdfreader.org/settings/settings](https://www.sumatrapdfreader.org/settings/settings)

Here are some things you can customize:

- [keyboard shortcuts](Customizing-keyboard-shortcuts.md)
- width of tab with `Tab Width`
- window background color with `FixedPageUI.BackgroundColor`
- color used to highlight text with `FixedPageUI.SelectionColor`
- hide scrollbars with `FixedPageUI.HideScrollbars`

Advanced settings file also stores the history and state of opened files so that we can e.g. re-open on the page

# Settings

Below is an explanation of what the different settings mean and what their default values are.

If you add or remove lines with square brackets, **make sure to always add/remove square brackets in pairs**! Else you risk losing all the data following them.

```
; if true, we check once a day if an update is available
CheckForUpdates = true

; actual resolution of the main screen in DPI (if this value isn't positive, the
; system's UI setting is used) (introduced in version 2.5)
CustomScreenDPI = 0

; default layout of pages. valid values: automatic, single page, facing, book
; view, continuous, continuous facing, continuous book view
DefaultDisplayMode = automatic

; default zoom (in %) or one of those values: fit page, fit width, fit content
DefaultZoom = fit page

; if true, we expose the SyncTeX inverse search command line in Settings ->
; Options
EnableTeXEnhancements = false

; if true, Esc key closes SumatraPDF
EscToExit = false

; if true, we show the full path to a file in the title bar (introduced in
; version 3.0)
FullPathInTitle = false

; pattern used to launch the LaTeX editor when doing inverse search
InverseSearchCmdLine =

; when restoring session, delay loading of documents until their tab is selected
; (introduced in version 3.6)
LazyLoading = true

; background color of the non-document windows, traditionally yellow
MainWindowBackground = #80fff200

; if true, doesn't open Home tab
NoHomeTab = false

; if true implements pre-3.6 behavior of showing opened files by frequently used
; count. If false, shows most recently opened first
HomePageSortByFrequentlyRead = false

; if true, a document will be reloaded automatically whenever it's changed
; (currently doesn't work for documents shown in the ebook UI) (introduced in
; version 2.5)
ReloadModifiedDocuments = true

; if true, we remember which files we opened and their display settings
RememberOpenedFiles = true

; if true, we store display settings for each document separately (i.e.
; everything after UseDefaultState in FileStates)
RememberStatePerDocument = true

; if true and SessionData isn't empty, that session will be restored at startup
RestoreSession = true

; if true, we'll always open files using existing SumatraPDF process
ReuseInstance = true

; if false, the menu bar will be hidden for all newly opened windows (use F9 to
; show it until the window closes or Alt to show it just briefly), only applies
; if UseTabs is false (introduced in version 2.5)
ShowMenubar = true

; if true, we show the toolbar at the top of the window
ShowToolbar = true

; if true, we show the Favorites sidebar
ShowFavorites = false

; if true, we show table of contents (Bookmarks) sidebar if it's present in the
; document
ShowToc = true

; if true we draw a blue border around links in the document (introduced in
; version 3.6)
ShowLinks = false

; if true, we show a list of frequently read documents when no document is
; loaded
ShowStartPage = true

; width of favorites/bookmarks sidebar (if shown)
SidebarDx = 0

; if true, we show scrollbar in single page mode (introduced in version 3.6)
ScrollbarInSinglePage = false

; if true, implements smooth scrolling (introduced in version 3.6)
SmoothScroll = false

; if true, mouse wheel scrolling is faster when mouse is over a scrollbar
; (introduced in version 3.6)
FastScrollOverScrollbar = false

; maximum width of a single tab
TabWidth = 300

; Valid themes: light, dark, darker (introduced in version 3.5)
Theme = 

; if both favorites and bookmarks parts of sidebar are visible, this is the
; height of bookmarks (table of contents) part
TocDy = 0

; height of toolbar (introduced in version 3.4)
ToolbarSize = 18

; font name for bookmarks and favorites tree views. automatic means Windows
; default
TreeFontName = automatic

; font size for bookmarks and favorites tree views. 0 means Windows default
; (introduced in version 3.3)
TreeFontSize = 0

; over-ride application font size. 0 means Windows default (introduced in
; version 3.6)
UIFontSize = 0

; if true, we use Windows system colors for background/text color. Over-rides
; other settings
UseSysColors = false

; if true, documents are opened in tabs instead of new windows (introduced in
; version 3.0)
UseTabs = true

; sequence of zoom levels when zooming in/out; all values must lie between 8.33
; and 6400
ZoomLevels = 

; zoom step size in percents relative to the current zoom level. if zero or
; negative, the values from ZoomLevels are used instead
ZoomIncrement = 0

; customization options for PDF, XPS, DjVu and PostScript UI
FixedPageUI [
    ; color value with which black (text) will be substituted
    TextColor = #000000

    ; color value with which white (background) will be substituted
    BackgroundColor = #ffffff

    ; color value for the text selection rectangle (also used to highlight found
    ; text) (introduced in version 2.4)
    SelectionColor = #f5fc0c

    ; top, right, bottom and left margin (in that order) between window and
    ; document
    WindowMargin = 2 4 2 4

    ; horizontal and vertical distance between two pages in facing and book view
    ; modes
    PageSpacing = 4 4

    ; colors to use for the gradient from top to bottom (stops will be inserted
    ; at regular intervals throughout the document); currently only up to three
    ; colors are supported; the idea behind this experimental feature is that
    ; the background might allow to subconsciously determine reading progress;
    ; suggested values: #2828aa #28aa28 #aa2828
    GradientColors =

    ; if true, TextColor and BackgroundColor of the document will be swapped
    InvertColors = false

    ; if true, hides the scrollbars but retains ability to scroll
    HideScrollbars = false
]

; customization options for eBookUI
EBookUI [
    ; font size, default 8.0
    FontSize = 0

    ; default is 420
    LayoutDx = 0

    ; default is 595
    LayoutDy = 0

    ; if true, we ignore ebook's CSS
    IgnoreDocumentCSS = false

    ; custom CSS. Might need to set IgnoreDocumentCSS = true
    CustomCSS =
]

; customization options for Comic Book and images UI
ComicBookUI [
    ; top, right, bottom and left margin (in that order) between window and
    ; document
    WindowMargin = 0 0 0 0

    ; horizontal and vertical distance between two pages in facing and book view
    ; modes
    PageSpacing = 4 4

    ; if true, default to displaying Comic Book files in manga mode (from right
    ; to left if showing 2 pages at a time)
    CbxMangaMode = false
]

; customization options for CHM UI. If UseFixedPageUI is true, FixedPageUI
; settings apply instead
ChmUI [
    ; if true, the UI used for PDF documents will be used for CHM documents as
    ; well
    UseFixedPageUI = false
]

; default values for annotations in PDF documents (introduced in version 3.3)
Annotations [
    ; highlight annotation color
    HighlightColor = #ffff00

    ; underline annotation color
    UnderlineColor = #00ff00

    ; squiggly annotation color (introduced in version 3.5)
    SquigglyColor = #ff00ff

    ; strike out annotation color (introduced in version 3.5)
    StrikeOutColor = #ff0000

    ; color of free text annotation (introduced in version 3.5)
    FreeTextColor = 

    ; size of free text annotation (introduced in version 3.5)
    FreeTextSize = 12

    ; width of free text annotation border (introduced in version 3.5)
    FreeTextBorderWidth = 1

    ; text icon annotation color
    TextIconColor = 

    ; type of text annotation icon: comment, help, insert, key, new paragraph,
    ; note, paragraph. If not set: note.
    TextIconType = 

    ; default author for created annotations, use (none) to not add an author at
    ; all. If not set will use Windows user name (introduced in version 3.4)
    DefaultAuthor = 
]

; list of additional external viewers for various file types. See docs for more information (https://www.sumatrapdfreader.org/docs/Customize-external-viewers)
ExternalViewers [
  [
    ; command line with which to call the external viewer, may contain %p for
    ; page number and "%1" for the file name (add quotation marks around paths
    ; containing spaces)
    CommandLine =

    ; name of the external viewer to be shown in the menu (implied by
    ; CommandLine if missing)
    Name =

    ; optional filter for which file types the menu item is to be shown;
    ; separate multiple entries using ';' and don't include any spaces (e.g.
    ; *.pdf;*.xps for all PDF and XPS documents)
    Filter =

    ; optional: keyboard shortcut e.g. Alt + 7 (introduced in version 3.6)
    Key =
  ]
]

; customization options for how we show forward search results (used from LaTeX
; editors)
ForwardSearch [
    ; when set to a positive value, the forward search highlight style will be
    ; changed to a rectangle at the left of the page (with the indicated amount
    ; of margin from the page margin)
    HighlightOffset = 0

    ; width of the highlight rectangle (if HighlightOffset is > 0)
    HighlightWidth = 15

    ; color used for the forward search highlight
    HighlightColor = #6581ff

    ; if true, highlight remains visible until the next mouse click (instead of
    ; fading away immediately)
    HighlightPermanent = false
]

; these override the default settings in the Print dialog
PrinterDefaults [
    ; default value for scaling (shrink, fit, none)
    PrintScale = shrink
]

; list of handlers for selected text, shown in context menu when text selection
; is active. See docs for more information (https://www.sumatrapdfreader.org/docs/Customize-search-translation-services)
SelectionHandlers [
  [
    ; url to invoke for the selection. ${selection} will be replaced with
    ; current selection and ${userlang} with language code for current UI (e.g.
    ; 'de' for German)
    URL =

    ; name shown in context menu
    Name =

    ; keyboard shortcut (introduced in version 3.6)
    Key =
  ]
]

; custom keyboard shortcuts
Shortcuts [
  [
    ; command
    Cmd = 

    ; keyboard shortcut (e.g. Ctrl-Alt-F)
    Key = 

    ; name shown in command palette (introduced in version 3.6)
    Name =

    ; if given, shows in toolbar (introduced in version 3.6)
    ToolbarText =
  ]
]

; color themes (introduced in version 3.6)
Themes [
  [
    ; name of the theme
    Name = 

    ; text color
    TextColor = 

    ; background color
    BackgroundColor = 

    ; control background color
    ControlBackgroundColor = 

    ; link color
    LinkColor = 

    ; should we colorize Windows controls and window areas
    ColorizeControls = false
  ]
]

; a whitespace separated list of passwords to try when opening a password
; protected document (passwords containing spaces must be quoted) (introduced in
; version 2.4)
DefaultPasswords =

; ISO code (langs.html) of the current UI language
UiLanguage =

; we won't ask again to update to this version
VersionToSkip =

; default state of the window. 1 is normal, 2 is maximized, 3 is fullscreen, 4
; is minimized
WindowState = 1

; default position (x, y) and size (width, height) of the window
WindowPos = 0 0 0 0

; information about opened files (in most recently used order)
FileStates [
  [
    ; path of the document
    FilePath =

    ; Values which are persisted for bookmarks/favorites
    Favorites [
      [
        ; name of this favorite as shown in the menu
        Name =

        ; number of the bookmarked page
        PageNo = 0

        ; label for this page (only present if logical and physical page numbers
        ; are not the same)
        PageLabel =
      ]
    ]

    ; a document can be "pinned" to the Frequently Read list so that it isn't
    ; displaced by recently opened documents
    IsPinned = false

    ; if true, the file is considered missing and won't be shown in any list
    IsMissing = false

    ; number of times this document has been opened recently
    OpenCount = 0

    ; data required to open a password protected document without having to ask
    ; for the password again
    DecryptionKey =

    ; if true, we use global defaults when opening this file (instead of the
    ; values below)
    UseDefaultState = false

    ; layout of pages. valid values: automatic, single page, facing, book view,
    ; continuous, continuous facing, continuous book view
    DisplayMode = automatic

    ; how far this document has been scrolled (in x and y direction)
    ScrollPos = 0 0

    ; number of the last read page
    PageNo = 1

    ; zoom (in %) or one of those values: fit page, fit width, fit content
    Zoom = fit page

    ; how far pages have been rotated as a multiple of 90 degrees
    Rotation = 0

    ; state of the window. 1 is normal, 2 is maximized, 3 is fullscreen, 4 is
    ; minimized
    WindowState = 0

    ; default position (can be on any monitor)
    WindowPos = 0 0 0 0

    ; if true, we show table of contents (Bookmarks) sidebar if it's present in
    ; the document
    ShowToc = true

    ; width of the left sidebar panel containing the table of contents
    SidebarDx = 0

    ; if true, the document is displayed right-to-left in facing and book view
    ; modes (only used for comic book documents)
    DisplayR2L = false

    ; data required to restore the last read page in the ebook UI
    ReparseIdx = 0

    ; data required to determine which parts of the table of contents have been
    ; expanded
    TocState =
  ]
]

; state of the last session, usage depends on RestoreSession (introduced in
; version 3.1)
SessionData [
  [
    ; data required for restoring the view state of a single tab
    TabStates [
      [
        ; path of the document
        FilePath =

        ; same as FileStates -> DisplayMode
        DisplayMode = automatic

        ; number of the last read page
        PageNo = 1

        ; same as FileStates -> Zoom
        Zoom = fit page

        ; same as FileStates -> Rotation
        Rotation = 0

        ; how far this document has been scrolled (in x and y direction)
        ScrollPos = 0 0

        ; if true, the table of contents was shown when the document was closed
        ShowToc = true

        ; same as FileStates -> TocState
        TocState =
      ]
    ]

    ; index of the currently selected tab (1-based)
    TabIndex = 1

    ; same as FileState -> WindowState
    WindowState = 0

    ; default position (can be on any monitor)
    WindowPos = 0 0 0 0

    ; width of favorites/bookmarks sidebar (if shown)
    SidebarDx = 0
  ]
]

; data required for reloading documents after an auto-update (introduced in
; version 3.0)
ReopenOnce =

; data required to determine when SumatraPDF last checked for updates
TimeOfLastUpdateCheck = 0 0

; value required to determine recency for the OpenCount value in FileStates
OpenCountWeek = 0
```

## Syntax for color values

The syntax for colors is: `#rrggbb` or `#rrggbb`.

The components are hex values (ranging from 00 to FF) and stand for:
- `rr` : red component
- `gg` : green component
- `bb` : blue component
- `aa` : alpha (transparency) component

For example #ff0000 means red color. #ff00007f is half-transparent red.

