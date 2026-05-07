import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";

// prettier-ignore
export const commands = [
    "CmdOpenFile", "Open File...",
    "CmdClose", "Close Document",
    "CmdCloseCurrentDocument", "Close Current Document",
    "CmdCloseOtherTabs", "Close Other Tabs",
    "CmdCloseTabsToTheRight", "Close Tabs To The Right",
    "CmdCloseTabsToTheLeft", "Close Tabs To The Left",
    "CmdCloseAllTabs", "Close All Tabs",
    "CmdSaveAs", "Save File As...",
    "CmdPrint", "Print Document...",
    "CmdShowInFolder", "Show File In Folder...",
    "CmdRenameFile", "Rename File...",
    "CmdDeleteFile", "Delete File",
    "CmdExit", "Exit Application",
    "CmdReloadDocument", "Reload Document",
    "CmdCreateShortcutToFile", "Create .lnk Shortcut",
    "CmdSendByEmail", "Send Document By Email...",
    "CmdProperties", "Show Document Properties...",
    "CmdSinglePageView", "Single Page View",
    "CmdFacingView", "Facing View",
    "CmdBookView", "Book View",
    "CmdToggleContinuousView", "Toggle Continuous View",
    "CmdToggleMangaMode", "Toggle Manga Mode",
    "CmdRotateLeft", "Rotate Left",
    "CmdRotateRight", "Rotate Right",
    "CmdToggleBookmarks", "Toggle Bookmarks",
    "CmdToggleTableOfContents", "Toggle Table Of Contents",
    "CmdToggleFullscreen", "Toggle Fullscreen",
    "CmdPresentationWhiteBackground", "Presentation White Background",
    "CmdPresentationBlackBackground", "Presentation Black Background",
    "CmdTogglePresentationMode", "View: Presentation Mode",
    "CmdToggleToolbar", "Toggle Toolbar",
    "CmdChangeScrollbar", "Change Scrollbar",
    "CmdToggleMenuBar", "Toggle Menu Bar",
    "CmdToggleUseTabs", "Toggle Use Tabs",
    "CmdToggleTabsMru", "Toggle Tabs MRU Order",
    "CmdCopySelection", "Copy Selection",
    "CmdTranslateSelectionWithGoogle", "Translate Selection with Google",
    "CmdTranslateSelectionWithDeepL", "Translate Selection With DeepL",
    "CmdSearchSelectionWithGoogle", "Search Selection with Google",
    "CmdSearchSelectionWithBing", "Search Selection with Bing",
    "CmdSearchSelectionWithWikipedia", "Search Selection with Wikipedia",
    "CmdSearchSelectionWithGoogleScholar", "Search Selection with Google Scholar",
    "CmdSelectAll", "Select All",
    "CmdNewWindow", "Open New SumatraPDF Window",
    "CmdDuplicateInNewWindow", "Open Current Document In New Window",
    "CmdDuplicateInNewTab", "Open Current Document In New Tab",
    "CmdCopyImage", "Copy Image",
    "CmdCopyLinkTarget", "Copy Link Target",
    "CmdCopyComment", "Copy Comment",
    "CmdCopyFilePath", "Copy File Path",
    "CmdScrollUp", "Scroll Up",
    "CmdScrollDown", "Scroll Down",
    "CmdScrollLeft", "Scroll Left",
    "CmdScrollRight", "Scroll Right",
    "CmdScrollLeftPage", "Scroll Left By Page",
    "CmdScrollRightPage", "Scroll Right By Page",
    "CmdScrollUpPage", "Scroll Up By Page",
    "CmdScrollDownPage", "Scroll Down By Page",
    "CmdScrollDownHalfPage", "Scroll Down By Half Page",
    "CmdScrollUpHalfPage", "Scroll Up By Half Page",
    "CmdGoToNextPage", "Next Page",
    "CmdGoToPrevPage", "Previous Page",
    "CmdGoToFirstPage", "First Page",
    "CmdGoToLastPage", "Last Page",
    "CmdGoToPage", "Go to Page...",
    "CmdFindFirst", "Find",
    "CmdFindNext", "Find Next",
    "CmdFindPrev", "Find Previous",
    "CmdFindNextSel", "Find Next Selection",
    "CmdFindPrevSel", "Find Previous Selection",
    "CmdFindToggleMatchCase", "Find: Toggle Match Case",
    "CmdSaveAnnotations", "Save Annotations to existing PDF",
    "CmdSaveAnnotationsNewFile", "Save Annotations to a new PDF",
    "CmdEditAnnotations", "Edit Annotations",
    "CmdDeleteAnnotation", "Delete Annotation",
    "CmdZoomFitPage", "Zoom: Fit Page",
    "CmdZoomActualSize", "Zoom: Actual Size",
    "CmdZoomFitWidth", "Zoom: Fit Width",
    "CmdZoom6400", "Zoom: 6400%",
    "CmdZoom3200", "Zoom: 3200%",
    "CmdZoom1600", "Zoom: 1600%",
    "CmdZoom800", "Zoom: 800%",
    "CmdZoom400", "Zoom: 400%",
    "CmdZoom200", "Zoom: 200%",
    "CmdZoom150", "Zoom: 150%",
    "CmdZoom125", "Zoom: 125%",
    "CmdZoom100", "Zoom: 100%",
    "CmdZoom50", "Zoom: 50%",
    "CmdZoom25", "Zoom: 25%",
    "CmdZoom12_5", "Zoom: 12.5%",
    "CmdZoom8_33", "Zoom: 8.33%",
    "CmdZoomFitContent", "Zoom: Fit Content",
    "CmdZoomShrinkToFit", "Zoom: Shrink To Fit",
    "CmdZoomCustom", "Zoom: Custom...",
    "CmdZoomIn", "Zoom In",
    "CmdZoomOut", "Zoom Out",
    "CmdZoomFitWidthAndContinuous", "Zoom: Fit Width And Continuous",
    "CmdZoomFitPageAndSinglePage", "Zoom: Fit Page and Single Page",
    "CmdContributeTranslation", "Contribute Translation",
    "CmdOpenWithKnownExternalViewerFirst", "don't use",
    "CmdOpenWithExplorer", "Open Directory In Explorer",
    "CmdOpenWithDirectoryOpus", "Open Directory In Directory Opus",
    "CmdOpenWithTotalCommander", "Open Directory In Total Commander",
    "CmdOpenWithDoubleCommander", "Open Directory In Double Commander",
    "CmdOpenWithAcrobat", "Open in Adobe Acrobat",
    "CmdOpenWithFoxIt", "Open in Foxit Reader",
    "CmdOpenWithFoxItPhantom", "Open in Foxit PhantomPDF",
    "CmdOpenWithPdfXchange", "Open in PDF-XChange",
    "CmdOpenWithXpsViewer", "Open in Microsoft Xps Viewer",
    "CmdOpenWithHtmlHelp", "Open in Microsoft HTML Help",
    "CmdOpenWithPdfDjvuBookmarker", "Open With Pdf&Djvu Bookmarker",
    "CmdOpenWithKnownExternalViewerLast", "don't use",
    "CmdOpenSelectedDocument", "Open Selected Document",
    "CmdPinSelectedDocument", "Pin Selected Document",
    "CmdForgetSelectedDocument", "Remove Selected Document From History",
    "CmdExpandAll", "Expand All",
    "CmdCollapseAll", "Collapse All",
    "CmdSaveEmbeddedFile", "Save Embedded File...",
    "CmdOpenEmbeddedPDF", "Open Embedded PDF",
    "CmdSaveAttachment", "Save Attachment...",
    "CmdOpenAttachment", "Open Attachment",
    "CmdOptions", "Options...",
    "CmdAdvancedOptions", "Advanced Options...",
    "CmdAdvancedSettings", "Advanced Settings...",
    "CmdChangeLanguage", "Change Language...",
    "CmdCheckUpdate", "Check For Updates",
    "CmdHelpOpenManual", "Help: Manual",
    "CmdHelpOpenManualOnWebsite", "Help: Manual On Website",
    "CmdHelpOpenKeyboardShortcuts", "Help: Keyboard Shortcuts",
    "CmdHelpVisitWebsite", "Help: SumatraPDF Website",
    "CmdHelpAbout", "Help: About SumatraPDF",
    "CmdMoveFrameFocus", "Move Frame Focus",
    "CmdFavoriteAdd", "Add Favorite",
    "CmdFavoriteDel", "Delete Favorite",
    "CmdFavoriteToggle", "Toggle Favorites",
    "CmdToggleLinks", "Toggle Show Links",
    "CmdToggleShowAnnotations", "Toggle Show Annotations",
    "CmdShowAnnotations", "Show Annotations",
    "CmdHideAnnotations", "Hide Annotations",
    "CmdCreateAnnotText", "Create Text Annotation",
    "CmdCreateAnnotLink", "Create Link Annotation",
    "CmdCreateAnnotFreeText", "Create Free Text Annotation",
    "CmdCreateAnnotLine", "Create Line Annotation",
    "CmdCreateAnnotSquare", "Create Square Annotation",
    "CmdCreateAnnotCircle", "Create Circle Annotation",
    "CmdCreateAnnotPolygon", "Create Polygon Annotation",
    "CmdCreateAnnotPolyLine", "Create Poly Line Annotation",
    "CmdCreateAnnotHighlight", "Create Highlight Annotation",
    "CmdCreateAnnotUnderline", "Create Underline Annotation",
    "CmdCreateAnnotSquiggly", "Create Squiggly Annotation",
    "CmdCreateAnnotStrikeOut", "Create Strike Out Annotation",
    "CmdCreateAnnotRedact", "Create Redact Annotation",
    "CmdCreateAnnotStamp", "Create Stamp Annotation",
    "CmdCreateAnnotCaret", "Create Caret Annotation",
    "CmdCreateAnnotInk", "Create Ink Annotation",
    "CmdCreateAnnotPopup", "Create Popup Annotation",
    "CmdCreateAnnotFileAttachment", "Create File Attachment Annotation",
    "CmdInvertColors", "Invert Colors",
    "CmdTogglePageInfo", "Toggle Page Info",
    "CmdToggleZoom", "Toggle Zoom",
    "CmdNavigateBack", "Navigate Back",
    "CmdNavigateForward", "Navigate Forward",
    "CmdToggleCursorPosition", "Toggle Cursor Position",
    "CmdOpenNextFileInFolder", "Open Next File In Folder",
    "CmdOpenPrevFileInFolder", "Open Previous File In Folder",
    "CmdCommandPalette", "Command Palette",
    "CmdShowLog", "Show Logs",
    "CmdShowErrors", "Show Errors",
    "CmdClearHistory", "Clear History",
    "CmdReopenLastClosedFile", "Reopen Last Closed",
    "CmdNextTab", "Next Tab",
    "CmdPrevTab", "Previous Tab",
    "CmdNextTabSmart", "Smart Next Tab",
    "CmdPrevTabSmart", "Smart Next Tab",
    "CmdMoveTabLeft", "Move Tab Left",
    "CmdMoveTabRight", "Move Tab Right",
    "CmdSelectNextTheme", "Select next theme",
    "CmdToggleFrequentlyRead", "Toggle Frequently Read",
    "CmdInvokeInverseSearch", "Invoke Inverse Search",
    "CmdExec", "Execute a program",
    "CmdViewWithExternalViewer", "View With Custom External Viewer",
    "CmdSelectionHandler", "Launch a browser or run command with selection",
    "CmdSetTheme", "Set theme",
    "CmdToggleInverseSearch", "Toggle Inverse Search",
    "CmdDebugCorruptMemory", "Debug: Corrupt Memory",
    "CmdDebugCrashMe", "Debug: Crash Me",
    "CmdDebugDownloadSymbols", "Debug: Download Symbols",
    "CmdDebugTestApp", "Debug: Test App",
    "CmdDebugShowNotif", "Debug: Show Notification",
    "CmdDebugStartStressTest", "Debug: Start Stress Test",
    "CmdDebugTogglePredictiveRender", "Debug: Toggle Predictive Rendering",
    "CmdDebugToggleRtl", "Debug: Toggle Rtl",
    "CmdToggleAntiAlias", "Toggle Anti-Alias Rendering",
    "CmdToggleSmoothScroll", "Toggle Smooth Scroll",
    // removed: CmdToggleHideScrollbar (replaced by CmdChangeScrollbar)
    "CmdToggleScrollbarInSinglePage", "Toggle Scrollbar In Single Page",
    "CmdToggleLazyLoading", "Toggle Lazy Loading",
    "CmdToggleEscToExit", "Toggle Esc to Exit",
    "CmdListPrinters", "List Printers",
    "CmdToggleWindowsPreviewer", "Toggle Windows Previewer",
    "CmdToggleWindowsSearchFilter", "Toggle Windows Search Filter",
    "CmdScreenshot", "Take Screenshot",
    "CmdCropImage", "Crop Image",
    "CmdResizeImage", "Resize Image",
    "CmdSaveImage", "Save Image",
    "CmdPasteClipboardImage", "Paste Image From Clipboard",
    "CmdTabGroupSave", "Save Tab Group",
    "CmdTabGroupRestore", "Restore Tab Group",
    "CmdToggleTips", "Toggle Tips",
    "CmdChangeBackgroundColor", "Change Background Color",
    "CmdSetTabColor", "Set Tab Color",
    "CmdPdfCompress", "Compress PDF",
    "CmdPdfDecompress", "Decompress PDF",
    "CmdPdfDeletePages", "Delete Pages From PDF",
    "CmdPdfExtractPages", "Extract Pages From PDF",
    "CmdPdfEncrypt", "Encrypt PDF",
    "CmdPdfDecrypt", "Decrypt PDF",
    "CmdPdfBake", "Bake PDF File",
    "CmdPdShowInfo", "Show PDF Info",
    "CmdDocumentExtractText", "Extract Text From Document",
    "CmdDocumentShowOutline", "Show Document Bookmarks",
    "CmdSetScreenshotHotkey", "Set Screenshot Hotkey",
    "CmdToggleReuseInstance", "Toggle Reuse Instance",
    "CmdToggleChmUI", "Toggle CHM UI",
    "CmdNone", "Do nothing",
];

function getNames(): string[] {
  const names: string[] = [];
  for (let i = 0; i < commands.length; i += 2) {
    names.push(commands[i]);
  }
  return names;
}

function getDescs(): string[] {
  const descs: string[] = [];
  for (let i = 0; i < commands.length; i += 2) {
    descs.push(commands[i + 1]);
  }
  return descs;
}

function generateEnum(): string {
  const names = getNames();
  const lines: string[] = [];

  lines.push("// clang-format off");
  lines.push("enum {");
  lines.push("    // commands are integers sent with WM_COMMAND so start them");
  lines.push("    // at some number higher than 0");
  lines.push("    CmdFirst = 200,");
  lines.push("    CmdSeparator = CmdFirst,");
  lines.push("");

  let firstCmdId = 201;
  for (let i = 0; i < names.length; i++) {
    let cmd = names[i];
    let id = firstCmdId + i;
    lines.push(`    ${cmd} = ${id},`);
  }

  lines.push("");
  lines.push("    /* range for file history */");
  lines.push("    CmdFileHistoryFirst,");
  lines.push("    CmdFileHistoryLast = CmdFileHistoryFirst + 32,");
  lines.push("");
  lines.push("    /* range for favorites */");
  lines.push("    CmdFavoriteFirst,");
  lines.push("    CmdFavoriteLast = CmdFavoriteFirst + 256,");
  lines.push("");
  lines.push("    CmdLast = CmdFavoriteLast,");
  lines.push("    CmdFirstCustom = CmdLast + 100,");
  lines.push("");
  lines.push("    // aliases, at the end to not mess ordering");
  lines.push("    CmdViewLayoutFirst = CmdSinglePageView,");
  lines.push("    CmdViewLayoutLast = CmdToggleMangaMode,");
  lines.push("");
  lines.push("    CmdZoomFirst = CmdZoomFitPage,");
  lines.push("    CmdZoomLast = CmdZoomCustom,");
  lines.push("");
  lines.push("    CmdCreateAnnotFirst = CmdCreateAnnotText,");
  lines.push("    CmdCreateAnnotLast = CmdCreateAnnotFileAttachment,");
  lines.push("};");
  lines.push("// clang-format on");

  return lines.join("\n");
}

function generateArrays(): string {
  const names = getNames();
  const descs = getDescs();
  const lines: string[] = [];

  lines.push("// clang-format off");

  // gCommandNames: SeqStrings (null-separated, double-null terminated)
  lines.push("static SeqStrings gCommandNames =");
  for (let i = 0; i < names.length; i++) {
    const chunk = names.slice(i, i + 1);
    const parts = chunk.map((s) => `"${s}\\0"`).join(" ");
    lines.push(`    ${parts}`);
  }
  lines.push(`    "\\0";`);
  lines.push("");

  // gCommandIds
  lines.push("static i32 gCommandIds[] = {");
  for (let i = 0; i < names.length; i++) {
    const chunk = names.slice(i, i + 1).join(", ");
    lines.push(`    ${chunk},`);
  }
  lines.push("};");
  lines.push("");

  // gCommandDescriptions
  lines.push("SeqStrings gCommandDescriptions =");
  for (let i = 0; i < descs.length; i++) {
    const chunk = descs.slice(i, i + 1);
    const parts = chunk.map((s) => `"${s}\\0"`).join(" ");
    lines.push(`    ${parts}`);
  }
  lines.push(`    "\\0";`);
  lines.push("// clang-format on");

  return lines.join("\n");
}

function replaceBetweenMarkers(content: string, startMarker: string, endMarker: string, generated: string): string {
  const startIdx = content.indexOf(startMarker);
  const endIdx = content.indexOf(endMarker);
  if (startIdx < 0 || endIdx < 0) {
    console.error(`Could not find markers '${startMarker}' and '${endMarker}'`);
    process.exit(1);
  }
  const before = content.substring(0, startIdx + startMarker.length);
  const after = content.substring(endIdx);
  return before + "\n" + generated + "\n" + after;
}

function main() {
  const rootDir = join(import.meta.dir, "..");
  const headerPath = join(rootDir, "src", "Commands.h");
  const cppPath = join(rootDir, "src", "Commands.cpp");

  let headerContent = readFileSync(headerPath, "utf-8");
  let cppContent = readFileSync(cppPath, "utf-8");

  const enumCode = generateEnum();
  headerContent = replaceBetweenMarkers(headerContent, "// @gen-start cmd-enum", "// @gen-end cmd-enum", enumCode);
  writeFileSync(headerPath, headerContent, "utf-8");
  console.log("Generated enum in src/Commands.h");

  const arraysCode = generateArrays();
  cppContent = replaceBetweenMarkers(cppContent, "// @gen-start cmd-c", "// @gen-end cmd-c", arraysCode);
  writeFileSync(cppPath, cppContent, "utf-8");
  console.log("Generated arrays in src/Commands.cpp");
}

if (import.meta.main) {
  main();
}
