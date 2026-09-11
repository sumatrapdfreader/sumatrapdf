/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/CmdLineArgs.h"
#include "base/File.h"
#include "base/BitManip.h"
#include "gui/Dpi.h"
#include "base/Win.h"
#include "base/Pixmap.h"
#include "base/GdiPlusUtil.h"

#include "gui/UIModels.h"
#include "gui/Gfx.h"
#include "gui/PlatformFont.h"

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
#include "Canvas.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "ExternalViewers.h"
#include "Favorites.h"
#include "FileThumbnails.h"
#include "HomePage.h"
#include "Translations.h"
#include "Toolbar.h"
#include "resource.h"
#include "DarkMode.h"
#include "Tabs.h"
#include "Accelerators.h"
#include "ImageSaveCropResize.h"
#include "GoogleLens.h"
#include "CommandAvailability.h"
#include "ReadAloud.h"
#include "ReadingAutoScroll.h"
#include "ReadingBar.h"
#include "Menu.h"

// value associated with menu item for owner-drawn purposes
struct MenuOwnerDrawInfo {
    Str text;
    // copy of MENUITEMINFO fields
    uint fType = 0;
    uint fState = 0;
    HBITMAP hbmpChecked = nullptr;
    HBITMAP hbmpUnchecked = nullptr;
    HBITMAP hbmpItem = nullptr;
};

constexpr UINT kMenuSeparatorID = (UINT)-13;

static bool gAddCrashMeMenu = false;
static bool ShowDebugMenu() {
    return gIsDebugBuild || gIsPreReleaseBuild;
}

// note: IDM_VIEW_SINGLE_PAGE - IDM_VIEW_CONTINUOUS and also
//       CmdZoomFIT_PAGE - CmdZoomCUSTOM must be in a continuous range!
static_assert(CmdViewLayoutLast - CmdViewLayoutFirst == 4, "view layout ids are not in a continuous range");
static_assert(CmdZoomLast - CmdZoomFirst == 19, "zoom ids are not in a continuous range");

// clang-format off
//[ ACCESSKEY_GROUP File Menu
static MenuDef menuDefFile[] = {
    {
        TrN("New &window"),
        CmdNewWindow,
    },
    {
        TrN("&Open..."),
        CmdOpenFile,
    },
    {
        TrN("Use SumatraPDF File Picker"),
        CmdToggleFilePicker,
    },
    {
        TrN("&Close"),
        CmdClose,
    },
    {
        TrN("Show in &folder"),
        CmdShowInFolder,
    },
    {
        TrN("Open Next File In Folder"),
        CmdOpenNextFileInFolder,
    },
    {
        TrN("Open Previous File In Folder"),
        CmdOpenPrevFileInFolder,
    },
    {
        TrN("&Save As..."),
        CmdSaveAs,
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
        TrN("Save Annotations to existing PDF"),
        CmdSaveAnnotations,
    },
    {
        TrN("Apply Redactions"),
        CmdApplyRedactions,
    },
    {
        TrN("Insert Image..."),
        CmdInsertImage,
    },
    {
        TrN("Sign Document..."),
        CmdSignDocument,
    },
//[ ACCESSKEY_ALTERNATIVE // only one of these two will be shown
#ifdef ENABLE_SAVE_SHORTCUT
    {
        TrN("Save S&hortcut..."),
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
        TrN("Delete and Open Next File"),
        CmdDeleteFileAndOpenNext,
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
        TrN("Open Directory in Directory &Opus"),
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
        CmdOpenWithFoxIt,
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
        TrN("Send by &E-mail..."),
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
        TrN("F&ullscreen"),
        CmdToggleFullscreen,
    },
    {
        TrN("A&utomatically Scroll"),
        CmdToggleAutomaticallyScroll,
    },
    {
        TrN("Reading &Bar"),
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
        TrN("Show &Menu"),
        CmdToggleMenuBar,
    },
    {
        TrN("Show &Toolbar"),
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

static MenuDef menuDefZoomShort[] = {
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
        TrN("&Shrink To Fit"),
        CmdZoomShrinkToFit,
    },
    {
        TrN("Custom &Zoom..."),
        CmdZoomCustom,
    },
    {
        TrN("To &Selection"),
        CmdZoomToSelection,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        {},
        0,
    },
};

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
        TrN("&Shrink To Fit"),
        CmdZoomShrinkToFit,
    },
    {
        TrN("Custom &Zoom..."),
        CmdZoomCustom,
    },
    {
        TrN("To &Selection"),
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
    {
        TrN("Change Language"),
        CmdChangeLanguage,
    },
#if 0
    { TrN("Contribute Translation"),       CmdContributeTranslation },
    { StrL(kMenuSeparator),                       0                  },
#endif
    {
        TrN("Use SumatraPDF File Picker"),
        CmdToggleFilePicker,
    },
    {
        TrN("&Options..."),
        CmdOptions,
    },
    {
        TrN("&Advanced Options..."),
        CmdAdvancedOptions,
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

//[ ACCESSKEY_GROUP Context Menu (Google Lens)
static MenuDef menuDefGoogleLens[] = {
    {
        TrN("Selection As &Image"),
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
        TrN("&Translate With Google"),
        CmdTranslateSelectionWithGoogle,
    },
    {
        TrN("Translate with &DeepL"),
        CmdTranslateSelectionWithDeepL,
    },
    {
        TrN("Translate with &Grok Build"),
        CmdTranslateSelectionWithGrokBuild,
    },
    {
        TrN("Translate with &Claude Code"),
        CmdTranslateSelectionWithClaudeCode,
    },
    {
        TrN("Translate with OpenAI &Codex"),
        CmdTranslateSelectionWithOpenAICodex,
    },
    {
        TrN("Translate with &Antigravity"),
        CmdTranslateSelectionWithAntiGravity,
    },
    {
        TrN("Search With &Google"),
        CmdSearchSelectionWithGoogle,
    },
    {
        TrN("Search With &Bing"),
        CmdSearchSelectionWithBing,
    },
    {
        TrN("Search with &Wikipedia"),
        CmdSearchSelectionWithWikipedia,
    },
    {
        TrN("Search with &Google Scholar"),
        CmdSearchSelectionWithGoogleScholar,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Selection)

//[ ACCESSKEY_GROUP Menu (Selection)
static MenuDef menuDefMainSelection[] = {
    {
        TrN("&Copy To Clipboard"),
        CmdCopySelection,
    },
    {
        TrN("&Translate With Google"),
        CmdTranslateSelectionWithGoogle,
    },
    {
        TrN("Translate with &DeepL"),
        CmdTranslateSelectionWithDeepL,
    },
    {
        TrN("Translate with &Grok Build"),
        CmdTranslateSelectionWithGrokBuild,
    },
    {
        TrN("Translate with &Claude Code"),
        CmdTranslateSelectionWithClaudeCode,
    },
    {
        TrN("Translate with OpenAI &Codex"),
        CmdTranslateSelectionWithOpenAICodex,
    },
    {
        TrN("Translate with &Antigravity"),
        CmdTranslateSelectionWithAntiGravity,
    },
    {
        TrN("&Search With Google"),
        CmdSearchSelectionWithGoogle,
    },
    {
        TrN("Search With &Bing"),
        CmdSearchSelectionWithBing,
    },
    {
        TrN("Search with &Wikipedia"),
        CmdSearchSelectionWithWikipedia,
    },
    {
        TrN("Search with &Google Scholar"),
        CmdSearchSelectionWithGoogleScholar,
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
        CmdReadAloud,
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Read Aloud Menu

//[ ACCESSKEY_GROUP Context Menu (Read Aloud)
static MenuDef menuDefContextReadAloud[] = {
    {
        TrN("Stop Reading"),
        CmdStopReadAloud,
    },
    {
        TrN("Start Reading From Top"),
        CmdReadAloud,
    },
    {
        {},
        0,
    },
};
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
        TrN("Image From &File..."),
        CmdInsertImage,
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

//[ ACCESSKEY_GROUP Context Menu (Document )
static MenuDef menuDefDocumentOperations[] = {
    {
        TrN("P&roperties"),
        CmdProperties,
    },
    {
        TrN("Show PDF Info"),
        CmdPdShowInfo,
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
        TrN("Show in &folder"),
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
        TrN("Copy &Link Address"),
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
        TrN("Show &Toolbar"),
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
static int disableIfDirectoryOrBrokenPDF[] = {
    CmdRenameFile,
    CmdDeleteFile,
    CmdDeleteFileAndOpenNext,
    CmdSendByEmail,
    CmdOpenWithAcrobat,
    CmdOpenWithFoxIt,
    CmdOpenWithPdfXchange,
    CmdShowInFolder, // TODO: why?
};

// translate / search selection commands need selected text to operate on
static UINT_PTR selectionTextCmds[] = {
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

static UINT_PTR menusNoTranslate[] = {
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
// clang-format on

static bool CmdIdInList(UINT_PTR cmdId, UINT_PTR* idsList, int n) {
    for (int i = 0; i < n; i++) {
        UINT_PTR id = idsList[i];
        if (id == cmdId) {
            return true;
        }
    }
    return false;
}

#define cmdIdInList(name) CmdIdInList(cmdId, name, dimof(name))

static void AddFileMenuItem(HMENU menuFile, Str filePath, int index) {
    ReportIf(len(filePath) == 0 || !menuFile);
    if (len(filePath) == 0 || !menuFile) {
        return;
    }

    TempStr menuString = path::GetBaseNameTemp(filePath);
    // shorten very long file names so that menu isn't too wide
    const int kMaxRunes = 70;
    menuString = ShortenStringUtf8InTheMiddleTemp(menuString, kMaxRunes);

    TempStr fileName = MenuToSafeStringTemp(menuString);
    int menuIdx = (index + 1) % 10;
    menuString = fmt("&%d) %s", menuIdx, fileName);
    uint menuId = CmdFileHistoryFirst + index;
    uint flags = MF_BYCOMMAND | MF_ENABLED | MF_STRING;
    InsertMenuW(menuFile, CmdExit, flags, menuId, CWStrTemp(menuString));
}

static void AppendRecentFilesToMenu(HMENU m) {
    if (!CanAccessDisk()) {
        return;
    }

    int i;
    for (i = 0; i < kFileHistoryMaxRecent; i++) {
        FileState* fs = FileHistoryGet(i);
        if (!fs || fs->isMissing) {
            break;
        }
        Str fp = fs->filePath;
        if (len(fp) == 0) {
            // comes from settings file so can be missing due to user modifications
            continue;
        }
        AddFileMenuItem(m, fp, i);
    }

    if (i > 0) {
        InsertMenuW(m, CmdExit, MF_BYCOMMAND | MF_SEPARATOR, 0, nullptr);
    }
}

static void AppendCommandsToMenu(HMENU m, const Vec<CustomCommand*>& cmds, bool isEnabled) {
    for (CustomCommand* cmd : cmds) {
        if (len(cmd->name) == 0) {
            continue;
        }
        TempStr menuString = cmd->name;
        int cmdId = cmd->id;
        menuString = AppendAccelKeyToMenuStringTemp(menuString, cmdId);
        UINT flags = MF_STRING;
        flags |= isEnabled ? MF_ENABLED : MF_DISABLED;
        WCHAR* ws = CWStrTemp(menuString);
        AppendMenuW(m, flags, (UINT_PTR)cmd->id, ws);
    }
}

static void AppendThemesToMenu(HMENU m) {
    Vec<CustomCommand*> cmds;
    GetCommandsWithOrigId(cmds, CmdSetTheme);
    AppendCommandsToMenu(m, cmds, true);
}

static void AppendSelectionHandlersToMenu(HMENU m, bool isEnabled) {
    Vec<CustomCommand*> cmds;
    GetCommandsWithOrigId(cmds, CmdSelectionHandler);
    AppendCommandsToMenu(m, cmds, isEnabled);
}

static void AppendExternalViewersToMenu(HMENU menuFile, Str filePath) {
    if (!CanAccessDisk() || (filePath && !file::Exists(filePath))) {
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
        if (filter && !(filePath && PathMatchFilter(filePath, filter))) {
            continue;
        }
        TempStr name = cmd->name;
        if (str::IsEmptyOrWhiteSpace(cmd->name)) {
            if (str::IsEmptyOrWhiteSpace(name)) {
                StrNode* args = ParseCmdLine(ToWStrTemp(commandLine));
                defer {
                    FreeStrNode(nullptr, args);
                };
                StrNode* arg0 = args;
                for (int i = 0; arg0 && i < 2; i++) {
                    arg0 = arg0->next;
                }
                int nArgs = 0;
                for (StrNode* n = arg0; n; n = n->next) {
                    nArgs++;
                }
                if (nArgs <= 0) {
                    continue;
                }
                name = path::GetBaseNameTemp(arg0->s);
                int dotPos = str::IndexOfChar(name, '.');
                if (dotPos >= 0) {
                    name = str::DupTemp(Str(name.s, dotPos));
                }
            }
        }
        // TempStr menuString = fmt(Tr("Open in %s"), name);
        TempStr menuString = name;
        int cmdId = cmd->id;
        menuString = AppendAccelKeyToMenuStringTemp(menuString, cmdId);
        WCHAR* ws = CWStrTemp(menuString);
        InsertMenuW(menuFile, cmdId, MF_BYCOMMAND | MF_ENABLED | MF_STRING, (UINT_PTR)cmdId, ws);
        if (len(filePath) == 0) {
            MenuSetEnabled(menuFile, cmdId, false);
        }
    }
}

// shows duplicate separator if no external viewers
static void DynamicPartOfFileMenu(HMENU menu, BuildMenuCtx* ctx) {
    AppendRecentFilesToMenu(menu);

    // Suppress menu items that depend on specific software being installed:
    // e-mail client, Adobe Reader, Foxit, PDF-XChange
    // Don't hide items here that won't always be hidden
    // (MenuUpdateStateForWindow() is for that)
    int idFirst = CmdOpenWithKnownExternalViewerFirst + 1;
    int idLast = CmdOpenWithKnownExternalViewerLast;
    for (int cmdId = idFirst; cmdId < idLast; cmdId++) {
        bool remove, disable;
        GetCommandIdState(ctx, cmdId, &remove, &disable);
        if (remove || disable) {
            MenuRemove(menu, cmdId);
        }
    }
}

void RemoveBadMenuSeparators(HMENU menu) {
    int nMenus;
    // remove separator items at the beginning
again1:
    nMenus = GetMenuItemCount(menu);
    if (nMenus == 0) {
        return;
    }
    UINT id = GetMenuItemID(menu, 0);
    if (id == kMenuSeparatorID) {
        RemoveMenu(menu, 0, MF_BYPOSITION);
        goto again1;
    }
    // remove separator items at the end
again2:
    nMenus = GetMenuItemCount(menu);
    if (nMenus == 0) {
        return;
    }
    id = GetMenuItemID(menu, nMenus - 1);
    if (id == kMenuSeparatorID) {
        RemoveMenu(menu, nMenus - 1, MF_BYPOSITION);
        goto again2;
    }
    // remove 2 or more consequitive separator items
again3:
    nMenus = GetMenuItemCount(menu);
    for (int i = 1; i < nMenus; i++) {
        id = GetMenuItemID(menu, i);
        UINT idPrev = GetMenuItemID(menu, i - 1);
        if ((id == idPrev) && (id == kMenuSeparatorID)) {
            RemoveMenu(menu, i, MF_BYPOSITION);
            goto again3;
        }
    }
}

static void MenuSetEnabledForDocumentCommands(HMENU menu, bool hasDocument) {
    for (int cmdId = (int)CmdFirst; cmdId <= (int)CmdLast; cmdId++) {
        if (!CmdWorksWithoutDocument(cmdId)) {
            MenuSetEnabled(menu, cmdId, hasDocument);
        }
    }
}

HMENU BuildMenuFromDef(MenuDef* menuDef, HMENU menu, BuildMenuCtx* ctx) {
    ReportIf(!menu);

    bool isDebugMenu = menuDef == menuDefDebug;
    int i = 0;

    // insert before built-in selection handlers
    if (menuDef == menuDefSelection) {
        AppendSelectionHandlersToMenu(menu, ctx ? ctx->hasSelection : false);
    }

    if (menuDef == menuDefThemes) {
        AppendThemesToMenu(menu);
    }

    bool addExternalViewersNext = false;
    while (true) {
        MenuDef md = menuDef[i];
        if (len(md.title) == 0) { // sentinel
            break;
        }
        i++;

        if (addExternalViewersNext && ctx) {
            // append user external viewers after menu item with CmdOpenWithHtmlHelp
            WindowTab* tab = ctx->tab;
            Str path = tab ? tab->filePath : Str{};
            AppendExternalViewersToMenu(menu, path);
            addExternalViewersNext = false;
            continue;
        }

        int cmdId = (int)md.idOrSubmenu;

        if (cmdId == CmdOpenWithHtmlHelp) {
            addExternalViewersNext = true;
        }

        if (menuDef == menuDefMainSelection && cmdId == CmdTranslateSelectionWithGoogle) {
            AppendSelectionHandlersToMenu(menu, true);
        }

        MenuDef* subMenuDef = (MenuDef*)md.idOrSubmenu;
        // hacky but works: small number is command id, large is submenu (a pointer)
        bool isSubMenu = md.idOrSubmenu > CmdLast + 10000;

        // handle separators before command state checks
        // (separators have idOrSubmenu=0 which would match the 0 sentinel in removal lists)
        if (str::Eq(md.title, StrL(kMenuSeparator))) {
            AppendMenuW(menu, MF_SEPARATOR, kMenuSeparatorID, nullptr);
            continue;
        }

        // Only real commands have a command-state. For submenu entries cmdId is a
        // truncated pointer (garbage), so don't run it through GetCommandIdState:
        // the no-document gate (and negative truncations) would wrongly remove the
        // whole submenu, leaving e.g. an empty menu bar on the home page. Submenu
        // visibility is decided by the explicit checks below and the emptiness
        // check after the submenu is built.
        bool removeMenu = false;
        bool disableMenu = false;
        // a null ctx means "don't auto-gate commands" -- the caller (e.g. the
        // ToC / Favorites context menus) does its own per-item filtering and
        // wants all items present. With an empty ctx, GetCommandIdState's
        // no-document gate would wrongly strip document-dependent commands.
        if (!isSubMenu && ctx) {
            GetCommandIdState(ctx, cmdId, &removeMenu, &disableMenu);
        }
        if (ctx) {
            removeMenu |= !ctx->isCursorOnPage && (subMenuDef == menuDefCreateAnnotUnderCursor);
            // these annotations need text to mark up, so a rectangular
            // selection doesn't count
            removeMenu |=
                (!ctx->hasTextSelection || !ctx->supportsAnnots) && (subMenuDef == menuDefCreateAnnotFromSelection);
            // in the context menu only show translate / search items for a text
            // selection (the menubar variant is live-updated via
            // SetMenuStateForSelection instead)
            removeMenu |= (menuDef == menuDefSelection) && !ctx->hasTextSelection && cmdIdInList(selectionTextCmds);
            bool isRectSel = ctx->hasSelection && !ctx->hasTextSelection;
            if (menuDef == menuDefSelection) {
                removeMenu |= !ctx->hasSelection && cmdId == CmdCopySelection;
                if (!isRectSel) {
                    removeMenu |= cmdId == CmdCopySelectionAsImage || cmdId == CmdSaveSelectionAsImage ||
                                  cmdId == CmdSearchGoogleLens || cmdId == CmdZoomToSelection;
                }
            }
            if (menuDef == menuDefGoogleLens) {
                removeMenu |= cmdId == CmdSearchGoogleLens && !ctx->hasSelection;
                removeMenu |= cmdId == CmdSearchGoogleLensPage && !ctx->isCursorOnPage;
                bool onImage = ctx->cursorOnImage || ctx->engineKind == kindEngineImage;
                removeMenu |= cmdId == CmdSearchGoogleLensImage && !onImage;
            }
        }
        removeMenu |= ((subMenuDef == menuDefDebug) && !ShowDebugMenu());
        if (removeMenu) {
            continue;
        }

        bool noTranslate = isDebugMenu || cmdIdInList(menusNoTranslate);
        noTranslate |= (subMenuDef == menuDefDebug);
        Str title = md.title;
        if (!noTranslate) {
            title = Str(trans::GetTranslation(md.title));
        }

        if (isSubMenu) {
            HMENU subMenu = BuildMenuFromDef(subMenuDef, CreatePopupMenu(), ctx);
            if (GetMenuItemCount(subMenu) == 0) {
                DestroyMenu(subMenu);
                continue;
            }
            UINT flags = MF_POPUP | (disableMenu ? MF_DISABLED : MF_ENABLED);
            if (subMenuDef == menuDefFile) {
                DynamicPartOfFileMenu(subMenu, ctx);
            }
            if (subMenuDef == menuDefReadAloud) {
                SetReadAloudAppSubmenu(subMenu);
            }
            if (subMenuDef == menuDefContextReadAloud) {
                SetReadAloudContextSubmenu(subMenu);
            }
            WCHAR* ws = CWStrTemp(title);
            AppendMenuW(menu, flags, (UINT_PTR)subMenu, ws);
        } else {
            // Ctrl+C / Ctrl+V are bound to CmdCopySelection and
            // CmdPasteClipboardImage, which hand off to these when an
            // annotation is what there is to copy or paste. Show the key the
            // user actually presses rather than nothing.
            int accelCmdId = cmdId;
            if (cmdId == CmdCopyAnnotation) {
                accelCmdId = CmdCopySelection;
            } else if (cmdId == CmdPasteAnnotation) {
                accelCmdId = CmdPasteClipboardImage;
            }
            title = AppendAccelKeyToMenuStringTemp(title, accelCmdId);
            UINT flags = MF_STRING | (disableMenu ? MF_DISABLED : MF_ENABLED);
            WCHAR* ws = CWStrTemp(title);
            AppendMenuW(menu, flags, md.idOrSubmenu, ws);
        }
    }
    RemoveBadMenuSeparators(menu);
    return menu;
}

// clang-format off
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
    { CmdZoomShrinkToFit, kZoomShrinkToFit },
    { CmdZoomActualSize, kZoomActualSize },
};
// clang-format on

static void BuildMenuZoom(HMENU m) {
    auto* prefs = gSettings;
    auto* customZoomLevels = prefs->zoomLevels;
    int n = len(*customZoomLevels);
    if (n <= 0) {
        return;
    }
    MenuEmpty(m);
    TempStr title;
    int cmdId;
    BuildMenuFromDef(menuDefZoomShort, m, nullptr);
    for (int i = 0; i < n; i++) {
        int idx = n - i - 1; // largest first
        float zl = (*customZoomLevels)[idx];
        cmdId = (*prefs->zoomLevelsCmdIds)[idx];
        title = ZoomLevelStr(zl);
        title = AppendAccelKeyToMenuStringTemp(title, cmdId);
        UINT flags = MF_STRING | MF_ENABLED;
        WCHAR* ws = CWStrTemp(title);
        AppendMenuW(m, flags, cmdId, ws);
    }
}

int CmdIdFromVirtualZoom(float virtualZoom) {
    for (auto&& it : gZoomMenuIds) {
        if (virtualZoom == it.zoom) {
            return it.cmdId;
        }
    }
    return CmdZoomCustom;
}

// Custom ZoomLevels menu items use dynamically allocated command ids (not in
// CmdZoomFirst..CmdZoomLast). Map an absolute zoom % to that custom id, or 0.
static int CustomZoomCmdIdFromLevel(float zoomVirtual) {
    auto* prefs = gSettings;
    if (!prefs || !prefs->zoomLevels || !prefs->zoomLevelsCmdIds) {
        return 0;
    }
    int n = len(*prefs->zoomLevels);
    if (n <= 0 || len(*prefs->zoomLevelsCmdIds) != n) {
        return 0;
    }
    // same fuzz as DisplayModel::GetNextZoomStep
    constexpr float kZoomFuzz = 0.01f;
    for (int i = 0; i < n; i++) {
        float zl = (*prefs->zoomLevels)[i];
        if (zoomVirtual + kZoomFuzz >= zl && zoomVirtual - kZoomFuzz <= zl) {
            return (*prefs->zoomLevelsCmdIds)[i];
        }
    }
    return 0;
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

static void ZoomMenuItemCheck(HMENU m, int cmdId, bool canZoom) {
    for (auto&& it : gZoomMenuIds) {
        MenuSetEnabled(m, it.cmdId, canZoom);
    }

    auto* prefs = gSettings;
    Vec<int>* customIds = prefs ? prefs->zoomLevelsCmdIds : nullptr;
    int nCustom = customIds ? len(*customIds) : 0;
    for (int i = 0; i < nCustom; i++) {
        MenuSetEnabled(m, (*customIds)[i], canZoom);
    }

    // Uncheck all fixed zoom menu commands in the radio range
    for (auto&& it : gZoomMenuIds) {
        MenuSetChecked(m, it.cmdId, false);
    }
    // Uncheck all custom ZoomLevels commands (ids are not a contiguous radio range)
    for (int i = 0; i < nCustom; i++) {
        MenuSetChecked(m, (*customIds)[i], false);
    }

    if (!canZoom || cmdId == 0) {
        return;
    }

    // Fixed Fit*/Actual Size / built-in percentage commands
    if (cmdId >= CmdZoomFirst && cmdId <= CmdZoomLast) {
        if (CmdZoom100 == cmdId) {
            cmdId = CmdZoomActualSize;
        }
        CheckMenuRadioItem(m, CmdZoomFirst, CmdZoomLast, cmdId, MF_BYCOMMAND);
        if (CmdZoomActualSize == cmdId) {
            // also mark 100% when present in the default menu
            CheckMenuRadioItem(m, CmdZoom100, CmdZoom100, CmdZoom100, MF_BYCOMMAND);
        }
        return;
    }

    // Custom ZoomLevels percentage entry (issue #5832)
    MenuSetChecked(m, cmdId, true);
    // When at 100%, also mark "Actual Size" if present
    if (nCustom > 0) {
        int id100 = CustomZoomCmdIdFromLevel(100.0f);
        if (id100 != 0 && id100 == cmdId) {
            MenuSetChecked(m, CmdZoomActualSize, true);
        }
    }
}

static void MenuUpdateZoom(MainWindow* win) {
    float zoomVirtual = gSettings->defaultZoomFloat;
    if (win->IsDocLoaded()) {
        zoomVirtual = win->ctrl->GetZoomVirtual();
    }

    int menuId = 0;
    // Prefer custom ZoomLevels command ids when that menu is active (issue #5832).
    // Fit/virtual zooms still use fixed CmdZoomFit* ids.
    if (zoomVirtual > 0) {
        menuId = CustomZoomCmdIdFromLevel(zoomVirtual);
    }
    if (menuId == 0) {
        menuId = CmdIdFromVirtualZoom(zoomVirtual);
    }
    ZoomMenuItemCheck(win->menu, menuId, win->IsDocLoaded());
}

static void MenuUpdatePrintItem(MainWindow* win, HMENU menu, bool disableOnly = false) {
    bool filePrintEnabled = win->IsDocLoaded();
#ifdef DISABLE_DOCUMENT_RESTRICTIONS
    bool filePrintAllowed = true;
#else
    bool filePrintAllowed = !filePrintEnabled || !win->AsFixed() || win->AsFixed()->GetEngine()->AllowsPrinting();
#endif

    for (auto& def : menuDefFile) {
        if (def.idOrSubmenu != CmdPrint) {
            continue;
        }
        TempStr printItem = trans::GetTranslation(def.title);
        if (!filePrintAllowed) {
            printItem = Tr("&Print... (denied)");
        } else {
            printItem = AppendAccelKeyToMenuStringTemp(printItem, CmdPrint);
        }
        if (!filePrintAllowed || !disableOnly) {
            WCHAR* ws = CWStrTemp(printItem);
            ModifyMenuW(menu, CmdPrint, MF_BYCOMMAND | MF_STRING, (UINT_PTR)CmdPrint, ws);
        }
        MenuSetEnabled(menu, CmdPrint, filePrintEnabled && filePrintAllowed);
    }
}

static void RebuildFileMenu(WindowTab* tab, HMENU menu) {
    MenuEmpty(menu);
    auto* ctx = NewBuildMenuCtx(tab, Point{0, 0});
    AutoDelete delCtx(ctx);
    BuildMenuFromDef(menuDefFile, menu, ctx);
    DynamicPartOfFileMenu(menu, ctx);
    RemoveBadMenuSeparators(menu);
}

static bool IsFileCloseMenuEnabled() {
    for (int i = 0; i < len(gWindows); i++) {
        if (gWindows[i]->IsDocLoaded()) {
            return true;
        }
    }
    return false;
}

static void SetMenuStateForSelection(WindowTab* tab, HMENU menu) {
    bool isTextSelected = tab && tab->win && tab->win->showSelection && tab->selectionOnPage;
    for (int i = 0; disableIfNoSelection[i]; i++) {
        MenuSetEnabled(menu, (int)disableIfNoSelection[i], isTextSelected);
    }
    auto* curr = gFirstCustomCommand;
    while (curr) {
        if (curr->origId == CmdSelectionHandler) {
            MenuSetEnabled(menu, curr->id, isTextSelected);
        }
        curr = curr->next;
    }
}

static void MenuUpdateDisplayMode(MainWindow* win) {
    bool enabled = win->IsDocLoaded();
    DisplayMode displayMode = gSettings->defaultDisplayModeEnum;
    if (enabled) {
        displayMode = win->ctrl->GetDisplayMode();
    }

    for (int id = CmdViewLayoutFirst; id <= CmdViewLayoutLast; id++) {
        MenuSetEnabled(win->menu, id, enabled);
    }

    int id = 0;
    if (IsSingle(displayMode)) {
        id = CmdSinglePageView;
    } else if (IsFacing(displayMode)) {
        id = CmdFacingView;
    } else if (IsBookView(displayMode)) {
        id = CmdBookView;
    } else {
        ReportIf(win->ctrl || DisplayMode::Automatic != displayMode);
    }

    CheckMenuRadioItem(win->menu, CmdViewLayoutFirst, CmdViewLayoutLast, id, MF_BYCOMMAND);
    MenuSetChecked(win->menu, CmdToggleContinuousView, IsContinuous(displayMode));
    MenuSetChecked(win->menu, CmdToggleAutomaticallyScroll, ReadingAutoScrollIsOn(win));
    MenuSetChecked(win->menu, CmdToggleReadingBar, ReadingBarIsOn(win));
    MenuSetChecked(win->menu, CmdToggleReadingBarInvert, gSettings && gSettings->readingBar.invert);

    DisplayModel* dm = win->AsFixed();
    if (dm && win->CurrentTab()) {
        bool mangaMode = dm->GetDisplayR2L();
        MenuSetChecked(win->menu, CmdToggleMangaMode, mangaMode);
        MenuSetEnabled(win->menu, CmdToggleMangaMode, true);
        MenuSetChecked(win->menu, CmdToggleUniformPageWidth, dm->GetUniformPageWidth());
        MenuSetEnabled(win->menu, CmdToggleUniformPageWidth, true);
        MenuSetChecked(win->menu, CmdToggleTrimEmptyMargins, dm->GetTrimEmptyMargins());
        MenuSetEnabled(win->menu, CmdToggleTrimEmptyMargins, true);
    }
}

static void MenuUpdateStateForWindow(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();

    bool hasDocument = tab && tab->IsDocLoaded();
    MenuSetEnabledForDocumentCommands(win->menu, hasDocument);

    SetMenuStateForSelection(tab, win->menu);
    MenuSetEnabled(win->menu, CmdClose, IsFileCloseMenuEnabled());

    MenuUpdatePrintItem(win, win->menu);

    bool enabled = win->IsDocLoaded() && tab && tab->ctrl->HasToc();
    MenuSetEnabled(win->menu, CmdToggleBookmarks, enabled);

    bool documentSpecific = win->IsDocLoaded();
    bool checked = documentSpecific ? win->uiState.tocVisible : gSettings->showToc;
    MenuSetChecked(win->menu, CmdToggleBookmarks, checked);

    MenuSetChecked(win->menu, CmdFavoriteToggle, gSettings->showFavorites);
    MenuSetChecked(win->menu, CmdFavoriteShowInTab, FindFavoritesTab(win) != nullptr);
    {
        // checked when mode is not "hide" (show or overlay)
        bool toolbarOn = win->isFullScreen ? FullscreenToolbarModeFromPrefs() != kToolbarHide : !ToolbarModeIsHidden();
        MenuSetChecked(win->menu, CmdToggleToolbar, toolbarOn);
    }
    MenuSetChecked(win->menu, CmdToggleMenuBar, gSettings->showMenubar);
    // CmdChangeScrollbar doesn't need a check mark - it opens a dialog
    MenuUpdateDisplayMode(win);
    MenuUpdateZoom(win);

    if (win->IsDocLoaded() && tab) {
        MenuSetEnabled(win->menu, CmdNavigateBack, tab->ctrl->CanNavigate(-1));
        MenuSetEnabled(win->menu, CmdNavigateForward, tab->ctrl->CanNavigate(1));
    }

    // TODO: is this check too expensive?
    bool fileExists = tab && file::Exists(tab->filePath);

    if (tab && tab->ctrl && !fileExists && dir::Exists(tab->filePath)) {
        for (int id : disableIfDirectoryOrBrokenPDF) {
            MenuSetEnabled(win->menu, id, false);
        }
    } else if (fileExists && CouldBePDFDoc(tab)) {
        for (int id : disableIfDirectoryOrBrokenPDF) {
            MenuSetEnabled(win->menu, id, true);
        }
    }

    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (engine) {
        MenuSetEnabled(win->menu, CmdFindFirst, !engine->IsImageCollection());
    }

    if (win->IsDocLoaded() && !fileExists) {
        MenuSetEnabled(win->menu, CmdRenameFile, false);
        MenuSetEnabled(win->menu, CmdDeleteFile, false);
        MenuSetEnabled(win->menu, CmdDeleteFileAndOpenNext, false);
    }

    CheckMenuRadioItem(win->menu, gFirstSetThemeCmdId, gLastSetThemeCmdId, gCurrSetThemeCmdId, MF_BYCOMMAND);

    MenuSetChecked(win->menu, CmdToggleLinks, gSettings->showLinks);
    MenuSetChecked(win->menu, CmdTogglePageBoxes, win->showPageBoxes);
    MenuSetChecked(win->menu, CmdToggleHighlightFormFields, gSettings->highlightFormFields);
    MenuSetChecked(win->menu, CmdToggleTransparencyGrid, ShowTransparencyGrid());
    MenuSetChecked(win->menu, CmdTogglePageGrid, ShowPageGrid());
    MenuSetChecked(win->menu, CmdToggleImages, ShowImageOutlines());
    MenuSetChecked(win->menu, CmdDebugShowFitContentArea, ShowFitContentArea());
    MenuSetEnabled(win->menu, CmdTabGroupSave, HasOpenedDocuments(win));
    MenuSetChecked(win->menu, CmdToggleFilePicker, gSettings && str::EqI(gSettings->filePicker, StrL("sumatrapdf")));
}

void OnAboutContextMenu(MainWindow* win, int x, int y) {
    if (!HasPermission(Perm::SavePreferences | Perm::DiskAccess) || !SettingsRememberOpenedFiles() ||
        !gSettings->showStartPage) {
        return;
    }

    // Prefer the file under the click; keyboard/context-menu key falls back to
    // the keyboard-selected home entry.
    TempStr path = HomePageFilePathAtTemp(win, x, y);
    bool fromClick = path && path::IsAbsolute(path);
    if (!fromClick) {
        path = str::DupTemp(HomePageSelectedFilePathTemp(win));
    }
    if (len(path) == 0 || !path::IsAbsolute(path)) {
        return;
    }

    // Keep keyboard selection in sync with the right-clicked thumbnail
    if (fromClick) {
        HomePageOnHover(win, x, y);
    }

    FileState* fs = FileHistoryFindByPath(path);
    if (!fs) {
        return;
    }

    BuildMenuCtx ctx;
    ctx.isDocLoaded = true;
    ctx.filePath = path;
    HMENU popup = BuildMenuFromDef(menuDefContextStart, CreatePopupMenu(), &ctx);
    MenuSetChecked(popup, CmdPinSelectedDocument, fs->isPinned);
    // Del is home-page-only (not a global accelerator), so AppendAccelKey won't
    // pick it up — show it next to Remove From History explicitly
    MenuSetText(popup, CmdForgetSelectedDocument, str::JoinTemp(Tr("&Remove From History"), StrL("\tDel")));
    Point pt = HwndMapWindowPoint(win->hwndCanvas, HWND_DESKTOP, {x, y});
    // keyboard menu (no hit under the cursor): place at cursor or near the frame
    if (!fromClick) {
        Point cursor = GetCursorPosition();
        if (!cursor.IsEmpty()) {
            pt = cursor;
        } else {
            Rect rc = HwndWindowRect(win->hwndFrame);
            pt = Point(rc.x + 40, rc.y + 80);
        }
    }
    MarkMenuOwnerDraw(popup);
    INT cmd = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);

    if (CmdOpenSelectedDocument == cmd) {
        LoadArgs args(path, win);
        args.activateExisting = !IsCtrlPressed();
        args.activateExistingInWindow = true;
        LoadDocument(&args);
        return;
    }

    if (CmdShowInFolder == cmd) {
        ShowFileInFolder(win, path);
        return;
    }

    if (CmdDeleteFile == cmd) {
        if (!CanAccessDisk() || gPluginMode) {
            return;
        }
        // own the path: delete closes tabs and rewrites history
        TempStr pathOwned = str::DupTemp(path);
        if (file::Exists(pathOwned)) {
            WindowTab* tab = FindTabByFilePath(pathOwned);
            if (tab) {
                if (!MaybeSaveAnnotations(tab)) {
                    return;
                }
                CloseTab(tab, false);
            }
            DeleteFileFromDiskAndHistory(pathOwned);
        } else {
            // missing file: still drop it from history
            ForgetFileFromFrequentlyRead(win, pathOwned);
            return;
        }
        if (IsMainWindowValidAndNotClosing(win)) {
            win->DeleteToolTip();
            win->RedrawAll(true);
        }
        return;
    }

    if (CmdPinSelectedDocument == cmd) {
        fs->isPinned = !fs->isPinned;
        win->DeleteToolTip();
        win->RedrawAll(true);
        return;
    }

    if (CmdForgetSelectedDocument == cmd) {
        ForgetFileFromFrequentlyRead(win, path);
        return;
    }
}

// removes a file from the Frequently Read list on the home page. Files with
// favorites are only hidden (so the favorites aren't lost). Used by both the
// context menu and the per-thumbnail ✕ button (issue #283).
void ForgetFileFromFrequentlyRead(MainWindow* win, Str filePath) {
    FileState* fs = FileHistoryFindByPath(filePath);
    if (!fs) {
        return;
    }
    TempStr path = str::DupTemp(fs->filePath);
    if (len(*fs->favorites) > 0) {
        // only hide documents with favorites
        FileHistoryMarkFileInexistent(fs->filePath, true);
    } else {
        FileHistoryRemove(fs);
        DeleteFileState(fs);
    }
    DeleteThumbnailForFile(path);
    ScheduleSaveSettings();
    win->DeleteToolTip();
    win->RedrawAll(true);
}

// s could be in format "file://path.pdf#page=1" or "mailto:foo@bar.com"
// We only want the "path.pdf" / "foo@bar.com"
static TempStr CleanupURLForClipbardCopyTemp(Str s) {
    Str slice = s;
    str::TrimPrefix(slice, StrL("file:"));
    str::TrimPrefix(slice, StrL("mailto:"));
    return str::DupTemp(slice);
}

void OnWindowContextMenu(MainWindow* win, int x, int y) {
    DisplayModel* dm = win->AsFixed();
    ReportIf(!dm);
    if (!dm) {
        return;
    }

    Point cursorPos{x, y};
    WindowTab* tab = win->CurrentTab();
    int pageNoUnderEl = 0;
    IPageElement* pageEl = dm->GetElementAtPos(cursorPos, &pageNoUnderEl);

    Str value = {};
    if (pageEl) {
        value = pageEl->GetValue();
    }

    auto* ctx = NewBuildMenuCtx(tab, cursorPos);
    AutoDelete delCtx(ctx);
    HMENU popup = BuildMenuFromDef(menuDefContext, CreatePopupMenu(), ctx);

    // in fullscreen, add "Menu" as first item containing the full menu bar
    bool isFullScreen = win->isFullScreen || win->presentation;
    if (isFullScreen) {
        HMENU menuBarCopy = BuildMenuFromDef(menuDefMenubar, CreatePopupMenu(), ctx);
        WCHAR* menuLabel = CWStrTemp(Tr("Menu"));
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_STRING | MIIM_SUBMENU;
        mii.hSubMenu = menuBarCopy;
        mii.dwTypeData = menuLabel;
        InsertMenuItemW(popup, 0, TRUE, &mii);
    }

    int pageNoUnderCursor = dm->GetPageNoByPoint(cursorPos);
    EngineBase* engine = dm->GetEngine();

    bool onImage = pageEl && pageEl->Is(kindPageElementImage);
    onImage = onImage || (engine && engine->kind == kindEngineImage);
    if (pageNoUnderCursor > 0) {
        TempStr pageItem;
        if (win->ctrl->HasChapters()) {
            Location loc = win->ctrl->LocationFromPageNo(pageNoUnderCursor);
            pageItem = fmt(Tr("Chapter %d Page %d").s, loc.chapter, loc.page);
        } else {
            TempStr pageLabel = win->ctrl->GetPageLabeTemp(pageNoUnderCursor);
            pageItem = fmt(Tr("Page %s").s, pageLabel);
        }
        MenuSetText(popup, CmdSearchGoogleLensPage, pageItem);
    }

    win->contextMenuPt = cursorPos;
    bool isImageDoc = engine && (engine->IsImageCollection() || engine->kind == kindEngineImage ||
                                 engine->kind == kindEngineImageDir || engine->kind == kindEngineComicBooks);
    win->contextMenuPtValid = !isImageDoc && ReadAloudCanReadFromCursor(dm, cursorPos);
    HMENU readAloudCtxMenu = GetReadAloudContextSubmenu();
    // no text to speak on comics / image folders / single images
    if (readAloudCtxMenu && !isImageDoc) {
        RebuildReadAloudMenu(win, readAloudCtxMenu, true, win->contextMenuPtValid);
    }

    if (!pageEl || !pageEl->Is(kindPageElementDest) || !PageDestHasAddress(pageEl->AsLink())) {
        MenuRemove(popup, CmdCopyLinkTarget);
    }
    bool hasCommentToCopy = pageEl && pageEl->Is(kindPageElementComment) && value;
    if (ctx->annotationUnderCursor) {
        hasCommentToCopy = !str::IsEmptyOrWhiteSpace(Contents(ctx->annotationUnderCursor));
    }
    if (!hasCommentToCopy) {
        MenuRemove(popup, CmdCopyComment);
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
        // remove the Image submenu entirely if no items left
        if (!onImage && !isImageEngine) {
            // find and remove the submenu by scanning top-level items
            int n = GetMenuItemCount(popup);
            for (int j = 0; j < n; j++) {
                HMENU sub = GetSubMenu(popup, j);
                if (sub && GetMenuItemCount(sub) == 0) {
                    RemoveMenu(popup, j, MF_BYPOSITION);
                    break;
                }
            }
        }
    }

    if (!engine || !engine->HasErrors()) {
        MenuRemove(popup, CmdShowErrors);
    }

    if (!isFullScreen) {
        MenuRemove(popup, CmdToggleFullscreen);
    }
    SetMenuStateForSelection(tab, popup);

    MenuUpdatePrintItem(win, popup, true);
    MenuSetEnabled(popup, CmdToggleBookmarks, win->ctrl->HasToc());
    MenuSetChecked(popup, CmdToggleBookmarks, win->uiState.tocVisible);

    MenuSetEnabled(popup, CmdFavoriteToggle, HasFavorites());
    MenuSetChecked(popup, CmdFavoriteToggle, gSettings->showFavorites);
    MenuSetEnabled(popup, CmdFavoriteShowInTab, HasFavorites() && SettingsUseTabs());
    MenuSetChecked(popup, CmdFavoriteShowInTab, FindFavoritesTab(win) != nullptr);

    Str filePath = win->ctrl->GetFilePath();
    bool favsSupported = HasPermission(Perm::SavePreferences) && CanAccessDisk();
    if (favsSupported) {
        if (pageNoUnderCursor > 0) {
            bool isBookmarked = IsPageInFavorites(filePath, pageNoUnderCursor, win->ctrl);

            TempStr addText;
            TempStr delText;
            if (win->ctrl->HasChapters()) {
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
                TempStr s = AppendAccelKeyToMenuStringTemp(addText, CmdFavoriteAdd);
                MenuSetText(popup, CmdFavoriteAdd, s);
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
    RemoveBadMenuSeparators(popup);

    // highlight the element under cursor while context menu is open
    if (pageEl && pageNoUnderEl > 0) {
        win->contextMenuHighlightRect = pageEl->GetRect();
        win->contextMenuHighlightPageNo = pageNoUnderEl;
        HwndRepaintNow(win->hwndCanvas);
    }

    Point pt = HwndMapWindowPoint(win->hwndCanvas, HWND_DESKTOP, {x, y});
    MarkMenuOwnerDraw(popup);
    UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    int cmdId = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);

    // TrackPopupMenu runs a nested message loop; during that time the window
    // can be force-closed (e.g. by a plugin host destroying the parent).
    // If that happened, all our cached pointers (win, dm, pageEl, etc.) are dangling.
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }

    // clear the highlight after context menu closes
    if (win->contextMenuHighlightPageNo > 0) {
        win->contextMenuHighlightPageNo = 0;
        HwndRepaintNow(win->hwndCanvas);
    }

    auto* cmd = FindCustomCommand(cmdId);
    if (cmd && cmd->origId == CmdSelectionHandler) {
        HwndSendCommand(win->hwndFrame, cmd->id);
        return;
    }

    // handle in FrameOnCommand() in SumatraPDF.cpp
    if (CommandUsesContextMenuPoint(cmdId)) {
        LPARAM lpArg = MAKELPARAM(x, y);
        HwndSendCommand(win->hwndFrame, cmdId, lpArg);
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
        case CmdConvertImageToPdf: {
            if (!pageEl || !pageEl->Is(kindPageElementImage)) {
                HwndSendCommand(win->hwndFrame, cmdId);
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
            // a standalone image file: load from disk so Save can write the original
            // bytes when the image is not cropped or resized
            if (imgEngine->kind == kindEngineImage && imgEngine->FilePath()) {
                ShowImageEditWindow(win->hwndFrame, m, imgEngine->FilePath(), nullptr, selectPdf);
                return;
            }
            RenderedBitmap* bmp = imgEngine->GetImageForPageElement(pageEl);
            if (!bmp) {
                return;
            }
            TempStr dir = path::GetDirTemp(filePath);
            TempStr base = path::GetBaseNameTemp(filePath);
            TempStr noExt = path::GetPathNoExtTemp(base);
            Str origData = imgEngine->GetImageDataForPageElement(pageEl);
            Str ext = ImageSaveExtFromData(origData);
            if (len(ext) == 0) {
                ext = StrL(".png");
            }
            TempStr destPath = path::JoinTemp(dir, fmt("%s_page_%d%s", noExt, pageNoUnderCursor, ext));
            ShowImageEditWindow(win->hwndFrame, m, destPath, bmp, selectPdf, origData);
            str::Free(origData);
            delete bmp;
            return;
        };

        case CmdCopyLinkTarget: {
            if (len(value) > 0) {
                TempStr tmp = CleanupURLForClipbardCopyTemp(value);
                CopyTextToClipboard(tmp);
            }
            return;
        };
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
                CopyTextToClipboard(comment);
            }
            return;
        }

        case CmdSaveAttachment: {
            if (!pageEl || !pageEl->Is(kindPageElementDest)) {
                return;
            }
            IPageDestination* elDest = pageEl->AsLink();
            PageDestination* pd = (PageDestination*)elDest;
            if (!pd || pd->embedObjNum <= 0) {
                return;
            }
            // attachments are arbitrary binary
            Str data = EngineMupdfLoadAnnotAttachment(engine, pd->embedObjNum);
            if (len(data) == 0) {
                return;
            }
            Str fileName = pd->GetValue2();
            TempStr dir = path::GetDirTemp(filePath);
            fileName = path::GetBaseNameTemp(fileName);
            TempStr dstPath = path::JoinTemp(dir, fileName);
            SaveDataToFile(win->hwndFrame, dstPath, data);
            str::Free(data);
            return;
        }

        case CmdCopyImage: {
            if (pageEl) {
                RenderedBitmap* bmp = dm->GetEngine()->GetImageForPageElement(pageEl);
                if (bmp) {
                    // via the Pixmap, so an image with an alpha channel reaches
                    // the clipboard with its transparency intact (#5844, #5598)
                    Pixmap* px = PixmapFromRenderedBitmap(bmp); // takes ownership of bmp
                    CopyPixmapToClipboard(px, false);
                    FreePixmap(px);
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
    // everything else we forward to FrameOnCommand() in SumatraPDF.cpp
    HwndSendCommand(win->hwndFrame, cmdId);
}

// Commands whose frame handler needs the original canvas click rather than
// the cursor's position after the context menu has closed.
bool CommandUsesContextMenuPoint(int cmdId) {
    if (cmdId == CmdAnnotationHighlightBrush) {
        // a drag-to-paint tool, not a point-placed annotation: dispatch it
        // without a point so it enters brush mode instead of stamping a stroke
        return false;
    }
    if (CmdIdToAnnotationType(cmdId) != AnnotationType::Unknown) {
        return true;
    }
    return cmdId == CmdDeleteAnnotation || cmdId == CmdCreateAnnotImageFromClipboard || cmdId == CmdInsertImage ||
           cmdId == CmdPasteAnnotation || cmdId == CmdCopyAnnotation || cmdId == CmdCutAnnotation;
}

// so that we can do free everything at exit
static Vec<MenuOwnerDrawInfo*> g_menuDrawInfos;

void FreeAllMenuDrawInfos() {
    while (len(g_menuDrawInfos) != 0) {
        // Note: could be faster
        FreeMenuOwnerDrawInfo(g_menuDrawInfos[0]);
    }
}

void FreeMenuOwnerDrawInfo(MenuOwnerDrawInfo* modi) {
    VecRemove(g_menuDrawInfos, modi);
    str::Free(modi->text);
    free(modi);
}

// menu text consists of potentially 2 parts:
// - text of the menu item
// - text for the keyboard shortcut
// They are separated with \t
static TempStr ParseMenuTextTemp(Str sIn, Str* shortcutOut) {
    *shortcutOut = {};
    Str before, after;
    if (!str::CutChar(sIn, '\t', &before, &after)) {
        return sIn;
    }
    *shortcutOut = after;
    return str::DupTemp(before);
}

struct MenuAccelText {
    Str display;
    int underlineOff = -1;
    int underlineLen = 0;
};

// Remove Win32's '&' accelerator markup and remember the first character that
// needs an underline. A doubled ampersand is a literal one.
static MenuAccelText ParseMenuAccelTextTemp(Str s) {
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

static void DrawMenuText(Gfx* gfx, Str text, Rect rc, u32 flags, PlatformFont* font, Color col) {
    MenuAccelText parsed = ParseMenuAccelTextTemp(text);
    gfx->DrawText(parsed.display, rc, flags, font, col);
    if (parsed.underlineOff < 0 || parsed.underlineLen <= 0) {
        return;
    }
    Size full = gfx->MeasureText(parsed.display, font);
    Size before = parsed.underlineOff > 0 ? gfx->MeasureText(Str(parsed.display.s, parsed.underlineOff), font) : Size{};
    Size ch = gfx->MeasureText(Str(parsed.display.s + parsed.underlineOff, parsed.underlineLen), font);
    int textX = (flags & gfxTextRight) ? rc.x + rc.dx - full.dx : rc.x;
    int underlineY = rc.y + full.dy - 1;
    gfx->DrawLine({textX + before.dx, underlineY, ch.dx, 0}, col);
}

void FreeMenuOwnerDrawInfoData(HMENU hmenu) {
    MENUITEMINFOW mii{};
    mii.cbSize = sizeof(MENUITEMINFOW);

    int n = GetMenuItemCount(hmenu);
    for (int i = 0; i < n; i++) {
        mii.fMask = MIIM_DATA | MIIM_FTYPE | MIIM_SUBMENU;
        BOOL ok = GetMenuItemInfoW(hmenu, (uint)i, TRUE /* by position */, &mii);
        ReportIf(!ok);
        auto* modi = (MenuOwnerDrawInfo*)mii.dwItemData;
        if (modi != nullptr) {
            FreeMenuOwnerDrawInfo(modi);
            mii.dwItemData = 0;
            mii.fType &= ~MFT_OWNERDRAW;
            SetMenuItemInfoW(hmenu, (uint)i, TRUE /* by position */, &mii);
        }
        if (mii.hSubMenu != nullptr) {
            FreeMenuOwnerDrawInfoData(mii.hSubMenu);
        }
    };
}
#if 1
void MarkMenuOwnerDraw(HMENU /*hmenu*/, bool /*isMenuBar*/) {
    // our painting isn't good enough so disable for now
    // rely on darkmodelib for menu theming, which only does light / dark theme from os
}
#else
void MarkMenuOwnerDraw(HMENU hmenu, bool isMenuBar) {
    // darkmodelib handles the menu bar via setWindowMenuBarSubclass
    // but doesn't handle popup/context menus, so we owner-draw those
    if (isMenuBar && DarkModeIsActive()) {
        return;
    }
    if (!ThemeColorizeControls()) {
        return;
    }

    // https://stackoverflow.com/questions/30353644/cmenu-border-color-on-mfc
    static HBRUSH hbrBrush = nullptr;
    static Color bgCol = (Color)-1;
    Color col = ThemeMainWindowBackgroundColor();
    if (!hbrBrush) {
        bgCol = col;
        hbrBrush = ::CreateSolidBrush(col);
    } else {
        if (col != bgCol) {
            // in case theme changed
            DeleteBrush(hbrBrush);
            bgCol = col;
            hbrBrush = ::CreateSolidBrush(col);
        }
    }

    MENUINFO mi{};
    mi.cbSize = sizeof(MENUINFO);
    GetMenuInfo(hmenu, &mi);
    mi.hbrBack = hbrBrush;
    mi.fMask = MIM_BACKGROUND | MIM_STYLE | MIM_APPLYTOSUBMENUS;
    SetMenuInfo(hmenu, &mi);

    WCHAR buf[1024];

    MENUITEMINFOW mii{};
    mii.cbSize = sizeof(MENUITEMINFOW);

    int n = GetMenuItemCount(hmenu);

    for (int i = 0; i < n; i++) {
        buf[0] = 0;
        mii.fMask = MIIM_BITMAP | MIIM_CHECKMARKS | MIIM_DATA | MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU | MIIM_STRING;
        mii.dwTypeData = &(buf[0]);
        mii.cch = dimof(buf);
        BOOL ok = GetMenuItemInfoW(hmenu, (uint)i, TRUE /* by position */, &mii);
        ReportIf(!ok);
        mii.fMask = MIIM_FTYPE | MIIM_DATA;
        mii.fType |= MFT_OWNERDRAW;
        if (mii.dwItemData != 0) {
            auto modi = (MenuOwnerDrawInfo*)mii.dwItemData;
            FreeMenuOwnerDrawInfo(modi);
        }
        auto modi = AllocStruct<MenuOwnerDrawInfo>();
        g_menuDrawInfos.Append(modi);
        modi->fState = mii.fState;
        modi->fType = mii.fType;
        modi->hbmpItem = mii.hbmpItem;
        modi->hbmpChecked = mii.hbmpChecked;
        modi->hbmpUnchecked = mii.hbmpUnchecked;
        if (len(buf) > 0) {
            modi->text = ToUtf8(buf);
        }
        mii.dwItemData = (ULONG_PTR)modi;
        SetMenuItemInfoW(hmenu, (uint)i, TRUE /* by position */, &mii);

        if (mii.hSubMenu != nullptr) {
            MarkMenuOwnerDraw(mii.hSubMenu);
        }
    }
}
#endif

static int GetMenuCheckMarkCx(HWND hwnd) {
    DpiSetFromHwnd(hwnd);
    // GetSystemMetrics() already answers in pixels for the system dpi, so the
    // old DpiScale(GetSystemMetrics(...)) scaled it a second time
    int cx = DpiGetSystemMetrics(SM_CXMENUCHECK);
    if (!IsMenuFontSizeDefault()) {
        cx = GetAppMenuFontSize();
        // this applies scaling for default values on my win 11 i.e.:
        // font size is 12, menu checkmark is 15
        cx = (cx * 15) / 12;
        cx = DpiScale(cx);
    }
    return cx;
}

constexpr int kMenuPaddingY = 4;
constexpr int kMenuPaddingX = 8;

void MenuCustomDrawMesureItem(HWND hwnd, MEASUREITEMSTRUCT* mis) {
    DpiSetFromHwnd(hwnd);
    if (ODT_MENU != mis->CtlType) {
        return;
    }
    auto* modi = (MenuOwnerDrawInfo*)mis->itemData;

    bool isSeparator = bit::IsMaskSet(modi->fType, (uint)MFT_SEPARATOR);
    if (isSeparator) {
        mis->itemHeight = DpiScale(7);
        mis->itemWidth = DpiScale(33);
        return;
    }

    Str text = modi && modi->text ? modi->text : StrL("Dummy");
    PlatformFont* font = GetAppMenuFont();
    Str shortcutText = {};
    TempStr menuText = ParseMenuTextTemp(text, &shortcutText);
    MenuAccelText parsed = ParseMenuAccelTextTemp(menuText);

    auto size = PlatformFontMeasureText(font, parsed.display);
    mis->itemHeight = size.dy;
    int dx = size.dx;
    if (shortcutText) {
        // add space betweeen menu text and shortcut
        size = PlatformFontMeasureText(font, StrL("    "));
        dx += size.dx;
        size = PlatformFontMeasureText(font, shortcutText);
        dx += size.dx;
    }
    auto padX = DpiScale(kMenuPaddingX);
    auto padY = DpiScale(kMenuPaddingY);

    int cxMenuCheckMark = GetMenuCheckMarkCx(hwnd);
    mis->itemHeight += padY * 2;
    mis->itemWidth = uint(dx + cxMenuCheckMark + (padX * 2));
}

// https://gist.github.com/kjk/1df108aa126b7d8e298a5092550a53b7
// TODO: improve how we paint the menu:
// - position text the right way (not just DT_CENTER)
//   taking into account LTR mode
// - paint shortcut (part after \t if exists) separately
// - paint MFS_DISABLED state
// - paint icons for system menus
void MenuCustomDrawItem(HWND hwnd, DRAWITEMSTRUCT* dis) {
    DpiSetFromHwnd(hwnd);
    if (ODT_MENU != dis->CtlType) {
        return;
    }
    auto* modi = (MenuOwnerDrawInfo*)dis->itemData;
    if (!modi) {
        return;
    }

    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms647578(v=vs.85).aspx

    // low-order word of the dwTypeData member is the bitmap handle
    // HBITMAP bmp = (HBITMAP)LOWORD(modi->dwTypeData) ?
    // bool isBitmap = bit::IsMaskSet(modi->fType, (uint)MFT_BITMAP);

    // ???
    // bool isMenuBarBreak = bit::IsMaskSet(modi->fType, (uint)MFT_MENUBARBREAK);

    // ???
    // bool isMenuBreak = bit::IsMaskSet(modi->fType, (uint)MFT_MENUBREAK);

    bool isSeparator = bit::IsMaskSet(modi->fType, (uint)MFT_SEPARATOR);

    // default should be drawn in bold
    // bool isDefault = bit::IsMaskSet(modi->fState, (uint)MFS_DEFAULT);

    // disabled should be drawn grayed out
    bool isDisabled = bit::IsMaskSet(modi->fState, (uint)MFS_DISABLED);

    // don't know what that means
    // bool isHilited = bit::IsMaskSet(modi->fState, (uint)MFS_HILITE);

    // checked/unchecked state for check and radio menus
    bool isChecked = bit::IsMaskSet(modi->fState, (uint)MFS_CHECKED);

    // if isChecked, show as radio button (i.e. circle)
    bool isRadioCheck = bit::IsMaskSet(modi->fType, (uint)MFT_RADIOCHECK);

    PlatformFont* font = GetAppMenuFont();
    Gfx* gfx = GfxCreate(dis->hDC);
    defer {
        delete gfx;
    };

    Color bgCol = ThemeMainWindowBackgroundColor();
    Color txtCol = ThemeWindowTextColor();

    bool isSelected = bit::IsMaskSet(dis->itemState, (uint)ODS_SELECTED);
    if (isDisabled) {
        txtCol = ThemeWindowTextDisabledColor();
        if (isSelected) {
            // subtle highlight for disabled selected items
            bgCol = AccentColor(bgCol, 10);
        }
    } else if (isSelected) {
        bgCol = AccentColor(bgCol, 40);
    }

    Rect rc = ToRect(dis->rcItem);
    int rcDy = rc.dy;

    int cxCheckMark = GetMenuCheckMarkCx(hwnd);
    int padY = DpiScale(kMenuPaddingY);
    int padX = DpiScale(kMenuPaddingX);

    gfx->FillRect(rc, bgCol);

    if (isSeparator) {
        ReportIf(len(modi->text) != 0);
        int sx = rc.x + cxCheckMark;
        int y = rc.y + (rcDy / 2);
        int ex = rc.x + rc.dx - padX;
        gfx->DrawLine({sx, y, ex - sx, 0}, txtCol);
        return;
    }

    // TODO: probably could be a bitmap etc.
    if (len(modi->text) == 0) {
        return;
    }

    Str shortcutText = {};
    TempStr menuText = ParseMenuTextTemp(modi->text, &shortcutText);

    rc.y += padY;
    rc.dy -= padY;
    rc.x += cxCheckMark;
    rc.dx -= cxCheckMark;
    DrawMenuText(gfx, menuText, rc, gfxTextSingleLine, font, txtCol);
    if (shortcutText) {
        rc = ToRect(dis->rcItem);
        rc.y += padY;
        rc.dy -= padY;
        rc.dx -= padX + (cxCheckMark / 2);
        gfx->DrawText(shortcutText, rc, gfxTextSingleLine | gfxTextRight, font, txtCol);
    }

    constexpr int kRadioCircleDx = 6;
    if (isChecked) {
        rc = ToRect(dis->rcItem);
        // draw radio check indicator (a circle)
        if (isRadioCheck) {
            int dx = DpiScale(kRadioCircleDx);
            int offX = DpiScale(1); // why? beause it looks better
            rc.x = rc.x + offX + (cxCheckMark / 2) - (dx / 2);
            rc.dx = dx;
            rc.y = rc.y + (rcDy / 2) - (dx / 2);
            rc.dy = dx;
            gfx->FillEllipse(rc, txtCol);
            return;
        }

        // draw a checkmark
        int offX = DpiScale(6); // 6 is chosen experimentally
        Point p0 = {rc.x + offX, rc.y + (rcDy / 2)};
        Point p1 = {rc.x + (cxCheckMark / 2), rc.y + rc.dy - (padY * 3)};
        Point p2 = {rc.x + cxCheckMark - offX, rc.y + (padY * 3)};
        gfx->DrawLineAA(p0, p1, txtCol, 2);
        gfx->DrawLineAA(p1, p2, txtCol, 2);
    }
}

HMENU BuildMenu(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();

    auto* ctx = NewBuildMenuCtx(tab, Point{0, 0});
    AutoDelete delCtx(ctx);
    HMENU mainMenu = BuildMenuFromDef(menuDefMenubar, CreateMenu(), ctx);
    // BuildMenuFromDef just set the global to this window's submenu
    win->menuReadAloud = GetReadAloudAppSubmenu();

    MarkMenuOwnerDraw(mainMenu, true);
    return mainMenu;
}

void UpdateAppMenu(MainWindow* win, HMENU m) {
    ReportIf(!win);
    if (!win) {
        return;
    }
    UINT_PTR id = (UINT_PTR)GetMenuItemID(m, 0);
    if (id == menuDefFile[0].idOrSubmenu) {
        RebuildFileMenu(win->CurrentTab(), m);
    } else if (id == menuDefFavorites[0].idOrSubmenu) {
        MenuEmpty(m);
        // build with a real ctx (not nullptr): command-visibility now hides
        // document-dependent commands when no document is loaded, and a null ctx
        // looks like "no document" -- which dropped CmdFavoriteAdd/CmdFavoriteDel
        // from the rebuilt menu, so RebuildFavMenu's MenuSetText then failed
        auto* ctx = NewBuildMenuCtx(win->CurrentTab(), Point{0, 0});
        AutoDelete delCtx(ctx);
        BuildMenuFromDef(menuDefFavorites, m, ctx);
        RebuildFavMenu(win, m);
    } else if (id == menuDefZoom[0].idOrSubmenu) {
        BuildMenuZoom(m);
    } else if (m && m == win->menuReadAloud) {
        RebuildReadAloudMenu(win, m, false, false);
    } else if (IsReadAloudContextSubmenu(m)) {
        RebuildReadAloudMenu(win, m, true, win->contextMenuPtValid);
    }
    MenuUpdateStateForWindow(win);
    MarkMenuOwnerDraw(win->menu, true);
}

// show/hide top-level menu bar. This doesn't persist across launches
// so that accidental removal of the menu isn't catastrophic
void ToggleMenuBar(MainWindow* win, bool showTemporarily) {
    ReportIf(!win->menu);

    if (win->presentation) {
        return;
    }

    HWND hwnd = win->hwndFrame;

    if (showTemporarily) {
        if (win->tabsInTitlebar) {
            // can't show regular menu with custom caption, so do nothing
            return;
        }
        SetMenu(hwnd, win->menu);
        return;
    }

    if (win->isFullScreen) {
        gSettings->fullscreen.showMenubar = !gSettings->fullscreen.showMenubar;
        if (gSettings->fullscreen.showMenubar) {
            // use rebar-based menu bar (WS_CAPTION is stripped in fullscreen, so SetMenu won't work)
            CreateMenuBarRebar(win);
        } else {
            DestroyMenuBarRebar(win);
        }
        ScheduleUiUpdate(win);
        ShowMenuBarRebar(win);
        return;
    }

    if (win->tabsInTitlebar) {
        // toggle rebar menu bar while keeping tabs in titlebar
        bool isShowing = IsShowingMenuBarRebar(win);
        if (isShowing) {
            DestroyMenuBarRebar(win);
            gSettings->showMenubar = false;
            gSettings->showMenubarWithTabs = false;
        } else {
            CreateMenuBarRebar(win);
            gSettings->showMenubar = true;
            gSettings->showMenubarWithTabs = true;
        }
        // layout first so the rebar is positioned correctly, then show it
        ScheduleUiUpdate(win);
        ShowMenuBarRebar(win);
        return;
    }

    bool hideMenu = GetMenu(hwnd) != nullptr;
    SetMenu(hwnd, hideMenu ? nullptr : win->menu);
    gSettings->showMenubar = !hideMenu;
    gSettings->showMenubarWithTabs = !hideMenu;
}

// --- Menu bar as rebar control (used when tabs are in titlebar) ---

static int MenuBarToolbarIdealDy() {
    PlatformFont* font = GetAppMenuFont();
    int dy = PlatformFontLineHeight(font) + DpiScale(4);
    int minDy = DpiScale(kTabBarDy);
    return std::max(dy, minDy);
}

int GetMenuBarRebarHeight(MainWindow* win) {
    HWND hwnd = win ? win->hwndMenuReBar : nullptr;
    if (!hwnd || !::IsWindow(hwnd)) {
        return 0;
    }
    // RB_GETBARHEIGHT underreports by 1px without WS_BORDER
    int dy = (int)SendMessageW(hwnd, RB_GETBARHEIGHT, 0, 0) + 1;
    if (dy > 1) {
        if (IsRunningOnWine()) {
            logf("GetMenuBarRebarHeight: rebar=%p RB_GETBARHEIGHT=%d\n", win->hwndMenuReBar, dy);
        }
        return dy;
    }
    int ideal = MenuBarToolbarIdealDy();
    if (IsRunningOnWine()) {
        logf("GetMenuBarRebarHeight: rebar=%p RB_GETBARHEIGHT=%d fallbackIdeal=%d\n", win->hwndMenuReBar, dy, ideal);
    }
    return ideal;
}

static LRESULT CALLBACK MenuBarReBarWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass,
                                            DWORD_PTR /*dwRefData*/) {
    if (WM_ERASEBKGND == uMsg) {
        // always paint background with theme color to avoid gray strips in light theme
        HDC hdc = (HDC)wParam;
        Color bgCol = ThemeControlBackgroundColor();
        auto* bgBrush = CreateSolidBrush(bgCol);
        HdcFillRect(hdc, HwndClientRect(hWnd), bgBrush);
        DeleteObject(bgBrush);
        return 1;
    }
    if (WM_NOTIFY == uMsg) {
        auto* win = FindMainWindowByHwnd(hWnd);
        NMHDR* hdr = (NMHDR*)lParam;
        if (win && hdr->code == NM_CUSTOMDRAW && hdr->hwndFrom == win->hwndMenuToolbar) {
            NMTBCUSTOMDRAW* custDraw = (NMTBCUSTOMDRAW*)hdr;
            switch (custDraw->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT: {
                    auto col = ThemeWindowTextColor();
                    UINT itemState = custDraw->nmcd.uItemState;
                    if (itemState & CDIS_DISABLED) {
                        col = ThemeWindowTextDisabledColor();
                    }
                    custDraw->clrText = col;
                    return CDRF_DODEFAULT;
                }
            }
        }
    }
    if (WM_NCDESTROY == uMsg) {
        RemoveWindowSubclass(hWnd, MenuBarReBarWndProc, uIdSubclass);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK MenuBarToolbarWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass,
                                              DWORD_PTR /*dwRefData*/) {
    if (WM_ERASEBKGND == uMsg) {
        // don't erase background here; toolbar paints its own background during WM_PAINT
        // filling here causes visible flicker (erase then paint) during window resize
        return 1;
    }
    if (WM_NCDESTROY == uMsg) {
        RemoveWindowSubclass(hWnd, MenuBarToolbarWndProc, uIdSubclass);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

constexpr int kMenuBarCmdFirst = 50000;
constexpr int kMenuBarCmdLast = 50020;

struct MenuBarPopupNav {
    MainWindow* win = nullptr;
    HMENU rootMenu = nullptr;
    HMENU currentMenu = nullptr;
    UINT currentFlags = 0;
    int nextMenuIdx = -1;
};

static MenuBarPopupNav gMenuBarPopupNav;

// track when a menu popup was last dismissed so a second click on the same
// menu bar button closes the popup instead of immediately reopening it
static int gMenuBarLastDismissedIdx = -1;
static u64 gMenuBarLastDismissedTick = 0;

static bool ShouldSwitchCustomMenuBarPopup(UINT vk) {
    if (!gMenuBarPopupNav.win || !gMenuBarPopupNav.rootMenu) {
        return false;
    }
    if (!gMenuBarPopupNav.currentMenu || gMenuBarPopupNav.currentMenu != gMenuBarPopupNav.rootMenu) {
        return false;
    }
    if (bit::IsMaskSet(gMenuBarPopupNav.currentFlags, (UINT)MF_POPUP)) {
        return false;
    }

    int menuCount = GetMenuItemCount(gMenuBarPopupNav.win->menu);
    if (menuCount <= 1) {
        return false;
    }

    int step = 0;
    if (vk == VK_LEFT) {
        step = -1;
    } else if (vk == VK_RIGHT) {
        step = 1;
    }
    if (step == 0) {
        return false;
    }

    gMenuBarPopupNav.nextMenuIdx += step;
    if (gMenuBarPopupNav.nextMenuIdx < 0) {
        gMenuBarPopupNav.nextMenuIdx = menuCount - 1;
    } else if (gMenuBarPopupNav.nextMenuIdx >= menuCount) {
        gMenuBarPopupNav.nextMenuIdx = 0;
    }
    return true;
}

// check if mouse is over a different toolbar button and switch to it
static bool ShouldSwitchMenuBarOnMouseMove() {
    if (!gMenuBarPopupNav.win || !gMenuBarPopupNav.win->hwndMenuToolbar) {
        return false;
    }
    HWND hwndTb = gMenuBarPopupNav.win->hwndMenuToolbar;

    Point pt = HwndGetCursorPos(hwndTb);

    // hit-test the toolbar
    int btnCount = TbGetButtonCount(hwndTb);
    for (int i = 0; i < btnCount; i++) {
        Rect rc = TbGetItemRect(hwndTb, i);
        if (rc.Contains(pt.x, pt.y)) {
            TBBUTTON tb{};
            SendMessageW(hwndTb, TB_GETBUTTON, i, (LPARAM)&tb);
            int menuIdx = tb.idCommand - kMenuBarCmdFirst;
            if (menuIdx != gMenuBarPopupNav.nextMenuIdx) {
                gMenuBarPopupNav.nextMenuIdx = menuIdx;
                return true;
            }
            return false;
        }
    }
    return false;
}

static LRESULT CALLBACK MenuBarMsgFilterHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code == MSGF_MENU && gMenuBarPopupNav.win) {
        MSG* msg = (MSG*)lParam;
        if ((msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) &&
            ShouldSwitchCustomMenuBarPopup((UINT)msg->wParam)) {
            EndMenu();
            return 1;
        }
        if (msg->message == WM_MOUSEMOVE && ShouldSwitchMenuBarOnMouseMove()) {
            EndMenu();
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void UpdateCustomMenuBarMenuSelect(MainWindow* win, WPARAM wp, LPARAM lp) {
    if (gMenuBarPopupNav.win != win) {
        return;
    }

    UINT flags = HIWORD(wp);
    HMENU menu = (HMENU)lp;
    if (flags == 0xFFFF && !menu) {
        gMenuBarPopupNav.currentMenu = nullptr;
        gMenuBarPopupNav.currentFlags = 0;
        return;
    }

    gMenuBarPopupNav.currentMenu = menu;
    gMenuBarPopupNav.currentFlags = flags;
}

void RebuildMenuBarButtons(MainWindow* win) {
    HWND hwndMb = win->hwndMenuToolbar;
    if (!hwndMb) {
        return;
    }

    // remove existing buttons
    while (SendMessageW(hwndMb, TB_DELETEBUTTON, 0, 0)) {
    }

    HMENU menu = win->menu;
    int count = GetMenuItemCount(menu);
    if (count <= 0) {
        return;
    }

    MENUITEMINFOW mii{};
    mii.cbSize = sizeof(MENUITEMINFOW);
    mii.fMask = MIIM_SUBMENU | MIIM_STRING;

    for (int i = 0; i < count && i < (kMenuBarCmdLast - kMenuBarCmdFirst); i++) {
        mii.dwTypeData = nullptr;
        mii.cch = 0;
        GetMenuItemInfoW(menu, i, TRUE, &mii);
        if (!mii.hSubMenu || !mii.cch) {
            continue;
        }
        mii.cch++;
        WCHAR* name = AllocArrayTemp<WCHAR>((int)mii.cch);
        mii.dwTypeData = name;
        GetMenuItemInfoW(menu, i, TRUE, &mii);

        TBBUTTON b{};
        b.iBitmap = I_IMAGENONE;
        b.idCommand = kMenuBarCmdFirst + i;
        b.fsState = TBSTATE_ENABLED;
        b.fsStyle = BTNS_AUTOSIZE | BTNS_SHOWTEXT;
        b.iString = (INT_PTR)name;
        TbAddButtons(hwndMb, 1, &b);
    }

    TbAutoSize(hwndMb);

    if (win->hwndMenuReBar) {
        Rect rc = TbGetItemRect(hwndMb, 0);
        int menuBarDy = MenuBarToolbarIdealDy();
        if (rc.dy > 0) {
            menuBarDy = rc.dy + (2 * rc.y);
        }
        REBARBANDINFOW rbBand{};
        rbBand.cbSize = sizeof(REBARBANDINFOW);
        rbBand.fMask = RBBIM_CHILDSIZE;
        rbBand.cyChild = menuBarDy;
        rbBand.cyMinChild = menuBarDy;
        SendMessageW(win->hwndMenuReBar, RB_SETBANDINFO, 0, (LPARAM)&rbBand);
    }
}

void CreateMenuBarRebar(MainWindow* win) {
    if (!win || win->hwndMenuReBar) {
        return;
    }
    // embedded hosts (TC lister) must not get titlebar menu rebar chrome
    if (gMyWindowWasEmbedded) {
        return;
    }
    HWND hwndParent = win->hwndFrame;
    if (!hwndParent || !::IsWindow(hwndParent)) {
        return;
    }

    bool isRtl = IsUIRtl();
    HINSTANCE hinst = GetModuleHandle(nullptr);

    // create hidden; caller shows after the scheduled relayout positions it
    // no WS_BORDER (avoids 1px gap) and no RBS_BANDBORDERS (avoids gray band separators)
    DWORD style = WS_CHILD | WS_CLIPCHILDREN | RBS_VARHEIGHT;
    style |= CCS_NODIVIDER | CCS_NOPARENTALIGN;
    DWORD exStyle = WS_EX_TOOLWINDOW;
    if (isRtl) {
        exStyle |= WS_EX_LAYOUTRTL;
    }

    win->hwndMenuReBar = CreateWindowExW(exStyle, REBARCLASSNAME, nullptr, style, 0, 0, 0, 0, hwndParent,
                                         (HMENU)IDC_MENUBAR_REBAR, hinst, nullptr);
    SetWindowSubclass(win->hwndMenuReBar, MenuBarReBarWndProc, 0, 0);

    REBARINFO rbi{};
    rbi.cbSize = sizeof(REBARINFO);
    SendMessageW(win->hwndMenuReBar, RB_SETBARINFO, 0, (LPARAM)&rbi);
    SendMessageW(win->hwndMenuReBar, RB_SETBKCOLOR, 0, ThemeControlBackgroundColor());

    style = WS_CHILD | WS_CLIPSIBLINGS | TBSTYLE_FLAT | TBSTYLE_LIST;
    style |= CCS_NODIVIDER | CCS_NOPARENTALIGN;
    exStyle = 0;
    if (isRtl) {
        exStyle |= WS_EX_LAYOUTRTL;
    }

    win->hwndMenuToolbar = CreateWindowExW(exStyle, TOOLBARCLASSNAME, nullptr, style, 0, 0, 0, 0, win->hwndMenuReBar,
                                           (HMENU)IDC_MENUBAR, hinst, nullptr);
    SetWindowSubclass(win->hwndMenuToolbar, MenuBarToolbarWndProc, 0, 0);
    TbSetButtonStructSize(win->hwndMenuToolbar, sizeofi(TBBUTTON));

    if (!DarkModeIsActive()) {
        if (!IsCurrentThemeDefault()) {
            SetWindowTheme(win->hwndMenuToolbar, L"", L"");
        }
    }

    PlatformFont* font = GetAppMenuFont();
    HwndSetFont(win->hwndMenuToolbar, font->GetHFont());

    DWORD tbExStyle = TbGetExtendedStyle(win->hwndMenuToolbar);
    tbExStyle |= TBSTYLE_EX_MIXEDBUTTONS;
    TbSetExtendedStyle(win->hwndMenuToolbar, tbExStyle);

    RebuildMenuBarButtons(win);

    Rect rc = TbGetItemRect(win->hwndMenuToolbar, 0);
    int menuBarDy = rc.dy + (2 * rc.y);
    if (menuBarDy <= 0) {
        menuBarDy = MenuBarToolbarIdealDy();
    }

    ShowWindow(win->hwndMenuToolbar, SW_SHOW);

    REBARBANDINFOW rbBand{};
    rbBand.cbSize = sizeof(REBARBANDINFOW);
    rbBand.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE;
    rbBand.fStyle = RBBS_FIXEDSIZE;
    rbBand.hwndChild = win->hwndMenuToolbar;
    rbBand.cxMinChild = 0;
    rbBand.cyMinChild = menuBarDy;
    rbBand.cx = 0;
    SendMessageW(win->hwndMenuReBar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

    DarkModeApplyToMenuBar(win->hwndMenuReBar);
}

void ShowMenuBarRebar(MainWindow* win) {
    HWND hwnd = win ? win->hwndMenuReBar : nullptr;
    if (hwnd && ::IsWindow(hwnd)) {
        ShowWindow(hwnd, SW_SHOW);
    }
}

void DestroyMenuBarRebar(MainWindow* win) {
    if (!win) {
        return;
    }
    // clear fields first so re-entrant layout/paint cannot SetWindowPos them
    HWND hwndTb = win->hwndMenuToolbar;
    HWND hwndRb = win->hwndMenuReBar;
    win->hwndMenuToolbar = nullptr;
    win->hwndMenuReBar = nullptr;
    // hide before destroy so nested paint is less likely to walk half-torn
    // rebar/toolbar scroll-arrow state (comctl32!DrawScrollBar AV)
    if (hwndRb && ::IsWindow(hwndRb)) {
        ShowWindow(hwndRb, SW_HIDE);
    }
    if (hwndTb && ::IsWindow(hwndTb)) {
        ShowWindow(hwndTb, SW_HIDE);
        DestroyWindow(hwndTb);
    }
    if (hwndRb && ::IsWindow(hwndRb)) {
        DestroyWindow(hwndRb);
    }
}

bool IsShowingMenuBarRebar(MainWindow* win) {
    if (!win) {
        return false;
    }
    HWND hwnd = win->hwndMenuReBar;
    if (!hwnd || !::IsWindow(hwnd)) {
        return false;
    }
    // host reparented us as WS_CHILD: menu rebar is being (or about to be)
    // torn down; treat as not showing so layout does not SetWindowPos it
    if (gMyWindowWasEmbedded) {
        return false;
    }
    if (win->presentation || win->isQuickLook) {
        return false;
    }
    return true;
}

bool HandleMenuBarCommand(MainWindow* win, int cmdId) {
    if (cmdId < kMenuBarCmdFirst || cmdId >= kMenuBarCmdLast) {
        return false;
    }
    if (!win->hwndMenuToolbar) {
        return false;
    }

    int menuCount = GetMenuItemCount(win->menu);
    int menuIdx = cmdId - kMenuBarCmdFirst;

    // if same button was clicked shortly after dismissing its popup, treat as toggle-close
    u64 now = GetTickCount64();
    if (menuIdx == gMenuBarLastDismissedIdx && (now - gMenuBarLastDismissedTick) < 500) {
        gMenuBarLastDismissedIdx = -1;
        return true;
    }

    UINT flags = TPM_LEFTALIGN | TPM_TOPALIGN;
    if (IsUIRtl()) {
        flags = TPM_RIGHTALIGN | TPM_TOPALIGN;
    }

    for (;;) {
        HMENU subMenu = GetSubMenu(win->menu, menuIdx);
        if (!subMenu) {
            return true;
        }

        // get button rect in screen coordinates
        int btnCmdId = kMenuBarCmdFirst + menuIdx;
        int btnIdx = (int)SendMessageW(win->hwndMenuToolbar, TB_COMMANDTOINDEX, btnCmdId, 0);
        Rect btnRect = TbGetItemRect(win->hwndMenuToolbar, btnIdx);
        btnRect = HwndMapRectToWindow(btnRect, win->hwndMenuToolbar, HWND_DESKTOP);

        gMenuBarPopupNav.win = win;
        gMenuBarPopupNav.rootMenu = subMenu;
        gMenuBarPopupNav.currentMenu = subMenu;
        gMenuBarPopupNav.currentFlags = 0;
        gMenuBarPopupNav.nextMenuIdx = menuIdx;

        HHOOK hook = SetWindowsHookExW(WH_MSGFILTER, MenuBarMsgFilterHook, nullptr, GetCurrentThreadId());
        TrackPopupMenu(subMenu, flags, btnRect.x, btnRect.y + btnRect.dy, 0, win->hwndFrame, nullptr);
        if (hook) {
            UnhookWindowsHookEx(hook);
        }

        int nextMenuIdx = gMenuBarPopupNav.nextMenuIdx;
        gMenuBarPopupNav = {};
        if (nextMenuIdx == menuIdx || menuCount <= 1) {
            gMenuBarLastDismissedIdx = menuIdx;
            gMenuBarLastDismissedTick = GetTickCount64();
            break;
        }
        menuIdx = nextMenuIdx;
    }

    return true;
}

// Activate a menu bar button by accelerator key (Alt+letter).
// If accel is 0, activate the first menu item.
// Returns true if handled.
bool ActivateMenuBarByAccel(MainWindow* win, WCHAR accel) {
    if (!win->hwndMenuToolbar || !win->menu) {
        return false;
    }

    int count = GetMenuItemCount(win->menu);
    if (count <= 0) {
        return false;
    }

    // if accel is 0 (bare Alt press), open the first menu
    if (accel == 0) {
        return HandleMenuBarCommand(win, kMenuBarCmdFirst);
    }

    // normalize to uppercase for matching
    if (accel >= 'a' && accel <= 'z') {
        accel -= 'a' - 'A';
    }

    // find the menu item whose text has &<accel>
    MENUITEMINFOW mii{};
    mii.cbSize = sizeof(MENUITEMINFOW);
    mii.fMask = MIIM_STRING;

    for (int i = 0; i < count && i < (kMenuBarCmdLast - kMenuBarCmdFirst); i++) {
        mii.dwTypeData = nullptr;
        mii.cch = 0;
        GetMenuItemInfoW(win->menu, i, TRUE, &mii);
        if (!mii.cch) {
            continue;
        }
        mii.cch++;
        WCHAR* name = AllocArrayTemp<WCHAR>((int)mii.cch);
        mii.dwTypeData = name;
        GetMenuItemInfoW(win->menu, i, TRUE, &mii);

        // look for &X where X matches accel
        WStr menuName(name, len(WStr(name)));
        for (int off = 0; off < menuName.len; off++) {
            if (menuName.s[off] == L'&' && off + 1 < menuName.len) {
                WCHAR ch = menuName.s[off + 1];
                if (ch >= 'a' && ch <= 'z') {
                    ch -= 'a' - 'A';
                }
                if (ch == accel) {
                    return HandleMenuBarCommand(win, kMenuBarCmdFirst + i);
                }
                break;
            }
        }
    }

    return false;
}
