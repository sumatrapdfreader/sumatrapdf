# Commands

You can control SumatraPDF with commands.

You can:

- use [command palette](Command-Palette.md) (`Ctrl + K`) to invoke a command by its description
- you can [customize a keyboard shortcut](Customizing-keyboard-shortcuts.md) to invoke a command by it’s id
- (since 3.5) you can [send a command via DDE](DDE-Commands.md) e.g. `[CmdClose]` to invoke `Close Document` command

:search:

### **File**

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdClose,"Ctrl + W, Ctrl + F4",Close Document,
CmdCloseCurrentDocument,q,Close Current Document,
CmdCloseAllTabs,,Close All Tabs,ver 3.6+
CmdCloseTabsToTheLeft,,Close Tabs To The Left,ver 3.6+
CmdCloseTabsToTheRight,,Close Tabs To The Right,ver 3.6+
CmdCloseOtherTabs,,Close Other Tabs,ver 3.6+
CmdCommandPalette,Ctrl + K,Command Palette,
CmdCommandPaletteNoFiles,Shift + Ctrl + K,Command Palette No Files,
CmdCommandPaletteOnlyTabs,Alt + K,Command Palette Only Tabs,ver 3.5+
CmdDuplicateInNewWindow,Shift + Ctrl + N,Open Current Document In New Window,
CmdExit,Ctrl + Q,Exit Application,
CmdMoveFrameFocus,F6,Move Frame Focus,
CmdNewWindow,Ctrl + N,Open New SumatraPDF Window,
CmdOpenFile,Ctrl + O,Open File...,
CmdOpenFolder,,Open Folder...,
CmdOpenNextFileInFolder,Shift + Ctrl + Right,Open Next File In Folder,
CmdOpenPrevFileInFolder,Shift + Ctrl + Left,Open Previous File In Folder,
CmdOpenSelectedDocument,,Open Selected Document,
CmdPinSelectedDocument,,Pin Selected Document,
CmdPrint,Ctrl + P,Print Document...,
CmdProperties,Ctrl + D,Show Document Properties...,
CmdReloadDocument,r,Reload Document,
CmdRenameFile,F2,Rename File...,
CmdReopenLastClosedFile,Shift + Ctrl + T,Reopen Last Closed,
CmdSaveAs,Ctrl + S,Save File As...,
CmdToggleCursorPosition,m,Toggle Cursor Position,
CmdShowInFolder,,Show File In Folder...,
CmdToggleBookmarks,"Shift + F12, F12",Toggle Bookmarks,
CmdToggleTableOfContents,,Toggle Table Of Contents,ver 3.6+
CmdCollapseAll,,Collapse All,
CmdExpandAll,,Expand All,
CmdOpenEmbeddedPDF,,Open Embedded PDF,
CmdSaveEmbeddedFile,,Save Embedded File...,
CmdCreateShortcutToFile,,Create .lnk Shortcut,
CmdSelectAll,Ctrl + A,Select All,
CmdCopyComment,,Copy Comment,
CmdCopyImage,,Copy Image,
CmdCopyLinkTarget,,Copy Link Target,
CmdCopySelection,"Ctrl + C, Ctrl + Insert",Copy Selection,
CmdCopyFilePath,,Copy File Path,ver 3.5+
CmdDeleteFile,,Delete Currently Opened File, ver 3.6+
```

### **Search**

```commands
Command IDs,Keyboard shortcuts,Command Palette
CmdFindFirst,Ctrl + F,Find
CmdFindMatch,,Find: Match Case
CmdFindNext,F3,Find Next
CmdFindNextSel,Ctrl + F3,Find Next Selection
CmdFindPrev,Shift + F3,Find Previous
CmdFindPrevSel,Shift + Ctrl + F3,Find Previous Selection
```

### **Viewing**

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdBookView,Ctrl + 8,Book View,
CmdFacingView,Ctrl + 7,Facing View,
CmdInvertColors,i,Invert Colors,
CmdRotateLeft,"[, Shift + Ctrl + Subtract",Rotate Left,
CmdRotateRight,"], Shift + Ctrl + Add",Rotate Right,
CmdSinglePageView,Ctrl + 6,Single Page View,
CmdToggleContinuousView,c,Toggle Continuous View,
CmdToggleFullscreen,"f, Shift + Ctrl + L, F11",Toggle Fullscreen,
CmdToggleMangaMode,,Toggle Manga Mode,
CmdToggleMenuBar,F9,Toggle Menu Bar,
CmdTogglePageInfo,Shift + i,Show / Hide Current Page Number,
CmdToggleScrollbars,,Toggle Scrollbars,
CmdToggleToolbar,F8,Toggle Toolbar,
CmdNextTab,Ctrl + Right,Next Tab,
CmdPrevTab,Ctrl + Left,Previous Tab,
CmdToggleFrequentlyRead,,Toggle Frequently Read,ver 3.5+
CmdSelectNextTheme,,Select Next Theme,ver 3.5+
CmdToggleLinks,,Toggle Show Links, Toggle drawing blue rectangle around links
```

### **Navigation**

```commands
Command IDs,Keyboard shortcuts,Command Palette
CmdScrollUp,"k, Up",Scroll Up
CmdScrollDown,"j, Down",Scroll Down
CmdScrollLeft,"h, Left",Scroll Left
CmdScrollRight,"l, Right",Scroll Right
CmdScrollUpHalfPage,Shift + Up,Scroll Up By Half Page
CmdScrollDownHalfPage,Shift + Down,Scroll Down By Half Page
CmdScrollUpPage,"Ctrl + Up, PageUp, Shift + Return, Shift + Space",Scroll Up By Page
CmdScrollDownPage,"Ctrl + Down, PageDown, Return, Space",Scroll Down By Page
CmdScrollLeftPage,Shift + Left,Scroll Left By Page
CmdScrollRightPage,Shift + Right,Scroll Right By Page
CmdGoToFirstPage,"Ctrl + Home, Home",First Page
CmdGoToLastPage,"Ctrl + End, End",Last Page
CmdGoToPrevPage,"p, Ctrl + PageUp",Previous Page
CmdGoToNextPage,"n, Ctrl + PageDown",Next Page
CmdGoToPage,"g, Ctrl + G",Go to Page...
CmdNavigateBack,"Alt + Left, Backspace",Navigate Back
CmdNavigateForward,"Alt + Right, Shift + Backspace",Navigate Forward
```

### **Favorite**

```commands
Command IDs,Keyboard shortcuts,Command Palette
CmdFavoriteAdd,Ctrl + B,Add Favorite
CmdFavoriteDel,,Delete Favorite
CmdFavoriteToggle,,Toggle Favorites
```

### **Presentation**

```commands
Command IDs,Keyboard shortcuts,Command Palette
CmdTogglePresentationMode,"Ctrl + L, Shift + F11, F5",View: Presentation Mode
CmdPresentationBlackBackground,.,Presentation Black Background
CmdPresentationWhiteBackground,w,Presentation White Background
```

### **Annotation**

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
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
CmdCreateAnnotRedact,,Create Redact Annotation,
CmdCreateAnnotSquare,,Create Square Annotation,
CmdCreateAnnotSquiggly,,Create Squiggly Annotation,
CmdCreateAnnotStamp,,Create Stamp Annotation,
CmdCreateAnnotStrikeOut,,Create Strike Out Annotation,
CmdCreateAnnotText,,Create Text Annotation,
CmdCreateAnnotUnderline,"u, U",Create Underline Annotation,
CmdDeleteAnnotation,Delete,Delete Annotation,
CmdEditAnnotations,,Edit Annotations,
CmdSaveAnnotations,Shift + Ctrl + S,Save Annotations to existing PDF,
CmdSaveAnnotationsNewFile,,Save Annotations to new PDF,ver 3.6+
```

### **Zoom menu**

```commands
Command IDs,Keyboard shortcuts,Command Palette
CmdToggleZoom,z,Toggle Zoom
CmdZoomActualSize,Ctrl + 1,Zoom: Actual Size
CmdZoomCustom,Ctrl + Y,Zoom: Custom...
CmdZoomFitContent,Ctrl + 3,Zoom: Fit Content
CmdZoomFitPage,Ctrl + 0,Zoom: Fit Page
CmdZoomFitPageAndSinglePage,,Zoom: Fit Page and Single Page
CmdZoomFitWidth,Ctrl + 2,Zoom: Fit Width
CmdZoomFitWidthAndContinuous,,Zoom: Fit Width And Continuous
CmdZoomIn,Ctrl + Add,Zoom In
CmdZoomOut,Ctrl + Subtract,Zoom Out
CmdZoom100,,Zoom: 100%
CmdZoom12_5,,Zoom: 12.5%
CmdZoom125,,Zoom: 125%
CmdZoom150,,Zoom: 150%
CmdZoom1600,,Zoom: 1600%
CmdZoom200,,Zoom: 200%
CmdZoom25,,Zoom: 25%
CmdZoom3200,,Zoom: 3200%
CmdZoom400,,Zoom: 400%
CmdZoom50,,Zoom: 50%
CmdZoom6400,,Zoom: 6400%
CmdZoom8_33,,Zoom: 8.33%
CmdZoom800,,Zoom: 800%
```

### **External app**

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdOpenWithExplorer,,Open Directory In Explorer,3.5 or later
CmdOpenWithDirectoryOpus,,Open Directory In Directory Opu,3.5 or later
CmdOpenWithTotalCommander,,Open Directory In Total Commander,3.5 or later
CmdOpenWithDoubleCommander,,Open Directory In Double Commander,3.5 or later
CmdOpenWithAcrobat,,Open With Adobe Acrobat,
CmdOpenWithFoxIt,,Open With FoxIt,
CmdOpenWithFoxItPhantom,,Open With FoxIt Phantom,
CmdOpenWithHtmlHelp,,Open With HTML Help,
CmdOpenWithPdfDjvuBookmarker,,Open With Pdf&Djvu Bookmarker,
CmdOpenWithPdfXchange,,Open With PdfXchange,
CmdOpenWithXpsViewer,,Open With Xps Viewer,
CmdTranslateSelectionWithDeepL,,Translate Selection With DeepL,
CmdTranslateSelectionWithGoogle,,Translate Selection with Google,
CmdSearchSelectionWithBing,,Search Selection with Bing,
CmdSearchSelectionWithGoogle,,Search Selection with Google,
CmdSearchSelectionWithWikipedia,,Search Selection with Wikipedia,ver 3.6+
CmdSearchSelectionWithGoogleScholar,,Search Selection with Goolge Scholar,ver 3.6+
CmdSendByEmail,,Send Document By Email...,
CmdInvokeInverseSearch,,Invoke Inverse Search,ver 3.6+

```

### **System**

```commands
Command IDs,Keyboard shortcuts,Command Palette
CmdAdvancedOptions,,Advanced Options (Settings)...
CmdAdvancedSettings,,Advanced Options (Settings)...
CmdChangeLanguage,,Change Language...
CmdCheckUpdate,,Check For Updates
CmdClearHistory,,Clear History
CmdContributeTranslation,,Contribute Translation
CmdForgetSelectedDocument,,Remove Selected Document From History
CmdOptions,,Options...
CmdShowLog,,Show Log
```

### **Help**

```commands
Command IDs,Keyboard shortcuts,Command Palette
CmdHelpOpenManual,,Help: Manual
CmdHelpOpenKeyboardShortcuts,,Help: Keyboard Shortcuts
CmdHelpAbout,,Help: About SumatraPDF
CmdHelpOpenManualOnWebsite,,Help: Manual On Website
CmdHelpVisitWebsite,,Help: SumatraPDF Website
```

### **Debug**

```commands
Command IDs,Keyboard shortcuts,Command Palette
CmdDebugCrashMe,,Debug: Crash Me
CmdDebugDownloadSymbols,,Debug: Download Symbols
CmdDebugShowNotif,,Debug: Show Notification
CmdDebugStartStressTest,,Debug: Start Stress Test
CmdDebugTestApp,,Debug: Test App
CmdNone,,Do nothing
```

### **Deprecated**
```commands
Command IDs,Keyboard shortcuts,Command Palette
CmdOpenWithFirst,,don't use
CmdOpenWithLast,,don't use
```
