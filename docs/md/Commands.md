# Commands

You can control SumatraPDF with commands:

- use [command palette](Command-Palette.md) (`Ctrl + K`) to invoke a command by its description
- [customize a keyboard shortcut](Customize-keyboard-shortcuts.md) to invoke a command by its id
- **ver 3.5+:** [send a command via DDE](DDE-Commands.md) e.g. `[CmdClose]` to invoke `Close Document` command
- **ver 3.6+:** some commands [accept arguments](Commands.md#commands-with-arguments)

:search:

## File

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdClose,"Ctrl + W, Ctrl + F4",Close Document,
CmdCloseCurrentDocument,q,Close Current Document,
CmdCommandPalette,Ctrl + K,Command Palette,
CmdCommandPaletteTOC,Shift + F12,Command Palette: Table Of Contents,"Jump to a table of contents entry of the current document, ver 3.7+"
CmdCommandPaletteFavorites,,Command Palette: Favorites,"Jump to a favorite, ver 3.7+"
CmdDuplicateInNewWindow,Shift + Ctrl + N,Open Current Document In New Window,
CmdDuplicateInNewTab,,Open Current Document In New Tab,ver 3.6+
CmdExit,Ctrl + Q,Exit Application,
CmdMoveFrameFocus,F6,Move Frame Focus,
CmdNewWindow,Ctrl + N,Open New SumatraPDF Window,
CmdOpenFile,Ctrl + O,Open File...,"uses the Windows file picker or Navigate Files in Folder according to the `FilePicker` advanced setting (empty/os = Windows, sumatrapdf = in-app), ver 3.7+"
CmdOpenFileWithOSFilePicker,,Open File With Windows File Picker...,"always the standard Windows multi-select file open dialog, ver 3.7+"
CmdToggleFilePicker,,SumatraPDF File Picker,"checkbox under File and Settings; toggles `FilePicker` empty/os ↔ sumatrapdf, ver 3.7+"
CmdToggleBoolSetting,,Toggle Boolean Setting,"custom shortcuts: `CmdToggleBoolSetting <SettingName>` toggles a boolean advanced setting (case-insensitive leaf or dotted path), e.g. `Fullscreen.ShowMenubar` (fixes #5912), ver 3.7+"
CmdFixDefaultApp,,Fix Default App For Extension,"`CmdFixDefaultApp .pdf` opens the OS dialog to set the default app for that extension; used by the home-page bottom bar when Sumatra is no longer the default, ver 3.7+"
CmdOpenNextFileInFolder,Shift + Ctrl + Right,Open Next File In Folder,
CmdNavigateFilesInFolder,Shift + Ctrl + Up,Navigate Files in Folder,"directory browser for openable files in the current file's folder (stays open; Enter/double-click replaces the current tab, Ctrl+Enter/Ctrl+double-click switches to the tab already showing the file or opens a new tab, Alt+Up goes to the parent directory, Del moves the selected file to the recycle bin, F5 re-reads the directory); also used when `FilePicker = sumatrapdf`, ver 3.7+"
CmdOpenPrevFileInFolder,Shift + Ctrl + Left,Open Previous File In Folder,
CmdOpenSelectedDocument,,Open Selected Document,
CmdPinSelectedDocument,,Pin Selected Document,
CmdPrint,Ctrl + P,Print Document...,
CmdProperties,Ctrl + D,Document Properties...,
CmdReloadDocument,r,Reload Document,
CmdRenameFile,F2,Rename File...,
CmdReopenLastClosedFile,Shift + Ctrl + T,Reopen Last Closed,
CmdSaveAs,Ctrl + S,Save File As...,
CmdToggleCursorPosition,m,Toggle Cursor Position,
CmdShowInFolder,,Show File In Folder...,
CmdToggleBookmarks,F12,Toggle Bookmarks,
CmdToggleTableOfContents,,Toggle Table Of Contents,ver 3.6+
CmdCollapseAll,,Collapse All,"Bookmarks: collapse the outline; if there is only one top-level entry with children, expand it one level (Word-style TOC), ver 3.7+"
CmdExpandAll,,Expand All,
CmdExpandToCurrentPage,,Expand TOC to Current Page,"In the Bookmarks (table of contents) sidebar, expand the tree down to the entry for the current page and select it, ver 3.7+"
CmdTocExpandToLevel1,,Bookmarks: Expand to Level 1,"Collapse the Bookmarks tree so only top-level entries are visible (context menu), ver 3.7+"
CmdTocExpandToLevel2,,Bookmarks: Expand to Level 2,"Expand top-level Bookmarks once (context menu), ver 3.7+"
CmdTocExpandToLevel3,,Bookmarks: Expand to Level 3,"Expand Bookmarks two levels deep (context menu), ver 3.7+"
CmdTocCollapseSameLevel,,Bookmarks: Collapse Same Level,"Collapse all Bookmarks siblings of the selected/clicked entry (same nesting level), ver 3.7+"
CmdOpenEmbeddedPDF,,Open Embedded PDF,
CmdSaveEmbeddedFile,,Save Embedded File...,
CmdCreateShortcutToFile,,Create .lnk Shortcut,
CmdSelectAll,Ctrl + A,Select All,
CmdExtendSelectionCharLeft,,Extend Selection One Character Left,"ver 3.7+, no default shortcut; grows or shrinks the existing text selection (see below)"
CmdExtendSelectionCharRight,,Extend Selection One Character Right,"ver 3.7+, no default shortcut; grows or shrinks the existing text selection (see below)"
CmdExtendSelectionWordLeft,,Extend Selection One Word Left,"ver 3.7+, no default shortcut; grows or shrinks the existing text selection (see below)"
CmdExtendSelectionWordRight,,Extend Selection One Word Right,"ver 3.7+, no default shortcut; grows or shrinks the existing text selection (see below)"
CmdCopyComment,,Copy Comment,
CmdCopyImage,,Copy Image,
CmdCopyLinkTarget,,Copy Link Target,
CmdCopySelection,"Ctrl + C, Ctrl + Insert",Copy Selection,
CmdCopyFilePath,,Copy File Path,ver 3.5+
CmdDeleteFile,,Delete Currently Opened File, ver 3.6+
CmdDeleteFileAndOpenNext,,Delete File And Open Next,"moves the current file to the Recycle Bin after the next file opens successfully, ver 3.7+"
CmdShowGeneratedHTML,,Show Generated HTML,"available for Markdown files; saves the generated HTML to a temporary .html file and opens it in Notepad, ver 3.7+"
```

## Search

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdFindFirst,Ctrl + F,Find,
CmdFindToggleMatchCase,,Find: Toggle Match Case,
CmdFindToggleMatchWholeWord,,Find: Toggle Match Whole Word,
CmdFindNext,F3,Find Next,
CmdFindNextSel,Ctrl + F3,Find Next Selection,
CmdFindPrev,Shift + F3,Find Previous,
CmdFindPrevSel,Shift + Ctrl + F3,Find Previous Selection,
```

## Viewing

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdBookView,"Ctrl + 8, Ctrl + Numpad 8",Book View,
CmdFacingView,"Ctrl + 7, Ctrl + Numpad 7",Facing View,
CmdInvertColors,Shift + I,Invert Colors,was `i` before 3.6
CmdRotateLeft,"[, Shift + Ctrl + Subtract",Rotate Left,
CmdRotateRight,"], Shift + Ctrl + Add",Rotate Right,
CmdSinglePageView,"Ctrl + 6, Ctrl + Numpad 6",Single Page View,
CmdToggleContinuousView,c,Toggle Continuous View,
CmdSelectTextViaKeyboard,F7,Select Text With Keyboard,"ver 3.7+, caret browsing: puts a text caret in the page which the arrow keys move; Shift + arrows extend the selection, v toggles visual mode (arrows select without Shift), Home/End go to the line ends, Ctrl + Home/End to the document ends, Ctrl + arrows move by word, Ctrl + C or y copies, Esc or F7 leaves the mode. Not available for documents with no extractable text (fixes #4684, #4116)"
CmdToggleKeyboardLinkFollowing,Shift + F,Follow Link With Keyboard,"ver 3.7+, labels visible links with Vimium-style letter hints; type a hint to follow its link, Esc or Shift + F again leaves the mode. Multi-letter hints are used when needed. Not available for comic books, image folders and images (fixes #2629)"
CmdToggleFullscreen,"f, Shift + Ctrl + L, F11",Toggle Fullscreen,
CmdToggleMangaMode,,Toggle Manga Mode,"Right-to-left facing/book layout for fixed-page documents; before 3.7 this was limited to comic books"
CmdToggleUniformPageWidth,,Toggle Uniform Page Width,"At percentage zoom levels, scales every page to the width page 1 has at that zoom; remembered per document (fixes #5512)"
CmdToggleMenuBar,F9,Toggle Menu Bar,
CmdTogglePageInfo,i,Show / Hide Current Page Number,was Shift + i before 3.6
CmdTogglePageBoxes,,Toggle Page Boxes,"ver 3.7+, outlines the PDF MediaBox, CropBox, BleedBox, TrimBox and ArtBox on each page (only boxes that page actually has) and labels them. Palette and Debug menu. No default shortcut (fixes #814)"
CmdChangeScrollbar,,Change Scrollbar,"Opens dialog to choose scrollbar mode (windows/smart/overlay/hidden)"
CmdChangeBackgroundColor,,Change Background Color,"Opens color picker to change document background color"
CmdChangeEbookSettings,,Change eBook Settings,"ver 3.7+, opens a dialog for the font, size, line spacing and CSS of a reflowable document (EPUB, MOBI, FB2, ...), for that document only or for all ebooks. Not shown for fixed-page documents. See Customize-eBook-UI.md"
CmdToggleToolbar,F8,Toggle Toolbar,
CmdAIChatWithClaudeCode,,AI Chat with document using Claude Code,"Toggle Claude Code chat sidebar, ver 3.7+. See AI-Chat-with-document.md"
CmdAIChatWithGrokBuild,,AI Chat with document using Grok Build,"Toggle Grok Build chat sidebar, ver 3.7+. See AI-Chat-with-document.md#grok-build"
CmdAIChatWithOpenAICodex,,AI Chat with document using OpenAI Codex,"Toggle OpenAI Codex chat sidebar, ver 3.7+. See AI-Chat-with-document.md#openai-codex"
CmdAIChatWithAntiGravity,,AI Chat with document using Antigravity,"Toggle Antigravity chat sidebar, ver 3.7+. See AI-Chat-with-document.md#antigravity"
CmdTranslateSelectionWithAntiGravity,,Translate Selection with Antigravity,"Translate selected text with Antigravity CLI, ver 3.7+"
CmdChangeTheme,,Change Theme...,"ver 3.7+, opens a dialog to pick a UI theme (including **Follow Windows**, which automatically tracks Windows light/dark app mode) and optionally how document colors follow the theme (`DocumentColorsFollowTheme`)"
CmdToggleLightDarkTheme,,Toggle Light/Dark Theme,"ver 3.7+, switches between the last used light and dark themes (see `LastLightTheme` / `LastDarkTheme` advanced settings)"
CmdToggleEngineeringDrawingEnhance,,Toggle Engineering Drawing Enhancement,"ver 3.7+, toggles CAD/engineering-drawing line enhancement for the current PDF (see the `EngineeringDrawingEnhance` advanced setting)"
CmdSetDocumentColorsFollowTheme,,Set Document Colors Follow Theme,"ver 3.7+, opens a dialog to pick how MuPDF-rendered documents follow the UI theme (`DocumentColorsFollowTheme`: off, smart, legacy)"
CmdTogglePreservePdfImages,,Toggle Preserve PDF Image Colors in Dark Mode,"ver 3.7+, session-only toggle of image preservation on inverted pages"
CmdToggleLinks,,Toggle Show Links,"Toggle drawing blue rectangle around links, ver 3.6+"
CmdToggleHighlightFormFields,,Toggle Highlight Form Fields,"ver 3.7+, toggles pale-blue highlight of empty fillable PDF form fields (`HighlightFormFields`; default on) (fixes #5966)"
CmdToggleTransparencyGrid,,Toggle Transparency Grid,"ver 3.7+, checkerboard under the page so transparent PDFs (white art on a hole) are visible, like Acrobat's Transparency Grid. Session-only, not saved (fixes #1809)"
CmdTogglePageGrid,,Toggle Page Grid,"ver 3.7+, dotted major/minor graph paper on PDF, EPUB, and other paginated document pages; not comics. Session-only visibility; spacing, origin, color and style are `FixedPageUI.PageGrid` (fixes #4398)"
CmdConfigurePageGrid,,Configure Page Grid...,"ver 3.7+, dialog to set page-grid units, spacing, subdivisions, origin, color and line style (dots / dotted / solid), like PDF-XChange Measurement. Reset to defaults restores the shipped appearance settings; Show Grid is the session toggle"
CmdToggleDisableLinks,,Toggle Disable Links,"ver 3.7+, palette-only; toggles `DisableLinks` so clicks, hover and keyboard following ignore document links (fixes #5939)"
CmdToggleHoverPreview,,Toggle Hover Preview,"ver 3.7+, palette-only; toggles the citation/reference hover popup (`CitationHoverDelay`: 300 ms when on, -1 when off)"
```

## Tabs

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdCloseAllTabs,,Close All Tabs,ver 3.6+
CmdCloseTabsToTheLeft,,Close Tabs To The Left,ver 3.6+
CmdCloseTabsToTheRight,,Close Tabs To The Right,ver 3.6+
CmdCloseOtherTabs,,Close Other Tabs,ver 3.6+
CmdNextTab,Ctrl + PageUp,Next Tab,
CmdPrevTab,Ctrl + PageDown,Previous Tab,
CmdMoveTabRight,Ctrl + Shift + PageUp,Move Tab Right,ver 3.6+
CmdMoveTabLeft,Ctrl + Shift + PageDown,Move Tab Left,ver 3.6+
CmdNextTabSmart,Ctrl + Tab,Smart tab Switch,ver 3.6+
CmdPrevTabSmart,Ctrl + Shift + Tab,Smart tab Switch,ver 3.6+
CmdTabGroupSave,,Save Tab Group,ver 3.7+
CmdTabGroupRestore,,Restore Tab Group,ver 3.7+
```

## Navigation

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdScrollUp,"k, Up",Scroll Up,
CmdScrollDown,"j, Down",Scroll Down,
CmdScrollLeft,"h, Left",Scroll Left,
CmdScrollRight,"l, Right",Scroll Right,
CmdScrollUpHalfPage,Shift + Up,Scroll Up By Half Page,
CmdScrollDownHalfPage,Shift + Down,Scroll Down By Half Page,
CmdStartAutoScroll,,Start Auto-Scroll,"Start (or stop) middle-click-style auto-scroll anchored at the cursor, without needing a middle mouse button; move the cursor away from the anchor to scroll. Invoke again (or middle-click) to stop, ver 3.7+"
CmdScrollUpPage,"Ctrl + Up, PageUp, Shift + Return, Shift + Space",Scroll Up By Page,
CmdScrollDownPage,"Ctrl + Down, PageDown, Return, Space",Scroll Down By Page,
CmdScrollLeftPage,Shift + Left,Scroll Left By Page,
CmdScrollRightPage,Shift + Right,Scroll Right By Page,
CmdGoToFirstPage,"Ctrl + Home, Home",First Page,
CmdGoToLastPage,"Ctrl + End, End",Last Page,
CmdGoToPrevPage,"p",Previous Page,
CmdGoToNextPage,"n",Next Page,
CmdGoToPage,"g, Ctrl + G",Go to Page...,
CmdNavigateBack,"Alt + Left, Backspace",Navigate Back,
CmdNavigateForward,"Alt + Right, Shift + Backspace",Navigate Forward,
```

## Favorites

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdFavoriteAdd,Ctrl + B,Add Favorite,
CmdFavoriteDel,,Delete Favorite,
CmdFavoriteToggle,,Toggle Favorites,
CmdFavoriteShowInTab,,Show Favorites in Tab,"Full-window Favorites tab (independent of sidebar), ver 3.7+"
CmdToggleFavoritesSort,,Sort Favorites By Name,"Toggle alphabetical order within each file (also Favorites tree context menu checkbox); maps to `SortFavoritesByName`, ver 3.7+"
CmdGoToNextFavorite,,Go to Next Favorite,
CmdGoToPrevFavorite,,Go to Previous Favorite,
```

## Presentation

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdTogglePresentationMode,"Ctrl + L, Shift + F11, F5",View: Presentation Mode,
CmdPresentationBlackBackground,.,Presentation Black Background,
CmdPresentationWhiteBackground,w,Presentation White Background,
CmdToggleLaserPointer,,Toggle Laser Pointer,"ver 3.7+, replaces the mouse cursor over the document with a glowing red laser dot for pointing things out. No default shortcut, assign your own. Stays visible in presentation mode (the cursor doesn't auto-hide while it's on)"
```

## Annotations

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdInsertImage,,Insert Image...,"Pick an image and stamp it onto the current page as a stamp annotation - a signature, say (fixes #1744), ver 3.7+"
CmdCreateAnnotCaret,,Create Caret Annotation,
CmdCreateAnnotCircle,,Create Circle Annotation,
CmdCreateAnnotFileAttachment,,Create File Attachment Annotation,
CmdCreateAnnotFreeText,,Create Free Text Annotation,
CmdCreateAnnotHighlight,"a, A",Create Highlight Annotation,
CmdCreateAnnotInk,,Create Ink Annotation,
CmdCreateAnnotLine,,Create Line Annotation,
CmdCreateAnnotLink,,Create Link Annotation,
CmdCreateAnnotPolygon,,Create Polygon Annotation,
CmdCreateAnnotPolyLine,,Create Poly Line Annotation,
CmdCreateAnnotPopup,,Create Popup Annotation,
CmdCreateAnnotRedact,,Create Redact Annotation,"marks selected text, or a dragged rectangle, for removal; content is not deleted until Apply Redactions, ver 3.7+"
CmdApplyRedactions,,Apply Redactions,"permanently deletes content marked with the Redact tool, after a confirmation; save a copy afterwards, ver 3.7+"
CmdCreateAnnotSquare,,Create Square Annotation,
CmdCreateAnnotSquiggly,,Create Squiggly Annotation,
CmdCreateAnnotStamp,,Create Stamp Annotation,
CmdCreateAnnotImageFromClipboard,,Create Image Annotation From Clipboard,
CmdCreateAnnotStrikeOut,,Create Strike Out Annotation,
CmdCreateAnnotText,,Create Text Annotation,
CmdCreateAnnotUnderline,"u, U",Create Underline Annotation,
CmdDeleteAnnotation,Delete,Delete Annotation,
CmdEditAnnotations,,Edit Annotations,
CmdSaveAnnotations,Shift + Ctrl + S,Save Annotations to existing PDF,
CmdSaveAnnotationsNewFile,,Save Annotations to new PDF,ver 3.6+
CmdSignDocument,,Sign Document...,"digitally sign a PDF with a certificate from the Windows store or a .pfx / .p12 file; choose which name/date/labels to draw and optionally a PNG/JPEG; a new signature is placed by clicking or dragging on the page, ver 3.7+"
CmdDiscardChanges,,Discard Changes,"reloads the document from disk, discarding unsaved annotations and form changes; also on the tab context menu when there are unsaved changes, ver 3.7+ (renamed from `CmdDiscardAnnotations`)"
CmdShowAnnotations,,Show Annotations,"ver 3.6+, for current document"
CmdHideAnnotations,,Hide Annotations,"ver 3.6+, for current document"
CmdToggleShowAnnotations,,Toggle Showing Annotations,"ver 3.6+, for current document"
CmdTogglePdfAnnotationsToolbar,,Toggle PDF Annotations Toolbar,"shows or hides the PDF annotation toolbar below the standard toolbar, ver 3.7+"
```

## Zoom

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdToggleZoom,z,Toggle Zoom,
CmdZoomActualSize,"Ctrl + 1, Ctrl + Numpad 1",Zoom: Actual Size,
CmdZoomCustom,Ctrl + Y,Zoom: Custom...,
CmdZoomFitContent,"Ctrl + 3, Ctrl + Numpad 3",Zoom: Fit Content,
CmdZoomToSelection,"Ctrl + 4, Ctrl + Numpad 4",Zoom: To Selection,"ver 3.7+, zooms so the current selection (Ctrl + drag rectangle or selected text) fills the window and centers it; the selection is kept so it can still be copied, and Navigate Back (Alt + Left) returns to the view it was zoomed from. Also in the Zoom menu and the right-click menu (fixes #1699)"
CmdZoomShrinkToFit,,Zoom: Shrink To Fit,"Shows at 100% if page is smaller than view area, otherwise fits page"
CmdZoomFitPage,"Ctrl + 0, Ctrl + Numpad 0",Zoom: Fit Page,
CmdZoomFitPageAndSinglePage,,Zoom: Fit Page and Single Page,
CmdZoomFitWidth,"Ctrl + 2, Ctrl + Numpad 2",Zoom: Fit Width,
CmdZoomFitHeight,,Zoom: Fit Height,"Scale so the page fills the window height (may scroll horizontally); useful for landscape pages on portrait screens (fixes #1714), ver 3.7+"
CmdZoomFitByOrientation,,Zoom: Fit Page or Width by Orientation,"Fit width when the view is wider than tall (landscape), fit page otherwise (portrait); re-evaluated as the window/screen is resized or rotated"
CmdZoomFitWidthAndContinuous,,Zoom: Fit Width And Continuous,
CmdZoomIn,Ctrl + Add,Zoom In,
CmdZoomOut,Ctrl + Subtract,Zoom Out,
CmdZoom100,,Zoom: 100%,
CmdZoom12_5,,Zoom: 12.5%,
CmdZoom125,,Zoom: 125%,
CmdZoom150,,Zoom: 150%,
CmdZoom1600,,Zoom: 1600%,
CmdZoom200,,Zoom: 200%,
CmdZoom25,,Zoom: 25%,
CmdZoom3200,,Zoom: 3200%,
CmdZoom400,,Zoom: 400%,
CmdZoom50,,Zoom: 50%,
CmdZoom6400,,Zoom: 6400%,
CmdZoom8_33,,Zoom: 8.33%,
CmdZoom800,,Zoom: 800%,
```

## File information / conversion

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdPdShowInfo,,Show PDF Info,shows information about currently opened PDF file
CmdDocumentShowOutline,,Show Document Outline,shows the outline (table of contents) of currently opened document
CmdPdfBake,,Bake PDF File,bakes interactive form and annotation content into static graphics; saves to a new PDF file and opens it
CmdDocumentExtractText,,Extract Text From Document,"extract text from document pages to a .txt file, with configurable page ranges, ver 3.7+"
```

## External app

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdOpenWithExplorer,,Open Directory In Explorer,ver 3.5+
CmdOpenWithDirectoryOpus,,Open Directory In Directory Opus,ver 3.5+
CmdOpenWithTotalCommander,,Open Directory In Total Commander,ver 3.5+
CmdOpenWithDoubleCommander,,Open Directory In Double Commander,ver 3.5+
CmdOpenWithAcrobat,,Open in Adobe Acrobat,
CmdOpenWithFoxIt,,Open in Foxit Reader,
CmdOpenWithFoxItPhantom,,Open in Foxit Phantom,
CmdOpenWithHtmlHelp,,Open in Microsoft HTML Help,
CmdOpenWithPdfDjvuBookmarker,,Open in Pdf&Djvu Bookmarker,
CmdOpenWithPdfXchange,,Open in PDF-XChange,
CmdOpenWithXpsViewer,,Open in Microsoft Xps Viewer,
CmdTranslateSelection,,Translate Selection...,"ver 3.7+, opens a dialog to translate the selected text (edit text, pick languages and engine); used from the selection context menu and command palette"
CmdTranslateSelectionWithDeepL,,Translate Selection With DeepL,
CmdTranslateSelectionWithGoogle,,Translate Selection with Google,
CmdTranslateSelectionWithGrokBuild,,Translate Selection with Grok Build,"ver 3.7+, requires the Grok Build CLI"
CmdTranslateSelectionWithClaudeCode,,Translate Selection with Claude Code,"ver 3.7+, requires the Claude Code CLI"
CmdTranslateSelectionWithOpenAICodex,,Translate Selection with OpenAI Codex,"ver 3.7+, requires the OpenAI Codex CLI"
CmdSearchSelectionWithBing,,Search Selection with Bing,
CmdSearchSelectionWithGoogle,,Search Selection with Google,
CmdSearchSelectionWithWikipedia,,Search Selection with Wikipedia,ver 3.6+
CmdSearchSelectionWithGoogleScholar,,Search Selection with Google Scholar,ver 3.6+
CmdSendByEmail,,Send Document By Email...,
CmdInvokeInverseSearch,,Invoke Inverse Search,ver 3.6+

```

## System

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdAdvancedOptions,,Advanced Options...,Opens the settings file in a text editor
CmdAdvancedSettings,,Advanced Settings...,"ver 3.7+, opens a dialog for viewing and editing the advanced settings: filter by name, click a setting to toggle / pick / edit its value, then Save"
CmdChangeLanguage,,Change Language...,
CmdCheckUpdate,,Check For Updates,
CmdClearHistory,,Clear History,Clears history of opened files (for recently opened list in home page)
CmdRemoveDeletedFilesFromHistory,,Remove Deleted Files From History,"ver 3.7+, removes from history entries for files that no longer exist on disk"
CmdDeleteCachedFiles,,Delete Cached Files,"ver 3.7+, deletes local copies of comic book archives cached under cbx-cache when opened from a network drive"
CmdContributeTranslation,,Contribute Translation,
CmdForgetSelectedDocument,,Remove Selected Document From History,
CmdListPrinters,,List Printers,ver 3.7+
CmdOptions,,Options...,
CmdSetInverseSearch,,Set Inverse Search Command Line,"ver 3.7+, opens a dialog to set the SyncTeX inverse-search command and enables TeX enhancements"
CmdScreenshot,,Take Screenshot,"ver 3.7+, requires Shortcuts entry (e.g. Key = PrtSc) to register global hotkey"
CmdCropImage,,Crop Image,ver 3.7+
CmdResizeImage,,Resize Image,ver 3.7+
CmdSaveImage,,Save Image,"Save image from context menu, ver 3.7+"
CmdConvertImageToPdf,,Convert Page To PDF,"Save the image under the cursor (or the current image document page) as a new PDF via the image editor, ver 3.7+"
CmdConvertToPDF,,Convert To PDF,"Convert a comic book, image folder, or single image to a multi-page PDF (dialog picks a unique .pdf path), ver 3.7+ (fixes #4118, #5532). Docs: Convert-to-PDF.md"
CmdConvertPdfToImages,,Convert PDF to Images...,"Render PDF pages to PNG, JPEG or BMP (format drop-down updates the extension). Destination path is a template: <N> is replaced by the page number. Pages: current, all, or a custom range. PNGs are optimized in the background, ver 3.7+ (fixes #5991). Docs: Convert-PDF-to-images.md"
CmdPasteClipboardImage,,Paste Image From Clipboard,"Paste image from clipboard and open it, ver 3.7+"
CmdShowErrors,,Show Errors,"Show mupdf warnings/errors in right-click context menu, ver 3.7+"
CmdShowLog,,Show Logs,
```

## Help

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdHelpOpenManual,F1,Help: Manual,
CmdHelpOpenKeyboardShortcuts,,Help: Keyboard Shortcuts,
CmdToggleKeyboardHelp,?,Show Keyboard Shortcuts,"Overlay listing the most useful commands by section, each row showing the key it is currently bound to. Press again (or Esc) to close it, ver 3.7+"
CmdHelpAbout,,Help: About SumatraPDF,
CmdHelpOpenManualOnWebsite,,Help: Manual On Website,
CmdHelpVisitWebsite,,Help: SumatraPDF Website,
```

## Misc

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdToggleInverseSearch,,Toggle Inverse Search,"temporarily disable TeX inverse search on left mouse click, ver 3.6+"
CmdToggleWindowsPreviewer,,Register / Un-register Windows Previewer,"Only available when SumatraPDF is installed. Registers or un-registers the PDF preview handler for Windows Explorer preview pane, ver 3.7+"
CmdToggleWindowsSearchFilter,,Register / Un-register Windows Search Filter,"Only available when SumatraPDF is installed. Registers or un-registers the PDF search filter for Windows Search indexing, ver 3.7+"
CmdSetTabColor,,Set Tab Color,"Set a custom color for the tab of the current document, available from tab context menu, ver 3.7+"
CmdPdfCompress,,Compress PDF,"Compress a PDF file using aggressive garbage collection and optimization, ver 3.7+"
CmdPdfDecompress,,Decompress PDF,"Decompress a PDF file for easier inspection, ver 3.7+"
CmdPdfDeletePages,,Delete Pages From PDF,"Delete pages from a PDF file using page ranges like 1,3-8,13-N where N is the last page, ver 3.7+"
CmdPdfExtractPages,,Extract Pages From PDF,"Extract pages from a PDF file using page ranges like 1,3-8,13-N where N is the last page, ver 3.7+"
CmdPdfEncrypt,,Encrypt PDF,"Encrypt a PDF file with a password using AES-256 encryption, ver 3.7+"
CmdPdfDecrypt,,Decrypt PDF,"Decrypt an encrypted PDF file, removing password protection, ver 3.7+"
CmdSetScreenshotHotkey,,Set Screenshot Hotkey,"Open dialog to set or remove a global hotkey for taking screenshots, ver 3.7+"
CmdReadAloud,,Read Aloud,"Read selected text (or from the viewport if no selection) through the end of the document using Windows text-to-speech. Invoking again pauses reading. Voice is chosen in the Read Aloud Voice submenu and remembered in ReadAloudVoiceId, ver 3.7+"
CmdPauseReadAloud,,Pause Reading,"Pause reading text aloud; resume with CmdContinueReadAloud, ver 3.7+"
CmdContinueReadAloud,,Continue Reading,"Continue reading text aloud from where it was paused, ver 3.7+"
CmdStopReadAloud,,Stop Reading,"Stop reading text aloud and clear the resume position. Always in the Read Aloud menu and command palette, even when the playback bar is not visible, ver 3.7+"
CmdReadAloudFromTopPage,,Start Reading From Top,"Read from the first visible text in the viewport through the end of the document, ver 3.7+"
CmdReadAloudSelection,,Start Reading Selection,"Read the current text selection aloud, ver 3.7+"
CmdToggleToolbarShowReadAloud,,Read Aloud: Show In Toolbar,"Show or hide the Read Aloud buttons in the toolbar; remembered in the `ToolbarShowReadAloud` setting, ver 3.7+"
```

## Debug

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdDebugCrashMe,,Debug: Crash Me,
CmdDebugDownloadSymbols,,Debug: Download Symbols,
CmdDebugShowNotif,,Debug: Show Notification,
CmdDebugStartStressTest,,Debug: Start Stress Test,
CmdDebugTestApp,,Debug: Test App,
CmdDebugTogglePredictiveRender,,Debug: Toggle Predictive Rendering,
CmdDebugToggleRenderInfo,,Debug: Toggle Render Queue Info,
CmdDebugToggleCacheInfo,,Debug: Toggle Cache Info,
CmdDebugToggleRtl,,Debug: Toggle Rtl,
CmdToggleImages,,Toggle Show Images,"Outline the images on the page, like the link outlines. A debug aid: it lasts for the session and is not saved in the settings, ver 3.7+"
CmdDebugShowFitContentArea,,Debug: Show Fit Content Area,"outlines in red the area Fit Content zoom would fit to (whole page if no content box was detected), without changing the zoom, ver 3.7+"
CmdNone,,Do nothing,
```

## Deprecated or internal

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdInstallPrereleaseUpdate,,internal,"used by the pre-release update notification link (Update); not for user shortcuts or DDE"
CmdTogglePdfPreviewLogging,,internal,"toggles PDF shell-preview logging for debugging the Windows preview handler; not for normal use"
CmdDebugCorruptMemory,,don't use,
CmdOpenWithKnownExternalViewerFirst,,don't use,
CmdOpenWithKnownExternalViewerLast,,don't use,
CmdSelectionHandler,,use SelectionHandlers advanced setting instead,
CmdSetTheme,,don't use,
CmdViewWithExternalViewer,,don't use,
CmdSaveAttachment,,don't use,
CmdOpenAttachment,,don't use,
CmdExec,,internal,"runs an external program with optional filter; used internally (e.g. selection handlers), not for normal shortcuts or DDE"
CmdDebugToggleDpiOverride,,internal,"debug builds only: cycles a pretend DPI (125, 150, 75, off) to check how the UI reacts to a DPI change; does nothing in a release build"
```

`CmdFindMatch` is an old name for `CmdFindToggleMatchCase`. It is not a generated command ID, but SumatraPDF still accepts it in old shortcut settings for compatibility.

# Commands with arguments

**Ver 3.6+:** some commands accept arguments which provides more capabilities when creating [custom keyboard shortcut](Customize-keyboard-shortcuts.md).

For example:

```
Shortcuts [
    [
        Cmd = CmdCreateAnnotHighlight #00ff00 openedit
        Key = a
    ]
]
```

By default `a` invokes `CmdCreateAnnotHighlight` with default yellow color.

You can over-ride `a` shortcut to create green (`#00ff00`) highlight annotation instead and automatically open annotations edit window (`openedit` boolean argument).

You can create multiple keyboard shortcuts for multiple colors.

Arguments can be: strings, numbers, booleans, colors (`#rrggbb` or `#aarrggbb` format).

Arguments have names. For example `CmdCreateAnnotHighlight` has `color` argument of type color and optional `openedit` boolean argument.

The format of providing arguments is: `CmdCreateAnnotHighlight color: #fafafa openedit: true`.

For boolean arguments name is the same as `true` value i.e. `openedit` is the same as `openedit: true`.

For default arguments you can skip the name. For example: `color` is a default `CmdCreateAnnotHighlight` argument so `CmdCreateAnnotHighlight #fafafa` is the same as `CmdCreateAnnotHighlight color: #fafafa`

You can combine those rules: `CmdCreateAnnotHighlight #fafafa openedit` is the same as `CmdCreateAnnotHighlight color: #fafafa openedit: true`.

## `CmdExtendSelectionCharLeft` and other `CmdExtendSelection*`

**Ver 3.7+**

`CmdExtendSelectionCharLeft`, `CmdExtendSelectionCharRight`, `CmdExtendSelectionWordLeft` and `CmdExtendSelectionWordRight` move the free end of the current text selection by one character or one word, the way `Shift + Left/Right` and `Ctrl + Shift + Left/Right` do in a text editor. Moving toward the selection's anchor shrinks it, moving away grows it, and the selection continues across page boundaries.

They have no default shortcut because the obvious keys are already taken (`Ctrl + Shift + Left/Right` navigate between files), so assign your own:

```
Shortcuts [
    [
        Cmd = CmdExtendSelectionWordRight
        Key = Ctrl + Shift + Right
    ]
    [
        Cmd = CmdExtendSelectionWordLeft
        Key = Ctrl + Shift + Left
    ]
]
```

The selection can come from anywhere: dragging with the mouse, double-clicking a word, `Ctrl + A` or keyboard selection (`F7`). While `F7` keyboard selection is active these commands move the caret, so it stays at the end of the selection. Without a selection they do nothing, and they're not available for documents with no extractable text.

## `CmdScrollUp`, `CmdScrollDown`

**Ver 3.6+**

Arguments:

- `n` : default, integer, how many lines to scroll up or down (default: 1)

Use case: if you want to speed up scrolling with `j`, `k` keys, you can re-assign them:

```
Shortcuts [
    [
        Cmd = CmdScrollDown 5
        Key = j
    ]
    [
        Cmd = CmdScrollUp n: 5
        Key = k
    ]
]
```

## `CmdGoToNextPage`, `CmdGoToPrevPage`

**Ver 3.6+**

Arguments:

- `n` : default, integer, how many pages to advance by (default: 1)

Use case: if you want to go forward, back by more than 1 page

## `CmdCreateAnnotHighlight` and other `CmdCreateAnnot*`

Arguments:

- `color` : default, color
- `openedit` : boolean, `false` if not given
- `copytoclipboard` : boolean, `false` if not given. For highlight/underline/squiggly/strikeout annotations, copies the selection (text of annotation) to clipboard. This used to be default behavior for built-in `a` etc. keyboard shortcuts but now it has to be explicitly chosen.
- `setcontent` : boolean, false if not given. For highlight/underline/squiggly/strikeout sets content of annotation to the selection (text of annotation)

Use cases:

- change default color for annotations
- create multiple shortcuts for different colors

Example: change `a` to create green highlight annotation:

```
Shortcuts [
    [
        Cmd = CmdCreateAnnotHighlight #00ff00 openedit
        Key = a
    ]
]
```

## Other CmdCreateAnnot\* arguments

**Ver 3.6+**

Arguments for `CmdCreateAnnotHighlight` plus:

- `color` : default, color of text and border, black if not given
- `bgcolor` : background color of annotation, fully transparent if not given
- `textsize` : size of annotation text, 12 if not given
- `borderwidth` : border width, 1 if not given
- `alignment` : **ver 3.7+**, how free text is aligned in its box: `left`, `center` or `right`. Left if not given
- `opacity` : opacity of annotation, 0 - fully transparent (i.e. invisible), 100 - fully opaque (default if not given)
- `interiorcolor` : interior color for circle, square etc. annotations, fully transparent if not given
- `focusedit` : boolean, when annotation edit window opens, focus the contents edit control
- `focuslist` : boolean, when annotation edit window opens, focus the annotations list

## `CmdToggleBoolSetting`

**Ver 3.7+**

Arguments:

- `name` : default, string — name of a boolean advanced setting (case-insensitive leaf name or dotted path, e.g. `Fullscreen.ShowMenubar`)

Toggles that setting between `true` and `false`. Useful for custom shortcuts or toolbar buttons. Unknown setting names show a warning when the shortcut is defined and when the command runs.

Example: toggle fullscreen menubar with `t`:

```
Shortcuts [
	[
		Cmd = CmdToggleBoolSetting Fullscreen.ShowMenubar
		Key = t
		Name = Toggle Fullscreen Menubar
	]
]
```

Add `ToolbarText` to the same entry to also get a toolbar button for it. Example: turn the reading mode of [`MouseWheelTurnsPage`](Advanced-options-settings.md) on and off with `w` or from the toolbar:

```
Shortcuts [
	[
		Cmd = CmdToggleBoolSetting MouseWheelTurnsPage
		Key = w
		Name = Wheel Turns Page
		ToolbarText = Wheel Turns Page
	]
]
```

The same works for any other boolean advanced setting, e.g. `CmdToggleBoolSetting RememberViewOffsetOnPageTurn` or `CmdToggleBoolSetting ClickEdgeToTurnPage`.

## `CmdZoomCustom`

**Ver 3.6+**

Arguments:

- `level` : default, string or integer, zoom level

`level` can be:

- a number describing zoom level in percent e.g.:
  - `50` or `50%` means 50% zoom
  - `125` means 125% zoom
- a virtual zoom level:
  - `actual size` (100% zoom level)
  - `fit page`
  - `fit width`
  - `fit content`

Example:

```
Shortcuts [
    [
        Cmd = CmdZoomCustom 50%
        Key = z
    ]
]
```

## `CmdCommandPalette`

**Ver 3.6+**

Argument:

- `mode` : default, optional string, Values:
  - `@` for opened files (tabs)
  - `#` for history of files
  - `>` for commands
  - `%` for table of contents (`CmdCommandPaletteTOC`, `Shift + F12`; `*` still works)
  - `$` for favorites (`CmdCommandPaletteFavorites`)

Without an argument it defaults to `>`.

To bind a keyboard shortcut to a palette mode, prefer the dedicated commands
(`CmdCommandPaletteTOC`, `CmdCommandPaletteFavorites`) over `CmdCommandPalette`
with a `mode` argument. A literal `$` in settings must be written as `$$` (see
[customize keyboard shortcuts](Customize-keyboard-shortcuts.md#escaping-in-settings-values)).

Example:

```
Shortcuts [
    [
        Cmd = CmdCommandPalette #
        Key = Ctrl + h
    ]
    [
        Cmd = CmdCommandPaletteFavorites
        Key = b
    ]
]
```

## Toggle commands (force an on/off state)

**Ver 3.7+**

These toggle commands accept an optional `state` boolean argument that forces an explicit state instead of flipping the current one. This is useful for automation (e.g. via [DDE](DDE-Commands.md)) where you want to set a known state rather than toggle blindly:

- `CmdToggleFullscreen`
- `CmdTogglePresentationMode`
- `CmdToggleToolbar`
- `CmdToggleMenuBar`
- `CmdToggleContinuousView`
- `CmdToggleTableOfContents`, `CmdToggleBookmarks`
- `CmdToggleDisableLinks`

Arguments:

- `state` : default, boolean. `on`/`true`/`1`/`yes` forces the feature on, `off`/`false`/`0`/`no` forces it off. If the document is already in the requested state, nothing happens. If omitted, the command toggles (the original behavior).

Examples (DDE):

```
[CmdToggleFullscreen on]
[CmdToggleToolbar off]
[CmdToggleContinuousView state: on]
```

# Debugging

If a custom shortcut defined in `Shortcuts` doesn't work it could be caused by invalid command name or invalid command arguments.

We log information about unsuccessful parsing of a shortcut so [check the logs](Debugging-Sumatra.md#getting-logs) if things don't work as expected.
