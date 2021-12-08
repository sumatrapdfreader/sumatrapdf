/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/FileUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/BitManip.h"
#include "utils/Dpi.h"
#include "utils/GdiPlusUtil.h"
#include "mui/Mui.h"
#include "utils/WinUtil.h"

#include "Annotation.h"
#include "wingui/TreeModel.h"
#include "DisplayMode.h"
#include "Controller.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "SettingsStructs.h"
#include "DisplayModel.h"
#include "FileHistory.h"
#include "Theme.h"
#include "GlobalPrefs.h"
#include "AppColors.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "resource.h"
#include "Commands.h"
#include "ExternalViewers.h"
#include "Favorites.h"
#include "FileThumbnails.h"
#include "Menu.h"
#include "Selection.h"
#include "SumatraAbout.h"
#include "SumatraDialogs.h"
#include "Translations.h"
#include "Toolbar.h"
#include "EditAnnotations.h"

// SumatraPDF.cpp
extern Vec<Annotation*> MakeAnnotationFromSelection(TabInfo* tab, AnnotationType annotType);

struct BuildMenuCtx {
    TabInfo* tab{nullptr};
    bool isCbx{false};
    bool hasSelection{false};
    bool supportsAnnotations{false};
    Annotation* annotationUnderCursor{nullptr};
    bool hasUnsavedAnnotations{false};
    bool isCursorOnPage{false};
    bool canSendEmail{false};
    ~BuildMenuCtx();
};

BuildMenuCtx::~BuildMenuCtx() {
    delete annotationUnderCursor;
}

// value associated with menu item for owner-drawn purposes
struct MenuOwnerDrawInfo {
    const WCHAR* text{nullptr};
    // copy of MENUITEMINFO fields
    uint fType{0};
    uint fState{0};
    HBITMAP hbmpChecked{nullptr};
    HBITMAP hbmpUnchecked{nullptr};
    HBITMAP hbmpItem{nullptr};
};

struct MenuDef {
    const char* title{nullptr};
    UINT_PTR idOrSubmenu{0};
};

constexpr const char* kMenuSeparator = "-----";
constexpr UINT kMenuSeparatorID = (UINT)-13;

bool gAddCrashMeMenu = false;

#if defined(DEBUG) || defined(PRE_RELEASE_VER)
bool gShowDebugMenu = true;
#else
bool gShowDebugMenu = false;
#endif

// note: IDM_VIEW_SINGLE_PAGE - IDM_VIEW_CONTINUOUS and also
//       CmdZoomFIT_PAGE - CmdZoomCUSTOM must be in a continuous range!
static_assert(CmdViewLayoutLast - CmdViewLayoutFirst == 4, "view layout ids are not in a continuous range");
static_assert(CmdZoomLast - CmdZoomFirst == 17, "zoom ids are not in a continuous range");

MenuDef menuDefContextToc[] = {
    {
        _TRN("Expand All"),
        CmdExpandAll,
    },
    {
        _TRN("Collapse All"),
        CmdCollapseAll,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("Open Embedded PDF"),
        CmdOpenEmbeddedPDF,
    },
    {
        _TRN("Save Embedded File..."),
        CmdSaveEmbeddedFile,
    },
    // note: strings cannot be "" or else items are not there
    {
        "Add to favorites",
        CmdFavoriteAdd,
    },
    {
        "Remove from favorites",
        CmdFavoriteDel,
    },
    {
        nullptr,
        0,
    },
};

MenuDef menuDefContextFav[] = {{_TRN("Remove from favorites"), CmdFavoriteDel},
                               {
                                   nullptr,
                                   0,
                               }};

//[ ACCESSKEY_GROUP File Menu
static MenuDef menuDefFile[] = {
    {
        _TRN("New &window\tCtrl+N"),
        CmdNewWindow,
    },
    {
        _TRN("&Open...\tCtrl+O"),
        CmdOpen,
    },
    // TODO: should make it available for everyone?
    //{ "Open Folder",                        CmdOpenFolder,             },
    {
        _TRN("&Close\tCtrl+W"),
        CmdClose,
    },
    {
        _TRN("Show in &folder"),
        CmdShowInFolder,
    },
    {
        _TRN("&Save As...\tCtrl+S"),
        CmdSaveAs,
    },
    {
        _TRN("Save Annotations to existing PDF"),
        CmdSaveAnnotations,
    },
//[ ACCESSKEY_ALTERNATIVE // only one of these two will be shown
#ifdef ENABLE_SAVE_SHORTCUT
    {
        _TRN("Save S&hortcut...\tCtrl+Shift+S"),
        CmdSaveAsBookmark,
    },
//| ACCESSKEY_ALTERNATIVE
#else
    {
        _TRN("Re&name...\tF2"),
        CmdRenameFile,
    },
#endif
    //] ACCESSKEY_ALTERNATIVE
    {
        _TRN("&Print...\tCtrl+P"),
        CmdPrint,
    },
    {
        kMenuSeparator,
        0,
    },
    //[ ACCESSKEY_ALTERNATIVE // PDF/XPS/CHM specific items are dynamically removed in RebuildFileMenu
    {
        _TRN("Open in &Adobe Reader"),
        CmdOpenWithAcrobat,
    },
    {
        _TRN("Open in &Foxit Reader"),
        CmdOpenWithFoxIt,
    },
    {
        _TRN("Open &in PDF-XChange"),
        CmdOpenWithPdfXchange,
    },
    //| ACCESSKEY_ALTERNATIVE
    {
        _TRN("Open in &Microsoft XPS-Viewer"),
        CmdOpenWithXpsViewer,
    },
    //| ACCESSKEY_ALTERNATIVE
    {
        _TRN("Open in &Microsoft HTML Help"),
        CmdOpenWithHtmlHelp,
    },
    //] ACCESSKEY_ALTERNATIVE
    // further entries are added if specified in gGlobalPrefs.vecCommandLine
    {
        _TRN("Send by &E-mail..."),
        CmdSendByEmail,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("P&roperties\tCtrl+D"),
        CmdProperties,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("E&xit\tCtrl+Q"),
        CmdExit,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP File Menu

//[ ACCESSKEY_GROUP View Menu
static MenuDef menuDefView[] = {
    {
        _TRN("&Single Page\tCtrl+6"),
        CmdViewSinglePage,
    },
    {
        _TRN("&Facing\tCtrl+7"),
        CmdViewFacing,
    },
    {
        _TRN("&Book View\tCtrl+8"),
        CmdViewBook,
    },
    {
        _TRN("Show &Pages Continuously"),
        CmdViewContinuous,
    },
    // TODO: "&Inverse Reading Direction" (since some Mangas might be read left-to-right)?
    {
        _TRN("Man&ga Mode"),
        CmdViewMangaMode,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("Rotate &Left\tCtrl+Shift+-"),
        CmdViewRotateLeft,
    },
    {
        _TRN("Rotate &Right\tCtrl+Shift++"),
        CmdViewRotateRight,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("Pr&esentation\tF5"),
        CmdViewPresentationMode,
    },
    {
        _TRN("F&ullscreen\tF11"),
        CmdViewFullScreen,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("Show Book&marks\tF12"),
        CmdViewBookmarks,
    },
    {
        _TRN("Show &Toolbar\tF8"),
        CmdViewShowHideToolbar,
    },
    {
        _TRN("Show Scr&ollbars"),
        CmdViewShowHideScrollbars,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP View Menu

//[ ACCESSKEY_GROUP GoTo Menu
static MenuDef menuDefGoTo[] = {
    {
        _TRN("&Next Page\tRight Arrow"),
        CmdGoToNextPage,
    },
    {
        _TRN("&Previous Page\tLeft Arrow"),
        CmdGoToPrevPage,
    },
    {
        _TRN("&First Page\tHome"),
        CmdGoToFirstPage,
    },
    {
        _TRN("&Last Page\tEnd"),
        CmdGoToLastPage,
    },
    {
        _TRN("Pa&ge...\tCtrl+G"),
        CmdGoToPage,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("&Back\tAlt+Left Arrow"),
        CmdGoToNavBack,
    },
    {
        _TRN("F&orward\tAlt+Right Arrow"),
        CmdGoToNavForward,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("Fin&d...\tCtrl+F"),
        CmdFindFirst,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP GoTo Menu

//[ ACCESSKEY_GROUP Zoom Menu
static MenuDef menuDefZoom[] = {
    {
        _TRN("Fit &Page\tCtrl+0"),
        CmdZoomFitPage,
    },
    {
        _TRN("&Actual Size\tCtrl+1"),
        CmdZoomActualSize,
    },
    {
        _TRN("Fit &Width\tCtrl+2"),
        CmdZoomFitWidth,
    },
    {
        _TRN("Fit &Content\tCtrl+3"),
        CmdZoomFitContent,
    },
    {
        _TRN("Custom &Zoom...\tCtrl+Y"),
        CmdZoomCustom,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        "6400%",
        CmdZoom6400,
    },
    {
        "3200%",
        CmdZoom3200,
    },
    {
        "1600%",
        CmdZoom1600,
    },
    {
        "800%",
        CmdZoom800,
    },
    {
        "400%",
        CmdZoom400,
    },
    {
        "200%",
        CmdZoom200,
    },
    {
        "150%",
        CmdZoom150,
    },
    {
        "125%",
        CmdZoom125,
    },
    {
        "100%",
        CmdZoom100,
    },
    {
        "50%",
        CmdZoom50,
    },
    {
        "25%",
        CmdZoom25,
    },
    {
        "12.5%",
        CmdZoom12_5,
    },
    {
        "8.33%",
        CmdZoom8_33,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Zoom Menu

//[ ACCESSKEY_GROUP Settings Menu
static MenuDef menuDefSettings[] = {
    {
        _TRN("Change Language"),
        CmdChangeLanguage,
    },
#if 0
    { _TRN("Contribute Translation"),       CmdContributeTranslation },
    { kMenuSeparator,                             0                  },
#endif
    {
        _TRN("&Options..."),
        CmdOptions,
    },
    {
        _TRN("&Advanced Options..."),
        CmdAdvancedOptions,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Settings Menu

//[ ACCESSKEY_GROUP Favorites Menu
MenuDef menuDefFavorites[] = {
    {
        _TRN("Add to favorites"),
        CmdFavoriteAdd,
    },
    {
        _TRN("Remove from favorites"),
        CmdFavoriteDel,
    },
    {
        _TRN("Show Favorites"),
        CmdFavoriteToggle,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Favorites Menu

//[ ACCESSKEY_GROUP Help Menu
static MenuDef menuDefHelp[] = {
    {
        _TRN("Visit &Website"),
        CmdHelpVisitWebsite,
    },
    {
        _TRN("&Manual"),
        CmdHelpOpenManualInBrowser,
    },
    {
        _TRN("Check for &Updates"),
        CmdCheckUpdate,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("&About"),
        CmdHelpAbout,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Help Menu

//[ ACCESSKEY_GROUP Debug Menu
static MenuDef menuDefDebug[] = {
    {
        "Highlight links",
        CmdDebugShowLinks,
    },
    {
        "Annotation from Selection",
        CmdDebugAnnotations,
    },
    {
        "Download symbols",
        CmdDebugDownloadSymbols,
    },
    {
        "Test app",
        CmdDebugTestApp,
    },
    {
        "Show notification",
        CmdDebugShowNotif,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Debug Menu

//[ ACCESSKEY_GROUP Context Menu (Selection)
static MenuDef menuDefSelection[] = {
    {
        _TRN("&Translate With Google"),
        CmdTranslateSelectionWithGoogle,
    },
    {
        _TRN("Translate with &DeepL"),
        CmdTranslateSelectionWithDeepL,
    },
    {
        _TRN("Search With &Google"),
        CmdSearchSelectionWithGoogle,
    },
    {
        _TRN("Search With &Bing"),
        CmdSearchSelectionWithBing,
    },
    {
        _TRN("Select &All\tCtrl+A"),
        CmdSelectAll,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Selection)

//[ ACCESSKEY_GROUP Menu (Selection)
static MenuDef menuDefMainSelection[] = {
    {
        _TRN("&Copy To Clipboard\tCtrl-C"),
        CmdCopySelection,
    },
    {
        _TRN("&Translate With Google"),
        CmdTranslateSelectionWithGoogle,
    },
    {
        _TRN("Translate with &DeepL"),
        CmdTranslateSelectionWithDeepL,
    },
    {
        _TRN("&Search With Google"),
        CmdSearchSelectionWithGoogle,
    },
    {
        _TRN("Search With &Bing"),
        CmdSearchSelectionWithBing,
    },
    {
        _TRN("Select &All\tCtrl+A"),
        CmdSelectAll,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Menu (Selection)

//[ ACCESSKEY_GROUP Menubar
static MenuDef menuDefMenubar[] = {
    {
        _TRN("&File"),
        (UINT_PTR)menuDefFile,
    },
    {
        _TRN("&View"),
        (UINT_PTR)menuDefView,
    },
    {
        _TRN("&Go To"),
        (UINT_PTR)menuDefGoTo,
    },
    {
        _TRN("&Zoom"),
        (UINT_PTR)menuDefZoom,
    },
    {
        _TRN("S&election"),
        (UINT_PTR)menuDefMainSelection,
    },
    {
        _TRN("F&avorites"),
        (UINT_PTR)menuDefFavorites,
    },
    {
        _TRN("&Settings"),
        (UINT_PTR)menuDefSettings,
    },
    {
        _TRN("&Help"),
        (UINT_PTR)menuDefHelp,
    },
    {
        "Debug",
        (UINT_PTR)menuDefDebug,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Menubar

//[ ACCESSKEY_GROUP Context Menu (Create annot from selection)
static MenuDef menuDefCreateAnnotFromSelection[] = {
    {
        _TRN("&Highlight\ta"),
        CmdCreateAnnotHighlight,
    },
    {
        _TRN("&Underline\tu"),
        CmdCreateAnnotUnderline,
    },
    {
        _TRN("&Strike Out"),
        CmdCreateAnnotStrikeOut,
    },
    {
        _TRN("S&quiggly"),
        CmdCreateAnnotSquiggly,
    },
    //{ _TRN("Redact"), CmdCreateAnnotRedact, },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Create annot from selection)

//[ ACCESSKEY_GROUP Context Menu (Create annot under cursor)
static MenuDef menuDefCreateAnnotUnderCursor[] = {
    {
        _TRN("&Text"),
        CmdCreateAnnotText,
    },
    {
        _TRN("&Free Text"),
        CmdCreateAnnotFreeText,
    },
    {
        _TRN("&Stamp"),
        CmdCreateAnnotStamp,
    },
    {
        _TRN("&Caret"),
        CmdCreateAnnotCaret,
    },
    //{ _TRN("Ink"), CmdCreateAnnotInk, },
    //{ _TRN("Square"), CmdCreateAnnotSquare, },
    //{ _TRN("Circle"), CmdCreateAnnotCircle, },
    //{ _TRN("Line"), CmdCreateAnnotLine, },
    //{ _TRN("Polygon"), CmdCreateAnnotPolygon, },
    //{ _TRN("Poly Line"), CmdCreateAnnotPolyLine, },
    //{ _TRN("File Attachment"), CmdCreateAnnotFileAttachment, },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Create annot under cursor)

//[ ACCESSKEY_GROUP Context Menu (Content)
static MenuDef menuDefContext[] = {
    {
        _TRN("&Copy Selection \tCtrl-C"),
        CmdCopySelection,
    },
    {
        _TRN("S&election"),
        (UINT_PTR)menuDefSelection,
    },
    {
        _TRN("Copy &Link Address"),
        CmdCopyLinkTarget,
    },
    {
        _TRN("Copy Co&mment"),
        CmdCopyComment,
    },
    {
        _TRN("Copy &Image"),
        CmdCopyImage,
    },
    // note: strings cannot be "" or else items are not there
    {
        "Add to favorites",
        CmdFavoriteAdd,
    },
    {
        "Remove from favorites",
        CmdFavoriteDel,
    },
    {
        _TRN("Show &Favorites"),
        CmdFavoriteToggle,
    },
    {
        _TRN("Show &Bookmarks\tF12"),
        CmdViewBookmarks,
    },
    {
        _TRN("Show &Toolbar\tF8"),
        CmdViewShowHideToolbar,
    },
    {
        _TRN("Show &Scrollbars"),
        CmdViewShowHideScrollbars,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("Select Annotation in Editor"),
        CmdSelectAnnotation,
    },
    {
        _TRN("Delete Annotation\tDel"),
        CmdDeleteAnnotation,
    },
    {
        _TRN("Edit Annotations"),
        CmdEditAnnotations,
    },
    {
        _TRN("Create Annotation From Selection"),
        (UINT_PTR)menuDefCreateAnnotFromSelection,
    },
    {
        _TRN("Create Annotation &Under Cursor"),
        (UINT_PTR)menuDefCreateAnnotUnderCursor,
    },
    {
        _TRN("Save Annotations to existing PDF"),
        CmdSaveAnnotations,
    },
    {
        _TRN("E&xit Fullscreen"),
        CmdExitFullScreen,
    },
    {
        nullptr,
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Content)

//[ ACCESSKEY_GROUP Context Menu (Start)
static MenuDef menuDefContextStart[] = {
    {
        _TRN("&Open Document"),
        CmdOpenSelectedDocument,
    },
    {
        _TRN("&Pin Document"),
        CmdPinSelectedDocument,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("&Remove From History"),
        CmdForgetSelectedDocument,
    },
    {
        nullptr,
        0,
    },
};

//] ACCESSKEY_GROUP Context Menu (Start)

// clang-format off
// those menu items will be disabled if no document is opened, enabled otherwise
static UINT_PTR disableIfNoDocument[] = {
    CmdViewRotateLeft,
    CmdViewRotateRight,
    CmdGoToNextPage,
    CmdGoToPrevPage,
    CmdGoToFirstPage,
    CmdGoToLastPage,
    CmdGoToNavBack,
    CmdGoToNavForward,
    CmdGoToPage,
    CmdFindFirst,
    CmdSaveAs,
    CmdSaveAsBookmark,
    CmdSendByEmail,
    CmdSelectAll,
    CmdProperties,
    CmdViewPresentationMode,
    CmdOpenWithAcrobat,
    CmdOpenWithFoxIt,
    CmdOpenWithPdfXchange,
    CmdRenameFile,
    CmdShowInFolder,
    CmdDebugAnnotations,
    // IDM_VIEW_WITH_XPS_VIEWER and IDM_VIEW_WITH_HTML_HELP
    // are removed instead of disabled (and can remain enabled
    // for broken XPS/CHM documents)
};

static UINT_PTR disableIfDirectoryOrBrokenPDF[] = {
    CmdRenameFile,
    CmdSendByEmail,
    CmdOpenWithAcrobat,
    CmdOpenWithFoxIt,
    CmdOpenWithPdfXchange,
    CmdShowInFolder,
};

static UINT_PTR disableIfNoSelection[] = {
    CmdCopySelection,
    CmdTranslateSelectionWithDeepL,
    CmdTranslateSelectionWithGoogle,
    CmdSearchSelectionWithBing,
    CmdSearchSelectionWithGoogle,
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

static UINT_PTR removeIfNoInternetPerms[] = {
    CmdCheckUpdate,
    CmdTranslateSelectionWithGoogle,
    CmdTranslateSelectionWithDeepL,
    CmdSearchSelectionWithGoogle,
    CmdSearchSelectionWithBing,
    CmdHelpVisitWebsite,
    CmdHelpOpenManualInBrowser,
    CmdContributeTranslation,
};

static UINT_PTR removeIfNoFullscreenPerms[] = {
    CmdViewPresentationMode,
    CmdViewFullScreen,
};

static UINT_PTR removeIfNoPrefsPerms[] = {
    CmdOptions,
    CmdAdvancedOptions,
    CmdPinSelectedDocument,
    CmdForgetSelectedDocument,
    CmdFavoriteAdd,
    CmdFavoriteDel,
    CmdFavoriteToggle,
};

static UINT_PTR removeIfNoCopyPerms[] = {
    // TODO: probably those are covered by menuDefSelection
    CmdTranslateSelectionWithGoogle,
    CmdTranslateSelectionWithDeepL,
    CmdSearchSelectionWithGoogle,
    CmdSearchSelectionWithBing,
    CmdSelectAll,

    CmdCopySelection,
    CmdCopyLinkTarget,
    CmdCopyComment,
    CmdCopyImage,
    (UINT_PTR)menuDefSelection,
    (UINT_PTR)menuDefMainSelection,
};

// TODO: all prefs params also fall under disk access
static UINT_PTR removeIfNoDiskAccessPerm[] = {
    CmdNewWindow, // ???
    CmdOpen,
    CmdOpenFolder,
    CmdClose, // ???
    CmdShowInFolder,
    CmdSaveAs,
    CmdSaveAnnotations,
    CmdRenameFile,
    CmdOpenWithAcrobat,
    CmdOpenWithFoxIt,
    CmdOpenWithPdfXchange,
    CmdOpenWithXpsViewer,
    CmdOpenWithHtmlHelp,
    CmdSendByEmail, // ???
    CmdContributeTranslation, // ???
    CmdAdvancedOptions,
    CmdFavoriteAdd,
    CmdFavoriteDel,
    CmdFavoriteToggle,
    CmdSaveAnnotations,
    CmdSelectAnnotation,
    CmdDeleteAnnotation,
    CmdEditAnnotations,
    CmdOpenSelectedDocument,
    CmdPinSelectedDocument,
    CmdForgetSelectedDocument,

    (UINT_PTR)menuDefCreateAnnotFromSelection,
    (UINT_PTR)menuDefCreateAnnotUnderCursor,
};

static UINT_PTR removeIfAnnotsNotSupported[] = {
    CmdSaveAnnotations,
    CmdSelectAnnotation,
    CmdEditAnnotations,
    CmdDeleteAnnotation,
    (UINT_PTR)menuDefCreateAnnotFromSelection,
    (UINT_PTR)menuDefCreateAnnotUnderCursor,
};

static UINT_PTR rmoveIfChm[] = {
    CmdSaveAsBookmark, // ???
    CmdViewSinglePage,
    CmdViewFacing,
    CmdViewBook,
    CmdViewContinuous,
    CmdViewRotateLeft,
    CmdViewRotateRight,
    CmdViewPresentationMode,
    CmdViewShowHideScrollbars,
    CmdZoomFitPage,
    CmdZoomActualSize,
    CmdZoomFitWidth,
    CmdZoomFitContent,
    CmdZoom6400,
    CmdZoom3200,
    CmdZoom1600,
    CmdZoom800,
    CmdZoom12_5,
    CmdZoom8_33,
    (UINT_PTR)menuDefContext,
};
// clang-format on

static bool __cmdIdInList(UINT_PTR cmdId, UINT_PTR* idsList, int n) {
    for (int i = 0; i < n; i++) {
        UINT_PTR id = idsList[i];
        if (id == cmdId) {
            return true;
        }
    }
    return false;
}

#define cmdIdInList(name) __cmdIdInList(md.idOrSubmenu, name, dimof(name))

static void AddFileMenuItem(HMENU menuFile, const WCHAR* filePath, int index) {
    CrashIf(!filePath || !menuFile);
    if (!filePath || !menuFile) {
        return;
    }

    AutoFreeWstr menuString;
    menuString.SetCopy(path::GetBaseNameTemp(filePath));

    // If the name is too long, save only the ends glued together
    // E.g. 'Very Long PDF Name (3).pdf' -> 'Very Long...e (3).pdf'
    const UINT MAX_LEN = 70;
    if (menuString.size() > MAX_LEN) {
        WCHAR* tmpStr = menuString.Get();
        WCHAR* newStr = AllocArray<WCHAR>(MAX_LEN);
        const UINT half = MAX_LEN / 2;
        const UINT strSize = menuString.size() + 1; // size()+1 because wcslen() doesn't include \0
        // Copy first N/2 characters, move last N/2 characters to the halfway point
        for (UINT i = 0; i < half; i++) {
            newStr[i] = tmpStr[i];
            newStr[i + half] = tmpStr[strSize - half + i];
        }
        // Add ellipsis
        newStr[half - 2] = newStr[half - 1] = newStr[half] = '.';
        // Ensure null-terminated string
        newStr[MAX_LEN - 1] = '\0';
        // Save truncated string
        menuString.Set(newStr);
    }

    auto fileName = win::menu::ToSafeString(menuString);
    int menuIdx = (int)((index + 1) % 10);
    menuString.Set(str::Format(L"&%d) %s", menuIdx, fileName));
    uint menuId = CmdFileHistoryFirst + index;
    uint flags = MF_BYCOMMAND | MF_ENABLED | MF_STRING;
    InsertMenuW(menuFile, CmdExit, flags, menuId, menuString);
}

static void AppendRecentFilesToMenu(HMENU m) {
    if (!HasPermission(Perm::DiskAccess)) {
        return;
    }

    int i;
    for (i = 0; i < kFileHistoryMaxRecent; i++) {
        FileState* fs = gFileHistory.Get(i);
        if (!fs || fs->isMissing) {
            break;
        }
        WCHAR* fp = ToWstrTemp(fs->filePath);
        AddFileMenuItem(m, fp, i);
    }

    if (i > 0) {
        InsertMenuW(m, CmdExit, MF_BYCOMMAND | MF_SEPARATOR, 0, nullptr);
    }
}

void FillBuildMenuCtx(TabInfo* tab, BuildMenuCtx* ctx, Point pt) {
    if (!tab) {
        return;
    }
    ctx->tab = tab;
    EngineBase* engine = tab->GetEngine();
    if (engine && (engine->kind == kindEngineComicBooks)) {
        ctx->isCbx = true;
    }
    ctx->supportsAnnotations = EngineSupportsAnnotations(engine) && !tab->win->isFullScreen;
    ctx->hasUnsavedAnnotations = EngineHasUnsavedAnnotations(engine);
    ctx->canSendEmail = CanSendAsEmailAttachment(tab);

    DisplayModel* dm = tab->AsFixed();
    if (dm) {
        int pageNoUnderCursor = dm->GetPageNoByPoint(pt);
        if (pageNoUnderCursor > 0) {
            ctx->isCursorOnPage = true;
        }
        ctx->annotationUnderCursor = dm->GetAnnotationAtPos(pt, nullptr);
    }
    ctx->hasSelection = tab->win->showSelection && tab->selectionOnPage;
}

static void AppendSelectionHandlersToMenu(HMENU m, bool isEnabled) {
    if (!HasPermission(Perm::InternetAccess) || !HasPermission(Perm::CopySelection)) {
        // TODO: when we add exe handlers, only filter the URL ones
        return;
    }
    int maxEntries = CmdSelectionHandlerLast - CmdSelectionHandlerFirst;
    int n = 0;
    for (auto& sh : *gGlobalPrefs->selectionHandlers) {
        if (str::EmptyOrWhiteSpaceOnly(sh->url) || str::EmptyOrWhiteSpaceOnly(sh->name)) {
            continue;
        }
        if (n >= maxEntries) {
            break;
        }
        WCHAR* name = ToWstrTemp(sh->name);
        sh->cmdID = (int)CmdSelectionHandlerFirst + n;
        UINT flags = MF_STRING;
        flags |= isEnabled ? MF_ENABLED : MF_DISABLED;
        AppendMenuW(m, flags, (UINT_PTR)sh->cmdID, name);
        n++;
    }
}

static void AppendExternalViewersToMenu(HMENU menuFile, const WCHAR* filePath) {
    if (0 == gGlobalPrefs->externalViewers->size()) {
        return;
    }
    if (!HasPermission(Perm::DiskAccess) || (filePath && !file::Exists(filePath))) {
        return;
    }

    int maxEntries = CmdOpenWithExternalLast - CmdOpenWithExternalFirst;
    int count = 0;

    for (auto& ev : *gGlobalPrefs->externalViewers) {
        if (count >= maxEntries) {
            break;
        }
        if (!ev->commandLine) {
            continue;
        }
        if (ev->filter && !(filePath && PathMatchFilter(filePath, ev->filter))) {
            continue;
        }

        WCHAR* name = ToWstrTemp(ev->name);
        if (str::IsEmpty(name)) {
            CmdLineArgsIter args(ToWstrTemp(ev->commandLine));
            int nArgs = args.nArgs - 2;
            if (nArgs == 0) {
                continue;
            }
            WCHAR* arg0 = args.at(2 + 0);
            name = str::DupTemp(path::GetBaseNameTemp(arg0));
            WCHAR* ext = (WCHAR*)path::GetExtTemp(name);
            if (ext) {
                *ext = 0;
            }
        }

        AutoFreeWstr menuString(str::Format(_TR("Open in %s"), name));
        uint menuId = CmdOpenWithExternalFirst + count;
        InsertMenuW(menuFile, menuId, MF_BYCOMMAND | MF_ENABLED | MF_STRING, menuId, menuString);
        if (!filePath) {
            win::menu::SetEnabled(menuFile, menuId, false);
        }
        count++;
    }
}

// shows duplicate separator if no external viewers
static void DynamicPartOfFileMenu(HMENU menu, BuildMenuCtx* ctx) {
    AppendRecentFilesToMenu(menu);

    // Suppress menu items that depend on specific software being installed:
    // e-mail client, Adobe Reader, Foxit, PDF-XChange
    // Don't hide items here that won't always be hidden
    // (MenuUpdateStateForWindow() is for that)
    TabInfo* tab = ctx->tab;
    for (int cmd = CmdOpenWithFirst + 1; cmd < CmdOpenWithLast; cmd++) {
        if (!CanViewWithKnownExternalViewer(tab, cmd)) {
            win::menu::Remove(menu, cmd);
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

static void RebuildFileMenu(TabInfo* tab, HMENU menu) {
    win::menu::Empty(menu);
    BuildMenuCtx buildCtx;
    FillBuildMenuCtx(tab, &buildCtx, Point{0, 0});
    BuildMenuFromMenuDef(menuDefFile, menu, &buildCtx);
    DynamicPartOfFileMenu(menu, &buildCtx);
    RemoveBadMenuSeparators(menu);
}

HMENU BuildMenuFromMenuDef(MenuDef* menuDef, HMENU menu, BuildMenuCtx* ctx) {
    CrashIf(!menu);

    bool isDebugMenu = menuDef == menuDefDebug;
    int i = 0;

    // insert before built-in selection handlers
    if (menuDef == menuDefSelection) {
        AppendSelectionHandlersToMenu(menu, ctx ? ctx->hasSelection : false);
    }

    while (true) {
        MenuDef md = menuDef[i];
        if (md.title == nullptr) { // sentinel
            break;
        }
        i++;

        int cmdId = (int)md.idOrSubmenu;
        if (menuDef == menuDefMainSelection && cmdId == CmdTranslateSelectionWithGoogle) {
            AppendSelectionHandlersToMenu(menu, true);
        }

        MenuDef* subMenuDef = (MenuDef*)md.idOrSubmenu;
        // hacky but works: small number is command id, large is submenu (a pointer)
        bool isSubMenu = md.idOrSubmenu > CmdLast + 10000;

        bool disableMenu = false;
        bool removeMenu = false;
        if (!HasPermission(Perm::InternetAccess)) {
            removeMenu |= cmdIdInList(removeIfNoInternetPerms);
        }
        if (!HasPermission(Perm::FullscreenAccess)) {
            removeMenu |= cmdIdInList(removeIfNoFullscreenPerms);
        }
        if (!HasPermission(Perm::SavePreferences)) {
            removeMenu |= cmdIdInList(removeIfNoPrefsPerms);
        }
        if (!HasPermission(Perm::PrinterAccess)) {
            removeMenu |= (cmdId == CmdPrint);
        }
        if (!HasPermission(Perm::DiskAccess)) {
            removeMenu |= cmdIdInList(removeIfNoDiskAccessPerm);
        }
        if (!HasPermission(Perm::CopySelection)) {
            removeMenu |= cmdIdInList(removeIfNoCopyPerms);
        }

        if (ctx) {
            removeMenu |= (ctx->tab && ctx->tab->AsChm() && cmdIdInList(rmoveIfChm));
            removeMenu |= (!ctx->isCbx && (cmdId == CmdViewMangaMode));
            removeMenu |= (!ctx->supportsAnnotations && cmdIdInList(removeIfAnnotsNotSupported));
            removeMenu |= !ctx->canSendEmail && (cmdId == CmdSendByEmail);

            disableMenu |= (!ctx->hasSelection && cmdIdInList(disableIfNoSelection));
            disableMenu |= (!ctx->annotationUnderCursor && (cmdId == CmdSelectAnnotation));
            disableMenu |= (!ctx->annotationUnderCursor && (cmdId == CmdDeleteAnnotation));
            disableMenu |= !ctx->hasUnsavedAnnotations && (cmdId == CmdSaveAnnotations);

            removeMenu |= !ctx->isCursorOnPage && (subMenuDef == menuDefCreateAnnotUnderCursor);
            removeMenu |= !ctx->hasSelection && (subMenuDef == menuDefCreateAnnotFromSelection);
        }
        removeMenu |= ((subMenuDef == menuDefDebug) && !gShowDebugMenu);
        if (removeMenu) {
            continue;
        }

        // prevent two consecutive separators
        if (str::Eq(md.title, kMenuSeparator)) {
            AppendMenuW(menu, MF_SEPARATOR, kMenuSeparatorID, nullptr);
            continue;
        }

        bool noTranslate = isDebugMenu || cmdIdInList(menusNoTranslate);
        noTranslate |= (subMenuDef == menuDefDebug);
        AutoFreeWstr tmp;
        const WCHAR* title = nullptr;
        if (noTranslate) {
            tmp = strconv::Utf8ToWstr(md.title);
            title = tmp.Get();
        } else {
            title = trans::GetTranslation(md.title);
        }

        if (isSubMenu) {
            HMENU subMenu = BuildMenuFromMenuDef(subMenuDef, CreatePopupMenu(), ctx);
            UINT flags = MF_POPUP | (disableMenu ? MF_DISABLED : MF_ENABLED);
            if (subMenuDef == menuDefFile) {
                DynamicPartOfFileMenu(subMenu, ctx);
            }
            AppendMenuW(menu, flags, (UINT_PTR)subMenu, title);
        } else {
            UINT flags = MF_STRING | (disableMenu ? MF_DISABLED : MF_ENABLED);
            AppendMenuW(menu, flags, md.idOrSubmenu, title);
        }

        if (cmdId == CmdOpenWithHtmlHelp) {
            TabInfo* tab = ctx->tab;
            AppendExternalViewersToMenu(menu, tab ? tab->filePath.Get() : nullptr);
        }
    }
    RemoveBadMenuSeparators(menu);
    return menu;
}

// clang-format off
static struct {
    int itemId;
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
    { CmdZoomFitPage,    ZOOM_FIT_PAGE    },
    { CmdZoomFitWidth,   ZOOM_FIT_WIDTH   },
    { CmdZoomFitContent, ZOOM_FIT_CONTENT },
    { CmdZoomActualSize, ZOOM_ACTUAL_SIZE },
};
// clang-format on

int MenuIdFromVirtualZoom(float virtualZoom) {
    for (auto&& it : gZoomMenuIds) {
        if (virtualZoom == it.zoom) {
            return it.itemId;
        }
    }
    return CmdZoomCustom;
}

float ZoomMenuItemToZoom(int menuItemId) {
    for (auto&& it : gZoomMenuIds) {
        if (menuItemId == it.itemId) {
            return it.zoom;
        }
    }
    CrashIf(true);
    return 100.0;
}

static void ZoomMenuItemCheck(HMENU m, int menuItemId, bool canZoom) {
    CrashIf((CmdZoomFirst > menuItemId) || (menuItemId > CmdZoomLast));

    for (auto&& it : gZoomMenuIds) {
        win::menu::SetEnabled(m, it.itemId, canZoom);
    }

    if (CmdZoom100 == menuItemId) {
        menuItemId = CmdZoomActualSize;
    }
    CheckMenuRadioItem(m, CmdZoomFirst, CmdZoomLast, menuItemId, MF_BYCOMMAND);
    if (CmdZoomActualSize == menuItemId) {
        CheckMenuRadioItem(m, CmdZoom100, CmdZoom100, CmdZoom100, MF_BYCOMMAND);
    }
}

void MenuUpdateZoom(WindowInfo* win) {
    float zoomVirtual = gGlobalPrefs->defaultZoomFloat;
    if (win->IsDocLoaded()) {
        zoomVirtual = win->ctrl->GetZoomVirtual();
    }
    int menuId = MenuIdFromVirtualZoom(zoomVirtual);
    ZoomMenuItemCheck(win->menu, menuId, win->IsDocLoaded());
}

void MenuUpdatePrintItem(WindowInfo* win, HMENU menu, bool disableOnly = false) {
    bool filePrintEnabled = win->IsDocLoaded();
#ifndef DISABLE_DOCUMENT_RESTRICTIONS
    bool filePrintAllowed = !filePrintEnabled || !win->AsFixed() || win->AsFixed()->GetEngine()->AllowsPrinting();
#else
    bool filePrintAllowed = true;
#endif

    int idx;
    for (idx = 0; idx < dimof(menuDefFile) && menuDefFile[idx].idOrSubmenu != CmdPrint; idx++) {
        // do nothing
    }
    if (idx < dimof(menuDefFile)) {
        const WCHAR* printItem = trans::GetTranslation(menuDefFile[idx].title);
        if (!filePrintAllowed) {
            printItem = _TR("&Print... (denied)");
        }
        if (!filePrintAllowed || !disableOnly) {
            ModifyMenuW(menu, CmdPrint, MF_BYCOMMAND | MF_STRING, (UINT_PTR)CmdPrint, printItem);
        }
    }

    win::menu::SetEnabled(menu, CmdPrint, filePrintEnabled && filePrintAllowed);
}

static bool IsFileCloseMenuEnabled() {
    for (size_t i = 0; i < gWindows.size(); i++) {
        if (!gWindows.at(i)->IsAboutWindow()) {
            return true;
        }
    }
    return false;
}

static void SetMenuStateForSelection(TabInfo* tab, HMENU menu) {
    bool isTextSelected = tab && tab->win && tab->win->showSelection && tab->selectionOnPage;
    for (int id : disableIfNoSelection) {
        win::menu::SetEnabled(menu, id, isTextSelected);
    }
    for (int id = CmdSelectionHandlerFirst; id < CmdSelectionHandlerLast; id++) {
        win::menu::SetEnabled(menu, id, isTextSelected);
    }
}

void MenuUpdateDisplayMode(WindowInfo* win) {
    bool enabled = win->IsDocLoaded();
    DisplayMode displayMode = gGlobalPrefs->defaultDisplayModeEnum;
    if (enabled) {
        displayMode = win->ctrl->GetDisplayMode();
    }

    for (int id = CmdViewLayoutFirst; id <= CmdViewLayoutLast; id++) {
        win::menu::SetEnabled(win->menu, id, enabled);
    }

    int id = 0;
    if (IsSingle(displayMode)) {
        id = CmdViewSinglePage;
    } else if (IsFacing(displayMode)) {
        id = CmdViewFacing;
    } else if (IsBookView(displayMode)) {
        id = CmdViewBook;
    } else {
        CrashIf(win->ctrl || DisplayMode::Automatic != displayMode);
    }

    CheckMenuRadioItem(win->menu, CmdViewLayoutFirst, CmdViewLayoutLast, id, MF_BYCOMMAND);
    win::menu::SetChecked(win->menu, CmdViewContinuous, IsContinuous(displayMode));

    if (win->currentTab && win->currentTab->GetEngineType() == kindEngineComicBooks) {
        bool mangaMode = win->AsFixed()->GetDisplayR2L();
        win::menu::SetChecked(win->menu, CmdViewMangaMode, mangaMode);
    }
}

static void MenuUpdateStateForWindow(WindowInfo* win) {
    TabInfo* tab = win->currentTab;

    bool hasDocument = tab && tab->IsDocLoaded();
    for (int id : disableIfNoDocument) {
        win::menu::SetEnabled(win->menu, id, hasDocument);
    }

    SetMenuStateForSelection(tab, win->menu);

    // TODO: happens with UseTabs = false with .pdf files
    ReportIf(IsFileCloseMenuEnabled() == win->IsAboutWindow());
    win::menu::SetEnabled(win->menu, CmdClose, IsFileCloseMenuEnabled());

    MenuUpdatePrintItem(win, win->menu);

    bool enabled = win->IsDocLoaded() && tab && tab->ctrl->HacToc();
    win::menu::SetEnabled(win->menu, CmdViewBookmarks, enabled);

    bool documentSpecific = win->IsDocLoaded();
    bool checked = documentSpecific ? win->tocVisible : gGlobalPrefs->showToc;
    win::menu::SetChecked(win->menu, CmdViewBookmarks, checked);

    win::menu::SetChecked(win->menu, CmdFavoriteToggle, gGlobalPrefs->showFavorites);
    win::menu::SetChecked(win->menu, CmdViewShowHideToolbar, gGlobalPrefs->showToolbar);
    win::menu::SetChecked(win->menu, CmdViewShowHideScrollbars, !gGlobalPrefs->fixedPageUI.hideScrollbars);
    MenuUpdateDisplayMode(win);
    MenuUpdateZoom(win);

    if (win->IsDocLoaded() && tab) {
        win::menu::SetEnabled(win->menu, CmdGoToNavBack, tab->ctrl->CanNavigate(-1));
        win::menu::SetEnabled(win->menu, CmdGoToNavForward, tab->ctrl->CanNavigate(1));
    }

    // TODO: is this check too expensive?
    bool fileExists = tab && file::Exists(tab->filePath);

    if (tab && tab->ctrl && !fileExists && dir::Exists(tab->filePath)) {
        for (int id : disableIfDirectoryOrBrokenPDF) {
            win::menu::SetEnabled(win->menu, id, false);
        }
    } else if (fileExists && CouldBePDFDoc(tab)) {
        for (int id : disableIfDirectoryOrBrokenPDF) {
            win::menu::SetEnabled(win->menu, id, true);
        }
    }

    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (engine) {
        win::menu::SetEnabled(win->menu, CmdFindFirst, !engine->IsImageCollection());
    }

    if (win->IsDocLoaded() && !fileExists) {
        win::menu::SetEnabled(win->menu, CmdRenameFile, false);
    }

#if defined(ENABLE_THEME)
    CheckMenuRadioItem(win->menu, IDM_CHANGE_THEME_FIRST, IDM_CHANGE_THEME_LAST,
                       IDM_CHANGE_THEME_FIRST + GetCurrentThemeIndex(), MF_BYCOMMAND);
#endif

    win::menu::SetChecked(win->menu, CmdDebugShowLinks, gDebugShowLinks);
    win::menu::SetEnabled(win->menu, CmdDebugAnnotations,
                          tab && tab->selectionOnPage && win->showSelection && EngineSupportsAnnotations(engine));
}

void OnAboutContextMenu(WindowInfo* win, int x, int y) {
    if (!HasPermission(Perm::SavePreferences | Perm::DiskAccess) || !gGlobalPrefs->rememberOpenedFiles ||
        !gGlobalPrefs->showStartPage) {
        return;
    }

    const WCHAR* filePathW = GetStaticLink(win->staticLinks, x, y, nullptr);
    char* filePath = ToUtf8Temp(filePathW);
    if (!filePath || *filePath == '<' || str::StartsWith(filePath, "http://") ||
        str::StartsWith(filePath, "https://")) {
        return;
    }

    FileState* fs = gFileHistory.Find(filePath, nullptr);
    CrashIf(!fs);
    if (!fs) {
        return;
    }

    HMENU popup = BuildMenuFromMenuDef(menuDefContextStart, CreatePopupMenu(), nullptr);
    win::menu::SetChecked(popup, CmdPinSelectedDocument, fs->isPinned);
    POINT pt = {x, y};
    MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
    MarkMenuOwnerDraw(popup);
    INT cmd = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);

    if (CmdOpenSelectedDocument == cmd) {
        LoadArgs args(filePath, win);
        LoadDocument(args);
        return;
    }

    if (CmdPinSelectedDocument == cmd) {
        fs->isPinned = !fs->isPinned;
        win->HideToolTip();
        win->RedrawAll(true);
        return;
    }

    if (CmdForgetSelectedDocument == cmd) {
        if (fs->favorites->size() > 0) {
            // just hide documents with favorites
            gFileHistory.MarkFileInexistent(fs->filePath, true);
        } else {
            gFileHistory.Remove(fs);
            DeleteDisplayState(fs);
        }
        CleanUpThumbnailCache(gFileHistory);
        win->HideToolTip();
        win->RedrawAll(true);
        return;
    }
}

void OnWindowContextMenu(WindowInfo* win, int x, int y) {
    DisplayModel* dm = win->AsFixed();
    CrashIf(!dm);
    if (!dm) {
        return;
    }

    TabInfo* tab = win->currentTab;
    IPageElement* pageEl = dm->GetElementAtPos({x, y}, nullptr);

    WCHAR* value = nullptr;
    if (pageEl) {
        value = pageEl->GetValue();
    }

    BuildMenuCtx buildCtx;
    FillBuildMenuCtx(tab, &buildCtx, Point{x, y});
    HMENU popup = BuildMenuFromMenuDef(menuDefContext, CreatePopupMenu(), &buildCtx);

    int pageNoUnderCursor = dm->GetPageNoByPoint(Point{x, y});
    PointF ptOnPage = dm->CvtFromScreen(Point{x, y}, pageNoUnderCursor);
    EngineBase* engine = dm->GetEngine();
    bool annotationsSupported = EngineSupportsAnnotations(engine) && !win->isFullScreen;

    if (!pageEl || !pageEl->Is(kindPageElementDest) || !value) {
        win::menu::Remove(popup, CmdCopyLinkTarget);
    }
    if (!pageEl || !pageEl->Is(kindPageElementComment) || !value) {
        win::menu::Remove(popup, CmdCopyComment);
    }
    if (!pageEl || !pageEl->Is(kindPageElementImage)) {
        win::menu::Remove(popup, CmdCopyImage);
    }

    bool isFullScreen = win->isFullScreen || win->presentation;
    if (!isFullScreen) {
        win::menu::Remove(popup, CmdExitFullScreen);
    }
    SetMenuStateForSelection(tab, popup);

    MenuUpdatePrintItem(win, popup, true);
    win::menu::SetEnabled(popup, CmdViewBookmarks, win->ctrl->HacToc());
    win::menu::SetChecked(popup, CmdViewBookmarks, win->tocVisible);

    win::menu::SetChecked(popup, CmdViewShowHideScrollbars, !gGlobalPrefs->fixedPageUI.hideScrollbars);

    win::menu::SetEnabled(popup, CmdFavoriteToggle, HasFavorites());
    win::menu::SetChecked(popup, CmdFavoriteToggle, gGlobalPrefs->showFavorites);

    const WCHAR* filePath = win->ctrl->GetFilePath();
    bool favsSupported = HasPermission(Perm::SavePreferences) && HasPermission(Perm::DiskAccess);
    if (favsSupported) {
        if (pageNoUnderCursor > 0) {
            AutoFreeWstr pageLabel = win->ctrl->GetPageLabel(pageNoUnderCursor);
            bool isBookmarked = gFavorites.IsPageInFavorites(filePath, pageNoUnderCursor);
            if (isBookmarked) {
                win::menu::Remove(popup, CmdFavoriteAdd);

                // %s and not %d because re-using translation from RebuildFavMenu()
                auto tr = _TR("Remove page %s from favorites");
                AutoFreeWstr s = str::Format(tr, pageLabel.Get());
                win::menu::SetText(popup, CmdFavoriteDel, s);
            } else {
                win::menu::Remove(popup, CmdFavoriteDel);

                // %s and not %d because re-using translation from RebuildFavMenu()
                auto tr = _TR("Add page %s to favorites\tCtrl+B");
                AutoFreeWstr s = str::Format(tr, pageLabel.Get());
                win::menu::SetText(popup, CmdFavoriteAdd, s);
            }
        } else {
            win::menu::Remove(popup, CmdFavoriteAdd);
            win::menu::Remove(popup, CmdFavoriteDel);
        }
    }

    // if toolbar is not shown, add option to show it
    if (gGlobalPrefs->showToolbar) {
        win::menu::Remove(popup, CmdViewShowHideToolbar);
    }
    RemoveBadMenuSeparators(popup);

    POINT pt = {x, y};
    MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
    MarkMenuOwnerDraw(popup);
    UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    int cmd = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);

    if (cmd >= CmdSelectionHandlerFirst && cmd < CmdSelectionHandlerLast) {
        HwndSendCommand(win->hwndFrame, cmd);
        return;
    }

    AnnotationType annotType = (AnnotationType)(cmd - CmdCreateAnnotText);
    Vec<Annotation*> createdAnnots;
    switch (cmd) {
        case CmdCopySelection:
        case CmdTranslateSelectionWithGoogle:
        case CmdTranslateSelectionWithDeepL:
        case CmdSearchSelectionWithGoogle:
        case CmdSearchSelectionWithBing:
        case CmdSelectAll:
        case CmdSaveAs:
        case CmdPrint:
        case CmdViewBookmarks:
        case CmdFavoriteToggle:
        case CmdProperties:
        case CmdViewShowHideToolbar:
        case CmdViewShowHideScrollbars:
        case CmdSaveAnnotations:
            // handle in FrameOnCommand() in SumatraPDF.cpp
            HwndSendCommand(win->hwndFrame, cmd);
            break;
        case CmdSelectAnnotation:
            CrashIf(!buildCtx.annotationUnderCursor);

            [[fallthrough]];
        case CmdEditAnnotations:
            StartEditAnnotations(tab, nullptr);
            SelectAnnotationInEditWindow(tab->editAnnotsWindow, buildCtx.annotationUnderCursor);
            break;
        case CmdDeleteAnnotation:
            DeleteAnnotationAndUpdateUI(tab, tab->editAnnotsWindow, buildCtx.annotationUnderCursor);
            break;
        case CmdCopyLinkTarget: {
            WCHAR* tmp = CleanupURLForClipbardCopy(value);
            CopyTextToClipboard(tmp);
            str::Free(tmp);
        } break;
        case CmdCopyComment:
            CopyTextToClipboard(value);
            break;

        case CmdCopyImage:
            if (pageEl) {
                RenderedBitmap* bmp = dm->GetEngine()->GetImageForPageElement(pageEl);
                if (bmp) {
                    CopyImageToClipboard(bmp->GetBitmap(), false);
                }
                delete bmp;
            }
            break;
        case CmdFavoriteAdd:
            AddFavoriteForCurrentPage(win);
            break;
        case CmdFavoriteDel:
            DelFavorite(filePath, pageNoUnderCursor);
            break;
        case CmdExitFullScreen:
            ExitFullScreen(win);
            break;
        case CmdCreateAnnotText:
        case CmdCreateAnnotFreeText:
        case CmdCreateAnnotStamp:
        case CmdCreateAnnotCaret:
        case CmdCreateAnnotSquare:
        case CmdCreateAnnotLine:
        case CmdCreateAnnotCircle: {
            auto annot = EngineMupdfCreateAnnotation(engine, annotType, pageNoUnderCursor, ptOnPage);
            if (annot) {
                WindowInfoRerender(win);
                ToolbarUpdateStateForWindow(win, true);
                createdAnnots.Append(annot);
            }
        } break;
        case CmdCreateAnnotHighlight:
            createdAnnots = MakeAnnotationFromSelection(tab, AnnotationType::Highlight);
            break;
        case CmdCreateAnnotSquiggly:
            createdAnnots = MakeAnnotationFromSelection(tab, AnnotationType::Squiggly);
            break;
        case CmdCreateAnnotStrikeOut:
            createdAnnots = MakeAnnotationFromSelection(tab, AnnotationType::StrikeOut);
            break;
        case CmdCreateAnnotUnderline:
            createdAnnots = MakeAnnotationFromSelection(tab, AnnotationType::Underline);
            break;
        case CmdCreateAnnotInk:
        case CmdCreateAnnotPolyLine:
            // TODO: implement me
            break;
    }
    if (!createdAnnots.empty()) {
        StartEditAnnotations(tab, createdAnnots);
    }
    // TODO: should delete it?
    // delete buildCtx.annotationUnderCursor;

    /*
        { _TR("Line"), CmdCreateAnnotLine, },
        { _TR_TODON("Highlight"), CmdCreateAnnotHighlight, },
        { _TR_TODON("Underline"), CmdCreateAnnotUnderline, },
        { _TR_TODON("Strike Out"), CmdCreateAnnotStrikeOut, },
        { _TR_TODON("Squiggly"), CmdCreateAnnotSquiggly, },
        { _TR_TODON("File Attachment"), CmdCreateAnnotFileAttachment, },
        { _TR_TODON("Redact"), CmdCreateAnnotRedact, },
    */
    // TODO: those require creating
    /*
        { _TR_TODON("Polygon"), CmdCreateAnnotPolygon, },
        { _TR_TODON("Poly Line"), CmdCreateAnnotPolyLine, },
    */
}

// so that we can do free everything at exit
Vec<MenuOwnerDrawInfo*> g_menuDrawInfos;

void FreeAllMenuDrawInfos() {
    while (g_menuDrawInfos.size() != 0) {
        // Note: could be faster
        FreeMenuOwnerDrawInfo(g_menuDrawInfos[0]);
    }
}

void FreeMenuOwnerDrawInfo(MenuOwnerDrawInfo* modi) {
    g_menuDrawInfos.Remove(modi);
    str::Free(modi->text);
    free(modi);
}

static HFONT gMenuFont = nullptr;

HFONT GetMenuFont() {
    if (!gMenuFont) {
        NONCLIENTMETRICS ncm = {0};
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        gMenuFont = CreateFontIndirect(&ncm.lfMenuFont);
    }
    return gMenuFont;
}

struct MenuText {
    WCHAR* menuText;
    int menuTextLen;
    WCHAR* shortcutText;
    int shortcutTextLen;
};

// menu text consists of potentially 2 parts:
// - text of the menu item
// - text for the keyboard shortcut
// They are separated with \t
static void ParseMenuText(WCHAR* s, MenuText& mt) {
    mt.shortcutText = nullptr;
    mt.menuText = s;
    while (*s && *s != L'\t') {
        s++;
    }
    mt.menuTextLen = (int)(s - mt.menuText);
    if (*s != L'\t') {
        return;
    }
    s++;
    mt.shortcutText = s;
    while (*s) {
        s++;
    }
    mt.shortcutTextLen = (int)(s - mt.shortcutText);
}

void FreeMenuOwnerDrawInfoData(HMENU hmenu) {
    MENUITEMINFOW mii = {0};
    mii.cbSize = sizeof(MENUITEMINFOW);

    int n = GetMenuItemCount(hmenu);
    for (int i = 0; i < n; i++) {
        mii.fMask = MIIM_DATA | MIIM_FTYPE | MIIM_SUBMENU;
        BOOL ok = GetMenuItemInfoW(hmenu, (uint)i, TRUE /* by position */, &mii);
        CrashIf(!ok);
        auto modi = (MenuOwnerDrawInfo*)mii.dwItemData;
        if (modi != nullptr) {
            FreeMenuOwnerDrawInfo(modi);
            mii.dwItemData = 0;
            mii.fType &= ~MFT_OWNERDRAW;
            SetMenuItemInfoW(hmenu, (uint)i, TRUE /* by position */, &mii);
        }
        if (mii.hSubMenu != nullptr) {
            MarkMenuOwnerDraw(mii.hSubMenu);
        }
    };
}
void MarkMenuOwnerDraw(HMENU hmenu) {
    if (!gOwnerDrawMenu) {
        return;
    }
    WCHAR buf[1024];

    MENUITEMINFOW mii = {0};
    mii.cbSize = sizeof(MENUITEMINFOW);

    int n = GetMenuItemCount(hmenu);

    for (int i = 0; i < n; i++) {
        buf[0] = 0;
        mii.fMask = MIIM_BITMAP | MIIM_CHECKMARKS | MIIM_DATA | MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU | MIIM_STRING;
        mii.dwTypeData = &(buf[0]);
        mii.cch = dimof(buf);
        BOOL ok = GetMenuItemInfoW(hmenu, (uint)i, TRUE /* by position */, &mii);
        CrashIf(!ok);

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
        if (str::Len(buf) > 0) {
            modi->text = str::Dup(buf);
        }
        mii.dwItemData = (ULONG_PTR)modi;
        SetMenuItemInfoW(hmenu, (uint)i, TRUE /* by position */, &mii);

        if (mii.hSubMenu != nullptr) {
            MarkMenuOwnerDraw(mii.hSubMenu);
        }
    }
}

constexpr int kMenuPaddingY = 2;
constexpr int kMenuPaddingX = 2;

void MenuOwnerDrawnMesureItem(HWND hwnd, MEASUREITEMSTRUCT* mis) {
    if (ODT_MENU != mis->CtlType) {
        return;
    }
    auto modi = (MenuOwnerDrawInfo*)mis->itemData;

    bool isSeparator = bit::IsMaskSet(modi->fType, (uint)MFT_SEPARATOR);
    if (isSeparator) {
        mis->itemHeight = DpiScale(hwnd, 7);
        mis->itemWidth = DpiScale(hwnd, 33);
        return;
    }

    auto text = modi && modi->text ? modi->text : L"Dummy";
    HFONT font = GetMenuFont();
    MenuText mt;
    ParseMenuText((WCHAR*)text, mt);

    auto size = TextSizeInHwnd(hwnd, mt.menuText, font);
    mis->itemHeight = size.dy;
    int dx = size.dx;
    if (mt.shortcutText != nullptr) {
        // add space betweeen menu text and shortcut
        size = TextSizeInHwnd(hwnd, L"    ", font);
        dx += size.dx;
        size = TextSizeInHwnd(hwnd, mt.shortcutText, font);
        dx += size.dx;
    }
    auto padX = DpiScale(hwnd, kMenuPaddingX);
    auto padY = DpiScale(hwnd, kMenuPaddingY);

    auto cxMenuCheck = GetSystemMetrics(SM_CXMENUCHECK);
    mis->itemHeight += padY * 2;
    mis->itemWidth = uint(dx + DpiScale(hwnd, cxMenuCheck) + (padX * 2));
}

// https://gist.github.com/kjk/1df108aa126b7d8e298a5092550a53b7
void MenuOwnerDrawnDrawItem(__unused HWND hwnd, DRAWITEMSTRUCT* dis) {
    if (ODT_MENU != dis->CtlType) {
        return;
    }
    auto modi = (MenuOwnerDrawInfo*)dis->itemData;
    if (!modi) {
        return;
    }

    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms647578(v=vs.85).aspx

    // low-order word of the dwTypeData member is the bitmap handle
    // HBITMAP bmp = (HBITMAP)LOWORD(modi->dwTypeData) ?
    // bool isBitmap = bit::IsMaskSet(modi->fType, (uint)MFT_BITMAP);

    // ???
    // bool isMenuBarBreak = bit::IsMaskSet(modi->fType, (uint)MFT_MENUBARBREAK);

    // ??
    // bool isMenuBreak = bit::IsMaskSet(modi->fType, (uint)MFT_MENUBREAK);

    // bool isRadioCheck = bit::IsMaskSet(modi->fType, (uint)MFT_RADIOCHECK);

    bool isSeparator = bit::IsMaskSet(modi->fType, (uint)MFT_SEPARATOR);

    // default should be drawn in bold
    // bool isDefault = bit::IsMaskSet(modi->fState, (uint)MFS_DEFAULT);

    // disabled should be drawn grayed out
    // bool isDisabled = bit::IsMaskSet(modi->fState, (uint)MFS_DISABLED);

    // don't know what that means
    // bool isHilited = bit::IsMaskSet(modi->fState, (uint)MFS_HILITE);

    // checked/unchecked state for check and radio menus?
    // uses hbmpChecked, otherwise use hbmpUnchecked ?
    // bool isChecked = bit::IsMaskSet(modi->fState, (uint)MFS_CHECKED);

    auto hdc = dis->hDC;
    HFONT font = GetMenuFont();
    auto prevFont = SelectObject(hdc, font);

    COLORREF bgCol = GetAppColor(AppColor::MainWindowBg);
    COLORREF txtCol = GetAppColor(AppColor::MainWindowText);

    bool isSelected = bit::IsMaskSet(dis->itemState, (uint)ODS_SELECTED);
    if (isSelected) {
        // TODO: probably better colors
        std::swap(bgCol, txtCol);
    }

    RECT rc = dis->rcItem;

    int padY = DpiScale(hwnd, kMenuPaddingY);
    int padX = DpiScale(hwnd, kMenuPaddingX);
    int dxCheckMark = DpiScale(hwnd, GetSystemMetrics(SM_CXMENUCHECK));

    auto hbr = CreateSolidBrush(bgCol);
    FillRect(hdc, &rc, hbr);
    DeleteObject(hbr);

    if (isSeparator) {
        CrashIf(modi->text);
        int sx = rc.left + dxCheckMark;
        int y = rc.top + (RectDy(rc) / 2);
        int ex = rc.right - padX;
        auto pen = CreatePen(PS_SOLID, 1, txtCol);
        auto prevPen = SelectObject(hdc, pen);
        MoveToEx(hdc, sx, y, nullptr);
        LineTo(hdc, ex, y);
        SelectObject(hdc, prevPen);
        DeleteObject(pen);
        return;
    }

    // TODO: probably could be a bitmap etc.
    if (!modi->text) {
        return;
    }

    MenuText mt;
    ParseMenuText((WCHAR*)modi->text, mt);

    // TODO: improve how we paint the menu:
    // - paint checkmark if this is checkbox menu
    // - position text the right way (not just DT_CENTER)
    //   taking into account LTR mode
    // - paint shortcut (part after \t if exists) separately
    // - paint disabled state better
    // - paint icons for system menus
    SetTextColor(hdc, txtCol);
    SetBkColor(hdc, bgCol);

    // DrawTextEx handles & => underscore drawing
    rc.top += padY;
    rc.left += dxCheckMark;
    DrawTextExW(hdc, mt.menuText, mt.menuTextLen, &rc, DT_LEFT, nullptr);
    if (mt.shortcutText != nullptr) {
        rc = dis->rcItem;
        rc.top += padY;
        rc.right -= (padX + dxCheckMark / 2);
        DrawTextExW(hdc, mt.shortcutText, mt.shortcutTextLen, &rc, DT_RIGHT, nullptr);
    }
    SelectObject(hdc, prevFont);
}

HMENU BuildMenu(WindowInfo* win) {
    TabInfo* tab = win->currentTab;

    BuildMenuCtx buildCtx;
    FillBuildMenuCtx(tab, &buildCtx, Point{0, 0});

    HMENU mainMenu = BuildMenuFromMenuDef(menuDefMenubar, CreateMenu(), &buildCtx);

#if defined(ENABLE_THEME) && 0
    // Build the themes sub-menu of the settings menu
    MenuDef menuDefTheme[THEME_COUNT + 1];
    static_assert(IDM_CHANGE_THEME_LAST - IDM_CHANGE_THEME_FIRST + 1 >= THEME_COUNT,
                  "Too many themes. Either remove some or update IDM_CHANGE_THEME_LAST");
    for (int i = 0; i < THEME_COUNT; i++) {
        menuDefTheme[i] = {GetThemeByIndex(i)->name, IDM_CHANGE_THEME_FIRST + i, 0};
    }
    HMENU m2 = BuildMenuFromMenuDef(menuDefTheme, CreateMenu(), filter);
    AppendMenuW(m, MF_POPUP | MF_STRING, (UINT_PTR)m2, _TR("&Theme"));
#endif

    MarkMenuOwnerDraw(mainMenu);
    return mainMenu;
}

void UpdateAppMenu(WindowInfo* win, HMENU m) {
    CrashIf(!win);
    if (!win) {
        return;
    }
    UINT_PTR id = (UINT_PTR)GetMenuItemID(m, 0);
    if (id == menuDefFile[0].idOrSubmenu) {
        RebuildFileMenu(win->currentTab, m);
    } else if (id == menuDefFavorites[0].idOrSubmenu) {
        win::menu::Empty(m);
        BuildMenuFromMenuDef(menuDefFavorites, m, nullptr);
        RebuildFavMenu(win, m);
    }
    MenuUpdateStateForWindow(win);
    MarkMenuOwnerDraw(win->menu);
}

// show/hide top-level menu bar. This doesn't persist across launches
// so that accidental removal of the menu isn't catastrophic
void ShowHideMenuBar(WindowInfo* win, bool showTemporarily) {
    CrashIf(!win->menu);
    if (win->presentation || win->isFullScreen) {
        return;
    }

    HWND hwnd = win->hwndFrame;

    if (showTemporarily) {
        SetMenu(hwnd, win->menu);
        return;
    }

    bool hideMenu = !showTemporarily && GetMenu(hwnd) != nullptr;
    SetMenu(hwnd, hideMenu ? nullptr : win->menu);
    win->isMenuHidden = hideMenu;
}
