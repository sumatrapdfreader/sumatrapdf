# Version history

## [RSS/Atom feed](https://github.com/sumatrapdfreader/sumatrapdf/releases)

## Version history

### next (3.6)

Changes in [pre-release builds](https://www.sumatrapdfreader.org/prerelease):

- in Command Palette, if you start search with ":" we show everything (like in 3.5)
- in Command Palette, when viewing opened files history (#), you can press Delete to remove the entry from history
- improved zooming:
  - zooming with pinch touch screen gesture or with ctrl + scroll wheel now zooms around the mouse position and does continuous zoom levels. Used to zoom around top-left corner and progress fixed zoom levels shown in menu
- include manual (`F1` to launch browser with documentation)
- add `LazyLoading` advanced setting, defaults to true. When restoring a session lazy loading delays loading a file until its tab is selected. Makes SumatraPDF startup faster.
- new commands in command palette (`Ctrl + K`):
  - `CmdCloseAllTabs` : "Close All Tabs"
  - `CmdCloseTabsToTheLeft` : "Close Tabs To The Left"
  - `CmdDeleteFile`: "Delete File"
  - `CmdToggleFrequentlyRead` : "Toggle Frequently Read"
  - `CmdToggleLinks` : "Toggle Show Links"
  - `CmdInvokeInverseSearch`
  - `CmdMoveTabRight` (`Ctrl + Shift + PageUp`), `CmdMoveTabLeft` (`Ctrl + Shift + PageDown`) to move tabs left / right, like in Chrome
- add ability to provide arguments to some commands when creating bindings in `Shortcuts`:
  - CmdCreateAnnot\* commands take a color argument, `openedit` to automatically open edit annotations window when creating an annotation, `copytoclipboard` to copy selection to clipboard and `setcontent` to set contents of annotation to selection
  - `CmdScrollDown`, `CmdScrollUp` : integer argument, how many lines to scroll
  - `CmdGoToNextPage`, `CmdGoToPrevPage` : integer argument, how many pages to advance
  - `CmdNextTabSmart`, `CmdPrevTabSmart` (`Smart Tab Switch`), shortcut: `Ctrl + Tab`, `Ctrl + Shift + Tab``
- added `UIFontSize` advanced setting
- removed `TreeFontWeightOffset` advanced setting
- increase number of thumbnails on home page from 10 => 30
- add `ShowLinks` advanced setting and "Toggle Show Links" (`CmdToggleLinks`) for command palette
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
- change: `Ctrl + Tab` is now `CmdNextTabSmart`, was `CmdNextTab`. `Ctrl + Shift + Tab` is now `CmdPrevTabSmart`, was `CmdPrevTab`. You can [re-bind it](Customizing-keyboard-shortcuts.md) if you prefer old behavior
- `CmdCommandPalette` takes optional `mode` argument: `@` for tab selection, `#` for selecting from file history and `>` for commands.
- command palette no longer shows combined tabs/file history/commands. `CmdCommandPalette` only shows commands. Because of that removed `CmdCommandPaletteNoFiles` because now ``CmdCommandPalette` behaves like it
- removed `CmdCommandPaletteOnlyTabs`, replaced by`CmdCommandPaletteNoFiles @`
- `Ctrl + Shift + K` no longer active, use `Ctrl + K`. You can restore this shortcut by binding it to `CmdCommandPalette >` command.
- add `Name` field for shortcuts. If given, the command will show up in Command Palette (`Ctrl + K`)
- closing a current tab now behaves like in Chrome: selects next tab (to the right). We used to select previously active tab, but that's unpredicable and we prefer to align SumatraPDF behavior with other popular apps.
- swapped key bindings: 'i' is now CmdTogglePageInfo, 'I' is CmdInvertColors. Several people were confused by accidentally typing 'i' to invert colors, is less likely to type it accidentally
- allow creating custom themes in advanced settings in `Themes` section. [See docs](https://www.sumatrapdfreader.org/docs/Customize-theme-colors).
- improve scrolling with middle click drag [#4529](https://github.com/sumatrapdfreader/sumatrapdf/issues/4529)

### 3.5.2 (2023-10-25)

- fix not showing tab text
- make menus in dark themes look more like standard menus (bigger padding)
- fix Bookmarks for folder showing bad file names
- update translations

### 3.5.1 (2023-10-24)

- fix uninstaller crash
- disable lazy loading of files when restoring a session

### 3.5 (2023-10-23)

- Arm 64-bit builds
- dark mode (menu `Settings / Theme` or `Ctrl + K` command `Select next theme`)
  you can use `i` (invert colors) to match the background / text color of rendered
  PDF document. Due to technical limitations, it doesn't work well with images
- `i` (invert colors) is remembered in settings
- `CmdEditAnnotations` select annotation under cursor and open annotation edit window
- rename `CmdShowCursorPosition` => `CmdToggleCursorPosition`
- add `Annotations [ FreeTextColor, FreeTextSize, FreeTextBorderWidth ]` settings
- ability to move annotations. `Ctrl + click` to select annotation and them move via drag & drop
- add `CmdCommandPaletteOnlyTabs` command with `Alt + K` shortcut
- exit full screen / presentation modes via double click with left mouse button
- ability to drag out a tab to open it in new window
- support opening `.avif` images (including inside .cbz/,cbr files)
- respect image orientation `exif` metadata in .jpeg and .png images
- support Adobe Reader syntax for opening files `/A "page=<pageno>#nameddest=<dest>search=<string>`
- add `Next Tab` / `Prev Tab` commands with `Ctrl + PageUp` / `Ctrl + PageDown` shortcuts
- keep Home tab open; add `NoHomeTab` advanced option to disable that
- add context menu to tabs
- bugfix: handle files we can't open in `next file in folder` / `prev file in folder` commands
- command palette: when search starts with `>`, only show commands, not files (like in Visual Studio Code)
- add `reopen last closed` command (`Ctrl + Shift + T`, like in web browsers)
- add `clear history` command
- can send commands via [DDE](https://www.sumatrapdfreader.org/docs/DDE-Commands)
- added `CmdOpenWithExplorer`, `CmdOpenWithDirectoryOpus`, `CmdOpenWithTotalCommander`, `CmdOpenWithDoubleCommander` commands
- enable `CmdCloseOtherTabs`, `CmdCloseTabsToTheRight` commands from command palette
- recognize `PgUp` / `PgDown` and a few more in keyboard shortcuts
- add `-disable-auto-rotation` cmd-line print option
- add `-dde` cmd-line option

### 3.4.6 (2022-06-08)

- fix crashes
- fix hang in Fit Content mode and Bookmark links

### 3.4.5 (2022-06-05)

- fix crashes

### 3.4.4 (2022-06-02)

- restore `HOME` and `END` in find edit field
- fix crashes

### 3.4.3 (2022-05-29)

- re-enable `Backspace` in edit field
- fix installation for all users when using custom installation directory
- re-enable `Copy Image` context menu for comic book files
- fix display of some PDF images
- fix slow loading of some ePub files

### 3.4.2 (2022-05-27)

- make keyboard accelerators work when tree view has focus
- fix `-set-color-range` and `-bg-color` replacing `MainWindowBackground`
- fix crash with incorrectly defined selection handlers

### 3.4.1 (2022-05-25)

- fix downloading of symbols for better crash reports

### 3.4 (2022-05-24)

- [Command Palette](Command-Palette.md)
- [customizable keyboard shortcuts](Customizing-keyboard-shortcuts.md)
- better support for epub files using mupdf's epub engine. Adds text selection and search in ebook files. Better rendering fidelity. On the downside, might be slower.
- [search / translate selected text](Customize-search-translation-services.md) with web services
  - we have few built-in and you can [add your own](https://www.sumatrapdfreader.org/settings/settings3-4#SelectionHandlers)
- installer: `-all-users` cmd-line arg for system-wide install
- added `Annotations.TextIconColor` and `TextIconType` advanced settings
- added `Annotations.UnderlineColor` advanced setting
- added `Annotations.DefaultAuthor` advanced setting
- `i` keyboard shortcuts inverts document colors `Shift + i` does what `i` used to do i.e. show page number
- `u` and `Shift + u` keyboard shortcuts adds underline annotation for currently selected text
- `Delete` / `Backspace` keyboard shortcuts delete an annotation under mouse cursor
- support .svg files
- faster scrolling with mouse wheel when cursor over scrollbar
- add `-search` cmd-line option and `[Search("<file>", "<search-term>")]` DDE command
- a way to get list of used fonts in properties window
- support opening `.heic` image files (if Windows heic codec is installed)
- add experimental smooth scrolling (enabled with `SmoothScroll` advanced setting)

### 3.3.3 (2021-07-20)

- fix a crash in PdfFilter.dll

### 3.3.2 (2021-07-19)

- restore showing Table Of Contents for .chm files
- fix crashes

### 3.3.1 (2021-07-14)

- fix rotation in DjVu documents

### 3.3 (2021-07-06)

- added support for adding / removing / editing annotations in PDF files. Read [the tutorial](Editing-annotations.md)
- new toolbar

  - changed toolbar to scale with DPI by using new, vector icons
  - added rotate left / right to the toolbar
  - new toolbar:

  ![Toolbar](img/toolbar.png)

- added ability to hide scrollbar (more screen space for the document). Use right-click context menu.
- add `-paperkind=${num}` printing option ([checkin](https://github.com/sumatrapdfreader/sumatrapdf/pull/1815/commits/2104e6104ea759dc4f839c7e8be5973f5a4f0488))

Minor improvements and bug-fixes::

- advanced setting to change font size in bookmarks / favorites tree view e.g. `TreeFontSize = 12`
- support newer versions of ghostscript (≥ 9.54) for opening .ps files
- support jpeg-xr images in .xps files
- restore tooltips (regression in 3.2)
- update mupdf to latest version
- make silent installation always silent
- don't crash when attempting to zoom in on home page
- don't show "manga" view menu item for documents that are not comic books
- allow opening `fb2.zip` files ([#1657](https://github.com/sumatrapdfreader/sumatrapdf/issues/1657))
- restore ability to save embedded files (fixes [#1557](https://github.com/sumatrapdfreader/sumatrapdf/issues/1557))
- `Alt + Space` opens a sys menu

### 3.2 (2020-03-15)

This release upgrades the core PDF parsing and rendering library mupdf to the latest version. This fixes PDF rendering bugs and improves performance.

Added support multiple windows with tabs:

- added `File / New Window` (`Ctrl-n`) which opens a new window
- to compare the same file side-by-side, `Ctrl-Shift-n` shortcut opens current file a new window. The same file is now opened in 2 windows that you can re-arrange as needed
- `-new-window` cmd-line option will open the document in new window
- if you hold `SHIFT` when drag&dropping files from Explorer (and other apps), the file will be opened in a new window

Improved management of favorites:

- context menu (right mouse click) on the document area adds menu items for:
  - showing / hiding favorites view
  - adding current page to favorites (or removing if already is in favorites)
- context menu in bookmarks view adds menu item for adding selected page to favorites

This release no longer supports Windows XP. Latest version that support XP is 3.1.2 that you can download from

[https://www.sumatrapdfreader.org/download-prev.html](https://www.sumatrapdfreader.org/download-prev.html)

### 3.1.2 (2016-08-14)

- fixed issue with icons being purple in latest Windows 10 update
- tell Windows 10 that SumatraPDF can open supported file types

### 3.1.1 (2015-11-02)

- (re)add support for old processors that don’t have SSE2
- support newer versions of unrar.dll
- allow keeping browser plugin if it’s already installed
- crash fixes

### 3.1 (2015-10-24)

- 64bit builds
- all documents are restored at startup if a window with multiple tabs is closed (or if closing happened through File -> Exit); this can be disabled through the `RestoreSession` advanced setting
- printing happens (again) always as image which leads to more reliable results at the cost of requiring more printer memory; the "Print as Image" advanced printing option has been removed
- scrolling with touchpad (e.g. on Surface Pro) now works
- many crash and other bug fixes

### 3.0 (2014-10-18)

- Tabs! Enabled by default. Use Settings/Options... menu to go back to the old UI
- support table of contents and links in ebook UI
- add support for PalmDoc ebooks
- add support for displaying CB7 and CBT comic books (in addition to CBZ and CBR)
- add support for LZMA and PPMd compression in CBZ comic books
- allow saving Comic Book files as PDF
- swapped keybindings:
  - F11 : Fullscreen mode (still also Ctrl+Shift+L)
  - F5 : Presentation mode (also Shift+F11, still also Ctrl+L)
- added a document measurement UI. Press 'm' to start. Keep pressing 'm' to change measurement units
- new advanced settings: FullPathInTitle, UseSysColors (no longer exposed through the Options dialog), UseTabs
- replaced non-free UnRAR with a free RAR extraction library. If some CBR files fail to open for you, download unrar.dll from https://www.rarlab.com/rar_add.htm and place it alongside SumatraPDF.exe
- deprecated browser plugin. We keep it if was installed in earlier version

### 2.5.2 (2014-05-13)

- use less memory for comic book files
- PDF rendering fixes

### 2.5.1 (2014-05-07)

- hopefully fix frequent ebook crashes

### 2.5 (2014-05-05)

- 2 page view for ebooks
- new keybindings:
  - Ctrl+PgDn, Ctrl+Right : go to next page
  - Ctrl+PgUp, Ctrl+Left : go to previous page
- 10x faster ebook layout
- support JP2 images
- new **[advanced settings](https://www.sumatrapdfreader.org/settings.html)**: ShowMenuBar, ReloadModifiedDocuments, CustomScreenDPI
- left/right clicking no longer changes pages in fullscreen mode (use Presentation mode if you rely on this feature)
- fixed multiple crashes and made multiple minor improvements

### 2.4 (2013-10-01)

- full-screen mode for ebooks (Ctrl-L)
- new key bindings:
  - F9 - show/hide menu (not remembered after quitting)
  - F8 - show/hide toolbar
- support WebP images (standalone and in comic books)
- support for RAR5 compressed comic books
- fixed multiple crashes

### 2.3.2 (2013-05-25)

- fix changing a language via Settings/Change Language

### 2.3.1 (2013-05-23)

- don't require SSE2 (to support old computers without SSE2 support)

### 2.3 (2013-05-22)

- greater configurability via **[advanced settings](https://www.sumatrapdfreader.org/settings.html)**
- "Go To Page" in ebook ui
- add View/Manga Mode menu item for Comic Book (CBZ/CBR) files
- new key bindings:
  - Ctrl-Up : page up
  - Ctrl-Down : page down
- add support for OpenXPS documents
- support Deflate64 in Comic Book (CBZ/CBR) files
- fixed missing paragraph indentation in EPUB documents
- printing with "Use original page sizes" no longer centers pages on paper
- reduced size. Installer is ~1MB smaller
- downside: this release no longer supports very old processors without **[SSE2 instructions](https://en.wikipedia.org/wiki/SSE2)**. Using SSE2 makes Sumatra faster. If you have an old computer without SSE2, you need to use 2.2.1.

### 2.2.1 (2013-01-12)

- fixed ebooks sometimes not remembering the viewing position
- fixed Sumatra not exiting when opening files from a network drive
- fixes for most frequent crashes and PDF parsing robustness fixes

### 2.2 (2012-12-24)

- add support for FictionBook ebook format
- add support for PDF documents encrypted with Acrobat X
- “Print as image” compatibility option in print dialog for documents that fail to print properly
- new command-line option: -manga-mode [1|true|0|false] for proper display of manga comic books
- many robustness fixes and small improvements

### 2.1.1 (2012-05-07)

- fixes for a few crashes

### 2.1 (2012-05-03)

- support for EPUB ebook format
- added File/Rename menu item to rename currently viewed file (contributed by Vasily Fomin)
- support multi-page TIFF files
- support TGA images
- support for some comic book (CBZ) metadata
- support JPEG XR images (available on Windows Vista or later, for Windows XP the **[Windows Imaging Component](https://www.microsoft.com/en-us/download/details.aspx?id=32)** has to be installed)
- the installer is now signed

### 2.0.1 (2012-04-08)

- fix loading .mobi files from command line
- fix a crash loading multiple .mobi files at once
- fix a crash showing tooltips for table of contents tree entries

### 2.0 (2012-04-02)

- support for **[MOBI](https://blog.kowalczyk.info/articles/mobi-ebook-reader-viewer-for-windows.html)** eBook format
- support opening CHM documents from network drives
- a selection can be copied to a clipboard as an image by using right-click context menu
- using ucrt to reduce program size

### 1.9 (2011-11-23)

- support for **[CHM](https://blog.kowalczyk.info/articles/chm-reader-viewer-for-windows.html)** documents
- support touch gestures, available on Windows 7 or later. Contributed by Robert Prouse
- open linked audio and video files in an external media player
- improved support for PDF transparency groups

### 1.8 (2011-09-18)

- improved support for PDF form text fields
- various minor improvements and bug fixes
- speedup handling some types of djvu files

### 1.7 (2011-07-18)

- favorites
- improved support for right-to-left languages e.g. Arabic
- logical page numbers are displayed and used, if a document provides them (such as i, ii, iii, etc.)
- allow to restrict SumatraPDF's features with more granularity; see **[sumatrapdfrestric.init](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/docs/sumatrapdfrestrict.ini)** for documentation
- -named-dest also matches strings in table of contents
- improved support for EPS files (requires Ghostscript)
- more robust installer
- many minor improvements and bugfixes

### 1.6 (2011-05-30)

- add support for displaying DjVu documents
- display Frequently Read list when no document is open
- add support for displaying Postscript documents (requires recent Ghostscript version to be already installed)
- add support for displaying a folder containing images: drag the folder to SumatraPDF window
- support clickable links and a Table of Content for XPS documents
- display printing progress and allow to cancel it
- add Print toolbar button
- experimental: previewing of PDF documents in Windows Vista and 7. Creates thumbnails and displays documents in Explorer's Preview pane. Needs to be explicitly selected during install process. We've had reports that it doesn't work on Windows 7 x64.

### 1.5.1 (2011-04-26)

- fixes for rare crashes

### 1.5 (2011-04-23)

- add support for viewing XPS documents
- add support for viewing CBZ and CBR comic books
- add File/Save Shortcut menu item to create shortcuts to a specific place in a document
- add context menu for copying text, link addresses and comments. In browser plugin it also adds saving and printing commands
- add folder browsing (Ctrl+Shift+Right opens next PDF document in the current folder, Ctrl+Shift+Left opens previous document)

### 1.4 (2011-03-12)

- browser plugin for Firefox/Chrome/Opera (Internet Explorer is not supported). It's not installed by default so you have to check the apropriate checkbox in the installer
- IFilter that enables full-text search of PDF files in Windows Desktop Search (i.e. search from Windows Vista/7's Start Menu). Also not installed by default
- scrolling with right mouse button
- you can choose a custom installation directory in the installer
- menu items for re-opening current document in Foxit and PDF-XChange (if they're installed)
- we no longer compress the installer executable with mpress. It caused some anti-virus programs to falsely report Sumatra as a virus. The downside is that the binaries on disk are now bigger. Note: we still compress the portable .zip version
- -title cmd-line option was removed
- support for AES-256 encrypted PDF documents
- fixed an integer overflow reported by Jeroen van der Gun and and other small fixes and improvements to PDF handling

### 1.3 (2011-02-04)

- improved text selection and copying. We now mimic the way a browser or Adobe Reader works: just select text with mouse and use Ctrl-C to copy it to a clipboard
- Shift+Left Mouse now scrolls the document, Ctrl+Left mouse still creates a rectangular selection (for copying images)
- 'c' shortcut toggles continuous mode
- '+' / '\*' on the numeric keyboard now do zoom and rotation
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

### 1.2 (2010-11-26)

- improved printing: faster and uses less resources
- add Ctrl-Y as a shortcut for Custom Zoom
- add Ctrl-A as a shortcut for Select All Text
- improved full screen mode
- open embedded PDF documents
- allow saving PDF document attachements to disk
- latest fixes and improvements to PDF rendering from mupdf project

### 1.1 (2010-05-20)

- added book view (“View/Book View” menu item) option. It’s known as “Show Cover Page During Two-Up” in Adobe Reader
- added “File/Properties” menu item, showing basic information about PDF file
- added “File/Send by email” menu
- added export as text. When doing “File/Save As”, change “Save As types” from “ PDF documents” to “Text documents”. Don’t expect miracles, though. Conversion to text is not very good in most cases.
- auto-detect commonly used TeX editors for inverse-search command
- bug fixes to PDF handling (more PDFs are shown correctly)
- misc bug fixes and small improvements in UI
- add Ctrl + and Ctrl – as shortcuts for zooming (matches Adobe Reader)

### 1.0.1 (2009-11-27)

- many memory leaks fixed (Simon Bünzli)
- potential crash due to stack corruption (pointed out by Christophe Devine)
- making Sumatra default PDF reader no longer asks for admin priviledges on Vista/Windows 7
- translation updates

### 1.0 (2009-11-17)

- lots of small bug fixes and improvements

### 0.9.4 (2009-07-19)

- improved PDF compatibility (more types of documents can be rendered)
- added settings dialog (contributed by Simon Bünzli)
- improvements in handling unicode
- changed default view from single page to continuous
- SyncTex improvements (contributed by William Blum)
- add option to not remember opened files
- a new icon for documents association (contributed by George Georgiou)
- lots of bugfixes and UI polish

### 0.9.3 (2008-10-07)

- fix an issue with opening non-ascii files
- updated Japanese and Brazillian translation

### 0.9.2 (2008-10-06)

- ability to disable auto-update check
- improved text rendering - should fix problems with overlapping text
- improved font substition for fonts not present in PDF file
- can now open PDF files with non-ascii names
- improvements to DDE (contributed by Danilo Roascio)
- SyncTex improvements
- improve persistance of state (contributed by Robert Liu)
- fix crash when pressing 'Cancel' when entering a password
- updated translations

### 0.9.1 (2008-08-22)

- improved rendering of some PDFs
- support for links inside PDF file
- added -restrict and -title cmd-line options (contributed by Matthew Wilcoxson)
- enabled SyncTex support which mistakenly disabled in 0.9
- misc fixes and translation updates

### 0.9 (2008-08-10)

- add Ctrl-P as print shortcut
- add F11 as full-screen shortcut
- password dialog no longer shows the password
- support for AES-encrypted PDF files
- updates to SyncTeX/PdfSync integration (contributed by William Blum)
- add -nameddest command-line option and DDE commands for jumping to named destination(contributed by Alexander Klenin)
- add -reuse-instance command-line option (contributed by William Blum)
- add DDE command to open PDF file (contributed by William Blum)
- removed poppler rendering engine resulting in smaller program and updated to latest mupdf sources
- misc bugfixes and translation updates

### 0.8.1 (2008-05-27)

- automatic reloading of changed PDFs (contributed by William Blum)
- tex integration (contributed by William Blum)
- updated icon for case-sensitivity selection in find (contributed by Sonke Tesch)
- language change is now a separate dialog instead of a menu
- remember more settings (like default view)
- automatic checks for new versions
- add command-line option -lang $lang
- add command-line option -print-dialog (contributed by Peter Astrand)
- ESC or single mouse click hides selection
- fix showing boxes in table of contents tree
- translation updates

### 0.8 (2008-01-01)

- added search (contributed by MrChuoi)
- added table of contents (contributed by MrChuoi)
- added many translation
- new program icon
- fixed printing
- fixed some crashes
- rendering speedups
- fixed loading of some PDFs
- add command-line option -esc-to-exit
- add command-line option -bgcolor $color

### 0.7 (2007-07-28)

- added ability to select the text and copy to clipboard - contributed by Tomek Weksej
- made it multi-lingual (13 translations)
- added Save As option
- list of recently opened files is updated immediately
- fixed .pdf extension registration on Vista
- added ability to compile as DLL and C# sample application - contributed by Valery Possoz
- mingw compilation fixes and project files for CodeBlocks - contributed by MrChuoi
- fixed a few crashes
- moved the sources to Google Code project hosting

### 0.6 (2007-04-29)

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

### 0.5 (2007-03-04)

- fixed rendering problems with some PDF files
- speedups - the application should feel be snappy and there should be less waiting for rendering
- added 'r' keybinding for reloading currently open PDF file
- added <Ctrl>-<Shift>-+ and <Ctrl>-<Shift>-- keybindings to rotate clockwise and counter-clockwise (just like Acrobat Reader)
- fixed a crash or two

### 0.4 (2007-02-18)

- printing
- ask before registering as a default handler for PDF files
- faster rendering thanks to alternative PDF rendering engine. Previous engine is available as well.
- scrolling with mouse wheel
- fix toolbar issues on win2k
- improve the way fonts directory is found
- improvements to portable mode
- uninstaller completely removes the program
- changed name of preferences files from prefs.txt to sumatrapdfprefs.txt

### 0.3 (2006-11-25)

- added toolbar for most frequently used operations
- should be more snappy because rendering is done in background and it caches one page ahead
- some things are faster

### 0.2 (2006-08-06)

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

### 0.1 (2006-06-01)

- first version released
