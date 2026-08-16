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
; default layout of pages. valid values: automatic, single page, facing, book
; view, continuous, continuous facing, continuous book view, page aspect. page
; aspect (3.7+): first open of a PDF, XPS, DjVu or PostScript file uses page 1 —
; taller than wide is continuous + fit width, wider than tall is single page +
; fit page; a remembered FileState still wins
DefaultDisplayMode = automatic

; default zoom. valid values: fit page, fit width, fit height, fit content or
; percent like 100%
DefaultZoom = fit page

; if true, JavaScript in PDF documents is disabled (e.g. form-field calculations
; won't run) (introduced in version 3.7)
DisableJavaScript = false

; if true, a PDF may load an image stored in a separate file referenced by name
; (an external image stream); the file must sit next to the PDF. Off by default
; for security (matches Acrobat) (introduced in version 3.7)
AllowExternalImages = false

; if true, show the SyncTeX inverse search command line in Settings -> Options,
; so a double-click in the document can jump to the matching line in a LaTeX
; editor
EnableTeXEnhancements = false

; if true, Esc key closes SumatraPDF
EscToExit = false

; if true, show the full path to the document in the title bar (introduced in
; version 3.0)
FullPathInTitle = false

; pattern used to launch the LaTeX editor when doing inverse search
InverseSearchCmdLine =

; if true, restoring a session delays loading each document until its tab is
; selected (introduced in version 3.6)
LazyLoading = false

; background color of the area around the document, traditionally yellow. Only
; applies to the Light theme; the default #80fff200 is a marker meaning "use the
; theme's color", so setting any other value also colorizes the toolbar and
; sidebars
MainWindowBackground = #80fff200

; if true, doesn't open Home tab
NoHomeTab = false

; if true, the home page lists documents by how often they've been opened (the
; pre-3.6 behavior); if false, the most recently opened come first
HomePageSortByFrequentlyRead = false

; valid values: thumbnails, list (introduced in version 3.7)
HomePageViewMode = thumbnails

; valid values: (empty), os, sumatrapdf (introduced in version 3.7)
FilePicker = 

; if true, a document will be reloaded automatically whenever it's changed
; (currently doesn't work for documents shown in the ebook UI) (introduced in
; version 2.5)
ReloadModifiedDocuments = true

; if true, remember which documents were opened and their display settings
RememberOpenedFiles = true

; if true, store display settings for each document separately (i.e. everything
; after UseDefaultState in FileStates)
RememberStatePerDocument = true

; if true and SessionData isn't empty, that session will be restored at startup
RestoreSession = true

; if true, open documents in the already running SumatraPDF instead of starting
; a new one
ReuseInstance = true

; if true, show the menu bar (F9 toggles it; the choice is remembered across
; sessions) (introduced in version 2.5)
ShowMenubar = true

; if true, show the menu bar when using tabs (useTabs = true) (introduced in
; version 3.7)
ShowMenubarWithTabs = false

; if true, show tips on the home page (introduced in version 3.7)
ShowTips = true

; up to 13 custom colors for the background color picker, separated by space
; (e.g. '#ff0000 #00ff00 #0000ff') (introduced in version 3.7)
CustomColors =

; legacy bool for toolbar; if Toolbar is empty, derived as show/hide (internal;
; use Toolbar instead)
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

; if true, show the Favorites sidebar
ShowFavorites = false

; if true, favorites within each file are sorted alphabetically by name (or page
; label); if false (the default), they are sorted by page number (introduced in
; version 3.7)
SortFavoritesByName = false

; if true, show the table of contents (Bookmarks) sidebar when the document has
; one
ShowToc = true

; if true, put the bookmarks / favorites sidebar on the right of the window
; (left is the default; right-to-left UI languages already put it on the right)
; (introduced in version 3.7)
SidebarOnRight = false

; if true, draw a blue border around links in the document (introduced in
; version 3.6)
ShowLinks = false

; if true, a click (not a drag) on the left fifth of the page area goes to the
; previous page and a click on the right fifth goes to the next page (reversed
; in manga / right-to-left mode). Links, annotations and presentation-mode
; clicks are unchanged (introduced in version 3.7)
ClickEdgeToTurnPage = false

; if true, document links are ignored so you can select and read (useful for
; drawings with many links); if false, clicking a link follows it (introduced in
; version 3.7)
DisableLinks = false

; if true, next/previous page keeps the same view position on the page instead
; of jumping to the top (useful when zoomed in on similarly sized pages)
; (introduced in version 3.7)
RememberViewOffsetOnPageTurn = false

; if true, draw a focus ring around the document when it has keyboard focus (Tab
; to the page area) (introduced in version 3.7)
ShowDocumentFocusIndicator = false

; if true, show a tip when hovering an annotation (e.g. "Highlight annotation.
; Ctrl+click to edit.") (introduced in version 3.7)
ShowAnnotationNotification = true

; if true, show page numbers (labels) right-aligned on bookmark /
; table-of-contents entries (introduced in version 3.7)
ShowTocPageNumbers = true

; if true, show a list of frequently read documents when no document is loaded
ShowStartPage = true

; width of the favorites / bookmarks sidebar in screen pixels, as last resized
; (0 means the default)
SidebarDx = 0

; scrollbar mode: windows (standard Windows scrollbar), smart (overlay scrollbar
; with auto-hide), overlay (always visible overlay scrollbar), hidden (no
; scrollbars) (introduced in version 3.7)
Scrollbars = windows

; if true, show a scrollbar in single page mode as well (introduced in version
; 3.6)
ScrollbarInSinglePage = false

; if true, smooth mouse-wheel scrolling (exponential chase of the target;
; continuous wheel input stays fluid) (introduced in version 3.6)
SmoothScroll = true

; distance, in screen pixels at 96 DPI, scrolled by an arrow-key press or one
; mouse-wheel line; values below 1 use 16 (introduced in version 3.7)
ScrollLineAmount = 16

; if true, continuous view has extra scroll room after the last page so you can
; scroll the end of the document to the top of the window (introduced in version
; 3.7)
PaddingAfterLastPage = false

; if true, going to a destination (clicking a bookmark or a link inside the
; document) keeps the current zoom instead of applying the zoom the destination
; asks for; it still goes to the page and the position. Same as Adobe Reader's
; 'forbid the change of the current zoom factor during execution of Go to
; Destination actions' (introduced in version 3.7)
IgnoreDestinationZoom = false

; how long an internal-document link has to be hovered, in milliseconds, before
; a popup rendering the destination region (citation entry, figure, footnote)
; appears. -1 (the default) disables the popup; set a positive value like 300 to
; enable it (introduced in version 3.7)
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

; maximum width of a single tab, in pixels at 100% display scaling (at least 60)
TabWidth = 300

; valid themes: Light, Dark, Light Warm, Dark from 3.5, Charcoal, Solarized
; Light, Solarized Dark, Dracula, Nebula, Greeny, Choco, Purpy, One Dark,
; Monokai, Nord, GitHub Dark, Catppuccin Mocha, Tokyo Night, Gruvbox, Night Owl,
; Ayu, Palenight, System (introduced in version 3.5)
Theme = 

; the light theme the light/dark toggle and the System theme switch to
; (introduced in version 3.7)
LastLightTheme = 

; the dark theme the light/dark toggle and the System theme switch to
; (introduced in version 3.7)
LastDarkTheme = 

; how MuPDF-rendered documents (PDF, XPS, DjVu, EPUB, MOBI, FB2, CBZ, images,
; etc.) use UI / FixedPageUI colors for the page. Values: off (document's own
; colors; default); smart (recolor text and page background, keep photos/images
; as-is — best for dark reading); legacy (also recolor images; pre-3.7
; invert-style). Does not change menus/toolbars — use Theme for UI chrome.
; Settings / Theme and the CmdSetDocumentColorsFollowTheme command set all three
; values. Shift+I (Invert Colors) is separate: it swaps the page colors for the
; session whatever this is set to (introduced in version 3.7)
DocumentColorsFollowTheme = off

; if both the favorites and the bookmarks part of the sidebar are visible, this
; is the height of the bookmarks (table of contents) part, in screen pixels
TocDy = 0

; the toolbar's built-in buttons, in the order you want them, e.g. CmdOpenFile
; CmdPrint PageInfo | CmdFindFirst. Leave a button out to hide it. | is a
; separator and PageInfo is the page number box. Empty (the default) means the
; standard layout. Buttons you added yourself (see Shortcuts) still come last
; (introduced in version 3.7)
ToolbarCustomLayout = 

; if true, the toolbar has a Read Aloud button (with a drop-down for voice,
; speed and what to read). Read Aloud is still reachable from the Read Aloud
; menu when this is false (introduced in version 3.7)
ToolbarShowReadAloud = false

; size of the toolbar icons in pixels at 100% display scaling (8-64); the
; toolbar itself is a few pixels taller (introduced in version 3.4)
ToolbarSize = 18

; font name for bookmarks and favorites tree views. automatic means Windows
; default
TreeFontName = automatic

; font size for bookmarks and favorites tree views, in pixels; 0 means the
; Windows default. Not scaled by the display scaling (introduced in version 3.3)
TreeFontSize = 0

; overrides the font size used for menus, toolbar and dialogs, in pixels; 0
; means the Windows default. Not scaled by the display scaling (introduced in
; version 3.6)
UIFontSize = 0

; if true, render MuPDF-based documents (PDF, XPS, DjVu, EPUB etc.) without
; anti-aliasing, giving sharper but jagged edges (introduced in version 3.6)
DisableAntiAlias = false

; CAD/engineering PDF line rendering: off, auto (enhance if a CAD drawing is
; detected) or on (introduced in version 3.7)
EngineeringDrawingEnhance = auto

; if true, disables auto-linking of URLs and email addresses found in PDF text
; (introduced in version 3.7)
DisableAutoLinks = false

; if true, use the Windows system colors for the document background and text.
; Overrides other color settings
UseSysColors = false

; if true, documents are opened in tabs instead of new windows (introduced in
; version 3.0)
UseTabs = true

; if true, a small floating toolbar with selection actions (copy, read aloud,
; highlight etc.) pops up after selecting text. Set to false to disable it
; (introduced in version 3.7)
SelectionToolbar = true

; if true, Ctrl+Tab and Ctrl+Shift+Tab show the tab switcher in most recently
; used order instead of tab-strip order (introduced in version 3.7)
TabsMru = false

; if true, Ctrl+Tab and Ctrl+Shift+Tab immediately switch to the next / previous
; tab in tab-strip order (the behavior before version 3.6) instead of showing
; the tab switcher (introduced in version 3.7)
CtrlTabSimple = false

; sequence of zoom levels when zooming in/out; values must lie between 8.33 and
; 1000000 (the largest one becomes the maximum zoom, which is 6400 by default)
ZoomLevels = 

; how much a single zoom in / zoom out step changes the zoom, as a percentage of
; the current zoom level. If 0 or negative, zooming steps through ZoomLevels
; instead
ZoomIncrement = 0

; customization options for PDF, XPS, DjVu and PostScript UI
FixedPageUI [
    ; color used instead of black for the document's text
    TextColor = #000000

    ; color used instead of white for the document's page background
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

    ; experimental: instead of a single background color, fade through these
    ; colors from the top of the document to the bottom (stops are spread
    ; evenly, at most 3 colors). The shifting background is meant to give a
    ; subconscious sense of reading progress. Suggested values: #2828aa #28aa28
    ; #aa2828
    GradientColors =

    ; if given, sets the canvas background color for PDF files (introduced in
    ; version 3.7)
    WindowBgCol = 
]

; customization options for the ebook UI (EPUB, MOBI, FB2, PDB and plain text)
EBookUI [
    ; default font family for ebooks (e.g. Segoe UI, Georgia, Microsoft YaHei).
    ; empty uses the engine default (typically a serif). applied as user CSS
    ; with !important, which beats the document's own font-family even when it
    ; comes from an inline style attribute; leave empty to keep the publisher's
    ; fonts. wrapping quotes are stripped. a name that can't be loaded is
    ; reported with a notification when the document opens (introduced in
    ; version 3.7)
    FontName = 

    ; font size in points; 0 means the default (8.0)
    FontSize = 0

    ; white space around the text, in points (not screen pixels), like LayoutDx.
    ; one number sets all four sides, two are top/bottom and left/right, four
    ; are top, right, bottom, left - the same order as in CSS. empty keeps the
    ; default (3 em above and below, 2 em left and right, so it follows the font
    ; size); 0 leaves no margin at all. each value can be up to 200 (introduced
    ; in version 3.7)
    Margin =

    ; line-height multiplier for ebook text (e.g. 1.5); 0 keeps the document or
    ; engine default. values from 0.5 to 5 are accepted (introduced in version
    ; 3.7)
    LineSpacing = 0

    ; width of the page the ebook is laid out into, in points (not screen
    ; pixels); 0 means the default (420)
    LayoutDx = 0

    ; height of the page the ebook is laid out into, in points (not screen
    ; pixels); 0 derives it from the window's shape when the document is opened,
    ; so Fit Width shows a whole page
    LayoutDy = 0

    ; if true, the CSS in the ebook is ignored and only CustomCSS applies
    IgnoreDocumentCSS = false

    ; additional CSS applied to ebooks; set IgnoreDocumentCSS = true if the
    ; document's own CSS overrides it
    CustomCSS =

    ; if given, sets the canvas background color for ebook documents (epub, mobi
    ; etc.) (introduced in version 3.7)
    WindowBgCol = 

    ; default page layout for ebooks; empty uses the global DefaultDisplayMode.
    ; valid values: automatic, single page, facing, book view, continuous,
    ; continuous facing, continuous book view (introduced in version 3.7)
    DefaultDisplayMode = 
]

; customization options for Comic Book UI
ComicBookUI [
    ; top, right, bottom and left margin (in that order) between window and
    ; document
    WindowMargin = 0 0 0 0

    ; horizontal and vertical distance between two pages in facing and book view
    ; modes
    PageSpacing = 4 4

    ; if true, documents that don't state their own reading direction default to
    ; manga mode, i.e. right to left. A document that states a direction (e.g.
    ; an EPUB with page-progression-direction) is shown the way it asks for
    CbxMangaMode = false

    ; if given, sets the canvas background color for comic book files
    ; (introduced in version 3.7)
    WindowBgCol = 

    ; if true, absolute zoom never makes a page wider than the window (each page
    ; is capped at Fit Width). Useful for comics/manga with double-page spreads
    ; that are much wider than regular pages (issue #2197) (introduced in
    ; version 3.7)
    LimitToWindowWidth = false

    ; if true, absolute zoom never makes a page taller than the window (each
    ; page is capped at Fit Height) (introduced in version 3.7)
    LimitToWindowHeight = false

    ; default page layout for comic books; empty uses the global
    ; DefaultDisplayMode. valid values: automatic, single page, facing, book
    ; view, continuous, continuous facing, continuous book view (introduced in
    ; version 3.7)
    DefaultDisplayMode = 

    ; default zoom for comic books; empty uses the global DefaultZoom. valid
    ; values: fit page, fit width, fit height, fit content, shrink to fit or
    ; percent like 100% (introduced in version 3.7)
    DefaultZoom = 
]

; customization options for image files UI
ImageUI [
    ; if given, sets the canvas background color for image files (introduced in
    ; version 3.7)
    WindowBgCol = 

    ; default zoom for image files. valid values: fit page, fit width, fit
    ; height, fit content, shrink to fit or percent like 100% (introduced in
    ; version 3.7)
    DefaultZoom = shrink to fit

    ; if true, absolute zoom never makes a page wider than the window (each page
    ; is capped at Fit Width). Useful for image folders with mixed aspect ratios
    ; (issue #2197) (introduced in version 3.7)
    LimitToWindowWidth = false

    ; if true, absolute zoom never makes a page taller than the window (each
    ; page is capped at Fit Height) (introduced in version 3.7)
    LimitToWindowHeight = false
]

; customization options for CHM UI. UseFixedPageUI switches to the PDF-style
; view; FontName applies to that view
ChmUI [
    ; if true, the UI used for PDF documents will be used for CHM documents as
    ; well
    UseFixedPageUI = false

    ; font family for the CHM fixed-page view (e.g. Segoe UI, Georgia, Microsoft
    ; YaHei). empty uses EBookUI.FontName or the engine default. overrides fonts
    ; specified by the document; wrapping quotes are stripped (introduced in
    ; version 3.7)
    FontName =
]

; customization options for Markdown UI. If UseFixedPageUI is true, MuPDF is
; used; otherwise WebView2 browser view is used when available
MarkdownUI [
    ; if true, use MuPDF (cmark-gfm) to render markdown; if false, use WebView2
    ; browser view when available
    UseFixedPageUI = false
]

; customization options for HTML UI. If UseFixedPageUI is true, MuPDF is used;
; otherwise WebView2 browser view is used when available (introduced in version
; 3.7)
HtmlUI [
    ; if true, use MuPDF to render HTML; if false, use WebView2 browser view
    ; when available
    UseFixedPageUI = false
]

; settings for the Claude Code chat sidebar (introduced in version 3.7)
ClaudeCode [
    ; Claude model alias for --model (e.g. sonnet, opus, haiku); uses opus if
    ; not in the model list
    Model = sonnet

    ; extra Claude model aliases for the dropdown, comma-separated; documented
    ; Claude Code aliases are always included
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
    ; Grok model ID for --model (e.g. grok-4.5)
    Model = grok-4.5

    ; extra Grok model IDs for the dropdown, comma-separated; used in addition
    ; to models reported by Grok
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

    ; extra Codex model IDs for the dropdown, comma-separated; used in addition
    ; to models reported by Codex
    Models = 

    ; Codex sandbox mode: 0=read-only, 1=workspace-write, 2=danger-full-access
    Sandbox = 1

    ; if true, pass --dangerously-bypass-approvals-and-sandbox to Codex
    SkipSandbox = false

    ; background color of the OpenAI Codex chat panel
    BgColor = #ffffff
]

; settings for the Antigravity chat sidebar (introduced in version 3.7)
AntiGravity [
    ; Antigravity model ID for --model (e.g. gemini-3.6-flash)
    Model = gemini-3.6-flash

    ; extra Antigravity model IDs for the dropdown, comma-separated
    Models = 

    ; Antigravity effort level: 0=Low, 1=Medium, 2=High, 3=Max
    Effort = 1

    ; if true, pass --dangerously-skip-permissions to Antigravity CLI so it can
    ; read the current file etc. in headless print mode (agy cannot prompt for
    ; permissions with -p)
    AutoApprove = true

    ; background color of the Antigravity chat panel
    BgColor = #ffffff
]

; width of the AI chat sidebar (0 = use default); shared by Claude Code, Grok
; Build, and OpenAI Codex (internal) (introduced in version 3.7)
AIChatSidebarDx = 0

; remembered destination language for selection translation; empty uses OS UI
; language (introduced in version 3.7)
TranslateToLang = 

; remembered source language for selection translation; empty means Auto
; (introduced in version 3.7)
TranslateFromLang = 

; remembered engine for Translate Selection: Google, DeepL, Grok Build, Claude
; Code, OpenAI Codex or Antigravity (introduced in version 3.7)
TranslateEngine = 

; default values for annotations in PDF documents (introduced in version 3.3)
Annotations [
    ; color of newly created highlight annotations. Use an #aarrggbb value to
    ; set default opacity (00 = transparent, FF = opaque); #rrggbb is fully
    ; opaque
    HighlightColor = #ffff00

    ; color of newly created underline annotations. #aarrggbb sets default
    ; opacity the same way as HighlightColor
    UnderlineColor = #00ff00

    ; color of newly created squiggly underline annotations. #aarrggbb sets
    ; default opacity the same way as HighlightColor (introduced in version 3.5)
    SquigglyColor = #ff00ff

    ; color of newly created strike out annotations. #aarrggbb sets default
    ; opacity the same way as HighlightColor (introduced in version 3.5)
    StrikeOutColor = #ff0000

    ; text color of newly created free text annotations (introduced in version
    ; 3.5)
    FreeTextColor = 

    ; background color of newly created free text annotations (introduced in
    ; version 3.6)
    FreeTextBackgroundColor = 

    ; opacity of free text annotation in percent (0-100); 0 - fully transparent
    ; (invisible), 50 - half transparent, 100 - fully opaque (introduced in
    ; version 3.6)
    FreeTextOpacity = 100

    ; font size of free text annotations, in points (introduced in version 3.5)
    FreeTextSize = 12

    ; border width of free text annotations, in points (introduced in version
    ; 3.5)
    FreeTextBorderWidth = 1

    ; how text is aligned in newly created free text annotations (Text Alignment
    ; in the annotation editor): left, center or right. Right-to-left scripts
    ; (Arabic, Hebrew, Persian) want right (introduced in version 3.7)
    FreeTextAlignment = left

    ; color of newly created text (sticky note) annotations
    TextIconColor = 

    ; icon shown for text (sticky note) annotations: comment, help, insert, key,
    ; new paragraph, note or paragraph. If not set, note is used
    TextIconType = 

    ; author recorded on newly created annotations. If not set, the Windows user
    ; name is used; set it to (none) to leave the author out entirely
    ; (introduced in version 3.4)
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

; customization options for how forward search results are shown (used from
; LaTeX editors)
ForwardSearch [
    ; if greater than 0, the forward search result is marked with a bar down the
    ; left side of the page instead of highlighting the matched text. The value
    ; is how far the bar sits from the left edge of the page, in document units
    HighlightOffset = 0

    ; width of that bar in document units (only used when HighlightOffset is
    ; greater than 0)
    HighlightWidth = 15

    ; color used for the forward search highlight
    HighlightColor = #6581ff

    ; if true, the highlight stays visible until the next mouse click instead of
    ; fading away after a few seconds
    HighlightPermanent = false
]

; these override the default settings in the Print dialog
PrinterDefaults [
    ; default value for scaling (shrink, fit, none)
    PrintScale = shrink

    ; default value for collate in the print dialog (default, collate,
    ; nocollate) (introduced in version 3.7)
    Collate = default
]

; options for fullscreen mode (introduced in version 3.7)
Fullscreen [
    ; legacy bool for fullscreen toolbar; if Fullscreen.Toolbar is empty,
    ; derived as show/hide (internal; use Fullscreen.Toolbar instead)
    ShowToolbar = false

    ; toolbar mode in fullscreen: show (pinned), hide (no toolbar), overlay
    ; (toolbar floats over the page, only shown when the mouse is near it). if
    ; empty, derived from Fullscreen.ShowToolbar (introduced in version 3.7)
    Toolbar =

    ; if true, show the menu bar in fullscreen mode
    ShowMenubar = false

    ; page layout in presentation (Ctrl+L) and windowed fullscreen (Shift+Ctrl+L
    ; / F11). empty keeps the current behavior: presentation uses single page,
    ; windowed fullscreen keeps the existing layout. valid values: automatic,
    ; single page, facing, book view, continuous, continuous facing, continuous
    ; book view (introduced in version 3.7)
    DisplayMode = 
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

    ; command line of a program to run instead of opening a URL. Use
    ; ${selectionfile} to pass the selection as a temporary utf-8 file, which
    ; has no length limit. If set, URL is ignored (introduced in version 3.7)
    Exe =

    ; how to send the selection. GET (default) puts it in the URL, which limits
    ; how much text fits. POST sends it in the request body with no length limit
    ; and shows the response. POST-VIA-BROWSER submits a form from your browser
    ; instead, so the service sees your normal browser session (cookies, logins)
    ; (introduced in version 3.7)
    Method = GET

    ; request body for POST / POST-VIA-BROWSER; the same ${selection},
    ; ${selectionjson} and ${userlang} substitutions apply. If unset, the body
    ; is the raw selection (introduced in version 3.7)
    Body =

    ; value of the Content-Type header for POST. Defaults to 'text/plain;
    ; charset=utf-8', which matches the default raw-selection body. Use
    ; 'application/json' or 'application/x-www-form-urlencoded' when Body is in
    ; that format. Ignored by POST-VIA-BROWSER, which always submits a form
    ; (introduced in version 3.7)
    ContentType =

    ; extra HTTP headers for POST, one per line as 'Name: value' (use \n to
    ; separate them in this file). Needed for services that authenticate with an
    ; api key, e.g. 'Authorization: Bearer sk-...'. Ignored by POST-VIA-BROWSER.
    ; Note that anything you put here is stored in plain text in this settings
    ; file (introduced in version 3.7)
    Headers =

    ; if set, the handler also gets a button in the toolbar that pops up over a
    ; text selection. The value is the button's text, or, if it starts with
    ; '<svg', an icon to draw instead (introduced in version 3.7)
    SelectToolbarNameOrSvg =
  ]
]

; custom keyboard shortcuts
Shortcuts [
  [
    ; command to run, e.g. CmdOpenFile. See the list of commands (https://www.sumatrapdfreader.org/docs/Commands)
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
    ; name of the theme, as shown in the Settings / Theme menu
    Name = 

    ; color of text in menus, toolbar, tabs and sidebars
    TextColor = 

    ; background color of the window around the document
    BackgroundColor = 

    ; background color of toolbar, tabs, sidebars and dialogs
    ControlBackgroundColor = 

    ; color of clickable links in the UI
    LinkColor = 

    ; color of disabled (grayed out) text; if empty, derived from the colors
    ; above (introduced in version 3.7)
    DisabledTextColor = 

    ; color of secondary / muted text like the page label; if empty, derived
    ; from the colors above (introduced in version 3.7)
    DarkerTextColor = 

    ; background color of a control the mouse is over; if empty, derived from
    ; the colors above (introduced in version 3.7)
    HotBackgroundColor = 

    ; color of control borders and separators; if empty, derived from the colors
    ; above (introduced in version 3.7)
    EdgeColor = 

    ; border color of a control the mouse is over; if empty, derived from the
    ; colors above (introduced in version 3.7)
    HotEdgeColor = 

    ; border color of a disabled control; if empty, derived from the colors
    ; above (introduced in version 3.7)
    DisabledEdgeColor = 

    ; background color of error messages; if empty, derived from the colors
    ; above (introduced in version 3.7)
    ErrorBackgroundColor = 

    ; background color of notification tips; if empty, derived from the colors
    ; above (introduced in version 3.7)
    NotificationBackgroundColor = 

    ; background color of a highlighted notification tip; if empty, derived from
    ; the colors above (introduced in version 3.7)
    NotificationHighlightColor = 

    ; text color of a highlighted notification tip; if empty, derived from the
    ; colors above (introduced in version 3.7)
    NotificationHighlightTextColor = 

    ; if true, apply the theme colors to Windows controls and window areas too
    ColorizeControls = false
  ]
]

; saved groups of tabs (introduced in version 3.7)
TabGroups [
  [
    ; name of the tab group, as shown when restoring it
    Name = 

    ; documents that belong to this tab group
    TabFiles [
      [
        ; path of the document
        Path = 
      ]
    ]
  ]
]

; actual resolution of the main screen in DPI, used to show documents at their
; physical size; if 0 or negative, the resolution reported by Windows is used
; (introduced in version 2.5)
CustomScreenDPI = 0

; a whitespace separated list of passwords to try when opening a password
; protected document (passwords containing spaces must be quoted) (introduced in
; version 2.4)
DefaultPasswords =

; ISO code (langs.html) of the current UI language
UiLanguage =

; SumatraPDF won't offer to update to this version again
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

    ; pages of this document bookmarked in the Favorites menu
    Favorites [
      [
        ; name of this favorite as shown in the menu
        Name =

        ; number of the bookmarked page
        PageNo = 0

        ; label for this page (only present if logical and physical page numbers
        ; are not the same)
        PageLabel =

        ; session-only favorite; omitted when serializing array elements
        ; (introduced in version 3.7)
        IsTemporary = false
      ]
    ]

    ; if true, the document is "pinned" to the Frequently Read list, so that
    ; recently opened documents don't displace it
    IsPinned = false

    ; if true, the file is considered missing and won't be shown in any list
    IsMissing = false

    ; number of times this document has been opened recently
    OpenCount = 0

    ; data required to open a password protected document without having to ask
    ; for the password again
    DecryptionKey =

    ; reflowable (ebook) settings for just this document. The block is absent
    ; until you add it; a field left empty or 0 uses the global EBookUI value.
    ; The global section's WindowBgCol and DefaultDisplayMode are already
    ; per-document as BgCol and DisplayMode below (introduced in version 3.7)
    EBookUI [
        ; font family for this document (e.g. Segoe UI, Microsoft YaHei); empty
        ; uses EBookUI.FontName (introduced in version 3.7)
        FontName = 

        ; font size in points for this document; 0 uses EBookUI.FontSize
        ; (introduced in version 3.7)
        FontSize = 0

        ; white space around the text for this document, in points; one, two or
        ; four values like EBookUI.Margin. empty uses EBookUI.Margin (introduced
        ; in version 3.7)
        Margin =

        ; line-height multiplier for this document (e.g. 1.5); 0 uses
        ; EBookUI.LineSpacing (introduced in version 3.7)
        LineSpacing = 0

        ; width of the page this document is laid out into, in points; 0 uses
        ; EBookUI.LayoutDx (introduced in version 3.7)
        LayoutDx = 0

        ; height of the page this document is laid out into, in points; 0 uses
        ; EBookUI.LayoutDy (introduced in version 3.7)
        LayoutDy = 0

        ; whether the CSS in this document is ignored: true or false; empty uses
        ; EBookUI.IgnoreDocumentCSS (introduced in version 3.7)
        IgnoreDocumentCSS = 

        ; additional CSS applied to this document; empty uses EBookUI.CustomCSS
        ; (introduced in version 3.7)
        CustomCSS = 
    ]

    ; if true, this document opens with the global defaults instead of the
    ; values below
    UseDefaultState = false

    ; layout of pages. valid values: automatic, single page, facing, book view,
    ; continuous, continuous facing, continuous book view
    DisplayMode = automatic

    ; how far this document has been scrolled (in x and y direction)
    ScrollPos = 0 0

    ; number of the last read page
    PageNo = 1

    ; zoom (in %) or one of those values: fit page, fit width, fit height, fit
    ; content
    Zoom = fit page

    ; how far pages have been rotated as a multiple of 90 degrees
    Rotation = 0

    ; state of the window. 1 is normal, 2 is maximized, 3 is fullscreen, 4 is
    ; minimized
    WindowState = 0

    ; default position (can be on any monitor)
    WindowPos = 0 0 0 0

    ; if true, show the table of contents (Bookmarks) sidebar when the document
    ; has one
    ShowToc = true

    ; width of the bookmarks / favorites sidebar in screen pixels, as last
    ; resized
    SidebarDx = 0

    ; if true, the document is displayed right-to-left in facing and book view
    ; modes
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

        ; layout of pages in this tab. valid values: automatic, single page,
        ; facing, book view, continuous, continuous facing, continuous book view
        DisplayMode = automatic

        ; number of the last read page
        PageNo = 1

        ; zoom (in %) or one of those values: fit page, fit width, fit height,
        ; fit content
        Zoom = fit page

        ; how far pages have been rotated as a multiple of 90 degrees
        Rotation = 0

        ; how far this document has been scrolled (in x and y direction)
        ScrollPos = 0 0

        ; if true, the table of contents was shown when the document was closed
        ShowToc = true

        ; which table of contents items were expanded (see FileStates ->
        ; TocState)
        TocState =
      ]
    ]

    ; index of the currently selected tab (1-based)
    TabIndex = 1

    ; state of the window. 1 is normal, 2 is maximized, 3 is fullscreen, 4 is
    ; minimized
    WindowState = 0

    ; position of the window (can be on any monitor)
    WindowPos = 0 0 0 0

    ; width of the favorites / bookmarks sidebar in screen pixels (0 if it
    ; wasn't shown)
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

; if true, check once a day whether an update is available
CheckForUpdates = true
```

## Syntax for color values

The syntax for colors is: `#rrggbb` or `#aarrggbb`.

The components are hex values (ranging from 00 to FF) and stand for:
- `aa` : alpha (opacity). `00` is fully transparent, `FF` is fully opaque, and `7F` is about 50% opaque
- `rr` : red component
- `gg` : green component
- `bb` : blue component

For example `#ff0000` is opaque red. `#7fff0000` is half-transparent red. `#rrggbb` (6 digits) is the same as `#FFrrggbb`. You can use [Sphere](https://colorsphere.app/) to pick a color. This applies to annotation colors too (`Annotations.HighlightColor`, underline, squiggly, strike-out).

