/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's Menu.cpp builds win32 HMENUs and owner-draws them. The tables
// below are orig's, unchanged; BuildMenuFromDef fills a portable MenuModel
// instead of an HMENU and the gpui shell renders it (src/gui/AppShell.cpp).
// The context menus, the external viewers, the theme / read-aloud / selection
// submenus and the custom menu-bar rebar come with steps 8-14.

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
#include "PagePosition.h"
#include "FileHistory.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "ReadingBar.h"
#include "ReadingAutoScroll.h"
#include "WindowTab.h"
#include "Commands.h"
#include "Favorites.h"
#include "Translations.h"
#include "ShortcutParse.h"
#include "Accelerators.h"
#include "CommandAvailability.h"
#include "ExternalViewers.h"
#include "SumatraDialogs.h"
#include "Annotation.h"
#include "AnnotTextPopup.h"
#include "ImageSaveCropResize.h"
#include "ReadAloud.h"
#include "GoogleLens.h"
#include "gui/DocCanvas.h"
#include "gui/Sidebar.h"
#include "Menu.h"

#include "SumatraLog.h"

constexpr uint kMenuSeparatorID = (uint)-13;

static bool ShowDebugMenu() {
    return gIsDebugBuild || gIsPreReleaseBuild;
}

// note: IDM_VIEW_SINGLE_PAGE - IDM_VIEW_CONTINUOUS and also
//       CmdZoomFIT_PAGE - CmdZoomCUSTOM must be in a continuous range!
static_assert(CmdViewLayoutLast - CmdViewLayoutFirst == 4, "view layout ids are not in a continuous range");
static_assert(CmdZoomLast - CmdZoomFirst == 19, "zoom ids are not in a continuous range");

// clang-format off
//[ ACCESSKEY_GROUP File Open Menu
static MenuDef menuDefFileOpen[] = {
    { TrN("&Open..."), CmdOpenFile, },
    { TrN("Open using &Windows File Picker..."), CmdOpenFileWithOSFilePicker, },
    { TrN("Open using &SumatraPDF File Picker..."), CmdOpenFileWithSumatraFilePicker, },
    { TrN("Use SumatraPDF File Picker"), CmdToggleFilePicker, },
    { StrL(kMenuSeparator), 0, },
    { TrN("&Next File In Folder"), CmdOpenNextFileInFolder, },
    { TrN("&Previous File In Folder"), CmdOpenPrevFileInFolder, },
    { TrN("&Browse Files In Folder..."), CmdNavigateFilesInFolder, },
    { {}, 0, },
};
//] ACCESSKEY_GROUP File Open Menu

//[ ACCESSKEY_GROUP File Menu
static MenuDef menuDefFile[] = {
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
static MenuDef menuDefView[] = {
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
static MenuDef menuDefGoTo[] = {
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
static MenuDef menuDefZoom[] = {
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
static MenuDef menuDefThemes[] = {
    {
        {},
        0,
    },
};

//[ ACCESSKEY_GROUP Settings Menu
static MenuDef menuDefSettings[] = {
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
static MenuDef menuDefTabGroups[] = {
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

static MenuDef menuDefFavorites[] = {
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
static MenuDef menuDefHelp[] = {
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
static MenuDef menuDefDebug[] = {
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

//[ ACCESSKEY_GROUP Translate With Menu
static MenuDef menuDefTranslateWith[] = {
    { TrN("&Google"), CmdTranslateSelectionWithGoogle, },
    { TrN("&DeepL"), CmdTranslateSelectionWithDeepL, },
    { TrN("G&rok Build"), CmdTranslateSelectionWithGrokBuild, },
    { TrN("Claude C&ode"), CmdTranslateSelectionWithClaudeCode, },
    { TrN("OpenAI Code&x"), CmdTranslateSelectionWithOpenAICodex, },
    { TrN("A&ntigravity"), CmdTranslateSelectionWithAntiGravity, },
    { {}, 0, },
};
//] ACCESSKEY_GROUP Translate With Menu

//[ ACCESSKEY_GROUP Search With Menu
static MenuDef menuDefSearchWith[] = {
    { TrN("&Google"), CmdSearchSelectionWithGoogle, },
    { TrN("&Bing"), CmdSearchSelectionWithBing, },
    { TrN("&Wikipedia"), CmdSearchSelectionWithWikipedia, },
    { TrN("Google Sc&holar"), CmdSearchSelectionWithGoogleScholar, },
    { {}, 0, },
};
//] ACCESSKEY_GROUP Search With Menu

//[ ACCESSKEY_GROUP Menu (Selection)
static MenuDef menuDefMainSelection[] = {
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
static MenuDef menuDefReadAloud[] = {
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

//] ACCESSKEY_GROUP Context Menu (Read Aloud)

//[ ACCESSKEY_GROUP Menubar
static MenuDef menuDefMenubar[] = {
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

//[ ACCESSKEY_GROUP Context Menu (Google Lens)
static MenuDef menuDefGoogleLens[] = {
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

//[ ACCESSKEY_GROUP Context Menu (Selection)
static MenuDef menuDefSelection[] = {
    {
        TrN("Select &All"),
        CmdSelectAll,
    },
    {
        TrN("&Copy To Clipboard"),
        CmdCopySelection,
    },
    {
        TrN("Copy As &Image To Clipboard"),
        CmdCopySelectionAsImage,
    },
    {
        TrN("&Save As Image..."),
        CmdSaveSelectionAsImage,
    },
    {
        TrN("&Print Selection..."),
        CmdPrintSelection,
    },
    {
        TrN("Visual Search With Google &Lens"),
        CmdSearchGoogleLens,
    },
    {
        TrN("&Zoom To Selection"),
        CmdZoomToSelection,
    },
    {
        StrL(kMenuSeparator),
        kMenuSeparatorID,
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
//] ACCESSKEY_GROUP Context Menu (Selection)

//[ ACCESSKEY_GROUP Context Menu (Read Aloud)
static MenuDef menuDefContextReadAloud[] = {
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

//[ ACCESSKEY_GROUP Context Menu (Create annot from selection)
static MenuDef menuDefCreateAnnotFromSelection[] = {
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

//[ ACCESSKEY_GROUP Context Menu (Create annot under cursor)
static MenuDef menuDefCreateAnnotUnderCursor[] = {
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
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Create annot under cursor)

//[ ACCESSKEY_GROUP Context Menu (Annotations)
// everything annotation-related in the page context menu lives here, so the
// menu itself stays short
static MenuDef menuDefContextAnnotations[] = {
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
static MenuDef menuDefContextImage[] = {
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
static MenuDef menuDefDocumentAIChat[] = {
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
static MenuDef menuDefDocumentOperations[] = {
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

//[ ACCESSKEY_GROUP Context Menu (Main)
static MenuDef menuDefContext[] = {
    {
        TrN("S&election"),
        (UINT_PTR)menuDefSelection,
    },
    {
        TrN("Visual Search With Google &Lens"),
        (UINT_PTR)menuDefGoogleLens,
    },
    {
        TrN("Copy Link &Address"),
        CmdCopyLinkTarget,
    },
    {
        TrN("Copy Co&mment"),
        CmdCopyComment,
    },
    {
        TrN("Sho&w Comment"),
        CmdShowAnnotationText,
    },
    {
        TrN("Save Attachment"),
        CmdSaveAttachment,
    },
    {
        TrN("Selected &Image"),
        (UINT_PTR)menuDefContextImage,
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
        TrN("Show &Favorites"),
        CmdFavoriteToggle,
    },
    {
        TrN("Show &Bookmarks"),
        CmdToggleBookmarks,
    },
    {
        TrN("Sh&ow Toolbar"),
        CmdToggleToolbar,
    },
    {
        StrL(kMenuSeparator),
        kMenuSeparatorID,
    },
    {
        TrN("AI chat with document using"),
        (UINT_PTR)menuDefDocumentAIChat,
    },
    {
        TrN("Document"),
        (UINT_PTR)menuDefDocumentOperations,
    },
    {
        TrN("Read Aloud"),
        (UINT_PTR)menuDefContextReadAloud,
    },
    {
        TrN("Annotations"),
        (UINT_PTR)menuDefContextAnnotations,
    },
    {
        TrN("Show Errors"),
        CmdShowErrors,
    },
    {
        TrN("E&xit Fullscreen"),
        CmdToggleFullscreen, // only seen in full-screen mode
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Main)

//[ ACCESSKEY_GROUP Context Menu (Start)
static MenuDef menuDefContextStart[] = {
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
// clang-format on

// clang-format off
// translate / search selection commands need selected text to operate on
static uintptr_t selectionTextCmds[] = {
    CmdTranslateSelectionWithGoogle,
    CmdTranslateSelectionWithDeepL,
    CmdTranslateSelectionWithGrokBuild,
    CmdTranslateSelectionWithClaudeCode,
    CmdTranslateSelectionWithOpenAICodex,
    CmdTranslateSelectionWithAntiGravity,
    CmdSearchSelectionWithGoogle,
    CmdSearchSelectionWithBing,
    CmdSearchSelectionWithWikipedia,
    CmdSearchSelectionWithGoogleScholar,
};

static uintptr_t menusNoTranslate[] = {
    CmdZoom6400,
    CmdZoom3200,
    CmdZoom1600,
    CmdZoom800,
    CmdZoom400,
    CmdZoom200,
    CmdZoom150,
    CmdZoom125,
    CmdZoom100,
    CmdZoom50,
    CmdZoom25,
    CmdZoom12_5,
    CmdZoom8_33,
};

static struct {
    int cmdId;
    float zoom;
} gZoomMenuIds[] = {
    { CmdZoom6400,        6400.0 },
    { CmdZoom3200,        3200.0 },
    { CmdZoom1600,        1600.0 },
    { CmdZoom800,         800.0  },
    { CmdZoom400,         400.0  },
    { CmdZoom200,         200.0  },
    { CmdZoom150,         150.0  },
    { CmdZoom125,         125.0  },
    { CmdZoom100,         100.0  },
    { CmdZoom50,          50.0   },
    { CmdZoom25,          25.0   },
    { CmdZoom12_5,        12.5   },
    { CmdZoom8_33,        8.33f  },
    { CmdZoomCustom,      0      },
    { CmdZoomFitPage,    kZoomFitPage    },
    { CmdZoomFitWidth,   kZoomFitWidth   },
    { CmdZoomFitHeight,  kZoomFitHeight  },
    { CmdZoomFitByOrientation, kZoomFitByOrientation },
    { CmdZoomFitContent, kZoomFitContent },
    { CmdZoomFitVisible, kZoomFitVisible },
    { CmdZoomShrinkToFit, kZoomShrinkToFit },
    { CmdZoomActualSize, kZoomActualSize },
};
// clang-format on

static bool CmdIdInList(uintptr_t cmdId, uintptr_t* idsList, int n) {
    for (int i = 0; i < n; i++) {
        uintptr_t id = idsList[i];
        if (id == cmdId) {
            return true;
        }
    }
    return false;
}

#define cmdIdInList(name) CmdIdInList((uintptr_t)cmdId, name, dimofi(name))

void DeleteMenuModel(MenuModel* m) {
    if (!m) {
        return;
    }
    for (MenuItemModel& it : m->items) {
        DeleteMenuModel(it.submenu);
        str::Free(it.accel);
        if (it.titleOwned) {
            str::Free(it.title);
        }
    }
    delete m;
}

static MenuItemModel* MenuAppend(MenuModel* m) {
    MenuItemModel item;
    VecAppend(m->items, item);
    return &m->items[m->items.len - 1];
}

// ng: win32's MF_BYCOMMAND searches submenus too, and the context menu code
// relies on that (it edits rows of the Image / Annotations submenus through
// the top-level popup), so MenuFind / MenuRemove recurse
static MenuItemModel* MenuFind(MenuModel* m, int cmdId) {
    for (MenuItemModel& it : m->items) {
        if (it.submenu) {
            MenuItemModel* found = MenuFind(it.submenu, cmdId);
            if (found) {
                return found;
            }
            continue;
        }
        if (it.cmdId == cmdId && !it.separator) {
            return &it;
        }
    }
    return nullptr;
}

void MenuRemove(MenuModel* m, int cmdId) {
    for (int i = 0; i < m->items.len; i++) {
        MenuItemModel& it = m->items[i];
        if (it.submenu) {
            MenuRemove(it.submenu, cmdId);
            continue;
        }
        if (it.cmdId != cmdId || it.separator) {
            continue;
        }
        str::Free(it.accel);
        if (it.titleOwned) {
            str::Free(it.title);
        }
        VecRemoveAt(m->items, i);
        return;
    }
}

// orig removes an Image submenu whose rows were all removed by scanning the
// top level for an empty popup; the same, but for every level
static void MenuRemoveEmptySubmenus(MenuModel* m) {
    for (int i = 0; i < m->items.len; i++) {
        MenuItemModel& it = m->items[i];
        if (!it.submenu) {
            continue;
        }
        MenuRemoveEmptySubmenus(it.submenu);
        if (it.submenu->items.len > 0) {
            continue;
        }
        DeleteMenuModel(it.submenu);
        if (it.titleOwned) {
            str::Free(it.title);
        }
        str::Free(it.accel);
        VecRemoveAt(m->items, i);
        i--;
    }
}

void MenuSetText(MenuModel* m, int cmdId, Str s) {
    MenuItemModel* it = MenuFind(m, cmdId);
    if (!it) {
        return;
    }
    if (it->titleOwned) {
        str::Free(it->title);
    }
    it->title = str::Dup(s);
    it->titleOwned = true;
}

void MenuSetEnabled(MenuModel* m, int cmdId, bool enabled) {
    MenuItemModel* it = MenuFind(m, cmdId);
    if (it) {
        it->disabled = !enabled;
    }
}

void MenuSetChecked(MenuModel* m, int cmdId, bool checked) {
    MenuItemModel* it = MenuFind(m, cmdId);
    if (it) {
        it->checked = checked;
    }
}

void MenuAppendString(MenuModel* m, Str title, int cmdId) {
    MenuItemModel* it = MenuAppend(m);
    it->title = str::Dup(title);
    it->titleOwned = true;
    it->cmdId = cmdId;
}

void MenuAppendSeparator(MenuModel* m) {
    MenuAppend(m)->separator = true;
}

// orig's MenuEmpty(HMENU): drop every row, keeping the model itself
void MenuEmpty(MenuModel* m) {
    for (MenuItemModel& it : m->items) {
        DeleteMenuModel(it.submenu);
        str::Free(it.accel);
        if (it.titleOwned) {
            str::Free(it.title);
        }
    }
    m->items.len = 0;
}

MenuModel* MenuAppendSubmenu(MenuModel* m, Str title) {
    auto* sub = new MenuModel();
    MenuItemModel* it = MenuAppend(m);
    it->title = str::Dup(title);
    it->titleOwned = true;
    it->submenu = sub;
    return sub;
}

// ng: orig removes separators from an HMENU by position; same three rules
void RemoveBadMenuSeparators(MenuModel* menu) {
    Vec<MenuItemModel>& items = menu->items;
    // remove separator items at the beginning
    while (items.len > 0 && items[0].separator) {
        VecRemoveAt(items, 0);
    }
    // remove separator items at the end
    while (items.len > 0 && items[items.len - 1].separator) {
        items.len--;
    }
    // remove 2 or more consecutive separator items
    for (int i = 1; i < items.len; i++) {
        if (items[i].separator && items[i - 1].separator) {
            VecRemoveAt(items, i);
            i--;
        }
    }
}

// orig's AppendCommandsToMenu: the custom commands with a given origId
static void AppendCommandsToMenu(MenuModel* menu, const Vec<CustomCommand*>& cmds, bool isEnabled) {
    for (CustomCommand* cmd : cmds) {
        if (len(cmd->name) == 0) {
            continue;
        }
        MenuItemModel* item = MenuAppend(menu);
        item->title = str::Dup(cmd->name);
        item->titleOwned = true;
        item->cmdId = cmd->id;
        item->disabled = !isEnabled;
        item->accel = str::Dup(ShortcutsForCmdTemp(cmd->id, 1));
    }
}

struct FileHistoryEntry {
    Str path;
    int cmdId;
};

static void SetFileHistoryCmdIds(Vec<FileHistoryEntry>& files) {
    Vec<CustomCommand*> cmds;
    GetCommandsWithOrigId(cmds, CmdFileHistory);
    for (CustomCommand* cmd : cmds) {
        Str path = GetCommandStringArg(cmd, kCmdArgFilePath, {});
        for (FileHistoryEntry& fe : files) {
            if (fe.cmdId == 0 && str::EqI(path, fe.path)) {
                fe.cmdId = cmd->id;
                break;
            }
        }
    }
    for (FileHistoryEntry& fe : files) {
        if (fe.cmdId != 0) {
            continue;
        }
        CommandArg* arg = NewStringArg(kCmdArgFilePath, fe.path);
        fe.cmdId = CreateCustomCommand(StrL("CmdFileHistory"), CmdFileHistory, arg)->id;
    }
}

static void AppendRecentFilesToMenu(MenuModel* menu) {
    if (!CanAccessDisk()) {
        return;
    }
    Vec<FileHistoryEntry> files;
    for (int i = 0; i < kFileHistoryMaxRecent; i++) {
        FileState* fs = FileHistoryGet(i);
        if (!fs || fs->isMissing) {
            break;
        }
        if (len(fs->filePath) > 0) {
            VecAppend(files, FileHistoryEntry{fs->filePath, 0});
        }
    }
    if (len(files) == 0) {
        return;
    }
    SetFileHistoryCmdIds(files);
    for (int i = 0; i < len(files); i++) {
        TempStr name = ShortenStringUtf8InTheMiddleTemp(path::GetBaseNameTemp(files[i].path), 70);
        name = str::ReplaceTemp(name, StrL("&"), StrL("&&"));
        MenuAppendString(menu, fmt("&%d) %s", (i + 1) % 10, name), files[i].cmdId);
    }
    MenuAppendSeparator(menu);
}

static void AppendThemesToMenu(MenuModel* menu) {
    Vec<CustomCommand*> cmds;
    GetCommandsWithOrigId(cmds, CmdSetTheme);
    AppendCommandsToMenu(menu, cmds, true);
}

// the SelectionHandlers entries from the settings file
static void AppendSelectionHandlersToMenu(MenuModel* menu, bool isEnabled) {
    Vec<CustomCommand*> cmds;
    GetCommandsWithOrigId(cmds, CmdSelectionHandler);
    AppendCommandsToMenu(menu, cmds, isEnabled);
}

// one snippet is a row of its own; several share an Insert Text submenu
static void AppendTextSnippetsToMenu(MenuModel* menu) {
    Vec<CustomCommand*> cmds;
    GetCommandsWithOrigId(cmds, CmdInsertTextSnippet);
    if (len(cmds) < 2) {
        AppendCommandsToMenu(menu, cmds, true);
        return;
    }
    MenuModel* sub = MenuAppendSubmenu(menu, Tr("Insert Te&xt"));
    AppendCommandsToMenu(sub, cmds, true);
}

// orig's AppendExternalViewersToMenu: the ExternalViewers entries from the
// settings file, added after the last built-in "Open in ..." row
static void AppendExternalViewersToMenu(MenuModel* menu, Str filePath) {
    if (!CanAccessDisk() || (len(filePath) > 0 && !file::Exists(filePath))) {
        return;
    }
    Vec<CustomCommand*> cmds;
    GetCommandsWithOrigId(cmds, CmdViewWithExternalViewer);
    for (CustomCommand* cmd : cmds) {
        Str commandLine = GetCommandStringArg(cmd, kCmdArgCommandLine, {});
        Str filter = GetCommandStringArg(cmd, kCmdArgFilter, {});
        if (str::IsEmptyOrWhiteSpace(commandLine)) {
            continue;
        }
        if (len(filter) > 0 && !(len(filePath) > 0 && PathMatchFilter(filePath, filter))) {
            continue;
        }
        TempStr name = cmd->name;
        if (str::IsEmptyOrWhiteSpace(name)) {
            name = path::GetBaseNameTemp(commandLine);
            int dotPos = str::IndexOfChar(name, '.');
            if (dotPos >= 0) {
                name = str::DupTemp(Str(name.s, dotPos));
            }
        }
        if (str::IsEmptyOrWhiteSpace(name)) {
            continue;
        }
        MenuItemModel* item = MenuAppend(menu);
        item->title = str::Dup(name);
        item->titleOwned = true;
        item->cmdId = cmd->id;
        item->disabled = len(filePath) == 0;
    }
}

MenuModel* BuildMenuFromDef(MenuDef* menuDef, BuildMenuCtx* ctx) {
    MenuModel* menu = new MenuModel();
    bool isDebugMenu = menuDef == menuDefDebug;
    int i = 0;
    bool addExternalViewersNext = false;

    if (menuDef == menuDefThemes) {
        AppendThemesToMenu(menu);
    }

    while (true) {
        MenuDef md = menuDef[i];
        if (len(md.title) == 0) { // sentinel
            break;
        }
        i++;

        int cmdId = (int)md.idOrSubmenu;
        // orig appends the user's external viewers after CmdOpenWithHtmlHelp
        if (addExternalViewersNext && ctx) {
            WindowTab* evTab = ctx->tab;
            AppendExternalViewersToMenu(menu, evTab ? evTab->filePath : Str{});
            addExternalViewersNext = false;
        }
        if (cmdId == CmdOpenWithHtmlHelp) {
            addExternalViewersNext = true;
        }
        // custom selection handlers go before the built-in translate / search submenus
        if (md.idOrSubmenu == (UINT_PTR)menuDefTranslateWith) {
            if (menuDef == menuDefMainSelection) {
                AppendSelectionHandlersToMenu(menu, true);
            } else if (menuDef == menuDefSelection) {
                AppendSelectionHandlersToMenu(menu, ctx ? ctx->hasSelection : false);
            }
        }
        if (menuDef == menuDefCreateAnnotUnderCursor && cmdId == CmdAnnotationHighlightBrush && ctx &&
            ctx->supportsAnnots) {
            AppendTextSnippetsToMenu(menu);
        }

        if (menuDef == menuDefFile && cmdId == CmdExit) {
            AppendRecentFilesToMenu(menu);
        }

        MenuDef* subMenuDef = (MenuDef*)md.idOrSubmenu;
        // hacky but works: small number is command id, large is submenu (a pointer)
        bool isSubMenu = md.idOrSubmenu > CmdLast + 10000;

        // handle separators before command state checks
        if (str::Eq(md.title, StrL(kMenuSeparator))) {
            MenuAppend(menu)->separator = true;
            continue;
        }

        // Only real commands have a command-state. For submenu entries cmdId is a
        // truncated pointer (garbage), so don't run it through GetCommandIdState.
        bool removeMenu = false;
        bool disableMenu = false;
        if (!isSubMenu && ctx) {
            GetCommandIdState(ctx, cmdId, &removeMenu, &disableMenu);
        }
        if (ctx) {
            removeMenu |= (menuDef == menuDefMainSelection) && !ctx->hasTextSelection && cmdIdInList(selectionTextCmds);
            // in the context menu only show translate / search items for a text
            // selection (the menubar variant is live-updated via
            // SetMenuStateForSelection instead)
            bool isTextSelSubMenu = (subMenuDef == menuDefTranslateWith) || (subMenuDef == menuDefSearchWith);
            removeMenu |= (menuDef == menuDefSelection) && !ctx->hasTextSelection && isTextSelSubMenu;
        }
        removeMenu |= !isSubMenu && cmdId == CmdSignWithImage && len(gSettings->annotations.signatureImage) == 0;
        removeMenu |= ((subMenuDef == menuDefDebug) && !ShowDebugMenu());
        if (removeMenu) {
            continue;
        }

        bool noTranslate = isDebugMenu || cmdIdInList(menusNoTranslate);
        noTranslate |= (subMenuDef == menuDefDebug);
        Str title = md.title;
        if (!noTranslate) {
            title = trans::GetTranslation(md.title);
        }

        if (isSubMenu) {
            MenuModel* subMenu = BuildMenuFromDef(subMenuDef, ctx);
            // orig fills the Favorites menu when the user opens it
            // (WM_INITMENUPOPUP); the model is rebuilt on every change instead
            if (subMenuDef == menuDefFavorites && ctx && ctx->win) {
                RebuildFavMenu(ctx->win, subMenu);
            }
            // same for the Read Aloud submenus: the voices, the speeds and what
            // "Pause" / "Continue" should read are only known now. Their
            // placeholder rows went through GetCommandIdState, so an empty
            // submenu means read aloud is not offered here (image document, no
            // TTS engine) and must stay empty
            if (subMenu->items.len > 0 && ctx && ctx->win) {
                if (subMenuDef == menuDefReadAloud) {
                    RebuildReadAloudMenu(ctx->win, subMenu);
                } else if (subMenuDef == menuDefContextReadAloud) {
                    RebuildReadAloudMenu(ctx->win, subMenu, true, ctx->win->contextMenuPtValid);
                }
            }
            if (subMenu->items.len == 0) {
                DeleteMenuModel(subMenu);
                continue;
            }
            MenuItemModel* item = MenuAppend(menu);
            item->title = title;
            item->submenu = subMenu;
            item->disabled = disableMenu;
        } else {
            MenuItemModel* item = MenuAppend(menu);
            item->title = title;
            item->cmdId = cmdId;
            item->disabled = disableMenu;
            // Ctrl+C / Ctrl+V are bound to CmdCopySelection and
            // CmdPasteClipboardImage, which hand off to these.
            int accelCmd = cmdId;
            if (cmdId == CmdCopyAnnotation) {
                accelCmd = CmdCopySelection;
            } else if (cmdId == CmdPasteAnnotation) {
                accelCmd = CmdPasteClipboardImage;
            }
            item->accel = str::Dup(ShortcutsForCmdTemp(accelCmd, 1));
        }
    }
    RemoveBadMenuSeparators(menu);
    return menu;
}

int CmdIdFromVirtualZoom(float virtualZoom) {
    for (auto&& it : gZoomMenuIds) {
        if (virtualZoom == it.zoom) {
            return it.cmdId;
        }
    }
    return CmdZoomCustom;
}

float ZoomMenuItemToZoom(int menuItemId) {
    for (auto&& it : gZoomMenuIds) {
        if (menuItemId == it.cmdId) {
            return it.zoom;
        }
    }
    ReportIf(true);
    return 100.0;
}

// ng: orig checks the radio items through MENUITEMINFO; here the model carries
// the flag and the shell draws the tick
static void SetMenuChecks(MenuModel* menu, MainWindow* win) {
    DocController* ctrl = win->ctrl;
    DisplayMode displayMode = gSettings->defaultDisplayModeEnum;
    if (ctrl) {
        displayMode = ctrl->GetDisplayMode();
    }
    int layoutCmd = 0;
    if (IsSingle(displayMode)) {
        layoutCmd = CmdSinglePageView;
    } else if (IsFacing(displayMode)) {
        layoutCmd = CmdFacingView;
    } else if (IsBookView(displayMode)) {
        layoutCmd = CmdBookView;
    }
    int zoomCmd = ctrl ? CmdIdFromVirtualZoom(ctrl->GetZoomVirtual()) : 0;

    for (MenuItemModel& it : menu->items) {
        if (it.submenu) {
            SetMenuChecks(it.submenu, win);
            continue;
        }
        if (it.cmdId == 0) {
            continue;
        }
        if (it.cmdId == CmdNavigateBack) {
            it.disabled = !ctrl || !ctrl->CanNavigate(-1);
            continue;
        }
        if (it.cmdId == CmdNavigateForward) {
            it.disabled = !ctrl || !ctrl->CanNavigate(1);
            continue;
        }
        if (it.cmdId == CmdToggleFilePicker) {
            it.checked = gSettings && str::EqI(gSettings->filePicker, StrL("sumatrapdf"));
            continue;
        }
        if (it.cmdId == gCurrSetThemeCmdId) {
            it.checked = true;
            continue;
        }
        if (it.cmdId >= CmdViewLayoutFirst && it.cmdId <= CmdViewLayoutLast) {
            it.checked = it.cmdId == layoutCmd;
            continue;
        }
        if (it.cmdId == CmdToggleContinuousView) {
            it.checked = IsContinuous(displayMode);
            continue;
        }
        if (it.cmdId == CmdTogglePageGrid) {
            it.checked = ShowPageGrid();
            continue;
        }
        if (it.cmdId == CmdToggleBookmarks || it.cmdId == CmdToggleTableOfContents) {
            it.checked = SidebarContentVisible(win, SidebarContent::Bookmarks);
            continue;
        }
        if (it.cmdId == CmdToggleThumbnails) {
            it.checked = SidebarContentVisible(win, SidebarContent::Thumbnails);
            continue;
        }
        if (it.cmdId == CmdFavoriteToggle) {
            it.checked = SidebarContentVisible(win, SidebarContent::Favorites);
            continue;
        }
        if (it.cmdId == CmdToggleLinks) {
            it.checked = gSettings->showLinks;
            continue;
        }
        if (it.cmdId == CmdTogglePageBoxes) {
            it.checked = win->showPageBoxes;
            continue;
        }
        if (it.cmdId == CmdToggleTransparencyGrid) {
            it.checked = ShowTransparencyGrid();
            continue;
        }
        if (it.cmdId == CmdToggleImages) {
            it.checked = ShowImageOutlines();
            continue;
        }
        if (it.cmdId == CmdDebugShowFitContentArea) {
            it.checked = ShowFitContentArea();
            continue;
        }
        if (it.cmdId == CmdToggleAutomaticallyScroll) {
            it.checked = ReadingAutoScrollIsOn(win);
            continue;
        }
        if (it.cmdId == CmdToggleReadingBar) {
            it.checked = ReadingBarIsOn(win);
            continue;
        }
        if (it.cmdId == CmdToggleReadingBarInvert) {
            it.checked = gSettings && gSettings->readingBar.invert;
            continue;
        }
        if (ctrl && it.cmdId >= CmdZoomFirst && it.cmdId <= CmdZoomLast) {
            it.checked = it.cmdId == zoomCmd;
        }
    }
}

MenuModel* BuildMenu(MainWindow* win) {
    AppCommandCtx ctx = NewAppCommandCtx(win);
    MenuModel* menu = BuildMenuFromDef(menuDefMenubar, &ctx);
    SetMenuChecks(menu, win);
    return menu;
}

static void AppendMenuRows(str::Builder& out, MenuModel* menu, Str parent) {
    for (int i = 0; i < menu->items.len; i++) {
        const MenuItemModel& item = menu->items[i];
        TempStr path = len(parent) > 0 ? fmt("%s.%d", parent, i) : fmt("%d", i);
        if (item.separator) {
            out.Append(fmt("%s\t-\t\t0\t0\t\t\n", path));
            continue;
        }
        Str kind = item.submenu ? StrL("S") : StrL("I");
        Str cmd = item.cmdId > 0 ? GetCommandName(item.cmdId) : Str{};
        Str title = ParseMenuAccelTextTemp(item.title).display;
        out.Append(fmt("%s\t%s\t%s\t%d\t%d\t%s\t%s\n", path, kind, cmd, item.disabled ? 1 : 0, item.checked ? 1 : 0,
                       title, item.accel));
        if (item.submenu) {
            AppendMenuRows(out, item.submenu, path);
        }
    }
}

static void AppendFavIds(str::Builder& out, MenuModel* menu) {
    if (!menu) {
        return;
    }
    for (int i = 0; i < len(menu->items); i++) {
        const MenuItemModel& item = menu->items[i];
        if (item.submenu) {
            AppendFavIds(out, item.submenu);
            continue;
        }
        CustomCommand* cmd = FindCustomCommand(item.cmdId);
        if (!cmd || cmd->origId != CmdFavorite) {
            continue;
        }
        Str title = ParseMenuAccelTextTemp(item.title).display;
        out.Append(fmt("id=%d text=%s\n", item.cmdId, title));
    }
}

// Favorite rows as "id=N text=Page 1". Ids are the custom commands a
// WM_COMMAND uses, and a later rebuild reuses them.
TempStr FavoritesMenuIdsTemp(MainWindow* win) {
    if (!win) {
        return {};
    }
    MenuModel* menu = BuildMenu(win);
    str::Builder out;
    AppendFavIds(out, menu);
    DeleteMenuModel(menu);
    return ToStrTemp(out);
}

// Stable main-menu dump for debug-control parity checks.
TempStr MainMenuResultTemp(MainWindow* win) {
    MenuModel* menu = BuildMenu(win);
    str::Builder out;
    AppendMenuRows(out, menu, {});
    DeleteMenuModel(menu);
    return ToStrTemp(out);
}

// The page context menu at a canvas point, in the same row format.
TempStr ContextMenuAtPointResultTemp(MainWindow* win, int x, int y) {
    if (!win) {
        return StrL("NOTREADY no-window\n");
    }
    MenuModel* menu = BuildWindowContextMenu(win, Point{x, y});
    if (!menu) {
        return Str{};
    }
    str::Builder out;
    AppendMenuRows(out, menu, {});
    DeleteMenuModel(menu);
    return ToStrTemp(out);
}

// --- the page context menu (orig's OnWindowContextMenu) ---------------------

// s could be in format "file://path.pdf#page=1" or "mailto:foo@bar.com"
// We only want the "path.pdf" / "foo@bar.com"
static TempStr CleanupURLForClipbardCopyTemp(Str s) {
    Str slice = s;
    str::TrimPrefix(slice, StrL("file:"));
    str::TrimPrefix(slice, StrL("mailto:"));
    return str::DupTemp(slice);
}

// orig puts the whole menu bar under a "Menu" row at the top while full screen
static void PrependMenuBarSubmenu(MenuModel* popup, MenuModel* bar) {
    MenuItemModel item;
    item.title = str::Dup(Tr("Menu"));
    item.titleOwned = true;
    item.submenu = bar;
    VecInsertAt(popup->items, 0, item);
}

// one line naming every row, so a scripted session can check the menu against
// orig's without a screenshot (a popup is not in what PrintWindow captures)
// ng: recurses into submenus so one log line shows the whole popup
static TempStr MenuRowsTemp(MenuModel* m) {
    str::Builder b;
    for (const MenuItemModel& it : m->items) {
        if (len(ToStr(b)) > 0) {
            b.Append(StrL(", "));
        }
        if (it.separator) {
            b.Append(StrL("---"));
            continue;
        }
        b.Append(ParseMenuAccelTextTemp(it.title).display);
        if (it.submenu) {
            b.AppendChar('>');
            b.Append(fmt(" (%s)", MenuRowsTemp(it.submenu)));
        }
        if (it.disabled) {
            b.Append(StrL("(off)"));
        }
        if (it.checked) {
            b.Append(StrL("(on)"));
        }
    }
    return ToStrTemp(b);
}

// ng: orig tracks the popup and acts on what TrackPopupMenu returns. gpui
// builds the popup from this model in the frame after the right button went
// down and reports the pick as an action, so the two halves are separate:
// this makes the model and remembers the point, WindowContextMenuCommand()
// runs the command against that point.
MenuModel* BuildWindowContextMenu(MainWindow* win, Point cursorPos) {
    DisplayModel* dm = win->AsFixed();
    if (!dm || !win->ctrl) {
        return nullptr;
    }
    WindowTab* tab = win->CurrentTab();
    int pageNoUnderEl = 0;
    IPageElement* pageEl = dm->GetElementAtPos(cursorPos, &pageNoUnderEl);

    // ng: orig rebuilds the Read Aloud context submenu after the popup is
    // built; here BuildMenuFromDef fills it, so the point it reads has to be
    // on the window first
    EngineBase* engine = dm->GetEngine();
    bool isImageDoc = engine && (engine->isImageCollection || engine->kind == kindEngineImage ||
                                 engine->kind == kindEngineImageDir || engine->kind == kindEngineComicBooks);
    win->contextMenuPt = cursorPos;
    win->contextMenuPtValid = !isImageDoc && ReadAloudCanReadFromCursor(dm, cursorPos);

    BuildMenuCtx* ctx = NewBuildMenuCtx(tab, cursorPos);
    MenuModel* popup = BuildMenuFromDef(menuDefContext, ctx);

    // in fullscreen, add "Menu" as first item containing the full menu bar
    bool isFullScreen = win->isFullScreen || win->presentation;
    if (isFullScreen) {
        MenuModel* bar = BuildMenuFromDef(menuDefMenubar, ctx);
        SetMenuChecks(bar, win);
        PrependMenuBarSubmenu(popup, bar);
    }

    int pageNoUnderCursor = dm->GetPageNoByPoint(cursorPos);

    bool onImage = pageEl && pageEl->Is(kindPageElementImage);
    onImage = onImage || (engine && engine->kind == kindEngineImage);
    if (pageNoUnderCursor > 0) {
        TempStr pageItem;
        if (ShowChapterUi(win->ctrl)) {
            Location loc = win->ctrl->LocationFromPageNo(pageNoUnderCursor);
            pageItem = fmt(Tr("Chapter %d Page %d").s, loc.chapter, loc.page);
        } else {
            TempStr pageLabel = win->ctrl->GetPageLabeTemp(pageNoUnderCursor);
            pageItem = fmt(Tr("Page %s").s, pageLabel);
        }
        MenuSetText(popup, CmdSearchGoogleLensPage, pageItem);
    }

    // show "Save Attachment" only for file attachment annotations
    {
        bool isFileAttachment = false;
        if (pageEl && pageEl->Is(kindPageElementDest)) {
            IPageDestination* elDest = pageEl->AsLink();
            if (elDest && elDest->GetKind() == kindDestinationLaunchEmbedded) {
                isFileAttachment = true;
            }
        }
        if (!isFileAttachment) {
            MenuRemove(popup, CmdSaveAttachment);
        }
    }
    {
        bool isImageEngine = tab && tab->GetEngineType() == kindEngineImage;
        if (!onImage && !isImageEngine) {
            MenuRemove(popup, CmdCopyImage);
        }
        if (!onImage) {
            MenuRemove(popup, CmdSaveImage);
        }
        if (!isImageEngine && !onImage) {
            MenuRemove(popup, CmdCropImage);
            MenuRemove(popup, CmdResizeImage);
            MenuRemove(popup, CmdConvertImageToPdf);
        }
    }

    if (!engine || !engine->HasErrors()) {
        MenuRemove(popup, CmdShowErrors);
    }
    if (!isFullScreen) {
        MenuRemove(popup, CmdToggleFullscreen);
    }

    MenuSetEnabled(popup, CmdToggleBookmarks, win->ctrl->HasToc());
    MenuSetChecked(popup, CmdToggleBookmarks, SidebarContentVisible(win, SidebarContent::Bookmarks));
    MenuSetChecked(popup, CmdToggleThumbnails, SidebarContentVisible(win, SidebarContent::Thumbnails));

    MenuSetEnabled(popup, CmdFavoriteToggle, HasFavorites());
    MenuSetChecked(popup, CmdFavoriteToggle, SidebarContentVisible(win, SidebarContent::Favorites));

    Str filePath = win->ctrl->GetFilePath();
    bool favsSupported = HasPermission(Perm::SavePreferences) && CanAccessDisk();
    if (favsSupported) {
        if (pageNoUnderCursor > 0) {
            bool isBookmarked = IsPageInFavorites(filePath, pageNoUnderCursor, win->ctrl);

            TempStr addText;
            TempStr delText;
            if (ShowChapterUi(win->ctrl)) {
                Location loc = win->ctrl->LocationFromPageNo(pageNoUnderCursor);
                addText = fmt(Tr("Add chapter %d page %d to favorites").s, loc.chapter, loc.page);
                delText = fmt(Tr("Remove chapter %d page %d from favorites").s, loc.chapter, loc.page);
            } else {
                TempStr pageLabel = win->ctrl->GetPageLabeTemp(pageNoUnderCursor);
                addText = fmt(Tr("Add page %s to favorites").s, pageLabel);
                delText = fmt(Tr("Remove page %s from favorites").s, pageLabel);
            }

            if (isBookmarked) {
                MenuRemove(popup, CmdFavoriteAdd);
                MenuSetText(popup, CmdFavoriteDel, delText);
            } else {
                MenuRemove(popup, CmdFavoriteDel);
                MenuSetText(popup, CmdFavoriteAdd, addText);
            }
        } else {
            MenuRemove(popup, CmdFavoriteAdd);
            MenuRemove(popup, CmdFavoriteDel);
        }
    }

    // if toolbar is not shown, add option to show it
    if (gSettings->showToolbar) {
        MenuRemove(popup, CmdToggleToolbar);
    }
    MenuRemoveEmptySubmenus(popup);
    RemoveBadMenuSeparators(popup);
    DeleteBuildMenuCtx(ctx);

    logf("BuildWindowContextMenu: at %d,%d: %s\n", cursorPos.x, cursorPos.y, MenuRowsTemp(popup));

    // highlight the element under cursor while the context menu is open
    if (pageEl && pageNoUnderEl > 0) {
        win->contextMenuHighlightRect = pageEl->GetRect();
        win->contextMenuHighlightPageNo = pageNoUnderEl;
    }
    return popup;
}

// Commands whose handler needs the original canvas click rather than the
// cursor's position after the context menu has closed.
bool CommandUsesContextMenuPoint(int cmdId) {
    CustomCommand* cmd = FindCustomCommand(cmdId);
    if (cmd && cmd->origId == CmdInsertTextSnippet) {
        return true;
    }
    if (cmdId == CmdAnnotationHighlightBrush) {
        // a mode that highlights the text selected next, not an annotation
        // placed at a point: dispatch it without one
        return false;
    }
    if (CmdIdToAnnotationType(cmdId) != AnnotationType::Unknown) {
        return true;
    }
    return cmdId == CmdDeleteAnnotation || cmdId == CmdCreateAnnotImageFromClipboard || cmdId == CmdInsertImage ||
           cmdId == CmdSignWithImage || cmdId == CmdPasteAnnotation || cmdId == CmdCopyAnnotation ||
           cmdId == CmdCutAnnotation;
}

// The image the context menu's Save / Crop / Resize / Convert to PDF act on:
// a standalone image file is loaded from disk (so Save can write the original
// bytes back), an image inside a document is rendered out of the engine.
static void ContextMenuEditImage(MainWindow* win, IPageElement* pageEl, int pageNo, int cmdId) {
    DisplayModel* dm = win->AsFixed();
    if (!dm || !pageEl || !pageEl->Is(kindPageElementImage)) {
        ExecuteCmd(win, cmdId);
        return;
    }
    EngineBase* imgEngine = dm->GetEngine();
    ImageEditMode m = ImageEditMode::Save;
    bool selectPdf = false;
    if (cmdId == CmdCropImage) {
        m = ImageEditMode::Crop;
    } else if (cmdId == CmdResizeImage) {
        m = ImageEditMode::Resize;
    } else if (cmdId == CmdConvertImageToPdf) {
        selectPdf = true;
    }
    if (imgEngine->kind == kindEngineImage && len(imgEngine->FilePath()) > 0) {
        ShowImageEditWindow(win, m, imgEngine->FilePath(), nullptr, selectPdf);
        return;
    }
    RenderedBitmap* bmp = imgEngine->GetImageForPageElement(pageEl);
    if (!bmp) {
        return;
    }
    Str filePath = win->ctrl ? win->ctrl->GetFilePath() : Str{};
    TempStr dir = path::GetDirTemp(filePath);
    TempStr base = path::GetBaseNameTemp(filePath);
    TempStr noExt = path::GetPathNoExtTemp(base);
    Str origData = imgEngine->GetImageDataForPageElement(pageEl);
    Str ext = ImageSaveExtFromData(origData);
    if (len(ext) == 0) {
        ext = StrL(".png");
    }
    TempStr destPath = path::JoinTemp(dir, fmt("%s_page_%d%s", noExt, pageNo, ext));
    // ShowImageEditWindow takes ownership of the bitmap
    ShowImageEditWindow(win, m, destPath, bmp, selectPdf, origData);
    str::Free(origData);
}

// orig's switch after TrackPopupMenu returned
void WindowContextMenuCommand(MainWindow* win, int cmdId) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    win->contextMenuHighlightPageNo = 0;
    if (HandleReadAloudMenuCommand(win, cmdId)) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm || !win->ctrl) {
        return;
    }
    logf("WindowContextMenuCommand: %d (%s)\n", cmdId, GetCommandName(cmdId));
    Point cursorPos = win->contextMenuPt;
    WindowTab* tab = win->CurrentTab();
    Str filePath = win->ctrl->GetFilePath();
    // ng: orig keeps the IPageElement it built the menu from; a frame has
    // passed here, so it is looked up again at the point the menu opened on
    int pageNoUnderEl = 0;
    IPageElement* pageEl = dm->GetElementAtPos(cursorPos, &pageNoUnderEl);
    Str value = pageEl ? pageEl->GetValue() : Str{};
    int pageNoUnderCursor = dm->GetPageNoByPoint(cursorPos);
    EngineBase* engine = dm->GetEngine();
    BuildMenuCtx* ctx = NewBuildMenuCtx(tab, cursorPos);
    AutoDelete<AppCommandCtx> delCtx(ctx);

    // handled by ExecuteAnnotCreateCmd & co, which need the canvas point
    if (CommandUsesContextMenuPoint(cmdId)) {
        ExecuteCmdAtPoint(win, cmdId, cursorPos);
        return;
    }

    switch (cmdId) {
        case CmdSearchGoogleLens:
            SearchGoogleLensSelection(tab);
            return;
        case CmdSearchGoogleLensPage:
            SearchGoogleLensPage(tab, pageNoUnderCursor);
            return;
        case CmdSearchGoogleLensImage:
            SearchGoogleLensImage(tab, pageEl);
            return;

        case CmdSaveImage:
        case CmdCropImage:
        case CmdResizeImage:
        case CmdConvertImageToPdf:
            ContextMenuEditImage(win, pageEl, pageNoUnderCursor, cmdId);
            return;

        case CmdCopyLinkTarget: {
            if (len(value) > 0) {
                TempStr tmp = CleanupURLForClipbardCopyTemp(value);
                CopyTextToClipboard(win, tmp);
            }
            return;
        }
        case CmdShowAnnotationText: {
            Annotation* annot = ctx->annotationUnderCursor;
            if (annot) {
                ShowAnnotationTextPopup(win, annot);
            }
            return;
        }
        case CmdCopyComment: {
            Str comment = value;
            if (ctx->annotationUnderCursor) {
                // The page element's value is hover text. For FreeText that is
                // only the author because the contents are already on the page.
                comment = Contents(ctx->annotationUnderCursor);
            }
            if (!str::IsEmptyOrWhiteSpace(comment)) {
                CopyTextToClipboard(win, comment);
            }
            return;
        }
        case CmdSaveAttachment: {
            if (!pageEl || !pageEl->Is(kindPageElementDest)) {
                return;
            }
            IPageDestination* elDest = pageEl->AsLink();
            auto* pd = (PageDestination*)elDest;
            if (!pd || pd->embedObjNum <= 0) {
                return;
            }
            // attachments are arbitrary binary
            Str data = EngineMupdfLoadAnnotAttachment(engine, pd->embedObjNum);
            if (len(data) == 0) {
                return;
            }
            Str fileName = pd->GetValue();
            TempStr dir = path::GetDirTemp(filePath);
            fileName = path::GetBaseNameTemp(fileName);
            TempStr dstPath = path::JoinTemp(dir, fileName);
            SaveDataToFile(win, dstPath, data);
            str::Free(data);
            return;
        }
        case CmdCopyImage: {
            if (pageEl) {
                RenderedBitmap* bmp = engine->GetImageForPageElement(pageEl);
                if (bmp) {
                    CopyRenderedBitmapToClipboard(win, bmp);
                }
            }
            return;
        }
        case CmdFavoriteAdd: {
            if (pageNoUnderCursor > 0) {
                AddFavoriteForPage(win, pageNoUnderCursor);
            }
            return;
        }
        case CmdFavoriteDel: {
            DelFavorite(filePath, pageNoUnderCursor, win->ctrl);
            return;
        }
    }
    ExecuteCmd(win, cmdId);
}

// --- the home page's context menu (orig's OnAboutContextMenu) ---------------

MenuModel* BuildHomeContextMenu(MainWindow* win, Str filePath) {
    if (!HasPermission(Perm::SavePreferences | Perm::DiskAccess) || !SettingsRememberOpenedFiles() ||
        !gSettings->showStartPage) {
        return nullptr;
    }
    if (len(filePath) == 0 || !path::IsAbsolute(filePath)) {
        return nullptr;
    }
    FileState* fs = FileHistoryFindByPath(filePath);
    if (!fs) {
        return nullptr;
    }
    AppCommandCtx ctx;
    ctx.win = win;
    ctx.isDocLoaded = true;
    ctx.filePath = filePath;
    MenuModel* popup = BuildMenuFromDef(menuDefContextStart, &ctx);
    MenuSetChecked(popup, CmdPinSelectedDocument, fs->isPinned);
    // Del is home-page-only (not a global accelerator), so the accelerator
    // table won't pick it up - show it next to Remove From History explicitly
    MenuItemModel* it = MenuFind(popup, CmdForgetSelectedDocument);
    if (it) {
        str::Free(it->accel);
        it->accel = str::Dup(StrL("Del"));
    }
    logf("BuildHomeContextMenu: '%s': %s\n", path::GetBaseNameTemp(filePath), MenuRowsTemp(popup));
    return popup;
}

void HomeContextMenuCommand(MainWindow* win, Str filePathIn, int cmdId) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    // own the path: deleting closes tabs and rewrites the history
    TempStr path = str::DupTemp(filePathIn);
    logf("HomeContextMenuCommand: %d (%s) on '%s'\n", cmdId, GetCommandName(cmdId), path);
    FileState* fs = FileHistoryFindByPath(path);
    switch (cmdId) {
        case CmdOpenSelectedDocument:
            LoadDocument(win, path);
            return;
        case CmdShowInFolder:
            ShowFileInFolder(win, path);
            return;
        case CmdPinSelectedDocument:
            if (fs) {
                fs->isPinned = !fs->isPinned;
                ScheduleSaveSettings();
                win->RedrawAll(true);
            }
            return;
        case CmdForgetSelectedDocument:
            ForgetFileFromFrequentlyRead(win, path);
            return;
        case CmdDeleteFile:
            DeleteFileFromHomePage(win, path);
            return;
    }
    ExecuteCmd(win, cmdId);
}

// orig's ToggleMenuBar, without the win32 menu / rebar handling: in fullscreen
// F9 flips Fullscreen.ShowMenubar, otherwise ShowMenubar / ShowMenubarWithTabs,
// so the choice is remembered. ng: showTemporarily (Alt alone) has nothing to
// do here, the Alt access keys are handled by the shell.
void ToggleMenuBar(MainWindow* win, bool showTemporarily) {
    if (showTemporarily) {
        return;
    }
    if (win->isFullScreen) {
        gSettings->fullscreen.showMenubar = !gSettings->fullscreen.showMenubar;
        win->isMenuBarVisible = gSettings->fullscreen.showMenubar;
    } else {
        bool hideMenu = win->isMenuBarVisible;
        win->isMenuBarVisible = !hideMenu;
        gSettings->showMenubar = !hideMenu;
        gSettings->showMenubarWithTabs = !hideMenu;
    }
    win->RedrawAll();
}

// Remove Win32's '&' accelerator markup and remember the first character that
// needs an underline. A doubled ampersand is a literal one. ng: orig draws the
// underline itself (DrawMenuText); gpui's menu takes a plain label, so only
// `display` is used and the access key drives Alt+<key> (see "UI differences").
MenuAccelText ParseMenuAccelTextTemp(Str s) {
    MenuAccelText res;
    if (!str::Contains(s, StrL("&"))) {
        res.display = s;
        return res;
    }
    char* buf = AllocArrayTemp<char>(len(s) + 1);
    int out = 0;
    for (int i = 0; i < len(s); i++) {
        if (s.s[i] != '&') {
            buf[out++] = s.s[i];
            continue;
        }
        if (i + 1 >= len(s)) {
            break;
        }
        if (s.s[i + 1] == '&') {
            buf[out++] = '&';
            i++;
            continue;
        }
        if (res.underlineOff < 0) {
            res.underlineOff = out;
            int remain = len(s) - i - 1;
            int n = utf8RuneLen((const u8*)(s.s + i + 1));
            res.underlineLen = std::min(std::max(n, 1), remain);
        }
    }
    buf[out] = 0;
    res.display = Str(buf, out);
    return res;
}

// the ASCII letter that opens this menu with Alt, lower-cased; 0 if it has none
char MenuAccessKey(Str title) {
    MenuAccelText parsed = ParseMenuAccelTextTemp(title);
    if (parsed.underlineOff < 0 || parsed.underlineLen != 1) {
        return 0;
    }
    char c = parsed.display.s[parsed.underlineOff];
    if (c >= 'A' && c <= 'Z') {
        c = (char)(c - 'A' + 'a');
    }
    return (c >= 'a' && c <= 'z') ? c : 0;
}
