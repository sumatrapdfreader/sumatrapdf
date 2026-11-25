/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "FileHistory.h"
#include "Theme.h"
#include "Annotation.h"
#include "AnnotTextPopup.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "Commands.h"
#include "ExternalViewers.h"
#include "Favorites.h"
#include "Translations.h"
#include "PagePosition.h"
#include "Accelerators.h"
#include "ImageSaveCropResize.h"
#include "GoogleLens.h"
#include "CommandAvailability.h"
#include "ReadAloud.h"
#include "ReadingAutoScroll.h"
#include "ReadingBar.h"
#include "Menu.h"
#include "MenuDefs.h"

// Menu definitions shared by the original app and ng.

// clang-format off
//[ ACCESSKEY_GROUP File Open Menu
MenuDef menuDefFileOpen[] = {
    {
        TrN("&Open..."),
        CmdOpenFile,
    },
    {
        TrN("Open Using &Windows File Picker..."),
        CmdOpenFileWithOSFilePicker,
    },
    {
        TrN("Open Using &SumatraPDF File Picker..."),
        CmdOpenFileWithSumatraFilePicker,
    },
    {
        TrN("Open from &URL..."),
        CmdOpenFromURL,
    },
    {
        TrN("Use SumatraPDF File Picker"),
        CmdToggleFilePicker,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
#if OS_WASM
    // the folder is the stored documents (/uploads)
    {
        TrN("&Next Document"),
        CmdOpenNextFileInFolder,
    },
    {
        TrN("&Previous Document"),
        CmdOpenPrevFileInFolder,
    },
    {
        TrN("&Browse Documents..."),
        CmdNavigateFilesInFolder,
    },
#else
    {
        TrN("&Next File in Folder"),
        CmdOpenNextFileInFolder,
    },
    {
        TrN("&Previous File in Folder"),
        CmdOpenPrevFileInFolder,
    },
    {
        TrN("&Browse Files in Folder..."),
        CmdNavigateFilesInFolder,
    },
#endif
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP File Open Menu

//[ ACCESSKEY_GROUP File Menu
MenuDef menuDefFile[] = {
    {
        TrN("New &Window"),
        CmdNewWindow,
    },
    {
        TrN("&Open"),
        menuDefFileOpen,
    },
    {
        TrN("&Close"),
        CmdClose,
    },
    {
        TrN("Show in Fo&lder"),
        CmdShowInFolder,
    },
    {
        TrN("&Save As..."),
        CmdSaveAs,
    },
    {
        TrN("Convert to PDF..."),
        CmdConvertToPDF,
    },
//[ ACCESSKEY_ALTERNATIVE // only one of these two will be shown
#ifdef ENABLE_SAVE_SHORTCUT
    {
        TrN("Save Shortc&ut..."),
        CmdCreateShortcutToFile,
    },
//| ACCESSKEY_ALTERNATIVE
#else
    {
        TrN("Re&name..."),
        CmdRenameFile,
    },
    #endif
    //] ACCESSKEY_ALTERNATIVE
    {
        TrN("Delete"),
        CmdDeleteFile,
    },
    {
        TrN("&Print..."),
        CmdPrint,
    },
    {
        TrN("Lin&k SumatraPDF Web..."),
        CmdSumatraPDFWeb,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    //[ ACCESSKEY_ALTERNATIVE // PDF/XPS/CHM specific items are dynamically removed in RebuildFileMenu
    {
        TrN("Open Directory in &Explorer"),
        CmdOpenWithExplorer,
    },
    {
        TrN("Open Directory in Director&y Opus"),
        CmdOpenWithDirectoryOpus,
    },
    {
        TrN("Open Directory in &Total Commander"),
        CmdOpenWithTotalCommander,
    },
    {
        TrN("Open Directory in &Double Commander"),
        CmdOpenWithDoubleCommander,
    },
    {
        TrN("Open in &Adobe Reader"),
        CmdOpenWithAcrobat,
    },
    {
        TrN("Open in &Foxit Reader"),
        CmdOpenWithFoxit,
    },
    {
        TrN("Open &in PDF-XChange"),
        CmdOpenWithPdfXchange,
    },
    //| ACCESSKEY_ALTERNATIVE
    {
        TrN("Open in &Microsoft XPS-Viewer"),
        CmdOpenWithXpsViewer,
    },
    //| ACCESSKEY_ALTERNATIVE
    {
        TrN("Open in Microsoft &HTML Help"),
        CmdOpenWithHtmlHelp,
    },
    //] ACCESSKEY_ALTERNATIVE
    // further entries are added if specified in gSettings.vecCommandLine
    {
        TrN("Send &by E-mail..."),
        CmdSendByEmail,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("P&roperties..."),
        CmdProperties,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("E&xit"),
        CmdExit,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP File Menu

// the parts of the window to show; in View and the document context menu
//[ ACCESSKEY_GROUP Show Menu
MenuDef menuDefShow[] = {
    {
        TrN("&Bookmarks"),
        CmdToggleBookmarks,
    },
    {
        TrN("&Thumbnails"),
        CmdToggleThumbnails,
    },
    {
        TrN("&Favorites"),
        CmdFavoriteToggle,
    },
    {
        TrN("T&oolbar"),
        CmdToggleToolbar,
    },
    {
        TrN("&Menu Bar"),
        CmdToggleMenuBar,
    },
    {
        TrN("&Caption"),
        CmdToggleCaption,
    },
    {
        TrN("&Auto-Hide Sidebar"),
        CmdToggleSidebarAutoHide,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("&Highlight Form Fields"),
        CmdToggleHighlightFormFields,
    },
    {
        TrN("Transparency Gri&d"),
        CmdToggleTransparencyGrid,
    },
    {
        TrN("Page Grid"),
        CmdTogglePageGrid,
    },
    {
        TrN("Configure Page Grid..."),
        CmdConfigurePageGrid,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Show Menu

// how pages are laid out, like Adobe Reader's View > Page Display
//[ ACCESSKEY_GROUP Page Display Menu
MenuDef menuDefPageDisplay[] = {
    {
        TrN("&Single Page"),
        CmdSinglePageView,
    },
    {
        TrN("&Facing"),
        CmdFacingView,
    },
    {
        TrN("&Book View"),
        CmdBookView,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Show &Pages Continuously"),
        CmdToggleContinuousView,
    },
    // TODO: "&Inverse Reading Direction" (since some Mangas might be read left-to-right)?
    {
        TrN("Man&ga Mode"),
        CmdToggleMangaMode,
    },
    {
        TrN("&Uniform Page Width"),
        CmdToggleUniformPageWidth,
    },
    {
        TrN("Auto-Rotate Wide Pages"),
        CmdToggleAutoRotateWidePages,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Read &Columns"),
        CmdToggleReadColumns,
    },
    {
        TrN("Read Columns: Show &Whole Page"),
        CmdToggleColumnsWholePage,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("View as &Hex"),
        CmdViewAsHex,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Page Display Menu

//[ ACCESSKEY_GROUP Rotate Menu
MenuDef menuDefRotate[] = {
    {
        TrN("Rotate &Left"),
        CmdRotateLeft,
    },
    {
        TrN("Rotate &Right"),
        CmdRotateRight,
    },
    {
        TrN("Mirror Pages"),
        CmdToggleMirror,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Rotate Menu

//[ ACCESSKEY_GROUP Trim Menu
MenuDef menuDefTrim[] = {
    {
        TrN("&Trim Empty Margins"),
        CmdToggleTrimEmptyMargins,
    },
    {
        TrN("Trim Pages to Selection"),
        CmdTrimPagesToSelection,
    },
    {
        TrN("Show Whole Pages"),
        CmdShowWholePages,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Save Trimmed Pages As..."),
        CmdSaveTrimmedPagesAs,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Trim Menu

//[ ACCESSKEY_GROUP View Menu
MenuDef menuDefView[] = {
    {
        TrN("Command Palette"),
        CmdCommandPalette,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Page &Display"),
        menuDefPageDisplay,
    },
    {
        TrN("&Rotate"),
        menuDefRotate,
    },
    {
        TrN("Tri&m"),
        menuDefTrim,
    },
    {
        TrN("Sh&ow"),
        menuDefShow,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Pr&esentation"),
        CmdTogglePresentationMode,
    },
    {
        TrN("Fulls&creen"),
        CmdToggleFullscreen,
    },
    {
        TrN("&Automatically Scroll"),
        CmdToggleAutomaticallyScroll,
    },
    {
        TrN("Read&ing Bar"),
        CmdToggleReadingBar,
    },
    {
        TrN("Measure"),
        CmdMeasure,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("AI C&hat"),
        menuDefDocumentAIChat,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP View Menu

//[ ACCESSKEY_GROUP GoTo Menu
MenuDef menuDefGoTo[] = {
    {
        TrN("&Next Page"),
        CmdGoToNextPage,
    },
    {
        TrN("&Previous Page"),
        CmdGoToPrevPage,
    },
    {
        TrN("&First Page"),
        CmdGoToFirstPage,
    },
    {
        TrN("&Last Page"),
        CmdGoToLastPage,
    },
    {
        TrN("Pa&ge..."),
        CmdGoToPage,
    },
    {
        TrN("Navigate Thumbnails"),
        CmdNavigateThumbnail,
    },
    {
        TrN("Next &Chapter"),
        CmdGoToNextChapter,
    },
    {
        TrN("Previous C&hapter"),
        CmdGoToPrevChapter,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("&Back"),
        CmdNavigateBack,
    },
    {
        TrN("F&orward"),
        CmdNavigateForward,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Fin&d..."),
        CmdFindFirst,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP GoTo Menu

//[ ACCESSKEY_GROUP Zoom Menu
MenuDef menuDefZoom[] = {
    {
        TrN("Fit &Page"),
        CmdZoomFitPage,
    },
    {
        TrN("&Actual Size"),
        CmdZoomActualSize,
    },
    {
        TrN("Fit &Width"),
        CmdZoomFitWidth,
    },
    {
        TrN("Fit &Height"),
        CmdZoomFitHeight,
    },
    {
        TrN("Fit by &Orientation"),
        CmdZoomFitByOrientation,
    },
    {
        TrN("Fit &Content"),
        CmdZoomFitContent,
    },
    {
        TrN("Fit &Visible"),
        CmdZoomFitVisible,
    },
    {
        TrN("&Shrink to Fit"),
        CmdZoomShrinkToFit,
    },
    {
        TrN("Custom &Zoom..."),
        CmdZoomCustom,
    },
    {
        TrN("&To Selection"),
        CmdZoomToSelection,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        StrL("6400%"),
        CmdZoom6400,
    },
    {
        StrL("3200%"),
        CmdZoom3200,
    },
    {
        StrL("1600%"),
        CmdZoom1600,
    },
    {
        StrL("800%"),
        CmdZoom800,
    },
    {
        StrL("400%"),
        CmdZoom400,
    },
    {
        StrL("200%"),
        CmdZoom200,
    },
    {
        StrL("150%"),
        CmdZoom150,
    },
    {
        StrL("125%"),
        CmdZoom125,
    },
    {
        StrL("100%"),
        CmdZoom100,
    },
    {
        StrL("50%"),
        CmdZoom50,
    },
    {
        StrL("25%"),
        CmdZoom25,
    },
    {
        StrL("12.5%"),
        CmdZoom12_5,
    },
    {
        StrL("8.33%"),
        CmdZoom8_33,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Zoom Menu

// TODO: replace with CmdetTheme
MenuDef menuDefThemes[] = {
    {
        {},
        0,
    },
};

//[ ACCESSKEY_GROUP Settings Menu
MenuDef menuDefSettings[] = {
    {
        TrN("&Settings..."),
        CmdOptions,
    },
    {
        TrN("&Advanced Settings..."),
        CmdAdvancedSettings,
    },
    {
        TrN("&Open Advanced Settings File"),
        CmdOpenSettingsFile,
    },
    {
        TrN("Configure &Restricted Use..."),
        CmdConfigureRestrictedUse,
    },
    {
        TrN("Change Language..."),
        CmdChangeLanguage,
    },
    {
        TrN("&Theme"),
        menuDefThemes,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Settings Menu

//[ ACCESSKEY_GROUP Favorites Menu
MenuDef menuDefTabGroups[] = {
    {
        TrN("Save Tab Group..."),
        CmdTabGroupSave,
    },
    {
        TrN("Restore Tab Group..."),
        CmdTabGroupRestore,
    },
    {
        {},
        0,
    },
};

MenuDef menuDefFavorites[] = {
    {
        TrN("Add to Favorites..."),
        CmdFavoriteAdd,
    },
    {
        TrN("Remove from Favorites"),
        CmdFavoriteDel,
    },
    {
        TrN("Show Favorites"),
        CmdFavoriteToggle,
    },
    {
        TrN("Show Favorites in Tab"),
        CmdFavoriteShowInTab,
    },
    {
        TrN("Edit Favorites..."),
        CmdEditFavorites,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Tab Groups"),
        menuDefTabGroups,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Favorites Menu

//[ ACCESSKEY_GROUP Help Menu
MenuDef menuDefHelp[] = {
    {
        TrN("&Manual"),
        CmdHelpOpenManual,
    },
    {
        TrN("&Keyboard Shortcuts"),
        CmdHelpOpenKeyboardShortcuts
    },
    {
        TrN("Manual on Website"),
        CmdHelpOpenManualOnWebsite,
    },
    {
        TrN("Visit &Website"),
        CmdHelpVisitWebsite,
    },
    {
        TrN("Check for &Updates"),
        CmdCheckUpdate,
    },
    {
        TrN("Toggle Render Queue Info"),
        CmdDebugToggleRenderInfo,
    },
    {
        TrN("Toggle Cache Info"),
        CmdDebugToggleCacheInfo,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("&About..."),
        CmdHelpAbout,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Help Menu

//[ ACCESSKEY_GROUP Debug Menu
MenuDef menuDefDebug[] = {
    {
        StrL("Show links"),
        CmdToggleLinks,
    },
    {
        StrL("Show page boxes"),
        CmdTogglePageBoxes,
    },
    {
        StrL("Show images"),
        CmdToggleImages,
    },
    {
        StrL("Show fit content area"),
        CmdDebugShowFitContentArea,
    },
    {
        StrL("Show notification"),
        CmdDebugShowNotif,
    },
    {
        StrL("Show crash minidumps"),
        CmdDebugShowCrashMiniDumps,
    },
    {
        StrL("Show downloaded fonts"),
        CmdDebugShowDownloadedFonts,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Debug Menu

//[ ACCESSKEY_GROUP Context Menu (Google Lens)
MenuDef menuDefGoogleLens[] = {
    {
        TrN("&Selection as Image"),
        CmdSearchGoogleLens,
    },
    {
        TrN("&Page"),
        CmdSearchGoogleLensPage,
    },
    {
        TrN("Selected &Image"),
        CmdSearchGoogleLensImage,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Google Lens)

//[ ACCESSKEY_GROUP Translate With Menu
// shared by the Selection menu and the selection context menu
MenuDef menuDefTranslateWith[] = {
    {
        TrN("&Google..."),
        CmdTranslateSelectionWithGoogle,
    },
    {
        TrN("&DeepL..."),
        CmdTranslateSelectionWithDeepL,
    },
    {
        TrN("G&rok Build..."),
        CmdTranslateSelectionWithGrokBuild,
    },
    {
        TrN("Claude C&ode..."),
        CmdTranslateSelectionWithClaudeCode,
    },
    {
        TrN("OpenAI Code&x..."),
        CmdTranslateSelectionWithOpenAICodex,
    },
    {
        TrN("A&ntigravity..."),
        CmdTranslateSelectionWithAntiGravity,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Translate With Menu

//[ ACCESSKEY_GROUP Search With Menu
MenuDef menuDefSearchWith[] = {
    {
        TrN("&Google"),
        CmdSearchSelectionWithGoogle,
    },
    {
        TrN("&Bing"),
        CmdSearchSelectionWithBing,
    },
    {
        TrN("&Wikipedia"),
        CmdSearchSelectionWithWikipedia,
    },
    {
        TrN("Google Sc&holar"),
        CmdSearchSelectionWithGoogleScholar,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Search With Menu

//[ ACCESSKEY_GROUP Context Menu (Selection)
MenuDef menuDefSelection[] = {
    {
        TrN("&Copy to Clipboard"),
        CmdCopySelection,
    },
    {
        TrN("Copy as &Image to Clipboard"),
        CmdCopySelectionAsImage,
    },
    {
        TrN("&Save as Image..."),
        CmdSaveSelectionAsImage,
    },
    {
        TrN("&Print Selection..."),
        CmdPrintSelection,
    },
    {
        TrN("Visual Search with Google &Lens"),
        CmdSearchGoogleLens,
    },
    {
        TrN("&Zoom to Selection"),
        CmdZoomToSelection,
    },
    {
        TrN("&Trim Pages to Selection"),
        CmdTrimPagesToSelection,
    },
    {
        TrN("Count &Words"),
        CmdCountWords,
    },
    {
        TrN("Show Selected Te&xt"),
        CmdShowSelectedText,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("&Translate With"),
        menuDefTranslateWith,
    },
    {
        TrN("S&earch With"),
        menuDefSearchWith,
    },
    {
        TrN("Select C&urrent Page"),
        CmdSelectCurrentPage,
    },
    {
        TrN("Select &All"),
        CmdSelectAll,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Selection)

//[ ACCESSKEY_GROUP Menu (Selection)
MenuDef menuDefMainSelection[] = {
    {
        TrN("&Copy to Clipboard"),
        CmdCopySelection,
    },
    {
        TrN("&Translate With"),
        menuDefTranslateWith,
    },
    {
        TrN("S&earch With"),
        menuDefSearchWith,
    },
    {
        TrN("Select C&urrent Page"),
        CmdSelectCurrentPage,
    },
    {
        TrN("Select &All"),
        CmdSelectAll,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Menu (Selection)

//[ ACCESSKEY_GROUP Read Aloud Menu
// Placeholder only: real items are built in RebuildReadAloudMenu().
MenuDef menuDefReadAloud[] = {
    {
        TrN("Stop Reading"),
        CmdStopReadAloud,
    },
    {
        TrN("Start Reading from Top"),
        CmdReadAloudFromTopPage,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Read Aloud Menu

//[ ACCESSKEY_GROUP Context Menu (Read Aloud)
MenuDef menuDefContextReadAloud[] = {
    {
        TrN("Stop Reading"),
        CmdStopReadAloud,
    },
    {
        TrN("Start Reading from Top"),
        CmdReadAloudFromTopPage,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Read Aloud)

//[ ACCESSKEY_GROUP Menubar
MenuDef menuDefMenubar[] = {
    {
        TrN("&File"),
        menuDefFile,
    },
    {
        TrN("&View"),
        menuDefView,
    },
    {
        TrN("&Go To"),
        menuDefGoTo,
    },
    {
        TrN("&Zoom"),
        menuDefZoom,
    },
    {
        TrN("S&election"),
        menuDefMainSelection,
    },
    {
        TrN("Read Aloud"),
        menuDefReadAloud,
    },
    {
        TrN("F&avorites"),
        menuDefFavorites,
    },
    {
        TrN("&Settings"),
        menuDefSettings,
    },
    {
        TrN("&Help"),
        menuDefHelp,
    },
    {
        StrL("Debug"),
        menuDefDebug,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Menubar

//[ ACCESSKEY_GROUP Context Menu (Create annot from selection)
MenuDef menuDefCreateAnnotFromSelection[] = {
    {
        TrN("&Highlight"),
        CmdCreateAnnotHighlight,
    },
    {
        TrN("&Underline"),
        CmdCreateAnnotUnderline,
    },
    {
        TrN("&Strike Out"),
        CmdCreateAnnotStrikeOut,
    },
    {
        TrN("S&quiggly"),
        CmdCreateAnnotSquiggly,
    },
    {
        TrN("&Redact"),
        CmdCreateAnnotRedact,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Create annot from selection)

//[ ACCESSKEY_GROUP Context Menu (Annotations)
// everything annotation-related in the page context menu lives here, so the
// menu itself stays short
MenuDef menuDefContextAnnotations[] = {
    {
        TrN("Create from Selection"),
        menuDefCreateAnnotFromSelection,
    },
    {
        TrN("Create &Under Cursor"),
        menuDefCreateAnnotUnderCursor,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Edit Annotation Text"),
        CmdEditAnnotationText,
    },
    {
        TrN("Cut Annotation"),
        CmdCutAnnotation,
    },
    {
        TrN("Copy Annotation"),
        CmdCopyAnnotation,
    },
    {
        TrN("Paste Annotation"),
        CmdPasteAnnotation,
    },
    {
        TrN("Delete Annotation"),
        CmdDeleteAnnotation,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Apply Redactions"),
        CmdApplyRedactions,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Save Changes"),
        CmdSaveAnnotations,
    },
    {
        TrN("Save to New File..."),
        CmdSaveAnnotationsNewFile,
    },
    {
        TrN("Discard Changes"),
        CmdDiscardChanges,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Annotations)

//[ ACCESSKEY_GROUP Context Menu (Image)
MenuDef menuDefContextImage[] = {
    {
        TrN("C&opy to Clipboard"),
        CmdCopyImage,
    },
    {
        TrN("Visual Search with Google &Lens"),
        CmdSearchGoogleLensImage,
    },
    {
        TrN("&Save..."),
        CmdSaveImage,
    },
    {
        TrN("C&rop..."),
        CmdCropImage,
    },
    {
        TrN("R&esize..."),
        CmdResizeImage,
    },
    {
        TrN("Re&place Image..."),
        CmdReplaceImage,
    },
    {
        TrN("Edit Image in &Another App"),
        CmdEditImageExternally,
    },
    {
        TrN("Convert Page to &PDF..."),
        CmdConvertImageToPdf,
    },
    {
        TrN("Convert to PDF..."),
        CmdConvertToPDF,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Image)

//[ ACCESSKEY_GROUP Context Menu (Document AI chat)
MenuDef menuDefDocumentAIChat[] = {
    {
        TrN("Grok Build"),
        CmdAIChatWithGrokBuild,
    },
    {
        TrN("OpenAI Codex"),
        CmdAIChatWithOpenAICodex,
    },
    {
        TrN("Claude Code"),
        CmdAIChatWithClaudeCode,
    },
    {
        TrN("Antigravity"),
        CmdAIChatWithAntiGravity,
    },
    {
        TrN("Custom AI"),
        CmdAIChatWithCustomAPI,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Document AI chat)

//[ ACCESSKEY_GROUP Context Menu (Document)
MenuDef menuDefDocumentOperations[] = {
    {
        TrN("P&roperties..."),
        CmdProperties,
    },
    {
        TrN("Show PDF Info..."),
        CmdPdfShowInfo,
    },
    {
        TrN("Show Document Table of Contents..."),
        CmdDocumentShowOutline,
    },
    {
        TrN("Extract Pages..."),
        CmdPdfExtractPages,
    },
    {
        TrN("Delete Pages from PDF..."),
        CmdPdfDeletePages,
    },
    {
        TrN("Merge PDF..."),
        CmdMergePDF,
    },
    {
        TrN("Make PDF from Images..."),
        CmdImagesToPdf,
    },
    {
        TrN("Run PDF Script..."),
        CmdRunPDFScript,
    },
    {
        TrN("Extract Text from Document..."),
        CmdDocumentExtractText,
    },
    {
        TrN("Export as Markdown..."),
        CmdExportMarkdown,
    },
    {
        TrN("Export Annotations..."),
        CmdExportAnnotations,
    },
    {
        TrN("Compress PDF..."),
        CmdPdfCompress,
    },
    {
        TrN("Decompress PDF..."),
        CmdPdfDecompress,
    },
    {
        TrN("Encrypt PDF..."),
        CmdPdfEncrypt,
    },
    {
        TrN("Decrypt PDF..."),
        CmdPdfDecrypt,
    },
    {
        TrN("Bake PDF..."),
        CmdPdfBake,
    },
    {
        TrN("Add Watermark..."),
        CmdPdfWatermark,
    },
    {
        TrN("Add Page Numbers..."),
        CmdPdfAddPageNumbers,
    },
    {
        TrN("Insert Image..."),
        CmdInsertImage,
    },
    {
        TrN("Sign with Image"),
        CmdSignWithImage,
    },
    {
        TrN("Sign Document..."),
        CmdSignDocument,
    },
    {
        TrN("Convert to PDF..."),
        CmdConvertToPDF,
    },
    {
        TrN("Convert to Images..."),
        CmdConvertPdfToImages,
    },
    {
        TrN("Convert EPUB to PDF..."),
        CmdConvertEpubToPdf,
    },
    {
        TrN("Convert CAJ to PDF..."),
        CmdConvertCajToPDF,
    },
    {
        TrN("Show in Fo&lder"),
        CmdShowInFolder,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Document)

//[ ACCESSKEY_GROUP Context Menu (Start)
MenuDef menuDefContextStart[] = {
    {
        TrN("&Open Document"),
        CmdOpenSelectedDocument,
    },
    {
        TrN("Show in Folder"),
        CmdShowInFolder,
    },
    {
        TrN("&Pin Document"),
        CmdPinSelectedDocument,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("&Remove from History"),
        CmdForgetSelectedDocument,
    },
    {
        TrN("Delete File"),
        CmdDeleteFile,
    },
    {
        {},
        0,
    },
};

//] ACCESSKEY_GROUP Context Menu (Start)

static MenuDef menuDefContextTabDuplicate[] = {
    {
        TrN("In New Window"),
        CmdDuplicateInNewWindow,
    },
    {
        TrN("In New Tab"),
        CmdDuplicateInNewTab,
    },
    {
        {},
        0,
    },
};

MenuDef menuDefContextTab[] = {
    // these top items are removed unless the document has unsaved changes;
    // text matches the "Unsaved changes" close dialog
    {
        TrN("&Save Changes to Existing PDF"),
        CmdSaveAnnotations,
    },
    {
        TrN("Save Changes to &New PDF..."),
        CmdSaveAnnotationsNewFile,
    },
    {
        TrN("&Discard Changes"),
        CmdDiscardChanges,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Document Properties..."),
        CmdProperties,
    },
    {
        TrN("Show in Folder"),
        CmdShowInFolder,
    },
    {
        TrN("Copy File Path"),
        CmdCopyFilePath,
    },
    {
        TrN("Duplicate"),
        menuDefContextTabDuplicate,
    },
    {
        TrN("Change Tab Color..."),
        CmdSetTabColor,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Close"),
        CmdClose,
    },
    {
        TrN("Close Other Tabs"),
        CmdCloseOtherTabs,
    },
    {
        TrN("Close Tabs to the Right"),
        CmdCloseTabsToTheRight,
    },
    {
        TrN("Close Tabs to the Left"),
        CmdCloseTabsToTheLeft,
    },
    {
        TrN("Close All Tabs"),
        CmdCloseAllTabs,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    // each app puts an "Add Tab to Group" submenu after this one
    {
        TrN("Add Tab to New Group"),
        CmdStripGroupNew,
    },
    {
        TrN("Remove Tab from Group"),
        CmdStripGroupRemoveTab,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Save Tab Group..."),
        CmdTabGroupSave,
    },
    {
        TrN("Restore Tab Group..."),
        CmdTabGroupRestore,
    },
    {
        {},
        0,
    },
};

// right-click on a strip group's chip
MenuDef menuDefStripGroup[] = {
    {
        TrN("Rename Group..."),
        CmdStripGroupRename,
    },
    // each app puts a "Group Color" submenu after this one
    {
        TrN("Collapse Group"),
        CmdStripGroupToggle,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Ungroup"),
        CmdStripGroupUngroup,
    },
    {
        TrN("Close Group"),
        CmdStripGroupClose,
    },
    {
        {},
        0,
    },
};

MenuDef menuDefContextToc[] = {
    {
        TrN("Expand All"),
        CmdExpandAll,
    },
    {
        TrN("Collapse All"),
        CmdCollapseAll,
    },
    {
        TrN("Expand to Level 1"),
        CmdTocExpandToLevel1,
    },
    {
        TrN("Expand to Level 2"),
        CmdTocExpandToLevel2,
    },
    {
        TrN("Expand to Level 3"),
        CmdTocExpandToLevel3,
    },
    {
        TrN("Collapse Same Level"),
        CmdTocCollapseSameLevel,
    },
    {
        TrN("Expand Chapter"),
        CmdTocExpandChapter,
    },
    {
        TrN("Collapse Chapter"),
        CmdTocCollapseChapter,
    },
    {
        TrN("Expand to Current Page"),
        CmdExpandToCurrentPage,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Open Embedded PDF"),
        CmdOpenEmbeddedPDF,
    },
    {
        TrN("Save Embedded File..."),
        CmdSaveEmbeddedFile,
    },
    {
        TrN("Open Attachment"),
        CmdOpenAttachment,
    },
    {
        TrN("Save Attachment..."),
        CmdSaveAttachment,
    },
    // note: strings cannot be "" or else items are not there
    {
        StrL("Add to favorites"),
        CmdFavoriteAdd,
    },
    {
        StrL("Remove from favorites"),
        CmdFavoriteDel,
    },
    {
        {},
        0,
    },
};

MenuDef menuDefContextFav[] = {
    {
        TrN("Sort by Name"),
        CmdToggleFavoritesSort,
    },
    {
        TrN("Edit Favorites..."),
        CmdEditFavorites,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Remove from Favorites"),
        CmdFavoriteDel,
    },
    {
        {},
        0,
    },
};

//[ ACCESSKEY_GROUP Context Menu (Create annot under cursor)
MenuDef menuDefCreateAnnotUnderCursor[] = {
    {
        TrN("&Text"),
        CmdCreateAnnotText,
    },
    {
        TrN("&Free Text"),
        CmdCreateAnnotFreeText,
    },
    {
        TrN("&Highlighter"),
        CmdAnnotationHighlightBrush,
    },
    {
        TrN("&Stamp"),
        CmdCreateAnnotStamp,
    },
    {
        TrN("&Image from Clipboard"),
        CmdCreateAnnotImageFromClipboard,
    },
    {
        TrN("Image from Fi&le..."),
        CmdInsertImage,
    },
    {
        TrN("Si&gn with Image"),
        CmdSignWithImage,
    },
    {
        TrN("&Caret"),
        CmdCreateAnnotCaret,
    },
    {
        TrN("Line"),
        CmdCreateAnnotLine,
    },
    {
        TrN("Square"),
        CmdCreateAnnotSquare,
    },
    {
        TrN("Circle"),
        CmdCreateAnnotCircle,
    },
    //{
    //    TrN("Polygon"),
    //    CmdCreateAnnotPolygon,
    //},
    //{
    //    TrN("Polyline"),
    //    CmdCreateAnnotPolyLine,
    //},
    //{ TrN("Ink"), CmdCreateAnnotInk, },
    //{ TrN("File Attachment"), CmdCreateAnnotFileAttachment, },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Create annot under cursor)
// clang-format on
