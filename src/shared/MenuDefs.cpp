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

// The menus that are the same in orig and ng. An entry is a title and either a
// command id or, cast to an integer, the table of its submenu.

// clang-format off
//[ ACCESSKEY_GROUP File Open Menu
MenuDef menuDefFileOpen[] = {
    {
        TrN("&Open..."),
        CmdOpenFile,
    },
    {
        TrN("Open using &Windows File Picker..."),
        CmdOpenFileWithOSFilePicker,
    },
    {
        TrN("Open using &SumatraPDF File Picker..."),
        CmdOpenFileWithSumatraFilePicker,
    },
    {
        TrN("Use SumatraPDF File Picker"),
        CmdToggleFilePicker,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("&Next File In Folder"),
        CmdOpenNextFileInFolder,
    },
    {
        TrN("&Previous File In Folder"),
        CmdOpenPrevFileInFolder,
    },
    {
        TrN("&Browse Files In Folder..."),
        CmdNavigateFilesInFolder,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP File Open Menu

//[ ACCESSKEY_GROUP File Menu
MenuDef menuDefFile[] = {
    {
        TrN("New &window"),
        CmdNewWindow,
    },
    {
        TrN("&Open"),
        (UINT_PTR)menuDefFileOpen,
    },
    {
        TrN("&Close"),
        CmdClose,
    },
    {
        TrN("Show in fo&lder"),
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
        TrN("P&roperties"),
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

//[ ACCESSKEY_GROUP View Menu
MenuDef menuDefView[] = {
    {
        TrN("Command Palette"),
        CmdCommandPalette,
    },
    {
        TrN("Navigate Thumbnails"),
        CmdNavigateThumbnail,
    },
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
        TrN("Show &Pages Continuously"),
        CmdToggleContinuousView,
    },
    // TODO: "&Inverse Reading Direction" (since some Mangas might be read left-to-right)?
    {
        TrN("Man&ga Mode"),
        CmdToggleMangaMode,
    },
    {
        TrN("Auto-Rotate Wide Pages"),
        CmdToggleAutoRotateWidePages,
    },
    {
        TrN("&Uniform Page Width"),
        CmdToggleUniformPageWidth,
    },
    {
        TrN("&Trim Empty Margins"),
        CmdToggleTrimEmptyMargins,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Rotate &Left"),
        CmdRotateLeft,
    },
    {
        TrN("Rotate &Right"),
        CmdRotateRight,
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
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Show Book&marks"),
        CmdToggleBookmarks,
    },
    {
        TrN("Sho&w Thumbnails"),
        CmdToggleThumbnails,
    },
    {
        TrN("Show Me&nu"),
        CmdToggleMenuBar,
    },
    {
        TrN("Sh&ow Toolbar"),
        CmdToggleToolbar,
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
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Claude chat"),
        CmdAIChatWithClaudeCode,
    },
    {
        TrN("Grok chat"),
        CmdAIChatWithGrokBuild,
    },
    {
        TrN("Codex chat"),
        CmdAIChatWithOpenAICodex,
    },
    {
        TrN("Antigravity chat"),
        CmdAIChatWithAntiGravity,
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
        TrN("&Shrink To Fit"),
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
#if 0
    { TrN("Contribute Translation"),       CmdContributeTranslation },
    { StrL(kMenuSeparator),                       0                  },
#endif
    {
        TrN("&Settings..."),
        CmdOptions,
    },
    {
        TrN("&Advanced Settings..."),
        CmdAdvancedSettings,
    },
    {
        TrN("&Open Advanced Settings File..."),
        CmdOpenSettingsFile,
    },
    {
        TrN("Configure &Restricted Use..."),
        CmdConfigureRestrictedUse,
    },
    {
        TrN("Change Language"),
        CmdChangeLanguage,
    },
    {
        TrN("&Theme"),
        (UINT_PTR)menuDefThemes,
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
        TrN("Save Tab Group"),
        CmdTabGroupSave,
    },
    {
        TrN("Restore Tab Group"),
        CmdTabGroupRestore,
    },
    {
        {},
        0,
    },
};

MenuDef menuDefFavorites[] = {
    {
        TrN("Add to favorites"),
        CmdFavoriteAdd,
    },
    {
        TrN("Remove from favorites"),
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
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Tab Groups"),
        (UINT_PTR)menuDefTabGroups,
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
        TrN("Manual On Website"),
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
        TrN("&About"),
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
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Debug Menu

//[ ACCESSKEY_GROUP Context Menu (Google Lens)
MenuDef menuDefGoogleLens[] = {
    {
        TrN("&Selection As Image"),
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
        TrN("&Google"),
        CmdTranslateSelectionWithGoogle,
    },
    {
        TrN("&DeepL"),
        CmdTranslateSelectionWithDeepL,
    },
    {
        TrN("G&rok Build"),
        CmdTranslateSelectionWithGrokBuild,
    },
    {
        TrN("Claude C&ode"),
        CmdTranslateSelectionWithClaudeCode,
    },
    {
        TrN("OpenAI Code&x"),
        CmdTranslateSelectionWithOpenAICodex,
    },
    {
        TrN("A&ntigravity"),
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

//[ ACCESSKEY_GROUP Menu (Selection)
MenuDef menuDefMainSelection[] = {
    {
        TrN("&Copy To Clipboard"),
        CmdCopySelection,
    },
    {
        TrN("&Translate with"),
        (UINT_PTR)menuDefTranslateWith,
    },
    {
        TrN("S&earch with"),
        (UINT_PTR)menuDefSearchWith,
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
// idOrSubmenu must be a normal Cmd* id (not a Tts menu id above CmdLast), or
// BuildMenuFromDef mis-identifies it as a submenu pointer and crashes.
MenuDef menuDefReadAloud[] = {
    {
        TrN("Stop Reading"),
        CmdStopReadAloud,
    },
    {
        TrN("Start Reading From Top"),
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
        TrN("Start Reading From Top"),
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
        (UINT_PTR)menuDefFile,
    },
    {
        TrN("&View"),
        (UINT_PTR)menuDefView,
    },
    {
        TrN("&Go To"),
        (UINT_PTR)menuDefGoTo,
    },
    {
        TrN("&Zoom"),
        (UINT_PTR)menuDefZoom,
    },
    {
        TrN("S&election"),
        (UINT_PTR)menuDefMainSelection,
    },
    {
        TrN("Read Aloud"),
        (UINT_PTR)menuDefReadAloud,
    },
    {
        TrN("F&avorites"),
        (UINT_PTR)menuDefFavorites,
    },
    {
        TrN("&Settings"),
        (UINT_PTR)menuDefSettings,
    },
    {
        TrN("&Help"),
        (UINT_PTR)menuDefHelp,
    },
    {
        StrL("Debug"),
        (UINT_PTR)menuDefDebug,
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
        TrN("Create From Selection"),
        (UINT_PTR)menuDefCreateAnnotFromSelection,
    },
    {
        TrN("Create &Under Cursor"),
        (UINT_PTR)menuDefCreateAnnotUnderCursor,
    },
    {
        StrL(kMenuSeparator),
        kMenuSeparatorID,
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
        kMenuSeparatorID,
    },
    {
        TrN("Apply Redactions"),
        CmdApplyRedactions,
    },
    {
        StrL(kMenuSeparator),
        kMenuSeparatorID,
    },
    {
        TrN("Save changes"),
        CmdSaveAnnotations,
    },
    {
        TrN("Save to new file"),
        CmdSaveAnnotationsNewFile,
    },
    {
        TrN("Discard changes"),
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
        TrN("C&opy To Clipboard"),
        CmdCopyImage,
    },
    {
        TrN("Visual Search With Google &Lens"),
        CmdSearchGoogleLensImage,
    },
    {
        TrN("&Save"),
        CmdSaveImage,
    },
    {
        TrN("C&rop"),
        CmdCropImage,
    },
    {
        TrN("R&esize"),
        CmdResizeImage,
    },
    {
        TrN("Convert page to &PDF"),
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
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Document AI chat)

//[ ACCESSKEY_GROUP Context Menu (Document)
MenuDef menuDefDocumentOperations[] = {
    {
        TrN("P&roperties"),
        CmdProperties,
    },
    {
        TrN("Show PDF Info"),
        CmdPdfShowInfo,
    },
    {
        TrN("Show Document Table Of Contents"),
        CmdDocumentShowOutline,
    },
    {
        TrN("Extract Pages From PDF"),
        CmdPdfExtractPages,
    },
    {
        TrN("Delete Pages From PDF"),
        CmdPdfDeletePages,
    },
    {
        TrN("Merge PDF..."),
        CmdMergePDF,
    },
    {
        TrN("Run PDF Script..."),
        CmdRunPDFScript,
    },
    {
        TrN("Extract Text From Document"),
        CmdDocumentExtractText,
    },
    {
        TrN("Compress PDF"),
        CmdPdfCompress,
    },
    {
        TrN("Decompress PDF"),
        CmdPdfDecompress,
    },
    {
        TrN("Encrypt PDF"),
        CmdPdfEncrypt,
    },
    {
        TrN("Decrypt PDF"),
        CmdPdfDecrypt,
    },
    {
        TrN("Bake PDF"),
        CmdPdfBake,
    },
    {
        TrN("Insert Image..."),
        CmdInsertImage,
    },
    {
        TrN("Sign With Image"),
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
        TrN("Convert PDF to Images..."),
        CmdConvertPdfToImages,
    },
    {
        TrN("Show in fo&lder"),
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
        TrN("Show in folder"),
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
        TrN("&Remove From History"),
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

MenuDef menuDefContextTab[] = {
    // these top items are removed unless the document has unsaved changes;
    // text matches the "Unsaved changes" close dialog
    {
        TrN("&Save changes to existing PDF"),
        CmdSaveAnnotations,
    },
    {
        TrN("Save changes to &new PDF"),
        CmdSaveAnnotationsNewFile,
    },
    {
        TrN("&Discard changes"),
        CmdDiscardChanges,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Properties..."),
        CmdProperties,
    },
    {
        TrN("Show in folder"),
        CmdShowInFolder,
    },
    {
        TrN("Copy File Path"),
        CmdCopyFilePath,
    },
    {
        TrN("Open In New Window"),
        CmdDuplicateInNewWindow,
    },
    {
        TrN("Change Tab Color"),
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
        TrN("Close Tabs To The Right"),
        CmdCloseTabsToTheRight,
    },
    {
        TrN("Close Tabs To The Left"),
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
    {
        TrN("Save Tab Group"),
        CmdTabGroupSave,
    },
    {
        TrN("Restore Tab Group"),
        CmdTabGroupRestore,
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
        TrN("Sort By Name"),
        CmdToggleFavoritesSort,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Remove from favorites"),
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
        TrN("&Image From Clipboard"),
        CmdCreateAnnotImageFromClipboard,
    },
    {
        TrN("Image From Fi&le..."),
        CmdInsertImage,
    },
    {
        TrN("Si&gn With Image"),
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
