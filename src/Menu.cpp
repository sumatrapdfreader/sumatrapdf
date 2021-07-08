/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/CmdLineParser.h"
#include "utils/FileUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/BitManip.h"
#include "utils/Dpi.h"
#include "utils/GdiPlusUtil.h"
#include "mui/Mui.h"
#include "utils/WinUtil.h"

#include "wingui/TreeModel.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EngineCreate.h"
#include "EnginePdf.h"

#include "DisplayMode.h"
#include "SettingsStructs.h"
#include "Controller.h"
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
extern Annotation* MakeAnnotationFromSelection(TabInfo* tab, AnnotationType annotType);

constexpr int kPermFlagOffset = 9;
enum {
    MF_NO_TRANSLATE = 1 << 0,
    MF_NOT_FOR_CHM = 1 << 1,
    MF_NOT_FOR_EBOOK_UI = 1 << 2,
    MF_CBX_ONLY = 1 << 3,
    MF_NEEDS_CURSOR_ON_PAGE = 1 << 4, // cursor must be withing page boundaries
    MF_NEEDS_SELECTION = 1 << 5,      // user must have text selection active
    MF_NEEDS_ANNOTS = 1 << 6,         // engine needs to support annotations
    MF_NEEDS_ANNOT_UNDER_CURSOR = 1 << 7,
    MF_REQ_INET_ACCESS = Perm::InternetAccess << kPermFlagOffset,
    MF_REQ_DISK_ACCESS = Perm::DiskAccess << kPermFlagOffset,
    MF_REQ_PREF_ACCESS = Perm::SavePreferences << kPermFlagOffset,
    MF_REQ_PRINTER_ACCESS = Perm::PrinterAccess << kPermFlagOffset,
    MF_REQ_ALLOW_COPY = Perm::CopySelection << kPermFlagOffset,
    MF_REQ_FULLSCREEN = Perm::FullscreenAccess << kPermFlagOffset,
};

struct BuildMenuCtx {
    bool isChm{false};
    bool isEbookUI{false};
    bool isCbx{false};
    bool hasSelection{false};
    bool supportsAnnotations{false};
    bool hasAnnotationUnderCursor{false};
    bool isCursorOnPage{false};
};

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
    int flags{0};
};

constexpr const char* kMenuSeparator = "-----";
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

// clang-format off
MenuDef menuDefContextToc[] = {
    {_TRN("Expand All"),            CmdExpandAll,         0 },
    {_TRN("Collapse All"),          CmdCollapseAll,       0 },
    {kMenuSeparator,                CmdSeparatorEmbed,    MF_NO_TRANSLATE},
    {_TRN("Open Embedded PDF"),     CmdOpenEmbeddedPDF,   0 },
    {_TRN("Save Embedded File..."), CmdSaveEmbeddedFile,  0 },
    // note: strings cannot be "" or else items are not there
    {"add",                         CmdFavoriteAdd,       MF_NO_TRANSLATE},
    {"del",                         CmdFavoriteDel,       MF_NO_TRANSLATE},
    { 0, 0, 0 },
};
// clang-format on      


// clang-format off
MenuDef menuDefContextFav[] = {
    {_TRN("Remove from favorites"), CmdFavoriteDel, 0},
    { 0, 0, 0 }
};
// clang-format on
//
// clang-format off
//[ ACCESSKEY_GROUP File Menu
static MenuDef menuDefFile[] = {
    { _TRN("New &window\tCtrl+N"),          CmdNewWindow,              MF_REQ_DISK_ACCESS },
    { _TRN("&Open...\tCtrl+O"),             CmdOpen,                   MF_REQ_DISK_ACCESS },
    // TODO: should make it available for everyone?
    //{ "Open Folder",                        CmdOpenFolder,             MF_REQ_DISK_ACCESS },
    { _TRN("&Close\tCtrl+W"),               CmdClose,                  MF_REQ_DISK_ACCESS },
    { _TRN("Show in &folder"),              CmdShowInFolder,           MF_REQ_DISK_ACCESS },
    { _TRN("&Save As...\tCtrl+S"),          CmdSaveAs,                 MF_REQ_DISK_ACCESS },
    { _TRN("Save Annotations"),             CmdSaveAnnotations,        MF_REQ_DISK_ACCESS },
 //[ ACCESSKEY_ALTERNATIVE // only one of these two will be shown
#ifdef ENABLE_SAVE_SHORTCUT
    { _TRN("Save S&hortcut...\tCtrl+Shift+S"), Cmd::SaveAsBookmark,    MF_REQ_DISK_ACCESS | MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
//| ACCESSKEY_ALTERNATIVE
#else
    { _TRN("Re&name...\tF2"),               CmdRenameFile,             MF_REQ_DISK_ACCESS },
#endif
//] ACCESSKEY_ALTERNATIVE
    { _TRN("&Print...\tCtrl+P"),            CmdPrint,                  MF_REQ_PRINTER_ACCESS | MF_NOT_FOR_EBOOK_UI },
    { kMenuSeparator,                             0,                         MF_REQ_DISK_ACCESS },
//[ ACCESSKEY_ALTERNATIVE // PDF/XPS/CHM specific items are dynamically removed in RebuildFileMenu
    { _TRN("Open in &Adobe Reader"),        CmdOpenWithAcrobat,        MF_REQ_DISK_ACCESS | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Open in &Foxit Reader"),        CmdOpenWithFoxIt,          MF_REQ_DISK_ACCESS | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Open &in PDF-XChange"),         CmdOpenWithPdfXchange,     MF_REQ_DISK_ACCESS | MF_NOT_FOR_EBOOK_UI },
//| ACCESSKEY_ALTERNATIVE
    { _TRN("Open in &Microsoft XPS-Viewer"),CmdOpenWithXpsViewer,      MF_REQ_DISK_ACCESS | MF_NOT_FOR_EBOOK_UI },
//| ACCESSKEY_ALTERNATIVE
    { _TRN("Open in &Microsoft HTML Help"), CmdOpenWithHtmlHelp,       MF_REQ_DISK_ACCESS | MF_NOT_FOR_EBOOK_UI },
//] ACCESSKEY_ALTERNATIVE
    // further entries are added if specified in gGlobalPrefs.vecCommandLine
    { _TRN("Send by &E-mail..."),           CmdSendByEmail,            MF_REQ_DISK_ACCESS },
    { kMenuSeparator,                             0,                         MF_REQ_DISK_ACCESS },
    { _TRN("P&roperties\tCtrl+D"),          CmdProperties,             0 },
    { kMenuSeparator,                             0,                         0 },
    { _TRN("E&xit\tCtrl+Q"),                CmdExit,                   0 },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP File Menu

//[ ACCESSKEY_GROUP View Menu
static MenuDef menuDefView[] = {
    { _TRN("&Single Page\tCtrl+6"),         CmdViewSinglePage,        MF_NOT_FOR_CHM },
    { _TRN("&Facing\tCtrl+7"),              CmdViewFacing,            MF_NOT_FOR_CHM },
    { _TRN("&Book View\tCtrl+8"),           CmdViewBook,              MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Show &Pages Continuously"),     CmdViewContinuous,        MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    // TODO: "&Inverse Reading Direction" (since some Mangas might be read left-to-right)?
    { _TRN("Man&ga Mode"),                  CmdViewMangaMode,         MF_CBX_ONLY },
    { kMenuSeparator,                             0,                        MF_NOT_FOR_CHM },
    { _TRN("Rotate &Left\tCtrl+Shift+-"),   CmdViewRotateLeft,        MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Rotate &Right\tCtrl+Shift++"),  CmdViewRotateRight,       MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { kMenuSeparator,                             0,                        MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Pr&esentation\tF5"),            CmdViewPresentationMode,  MF_REQ_FULLSCREEN | MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { _TRN("F&ullscreen\tF11"),             CmdViewFullScreen,        MF_REQ_FULLSCREEN },
    { kMenuSeparator,                             0,                        MF_REQ_FULLSCREEN },
    { _TRN("Show Book&marks\tF12"),         CmdViewBookmarks,         0 },
    { _TRN("Show &Toolbar\tF8"),            CmdViewShowHideToolbar,   MF_NOT_FOR_EBOOK_UI },
    { _TRN("Show Scr&ollbars"),             CmdViewShowHideScrollbars,MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP View Menu

//[ ACCESSKEY_GROUP GoTo Menu
static MenuDef menuDefGoTo[] = {
    { _TRN("&Next Page\tRight Arrow"),      CmdGoToNextPage,         0 },
    { _TRN("&Previous Page\tLeft Arrow"),   CmdGoToPrevPage,         0 },
    { _TRN("&First Page\tHome"),            CmdGoToFirstPage,        0 },
    { _TRN("&Last Page\tEnd"),              CmdGoToLastPage,         0 },
    { _TRN("Pa&ge...\tCtrl+G"),             CmdGoToPage,             0 },
    { kMenuSeparator,                             0,                       0 },
    { _TRN("&Back\tAlt+Left Arrow"),        CmdGoToNavBack,          0 },
    { _TRN("F&orward\tAlt+Right Arrow"),    CmdGoToNavForward,       0 },
    { kMenuSeparator,                             0,                       MF_NOT_FOR_EBOOK_UI },
    { _TRN("Fin&d...\tCtrl+F"),             CmdFindFirst,            MF_NOT_FOR_EBOOK_UI },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP GoTo Menu

//[ ACCESSKEY_GROUP Zoom Menu
// the entire menu is MF_NOT_FOR_EBOOK_UI
static MenuDef menuDefZoom[] = {
    { _TRN("Fit &Page\tCtrl+0"),            CmdZoomFitPage,          MF_NOT_FOR_CHM },
    { _TRN("&Actual Size\tCtrl+1"),         CmdZoomActualSize,       MF_NOT_FOR_CHM },
    { _TRN("Fit &Width\tCtrl+2"),           CmdZoomFitWidth,         MF_NOT_FOR_CHM },
    { _TRN("Fit &Content\tCtrl+3"),         CmdZoomFitContent,       MF_NOT_FOR_CHM },
    { _TRN("Custom &Zoom...\tCtrl+Y"),      CmdZoomCustom,           0 },
    { kMenuSeparator,                             0,                       0 },
    { "6400%",                              CmdZoom6400,             MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { "3200%",                              CmdZoom3200,             MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { "1600%",                              CmdZoom1600,             MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { "800%",                               CmdZoom800,              MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { "400%",                               CmdZoom400,              MF_NO_TRANSLATE },
    { "200%",                               CmdZoom200,              MF_NO_TRANSLATE },
    { "150%",                               CmdZoom150,              MF_NO_TRANSLATE },
    { "125%",                               CmdZoom125,              MF_NO_TRANSLATE },
    { "100%",                               CmdZoom100,              MF_NO_TRANSLATE },
    { "50%",                                CmdZoom50,               MF_NO_TRANSLATE },
    { "25%",                                CmdZoom25,               MF_NO_TRANSLATE },
    { "12.5%",                              CmdZoom12_5,             MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { "8.33%",                              CmdZoom8_33,             MF_NO_TRANSLATE | MF_NOT_FOR_CHM },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Zoom Menu

//[ ACCESSKEY_GROUP Settings Menu
static MenuDef menuDefSettings[] = {
    { _TRN("Change Language"),              CmdChangeLanguage,        0 },
#if 0
    { _TRN("Contribute Translation"),       CmdContributeTranslation, MF_REQ_DISK_ACCESS },
    { kMenuSeparator,                             0,                        MF_REQ_DISK_ACCESS },
#endif
    { _TRN("&Options..."),                  CmdOptions,               MF_REQ_PREF_ACCESS },
    { _TRN("&Advanced Options..."),         CmdAdvancedOptions,       MF_REQ_PREF_ACCESS | MF_REQ_DISK_ACCESS },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Settings Menu

//[ ACCESSKEY_GROUP Favorites Menu
// the entire menu is MF_NOT_FOR_EBOOK_UI
MenuDef menuDefFavorites[] = {
    { _TRN("Add to favorites"),             CmdFavoriteAdd,                0 },
    { _TRN("Remove from favorites"),        CmdFavoriteDel,                0 },
    { _TRN("Show Favorites"),               CmdFavoriteToggle,             MF_REQ_DISK_ACCESS },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Favorites Menu

//[ ACCESSKEY_GROUP Help Menu
static MenuDef menuDefHelp[] = {
    { _TRN("Visit &Website"),               CmdHelpVisitWebsite,          MF_REQ_DISK_ACCESS },
    { _TRN("&Manual"),                      CmdHelpOpenManualInBrowser,   MF_REQ_DISK_ACCESS },
    { _TRN("Check for &Updates"),           CmdCheckUpdate,               MF_REQ_INET_ACCESS },
    { kMenuSeparator,                             0,                            MF_REQ_DISK_ACCESS },
    { _TRN("&About"),                       CmdHelpAbout,                 0 },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Help Menu

//[ ACCESSKEY_GROUP Debug Menu
static MenuDef menuDefDebug[] = {
    { _TRN("&Advanced Options..."),         CmdAdvancedOptions,       MF_REQ_PREF_ACCESS | MF_REQ_DISK_ACCESS },
    { "Highlight links",                    CmdDebugShowLinks,        MF_NO_TRANSLATE },
    { "Toggle ebook UI",                    CmdDebugEbookUI,          MF_NO_TRANSLATE },
    { "Mui debug paint",                    CmdDebugMui,              MF_NO_TRANSLATE },
    { "Annotation from Selection",          CmdDebugAnnotations,      MF_NO_TRANSLATE },
    { "Download symbols",                   CmdDebugDownloadSymbols,  MF_NO_TRANSLATE },
    { "Test app",                           CmdDebugTestApp,          MF_NO_TRANSLATE },
    { "Show notification",                  CmdDebugShowNotif,        MF_NO_TRANSLATE },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Debug Menu

//[ ACCESSKEY_GROUP Context Menu (Selection)
static MenuDef menuDefSelection[] = {
    { _TRN("&Translate With Google"),      CmdTranslateSelectionWithGoogle,  MF_REQ_ALLOW_COPY | MF_NOT_FOR_EBOOK_UI | MF_REQ_INET_ACCESS },
    { _TRN("Translate with &DeepL"),       CmdTranslateSelectionWithDeepL,   MF_REQ_ALLOW_COPY | MF_NOT_FOR_EBOOK_UI | MF_REQ_INET_ACCESS },
    { _TRN("&Search With Google"),         CmdSearchSelectionWithGoogle,     MF_REQ_ALLOW_COPY | MF_NOT_FOR_EBOOK_UI | MF_REQ_INET_ACCESS },
    { _TRN("Search With &Bing"),           CmdSearchSelectionWithBing,       MF_REQ_ALLOW_COPY | MF_NOT_FOR_EBOOK_UI | MF_REQ_INET_ACCESS },
    { _TRN("Select &All\tCtrl+A"),         CmdSelectAll,                     MF_REQ_ALLOW_COPY },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Context Menu (Selection)

//[ ACCESSKEY_GROUP Menu (Selection)
static MenuDef menuDefMainSelection[] = {
    { _TRN("&Copy To Clipboard\tCtrl-C"),  CmdCopySelection,                 MF_REQ_ALLOW_COPY | MF_NOT_FOR_EBOOK_UI },
    { _TRN("&Translate With Google"),      CmdTranslateSelectionWithGoogle,  MF_REQ_ALLOW_COPY | MF_NOT_FOR_EBOOK_UI | MF_REQ_INET_ACCESS },
    { _TRN("Translate with &DeepL"),       CmdTranslateSelectionWithDeepL,   MF_REQ_ALLOW_COPY | MF_NOT_FOR_EBOOK_UI | MF_REQ_INET_ACCESS },
    { _TRN("&Search With Google"),         CmdSearchSelectionWithGoogle,     MF_REQ_ALLOW_COPY | MF_NOT_FOR_EBOOK_UI | MF_REQ_INET_ACCESS },
    { _TRN("Search With &Bing"),           CmdSearchSelectionWithBing,       MF_REQ_ALLOW_COPY | MF_NOT_FOR_EBOOK_UI | MF_REQ_INET_ACCESS },
    { _TRN("Select &All\tCtrl+A"),         CmdSelectAll,                     MF_REQ_ALLOW_COPY },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Menu (Selection)

//[ ACCESSKEY_GROUP Context Menu (Create annot from selection)
static MenuDef menuDefCreateAnnotFromSelection[] = {
    { _TRN("&Highlight\ta"),    CmdCreateAnnotHighlight, 0 },
    { _TRN("&Underline"),       CmdCreateAnnotUnderline, 0 },
    { _TRN("&Strike Out"),      CmdCreateAnnotStrikeOut, 0 },
    { _TRN("S&quiggly"),        CmdCreateAnnotSquiggly,  0 },
    //{ _TRN("Redact"), CmdCreateAnnotRedact, 0 },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Context Menu (Create annot from selection)

//[ ACCESSKEY_GROUP Context Menu (Create annot under cursor)
static MenuDef menuDefCreateAnnotUnderCursor[] = {
    { _TRN("&Text"), CmdCreateAnnotText, 0 },
    { _TRN("&Free Text"), CmdCreateAnnotFreeText, 0 },
    { _TRN("&Stamp"), CmdCreateAnnotStamp, 0 },
    { _TRN("&Caret"), CmdCreateAnnotCaret, 0 },
    //{ _TRN("Ink"), CmdCreateAnnotInk, 0 },
    //{ _TRN("Square"), CmdCreateAnnotSquare, 0 },
    //{ _TRN("Circle"), CmdCreateAnnotCircle, 0 },
    //{ _TRN("Line"), CmdCreateAnnotLine, 0 },
    //{ _TRN("Polygon"), CmdCreateAnnotPolygon, 0 },
    //{ _TRN("Poly Line"), CmdCreateAnnotPolyLine, 0 },
    //{ _TRN("File Attachment"), CmdCreateAnnotFileAttachment, 0 },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Context Menu (Create annot under cursor)

//[ ACCESSKEY_GROUP Context Menu (Content)
// the entire menu is MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI
static MenuDef menuDefContext[] = {
    { _TRN("&Copy Selection \tCtrl-C"),         CmdCopySelection, MF_REQ_ALLOW_COPY | MF_NOT_FOR_EBOOK_UI },
    { _TRN("S&election"),                       (UINT_PTR)menuDefSelection, 0},
    { _TRN("Copy &Link Address"),               CmdCopyLinkTarget, MF_REQ_ALLOW_COPY },
    { _TRN("Copy Co&mment"),                    CmdCopyComment, MF_REQ_ALLOW_COPY },
    { _TRN("Copy &Image"),                      CmdCopyImage, MF_REQ_ALLOW_COPY },
    // note: strings cannot be "" or else items are not there
    { "add fav placeholder",                    CmdFavoriteAdd, MF_NO_TRANSLATE },
    { "del fav placeholder",                    CmdFavoriteDel, MF_NO_TRANSLATE },
    { _TRN("Show &Favorites"),                  CmdFavoriteToggle, 0 },
    { _TRN("Show &Bookmarks\tF12"),             CmdViewBookmarks, 0 },
    { _TRN("Show &Toolbar\tF8"),                CmdViewShowHideToolbar, MF_NOT_FOR_EBOOK_UI },
    { _TRN("Show &Scrollbars"),                 CmdViewShowHideScrollbars, MF_NOT_FOR_CHM | MF_NOT_FOR_EBOOK_UI },
    { _TRN("Save Annotations"),                 CmdSaveAnnotations, MF_REQ_DISK_ACCESS | MF_NEEDS_ANNOTS },
    { _TRN("Select Annotation in Editor"),      CmdSelectAnnotation, MF_REQ_DISK_ACCESS | MF_NEEDS_ANNOTS | MF_NEEDS_ANNOT_UNDER_CURSOR },
    { _TRN("Edit Annotations"),                 CmdEditAnnotations, MF_REQ_DISK_ACCESS | MF_NEEDS_ANNOTS },
    { _TRN("Create Annotation From Selection"), (UINT_PTR)menuDefCreateAnnotFromSelection, MF_NEEDS_SELECTION | MF_NEEDS_ANNOTS },
    { _TRN("Create Annotation Under Cursor"),   (UINT_PTR)menuDefCreateAnnotUnderCursor, MF_REQ_DISK_ACCESS | MF_NEEDS_ANNOTS | MF_NEEDS_CURSOR_ON_PAGE },
    { _TRN("E&xit Fullscreen"),                 CmdExitFullScreen, 0 },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Context Menu (Content)

//[ ACCESSKEY_GROUP Context Menu (Start)
static MenuDef menuDefContextStart[] = {
    { _TRN("&Open Document"),               CmdOpenSelectedDocument,   MF_REQ_DISK_ACCESS },
    { _TRN("&Pin Document"),                CmdPinSelectedDocument,    MF_REQ_DISK_ACCESS | MF_REQ_PREF_ACCESS },
    { kMenuSeparator,                             0,                         MF_REQ_DISK_ACCESS | MF_REQ_PREF_ACCESS },
    { _TRN("&Remove From History"),         CmdForgetSelectedDocument, MF_REQ_DISK_ACCESS | MF_REQ_PREF_ACCESS },
    { 0, 0, 0 },
};
//] ACCESSKEY_GROUP Context Menu (Start)
// clang-format on

HMENU BuildMenuFromMenuDef(MenuDef* menuDefs, HMENU menu, BuildMenuCtx* ctx) {
    CrashIf(!menu);
    bool wasSeparator = true;

    int i = 0;
    while (true) {
        MenuDef md = menuDefs[i];
        if (md.title == nullptr) {
            // sentinel
            break;
        }
        i++;

        if (!HasPermission((Perm)(md.flags >> kPermFlagOffset))) {
            continue;
        }

        // prevent two consecutive separators
        if (str::Eq(md.title, kMenuSeparator)) {
            if (!wasSeparator) {
                AppendMenuW(menu, MF_SEPARATOR, md.idOrSubmenu, nullptr);
            }
            wasSeparator = true;
            continue;
        }
        wasSeparator = false;

        if (ctx) {
            bool notForChm = MF_NOT_FOR_CHM == (md.flags & MF_NOT_FOR_CHM);
            bool notForEbook = MF_NOT_FOR_EBOOK_UI == (md.flags & MF_NOT_FOR_EBOOK_UI);
            bool cbxOnly = MF_CBX_ONLY == (md.flags & MF_CBX_ONLY);
            bool needsSelection = MF_NEEDS_SELECTION == (md.flags & MF_NEEDS_SELECTION);
            bool needsAnnots = MF_NEEDS_ANNOTS == (md.flags & MF_NEEDS_ANNOTS);
            bool needsAnnotUnderCursor = MF_NEEDS_ANNOT_UNDER_CURSOR == (md.flags & MF_NEEDS_ANNOT_UNDER_CURSOR);
            bool newsCursorOnPage = MF_NEEDS_CURSOR_ON_PAGE == (md.flags & MF_NEEDS_CURSOR_ON_PAGE);
            if (ctx->isChm && notForChm) {
                continue;
            }
            if (ctx->isEbookUI && notForEbook) {
                continue;
            }
            if (cbxOnly && !ctx->isCbx) {
                continue;
            }
            if (needsSelection && !ctx->hasSelection) {
                continue;
            }
            if (needsAnnots && !ctx->supportsAnnotations) {
                continue;
            }
            if (needsAnnotUnderCursor && !ctx->hasAnnotationUnderCursor) {
                continue;
            }
            if (newsCursorOnPage && !ctx->isCursorOnPage) {
                continue;
            }
        }

        bool noTranslate = MF_NO_TRANSLATE == (md.flags & MF_NO_TRANSLATE);
        AutoFreeWstr tmp;
        const WCHAR* title = nullptr;
        if (noTranslate) {
            tmp = strconv::Utf8ToWstr(md.title);
            title = tmp.Get();
        } else {
            title = trans::GetTranslation(md.title);
        }

        // hacky but works: small number is command id, large is submenu (a pointer)
        bool isSubMenu = md.idOrSubmenu > CmdLast + 10000;
        if (isSubMenu) {
            MenuDef* subMenuDef = (MenuDef*)md.idOrSubmenu;
            HMENU subMenu = BuildMenuFromMenuDef(subMenuDef, CreatePopupMenu(), ctx);
            AppendMenuW(menu, MF_ENABLED | MF_POPUP, (UINT_PTR)subMenu, title);
            continue;
        }
        AppendMenuW(menu, MF_STRING, md.idOrSubmenu, title);
    }

    // TODO: remove trailing separator if there ever is one
    CrashIf(wasSeparator);
    return menu;
}

static void AddFileMenuItem(HMENU menuFile, const WCHAR* filePath, int index) {
    CrashIf(!filePath || !menuFile);
    if (!filePath || !menuFile) {
        return;
    }

    AutoFreeWstr menuString;
    menuString.SetCopy(path::GetBaseNameNoFree(filePath));
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
    for (i = 0; i < FILE_HISTORY_MAX_RECENT; i++) {
        FileState* state = gFileHistory.Get(i);
        if (!state || state->isMissing) {
            break;
        }
        AddFileMenuItem(m, state->filePath, i);
    }

    if (i > 0) {
        InsertMenuW(m, CmdExit, MF_BYCOMMAND | MF_SEPARATOR, 0, nullptr);
    }
}

static void AppendExternalViewersToMenu(HMENU menuFile, const WCHAR* filePath) {
    if (0 == gGlobalPrefs->externalViewers->size()) {
        return;
    }
    if (!HasPermission(Perm::DiskAccess) || (filePath && !file::Exists(filePath))) {
        return;
    }

    int maxEntries = CmdOpenWithExternalLast - CmdOpenWithExternalFirst + 1;
    int count = 0;
    for (size_t i = 0; i < gGlobalPrefs->externalViewers->size() && count < maxEntries; i++) {
        ExternalViewer* ev = gGlobalPrefs->externalViewers->at(i);
        if (!ev->commandLine) {
            continue;
        }
        if (ev->filter && !str::Eq(ev->filter, L"*") && !(filePath && path::Match(filePath, ev->filter))) {
            continue;
        }

        AutoFreeWstr appName;
        const WCHAR* name = ev->name;
        if (str::IsEmpty(name)) {
            WStrVec args;
            ParseCmdLine(ev->commandLine, args, 2);
            if (args.size() == 0) {
                continue;
            }
            appName.SetCopy(path::GetBaseNameNoFree(args.at(0)));
            *(WCHAR*)path::GetExtNoFree(appName) = '\0';
        }

        AutoFreeWstr menuString(str::Format(_TR("Open in %s"), appName ? appName.Get() : name));
        uint menuId = CmdOpenWithExternalFirst + count;
        InsertMenuW(menuFile, CmdSendByEmail, MF_BYCOMMAND | MF_ENABLED | MF_STRING, menuId, menuString);
        if (!filePath) {
            win::menu::SetEnabled(menuFile, menuId, false);
        }
        count++;
    }
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
    int n = (int)dimof(gZoomMenuIds);
    for (int i = 0; i < n; i++) {
        if (virtualZoom == gZoomMenuIds[i].zoom) {
            return gZoomMenuIds[i].itemId;
        }
    }
    return CmdZoomCustom;
}

float ZoomMenuItemToZoom(int menuItemId) {
    for (int i = 0; i < dimof(gZoomMenuIds); i++) {
        if (menuItemId == gZoomMenuIds[i].itemId) {
            return gZoomMenuIds[i].zoom;
        }
    }
    CrashIf(true);
    return 100.0;
}

static void ZoomMenuItemCheck(HMENU m, int menuItemId, bool canZoom) {
    CrashIf((CmdZoomFirst > menuItemId) || (menuItemId > CmdZoomLast));

    for (int i = 0; i < dimof(gZoomMenuIds); i++) {
        win::menu::SetEnabled(m, gZoomMenuIds[i].itemId, canZoom);
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

// clang-format off
// those menu items will be disabled if no document is opened, enabled otherwise
static int menusToDisableIfNoDocument[] = {
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

static int menusToDisableIfDirectoryOrBrokenPDF[] = {
    CmdRenameFile,
    CmdSendByEmail,
    CmdOpenWithAcrobat,
    CmdOpenWithFoxIt,
    CmdOpenWithPdfXchange,
    CmdShowInFolder,
};

static int menusToDisableIfNoSelection[] = {
    CmdCopySelection,
    CmdTranslateSelectionWithDeepL,
    CmdTranslateSelectionWithGoogle,
    CmdSearchSelectionWithBing,
    CmdSearchSelectionWithGoogle,
};
// clang-format on

static void SetMenuStateForSelection(TabInfo* tab, HMENU menu) {
    bool isTextSelected = tab && tab->win && tab->win->showSelection && tab->selectionOnPage;
    for (int i = 0; i < dimof(menusToDisableIfNoSelection); i++) {
        int id = menusToDisableIfNoSelection[i];
        win::menu::SetEnabled(menu, id, isTextSelected);
    }
}

static void MenuUpdateStateForWindow(WindowInfo* win) {
    TabInfo* tab = win->currentTab;

    bool hasDocument = tab && tab->IsDocLoaded();
    for (int i = 0; i < dimof(menusToDisableIfNoDocument); i++) {
        int id = menusToDisableIfNoDocument[i];
        win::menu::SetEnabled(win->menu, id, hasDocument);
    }

    SetMenuStateForSelection(tab, win->menu);

    // TODO: happens with UseTabs = false with .pdf files
    SubmitCrashIf(IsFileCloseMenuEnabled() == win->IsAboutWindow());
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
        for (int i = 0; i < dimof(menusToDisableIfDirectoryOrBrokenPDF); i++) {
            int id = menusToDisableIfDirectoryOrBrokenPDF[i];
            win::menu::SetEnabled(win->menu, id, false);
        }
    } else if (fileExists && CouldBePDFDoc(tab)) {
        for (int i = 0; i < dimof(menusToDisableIfDirectoryOrBrokenPDF); i++) {
            int id = menusToDisableIfDirectoryOrBrokenPDF[i];
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
    win::menu::SetChecked(win->menu, CmdDebugEbookUI, gGlobalPrefs->ebookUI.useFixedPageUI);
    win::menu::SetChecked(win->menu, CmdDebugMui, mui::IsDebugPaint());
    win::menu::SetEnabled(win->menu, CmdDebugAnnotations,
                          tab && tab->selectionOnPage && win->showSelection && EngineSupportsAnnotations(engine));
}

void OnAboutContextMenu(WindowInfo* win, int x, int y) {
    if (!HasPermission(Perm::SavePreferences | Perm::DiskAccess) || !gGlobalPrefs->rememberOpenedFiles ||
        !gGlobalPrefs->showStartPage) {
        return;
    }

    const WCHAR* filePath = GetStaticLink(win->staticLinks, x, y, nullptr);
    if (!filePath || *filePath == '<' || str::StartsWith(filePath, L"http://") ||
        str::StartsWith(filePath, L"https://")) {
        return;
    }

    FileState* state = gFileHistory.Find(filePath, nullptr);
    CrashIf(!state);
    if (!state) {
        return;
    }

    HMENU popup = BuildMenuFromMenuDef(menuDefContextStart, CreatePopupMenu(), 0);
    win::menu::SetChecked(popup, CmdPinSelectedDocument, state->isPinned);
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
        state->isPinned = !state->isPinned;
        win->HideToolTip();
        win->RedrawAll(true);
        return;
    }

    if (CmdForgetSelectedDocument == cmd) {
        if (state->favorites->size() > 0) {
            // just hide documents with favorites
            gFileHistory.MarkFileInexistent(state->filePath, true);
        } else {
            gFileHistory.Remove(state);
            DeleteDisplayState(state);
        }
        CleanUpThumbnailCache(gFileHistory);
        win->HideToolTip();
        win->RedrawAll(true);
        return;
    }
}

void FillBuildMenuCtx(TabInfo* tab, BuildMenuCtx* ctx, Point pt) {
    if (!tab) {
        return;
    }
    if (tab->AsChm()) {
        ctx->isChm = true;
    }
    if (tab->AsEbook()) {
        ctx->isEbookUI = true;
    }
    EngineBase* engine = tab->GetEngine();
    if (engine && (engine->kind == kindEngineComicBooks)) {
        ctx->isCbx = true;
    }
    ctx->supportsAnnotations = EngineSupportsAnnotations(engine) && !tab->win->isFullScreen;

    DisplayModel* dm = tab->AsFixed();
    if (dm) {
        int pageNoUnderCursor = dm->GetPageNoByPoint(pt);
        if (pageNoUnderCursor > 0) {
            ctx->isCursorOnPage = true;
        }
        auto annotUnderCursor = dm->GetAnnotationAtPos(pt, nullptr);
        if (annotUnderCursor) {
            ctx->hasAnnotationUnderCursor = true;
        }
    }
    ctx->hasSelection = tab->win->showSelection && tab->selectionOnPage;
}

void OnWindowContextMenu(WindowInfo* win, int x, int y) {
    DisplayModel* dm = win->AsFixed();
    CrashIf(!dm);
    if (!dm) {
        return;
    }

    TabInfo* tab = win->currentTab;
    IPageElement* pageEl = dm->GetElementAtPos({x, y});
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
    Annotation* annotUnderCursor{nullptr};

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

    const WCHAR* filePath = win->ctrl->FilePath();
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

    // if toolbar is not shown, add option to show it
    if (gGlobalPrefs->showToolbar) {
        win::menu::Remove(popup, CmdViewShowHideToolbar);
    }

    POINT pt = {x, y};
    MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
    MarkMenuOwnerDraw(popup);
    UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    int cmd = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);

    AnnotationType annotType = (AnnotationType)(cmd - CmdCreateAnnotText);
    Annotation* createdAnnot{nullptr};
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
            CrashIf(!annotUnderCursor);
            SelectAnnotationInEditWindow(tab->editAnnotsWindow, annotUnderCursor);
            break;
        case CmdEditAnnotations:
            StartEditAnnotations(tab, nullptr);
            SelectAnnotationInEditWindow(tab->editAnnotsWindow, annotUnderCursor);
            break;
        case CmdCopyLinkTarget: {
            WCHAR* tmp = CleanupFileURL(value);
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
            createdAnnot = EnginePdfCreateAnnotation(engine, annotType, pageNoUnderCursor, ptOnPage);
            if (createdAnnot) {
                WindowInfoRerender(win);
                ToolbarUpdateStateForWindow(win, true);
            }
        } break;
        case CmdCreateAnnotHighlight:
            createdAnnot = MakeAnnotationFromSelection(tab, AnnotationType::Highlight);
            break;
        case CmdCreateAnnotSquiggly:
            createdAnnot = MakeAnnotationFromSelection(tab, AnnotationType::Squiggly);
            break;
        case CmdCreateAnnotStrikeOut:
            createdAnnot = MakeAnnotationFromSelection(tab, AnnotationType::StrikeOut);
            break;
        case CmdCreateAnnotUnderline:
            createdAnnot = MakeAnnotationFromSelection(tab, AnnotationType::Underline);
            break;
        case CmdCreateAnnotInk:
        case CmdCreateAnnotPolyLine:
            // TODO: implement me
            break;
    }
    if (createdAnnot) {
        StartEditAnnotations(tab, createdAnnot);
    }
    delete annotUnderCursor;

    /*
        { _TR_TODON("Line"), CmdCreateAnnotLine, 0 },
        { _TR_TODON("Highlight"), CmdCreateAnnotHighlight, 0 },
        { _TR_TODON("Underline"), CmdCreateAnnotUnderline, 0 },
        { _TR_TODON("Strike Out"), CmdCreateAnnotStrikeOut, 0 },
        { _TR_TODON("Squiggly"), CmdCreateAnnotSquiggly, 0 },
        { _TR_TODON("File Attachment"), CmdCreateAnnotFileAttachment, 0 },
        { _TR_TODON("Redact"), CmdCreateAnnotRedact, 0 },
    */
    // TODO: those require creating
    /*
        { _TR_TODON("Polygon"), CmdCreateAnnotPolygon, 0 },
        { _TR_TODON("Poly Line"), CmdCreateAnnotPolyLine, 0 },
    */

    delete pageEl;
}

static void RebuildFileMenu(TabInfo* tab, HMENU menu) {
    win::menu::Empty(menu);
    BuildMenuCtx buildCtx;
    FillBuildMenuCtx(tab, &buildCtx, Point{0, 0});
    HMENU m = BuildMenuFromMenuDef(menuDefFile, menu, &buildCtx);
    AppendRecentFilesToMenu(menu);
    AppendExternalViewersToMenu(menu, tab ? tab->filePath.Get() : nullptr);

    // Suppress menu items that depend on specific software being installed:
    // e-mail client, Adobe Reader, Foxit, PDF-XChange
    // Don't hide items here that won't always be hidden
    // (MenuUpdateStateForWindow() is for that)
    if (!CanSendAsEmailAttachment()) {
        win::menu::Remove(menu, CmdSendByEmail);
    }

    for (int cmd = CmdOpenWithFirst + 1; cmd < CmdOpenWithLast; cmd++) {
        if (!CanViewWithKnownExternalViewer(tab, cmd)) {
            win::menu::Remove(menu, cmd);
        }
    }

    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    EngineBase* engine = tab ? tab->GetEngine() : nullptr;
    bool enableSaveAnnotations = EngineHasUnsavedAnnotations(engine);
    win::menu::SetEnabled(menu, CmdSaveAnnotations, enableSaveAnnotations);
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
void MenuOwnerDrawnDrawItem([[maybe_unused]] HWND hwnd, DRAWITEMSTRUCT* dis) {
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

//[ ACCESSKEY_GROUP Main Menubar
HMENU BuildMenu(WindowInfo* win) {
    TabInfo* tab = win->currentTab;
    HMENU mainMenu = CreateMenu();

    BuildMenuCtx buildCtx;
    FillBuildMenuCtx(tab, &buildCtx, Point{0, 0});

    HMENU m = CreateMenu();
    RebuildFileMenu(tab, m);
    AppendMenuW(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&File"));
    m = BuildMenuFromMenuDef(menuDefView, CreateMenu(), &buildCtx);
    AppendMenuW(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&View"));
    m = BuildMenuFromMenuDef(menuDefGoTo, CreateMenu(), &buildCtx);
    AppendMenuW(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Go To"));
    if (!win->AsEbook()) {
        m = BuildMenuFromMenuDef(menuDefZoom, CreateMenu(), &buildCtx);
        AppendMenuW(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Zoom"));
    }
    if (!win->AsEbook()) {
        m = BuildMenuFromMenuDef(menuDefSelection, CreateMenu(), &buildCtx);
        AppendMenuW(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("S&election"));
    }

    // TODO: implement Favorites for ebooks
    if (HasPermission(Perm::SavePreferences) && !win->AsEbook()) {
        // I think it makes sense to disable favorites in restricted mode
        // because they wouldn't be persisted, anyway
        m = BuildMenuFromMenuDef(menuDefFavorites, CreateMenu(), &buildCtx);
        RebuildFavMenu(win, m);
        AppendMenuW(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("F&avorites"));
    }

    m = BuildMenuFromMenuDef(menuDefSettings, CreateMenu(), &buildCtx);
#if defined(ENABLE_THEME)
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
    AppendMenuW(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Settings"));

    m = BuildMenuFromMenuDef(menuDefHelp, CreateMenu(), &buildCtx);
    AppendMenuW(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Help"));
#if 0
    // see MenuBarAsPopupMenu in Caption.cpp
    m = GetSystemMenu(win->hwndFrame, FALSE);
    AppendMenuW(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _TR("&Window"));
#endif

    if (gShowDebugMenu) {
        m = BuildMenuFromMenuDef(menuDefDebug, CreateMenu(), &buildCtx);
        win::menu::Remove(m, CmdAdvancedOptions); // TODO: this was removed for !ramicro build

        if (!gIsDebugBuild) {
            RemoveMenu(m, CmdDebugTestApp, MF_BYCOMMAND);
        }

        if (gAddCrashMeMenu) {
            AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
            AppendMenuA(m, MF_STRING, (UINT_PTR)CmdDebugCrashMe, "Crash me");
        }

        AppendMenuW(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, L"Debug");
    }

    MarkMenuOwnerDraw(mainMenu);
    return mainMenu;
}
//] ACCESSKEY_GROUP Main Menubar

void UpdateAppMenu(WindowInfo* win, HMENU m) {
    CrashIf(!win);
    if (!win) {
        return;
    }
    int id = (int)GetMenuItemID(m, 0);
    if (id == menuDefFile[0].idOrSubmenu) {
        RebuildFileMenu(win->currentTab, m);
    } else if (id == menuDefFavorites[0].idOrSubmenu) {
        win::menu::Empty(m);
        BuildMenuFromMenuDef(menuDefFavorites, m, 0);
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
