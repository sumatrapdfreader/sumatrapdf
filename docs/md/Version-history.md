# Version history

**next: 3.7**

Available in [pre-release](https://www.sumatrapdfreader.org/prerelease) builds.

- can open and view Markdown documents (`.md`, `.markdown`): they render as formatted text (GitHub Flavored Markdown, including tables, task lists and strikethrough) via the rendering engine, and the installer registers the file association so they open from Explorer and drag&drop
- Read Aloud: adjustable playback speed — pick 0.5x .. 3x in the new `Speed` submenu (next to `Voice` in the Read Aloud menu, toolbar dropdown and context menu) or click the speed button on the playback bar to cycle presets (right-click cycles backwards); the speed persists across sessions via the `ReadAloudSpeed` advanced setting
- updated the bundled MuPDF rendering engine to 1.28.0
- add [AI Chat with document](AI-Chat-with-document.md) sidebar (in View menu and `Ctrl + k` [command palette](Command-Palette.md)) for asking questions about the open PDF or image via [Claude Code](https://docs.anthropic.com/en/docs/claude-code); per-tab session state, model/effort selection, and session history from `~/.claude/projects/`
- add `ClaudeCode` advanced settings (`Model`, `Effort`, `SkipPermissions`, `BgColor`, `SidebarDx`) in `SumatraPDF-settings.txt`
- auto-link plain-text DOIs in PDF text (e.g. `10.1109/WICSA.2015.29` opens `https://doi.org/...`); uses the same auto-detect path as URLs and email addresses
- add `DisableAutoLinks` advanced setting to disable auto-linking of URLs, email addresses, and plain-text DOIs found in PDF text (fixes #5703)
- the `FixedPageUI.SelectionColor` advanced setting now honors an alpha channel: set an `#aarrggbb` value (e.g. `#40f5fc0c`) to make the text-selection overlay more transparent so selected text stays crisp instead of looking washed out; `#rrggbb` keeps the previous default opacity (fixes #3209)
- middle-click auto-scroll is now smooth: it's driven by a high-frequency timer with fractional-pixel accumulation instead of a coarse 20ms timer with integer steps, so it no longer looks choppy (also enables fine, slow scroll speeds) (fixes #2693)
- associated file types now show their localized name in Explorer's "Type" column on non-English Windows; previously registration hardcoded an English name like "PDF File", overriding the name Windows would localize (fixes #3323)
- expand the table of contents tree down to the current page's entry and select it, like Explorer's "Expand to current folder" (in the Bookmarks sidebar right-click menu and the `Ctrl + k` command palette) (fixes #1998)
- Save As now warns instead of failing silently when a file can't be written (e.g. the destination path exceeds the Windows `MAX_PATH` limit); previously there was no way to tell the save hadn't happened (fixes #1016)
- can convert an image to a PDF: right-click an image (or an open image document) and choose `Image / Convert to PDF`, or pick `PDF` in the format drop-down of the Save Image dialog. The new PDF gets `CreationDate`/`ModDate` metadata with the current time and time zone (fixes #949)
- in the Favorites pane and menu, a favorite for a file with a long name now shows your favorite's name first, then the file name, so the name you gave it is no longer pushed out of view (fixes #829, #2236)
- case-insensitive search now treats German ß as equivalent to `ss`, so searching `Strasse` finds `Straße` and vice versa (fixes #933)
- hovering a thumbnail on the Frequently Read home page now shows a ✕ button in its top-right corner to remove that document from the list, without going through the right-click menu (fixes #283)
- new zoom mode `Fit by Orientation` (in the View / Zoom menu) that automatically fits width when the view is landscape and fits page when portrait, updating as you resize the window or rotate the screen (fixes #702)
- add `sumatrapdf-tool.exe` command-line tools for PDF manipulation (see [Tools](Tools.md))
- [command palette](Command-Palette.md) has a new `*` mode that jumps to a table of contents entry of the current document (`Shift + F12`). Shows the fully expanded outline, indented by nesting level, with the entry closest to the current page pre-selected (fixes #5676)
- [command palette](Command-Palette.md) has a new `$` mode that jumps to a favorite, listing the current document's favorites first, then favorites of other documents
- [external viewer](Customize-external-viewers.md) command lines now support `%%` as a literal `%`, so e.g. `%%d` is passed to the program as `%d` (fixes #5583)
- with `ReuseInstance`, opening a file now reuses a window on the current virtual desktop (or opens a new window there) instead of switching to a window on another desktop (fixes #5630)
- add Back / Forward navigation buttons to the toolbar; navigation history now also records views you scrolled to and stayed on, not just table of contents / link jumps
- improved overlay scrollbar
- make thumbnails on home page scrollable
- add ability to register / unregister Windows preview handler and search filter from `Ctrl + k` command palette
- set a custom color for a document's tab from the tab right-click context menu
- compress, decompress, encrypt, decrypt, bake, delete pages from, and extract pages from PDF files (via `Ctrl + k` [command palette](Command-Palette.md))
- extract text from document pages to a .txt file (via `Ctrl + k` [command palette](Command-Palette.md))
- read text aloud using Windows text-to-speech: toolbar, main menu, and context menu **Read Aloud (TTS)** submenus with **Start Reading From Top** (viewport through end of document), **Start Reading Selection**, **Start Reading From Cursor Position** (context menu), pause / continue / stop, a playback bar on the canvas showing the document name, current page, and scope, word highlight while speaking, and a **Voice** submenu; chosen voice is remembered via the `ReadAloudVoiceId` advanced setting
- add `ToolbarText` and `ToolbarSvgIcon` parameters for `ExternalViewers` advanced setting to show external viewer as a toolbar button with text or an SVG icon (fixes #5741)
- move `Scrollbars` advanced setting from `FixedPageUI` to top-level
- toolbar has a new `overlay` mode: the toolbar floats over the page (sized to its natural width and centered) and is only revealed when the mouse moves near it. Set it with the new top-level `Toolbar = show | hide | overlay` advanced setting; `F8` (Toggle Toolbar) now cycles show → overlay → hide instead of just toggling show/hide
- the toolbar can be placed at the top or bottom of the window via the new `ToolbarPosition = top | bottom` advanced setting (works in both show and overlay modes); toggle it with the `CmdToggleToolbarPosition` command (`Ctrl + k` command palette)
- DjVu documents can be rendered with a new built-in plain-C decoder (`ext/djvudec`) instead of libdjvu. Choose it with the new `DjvuEngine = djvudec | libdjvu` advanced setting (defaults to `djvudec`); toggle and reload the current document with the `CmdToggleDjvuEngine` command (`Ctrl + k` command palette)
- add `EBookUI.BackgroundColor` advanced setting to override background color for ebook documents (epub, mobi etc.)
- add `ComicBookUI.BackgroundColor` advanced setting to override the default black background for comic book files
- add `ImageUI.BackgroundColor` advanced setting to override the default black background for image files
- background color settings (`FixedPageUI.BackgroundColor`, `EBookUI.BackgroundColor`, `ComicBookUI.BackgroundColor`, `ImageUI.BackgroundColor`) accept `checkered` value to show a checkerboard transparency pattern
- change document background color per-file or for all files of the same type (via `Ctrl + k` [command palette](Command-Palette.md))
- `Ctrl + click` on a PDF link opens it in a new tab (instead of navigating in the current tab)
- you can now drag&drop selected text to another application, like a text editor
- triple-click selects the whole line of text (double-click still selects a word) (fixes #694)
- fix `ExternalViewers` `CommandLine` parsing mangling quotes (e.g. `-t="Hello World"` became `"-t=Hello World"`); the command line is now passed through with the user's quoting preserved, only substituting `%1`/`%p`/`%d` (fixes #5695)
- fix `-print-settings paper=A3` (and other standard sizes) when the printer driver reports a longer paper name such as `A3 297 x 420 mm` (fixes #5632)
- add `stretch` page scaling (and a matching "Stretch pages to fill paper" option in the Advanced print dialog) that fills the paper in both dimensions, ignoring the aspect ratio (fixes #2220)
- add "Center page horizontally on the paper" option to the Advanced print dialog, to center a page smaller than the paper (fixes #348)
- add "Choose paper source by document page size" option to the Advanced print dialog, to let the printer pick the input tray whose paper matches the page (fixes #349)
- add "Print each page at its document page size" option to the Advanced print dialog, to correctly print documents with mixed page sizes (fixes #533)
- use the file name (not the full path) as the print job name, which some printer drivers couldn't handle for long or non-ASCII paths (fixes #2166)
- command-line printing now honors print defaults embedded in a PDF's `ViewerPreferences` (`PrintScaling`, `NumCopies`, `Duplex`, `PickTrayByPDFSize`); explicit `-print-settings` values override them, and `ignore-pdf-print-settings` disables them (fixes #534)
- command-line printing (`-print-to` / `-print-to-default`) now returns a distinct process exit code per failure category (file not loadable, printing not allowed, printer not found, driver failed, etc.) instead of a plain success/failure, so unattended callers can tell why a print failed (fixes #3478)
- can set a default for the print dialog's Collate checkbox via the new `PrinterDefaults.Collate` advanced setting (fixes #1558)
- add a "Rotate printout" option to the Advanced print dialog, to rotate the printout and fix a wrong orientation (e.g. upside-down output on virtual printers like XPS / Print to PDF) (fixes #1246)
- implement the `[GetFileState]` [DDE command](DDE-Commands.md) to query a document's path, zoom, view mode and version; also fixes a crash and broken/empty replies in the DDE request path on 64-bit (fixes #483)
- `[GetFileState]` also returns the current page and page count, and a new `[GetOpenFiles]` DDE request returns the paths of all open documents (fixes #5060)
- add `[GetMousePos]` DDE request returning the document position under the mouse cursor in PDF points (the `.smx` unit), for external annotation tools (fixes #1411)
- `[SetView]` DDE command now accepts a zoom of `0` to keep the current zoom; previously, scrolling via `SetView` while passing a Fit zoom re-fit the page and jumped the scroll position to the next page (fixes #5068)
- toggle commands (`CmdToggleFullscreen`, `CmdTogglePresentationMode`, `CmdToggleToolbar`, `CmdToggleMenuBar`, `CmdToggleContinuousView`, `CmdToggleTableOfContents`/`CmdToggleBookmarks`) accept an optional `state` argument to force an on/off state instead of toggling, e.g. `[CmdToggleFullscreen on]` (fixes #5067); also fixes parsing of boolean command arguments (`off`/`no`/`0` and `on` are now recognized)
- add `[GotoPageWord]` [DDE command](DDE-Commands.md) to go to a page and select a search term only if it's on that page (a page-constrained search, unlike `[Search]` which wraps); also documented the existing `[Search]` DDE command (fixes #3085)
- fix EXIF orientation ignored for JPEG and WebP images (fixes #1544)
- move `DefaultImageZoom` advanced setting to `ImageUI.DefaultZoom`, default to `shrink to fit`
- improve Toggle Use Tabs: you can now transition between using tabs / not using tabs without restarting the app
- allow showing menu bar when using tabs (previously menu bar was only shown when not using tabs)
- capture screenshots of the desktop and all visible windows, saved as PNG files in `Screenshots` sub-directory of SumatraPDF data directory (global hotkey e.g. PrtSc requires a Shortcuts entry)
- you can drag&drop images from a browser onto SumatraPDF window. We'll download it to Downloads folder and open
- crop and resize images when viewing image files
- `Ctrl + V` pastes image from clipboard, saves as PNG in Downloads folder and opens it
- Can save images in different formats: PNG, JPEG, BMP, GIF, TIFF.
- add `Fullscreen` advanced setting with `ShowToolbar` and `ShowMenubar` options to show toolbar and menu bar in fullscreen mode. Use `F9` / `F8` to toggle them while in fullscreen
- add `Show Errors` in right-click context menu for PDF documents that have mupdf warnings/errors
- replace `HideScrollbars` and `UseOverlayScrollbar` settings with `Scrollbars` setting (values: `windows`, `smart`, `overlay`, `hidden`)
- save and restore groups of tabs; saved groups are persisted in `TabGroups` advanced setting
- add `Shrink To Fit` zoom mode: shows at 100% if page is smaller than view area, otherwise fits page
- add `TabsMru` advanced setting to change order of navigating tabs when using `Ctrl + Tab`
- improve document properties for comic book files (CBZ, CBR, CB7, CBT). We now show list of image files and per-image EXIF metadata
- improve document properties for image files: size, dimensions, DPI, exif metadata
- support encrypted .cbz, .cbr files
- you can drag&drop images from PDF documents to other applications (web apps, image editors, file explorer etc.)
- pen/stylus input now works for text selection on Windows tablets
- fix Edit Annotations window not restoring to the correct monitor in multi-monitor setups
- use `GetFileAttributesEx` instead of opening files for change detection on network drives, avoiding Windows Defender re-scans
- fix toolbar page number misalignment when `PrinterAccess` is revoked in `sumatrapdfrestrict.ini`
- add citation/reference hover preview: hovering an internal-document link (e.g. a `[1]` citation, figure reference, or footnote marker) now shows a small popup rendering the destination region, so you can see the bibliography entry / figure / footnote without leaving the current page. The `CitationHoverDelay` advanced setting sets the hover delay in ms (-1 disables the popup) (fixes [#128](https://github.com/sumatrapdfreader/sumatrapdf/issues/128), [#4221](https://github.com/sumatrapdfreader/sumatrapdf/issues/4221))
- translate selected text with Grok Build, Claude Code, or OpenAI Codex when the corresponding CLI is installed (selection context menu); opens a dialog to edit the text, pick source and destination languages, and show the translation inline
- add a **Match whole word** toggle to the Find bar (next to **Match Case**) so a search only matches complete words, e.g. `cat` no longer matches `category` (fixes #4295)
- when searching, the **current** match is now highlighted with the customizable `FixedPageUI.SelectionColor` (the color users tune to be most noticeable) and all other matches use a secondary orange highlight, so the active match is easier to spot; previously it was the other way around (fixes #5740)
- find-as-you-type now waits briefly after you stop typing before searching (500 ms, or 1 s for 1–2 character terms) instead of searching on every keystroke; pressing Enter searches immediately (fixes #4626)
- add **Go to Next/Previous Favorite** commands (`Ctrl + k` command palette) that jump to the nearest favorite (bookmark) page after / before the current page (fixes #3744)
- can paste an image from the clipboard into a PDF as an image stamp annotation: right-click → **Create Annotation Under Cursor** → **Image From Clipboard** (or the `Ctrl + k` command palette). The stamp is created at the click point, sized to the image, and immediately selected so it can be moved/resized
- text in some PDFs that use embedded subset fonts naming glyphs like `G45` (with no `ToUnicode` map) is now extracted and searchable: previously such text came out as `�` and couldn't be found (e.g. searching "Emergency" failed). mupdf now recovers Unicode from those glyph names the way pdf.js does (fixes #3219)
- add `AllowExternalImages` advanced setting (off by default): when on, a PDF may display an image stored in a separate file referenced by name (an "external image stream"); the file must sit next to the PDF. Off by default for security, matching Acrobat (fixes #3731)
- add **Set Inverse Search Command Line** (`Ctrl + K` [command palette](Command-Palette.md)): opens a standalone dialog to configure the SyncTeX inverse-search command (with detected TeX editors in a drop-down and a Help link to [LaTeX integration](LaTeX-integration.md)); OK saves `InverseSearchCmdLine` and enables TeX enhancements, Cancel leaves settings unchanged. Works from the home page without an open document

**New commands:**

- `CmdAIChatWithClaudeCode` : "AI Chat"
- `CmdChangeBackgroundColor` : "Change Background Color"
- `CmdChangeScrollbar` : "Change Scrollbar"
- `CmdCommandPalette *` : command palette table-of-contents mode (`CmdCommandPaletteTOC`, `Shift + F12`)
- `CmdCommandPalette $` : command palette favorites mode (`CmdCommandPaletteFavorites`)
- `CmdCommandPaletteFavorites` : "Command Palette: Favorites"
- `CmdContinueReadAloud` : "Continue Reading"
- `CmdFindToggleMatchWholeWord` : "Find: Toggle Match Whole Word" — Find bar toggle button
- `CmdGoToNextFavorite` : "Go to Next Favorite"
- `CmdNavigateFilesInFolder` : "Navigate Files in Folder" — floating directory browser for openable files (Enter/double-click opens a file or enters a directory, `..` goes up, Esc closes)
- `CmdGoToPrevFavorite` : "Go to Previous Favorite"
- `CmdCreateAnnotImageFromClipboard` : "Create Image Annotation From Clipboard"
- `CmdStopReadAloud` : "Stop Reading"
- `CmdReadAloudFromTopPage` : "Start Reading From Top"
- `CmdReadAloudSelection` : "Start Reading Selection"
- `CmdConvertImageToPdf` : "Convert Image To PDF"
- `CmdCropImage` : "Crop Image"
- `CmdDocumentExtractText` : "Extract Text From Document"
- `CmdDocumentShowOutline` : "Show Document Outline"
- `CmdExpandToCurrentPage` : "Expand TOC to Current Page"
- `CmdListPrinters` : "List Printers"
- `CmdPauseReadAloud` : "Pause Reading"
- `CmdPdfBake` : "Bake PDF File"
- `CmdPdfCompress` : "Compress PDF"
- `CmdPdfDecrypt` : "Decrypt PDF"
- `CmdPdfDecompress` : "Decompress PDF"
- `CmdPdfDeletePages` : "Delete Pages From PDF"
- `CmdPdfEncrypt` : "Encrypt PDF"
- `CmdPdfExtractPages` : "Extract Pages From PDF"
- `CmdPdShowInfo` : "Show PDF Info"
- `CmdReadAloud` : "Read Aloud"
- `CmdRemoveDeletedFilesFromHistory` : "Remove Deleted Files From History"
- `CmdResizeImage` : "Resize Image"
- `CmdScreenshot` : "Take Screenshot"
- `CmdSetScreenshotHotkey` : "Set Screenshot Hotkey"
- `CmdSetInverseSearch` : "Set Inverse Search Command Line" — opens a dialog to configure the SyncTeX inverse-search command (`Ctrl + k` command palette)
- `CmdSetTabColor` : "Set Tab Color"
- `CmdStartAutoScroll` : "Start Auto-Scroll"
- `CmdTabGroupRestore` : "Restore Tab Group"
- `CmdTabGroupSave` : "Save Tab Group"
- `CmdToggleChmUI` : "Toggle CHM UI"
- `CmdToggleEscToExit` : "Toggle Esc to Exit"
- `CmdToggleHoverPreview` : "Toggle Hover Preview"
- `CmdToggleReuseInstance` : "Toggle Reuse Instance"
- `CmdToggleScrollbarInSinglePage` : "Toggle Scrollbar In Single Page"
- `CmdToggleSmoothScroll` : "Toggle Smooth Scroll"
- `CmdToggleToolbarPosition` : "Toggle Toolbar Position" (command palette shows the target, e.g. "set to bottom")
- `CmdToggleDjvuEngine` : "Toggle DjVu Engine" (command palette shows the target, e.g. "set to libdjvu")
- `CmdToggleTabsMru` : "Toggle Tabs MRU"
- `CmdToggleTips` : "Toggle Tips"
- `CmdToggleWindowsPreviewer` : "Toggle Windows Previewer"
- `CmdToggleWindowsSearchFilter` : "Toggle Windows Search Filter"
- `CmdTranslateSelectionWithClaudeCode` : "Translate Selection with Claude Code"
- `CmdTranslateSelectionWithGrokBuild` : "Translate Selection with Grok Build"
- `CmdTranslateSelectionWithOpenAICodex` : "Translate Selection with OpenAI Codex"
- `CmdZoomFitByOrientation` : "Fit by Orientation"
- `CmdZoomShrinkToFit` : "Shrink To Fit"

**New command-line arguments:**

- `-for-testing` : for ad-hoc testing; always starts a new instance, doesn't restore a session, doesn't save settings
- `-dbg-control <named-pipe>` : drive automated tests over a named-pipe request/response protocol (`cmd/control.ts`)
- `-dump-chm <file>` : headlessly list CHM contents, unpack entries to memory, and print TOC/index metadata
- `-pwd <password>` : open password-protected documents from the command line (fixes #906)
- `-log-to-file <file>` : log to a specific file (like `-log` but with a custom log file path)
- `/p` : Adobe Reader-compatible alias for `-print-dialog`
- `/t` : Adobe Reader-compatible silent print (alias for `-print-to`)
- `sumatrapdf-tool.exe <tool> <args>` : command-line tools (draw, convert, audit, bake, clean, create, extract, info, merge, pages, poster, recolor, show, trim, grep, trace)
- `-print-settings` tokens: `stretch`, `center`, `bin=auto`, `paper=auto`, `collate` / `nocollate`, `rotate=<90|180|270>`, `ignore-pdf-print-settings`

## 3.6.1 (2026-04-06)

- bugfixes

## 3.6 (2026-03-17)

- add `DisableAntiAlias` advanced setting
- temporarily hide / show annotations
- temporarily disable mouse click invoking TeX inverse search
- add `bgcolor`, `opacity`, `textsize`, `borderWidth` arguments to `CmdCreateAnnot*` commands
- add `Annotations.FreeTextBackgroundColor` and `Annotations.FreeTextOpacity` advanced settings
- sort thumbnails on home page by most recently used date. Set advanced setting `HomePageSortByFrequentlyRead = true` to revert to pre-3.6 behavior of sorting by frequency of use.
- support brotli compression in PDF files
- in Command Palette, if you start search with `:` we show everything (like in 3.5)
- in Command Palette, when viewing opened files history (`#`), you can press Delete to remove the entry from history
- improved zooming:
  - zooming with pinch touch screen gesture or with ctrl + scroll wheel now zooms around the mouse position and does continuous zoom levels. Used to zoom around top-left corner and progress fixed zoom levels shown in menu
- include manual (`F1` to launch browser with documentation)
- add `LazyLoading` advanced setting, defaults to true. When restoring a session lazy loading delays loading a file until its tab is selected. Makes SumatraPDF startup faster.
- move tabs left / right, like in Chrome (`Ctrl + Shift + PageUp` / `Ctrl + Shift + PageDown`)
- add ability to provide arguments to some commands when creating bindings in `Shortcuts`:
  - `CmdCreateAnnot*` commands take a color argument, `openedit` to automatically open edit annotations window when creating an annotation, `copytoclipboard` to copy selection to clipboard and `setcontent` to set contents of annotation to selection
  - `CmdScrollDown`, `CmdScrollUp` : integer argument, how many lines to scroll
  - `CmdGoToNextPage`, `CmdGoToPrevPage` : integer argument, how many pages to advance
  - `CmdNextTabSmart`, `CmdPrevTabSmart` (`Smart Tab Switch`), shortcut: `Ctrl + Tab`, `Ctrl + Shift + Tab`
- added `UIFontSize` advanced setting
- removed `TreeFontWeightOffset` advanced setting
- increase number of thumbnails on home page from 10 => 30
- add `ShowLinks` advanced setting
- default `ReuseInstance` setting to true
- added `Key` arg to `ExternalViewers` advanced setting (keyboard shortcut)
- added `Key` arg to `SelectionHandlers` advanced setting (keyboard shortcut)
- improved scrolling with mouse wheel and touch gestures
- theming improvements
- go back to opening settings file with default .txt editor (notepad most likely)
- don't exit fullscreen on double-click. must double-click in upper-right corner
- when opening via double-click, if `Ctrl` is pressed will always open in new tab (vs. activating existing tab)
- register for handling `.webp` files
- bug fix: Del should not delete an annotation if editing content
- bug fix: re-enable tree view full row select
- change: `CmdCreateAnnotHighlight` etc. no longer copies selection to clipboard by default. To get that behavior back, you can use `copytoclipboard` argument [instead](Commands.md#cmdcreateannothighlight-and-other-cmdcreateannot).
- change: `Ctrl + Tab` is now `CmdNextTabSmart`, was `CmdNextTab`. `Ctrl + Shift + Tab` is now `CmdPrevTabSmart`, was `CmdPrevTab`. You can [re-bind it](Customize-keyboard-shortcuts.md) if you prefer old behavior
- `CmdCommandPalette` takes optional `mode` argument: `@` for tab selection, `#` for selecting from file history and `>` for commands.
- command palette no longer shows combined tabs/file history/commands. `CmdCommandPalette` only shows commands. Because of that removed `CmdCommandPaletteNoFiles` because now `CmdCommandPalette` behaves like it
- removed `CmdCommandPaletteOnlyTabs`, replaced by `CmdCommandPaletteNoFiles @`
- `Ctrl + Shift + K` no longer active, use `Ctrl + K`. You can restore this shortcut by binding it to `CmdCommandPalette >` command.
- add `Name` field for shortcuts. If given, the command will show up in Command Palette (`Ctrl + K`)
- closing a current tab now behaves like in Chrome: selects next tab (to the right). We used to select previously active tab, but that's unpredictable and we prefer to align SumatraPDF behavior with other popular apps.
- swapped key bindings: `i` is now CmdTogglePageInfo, `I` is CmdInvertColors. Several people were confused by accidentally typing `i` to invert colors, is less likely to type it accidentally
- allow creating custom themes in advanced settings in `Themes` section. [See docs](https://www.sumatrapdfreader.org/docs/Customize-theme-colors).
- improve scrolling with middle click drag [#4529](https://github.com/sumatrapdfreader/sumatrapdf/issues/4529)
- make built-in keyboard shortcuts work on non-us keyboards (cyrillic , hebrew etc.)
- open current document in a new tab

**New commands:**

- `CmdCloseAllTabs` : "Close All Tabs"
- `CmdCloseTabsToTheLeft` : "Close Tabs To The Left"
- `CmdDeleteFile` : "Delete File"
- `CmdDuplicateInNewTab` : "Open Current Document In New Tab"
- `CmdHideAnnotations` : "Hide Annotations"
- `CmdInvokeInverseSearch` : "Invoke Inverse Search"
- `CmdMoveTabLeft` : "Move Tab Left" (`Ctrl + Shift + PageDown`)
- `CmdMoveTabRight` : "Move Tab Right" (`Ctrl + Shift + PageUp`)
- `CmdNextTabSmart` : "Smart Tab Switch" (`Ctrl + Tab`)
- `CmdPrevTabSmart` : "Smart Tab Switch" (`Ctrl + Shift + Tab`)
- `CmdShowAnnotations` : "Show Annotations"
- `CmdToggleTableOfContents` : "Toggle Table Of Contents"
- `CmdToggleAntiAlias` : "Toggle Anti-Alias"
- `CmdToggleFrequentlyRead` : "Toggle Frequently Read"
- `CmdToggleInverseSearch` : "Toggle Inverse Search"
- `CmdToggleLinks` : "Toggle Show Links"
- `CmdToggleShowAnnotations` : "Toggle Show Annotations"

## 3.5.2 (2023-10-25)

- fix not showing tab text
- make menus in dark themes look more like standard menus (bigger padding)
- fix Bookmarks for folder showing bad file names
- update translations

## 3.5.1 (2023-10-24)

- fix uninstaller crash
- disable lazy loading of files when restoring a session

## 3.5 (2023-10-23)

- Arm 64-bit builds
- dark mode (menu `Settings / Theme` or `Ctrl + K` command `Select next theme`)
  you can use `i` (invert colors) to match the background / text color of rendered
  PDF document. Due to technical limitations, it doesn't work well with images
- `i` (invert colors) is remembered in settings
- select annotation under cursor and open annotation edit window
- rename `CmdShowCursorPosition` => `CmdToggleCursorPosition`
- add `Annotations [ FreeTextColor, FreeTextSize, FreeTextBorderWidth ]` settings
- ability to move annotations. `Ctrl + click` to select annotation and then move via drag & drop
- exit full screen / presentation modes via double click with left mouse button
- ability to drag out a tab to open it in new window
- support opening `.avif` images (including inside .cbz/,cbr files)
- respect image orientation `exif` metadata in .jpeg and .png images
- support Adobe Reader syntax for opening files (see command-line arguments below)
- add Next Tab / Prev Tab commands with `Ctrl + PageUp` / `Ctrl + PageDown` shortcuts
- keep Home tab open; add `NoHomeTab` advanced option to disable that
- add context menu to tabs
- bugfix: handle files we can't open in `next file in folder` / `prev file in folder` commands
- command palette: when search starts with `>`, only show commands, not files (like in Visual Studio Code)
- reopen last closed tab (`Ctrl + Shift + T`, like in web browsers)
- clear history from command palette
- can send commands via [DDE](https://www.sumatrapdfreader.org/docs/DDE-Commands)
- open file in Explorer, Directory Opus, Total Commander, or Double Commander from command palette
- close other tabs / close tabs to the right from command palette
- recognize `PgUp` / `PgDown` and a few more in keyboard shortcuts

**New commands:**

- `CmdClearHistory` : "Clear History"
- `CmdCloseOtherTabs` : "Close Other Tabs"
- `CmdCloseTabsToTheRight` : "Close Tabs To The Right"
- `CmdCommandPaletteOnlyTabs` : "Command Palette: Tabs Only" (`Alt + K`)
- `CmdEditAnnotations` : "Edit Annotations"
- `CmdNextTab` : "Next Tab" (`Ctrl + PageDown`)
- `CmdOpenWithDirectoryOpus` : "Open With Directory Opus"
- `CmdOpenWithDoubleCommander` : "Open With Double Commander"
- `CmdOpenWithExplorer` : "Open With Explorer"
- `CmdOpenWithTotalCommander` : "Open With Total Commander"
- `CmdPrevTab` : "Prev Tab" (`Ctrl + PageUp`)
- `CmdReopenLastClosedFile` : "Reopen Last Closed" (`Ctrl + Shift + T`)
- `CmdToggleCursorPosition` : "Toggle Cursor Position" (renamed from `CmdShowCursorPosition`)

**New command-line arguments:**

- `-dde` : enable DDE server
- `/A "page=<pageno>#nameddest=<dest>search=<string>"` : Adobe Reader-compatible open syntax
- `-print-settings disable-auto-rotation` : don't auto-rotate wide pages to fit paper

## 3.4.6 (2022-06-08)

- fix crashes
- fix hang in Fit Content mode and Bookmark links

## 3.4.5 (2022-06-05)

- fix crashes

## 3.4.4 (2022-06-02)

- restore `HOME` and `END` in find edit field
- fix crashes

## 3.4.3 (2022-05-29)

- re-enable `Backspace` in edit field
- fix installation for all users when using custom installation directory
- re-enable `Copy Image` context menu for comic book files
- fix display of some PDF images
- fix slow loading of some ePub files

## 3.4.2 (2022-05-27)

- make keyboard accelerators work when tree view has focus
- fix `-set-color-range` and `-bg-color` replacing `MainWindowBackground`
- fix crash with incorrectly defined selection handlers

## 3.4.1 (2022-05-25)

- fix downloading of symbols for better crash reports

## 3.4 (2022-05-24)

- [Command Palette](Command-Palette.md) (`Ctrl + K`)
- [customizable keyboard shortcuts](Customize-keyboard-shortcuts.md)
- better support for epub files using mupdf's epub engine. Adds text selection and search in ebook files. Better rendering fidelity. On the downside, might be slower.
- [search / translate selected text](Customize-search-translation-services.md) with web services
  - we have few built-in and you can [add your own](https://www.sumatrapdfreader.org/settings/settings3-4#SelectionHandlers)
- installer supports system-wide install via command line
- added `Annotations.TextIconColor` and `TextIconType` advanced settings
- added `Annotations.UnderlineColor` advanced setting
- added `Annotations.DefaultAuthor` advanced setting
- `i` keyboard shortcuts inverts document colors `Shift + i` does what `i` used to do i.e. show page number
- `u` and `Shift + u` keyboard shortcuts adds underline annotation for currently selected text
- `Delete` / `Backspace` keyboard shortcuts delete an annotation under mouse cursor
- support `.svg` files
- faster scrolling with mouse wheel when cursor over scrollbar
- search for text when opening a document from the command line; also `[Search("<file>", "<search-term>")]` DDE command
- a way to get list of used fonts in properties window
- support opening `.heic` image files (if Windows heic codec is installed)
- add experimental smooth scrolling (enabled with `SmoothScroll` advanced setting)

**New commands:**

- `CmdCommandPalette` : "Command Palette" (`Ctrl + K`)

**New command-line arguments:**

- `-all-users` : system-wide install (installer)
- `-search <term>` : search for text when opening a document

## 3.3.3 (2021-07-20)

- fix a crash in PdfFilter.dll

## 3.3.2 (2021-07-19)

- restore showing Table Of Contents for `.chm` files
- fix crashes

## 3.3.1 (2021-07-14)

- fix rotation in DjVu documents

## 3.3 (2021-07-06)

- added support for adding / removing / editing annotations in PDF files. Read [the tutorial](Editing-annotations.md)
- new toolbar
  - changed toolbar to scale with DPI by using new, vector icons
  - added rotate left / right to the toolbar
  - new toolbar:

  ![Toolbar](img/toolbar.png)

- added ability to hide scrollbar (more screen space for the document). Use right-click context menu.

Minor improvements and bug-fixes:

- advanced setting to change font size in bookmarks / favorites tree view e.g. `TreeFontSize = 12`
- support newer versions of ghostscript (≥ 9.54) for opening `.ps` files
- support jpeg-xr images in `.xps` files
- restore tooltips (regression in 3.2)
- update mupdf to latest version
- make silent installation always silent
- don't crash when attempting to zoom in on home page
- don't show "manga" view menu item for documents that are not comic books
- allow opening `fb2.zip` files ([#1657](https://github.com/sumatrapdfreader/sumatrapdf/issues/1657))
- restore ability to save embedded files (fixes [#1557](https://github.com/sumatrapdfreader/sumatrapdf/issues/1557))
- `Alt + Space` opens a sys menu

**New command-line arguments:**

- `-print-settings paperkind=${num}` : select paper by Windows paper kind constant ([checkin](https://github.com/sumatrapdfreader/sumatrapdf/pull/1815/commits/2104e6104ea759dc4f839c7e8be5973f5a4f0488))

## 3.2 (2020-03-15)

This release upgrades the core PDF parsing and rendering library mupdf to the latest version. This fixes PDF rendering bugs and improves performance.

Added support for multiple windows with tabs:

- added `File / New Window` (`Ctrl-n`) which opens a new window
- to compare the same file side-by-side, `Ctrl-Shift-n` shortcut opens current file in a new window. The same file is now opened in 2 windows that you can re-arrange as needed
- if you hold `SHIFT` when drag&dropping files from Explorer (and other apps), the file will be opened in a new window

Improved management of favorites:

- context menu (right mouse click) on the document area adds menu items for:
  - showing / hiding favorites view
  - adding current page to favorites (or removing if already is in favorites)
- context menu in bookmarks view adds menu item for adding selected page to favorites

This release no longer supports Windows XP. Latest version that support XP is 3.1.2 that you can download from

[https://www.sumatrapdfreader.org/download-prev.html](https://www.sumatrapdfreader.org/download-prev.html)

**New command-line arguments:**

- `-new-window` : open the document in a new window

## 3.1.2 (2016-08-14)

- fixed issue with icons being purple in latest Windows 10 update
- tell Windows 10 that SumatraPDF can open supported file types

## 3.1.1 (2015-11-02)

- (re)add support for old processors that don’t have SSE2
- support newer versions of unrar.dll
- allow keeping the browser plugin if it’s already installed
- crash fixes

## 3.1 (2015-10-24)

- 64bit builds
- all documents are restored at startup if a window with multiple tabs is closed (or if closing happened through File -> Exit); this can be disabled through the `RestoreSession` advanced setting
- printing happens (again) always as image which leads to more reliable results at the cost of requiring more printer memory; the "Print as Image" advanced printing option has been removed
- scrolling with touchpad (e.g. on Surface Pro) now works
- many crash and other bug fixes

## 3.0 (2014-10-18)

- Tabs! Enabled by default. Use Settings/Options... menu to go back to the old UI
- support table of contents and links in ebook UI
- add support for PalmDoc ebooks
- add support for displaying CB7 and CBT comic books (in addition to CBZ and CBR)
- add support for LZMA and PPMd compression in CBZ comic books
- allow saving Comic Book files as PDF
- swapped keybindings:
  - `F11` : Fullscreen mode (still also `Ctrl + Shift + L`)
  - `F5` : Presentation mode (also `Shift + F11`, still also `Ctrl + L`)
- added a document measurement UI. Press `m` to start. Keep pressing `m` to change measurement units
- new advanced settings: `FullPathInTitle`, `UseSysColors` (no longer exposed through the Options dialog), `UseTabs`
- replaced non-free UnRAR with a free RAR extraction library. If some CBR files fail to open for you, download unrar.dll from https://www.rarlab.com/rar_add.htm and place it alongside SumatraPDF.exe
- deprecated browser plugin. We keep it if it was installed in earlier version

## 2.5.2 (2014-05-13)

- use less memory for comic book files
- PDF rendering fixes

## 2.5.1 (2014-05-07)

- hopefully fix frequent ebook crashes

## 2.5 (2014-05-05)

- 2 page view for ebooks
- new keybindings:
  - `Ctrl + PgDn`, `Ctrl + Right` : go to next page
  - `Ctrl + PgUp`, `Ctrl + Left` : go to previous page
- 10x faster ebook layout
- support JP2 images
- new **[advanced settings](https://www.sumatrapdfreader.org/settings.html)**: `ShowMenuBar`, `ReloadModifiedDocuments`, `CustomScreenDPI`
- left/right clicking no longer changes pages in fullscreen mode (use Presentation mode if you rely on this feature)
- fixed multiple crashes and made multiple minor improvements

## 2.4 (2013-10-01)

- full-screen mode for ebooks (`Ctrl-L`)
- new key bindings:
  - `F9` - show/hide menu (not remembered after quitting)
  - `F8` - show/hide toolbar
- support WebP images (standalone and in comic books)
- support for RAR5 compressed comic books
- fixed multiple crashes

## 2.3.2 (2013-05-25)

- fix changing a language via Settings/Change Language

## 2.3.1 (2013-05-23)

- don't require SSE2 (to support old computers without SSE2 support)

## 2.3 (2013-05-22)

- greater configurability via **[advanced settings](https://www.sumatrapdfreader.org/settings.html)**
- "Go To Page" in ebook ui
- add View/Manga Mode menu item for Comic Book (CBZ/CBR) files
- new key bindings:
  - `Ctrl-Up` : page up
  - `Ctrl-Down` : page down
- add support for OpenXPS documents
- support Deflate64 in Comic Book (CBZ/CBR) files
- fixed missing paragraph indentation in EPUB documents
- printing with "Use original page sizes" no longer centers pages on paper
- reduced size. Installer is ~1MB smaller
- downside: this release no longer supports very old processors without **[SSE2 instructions](https://en.wikipedia.org/wiki/SSE2)**. Using SSE2 makes Sumatra faster. If you have an old computer without SSE2, you need to use 2.2.1.

## 2.2.1 (2013-01-12)

- fixed ebooks sometimes not remembering the viewing position
- fixed Sumatra not exiting when opening files from a network drive
- fixes for most frequent crashes and PDF parsing robustness fixes

## 2.2 (2012-12-24)

- add support for FictionBook ebook format
- add support for PDF documents encrypted with Acrobat X
- “Print as image” compatibility option in print dialog for documents that fail to print properly
- many robustness fixes and small improvements

**New command-line arguments:**

- `-manga-mode [1|true|0|false]` : proper display of manga comic books

## 2.1.1 (2012-05-07)

- fixes for a few crashes

## 2.1 (2012-05-03)

- support for EPUB ebook format
- added File/Rename menu item to rename currently viewed file (contributed by Vasily Fomin)
- support multi-page TIFF files
- support TGA images
- support for some comic book (CBZ) metadata
- support JPEG XR images (available on Windows Vista or later, for Windows XP the **[Windows Imaging Component](https://www.microsoft.com/en-us/download/details.aspx?id=32)** has to be installed)
- the installer is now signed

## 2.0.1 (2012-04-08)

- fix loading `.mobi` files from command line
- fix a crash loading multiple `.mobi` files at once
- fix a crash showing tooltips for table of contents tree entries

## 2.0 (2012-04-02)

- support for **[MOBI](https://blog.kowalczyk.info/articles/mobi-ebook-reader-viewer-for-windows.html)** eBook format
- support opening CHM documents from network drives
- a selection can be copied to a clipboard as an image by using right-click context menu
- using ucrt to reduce program size

## 1.9 (2011-11-23)

- support for **[CHM](https://blog.kowalczyk.info/articles/chm-reader-viewer-for-windows.html)** documents
- support touch gestures, available on Windows 7 or later. Contributed by Robert Prouse
- open linked audio and video files in an external media player
- improved support for PDF transparency groups

## 1.8 (2011-09-18)

- improved support for PDF form text fields
- various minor improvements and bug fixes
- speedup handling some types of djvu files

## 1.7 (2011-07-18)

- favorites
- improved support for right-to-left languages e.g. Arabic
- logical page numbers are displayed and used, if a document provides them (such as i, ii, iii, etc.)
- allow to restrict SumatraPDF's features with more granularity; see **[sumatrapdfrestrict.ini](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/docs/sumatrapdfrestrict.ini)** for documentation
- `-named-dest` also matches strings in table of contents (improvement to existing option)
- improved support for EPS files (requires Ghostscript)
- more robust installer
- many minor improvements and bugfixes

## 1.6 (2011-05-30)

- add support for displaying DjVu documents
- display Frequently Read list when no document is open
- add support for displaying Postscript documents (requires recent Ghostscript version to be already installed)
- add support for displaying a folder containing images: drag the folder to SumatraPDF window
- support clickable links and a Table of Content for XPS documents
- display printing progress and allow to cancel it
- add Print toolbar button
- experimental: previewing of PDF documents in Windows Vista and 7. Creates thumbnails and displays documents in Explorer's Preview pane. Needs to be explicitly selected during install process. We've had reports that it doesn't work on Windows 7 x64.

## 1.5.1 (2011-04-26)

- fixes for rare crashes

## 1.5 (2011-04-23)

- add support for viewing XPS documents
- add support for viewing CBZ and CBR comic books
- add File/Save Shortcut menu item to create shortcuts to a specific place in a document
- add context menu for copying text, link addresses and comments. In browser plugin it also adds saving and printing commands
- add folder browsing (`Ctrl + Shift + Right` opens next PDF document in the current folder, `Ctrl + Shift + Left` opens previous document)

## 1.4 (2011-03-12)

- browser plugin for Firefox/Chrome/Opera (Internet Explorer is not supported). It's not installed by default so you have to check the appropriate checkbox in the installer
- IFilter that enables full-text search of PDF files in Windows Desktop Search (i.e. search from Windows Vista/7's Start Menu). Also not installed by default
- scrolling with right mouse button
- you can choose a custom installation directory in the installer
- menu items for re-opening current document in Foxit and PDF-XChange (if they're installed)
- we no longer compress the installer executable with mpress. It caused some anti-virus programs to falsely report Sumatra as a virus. The downside is that the binaries on disk are now bigger. Note: we still compress the portable .zip version
- removed `-title` cmd-line option
- support for AES-256 encrypted PDF documents
- fixed an integer overflow reported by Jeroen van der Gun and other small fixes and improvements to PDF handling

## 1.3 (2011-02-04)

- improved text selection and copying. We now mimic the way a browser or Adobe Reader works: just select text with mouse and use `Ctrl-C` to copy it to a clipboard
- `Shift + Left Mouse` now scrolls the document, `Ctrl + Left mouse` still creates a rectangular selection (for copying images)
- `c` shortcut toggles continuous mode
- `+` / `*` on the numeric keyboard now do zoom and rotation
- added toolbar icons for Fit Page and Fit Width and updated the look of toolbar icons
- add support for back/forward mouse buttons for back/forward navigation
- 1.2 introduces a new full screen mode and made it the default full screen mode. Old mode was still available but not easily discoverable. We've added View/Presentation menu item for new full screen mode and View/Fullscreen menu item for the old full screen mode, to make it more discoverable
- new, improved installer
- improved zoom performance (zooming to 6400% no longer crashes)
- text find uses less memory
- further printing improvements
- translation updates
- updated to latest mupdf for misc bugfixes and improvements
- use libjpeg-turbo library instead of libjpeg, for faster decoding of some PDFs
- updated openjpeg library to version 1.4 and freetype to version 2.4.4
- fixed 2 integer overflows reported by Stefan Cornelius from Secunia Research

## 1.2 (2010-11-26)

- improved printing: faster and uses less resources
- add `Ctrl-Y` as a shortcut for Custom Zoom
- add `Ctrl-A` as a shortcut for Select All Text
- improved full screen mode
- open embedded PDF documents
- allow saving PDF document attachments to disk
- latest fixes and improvements to PDF rendering from mupdf project

## 1.1 (2010-05-20)

- added book view (“View/Book View” menu item) option. It’s known as “Show Cover Page During Two-Up” in Adobe Reader
- added “File/Properties” menu item, showing basic information about PDF file
- added “File/Send by email” menu
- added export as text. When doing “File/Save As”, change “Save As types” from “ PDF documents” to “Text documents”. Don’t expect miracles, though. Conversion to text is not very good in most cases.
- auto-detect commonly used TeX editors for inverse-search command
- bug fixes to PDF handling (more PDFs are shown correctly)
- misc bug fixes and small improvements in UI
- add `Ctrl +` and `Ctrl –` as shortcuts for zooming (matches Adobe Reader)

## 1.0.1 (2009-11-27)

- many memory leaks fixed (Simon Bünzli)
- potential crash due to stack corruption (pointed out by Christophe Devine)
- making Sumatra default PDF reader no longer asks for admin privileges on Vista/Windows 7
- translation updates

## 1.0 (2009-11-17)

- lots of small bug fixes and improvements

## 0.9.4 (2009-07-19)

- improved PDF compatibility (more types of documents can be rendered)
- added settings dialog (contributed by Simon Bünzli)
- improvements in handling unicode
- changed default view from single page to continuous
- SyncTex improvements (contributed by William Blum)
- add option to not remember opened files
- a new icon for documents association (contributed by George Georgiou)
- lots of bugfixes and UI polish

## 0.9.3 (2008-10-07)

- fix an issue with opening non-ascii files
- updated Japanese and Brazilian translation

## 0.9.2 (2008-10-06)

- ability to disable auto-update check
- improved text rendering - should fix problems with overlapping text
- improved font substitution for fonts not present in PDF file
- can now open PDF files with non-ascii names
- improvements to DDE (contributed by Danilo Roascio)
- SyncTex improvements
- improve persistence of state (contributed by Robert Liu)
- fix crash when pressing `Cancel` when entering a password
- updated translations

## 0.9.1 (2008-08-22)

- improved rendering of some PDFs
- support for links inside PDF file
- enabled SyncTex support which mistakenly disabled in 0.9
- misc fixes and translation updates

**New command-line arguments:**

- `-restrict` : restrict features via `sumatrapdfrestrict.ini` (contributed by Matthew Wilcoxson)
- `-title <title>` : set window title (contributed by Matthew Wilcoxson)

## 0.9 (2008-08-10)

- add `Ctrl-P` as print shortcut
- add `F11` as full-screen shortcut
- password dialog no longer shows the password
- support for AES-encrypted PDF files
- updates to SyncTeX/PdfSync integration (contributed by William Blum)
- add DDE commands for jumping to named destination and opening PDF files (contributed by Alexander Klenin, William Blum)
- removed poppler rendering engine resulting in smaller program and updated to latest mupdf sources
- misc bugfixes and translation updates

**New command-line arguments:**

- `-nameddest <name>` : jump to named destination (contributed by Alexander Klenin)
- `-reuse-instance` : reuse an already running instance (contributed by William Blum)

## 0.8.1 (2008-05-27)

- automatic reloading of changed PDFs (contributed by William Blum)
- tex integration (contributed by William Blum)
- updated icon for case-sensitivity selection in find (contributed by Sonke Tesch)
- language change is now a separate dialog instead of a menu
- remember more settings (like default view)
- automatic checks for new versions
- ESC or single mouse click hides selection
- fix showing boxes in table of contents tree
- translation updates

**New command-line arguments:**

- `-lang <language-code>` : set UI language
- `-print-dialog` : show print dialog (contributed by Peter Astrand)

## 0.8 (2008-01-01)

- added search (contributed by MrChuoi)
- added table of contents (contributed by MrChuoi)
- added many translations
- new program icon
- fixed printing
- fixed some crashes
- rendering speedups
- fixed loading of some PDFs

**New command-line arguments:**

- `-esc-to-exit` : press Esc to exit
- `-bgcolor <color>` : set background color

## 0.7 (2007-07-28)

- added ability to select the text and copy to clipboard - contributed by Tomek Weksej
- made it multi-lingual (13 translations)
- added Save As option
- list of recently opened files is updated immediately
- fixed `.pdf` extension registration on Vista
- added ability to compile as DLL and C# sample application - contributed by Valery Possoz
- mingw compilation fixes and project files for CodeBlocks - contributed by MrChuoi
- fixed a few crashes
- moved the sources to Google Code project hosting

## 0.6 (2007-04-29)

- enable opening password-protected PDFs
- don't allow printing in PDFs that have printing forbidden
- don't automatically reopen files at startup
- fix opening PDFs from network shares
- new, better icon
- reload the document when changing rendering engine
- improve cursor shown when dragging
- fix toolbar appearance on XP and Vista with classic theme
- when MuPDF engine cannot load a file or render a page, we fallback to poppler engine to make rendering more robust
- fixed a few crashes

## 0.5 (2007-03-04)

- fixed rendering problems with some PDF files
- speedups - the application should feel snappy and there should be less waiting for rendering
- added `r` keybinding for reloading currently open PDF file
- added `Ctrl-Shift-+` and `Ctrl-Shift--` keybindings to rotate clockwise and counter-clockwise (just like Acrobat Reader)
- fixed a crash or two

## 0.4 (2007-02-18)

- printing
- ask before registering as a default handler for PDF files
- faster rendering thanks to alternative PDF rendering engine. Previous engine is available as well.
- scrolling with mouse wheel
- fix toolbar issues on win2k
- improve the way fonts directory is found
- improvements to portable mode
- uninstaller completely removes the program
- changed name of preferences files from `prefs.txt` to `sumatrapdfprefs.txt`

## 0.3 (2006-11-25)

- added toolbar for most frequently used operations
- should be more snappy because rendering is done in background and it caches one page ahead
- some things are faster

## 0.2 (2006-08-06)

- added facing, continuous and continuous facing viewing modes
- remember history of opened files
- session saving i.e. on exit remember which files are opened and restore the session when the program is started without any command-line parameters
- ability to open encrypted files
- "Go to page dialog"
- less invasive (less yellow) icon that doesn't jump at you on desktop
- fixed problem where sometimes text wouldn't show (better mapping for fonts; use a default font if can't find the font specified in PDF file)
- handle URI links inside PDF documents
- show "About" screen
- provide a download in a .zip file for those who can't run installation program
- switched to poppler code instead of xpdf

## 0.1 (2006-06-01)

- first version released
