/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "ShortcutParse.h"
#include "Accelerators.h"
#include "AppSettings.h"
#include "Commands.h"

void MaybeDelayedWarningNotification(Str msg);

// @gen-start cmd-c
// clang-format off
const CommandInfo gCommands[] = {
    {CmdOpenFile, "CmdOpenFile\0", StrL("Open File...")},
    {CmdClose, "CmdClose\0", StrL("Close Document")},
    {CmdCloseCurrentDocument, "CmdCloseCurrentDocument\0", StrL("Close Current Document")},
    {CmdCloseOtherTabs, "CmdCloseOtherTabs\0", StrL("Close Other Tabs")},
    {CmdCloseTabsToTheRight, "CmdCloseTabsToTheRight\0", StrL("Close Tabs To The Right")},
    {CmdCloseTabsToTheLeft, "CmdCloseTabsToTheLeft\0", StrL("Close Tabs To The Left")},
    {CmdCloseAllTabs, "CmdCloseAllTabs\0", StrL("Close All Tabs")},
    {CmdSaveAs, "CmdSaveAs\0", StrL("Save File As...")},
    {CmdPrint, "CmdPrint\0", StrL("Print Document...")},
    {CmdShowInFolder, "CmdShowInFolder\0", StrL("Show File In Folder...")},
    {CmdRenameFile, "CmdRenameFile\0", StrL("Rename File...")},
    {CmdDeleteFile, "CmdDeleteFile\0", StrL("Delete File")},
    {CmdExit, "CmdExit\0", StrL("Exit Application")},
    {CmdReloadDocument, "CmdReloadDocument\0", StrL("Reload Document")},
    {CmdCreateShortcutToFile, "CmdCreateShortcutToFile\0", StrL("Create .lnk Shortcut")},
    {CmdSendByEmail, "CmdSendByEmail\0", StrL("Send Document By Email...")},
    {CmdProperties, "CmdProperties\0", StrL("Document Properties...")},
    {CmdSinglePageView, "CmdSinglePageView\0", StrL("Single Page View")},
    {CmdFacingView, "CmdFacingView\0", StrL("Facing View")},
    {CmdBookView, "CmdBookView\0", StrL("Book View")},
    {CmdToggleContinuousView, "CmdToggleContinuousView\0", StrL("Toggle Continuous View")},
    {CmdToggleMangaMode, "CmdToggleMangaMode\0", StrL("Toggle Manga Mode")},
    {CmdRotateLeft, "CmdRotateLeft\0", StrL("Rotate Left")},
    {CmdRotateRight, "CmdRotateRight\0", StrL("Rotate Right")},
    {CmdToggleBookmarks, "CmdToggleBookmarks\0", StrL("Toggle Bookmarks")},
    {CmdToggleTableOfContents, "CmdToggleTableOfContents\0", StrL("Toggle Table Of Contents")},
    {CmdToggleFullscreen, "CmdToggleFullscreen\0", StrL("Toggle Fullscreen")},
    {CmdPresentationWhiteBackground, "CmdPresentationWhiteBackground\0", StrL("Presentation White Background")},
    {CmdPresentationBlackBackground, "CmdPresentationBlackBackground\0", StrL("Presentation Black Background")},
    {CmdTogglePresentationMode, "CmdTogglePresentationMode\0", StrL("View: Presentation Mode")},
    {CmdToggleToolbar, "CmdToggleToolbar\0", StrL("Toggle Toolbar")},
    {CmdChangeScrollbar, "CmdChangeScrollbar\0", StrL("Change Scrollbar...")},
    {CmdToggleMenuBar, "CmdToggleMenuBar\0", StrL("Toggle Menu Bar")},
    {CmdCopySelection, "CmdCopySelection\0", StrL("Copy Selection")},
    {CmdTranslateSelectionWithGoogle, "CmdTranslateSelectionWithGoogle\0", StrL("Translate Selection with Google")},
    {CmdTranslateSelectionWithDeepL, "CmdTranslateSelectionWithDeepL\0", StrL("Translate Selection with DeepL")},
    {CmdSearchSelectionWithGoogle, "CmdSearchSelectionWithGoogle\0", StrL("Search Selection with Google")},
    {CmdSearchSelectionWithBing, "CmdSearchSelectionWithBing\0", StrL("Search Selection with Bing")},
    {CmdSearchSelectionWithWikipedia, "CmdSearchSelectionWithWikipedia\0", StrL("Search Selection with Wikipedia")},
    {CmdSearchSelectionWithGoogleScholar, "CmdSearchSelectionWithGoogleScholar\0", StrL("Search Selection with Google Scholar")},
    {CmdSelectAll, "CmdSelectAll\0", StrL("Select All")},
    {CmdNewWindow, "CmdNewWindow\0", StrL("New Window")},
    {CmdDuplicateInNewWindow, "CmdDuplicateInNewWindow\0", StrL("Open Current Document In New Window")},
    {CmdDuplicateInNewTab, "CmdDuplicateInNewTab\0", StrL("Open Current Document In New Tab")},
    {CmdCopyImage, "CmdCopyImage\0", StrL("Copy Image")},
    {CmdCopyLinkTarget, "CmdCopyLinkTarget\0", StrL("Copy Link Target")},
    {CmdCopyComment, "CmdCopyComment\0", StrL("Copy Comment")},
    {CmdCopyFilePath, "CmdCopyFilePath\0", StrL("Copy File Path")},
    {CmdScrollUp, "CmdScrollUp\0", StrL("Scroll Up")},
    {CmdScrollDown, "CmdScrollDown\0", StrL("Scroll Down")},
    {CmdScrollLeft, "CmdScrollLeft\0", StrL("Scroll Left")},
    {CmdScrollRight, "CmdScrollRight\0", StrL("Scroll Right")},
    {CmdScrollLeftPage, "CmdScrollLeftPage\0", StrL("Scroll Left By Page")},
    {CmdScrollRightPage, "CmdScrollRightPage\0", StrL("Scroll Right By Page")},
    {CmdScrollUpPage, "CmdScrollUpPage\0", StrL("Scroll Up By Page")},
    {CmdScrollDownPage, "CmdScrollDownPage\0", StrL("Scroll Down By Page")},
    {CmdScrollDownHalfPage, "CmdScrollDownHalfPage\0", StrL("Scroll Down By Half Page")},
    {CmdScrollUpHalfPage, "CmdScrollUpHalfPage\0", StrL("Scroll Up By Half Page")},
    {CmdGoToNextPage, "CmdGoToNextPage\0", StrL("Next Page")},
    {CmdGoToPrevPage, "CmdGoToPrevPage\0", StrL("Previous Page")},
    {CmdGoToFirstPage, "CmdGoToFirstPage\0", StrL("First Page")},
    {CmdGoToLastPage, "CmdGoToLastPage\0", StrL("Last Page")},
    {CmdGoToPage, "CmdGoToPage\0", StrL("Go to Page...")},
    {CmdFindFirst, "CmdFindFirst\0", StrL("Find...")},
    {CmdFindNext, "CmdFindNext\0", StrL("Find Next")},
    {CmdFindPrev, "CmdFindPrev\0", StrL("Find Previous")},
    {CmdFindNextSel, "CmdFindNextSel\0", StrL("Find Next Selection")},
    {CmdFindPrevSel, "CmdFindPrevSel\0", StrL("Find Previous Selection")},
    {CmdFindToggleMatchCase, "CmdFindToggleMatchCase\0", StrL("Find: Toggle Match Case")},
    {CmdSaveAnnotations, "CmdSaveAnnotations\0", StrL("Save Annotations to existing PDF")},
    {CmdSaveAnnotationsNewFile, "CmdSaveAnnotationsNewFile\0", StrL("Save Annotations to a new PDF...")},
    {CmdDiscardChanges, "CmdDiscardChanges\0", StrL("Discard Changes")},
    {CmdDeleteAnnotation, "CmdDeleteAnnotation\0", StrL("Delete Annotation")},
    {CmdZoomFitPage, "CmdZoomFitPage\0", StrL("Zoom: Fit Page")},
    {CmdZoomActualSize, "CmdZoomActualSize\0", StrL("Zoom: Actual Size")},
    {CmdZoomFitWidth, "CmdZoomFitWidth\0", StrL("Zoom: Fit Width")},
    {CmdZoomFitByOrientation, "CmdZoomFitByOrientation\0", StrL("Zoom: Fit Page or Width by Orientation")},
    {CmdZoom6400, "CmdZoom6400\0", StrL("Zoom: 6400%")},
    {CmdZoom3200, "CmdZoom3200\0", StrL("Zoom: 3200%")},
    {CmdZoom1600, "CmdZoom1600\0", StrL("Zoom: 1600%")},
    {CmdZoom800, "CmdZoom800\0", StrL("Zoom: 800%")},
    {CmdZoom400, "CmdZoom400\0", StrL("Zoom: 400%")},
    {CmdZoom200, "CmdZoom200\0", StrL("Zoom: 200%")},
    {CmdZoom150, "CmdZoom150\0", StrL("Zoom: 150%")},
    {CmdZoom125, "CmdZoom125\0", StrL("Zoom: 125%")},
    {CmdZoom100, "CmdZoom100\0", StrL("Zoom: 100%")},
    {CmdZoom50, "CmdZoom50\0", StrL("Zoom: 50%")},
    {CmdZoom25, "CmdZoom25\0", StrL("Zoom: 25%")},
    {CmdZoom12_5, "CmdZoom12_5\0", StrL("Zoom: 12.5%")},
    {CmdZoom8_33, "CmdZoom8_33\0", StrL("Zoom: 8.33%")},
    {CmdZoomFitContent, "CmdZoomFitContent\0", StrL("Zoom: Fit Content")},
    {CmdZoomShrinkToFit, "CmdZoomShrinkToFit\0", StrL("Zoom: Shrink To Fit")},
    {CmdZoomCustom, "CmdZoomCustom\0", StrL("Zoom: Custom...")},
    {CmdZoomIn, "CmdZoomIn\0", StrL("Zoom In")},
    {CmdZoomOut, "CmdZoomOut\0", StrL("Zoom Out")},
    {CmdZoomFitWidthAndContinuous, "CmdZoomFitWidthAndContinuous\0", StrL("Zoom: Fit Width And Continuous")},
    {CmdZoomFitPageAndSinglePage, "CmdZoomFitPageAndSinglePage\0", StrL("Zoom: Fit Page and Single Page")},
    {CmdContributeTranslation, "CmdContributeTranslation\0", StrL("Contribute Translation")},
    {CmdOpenWithExplorer, "CmdOpenWithExplorer\0", StrL("Open Directory In Explorer")},
    {CmdOpenWithDirectoryOpus, "CmdOpenWithDirectoryOpus\0", StrL("Open Directory In Directory Opus")},
    {CmdOpenWithTotalCommander, "CmdOpenWithTotalCommander\0", StrL("Open Directory In Total Commander")},
    {CmdOpenWithDoubleCommander, "CmdOpenWithDoubleCommander\0", StrL("Open Directory In Double Commander")},
    {CmdOpenWithAcrobat, "CmdOpenWithAcrobat\0", StrL("Open in Adobe Acrobat")},
    {CmdOpenWithFoxit, "CmdOpenWithFoxit\0", StrL("Open in Foxit Reader")},
    {CmdOpenWithFoxitPhantom, "CmdOpenWithFoxitPhantom\0", StrL("Open in Foxit PhantomPDF")},
    {CmdOpenWithPdfXchange, "CmdOpenWithPdfXchange\0", StrL("Open in PDF-XChange")},
    {CmdOpenWithXpsViewer, "CmdOpenWithXpsViewer\0", StrL("Open in Microsoft XPS Viewer")},
    {CmdOpenWithHtmlHelp, "CmdOpenWithHtmlHelp\0", StrL("Open in Microsoft HTML Help")},
    {CmdOpenWithPdfDjvuBookmarker, "CmdOpenWithPdfDjvuBookmarker\0", StrL("Open With Pdf&Djvu Bookmarker")},
    {CmdOpenSelectedDocument, "CmdOpenSelectedDocument\0", StrL("Open Selected Document")},
    {CmdPinSelectedDocument, "CmdPinSelectedDocument\0", StrL("Pin Selected Document")},
    {CmdForgetSelectedDocument, "CmdForgetSelectedDocument\0", StrL("Remove Selected Document From History")},
    {CmdExpandAll, "CmdExpandAll\0", StrL("Expand All")},
    {CmdCollapseAll, "CmdCollapseAll\0", StrL("Collapse All")},
    {CmdSaveEmbeddedFile, "CmdSaveEmbeddedFile\0", StrL("Save Embedded File...")},
    {CmdOpenEmbeddedPDF, "CmdOpenEmbeddedPDF\0", StrL("Open Embedded PDF")},
    {CmdSaveAttachment, "CmdSaveAttachment\0", StrL("Save Attachment...")},
    {CmdOpenAttachment, "CmdOpenAttachment\0", StrL("Open Attachment")},
    {CmdOptions, "CmdOptions\0", StrL("Settings...")},
    {CmdAdvancedSettings, "CmdAdvancedSettings\0", StrL("Advanced Settings...")},
    {CmdChangeLanguage, "CmdChangeLanguage\0", StrL("Change Language...")},
    {CmdCheckUpdate, "CmdCheckUpdate\0", StrL("Check For Updates")},
    {CmdInstallPrereleaseUpdate, "CmdInstallPrereleaseUpdate\0", StrL("Install Pre-release Update")},
    {CmdTogglePdfPreviewLogging, "CmdTogglePdfPreviewLogging\0", StrL("Toggle PDF Preview Logging")},
    {CmdHelpOpenManual, "CmdHelpOpenManual\0", StrL("Help: Manual")},
    {CmdHelpOpenManualOnWebsite, "CmdHelpOpenManualOnWebsite\0", StrL("Help: Manual On Website")},
    {CmdHelpOpenKeyboardShortcuts, "CmdHelpOpenKeyboardShortcuts\0", StrL("Help: Keyboard Shortcuts")},
    {CmdToggleKeyboardHelp, "CmdToggleKeyboardHelp\0", StrL("Show Keyboard Shortcuts")},
    {CmdHelpVisitWebsite, "CmdHelpVisitWebsite\0", StrL("Help: SumatraPDF Website")},
    {CmdHelpAbout, "CmdHelpAbout\0", StrL("Help: About SumatraPDF...")},
    {CmdMoveFrameFocus, "CmdMoveFrameFocus\0", StrL("Move Frame Focus")},
    {CmdFavoriteAdd, "CmdFavoriteAdd\0", StrL("Add Favorite")},
    {CmdFavoriteDel, "CmdFavoriteDel\0", StrL("Delete Favorite")},
    {CmdFavoriteToggle, "CmdFavoriteToggle\0", StrL("Toggle Favorites")},
    {CmdToggleLinks, "CmdToggleLinks\0", StrL("Toggle Show Links")},
    {CmdToggleShowAnnotations, "CmdToggleShowAnnotations\0", StrL("Toggle Show Annotations")},
    {CmdShowAnnotations, "CmdShowAnnotations\0", StrL("Show Annotations")},
    {CmdHideAnnotations, "CmdHideAnnotations\0", StrL("Hide Annotations")},
    {CmdCreateAnnotText, "CmdCreateAnnotText\0", StrL("Create Text Annotation")},
    {CmdCreateAnnotLink, "CmdCreateAnnotLink\0", StrL("Create Link Annotation")},
    {CmdCreateAnnotFreeText, "CmdCreateAnnotFreeText\0", StrL("Create Free Text Annotation")},
    {CmdCreateAnnotLine, "CmdCreateAnnotLine\0", StrL("Create Line Annotation")},
    {CmdCreateAnnotSquare, "CmdCreateAnnotSquare\0", StrL("Create Square Annotation")},
    {CmdCreateAnnotCircle, "CmdCreateAnnotCircle\0", StrL("Create Circle Annotation")},
    {CmdCreateAnnotPolygon, "CmdCreateAnnotPolygon\0", StrL("Create Polygon Annotation")},
    {CmdCreateAnnotPolyLine, "CmdCreateAnnotPolyLine\0", StrL("Create Polyline Annotation")},
    {CmdCreateAnnotHighlight, "CmdCreateAnnotHighlight\0", StrL("Create Highlight Annotation")},
    {CmdCreateAnnotUnderline, "CmdCreateAnnotUnderline\0", StrL("Create Underline Annotation")},
    {CmdCreateAnnotSquiggly, "CmdCreateAnnotSquiggly\0", StrL("Create Squiggly Annotation")},
    {CmdCreateAnnotStrikeOut, "CmdCreateAnnotStrikeOut\0", StrL("Create Strike Out Annotation")},
    {CmdCreateAnnotRedact, "CmdCreateAnnotRedact\0", StrL("Create Redact Annotation")},
    {CmdCreateAnnotStamp, "CmdCreateAnnotStamp\0", StrL("Create Stamp Annotation")},
    {CmdCreateAnnotCaret, "CmdCreateAnnotCaret\0", StrL("Create Caret Annotation")},
    {CmdCreateAnnotInk, "CmdCreateAnnotInk\0", StrL("Create Ink Annotation")},
    {CmdCreateAnnotPopup, "CmdCreateAnnotPopup\0", StrL("Create Popup Annotation")},
    {CmdCreateAnnotFileAttachment, "CmdCreateAnnotFileAttachment\0", StrL("Create File Attachment Annotation")},
    {CmdInvertColors, "CmdInvertColors\0", StrL("Invert Colors")},
    {CmdTogglePageInfo, "CmdTogglePageInfo\0", StrL("Toggle Page Info")},
    {CmdToggleZoom, "CmdToggleZoom\0", StrL("Toggle Zoom")},
    {CmdNavigateBack, "CmdNavigateBack\0", StrL("Navigate Back")},
    {CmdNavigateForward, "CmdNavigateForward\0", StrL("Navigate Forward")},
    {CmdToggleCursorPosition, "CmdToggleCursorPosition\0", StrL("Toggle Cursor Position")},
    {CmdOpenNextFileInFolder, "CmdOpenNextFileInFolder\0", StrL("Open Next File In Folder")},
    {CmdOpenPrevFileInFolder, "CmdOpenPrevFileInFolder\0", StrL("Open Previous File In Folder")},
    {CmdCommandPalette, "CmdCommandPalette\0", StrL("Command Palette")},
    {CmdShowLog, "CmdShowLog\0", StrL("Show Logs")},
    {CmdShowErrors, "CmdShowErrors\0", StrL("Show Errors...")},
    {CmdClearHistory, "CmdClearHistory\0", StrL("Clear History")},
    {CmdReopenLastClosedFile, "CmdReopenLastClosedFile\0", StrL("Reopen Last Closed")},
    {CmdNextTab, "CmdNextTab\0", StrL("Next Tab")},
    {CmdPrevTab, "CmdPrevTab\0", StrL("Previous Tab")},
    {CmdNextTabSmart, "CmdNextTabSmart\0", StrL("Smart Next Tab")},
    {CmdPrevTabSmart, "CmdPrevTabSmart\0", StrL("Smart Previous Tab")},
    {CmdMoveTabLeft, "CmdMoveTabLeft\0", StrL("Move Tab Left")},
    {CmdMoveTabRight, "CmdMoveTabRight\0", StrL("Move Tab Right")},
    {CmdInvokeInverseSearch, "CmdInvokeInverseSearch\0", StrL("Invoke Inverse Search")},
    {CmdExec, "CmdExec\0", StrL("Execute a program")},
    {CmdViewWithExternalViewer, "CmdViewWithExternalViewer\0", StrL("View With Custom External Viewer")},
    {CmdSelectionHandler, "CmdSelectionHandler\0", StrL("Launch a browser or run command with selection")},
    {CmdSetTheme, "CmdSetTheme\0", StrL("Set theme")},
    {CmdToggleInverseSearch, "CmdToggleInverseSearch\0", StrL("Toggle Inverse Search")},
    {CmdDebugCorruptMemory, "CmdDebugCorruptMemory\0", StrL("Debug: Corrupt Memory")},
    {CmdDebugCrashMe, "CmdDebugCrashMe\0", StrL("Debug: Crash Me")},
    {CmdDebugShowNotif, "CmdDebugShowNotif\0", StrL("Debug: Show Notification")},
    {CmdDebugStartStressTest, "CmdDebugStartStressTest\0", StrL("Debug: Start Stress Test")},
    {CmdDebugTogglePredictiveRender, "CmdDebugTogglePredictiveRender\0", StrL("Debug: Toggle Predictive Rendering")},
    {CmdDebugToggleRtl, "CmdDebugToggleRtl\0", StrL("Debug: Toggle RTL")},
    {CmdListPrinters, "CmdListPrinters\0", StrL("List Printers...")},
    {CmdToggleWindowsPreviewer, "CmdToggleWindowsPreviewer\0", StrL("Toggle Windows Previewer")},
    {CmdToggleWindowsSearchFilter, "CmdToggleWindowsSearchFilter\0", StrL("Toggle Windows Search Filter")},
    {CmdScreenshot, "CmdScreenshot\0", StrL("Take Screenshot...")},
    {CmdCropImage, "CmdCropImage\0", StrL("Crop Image...")},
    {CmdResizeImage, "CmdResizeImage\0", StrL("Resize Image...")},
    {CmdSaveImage, "CmdSaveImage\0", StrL("Save Image...")},
    {CmdPasteClipboardImage, "CmdPasteClipboardImage\0", StrL("Paste Image From Clipboard")},
    {CmdTabGroupSave, "CmdTabGroupSave\0", StrL("Save Tab Group...")},
    {CmdTabGroupRestore, "CmdTabGroupRestore\0", StrL("Restore Tab Group...")},
    {CmdChangeBackgroundColor, "CmdChangeBackgroundColor\0", StrL("Change Background Color...")},
    {CmdChangeEbookSettings, "CmdChangeEbookSettings\0", StrL("Change eBook Settings...")},
    {CmdSetTabColor, "CmdSetTabColor\0", StrL("Change Tab Color...")},
    {CmdPdfCompress, "CmdPdfCompress\0", StrL("Compress PDF...")},
    {CmdPdfDecompress, "CmdPdfDecompress\0", StrL("Decompress PDF...")},
    {CmdPdfDeletePages, "CmdPdfDeletePages\0", StrL("Delete Pages From PDF...")},
    {CmdPdfExtractPages, "CmdPdfExtractPages\0", StrL("Extract Pages From PDF...")},
    {CmdPdfEncrypt, "CmdPdfEncrypt\0", StrL("Encrypt PDF...")},
    {CmdPdfDecrypt, "CmdPdfDecrypt\0", StrL("Decrypt PDF...")},
    {CmdPdfBake, "CmdPdfBake\0", StrL("Bake PDF File...")},
    {CmdPdfShowInfo, "CmdPdfShowInfo\0", StrL("Show PDF Info...")},
    {CmdDocumentExtractText, "CmdDocumentExtractText\0", StrL("Extract Text From Document...")},
    {CmdDocumentShowOutline, "CmdDocumentShowOutline\0", StrL("Show Document Bookmarks...")},
    {CmdSetScreenshotHotkey, "CmdSetScreenshotHotkey\0", StrL("Set Screenshot Hotkey...")},
    {CmdToggleReadAloud, "CmdToggleReadAloud\0", StrL("Toggle Read Aloud")},
    {CmdPauseReadAloud, "CmdPauseReadAloud\0", StrL("Pause Reading")},
    {CmdContinueReadAloud, "CmdContinueReadAloud\0", StrL("Continue Reading")},
    {CmdStopReadAloud, "CmdStopReadAloud\0", StrL("Stop Reading")},
    {CmdReadAloudFromTopPage, "CmdReadAloudFromTopPage\0", StrL("Start Reading From Top")},
    {CmdReadAloudSelection, "CmdReadAloudSelection\0", StrL("Start Reading Selection")},
    {CmdToggleToolbarShowReadAloud, "CmdToggleToolbarShowReadAloud\0", StrL("Read Aloud: Show In Toolbar")},
    {CmdRemoveDeletedFilesFromHistory, "CmdRemoveDeletedFilesFromHistory\0", StrL("Remove Deleted Files From History")},
    {CmdCommandPaletteTOC, "CmdCommandPaletteTOC\0", StrL("Command Palette: Table Of Contents")},
    {CmdDebugToggleRenderInfo, "CmdDebugToggleRenderInfo\0", StrL("Debug: Toggle Render Queue Info")},
    {CmdConvertImageToPdf, "CmdConvertImageToPdf\0", StrL("Convert Image To PDF...")},
    {CmdExpandToCurrentPage, "CmdExpandToCurrentPage\0", StrL("Expand TOC to Current Page")},
    {CmdStartAutoScroll, "CmdStartAutoScroll\0", StrL("Start Auto-Scroll")},
    {CmdAIChatWithClaudeCode, "CmdAIChatWithClaudeCode\0", StrL("Claude chat...")},
    {CmdAIChatWithGrokBuild, "CmdAIChatWithGrokBuild\0", StrL("Grok chat...")},
    {CmdAIChatWithOpenAICodex, "CmdAIChatWithOpenAICodex\0", StrL("Codex chat...")},
    {CmdTranslateSelectionWithGrokBuild, "CmdTranslateSelectionWithGrokBuild\0", StrL("Translate Selection with Grok Build...")},
    {CmdTranslateSelectionWithClaudeCode, "CmdTranslateSelectionWithClaudeCode\0", StrL("Translate Selection with Claude Code...")},
    {CmdTranslateSelectionWithOpenAICodex, "CmdTranslateSelectionWithOpenAICodex\0", StrL("Translate Selection with OpenAI Codex...")},
    {CmdFindToggleMatchWholeWord, "CmdFindToggleMatchWholeWord\0", StrL("Find: Toggle Match Whole Word")},
    {CmdGoToNextFavorite, "CmdGoToNextFavorite\0", StrL("Go to Next Favorite")},
    {CmdGoToPrevFavorite, "CmdGoToPrevFavorite\0", StrL("Go to Previous Favorite")},
    {CmdCreateAnnotImageFromClipboard, "CmdCreateAnnotImageFromClipboard\0", StrL("Create Image Annotation From Clipboard")},
    {CmdSetInverseSearch, "CmdSetInverseSearch\0", StrL("Set Inverse Search Command Line...")},
    {CmdCommandPaletteFavorites, "CmdCommandPaletteFavorites\0", StrL("Command Palette: Favorites")},
    {CmdNavigateFilesInFolder, "CmdNavigateFilesInFolder\0", StrL("Navigate Files in Folder...")},
    {CmdDebugToggleCacheInfo, "CmdDebugToggleCacheInfo\0", StrL("Debug: Toggle Cache Info")},
    {CmdToggleEngineeringDrawingEnhance, "CmdToggleEngineeringDrawingEnhance\0", StrL("Toggle Engineering Drawing Enhancement")},
    {CmdSetDocumentColorsFollowTheme, "CmdSetDocumentColorsFollowTheme\0", StrL("Make Document Colors Follow Theme...")},
    {CmdTogglePreservePdfImages, "CmdTogglePreservePdfImages\0", StrL("Toggle Preserve PDF Image Colors in Dark Mode")},
    {CmdToggleLightDarkTheme, "CmdToggleLightDarkTheme\0", StrL("Toggle Light/Dark Theme")},
    {CmdChangeTheme, "CmdChangeTheme\0", StrL("Change Theme...")},
    {CmdTranslateSelection, "CmdTranslateSelection\0", StrL("Translate Selection...")},
    {CmdFavoriteShowInTab, "CmdFavoriteShowInTab\0", StrL("Show Favorites in Tab")},
    {CmdTocExpandToLevel1, "CmdTocExpandToLevel1\0", StrL("Bookmarks: Expand to Level 1")},
    {CmdTocExpandToLevel2, "CmdTocExpandToLevel2\0", StrL("Bookmarks: Expand to Level 2")},
    {CmdTocExpandToLevel3, "CmdTocExpandToLevel3\0", StrL("Bookmarks: Expand to Level 3")},
    {CmdTocCollapseSameLevel, "CmdTocCollapseSameLevel\0", StrL("Bookmarks: Collapse Same Level")},
    {CmdToggleFavoritesSort, "CmdToggleFavoritesSort\0", StrL("Sort Favorites By Name")},
    {CmdZoomFitHeight, "CmdZoomFitHeight\0", StrL("Zoom: Fit Height")},
    {CmdDeleteFileAndOpenNext, "CmdDeleteFileAndOpenNext\0", StrL("Delete File And Open Next")},
    {CmdShowGeneratedHTML, "CmdShowGeneratedHTML\0", StrL("Show Generated HTML")},
    {CmdDeleteCachedFiles, "CmdDeleteCachedFiles\0", StrL("Delete Cached Files")},
    {CmdToggleKeyboardLinkFollowing, "CmdToggleKeyboardLinkFollowing\0", StrL("Follow Link With Keyboard")},
    {CmdDebugToggleDpiOverride, "CmdDebugToggleDpiOverride\0", StrL("Debug: Toggle DPI Override")},
    {CmdToggleImages, "CmdToggleImages\0", StrL("Toggle Show Images")},
    {CmdSelectTextViaKeyboard, "CmdSelectTextViaKeyboard\0", StrL("Select Text With Keyboard")},
    {CmdOpenFileWithOSFilePicker, "CmdOpenFileWithOSFilePicker\0", StrL("Open File With Windows File Picker...")},
    {CmdToggleFilePicker, "CmdToggleFilePicker\0", StrL("SumatraPDF File Picker")},
    {CmdToggleBoolSetting, "CmdToggleBoolSetting\0", StrL("Toggle Boolean Setting")},
    {CmdFixDefaultApp, "CmdFixDefaultApp\0", StrL("Fix Default App For Extension")},
    {CmdAIChatWithAntiGravity, "CmdAIChatWithAntiGravity\0", StrL("Antigravity chat...")},
    {CmdTranslateSelectionWithAntiGravity, "CmdTranslateSelectionWithAntiGravity\0", StrL("Translate Selection with Antigravity...")},
    {CmdConvertToPDF, "CmdConvertToPDF\0", StrL("Convert To PDF...")},
    {CmdDebugShowFitContentArea, "CmdDebugShowFitContentArea\0", StrL("Debug: Show Fit Content Area")},
    {CmdExtendSelectionCharLeft, "CmdExtendSelectionCharLeft\0", StrL("Extend Selection One Character Left")},
    {CmdExtendSelectionCharRight, "CmdExtendSelectionCharRight\0", StrL("Extend Selection One Character Right")},
    {CmdExtendSelectionWordLeft, "CmdExtendSelectionWordLeft\0", StrL("Extend Selection One Word Left")},
    {CmdExtendSelectionWordRight, "CmdExtendSelectionWordRight\0", StrL("Extend Selection One Word Right")},
    {CmdToggleLaserPointer, "CmdToggleLaserPointer\0", StrL("Toggle Laser Pointer")},
    {CmdZoomToSelection, "CmdZoomToSelection\0", StrL("Zoom: To Selection")},
    {CmdToggleHoverPreview, "CmdToggleHoverPreview\0", StrL("Toggle Citation Hover Preview")},
    {CmdToggleDisableLinks, "CmdToggleDisableLinks\0", StrL("Toggle Disable Links")},
    {CmdSignDocument, "CmdSignDocument\0", StrL("Sign Document...")},
    {CmdInsertImage, "CmdInsertImage\0", StrL("Insert Image...")},
    {CmdToggleHighlightFormFields, "CmdToggleHighlightFormFields\0", StrL("Toggle Highlight Form Fields")},
    {CmdTogglePageBoxes, "CmdTogglePageBoxes\0", StrL("Toggle Page Boxes")},
    {CmdConvertPdfToImages, "CmdConvertPdfToImages\0", StrL("Convert PDF to Images...")},
    {CmdToggleUniformPageWidth, "CmdToggleUniformPageWidth\0", StrL("Toggle Uniform Page Width")},
    {CmdToggleTransparencyGrid, "CmdToggleTransparencyGrid\0", StrL("Toggle Transparency Grid")},
    {CmdTogglePageGrid, "CmdTogglePageGrid\0", StrL("Toggle Page Grid")},
    {CmdConfigurePageGrid, "CmdConfigurePageGrid\0", StrL("Configure Page Grid...")},
    {CmdToggleEditPDF, "CmdToggleEditPDF\0", StrL("Toggle Edit PDF")},
    {CmdApplyRedactions, "CmdApplyRedactions\0", StrL("Apply Redactions")},
    {CmdUndo, "CmdUndo\0", StrL("Undo")},
    {CmdRedo, "CmdRedo\0", StrL("Redo")},
    {CmdCutAnnotation, "CmdCutAnnotation\0", StrL("Cut Annotation")},
    {CmdCopyAnnotation, "CmdCopyAnnotation\0", StrL("Copy Annotation")},
    {CmdPasteAnnotation, "CmdPasteAnnotation\0", StrL("Paste Annotation")},
    {CmdSearchGoogleLens, "CmdSearchGoogleLens\0", StrL("Search with Google Lens")},
    {CmdNavigateThumbnail, "CmdNavigateThumbnail\0", StrL("Navigate Thumbnails")},
    {CmdShowAnnotationText, "CmdShowAnnotationText\0", StrL("Show Comment")},
    {CmdAnnotationHighlightBrush, "CmdAnnotationHighlightBrush\0", StrL("Highlighter")},
    {CmdFindAnnotation, "CmdFindAnnotation\0", StrL("Find Annotation")},
    {CmdOpenFileNoHistory, "CmdOpenFileNoHistory\0", StrL("Open File Without History...")},
    {CmdCopySelectionAsImage, "CmdCopySelectionAsImage\0", StrL("Copy Selection As Image")},
    {CmdSearchGoogleLensPage, "CmdSearchGoogleLensPage\0", StrL("Search Page with Google Lens")},
    {CmdSearchGoogleLensImage, "CmdSearchGoogleLensImage\0", StrL("Search Image with Google Lens")},
    {CmdSaveSelectionAsImage, "CmdSaveSelectionAsImage\0", StrL("Save As Image...")},
    {CmdToggleTrimEmptyMargins, "CmdToggleTrimEmptyMargins\0", StrL("Toggle Trim Empty Margins")},
    {CmdCopyLocationToClipboard, "CmdCopyLocationToClipboard\0", StrL("Copy Location To Clipboard")},
    {CmdToggleAutomaticallyScroll, "CmdToggleAutomaticallyScroll\0", StrL("Automatically Scroll")},
    {CmdAutomaticallyScrollFaster, "CmdAutomaticallyScrollFaster\0", StrL("Automatically Scroll Faster")},
    {CmdAutomaticallyScrollSlower, "CmdAutomaticallyScrollSlower\0", StrL("Automatically Scroll Slower")},
    {CmdToggleReadingBar, "CmdToggleReadingBar\0", StrL("Reading Bar")},
    {CmdToggleReadingBarInvert, "CmdToggleReadingBarInvert\0", StrL("Reading Bar Invert")},
    {CmdGoToHomePage, "CmdGoToHomePage\0", StrL("Go To Home Page")},
    {CmdToggleFreePan, "CmdToggleFreePan\0", StrL("Toggle Free Pan")},
    {CmdNone, "CmdNone\0", StrL("Do nothing")},
    {CmdFileHistory, "CmdFileHistory\0", StrL("Open Recent File")},
    {CmdFavorite, "CmdFavorite\0", StrL("Go to Favorite")},
    {CmdReadAloudFromCursorPosition, "CmdReadAloudFromCursorPosition\0", StrL("Start Reading From Cursor Position")},
    {CmdToggleGrayscale, "CmdToggleGrayscale\0", StrL("Toggle Grayscale")},
    {CmdPrintSelection, "CmdPrintSelection\0", StrL("Print Selection...")},
    {CmdAutoGenerateTOC, "CmdAutoGenerateTOC\0", StrL("Generate Table Of Contents")},
    {CmdOpenSettingsFile, "CmdOpenSettingsFile\0", StrL("Open Advanced Settings File...")},
    {CmdOpenFileWithSumatraFilePicker, "CmdOpenFileWithSumatraFilePicker\0", StrL("Open File With SumatraPDF File Picker...")},
    {CmdSelectCurrentPage, "CmdSelectCurrentPage\0", StrL("Select Current Page")},
    {CmdZoomFitVisible, "CmdZoomFitVisible\0", StrL("Zoom: Fit Visible")},
    {CmdSignWithImage, "CmdSignWithImage\0", StrL("Sign With Image")},
    {CmdInsertTextSnippet, "CmdInsertTextSnippet\0", StrL("Insert Text Snippet")},
    {CmdToggleThumbnails, "CmdToggleThumbnails\0", StrL("Toggle Thumbnails")},
    {CmdMergePDF, "CmdMergePDF\0", StrL("Merge PDF...")},
};
const int gCommandsCount = dimofi(gCommands);

const CommandInfo gCommandAltDescs[] = {
    {CmdNavigateFilesInFolder, "CmdNavigateFilesInFolder\0", StrL("Browse Files In Folder...")},
    {CmdAdvancedSettings, "CmdAdvancedSettings\0", StrL("Advanced Options...")},
};
const int gCommandAltDescsCount = dimofi(gCommandAltDescs);
// clang-format on
// @gen-end cmd-c

struct ArgSpec {
    int cmdId;
    Str name;
    CommandArg::Type type;
};

// arguments for the same command should follow each other
// first argument is default and can be specified without a name
static const ArgSpec argSpecs[] = {
    {CmdSelectionHandler, kCmdArgURL, CommandArg::Type::String}, // default
    {CmdSelectionHandler, kCmdArgExe, CommandArg::Type::String},
    {CmdSelectionHandler, kCmdArgMethod, CommandArg::Type::String},
    {CmdSelectionHandler, kCmdArgBody, CommandArg::Type::String},
    {CmdSelectionHandler, kCmdArgContentType, CommandArg::Type::String},
    {CmdSelectionHandler, kCmdArgHeaders, CommandArg::Type::String},
    {CmdSelectionHandler, kCmdArgSelectToolbar, CommandArg::Type::String},
    {CmdSelectionHandler, kCmdArgToolbarText, CommandArg::Type::String},
    {CmdSelectionHandler, kCmdArgToolbarSvgIcon, CommandArg::Type::String},

    {CmdExec, kCmdArgExe, CommandArg::Type::String}, // default
    {CmdExec, kCmdArgFilter, CommandArg::Type::String},

    // and all CmdCreateAnnot* commands
    {CmdCreateAnnotText, kCmdArgColor, CommandArg::Type::Color}, // default
    {CmdCreateAnnotText, kCmdArgBgColor, CommandArg::Type::Color},
    {CmdCreateAnnotText, kCmdArgOpacity, CommandArg::Type::Int},
    {CmdCreateAnnotText, kCmdArgOpenEdit, CommandArg::Type::Bool},
    {CmdCreateAnnotText, kCmdArgCopyToClipboard, CommandArg::Type::Bool},
    {CmdCreateAnnotText, kCmdArgSetContent, CommandArg::Type::Bool},
    {CmdCreateAnnotText, kCmdArgTextSize, CommandArg::Type::Int},
    {CmdCreateAnnotText, kCmdArgBorderWidth, CommandArg::Type::Int},
    {CmdCreateAnnotText, kCmdArgAlignment, CommandArg::Type::String},
    {CmdCreateAnnotText, kCmdArgInteriorColor, CommandArg::Type::Color},
    {CmdCreateAnnotText, kCmdArgFocusEdit, CommandArg::Type::Bool},
    {CmdCreateAnnotText, kCmdArgFocusList, CommandArg::Type::Bool},

    // and  CmdScrollDown, CmdGoToNextPage, CmdGoToPrevPage
    {CmdScrollUp, kCmdArgN, CommandArg::Type::Int}, // default

    {CmdSetTheme, kCmdArgTheme, CommandArg::Type::String}, // default

    {CmdZoomCustom, kCmdArgLevel, CommandArg::Type::String}, // default

    {CmdCommandPalette, kCmdArgMode, CommandArg::Type::String}, // default

    // toggle commands accept an optional bool to force a state (issue #5067),
    // e.g. [CmdToggleFullscreen on] / [CmdToggleToolbar state=off]
    {CmdToggleContinuousView, kCmdArgState, CommandArg::Type::Bool},   // default
    {CmdToggleToolbar, kCmdArgState, CommandArg::Type::Bool},          // default
    {CmdToggleMenuBar, kCmdArgState, CommandArg::Type::Bool},          // default
    {CmdToggleFullscreen, kCmdArgState, CommandArg::Type::Bool},       // default
    {CmdTogglePresentationMode, kCmdArgState, CommandArg::Type::Bool}, // default
    {CmdToggleBookmarks, kCmdArgState, CommandArg::Type::Bool},        // default
    {CmdToggleTableOfContents, kCmdArgState, CommandArg::Type::Bool},  // default
#ifndef NO_THUMBNAIL_STATE_ARG
    {CmdToggleThumbnails, kCmdArgState, CommandArg::Type::Bool}, // default
#endif

    // default string is the setting name, e.g. [CmdToggleBoolSetting Fullscreen.ShowMenubar]
    {CmdToggleBoolSetting, kCmdArgName, CommandArg::Type::String}, // default

    // extension including leading dot, e.g. [CmdFixDefaultApp .pdf]
    {CmdFixDefaultApp, kCmdArgExt, CommandArg::Type::String}, // default

    // a recent file in the File menu, e.g. [CmdFileHistory C:\dir\file.pdf]
    {CmdFileHistory, kCmdArgFilePath, CommandArg::Type::String}, // default

    // a favorite in the Favorites menu, e.g. [CmdFavorite C:\dir\file.pdf page=3]
    {CmdFavorite, kCmdArgFilePath, CommandArg::Type::String}, // default
    {CmdFavorite, kCmdArgPage, CommandArg::Type::String},

    {CmdNone, StrL(""), CommandArg::Type::None}, // sentinel
};

CustomCommand* gFirstCustomCommand = nullptr;

// cmdName is "CmdOpenFile" etc.
// returns -1 if not found
int GetCommandIdByName(Str cmdName) {
    for (const CommandInfo& cmd : gCommands) {
        if (SeqStrIndexIS(cmd.name, cmdName) == 0) {
            return cmd.id;
        }
    }
    // backwards compatibility for old names
    if (str::EqI(cmdName, StrL("CmdFindMatch"))) {
        return CmdFindToggleMatchCase;
    }
    if (str::EqI(cmdName, StrL("CmdTogglePdfAnnotationsToolbar"))) {
        return CmdToggleEditPDF;
    }
    if (str::EqI(cmdName, StrL("CmdReadAloud"))) {
        return CmdToggleReadAloud;
    }
    if (str::EqI(cmdName, StrL("CmdAdvancedOptions"))) {
        return CmdAdvancedSettings;
    }
    return -1;
}

static int FindCommandIndex(int commandId) {
    for (int i = 0; i < gCommandsCount; i++) {
        if (gCommands[i].id == commandId) {
            return i;
        }
    }
    return -1;
}

Str GetCommandName(int commandId) {
    int idx = FindCommandIndex(commandId);
    return idx < 0 ? Str{} : Str(gCommands[idx].name);
}

Str GetCommandDescription(int commandId) {
    int idx = FindCommandIndex(commandId);
    return idx < 0 ? Str{} : gCommands[idx].description;
}

// Pack the struct and its owned, NUL-terminated strings into one allocation.
template <typename T, size_t N>
static T* AllocCommandData(Str (&strings)[N]) {
    int cb = sizeofi(T);
    for (Str s : strings) {
        cb += std::max(len(s), 0) + 1;
    }
    auto* res = (T*)calloc(1, (size_t)cb);
    if (!res) {
        return nullptr;
    }

    char* dst = (char*)(res + 1);
    for (Str& s : strings) {
        int n = std::max(len(s), 0);
        if (n > 0 && s.s) {
            memcpy(dst, s.s, (size_t)n);
        }
        s = Str(dst, n);
        dst += n + 1;
    }
    return res;
}

CommandArg* AllocCommandArg(Str name, Str strVal) {
    Str strings[] = {name, strVal};
    auto* arg = AllocCommandData<CommandArg>(strings);
    if (arg) {
        arg->name = strings[0];
        arg->strVal = strings[1];
    }
    return arg;
}

void InsertArg(CommandArg** firstPtr, CommandArg* arg) {
    // for ease of use by callers, we shift null check here
    if (!arg) {
        return;
    }
    ListInsertFront(firstPtr, arg);
}

void FreeCommandArgs(CommandArg* first) {
    while (first) {
        CommandArg* next = first->next;
        free(first);
        first = next;
    }
}

static int gNextCustomCommandId = (int)CmdFirstCustom;

CustomCommand* AllocCustomCommand(Str definition, Str name, Str key) {
    Str strings[] = {definition, name, key};
    auto* cmd = AllocCommandData<CustomCommand>(strings);
    if (cmd) {
        cmd->definition = strings[0];
        cmd->name = strings[1];
        cmd->key = strings[2];
    }
    return cmd;
}

void FreeCustomCommand(CustomCommand* cmd) {
    if (!cmd) {
        return;
    }
    FreeCommandArgs(cmd->firstArg);
    free(cmd);
}

// Empty / whitespace name or key becomes empty. Invalid shortcut keys are
// rejected with a warning and stored as empty (same as SetCommandNameAndShortcut).
static void NormalizeCommandNameAndKey(Str definition, Str* name, Str* key) {
    if (str::IsEmptyOrWhiteSpace(*name)) {
        *name = {};
    }
    if (str::IsEmptyOrWhiteSpace(*key)) {
        *key = {};
        return;
    }
    if (!IsValidShortcutString(*key)) {
        logf("CreateCustomCommand: '%s' is not a valid shortcut for '%s'\n", *key, definition);
        MaybeDelayedWarningNotification(fmt("'%s' is not a valid shortcut for '%s'", *key, definition));
        *key = {};
    }
}

CustomCommand* CreateCustomCommand(Str definition, int origCmdId, CommandArg* args, Str name, Str key) {
    // if no args we retain original command id
    // only when we have unique args we have to create a new command id
    int id = origCmdId;
    if (args != nullptr) {
        id = gNextCustomCommandId++;
    }
    NormalizeCommandNameAndKey(definition, &name, &key);
    auto* cmd = AllocCustomCommand(definition, name, key);
    cmd->id = id;
    cmd->origId = origCmdId;
    cmd->firstArg = args;
    cmd->next = gFirstCustomCommand;
    gFirstCustomCommand = cmd;
    return cmd;
}

static CommandArg* CopyCommandArgs(CommandArg* first) {
    CommandArg* res = nullptr;
    CommandArg** tail = &res;
    for (CommandArg* curr = first; curr; curr = curr->next) {
        auto* arg = AllocCommandArg(curr->name, curr->strVal);
        arg->type = curr->type;
        arg->boolVal = curr->boolVal;
        arg->intVal = curr->intVal;
        arg->floatVal = curr->floatVal;
        arg->colorVal = curr->colorVal;
        *tail = arg;
        tail = &arg->next;
    }
    return res;
}

// Each settings entry needs its own toolbar ID, name and shortcut.
// Copy arguments so entries can be freed independently.
CustomCommand* CloneCustomCommand(CustomCommand* cmd, Str name, Str key) {
    auto* args = CopyCommandArgs(cmd->firstArg);
    auto* res = CreateCustomCommand(cmd->definition, cmd->origId, args, name, key);
    // Argumentless clones still need distinct toolbar IDs.
    if (!args) {
        res->id = gNextCustomCommandId++;
    }
    return res;
}

CustomCommand* FindCustomCommand(int cmdId) {
    for (auto* cmd = gFirstCustomCommand; cmd; cmd = cmd->next) {
        if (cmd->id == cmdId) {
            return cmd;
        }
    }
    return nullptr;
}

void FreeCustomCommands() {
    while (gFirstCustomCommand) {
        auto* cmd = gFirstCustomCommand;
        gFirstCustomCommand = cmd->next;
        FreeCustomCommand(cmd);
    }
}

void GetCommandsWithOrigId(Vec<CustomCommand*>& commands, int origId) {
    for (auto* cmd = gFirstCustomCommand; cmd; cmd = cmd->next) {
        if (cmd->origId == origId) {
            VecAppend(commands, cmd);
        }
    }
    // reverse so that they are returned in the order they were inserted
    VecReverse(commands);
}

static CommandArg* NewArg(CommandArg::Type type, Str name) {
    auto* res = AllocCommandArg(name, {});
    res->type = type;
    return res;
}

CommandArg* NewStringArg(Str name, Str val) {
    auto* res = AllocCommandArg(name, val);
    res->type = CommandArg::Type::String;
    return res;
}

CommandArg* NewFloatArg(Str name, float val) {
    auto* res = NewArg(CommandArg::Type::Float, name);
    res->floatVal = val;
    return res;
}

static CommandArg* ParseArgOfType(Str argName, CommandArg::Type type, Str val) {
    if (type == CommandArg::Type::Color) {
        ParsedColor col;
        ParseColor(col, val);
        if (!col.parsedOk) {
            // invalid value, skip it
            logf("parseArgOfType: invalid color value '%s'\n", val);
            return nullptr;
        }
        auto* arg = NewArg(type, argName);
        arg->colorVal = col;
        return arg;
    }

    if (type == CommandArg::Type::Int) {
        auto* arg = NewArg(type, argName);
        arg->intVal = ParseInt(val);
        return arg;
    }

    if (type == CommandArg::Type::String) {
        return NewStringArg(argName, val);
    }

    ReportIf(true);
    return nullptr;
}

static int ParseBool(Str s);

static CommandArg* TryParseDefaultArg(int defaultArgIdx, Str* argsInOut) {
    // first is default value
    Str rest = *argsInOut;
    str::TrimChar(rest, ' ');
    Str argName = argSpecs[defaultArgIdx].name;
    CommandArg::Type type = argSpecs[defaultArgIdx].type;
    Str val = rest;
    Str after;
    // A positional string consumes the rest, including spaces.
    if (type != CommandArg::Type::String) {
        str::CutChar(rest, ' ', &val, &after);
    }
    str::TrimChar(after, ' ');
    *argsInOut = after;

    if (type == CommandArg::Type::Bool) {
        // a default (positional) bool, e.g. [CmdToggleFullscreen on] (issue #5067)
        auto* arg = NewArg(type, argName);
        arg->boolVal = ParseBool(val) != 0; // 1 -> true, 0 -> false, -1 (unrecognized) -> true
        return arg;
    }
    return ParseArgOfType(argName, type, val);
}

// 1  : true
// 0  : false
// -1 : not a known boolean string
// returns 1 for a true value, 0 for a false value, -1 if not a recognized bool
static int ParseBool(Str s) {
    if (str::EqI(s, StrL("1")) || str::EqI(s, StrL("true")) || str::EqI(s, StrL("yes")) || str::EqI(s, StrL("on"))) {
        return 1;
    }
    if (str::EqI(s, StrL("0")) || str::EqI(s, StrL("false")) || str::EqI(s, StrL("no")) || str::EqI(s, StrL("off"))) {
        return 0;
    }
    return -1;
}

// parse:
//   <name> <value>
//   <name>: <value>
//   <name>=<value>
// for booleans only <name> works as well and represents true
static CommandArg* TryParseNamedArg(int firstArgIdx, Str* argsInOut) {
    Str valStart;
    Str argName;
    CommandArg::Type type = CommandArg::Type::None;
    Str rest = *argsInOut;
    int cmdId = argSpecs[firstArgIdx].cmdId;
    for (int i = firstArgIdx;; i++) {
        if (argSpecs[i].cmdId != cmdId) {
            // not a known argument for this command
            return nullptr;
        }
        argName = argSpecs[i].name;
        if (!str::TrimPrefixI(rest, argName)) {
            continue;
        }
        type = argSpecs[i].type;
        break;
    }
    if (len(rest) == 0 || rest.s[0] == ' ') {
        valStart = rest;
        str::TrimChar(valStart, ' ');
        if (type == CommandArg::Type::Bool) {
            *argsInOut = len(rest) == 0 ? Str{} : valStart;
            auto* arg = NewArg(type, argName);
            arg->boolVal = true;
            return arg;
        }
    } else if (rest.len >= 2 && rest.s[0] == ':' && rest.s[1] == ' ') {
        valStart = Str(rest.s + 1, rest.len - 1);
        str::TrimChar(valStart, ' ');
    } else if (rest.s[0] == '=') {
        valStart = Str(rest.s + 1, rest.len - 1);
    }
    if (len(valStart) == 0) {
        // <args> doesn't start with any of the available commands for this command
        return nullptr;
    }
    Str val = valStart;
    Str afterVal;
    str::CutChar(valStart, ' ', &val, &afterVal);
    if (type == CommandArg::Type::Bool) {
        auto bv = ParseBool(val);
        if (bv < 0) {
            return nullptr;
        }
        *argsInOut = afterVal;
        auto* arg = NewArg(type, argName);
        arg->boolVal = (bv == 1);
        return arg;
    }

    *argsInOut = afterVal;
    return ParseArgOfType(argName, type, val);
}

// create custom command as defined in Shortcuts section in advanced settings
// or DDE commands
// return null if unkown command
CustomCommand* CreateCommandFromDefinition(Str definition) {
    // Ignore empty Shortcuts entries, including stray "[ ]" blocks.
    if (str::IsEmptyOrWhiteSpace(definition)) {
        return nullptr;
    }

    // the same command can be sent via DDE many times
    // we don't want to create duplicate CustomCommand
    for (auto* cmd = gFirstCustomCommand; cmd; cmd = cmd->next) {
        if (str::Eq(definition, cmd->definition)) {
            return cmd;
        }
    }

    Str cmd = definition;
    str::TrimChar(cmd, ' ');
    Str currArg;
    str::CutChar(cmd, ' ', &cmd, &currArg);
    str::TrimChar(currArg, ' ');
    int cmdId = GetCommandIdByName(cmd);
    if (cmdId < 0) {
        MaybeDelayedWarningNotification(
            fmt("Error parsing Shortcuts in advanced settings. Unknown cmd name '%s'\n", definition));
        return nullptr;
    }
    if (len(currArg) == 0) {
        return CreateCustomCommand(definition, cmdId, nullptr);
    }

    // Annotation and navigation commands share argument specifications.
    int argCmdId = cmdId;
    if (cmdId >= CmdCreateAnnotFirst && cmdId <= CmdCreateAnnotLast) {
        argCmdId = CmdCreateAnnotText;
    } else if (cmdId == CmdScrollUp || cmdId == CmdScrollDown || cmdId == CmdGoToNextPage || cmdId == CmdGoToPrevPage) {
        argCmdId = CmdScrollUp;
    }

    int firstArgIdx = 0;
    for (;; firstArgIdx++) {
        int id = argSpecs[firstArgIdx].cmdId;
        if (id == CmdNone) {
            MaybeDelayedWarningNotification(
                fmt("Error parsing Shortcuts: cmd '%s' doesn't accept arguments\n", definition));
            return CreateCustomCommand(definition, cmdId, nullptr);
        }
        if (id == argCmdId) {
            break;
        }
    }

    currArg = str::DupTemp(currArg);

    CommandArg* firstArg = nullptr;
    while (currArg) {
        CommandArg* arg = TryParseNamedArg(firstArgIdx, &currArg);
        if (!arg) {
            arg = TryParseDefaultArg(firstArgIdx, &currArg);
        }
        InsertArg(&firstArg, arg);
    }
    if (!firstArg) {
        MaybeDelayedWarningNotification(
            fmt("Error parsing Shortcuts: failed to parse arguments for '%s'\n", definition));
        return nullptr;
    }

    if (cmdId == CmdCommandPalette) {
        // validate mode
        Str s = firstArg->strVal;
        static SeqStrings validModes = ">\0#\0@\0:\0*\0$\0%\0=\0"; // TODO: "@@\0" ?
        if (SeqStrIndex(validModes, s) < 0) {
            logf("CreateCommandFromDefinition: invalid CmdCommandPalette mode in '%s'\n", definition);
            FreeCommandArgs(firstArg);
            firstArg = nullptr;
        }
    }

    if (cmdId == CmdZoomCustom) {
        // special case: the argument is declared as string but it really is float
        // we convert it in-place here
        float zoomVal = ZoomFromString(firstArg->strVal, 0);
        if (0 == zoomVal) {
            FreeCommandArgs(firstArg);
            MaybeDelayedWarningNotification(
                fmt("CreateCommandFromDefinition: failed to parse arguments in '%s'\n", definition));
            return nullptr;
        }
        firstArg->type = CommandArg::Type::Float;
        firstArg->floatVal = zoomVal;
    }
    if (cmdId == CmdToggleBoolSetting) {
        // validate the named boolean setting exists (case-insensitive leaf or path)
        Str settingName = firstArg->strVal;
        if (len(settingName) == 0 || !FindSettingsBoolSetting(settingName)) {
            MaybeDelayedWarningNotification(
                fmt("Error parsing Shortcuts: unknown boolean setting '%s' in '%s'\n", settingName, definition));
            // still create the command so the shortcut is registered; execute
            // will warn again if the name is still wrong
        }
    }
    return CreateCustomCommand(definition, cmdId, firstArg);
}

CommandArg* GetCommandArg(CustomCommand* cmd, Str name) {
    if (!cmd) {
        return nullptr;
    }
    for (CommandArg* arg = cmd->firstArg; arg; arg = arg->next) {
        if (str::EqI(arg->name, name)) {
            return arg;
        }
    }
    return nullptr;
}

int GetCommandIntArg(CustomCommand* cmd, Str name, int defValue) {
    auto* arg = GetCommandArg(cmd, name);
    return arg ? arg->intVal : defValue;
}

bool GetCommandBoolArg(CustomCommand* cmd, Str name, bool defValue) {
    auto* arg = GetCommandArg(cmd, name);
    return arg ? arg->boolVal : defValue;
}

Str GetCommandStringArg(CustomCommand* cmd, Str name, Str defValue) {
    auto* arg = GetCommandArg(cmd, name);
    return arg ? arg->strVal : defValue;
}
