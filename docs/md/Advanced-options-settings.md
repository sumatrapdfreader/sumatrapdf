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

- [keyboard shortcuts](Customize-keyboard-shortcuts.md)
- width of tab with `Tab Width`
- window background color with `FixedPageUI.BackgroundColor`
- color used to highlight text with `FixedPageUI.SelectionColor`
- control scrollbar mode with `FixedPageUI.Scrollbars` (values: `windows`, `smart`, `overlay`, `hidden`)

Advanced settings file also stores the history and state of opened files so that we can e.g. re-open on the page

## Settings

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

; default zoom. valid values: fit page, fit width, fit content or percent like
; 100%
DefaultZoom = fit page

; if true, JavaScript in PDF documents is disabled (e.g. form-field calculations
; won't run) (introduced in version 3.7)
DisableJavaScript = false

; if true, a PDF may load an image stored in a separate file referenced by name
; (an external image stream); the file must sit next to the PDF. Off by default
; for security (matches Acrobat) (introduced in version 3.7)
AllowExternalImages = false

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
LazyLoading = false

; background color of the non-document windows, traditionally yellow
MainWindowBackground = #80fff200

; if true, doesn't open Home tab
NoHomeTab = false

; if true implements pre-3.6 behavior of showing opened files by frequently used
; count. If false, shows most recently opened first
HomePageSortByFrequentlyRead = false

; if true, shows the home page document history as a list instead of thumbnails
HomePageShowList = false

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

; if false, the menu bar will be hidden (use F9 to toggle, persisted across
; sessions) (introduced in version 2.5)
ShowMenubar = true

; if true, show the menu bar when using tabs (useTabs = true) (introduced in
; version 3.7)
ShowMenubarWithTabs = false

; if true, we show tips on the home page (introduced in version 3.7)
ShowTips = true

; up to 13 custom colors for the background color picker, separated by space
; (e.g. '#ff0000 #00ff00 #0000ff') (introduced in version 3.7)
CustomColors =

; if true, we show the toolbar at the top of the window
ShowToolbar = true

; toolbar mode: show (pinned), hide (no toolbar), overlay (toolbar floats over
; the page, sized to its natural width and centered, only shown when the mouse
; is near it). if empty, derived from ShowToolbar (introduced in version 3.7)
Toolbar =

; where the toolbar is placed: top or bottom (applies to both show and overlay
; modes) (introduced in version 3.7)
ToolbarPosition = top

; if true, the find UI is a floating, movable window with a results list instead
; of the compact toolbar overlay (introduced in version 3.7)
SearchUIFloating = false

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

; scrollbar mode: windows (standard Windows scrollbar), smart (overlay scrollbar
; with auto-hide), overlay (always visible overlay scrollbar), hidden (no
; scrollbars) (introduced in version 3.7)
Scrollbars = windows

; if true, we show scrollbar in single page mode (introduced in version 3.6)
ScrollbarInSinglePage = false

; if true, implements smooth scrolling (introduced in version 3.6)
SmoothScroll = false

; how long to hover an internal-document link (in ms) before we show a popup
; rendering the destination region (citation entry, figure, footnote). -1 (the
; default) disables the popup; set a positive value like 300 to enable it
; (introduced in version 3.7)
CitationHoverDelay = -1

; voice id for Read Aloud text-to-speech; empty or unset means system default.
; Voice ids match those used internally by the Read Aloud Voice menu (WinRT
; voice id or SAPI token id) (introduced in version 3.7)
ReadAloudVoiceId =

; playback speed multiplier for Read Aloud text-to-speech (0.5 .. 3.0), 1 is
; normal speed; can also be changed from the Read Aloud playback bar (introduced
; in version 3.7)
ReadAloudSpeed = 1

; if true, mouse wheel scrolling is faster when mouse is over a scrollbar
; (introduced in version 3.6)
FastScrollOverScrollbar = false

; if true, prevents the screen from turning off when in fullscreen or
; presentation mode
PreventSleepInFullscreen = true

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

; if true, disables anti-aliasing for rendering PDF documents (introduced in
; version 3.6)
DisableAntiAlias = false

; if true, disables auto-linking of URLs and email addresses found in PDF text
DisableAutoLinks = false

; if true, we use Windows system colors for background/text color. Over-rides
; other settings
UseSysColors = false

; if true, documents are opened in tabs instead of new windows (introduced in
; version 3.0)
UseTabs = true

; if true, Ctrl+Tab and Ctrl+Shift+Tab show the tab switcher in most recently
; used order instead of tab-strip order
TabsMru = false

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
    ; text). Use an #aarrggbb value to control opacity: a smaller alpha (e.g.
    ; #40ffff00) makes the selection more transparent so the selected text stays
    ; crisp; #rrggbb uses the default opacity (introduced in version 2.4)
    SelectionColor = #ffff00

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

    ; if given, sets the canvas background color for PDF files (introduced in
    ; version 3.7)
    WindowBgCol = 
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

    ; if given, sets the canvas background color for ebook documents (epub, mobi
    ; etc.) (introduced in version 3.7)
    WindowBgCol = 
]

; customization options for Comic Book UI
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

    ; if given, sets the canvas background color for comic book files
    ; (introduced in version 3.7)
    WindowBgCol = 
]

; customization options for image files UI
ImageUI [
    ; if given, sets the canvas background color for image files (introduced in
    ; version 3.7)
    WindowBgCol = 

    ; default zoom for image files. valid values: fit page, fit width, fit
    ; content, shrink to fit or percent like 100% (introduced in version 3.7)
    DefaultZoom = shrink to fit
]

; customization options for CHM UI. If UseFixedPageUI is true, FixedPageUI
; settings apply instead
ChmUI [
    ; if true, the UI used for PDF documents will be used for CHM documents as
    ; well
    UseFixedPageUI = false
]

; settings for the Claude Code chat sidebar (introduced in version 3.7)
ClaudeCode [
    ; Claude model alias for --model (e.g. sonnet, opus, haiku); uses opus if
    ; not in the model list
    Model = sonnet

    ; extra Claude model aliases for the dropdown, comma-separated; sonnet,
    ; opus, and haiku are always included
    Models = 

    ; Claude effort level: 0=Low, 1=Medium, 2=High, 3=Max
    Effort = 1

    ; if true, pass --dangerously-skip-permissions to Claude Code
    SkipPermissions = false

    ; background color of the Claude Code chat panel
    BgColor = #ffffff
]

; settings for the Grok Build chat sidebar (introduced in version 3.7)
GrokBuild [
    ; Grok model ID for --model (e.g. grok-composer-2.5-fast, grok-build)
    Model = grok-composer-2.5-fast

    ; extra Grok model IDs for the dropdown, comma-separated;
    ; grok-composer-2.5-fast and grok-build are always included
    Models = 

    ; Grok effort level: 0=Low, 1=Medium, 2=High, 3=XHigh, 4=Max
    Effort = 1

    ; if true, pass --always-approve to Grok Build (auto-approve tool
    ; executions)
    AlwaysApprove = false

    ; background color of the Grok Build chat panel
    BgColor = #ffffff
]

; settings for the OpenAI Codex chat sidebar (introduced in version 3.7)
CodexBuild [
    ; Codex model ID for -m (e.g. gpt-5.5, gpt-5.4, o3)
    Model = gpt-5.5

    ; extra Codex model IDs for the dropdown, comma-separated; gpt-5.5, gpt-5.4,
    ; and o3 are always included
    Models = 

    ; Codex sandbox mode: 0=read-only, 1=workspace-write, 2=danger-full-access
    Sandbox = 1

    ; if true, pass --dangerously-bypass-approvals-and-sandbox to Codex
    SkipSandbox = false

    ; background color of the OpenAI Codex chat panel
    BgColor = #ffffff
]

; width of the AI chat sidebar (0 = use default); shared by Claude Code, Grok
; Build, and OpenAI Codex (internal) (introduced in version 3.7)
AIChatSidebarDx = 0

; remembered destination language for selection translation; empty uses OS UI
; language (introduced in version 3.7)
TranslateToLang = 

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

    ; text color of free text annotation (introduced in version 3.5)
    FreeTextColor = 

    ; background color of free text annotation (introduced in version 3.6)
    FreeTextBackgroundColor = 

    ; opacity of free text annotation in percent (0-100); 0 - fully transparent
    ; (invisible), 50 - half transparent, 100 - fully opaque (introduced in
    ; version 3.6)
    FreeTextOpacity = 100

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

    ; if given, shows in toolbar (introduced in version 3.7)
    ToolbarText =

    ; optional SVG icon for toolbar button; if both ToolbarSvgIcon and
    ; ToolbarText are set, the icon is used (introduced in version 3.7)
    ToolbarSvgIcon =
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

    ; default value for collate in the print dialog (default, collate,
    ; nocollate)
    Collate = default
]

; options for fullscreen mode (introduced in version 3.7)
Fullscreen [
    ; if true, show the toolbar in fullscreen mode
    ShowToolbar = false

    ; if true, show the menu bar in fullscreen mode
    ShowMenubar = false
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

    ; optional SVG icon for toolbar button; if both ToolbarSvgIcon and
    ; ToolbarText are set, the icon is used (introduced in version 3.7)
    ToolbarSvgIcon =
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

; saved groups of tabs (introduced in version 3.7)
TabGroups [
  [
    ; name of the tab group
    Name = 

    ; files in the tab group
    TabFiles [
      [
        ; file path
        Path = 
      ]
    ]
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

; position/size of the floating find window (see SearchUIFloating)
SearchUIWindowPos = 0 0 0 0

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

    ; if given, overrides the background color for this document (introduced in
    ; version 3.7)
    BgCol = 

    ; if given, overrides the tab color for this document (introduced in version
    ; 3.7)
    TabCol = 

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

; position of the document properties window
PropWinPos = 0 0
```

## Syntax for color values

The syntax for colors is: `#rrggbb` or `#aarrggbb`.

The components are hex values (ranging from 00 to FF) and stand for:
- `aa` : alpha (transparency). ff is fully transparent, 0 is not transparent, and 7f is 50% transparent
- `rr` : red component
- `gg` : green component
- `bb` : blue component

For example #ff0000 means red color. #7fff0000 is half-transparent red.

