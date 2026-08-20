# Version history

**next: 3.7**

Available in [pre-release](https://www.sumatrapdfreader.org/prerelease) builds.

- **Uniform Page Width** in the View menu keeps mixed-size pages at the same displayed width at percentage zoom levels, using page 1 as the reference. The chosen reading size stays unchanged when the window or fullscreen size changes, and the choice is remembered per document (fixes #5512)
- Document Properties for digitally signed PDFs shows the hash and signature algorithms, the document hash, the issuer and expiry, whether the signature is LTV-enabled, an RFC 3161 timestamp when one is present (device time vs secure TSA time), a PAdES level when the signature is CAdES, and whether the certificate is on the EU Trusted List. **View Certificate...** opens the Windows certificate dialog; **Update EU Trusted List** downloads the EU LOTL (fixes #5581)
- Kindle Print Replica (`.azw4`) files open as the embedded PDF instead of as a reflowable MOBI ebook (fixes #1315)
- keyboard shortcuts accept `AltGr` (also `RAlt` / `RightAlt`) as a modifier, the same as `Ctrl + Alt` on Windows, so bindings like `AltGr + Return` and `AltGr + Shift + Return` work (fixes #5973). Set Screenshot Hotkey can record Return and the arrow keys when a modifier (including AltGr) is held; unmodified Return / arrows are ignored so they are not registered as a global hotkey
- Command Palette: `Ctrl + A` selects the query; `Home` / `End` jump to the first / last match when the caret is already at the start / end of the query (`Ctrl + Home` / `Ctrl + End` always do); `Page Up` / `Page Down` move the list a page at a time (fixes #5972)
- Command Palette: type `%` for the table of contents of the current document (like `$` for favorites). `% TOC` appears in the mode row when the document has a TOC. The old `*` prefix still works
- connecting Remote Desktop at a different scale no longer leaves the toolbar, title bar, bookmarks and menus at the old size; they follow the session DPI without restarting (fixes #4581)
- installer `-x` no longer tries to overwrite the running `SumatraPDF.exe` when extracting into that exe's own directory; extract errors are printed to an already-attached console (fixes #6003)
- comic book archives of 32 MB or less on a network drive are loaded into memory instead of being copied to the local `cbx-cache`
- PDFs opened from OneNote or Outlook (and other host-app cache folders) are copied first so the original file is not left locked; OneNote can sync the section instead of showing "We can't sync this section because we were denied access to the file" (fixes #4705)
- if a PDF has no outline, the Bookmarks sidebar is filled from numbered headings in the text (`I. Introduction`, `II.A. Nested`, `1.2 Title`) so you can still jump around papers and reports that never stored a TOC (fixes #5724)
- Command Palette mode switches at the top (`#` File History, `>` Commands, …) use the same smaller font as the hints at the bottom
- Explorer and Outlook preview pane: Ctrl+wheel zooms, drag pans when zoomed in, and the wheel pans then turns the page at the edge. Double-click returns to fit-page (fixes #859)
- Toggle Page Boxes (`CmdTogglePageBoxes`) outlines the PDF MediaBox, CropBox, BleedBox, TrimBox and ArtBox on each visible page — only boxes that page actually declares — and labels them `media`, `crop`, `trim`, and so on. For PDF development (print marks, trim vs crop). Palette and Debug menu; no default shortcut (fixes #814)
- Change Language is a modeless window like Change Theme (search box, language list, OK / Cancel) instead of a modal dialog
- Add Favorite is a modeless window like Change Theme instead of a modal dialog
- Change Scrollbar is a modeless window like Change Theme instead of a modal dialog
- Custom Zoom is a modeless window like Change Theme instead of a modal dialog
- Go to Page is a modeless window like Change Theme instead of a modal dialog (the toolbar page box is still used when the toolbar is visible)
- Enter password is a WindowBase dialog like Change Theme; it stays modal because opening an encrypted file has to wait for the password
- Set inverse search command line is a modeless window like Change Theme instead of a modal dialog
- Settings (Options) is a modeless window like Change Theme instead of a modal dialog
- Change Background Color and Change Tab Color share a modeless window like Change Theme instead of a modal dialog
- Navigate Files in Folder is a normal modeless window that stays open (no longer a popup that closes when it loses focus or after opening a file); Esc or the close button dismisses it, and Enter / double-click replaces the document in the current tab, while `Ctrl + Enter` / `Ctrl + double-click` switches to the tab already showing that file, or opens it in a new tab. `Alt + Up` goes to the parent directory (like Explorer), as does the `..` entry. `Del` moves the selected file to the recycle bin without asking (directories are not deleted); if that file is open in a tab, the tab closes first. The window uses the app icon and the regular UI font (fixes #5877). The listing re-reads the directory whenever the window is activated or the command is invoked again, so files renamed (`F2`), added or removed meanwhile show up; `F5` refreshes on demand (fixes #5878). It also works on the home page — there's a **Navigate Files in Folder** link next to **Open a document...** — starting in the folder of the most recently opened document
- Renamed the companion engine DLL from `libmupdf.dll` to `libsumatrapdf.dll` (through 3.6 the name was `libmupdf.dll`; 3.7 and later use `libsumatrapdf.dll`). Installer upgrades move the old name aside; see [Portable vs installer](SumatraPDF-portable.md) and [Failed to load libsumatrapdf.dll](Failed-to-load-libmupdf.md)
- Themes can set optional UI colors (`DisabledTextColor`, `DarkerTextColor`, `HotBackgroundColor`, `EdgeColor`, `HotEdgeColor`, `DisabledEdgeColor`, `ErrorBackgroundColor`, and notification highlight colors) so disabled and hover states are not derived only from `TextColor` / backgrounds; built-in themes (including Dracula) set them so tinted foregrounds no longer look muddy yellow (issue #4721)
- new built-in themes: **One Dark**, **Monokai**, **Nord**, **GitHub Dark**, **Catppuccin Mocha**, **Tokyo Night**, **Gruvbox**, **Night Owl**, **Ayu**, and **Palenight** (common palettes from VS Code and other editors)
- theme list cleanup: removed **Darker** (folded into **Charcoal**, the former “Dark background Bright text”); settings that still name `Darker` or the old long name keep working
- Screen readers (Narrator, NVDA, and other UI Automation clients) can access document text on the canvas for PDF, XPS, and DjVu; the experimental UIA provider is now enabled in release builds, not only debug, reading through the document character by character, word by word, line by line or page by page now advances instead of repeating the first line, and reading the text under the mouse pointer or finger works (Narrator mouse mode / touch exploration), including telling the screen reader where that text is on screen (issue #321)
- smoother mouse-wheel and arrow-key scrolling when `SmoothScroll` is enabled (default **true**): continuous exponential chase of the target, sub-pixel steps, 1 ms timer while animating; set `SmoothScroll = false` for instant steps
- add `ScrollLineAmount` advanced setting: distance scrolled by an arrow-key press or one mouse-wheel line; defaults to 16 screen pixels at 96 DPI (fixes #2447)
- Favorites can open as a full-window tab so long paths and names use the whole window; the sidebar Favorites panel still works independently. The sidebar Favorites/ToC width can also be dragged past half the window (keeps ~200px for the document)
- Favorites list has a search box (like Bookmarks), in both the sidebar panel and the Favorites tab
- Favorites can be sorted alphabetically by name (or page label) within each file instead of by page number: `SortFavoritesByName = true`, or the **Sort By Name** checkbox in the Favorites tree context menu (fixes #2277)
- rename `CmdDiscardAnnotations` to `CmdDiscardChanges` ("Discard Changes"): reloads the current document from disk, dropping unsaved annotations and form changes; available from the tab context menu when there are unsaved changes and from the `Ctrl + K` command palette
- PDF bookmark / link destinations now apply Adobe-style view modes (`/Fit`, `/FitH`, `/FitV`, `/FitB`, `/FitBH`, `/FitBV`, `/XYZ` zoom) instead of only jumping to the page (fixes #5828)
- add `IgnoreDestinationZoom` advanced setting: when true, clicking a bookmark or a link inside the document keeps the zoom you are reading at instead of switching to the zoom the destination asks for; it still goes to the page and the position on it. The same option Adobe Reader and Foxit call "forbid the change of the current Zoom factor during execution of 'Go to Destination' actions"; off by default (discussion #5938)
- following an internal link or bookmark can flash a highlight at the destination so you can see where you landed (a bibliography entry, figure, or named dest). Uses the same color and fade as LaTeX forward search, and stays solid for about two seconds before fading so it is still there after the page jump. Off by default; turn it on with `HighlightLinkDestination = true` (fixes #1085, #5945)
- empty fillable PDF form fields are highlighted in pale blue so they are visible without hovering. Toggle with View → Highlight Form Fields, the command palette (`CmdToggleHighlightFormFields`), or the `HighlightFormFields` advanced setting (default **true**) (fixes #5966)
- add `DisableLinks` advanced setting (default **false**): when true, document links are ignored so you can select and read without accidentally following them — useful for drawings and schematics with many links. Toggle it from the command palette. The existing **Toggle Show Links** only draws a blue outline around links; it does not disable them (fixes #5939)
- add `RememberViewOffsetOnPageTurn` advanced setting (default **false**): next/previous page keeps the same view position on the new page instead of jumping to the top, so zoomed-in pages of similar size stay lined up. It applies to explicit page turns (`N` / `P`, click-to-turn, the toolbar arrows); wheeling off the bottom of a page still opens the next page at its top, because there you are continuing to read rather than turning a page (fixes #5069)
- add `MouseWheelTurnsPage` advanced setting (default **false**): one wheel notch goes to the next / previous page instead of scrolling, even when the page is zoomed past the window. Together with `RememberViewOffsetOnPageTurn` the view stays parked on the part of the page you are reading and the pages move under it, which is how sheet music and scans with wide margins are read without touching the keyboard. `Alt + wheel` still scrolls, so the rest of the page stays reachable, and `Shift + wheel` (horizontal) and `Ctrl + wheel` (zoom) are unchanged. Bind it to a key or a toolbar button with `CmdToggleBoolSetting MouseWheelTurnsPage` (fixes #5069)
- `EBookUI` and `ComicBookUI` each have their own `DefaultDisplayMode` so ebooks and comics can open in a different layout than PDFs (empty keeps the global `DefaultDisplayMode`) (fixes #2588)
- `ComicBookUI.DefaultZoom` sets the first-open zoom for comic archives (`.cbz`, `.cbr`, …); empty keeps the global `DefaultZoom`. Use `fit width` so new comics fill the window while PDFs stay at Fit Page (fixes #5946)
- Favorites remember how far you had scrolled on the page, so jumping to one returns you to that place, not just the top of the page
- in facing and book view, a landscape comic or image page (wider than it is tall) occupies the whole two-page row instead of pairing with the next page, so a double-page spread stored as one image displays as a centerfold. Off with `ComicBookUI.LandscapeAsSpread = false` (or `ImageUI.LandscapeAsSpread` for image folders); default is on (fixes #1324, #872)
- add `EBookUI.FontName` advanced setting: the font family for ebooks (EPUB, MOBI, FB2, …), e.g. `Segoe UI` or `Microsoft YaHei`. Empty (the default) keeps the engine serif. When set, it overrides the document's own `font-family` so you can read in a sans-serif or a CJK UI font without editing the file (fixes #3138)
- `EBookUI.FontName` (and `CustomCSS`) now win over the document even when it sets the font in an inline `style="font-family: ..."` or on a `<span>`, so you no longer need `IgnoreDocumentCSS = true` to read an ebook in the font you picked. A font name that can't be loaded is reported with a notification instead of silently doing nothing (fixes #4600)
- the ebook settings (font, size, line spacing, layout size, CSS) can be set for a single document, as an `EBookUI` block inside its entry in `FileStates`. Fields you leave out use the global `EBookUI` value, and nothing is written to the settings file for documents you haven't customized (fixes #4600)
- add `EBookUI.Margin`: the white space around the text of a reflowable document, in points, instead of the fixed 3 em / 2 em MuPDF uses. Written like a CSS margin - one number for all four sides, two for top/bottom and left/right, or four in top-right-bottom-left order. `0` gives the text the whole page. Also settable per document, and in the eBook Settings dialog (fixes #4600)
- new **Change eBook Settings** command (`CmdChangeEbookSettings`): a dialog with the font, size, margin and line spacing for a reflowable document, which shows the CSS it generates from them and lets you take over and write that CSS yourself. Applies to the open document only or to all ebooks (fixes #4600)
- add `ChmUI.FontName` advanced setting to choose the font in the CHM fixed-page view and override fonts specified by the document (fixes #2737)
- add `EBookUI.LineSpacing` advanced setting: set a line-height multiplier such as `1.5` for more space between lines in EPUB, MOBI, FB2 and other reflowable ebooks; 0 keeps the document or engine default (fixes #476)
- **Toggle Manga Mode** is available for PDF, XPS, DjVu, ebooks, images and other fixed-page documents, not only comic books. It puts facing and book-view pages in right-to-left order and remembers that choice per document (fixes #2258)
- `DefaultDisplayMode` accepts `page aspect`: on first open of a PDF, XPS, DjVu or PostScript file, pick the layout from page 1 — taller than wide uses continuous + fit width (papers), wider than tall uses single page + fit page (slides). A remembered view for that file still wins (fixes #4055)
- add `Fullscreen.DisplayMode` advanced setting: page layout to use in presentation (`Ctrl + L`) and windowed fullscreen (`Shift + Ctrl + L` / `F11`). Empty (the default) keeps the current behavior — presentation still goes to single page and fit-page, windowed fullscreen keeps whatever layout you had. Set it to `facing` or `book view` to switch to that layout on enter and restore the previous layout on exit (fixes #4753)
- add `SidebarOnRight` advanced setting: when true, the bookmarks / favorites sidebar is on the right of the window instead of the left. Right-to-left UI languages already put it on the right; this setting does the same in left-to-right languages (fixes #2165)
- add `ClickEdgeToTurnPage` advanced setting: when true, a click (not a drag) on the left fifth of the page area goes to the previous page and a click on the right fifth goes to the next page — useful for comics and for reading with a mouse or touchpad. In manga (right-to-left) mode the sides are reversed. Off by default so a click still selects text. Presentation mode (`Ctrl + L`) already turns pages on click and is unchanged (fixes #1203)
- the floating Find window (`Ctrl + F`, then pop out) can limit a search to a page range: **Limit to pages 1-N:** accepts lists such as `3,4-6,18-`, or a single span `10-25`, `10`, `10-`, `-25`. Empty (the default) still searches the whole document. The `n / m` counter and the results list follow the same range (fixes #5694)
- warning notifications ("File not found", "Errors in document", update messages) use an amber background in every theme instead of a color derived from the theme accent, which came out as a loud purple in Dracula and a bright blue in GitHub Dark, and was indistinguishable from a normal notification in most other dark themes. Links inside a warning use the warning text color so they stay readable (fixes #5876)
- while a search is still scanning, the Find status shows the page it is currently on next to the running match count (e.g. `520 61`), so a long search over a big document visibly progresses even across stretches with no matches
- the compact Find bar can be resized: drag its left edge to make the search field wider or narrower (the bar stays anchored to the right edge and keeps its height)
- select text with the keyboard, like a browser's caret browsing: `F7` (**Select Text With Keyboard**) puts a text caret in the page. The arrow keys move it, `Shift + arrows` extend the selection, `Home` / `End` go to the ends of the line and `Ctrl + Home` / `Ctrl + End` to the ends of the document, `Ctrl + Left` / `Ctrl + Right` move by word, and `PageUp` / `PageDown` by a screenful. `v` switches to visual mode, where the arrows extend the selection without holding Shift (like Vimium's caret / visual modes). `Ctrl + C` or `y` copies and leaves the mode, `Esc` or `F7` leaves it without copying (fixes #4684, #4116)
- extend a text selection from the keyboard by a character or a word, whichever way it was started (mouse drag, double-click, `Ctrl + A` or `F7`): the **Extend Selection One Character/Word Left/Right** commands. They have no default shortcut because `Ctrl + Shift + Left/Right` already navigate between files, so assign your own in the `Shortcuts` section of the advanced settings (discussion #5922)
- laser pointer for presentations: the **Toggle Laser Pointer** command turns the mouse cursor over the document into a glowing red laser dot, so you can point at what you're talking about. It has no default shortcut — assign one in the `Shortcuts` section of the advanced settings — and while it's on the cursor no longer auto-hides in presentation mode. Invoke it again to get the normal cursors back (fixes #5930)
- zoom to a selection: select an area with `Ctrl + drag` (or select text), then **Zoom / To Selection** (`Ctrl + 4`, also in the right-click menu) scales the view so the selection fills the window and centers it. The selection is kept, so you can still copy it, and `Alt + Left` (Navigate Back) returns to the view you zoomed from, zoom included (fixes #1699)
- follow links from the keyboard, like Vimium: `Shift + F` labels visible links with letters and highlights them; type a hint to follow the link (internal destination or URL). Prefix-free multi-letter hints are used when needed. `Shift + F` again or `Esc` leaves the mode. The labels follow the page while you scroll and are recalculated shortly after scrolling stops. Not offered for comic books, image folders and images, whose pages can't have links (fixes #2629)
- Bookmarks sidebar: **Collapse All** expands a single top-level root one level when that is all the outline has (typical Word-export TOC), and the context menu has **Expand to Level 1/2/3** for explicit depth control (fixes #5239)
- Bookmarks context menu **Collapse Same Level** collapses every outline entry that shares the parent of the selected/clicked item (siblings at that nesting level) (fixes #1895)
- FB2 document properties (`Ctrl+D`) now show the book annotation from `title-info` as Subject (fixes #2254)
- hovering a link that goes to a place inside the document shows the description the PDF gives it (the link annotation's `/Contents`), the way hovering an external link shows its URL; before, such links showed nothing (fixes #1724)
- clicking a component on an Altium Designer schematic PDF shows a popup with the part's properties (comment, footprint, value, …). A line that contains a URL can be opened. Those hotspots used Acrobat JavaScript and previously did nothing (fixes #1198)
- bookmarks (PDF outline) entries are drawn with the color and the bold / italic style the document asks for again; they had been shown as plain text since 3.3 (fixes #3560)
- zoom further than 6400%, for documents like large maps where the old limit hid detail that is in the file: the largest level in the `ZoomLevels` advanced setting is now the maximum zoom, so adding e.g. `12800 25600 51200` to it zooms that far. Values up to 1000000 are accepted; a given document is also limited by its own size, since all of its pages have to fit on one canvas. [Scrolling and zooming](Scrolling-and-zooming.md) documents the built-in levels as a `ZoomLevels` line to copy and edit (fixes #1195)
- add `ToolbarCustomLayout` advanced setting: the built-in toolbar buttons you want and the order you want them in, e.g. `ToolbarCustomLayout = CmdFindFirst | PageInfo CmdGoToPrevPage CmdGoToNextPage`. Leaving a button out hides it, `|` is a separator and `PageInfo` is the page number box; empty (the default) is the standard layout, which [Customize toolbar](Customize-toolbar.md) documents as a layout you can edit down (fixes #5095)
- the annotation editor's list of annotations and the Contents box of the selected annotation grow with the window instead of being a fixed 5 (or 14) lines, so making the window taller shows more annotations and more of the annotation's text rather than more empty space (fixes #3769, #5834)
- new `Advanced Settings...` dialog (`Ctrl + K` command palette): a filterable list of the advanced settings where you can toggle booleans, pick enum values from a drop-down and edit strings, colors and numbers in-place, then `Save`. Replaces the per-setting toggle commands (`CmdToggleSmoothScroll`, `CmdToggleEscToExit`, `CmdToggleReuseInstance` etc.), which were removed
- can open and view Markdown documents (`.md`, `.markdown`): they render as formatted text (GitHub Flavored Markdown, including tables, task lists and strikethrough) via the rendering engine, and the installer registers the file association so they open from Explorer and drag&drop
- Markdown (WebView2 UI): fenced `mermaid` code blocks are rendered as diagrams (bundled Mermaid runtime, works offline). Fixed-page MuPDF markdown still shows the source code
- in Markdown and HTML documents, a relative link to a document rather than to another page — `[the manual](./manual.pdf)`, an `.epub`, a `.cbz` — opens that document in SumatraPDF (`Ctrl + click` opens it in a new window); files of a type we can't open are handed to the shell instead. Links to other pages, images and other web resources still follow inside the document view (discussion #5924)
- Markdown documents have a **Show Generated HTML** command (`Ctrl + K` command palette) that saves the rendered HTML to a temporary `.html` file and opens it in Notepad
- HEIC / HEIF still images now decode with a built-in decoder (no Windows HEIC codec required for most phone photos); AVIF still uses dav1d. The system WIC path remains as a fallback if built-in decode fails
- Read Aloud: adjustable playback speed — pick 0.5x .. 3x in the new `Speed` submenu (next to `Voice` in the Read Aloud menu, toolbar dropdown and context menu) or click the speed button on the playback bar to cycle presets (right-click cycles backwards); the speed persists across sessions via the `ReadAloudSpeed` advanced setting
- clicking an empty signature field in a PDF opens **Sign Document** with that field already selected, instead of doing nothing and leaving the command to be found in a menu (fixes #5964)
- **Sign Document** can use a certificate from the current user's Windows certificate store (the Personal / MY store), not only a `.pfx` / `.p12` file. The drop-down lists store certificates that have a private key; pick **Certificate file...** to keep the old file-and-password path (fixes #5965)
- adding a new signature (**New signature on page N**) now asks you to click or drag on the page to place it, instead of putting it in a fixed corner. A selection already on the page is still used (fixes #5967)
- **Sign Document** appearance: choose which lines the signature draws (labels such as "Digitally signed by", the name, the distinguished name, the date), and optionally a PNG or JPEG on the left instead of the large name (fixes #5963)
- EPUB, MOBI and HTML documents show WebP images instead of an IMAGE placeholder (fixes #3415). Standalone `.webp` files and comics already worked.
- updated the bundled MuPDF rendering engine to 1.28.2
- add [AI Chat with document](AI-Chat-with-document.md) sidebar (in View menu and `Ctrl + k` [command palette](Command-Palette.md)) for asking questions about the open PDF or image via [Claude Code](https://docs.anthropic.com/en/docs/claude-code); per-tab session state, model/effort selection, and session history from `~/.claude/projects/`
- add `ClaudeCode` advanced settings (`Model`, `Effort`, `SkipPermissions`, `BgColor`, `SidebarDx`) in `SumatraPDF-settings.txt`
- auto-link plain-text DOIs in PDF text (e.g. `10.1109/WICSA.2015.29` opens `https://doi.org/...`); uses the same auto-detect path as URLs and email addresses
- add `DisableAutoLinks` advanced setting to disable auto-linking of URLs, email addresses, and plain-text DOIs found in PDF text (fixes #5703)
- add `ShowDocumentFocusIndicator` advanced setting: when true, draws a focus ring around the document when it has keyboard focus (Tab to the page area); off by default (fixes #4644)
- add `ShowAnnotationNotification` advanced setting: when false, disables the tip shown when hovering an annotation (e.g. "Highlight annotation. Ctrl+click to edit."); on by default (fixes #4501)
- `TabWidth` advanced setting is now DPI-scaled so it takes effect on HiDPI / high-scaling displays (fixes #3850)
- Bookmarks sidebar shows page numbers (labels) right-aligned on each entry; disable with `ShowTocPageNumbers = false` (fixes #3288)
- page-info tip (`I` key) is preserved across tab switches and visits to the Home page (fixes #4454)
- page-info tip shows extra detail for images: comics / image folders list the current image file name and size (both pages when two are visible in facing view); a single image file shows pixel resolution, size, and DPI when it is not the default 96 (fixes #4456)
- `-new-window` with several file arguments opens one new window containing all files as tabs, instead of a separate window per file (fixes #5044)
- Manga mode (R2L): Left/Right arrows and horizontal swipe reverse so Left advances and Right goes back, matching right-to-left reading (fixes #3964)
- starting a search (Ctrl+F) remembers the current page as favorite `/` so you can jump back via Favorites or the command palette `$` mode (fixes #5726)
- PDFs with a Catalog `/OpenAction` GoTo destination open at that page on first load (when there is no remembered view); URI/Launch/JavaScript open actions are ignored (fixes #1631)
- add `PaddingAfterLastPage` advanced setting: when true, continuous view has extra scroll room after the last page so you can bring the end of the document up (e.g. when the window is partly covered); off by default (fixes #411)
- document properties page size shows cm, mm, in (locale unit first) and pixels, e.g. `21.0 x 29.7 cm, 210 x 297 mm, 8.27 x 11.69 in, 595 x 842 px` (fixes #2186)
- Zoom menu shows the current level (radio/check mark) when using custom `ZoomLevels` and zooming with +/- (fixes #5832)
- the `FixedPageUI.SelectionColor` advanced setting now honors an alpha channel: set an `#aarrggbb` value (e.g. `#40f5fc0c`) to make the text-selection overlay more transparent so selected text stays crisp instead of looking washed out; `#rrggbb` keeps the previous default opacity (fixes #3209)
- middle-click auto-scroll is now smooth: it's driven by a high-frequency timer with fractional-pixel accumulation instead of a coarse 20ms timer with integer steps, so it no longer looks choppy (also enables fine, slow scroll speeds) (fixes #2693)
- associated file types now show their localized name in Explorer's "Type" column on non-English Windows; previously registration hardcoded an English name like "PDF File", overriding the name Windows would localize (fixes #3323)
- expand the table of contents tree down to the current page's entry and select it, like Explorer's "Expand to current folder" (in the Bookmarks sidebar right-click menu and the `Ctrl + k` command palette) (fixes #1998)
- Save As now warns instead of failing silently when a file can't be written (e.g. the destination path exceeds the Windows `MAX_PATH` limit); previously there was no way to tell the save hadn't happened (fixes #1016)
- can convert an image to a PDF: right-click an image (or an open image document) and choose `Image / Convert to PDF`, or pick `PDF` in the format drop-down of the Save Image dialog. The new PDF gets `CreationDate`/`ModDate` metadata with the current time and time zone (fixes #949)
- **Image / Save** defaults to an exact copy: the dest file uses the original format (`.jpg` for a JPEG, …) and Save writes the original file bytes unless you crop, resize, or change the extension
- in the Favorites pane and menu, a favorite for a file with a long name now shows your favorite's name first, then the file name, so the name you gave it is no longer pushed out of view (fixes #829, #2236)
- case-insensitive search now treats German ß as equivalent to `ss`, so searching `Strasse` finds `Straße` and vice versa (fixes #933)
- hovering a thumbnail on the Frequently Read home page now shows a ✕ button in its top-right corner to remove that document from the list, without going through the right-click menu (fixes #283)
- home page document history: `Del` removes the keyboard-selected entry from history (shown as a shortcut on **Remove From History** in the right-click menu); the menu also has **Delete File** (recycle bin + drop from history) and **Show in folder** opens the in-app Navigate Files in Folder picker on that file's directory
- Open File (`Ctrl + O`) can use either the standard Windows file picker or the in-app Navigate Files in Folder window (`FilePicker = os | sumatrapdf`, empty means Windows). Toggle with **Settings / SumatraPDF File Picker** or the same check under **File** (below Open). **Open File With Windows File Picker** (`CmdOpenFileWithOSFilePicker`) always uses the system dialog. The home page no longer shows a separate Navigate Files in Folder link

- the home page document history can be shown as a list instead of thumbnails (toggle buttons next to the header, or the `HomePageViewMode = thumbnails | list` advanced setting). Each list row shows a small preview, the file name, the file's directory (right-aligned, muted), the file size, and remove/pin buttons (fixes #4909)
- new zoom mode `Fit by Orientation` (in the View / Zoom menu) that automatically fits width when the view is landscape and fits page when portrait, updating as you resize the window or rotate the screen (fixes #702)
- new zoom mode `Fit Height` (View / Zoom menu and command palette): scales the page so its height fills the window (width may require horizontal scrolling) — useful for landscape pages on portrait screens and for mixed page widths with a stable vertical size; also accepted as `-zoom fitheight` / advanced setting `fit height` and DDE zoom `-6` (fixes #1714)
- comics / images: advanced settings `ComicBookUI.LimitToWindowWidth` / `LimitToWindowHeight` and the same under `ImageUI` — when true, absolute zoom never makes a page wider (or taller) than the window; each page is capped independently so double-page spreads stay on screen while single pages can stay large (fixes #2197)
- comic book archives (`.cbz`, `.cbr`, …) whose images live in chapter folders now show those folders as nested Bookmarks (table of contents) entries. A directory shared by every file is still omitted so a single-folder comic stays a flat list. ComicInfo.xml bookmarks still win when present (fixes #5317)
- **Convert to PDF** (`CmdConvertToPDF`) for comic books (`.cbz`, `.cbr`, …), image folders, and single images: dialog suggests a unique `.pdf` path next to the source; original images are embedded when possible (fixes #4118, #5532). Docs: [Convert to PDF](Convert-to-PDF.md)
- **Convert PDF to Images** (`CmdConvertPdfToImages`): dialog like Extract Text, with a destination path template (`<N>` is the page number), a PNG / JPEG / BMP format drop-down that updates the extension, and **Current** / **All** / **Custom** page radios (custom takes a range such as `1,3-5`). Writes one file per page at 150 DPI; PNGs are losslessly recompressed in the background (fixes #5991). Docs: [Convert PDF to Images](Convert-PDF-to-images.md)
- Explorer preview / thumbnails for FictionBook: plain `.fb2` and zip containers `.fb2z`, `.fbz`, `.zfb2`, and `.fb2.zip` (reinstall or re-enable the preview handler to pick up the extra extensions) (fixes #1677)
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
- the toolbar can be placed at the top or bottom of the window via the new `ToolbarPosition = top | bottom` advanced setting (works in both show and overlay modes)
- DjVu documents can be rendered with a new built-in plain-C decoder (`ext/djvudec`) instead of libdjvu. Choose it with the new `DjvuEngine = djvudec | libdjvu` advanced setting (defaults to `djvudec`); toggle and reload the current document with the `CmdToggleDjvuEngine` command (`Ctrl + k` command palette)
- add `EBookUI.BackgroundColor` advanced setting to override background color for ebook documents (epub, mobi etc.)
- add `ComicBookUI.BackgroundColor` advanced setting to override the default black background for comic book files
- add `ImageUI.BackgroundColor` advanced setting to override the default black background for image files
- background color settings (`FixedPageUI.BackgroundColor`, `EBookUI.BackgroundColor`, `ComicBookUI.BackgroundColor`, `ImageUI.BackgroundColor`) accept `checkered` value to show a checkerboard transparency pattern
- change document background color per-file or for all files of the same type (via `Ctrl + k` [command palette](Command-Palette.md))
- `Ctrl + click` on a PDF link opens it in a new tab (instead of navigating in the current tab)
- you can now drag&drop selected text to another application, like a text editor
- triple-click selects the whole line of text (double-click still selects a word) (fixes #694)
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
- move `DefaultImageZoom` advanced setting to `ImageUI.DefaultZoom`, default to `shrink to fit`
- improve Toggle Use Tabs: you can now transition between using tabs / not using tabs without restarting the app
- allow showing menu bar when using tabs (previously menu bar was only shown when not using tabs)
- capture screenshots of the desktop and all visible windows, saved as PNG files in `Screenshots` sub-directory of SumatraPDF data directory (global hotkey e.g. PrtSc requires a Shortcuts entry)
- you can drag&drop images from a browser onto SumatraPDF window. We'll download it to Downloads folder and open
- crop and resize images when viewing image files
- `Ctrl + V` pastes image from clipboard, saves as PNG in Downloads folder and opens it
- Can save images in different formats: PNG, JPEG, BMP, GIF, TIFF.
- add `Fullscreen` advanced setting with `Toolbar` (show / hide / overlay, like `Toolbar`) and `ShowMenubar` options for fullscreen mode. Use `F8` / `F9` to toggle toolbar / menubar while in fullscreen. Legacy `ShowToolbar` / `Fullscreen.ShowToolbar` remain for migration only
- add `Show Errors` in right-click context menu for PDF documents that have mupdf warnings/errors
- replace `HideScrollbars` and `UseOverlayScrollbar` settings with `Scrollbars` setting (values: `windows`, `smart`, `overlay`, `hidden`)
- save and restore groups of tabs; saved groups are persisted in `TabGroups` advanced setting
- add `Shrink To Fit` zoom mode: shows at 100% if page is smaller than view area, otherwise fits page
- add `TabsMru` advanced setting to change order of navigating tabs when using `Ctrl + Tab`
- add `CtrlTabSimple` advanced setting: when true, `Ctrl + Tab` / `Ctrl + Shift + Tab` switch to the next / previous tab immediately, in tab-strip order, without showing the tab switcher (the behavior before 3.6)
- the tab switcher (`Ctrl + Tab`) window is sized to the number of tabs instead of always being a tall, mostly empty window
- improve document properties for comic book files (CBZ, CBR, CB7, CBT). We now show list of image files and per-image EXIF metadata
- improve document properties for image files: size, dimensions, DPI, exif metadata
- support encrypted .cbz, .cbr files
- you can drag&drop images from PDF documents to other applications (web apps, image editors, file explorer etc.)
- pen/stylus input now works for text selection on Windows tablets
- use `GetFileAttributesEx` instead of opening files for change detection on network drives, avoiding Windows Defender re-scans
- add citation/reference hover preview: hovering an internal-document link (e.g. a `[1]` citation, figure reference, or footnote marker) now shows a small popup rendering the destination region, so you can see the bibliography entry / figure / footnote without leaving the current page. The `CitationHoverDelay` advanced setting sets the hover delay in ms (-1 disables the popup) (fixes [#128](https://github.com/sumatrapdfreader/sumatrapdf/issues/128), [#4221](https://github.com/sumatrapdfreader/sumatrapdf/issues/4221))
- translate selected text with Grok Build, Claude Code, OpenAI Codex, or Antigravity when the corresponding CLI is installed (selection context menu); opens a dialog to edit the text, pick source and destination languages, and show the translation inline
- add a **Match whole word** toggle to the Find bar (next to **Match Case**) so a search only matches complete words, e.g. `cat` no longer matches `category` (fixes #4295)
- when searching, the **current** match is now highlighted with the customizable `FixedPageUI.SelectionColor` (the color users tune to be most noticeable) and all other matches use a secondary orange highlight, so the active match is easier to spot; previously it was the other way around (fixes #5740)
- find-as-you-type now waits briefly after you stop typing before searching (500 ms, or 1 s for 1–2 character terms) instead of searching on every keystroke; pressing Enter searches immediately (fixes #4626)
- add **Go to Next/Previous Favorite** commands (`Ctrl + k` command palette) that jump to the nearest favorite (bookmark) page after / before the current page (fixes #3744)
- can paste an image from the clipboard into a PDF as an image stamp annotation: right-click → **Create Annotation Under Cursor** → **Image From Clipboard** (or the `Ctrl + k` command palette). The stamp is created at the click point, sized to the image, and immediately selected so it can be moved/resized
- text in some PDFs that use embedded subset fonts naming glyphs like `G45` (with no `ToUnicode` map) is now extracted and searchable: previously such text came out as `�` and couldn't be found (e.g. searching "Emergency" failed). mupdf now recovers Unicode from those glyph names the way pdf.js does (fixes #3219)
- add `AllowExternalImages` advanced setting (off by default): when on, a PDF may display an image stored in a separate file referenced by name (an "external image stream"); the file must sit next to the PDF. Off by default for security, matching Acrobat (fixes #3731)
- add **Set Inverse Search Command Line** (`Ctrl + K` [command palette](Command-Palette.md)): opens a standalone dialog to configure the SyncTeX inverse-search command (with detected TeX editors in a drop-down and a Help link to [LaTeX integration](LaTeX-integration.md)); OK saves `InverseSearchCmdLine` and enables TeX enhancements, Cancel leaves settings unchanged. Works from the home page without an open document
- one-click light/dark theme switching: **Toggle Light/Dark Theme** (`Ctrl + K` command palette) flips between the last used light and dark themes (remembered in the new `LastLightTheme` / `LastDarkTheme` advanced settings). **Follow Windows** in the Change Theme dialog (or `Theme = System` in advanced settings) makes SumatraPDF follow the Windows light/dark app mode, switching automatically when the OS does (fixes #5995). Inspired by the [SumatraPDF Plus](https://github.com/dengxibo/sumatrapdf-plus) fork
- caption (title bar) minimize/maximize/restore/close buttons are drawn with Windows 11 style rounded glyphs (Segoe Fluent Icons outlines), DPI-scaled (from the [SumatraPDF Plus](https://github.com/dengxibo/sumatrapdf-plus) fork)
- new built-in **Light Warm** theme: a warm, paper-like "eye-care" chrome palette (from the [SumatraPDF Plus](https://github.com/dengxibo/sumatrapdf-plus) fork; their Dracula and pure-black dark palettes already exist here as the `Dracula` and `Dark` themes)
- smarter inverted (dark) page rendering for MuPDF documents (PDF, XPS, EPUB, MOBI, FB2, HTML, etc.): the `DocumentColorsFollowTheme` advanced setting (also via **Set Document Colors Follow Theme** in the `Ctrl + K` command palette and the **Change Theme** dialog) picks how page colors follow the UI theme: `off` (keep original page colors, the default), `smart` (recolor text and background but not images; **Invert Colors** / `Shift + I` toggles between `off` and `smart`), or `legacy` (recolor text, background and images — pre-3.7 behavior). **Toggle Preserve PDF Image Colors in Dark Mode** switches image preservation for the session. Ported from the [SumatraPDF Plus](https://github.com/dengxibo/sumatrapdf-plus) fork
- a small floating toolbar pops up after selecting text, with the most common selection actions: **Copy**, **Read Aloud**, **Highlight**, **Underline**, **Squiggly**, **Strike Out** (annotation buttons only for documents that support annotations). It appears half a second after the selection settles, so it does not flash in and out while you are still selecting. Disable it with the new `SelectionToolbar` advanced setting. Ported from the [SumatraPDF Plus](https://github.com/dengxibo/sumatrapdf-plus) fork
- add `Annotations.FreeTextAlignment` advanced setting (`left`, `center`, `right`): how text is aligned in newly created free text annotations, so the **Text Alignment** you want no longer has to be set by hand on every annotation. Right-to-left scripts (Arabic, Hebrew, Persian) want `right`. `CmdCreateAnnotFreeText` and the other `CmdCreateAnnot*` commands take a matching `alignment` argument (issue #4799)
- clearer rendering of CAD / engineering-drawing PDFs: hairline strokes get a zoom-aware minimum width and typical CAD-export grays are darkened toward Acrobat-like contrast, so drawings stay readable when zoomed out. Applied automatically when a drawing is detected (PDF/E marker, CAD authoring tool in the metadata, or content heuristics; screenshot/raster and WPS-style hairline exports are handled too); control it with the `EngineeringDrawingEnhance` advanced setting (`off` / `auto` / `on`) or per document with **Toggle Engineering Drawing Enhancement** (`Ctrl + K` command palette). Ported from the [SumatraPDF Plus](https://github.com/dengxibo/sumatrapdf-plus) fork
- **Fit Content** (`Ctrl + 3`) now works on print-ready PDFs. Those draw crop and registration marks in the bleed area outside the page, and the content detection used to count them, so it reported the whole page and zooming to content did nothing. Ink drawn outside the page is now ignored. Same for **Shrink** / **Fit to printable area** printing, which uses the same detection. Fit Content also crops the margins properly in the non-continuous **Single Page**, **Facing** and **Book View** modes (it used to show the top/left margin with the content cut off at the other end), and in the continuous modes the mouse wheel now scrolls the document instead of flipping a whole page per notch
- PDF documents can be digitally signed from the UI: **Sign Document...** (in the **File** menu and in the right-click **Document** submenu, for PDFs only) asks for a certificate (a `.pfx` / `.p12` file plus its password) and an optional reason and location, then fills in an empty signature field the document already has, or adds a new signature: you click or drag on the page to place it (or it uses the current selection if there is one) and saves to a file you pick. Previously signing was only possible with the bundled `sumatrapdf-tool sign` command line. The signature is written incrementally, so signatures already in the document stay valid, and Document Properties reports the signer and whether the document changed since signing (fixes #5962, #5967)
- The Annotations window can select many annotations with the usual Windows keys (`Shift` or `Ctrl` click, `Shift` + arrows, `Ctrl + A`) and `Del` deletes all of them (fixes #5976)
- **Bake PDF** includes annotations created in the current session. It used to re-open the file from disk, so a bake without saving first dropped those annotations (fixes #5977)
- Find remembers the last 10 search queries for the session (not saved to settings) and offers them from a drop-down on the find field (fixes #893)

**New commands:**

- `CmdToggleUniformPageWidth` : "Toggle Uniform Page Width"
- `CmdAIChatWithClaudeCode` : "AI Chat"
- `CmdChangeBackgroundColor` : "Change Background Color"
- `CmdChangeTheme` : "Change Theme..." — dialog to pick a UI theme and document-color follow mode
- `CmdChangeScrollbar` : "Change Scrollbar"
- `CmdCommandPalette %` : command palette table-of-contents mode (`CmdCommandPaletteTOC`, `Shift + F12`)
- `CmdCommandPalette $` : command palette favorites mode (`CmdCommandPaletteFavorites`)
- `CmdCommandPaletteFavorites` : "Command Palette: Favorites"
- `CmdContinueReadAloud` : "Continue Reading"
- `CmdDeleteFileAndOpenNext` : "Delete File And Open Next" — opens the next file in the folder, then moves the previous file to the Recycle Bin (discussion #5845)
- `CmdFavoriteShowInTab` : "Show Favorites in Tab" — full-window Favorites tab (sidebar Favorites still works)
- `CmdToggleFavoritesSort` : "Sort Favorites By Name" — Favorites tree context menu checkbox; toggles `SortFavoritesByName` (fixes #2277)
- `CmdZoomFitHeight` : "Zoom: Fit Height" — scale page height to the window (fixes #1714)
- `CmdSelectTextViaKeyboard` : "Select Text With Keyboard" (`F7`) — caret browsing: move a text caret with the arrows and select without the mouse (fixes #4684, #4116)
- `CmdExtendSelectionCharLeft`, `CmdExtendSelectionCharRight`, `CmdExtendSelectionWordLeft`, `CmdExtendSelectionWordRight` : "Extend Selection One Character/Word Left/Right" — no default shortcut, bind your own (discussion #5922)
- `CmdToggleLaserPointer` : "Toggle Laser Pointer" — laser dot cursor over the document, no default shortcut, bind your own (fixes #5930)
- `CmdToggleHoverPreview` : "Toggle Hover Preview" — palette-only; enables/disables the citation/reference hover popup (`CitationHoverDelay`)
- `CmdToggleDisableLinks` : "Toggle Disable Links" — palette-only; toggles `DisableLinks` (fixes #5939)
- `CmdZoomToSelection` : "Zoom: To Selection" (`Ctrl + 4`) — zoom so the selection fills the window (fixes #1699)
- `CmdOpenFileWithOSFilePicker` : "Open File With Windows File Picker..." — always the system multi-select open dialog (when `FilePicker = sumatrapdf`, use this to force the Windows picker)
- `CmdToggleFilePicker` : "SumatraPDF File Picker" — check under Settings and File menus; toggles `FilePicker` between Windows and SumatraPDF
- `CmdToggleBoolSetting` : "Toggle Boolean Setting" — custom shortcut/toolbar command with a setting name argument, e.g. `CmdToggleBoolSetting Fullscreen.ShowMenubar` (fixes #5912)
- `CmdFixDefaultApp` : "Fix Default App For Extension" — `CmdFixDefaultApp .pdf` opens the OS default-app UI for that extension; home page shows a bottom bar with fix links when registered extensions are no longer default
- `CmdToggleKeyboardLinkFollowing` : "Follow Link With Keyboard" (`Shift + F`) — labels visible links with Vimium-style letter hints; type a hint to follow its link (fixes #2629)
- `CmdFindToggleMatchWholeWord` : "Find: Toggle Match Whole Word" — Find bar toggle button
- `CmdGoToNextFavorite` : "Go to Next Favorite"
- `CmdNavigateFilesInFolder` : "Navigate Files in Folder" (`Ctrl + Shift + Up`) — directory browser for openable files (stays open; Enter/double-click replaces the current tab or enters a directory, `Ctrl + Enter`/`Ctrl + double-click` uses an existing or new tab, `..` goes up, `Del` deletes the selected file, `F5` refreshes, Esc closes)
- `CmdGoToPrevFavorite` : "Go to Previous Favorite"
- `CmdCreateAnnotImageFromClipboard` : "Create Image Annotation From Clipboard"
- `CmdStopReadAloud` : "Stop Reading"
- `CmdReadAloudFromTopPage` : "Start Reading From Top"
- `CmdReadAloudSelection` : "Start Reading Selection"
- `CmdConvertImageToPdf` : "Convert Page To PDF" — image editor path for one page
- `CmdConvertToPDF` : "Convert To PDF..." — comic / image folder / image → multi-page PDF (fixes #4118, #5532)
- `CmdConvertPdfToImages` : "Convert PDF to Images..." — PDF pages → PNG / JPEG / BMP (fixes #5991)
- `CmdCropImage` : "Crop Image"
- `CmdDocumentExtractText` : "Extract Text From Document"
- `CmdDocumentShowOutline` : "Show Document Outline"
- `CmdExpandToCurrentPage` : "Expand TOC to Current Page"
- `CmdTocExpandToLevel1` : "Bookmarks: Expand to Level 1" — Bookmarks context menu
- `CmdTocExpandToLevel2` : "Bookmarks: Expand to Level 2" — Bookmarks context menu
- `CmdTocExpandToLevel3` : "Bookmarks: Expand to Level 3" — Bookmarks context menu
- `CmdTocCollapseSameLevel` : "Bookmarks: Collapse Same Level" — Bookmarks context menu
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
- `CmdDeleteCachedFiles` : "Delete Cached Files" — deletes local network-drive comic book cache (`cbx-cache`)
- `CmdResizeImage` : "Resize Image"
- `CmdScreenshot` : "Take Screenshot"
- `CmdSetScreenshotHotkey` : "Set Screenshot Hotkey"
- `CmdSetInverseSearch` : "Set Inverse Search Command Line" — opens a dialog to configure the SyncTeX inverse-search command (`Ctrl + k` command palette)
- `CmdShowGeneratedHTML` : "Show Generated HTML" — available for Markdown documents
- `CmdSetTabColor` : "Set Tab Color"
- `CmdStartAutoScroll` : "Start Auto-Scroll"
- `CmdTabGroupRestore` : "Restore Tab Group"
- `CmdTabGroupSave` : "Save Tab Group"
- `CmdToggleDjvuEngine` : "Toggle DjVu Engine" (command palette shows the target, e.g. "set to libdjvu")
- `CmdSetDocumentColorsFollowTheme` : "Set Document Colors Follow Theme"
- `CmdToggleEngineeringDrawingEnhance` : "Toggle Engineering Drawing Enhancement" — per-document override of the `EngineeringDrawingEnhance` advanced setting
- `CmdToggleLightDarkTheme` : "Toggle Light/Dark Theme"
- `CmdTogglePreservePdfImages` : "Toggle Preserve PDF Image Colors in Dark Mode" — session-only toggle
- `CmdToggleWindowsPreviewer` : "Toggle Windows Previewer"
- `CmdToggleWindowsSearchFilter` : "Toggle Windows Search Filter"
- `CmdTranslateSelection` : "Translate Selection..." — dialog to translate the current selection
- `CmdTranslateSelectionWithClaudeCode` : "Translate Selection with Claude Code"
- `CmdTranslateSelectionWithGrokBuild` : "Translate Selection with Grok Build"
- `CmdTranslateSelectionWithOpenAICodex` : "Translate Selection with OpenAI Codex"
- `CmdTranslateSelectionWithAntiGravity` : "Translate Selection with Antigravity"
- `CmdZoomFitByOrientation` : "Fit by Orientation"
- `CmdZoomShrinkToFit` : "Shrink To Fit"
- `CmdDebugShowFitContentArea` : "Debug: Show Fit Content Area" — Debug menu checkbox; outlines in red the area **Fit Content** zoom would fit to, without changing the zoom
- `CmdTogglePageBoxes` : "Toggle Page Boxes" — palette / Debug menu; outlines PDF Media/Crop/Bleed/Trim/Art boxes that the page actually has (fixes #814)
- `CmdSignDocument` : "Sign Document..." — sign a PDF with a Windows-store or `.pfx` / `.p12` certificate (fixes #5962)

**New command-line arguments:**

- `-for-testing` : for ad-hoc testing; always starts a new instance, doesn't restore a session, doesn't save settings
- `-dbg-control <named-pipe>` : drive automated tests over a named-pipe request/response protocol (`tests/control.ts`)
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
- change: `Ctrl + Tab` is now `CmdNextTabSmart`, was `CmdNextTab`. `Ctrl + Shift + Tab` is now `CmdPrevTabSmart`, was `CmdPrevTab`. To restore pre-3.6 immediate switching (no switcher popup), [rebind the keys](Customize-keyboard-shortcuts.md#restore-pre-36-ctrltab-no-smart-tab-switch-popup) or use `Ctrl + PageDown` / `Ctrl + PageUp` — see [Tabs and windows](Tabs-and-windows.md#restore-pre-36-ctrltab-no-switcher-popup)
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
