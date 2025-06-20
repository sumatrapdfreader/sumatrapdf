# Commands

You can control SumatraPDF with commands:

- use [command palette](Command-Palette.md) (`Ctrl + K`) to invoke a command by its description
- [customize a keyboard shortcut](Customizing-keyboard-shortcuts.md) to invoke a command by its id
- **ver 3.5+:** [send a command via DDE](DDE-Commands.md) e.g. `[CmdClose]` to invoke `Close Document` command
- **ver 3.6+:** some commands [accept arguments](Commands.md#commands-with-arguments)

:search:

## File

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdClose,"Ctrl + W, Ctrl + F4",Close Document,
CmdCloseCurrentDocument,q,Close Current Document,
CmdCommandPalette,Ctrl + K,Command Palette,
CmdDuplicateInNewWindow,Shift + Ctrl + N,Open Current Document In New Window,
CmdExit,Ctrl + Q,Exit Application,
CmdMoveFrameFocus,F6,Move Frame Focus,
CmdNewWindow,Ctrl + N,Open New SumatraPDF Window,
CmdOpenFile,Ctrl + O,Open File...,
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

## Search

```commands
Command IDs,Keyboard shortcuts,Command Palette
CmdFindFirst,Ctrl + F,Find
CmdFindMatch,,Find: Match Case
CmdFindNext,F3,Find Next
CmdFindNextSel,Ctrl + F3,Find Next Selection
CmdFindPrev,Shift + F3,Find Previous
CmdFindPrevSel,Shift + Ctrl + F3,Find Previous Selection
```

## Viewing

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
CmdToggleFrequentlyRead,,Toggle Frequently Read,ver 3.5+
CmdSelectNextTheme,,Select Next Theme,ver 3.5+
CmdToggleLinks,,Toggle Show Links, Toggle drawing blue rectangle around links
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
```

## Navigation

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
CmdGoToPrevPage,"p",Previous Page
CmdGoToNextPage,"n",Next Page
CmdGoToPage,"g, Ctrl + G",Go to Page...
CmdNavigateBack,"Alt + Left, Backspace",Navigate Back
CmdNavigateForward,"Alt + Right, Shift + Backspace",Navigate Forward
```

## Favorites

```commands
Command IDs,Keyboard shortcuts,Command Palette
CmdFavoriteAdd,Ctrl + B,Add Favorite
CmdFavoriteDel,,Delete Favorite
CmdFavoriteToggle,,Toggle Favorites
```

## Presentation

```commands
Command IDs,Keyboard shortcuts,Command Palette
CmdTogglePresentationMode,"Ctrl + L, Shift + F11, F5",View: Presentation Mode
CmdPresentationBlackBackground,.,Presentation Black Background
CmdPresentationWhiteBackground,w,Presentation White Background
```

## Annotations

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

## Zoom

```commands
Command IDs,Keyboard shortcuts,Command Palette,Notes
CmdToggleZoom,z,Toggle Zoom,
CmdZoomActualSize,Ctrl + 1,Zoom: Actual Size,
CmdZoomCustom,Ctrl + Y,Zoom: Custom...,
CmdZoomFitContent,Ctrl + 3,Zoom: Fit Content,
CmdZoomFitPage,Ctrl + 0,Zoom: Fit Page,
CmdZoomFitPageAndSinglePage,,Zoom: Fit Page and Single Page,
CmdZoomFitWidth,Ctrl + 2,Zoom: Fit Width,
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
CmdTranslateSelectionWithDeepL,,Translate Selection With DeepL,
CmdTranslateSelectionWithGoogle,,Translate Selection with Google,
CmdSearchSelectionWithBing,,Search Selection with Bing,
CmdSearchSelectionWithGoogle,,Search Selection with Google,
CmdSearchSelectionWithWikipedia,,Search Selection with Wikipedia,ver 3.6+
CmdSearchSelectionWithGoogleScholar,,Search Selection with Goolge Scholar,ver 3.6+
CmdSendByEmail,,Send Document By Email...,
CmdInvokeInverseSearch,,Invoke Inverse Search,ver 3.6+

```

## System

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
CmdShowLog,,Show Logs
```

## Help

```commands
Command IDs,Keyboard shortcuts,Command Palette
CmdHelpOpenManual,F1,Help: Manual
CmdHelpOpenKeyboardShortcuts,,Help: Keyboard Shortcuts
CmdHelpAbout,,Help: About SumatraPDF
CmdHelpOpenManualOnWebsite,,Help: Manual On Website
CmdHelpVisitWebsite,,Help: SumatraPDF Website
```

## Debug

```commands
Command IDs,Keyboard shortcuts,Command Palette
CmdDebugCrashMe,,Debug: Crash Me
CmdDebugDownloadSymbols,,Debug: Download Symbols
CmdDebugShowNotif,,Debug: Show Notification
CmdDebugStartStressTest,,Debug: Start Stress Test
CmdDebugTestApp,,Debug: Test App
CmdDebugTogglePredictiveRender,,Debug: Toggle Predictive Rendering
CmdDebugToggleRtl,,Debug: Toggle Rtl
CmdNone,,Do nothing
```

## Deprecated or internal

```commands
Command IDs,Keyboard shortcuts,Command Palette
CmdDebugCorruptMemory,,don't use
CmdOpenWithKnownExternalViewerFirst,,don't use
CmdOpenWithKnownExternalViewerLast,,don't use
CmdSelectionHandler,,use SelectionHandlers advanced setting instead
CmdSetTheme,,don't use
CmdViewWithExternalViewer,,don't use
CmdSaveAttachment,,don't use
CmdOpenAttachment,,don't use
```

# Commands with arguments

**Ver 3.6+:** some commands accept arguments which provides more capabilities when creating [custom keyboard shortcut](Customizing-keyboard-shortcuts.md).

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

You can over-ride `a` shortcut to creat green (`#00ff00`) highlight annotation instead and automatically open annotations edit window (`openedit` boolean argument).

You can create multiple keyboard shortcuts for multiple colors.

Arguments can be: strings, numbers, booleans, colors (`#rrggbb` or `#aarrggbb` format).

Arguments have names. For example `CmdCreateAnnotHighlight` has `color` argument of type color and optional `openedit` boolean argument.

The format of providing arguments is: `CmdCreateAnnotHighlight color: #fafafa openedit: true`.

For boolean arguments name is the same as `true` value i.e. `openedit` is the same as `openedit: true`.

For default arguments you can skip the name. For example: `color` is a default `CmdCreateAnnotHighlight` argument so `CmdCreateAnnotHighlight #fafafa` is the same as `CmdCreateAnnotHighlight color: #fafafa`

You can combine those rules: `CmdCreateAnnotHighlight #fafafa openedit` is the same as `CmdCreateAnnotHighlight color: #fafafa openedit: true`.

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
- `copytoclipboard` : boolean, `false` if not given. For highlight/underline/squiggly/strikeout  annotations, copies the selection (text of annotation) to clipboard. This used to be default behavior for built-in `a` etc. keyboard shortcuts but now it has to be explicitly chosen.
- `setcontent` : boolean, false if not give. For highlight/underline/squiggly/strikeout sets content of annotation to the selection (text of annotation)

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

## `CmdZoomCustom`

**Ver 3.6+**

Arguments:
- `level` : default, string or intenger, zoom level

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

Without an argument it defaults to `>`.

Example:

```
Shortcuts [
    [
        Cmd = CmdCommandPalette #
        Key = Ctrl + h
    ]
]
```

# Debugging

If a custom shortcut defined in `Shortcuts` doesn't work it could be caused by invalid command name or invalid command arguments.

We log information about unsuccessful parsing of a shortcut so [check the logs](Debugging-Sumatra.md#getting-logs) if things don't work as expected.
