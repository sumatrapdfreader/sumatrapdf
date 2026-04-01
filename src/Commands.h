/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// @gen-start cmd-enum
// clang-format off
enum {
    // commands are integers sent with WM_COMMAND so start them
    // at some number higher than 0
    CmdFirst = 200,
    CmdSeparator = CmdFirst,

    CmdOpenFile = 201, CmdClose = 202, CmdCloseCurrentDocument = 203,
    CmdCloseOtherTabs = 204, CmdCloseTabsToTheRight = 205, CmdCloseTabsToTheLeft = 206,
    CmdCloseAllTabs = 207, CmdSaveAs = 208, CmdPrint = 209,
    CmdShowInFolder = 210, CmdRenameFile = 211, CmdDeleteFile = 212,
    CmdExit = 213, CmdReloadDocument = 214, CmdCreateShortcutToFile = 215,
    CmdSendByEmail = 216, CmdProperties = 217, CmdSinglePageView = 218,
    CmdFacingView = 219, CmdBookView = 220, CmdToggleContinuousView = 221,
    CmdToggleMangaMode = 222, CmdRotateLeft = 223, CmdRotateRight = 224,
    CmdToggleBookmarks = 225, CmdToggleTableOfContents = 226, CmdToggleFullscreen = 227,
    CmdPresentationWhiteBackground = 228, CmdPresentationBlackBackground = 229, CmdTogglePresentationMode = 230,
    CmdToggleToolbar = 231, CmdToggleScrollbars = 232, CmdToggleOverlayScrollbar = 233,
    CmdToggleMenuBar = 234, CmdToggleUseTabs = 235, CmdCopySelection = 236,
    CmdTranslateSelectionWithGoogle = 237, CmdTranslateSelectionWithDeepL = 238, CmdSearchSelectionWithGoogle = 239,
    CmdSearchSelectionWithBing = 240, CmdSearchSelectionWithWikipedia = 241, CmdSearchSelectionWithGoogleScholar = 242,
    CmdSelectAll = 243, CmdNewWindow = 244, CmdDuplicateInNewWindow = 245,
    CmdDuplicateInNewTab = 246, CmdCopyImage = 247, CmdCopyLinkTarget = 248,
    CmdCopyComment = 249, CmdCopyFilePath = 250, CmdScrollUp = 251,
    CmdScrollDown = 252, CmdScrollLeft = 253, CmdScrollRight = 254,
    CmdScrollLeftPage = 255, CmdScrollRightPage = 256, CmdScrollUpPage = 257,
    CmdScrollDownPage = 258, CmdScrollDownHalfPage = 259, CmdScrollUpHalfPage = 260,
    CmdGoToNextPage = 261, CmdGoToPrevPage = 262, CmdGoToFirstPage = 263,
    CmdGoToLastPage = 264, CmdGoToPage = 265, CmdFindFirst = 266,
    CmdFindNext = 267, CmdFindPrev = 268, CmdFindNextSel = 269,
    CmdFindPrevSel = 270, CmdFindToggleMatchCase = 271, CmdSaveAnnotations = 272,
    CmdSaveAnnotationsNewFile = 273, CmdEditAnnotations = 274, CmdDeleteAnnotation = 275,
    CmdZoomFitPage = 276, CmdZoomActualSize = 277, CmdZoomFitWidth = 278,
    CmdZoom6400 = 279, CmdZoom3200 = 280, CmdZoom1600 = 281,
    CmdZoom800 = 282, CmdZoom400 = 283, CmdZoom200 = 284,
    CmdZoom150 = 285, CmdZoom125 = 286, CmdZoom100 = 287,
    CmdZoom50 = 288, CmdZoom25 = 289, CmdZoom12_5 = 290,
    CmdZoom8_33 = 291, CmdZoomFitContent = 292, CmdZoomCustom = 293,
    CmdZoomIn = 294, CmdZoomOut = 295, CmdZoomFitWidthAndContinuous = 296,
    CmdZoomFitPageAndSinglePage = 297, CmdContributeTranslation = 298, CmdOpenWithKnownExternalViewerFirst = 299,
    CmdOpenWithExplorer = 300, CmdOpenWithDirectoryOpus = 301, CmdOpenWithTotalCommander = 302,
    CmdOpenWithDoubleCommander = 303, CmdOpenWithAcrobat = 304, CmdOpenWithFoxIt = 305,
    CmdOpenWithFoxItPhantom = 306, CmdOpenWithPdfXchange = 307, CmdOpenWithXpsViewer = 308,
    CmdOpenWithHtmlHelp = 309, CmdOpenWithPdfDjvuBookmarker = 310, CmdOpenWithKnownExternalViewerLast = 311,
    CmdOpenSelectedDocument = 312, CmdPinSelectedDocument = 313, CmdForgetSelectedDocument = 314,
    CmdExpandAll = 315, CmdCollapseAll = 316, CmdSaveEmbeddedFile = 317,
    CmdOpenEmbeddedPDF = 318, CmdSaveAttachment = 319, CmdOpenAttachment = 320,
    CmdOptions = 321, CmdAdvancedOptions = 322, CmdAdvancedSettings = 323,
    CmdChangeLanguage = 324, CmdCheckUpdate = 325, CmdHelpOpenManual = 326,
    CmdHelpOpenManualOnWebsite = 327, CmdHelpOpenKeyboardShortcuts = 328, CmdHelpVisitWebsite = 329,
    CmdHelpAbout = 330, CmdMoveFrameFocus = 331, CmdFavoriteAdd = 332,
    CmdFavoriteDel = 333, CmdFavoriteToggle = 334, CmdToggleLinks = 335,
    CmdToggleShowAnnotations = 336, CmdShowAnnotations = 337, CmdHideAnnotations = 338,
    CmdCreateAnnotText = 339, CmdCreateAnnotLink = 340, CmdCreateAnnotFreeText = 341,
    CmdCreateAnnotLine = 342, CmdCreateAnnotSquare = 343, CmdCreateAnnotCircle = 344,
    CmdCreateAnnotPolygon = 345, CmdCreateAnnotPolyLine = 346, CmdCreateAnnotHighlight = 347,
    CmdCreateAnnotUnderline = 348, CmdCreateAnnotSquiggly = 349, CmdCreateAnnotStrikeOut = 350,
    CmdCreateAnnotRedact = 351, CmdCreateAnnotStamp = 352, CmdCreateAnnotCaret = 353,
    CmdCreateAnnotInk = 354, CmdCreateAnnotPopup = 355, CmdCreateAnnotFileAttachment = 356,
    CmdInvertColors = 357, CmdTogglePageInfo = 358, CmdToggleZoom = 359,
    CmdNavigateBack = 360, CmdNavigateForward = 361, CmdToggleCursorPosition = 362,
    CmdOpenNextFileInFolder = 363, CmdOpenPrevFileInFolder = 364, CmdCommandPalette = 365,
    CmdShowLog = 366, CmdShowPdfInfo = 367, CmdShowErrors = 368,
    CmdClearHistory = 369, CmdReopenLastClosedFile = 370, CmdNextTab = 371,
    CmdPrevTab = 372, CmdNextTabSmart = 373, CmdPrevTabSmart = 374,
    CmdMoveTabLeft = 375, CmdMoveTabRight = 376, CmdSelectNextTheme = 377,
    CmdToggleFrequentlyRead = 378, CmdInvokeInverseSearch = 379, CmdExec = 380,
    CmdViewWithExternalViewer = 381, CmdSelectionHandler = 382, CmdSetTheme = 383,
    CmdToggleInverseSearch = 384, CmdDebugCorruptMemory = 385, CmdDebugCrashMe = 386,
    CmdDebugDownloadSymbols = 387, CmdDebugTestApp = 388, CmdDebugShowNotif = 389,
    CmdDebugStartStressTest = 390, CmdDebugTogglePredictiveRender = 391, CmdDebugToggleRtl = 392,
    CmdToggleAntiAlias = 393, CmdToggleSmoothScroll = 394, CmdToggleHideScrollbar = 395,
    CmdToggleScrollbarInSinglePage = 396, CmdToggleLazyLoading = 397, CmdListPrinters = 398,
    CmdToggleWindowsPreviewer = 399, CmdToggleWindowsSearchFilter = 400, CmdScreenshot = 401,
    CmdCropImage = 402, CmdResizeImage = 403, CmdSaveImage = 404,
    CmdPasteClipboardImage = 405, CmdNone = 406,

    /* range for file history */
    CmdFileHistoryFirst,
    CmdFileHistoryLast = CmdFileHistoryFirst + 32,

    /* range for favorites */
    CmdFavoriteFirst,
    CmdFavoriteLast = CmdFavoriteFirst + 256,

    CmdLast = CmdFavoriteLast,
    CmdFirstCustom = CmdLast + 100,

    // aliases, at the end to not mess ordering
    CmdViewLayoutFirst = CmdSinglePageView,
    CmdViewLayoutLast = CmdToggleMangaMode,

    CmdZoomFirst = CmdZoomFitPage,
    CmdZoomLast = CmdZoomCustom,

    CmdCreateAnnotFirst = CmdCreateAnnotText,
    CmdCreateAnnotLast = CmdCreateAnnotFileAttachment,
};
// clang-format on
// @gen-end cmd-enum

// order of CreateAnnot* must be the same as enum AnnotationType
/*
TOOD: maybe add commands for those annotations
Sound,
Movie,
Widget,
Screen,
PrinterMark,
TrapNet,
Watermark,
ThreeD,
*/

struct CommandArg {
    enum class Type : u16 {
        None,
        Bool,
        Int,
        Float,
        String,
        Color,
    };

    // arguments are a linked list for simplicity
    struct CommandArg* next = nullptr;

    Type type = Type::None;

    // TODO: we have a fixed number of argument names
    // we could use seqstrings and use u16 for arg name id
    const char* name = nullptr;

    // TODO: could be a union
    const char* strVal = nullptr;
    bool boolVal = false;
    int intVal = 0;
    float floatVal = 0.0;
    ParsedColor colorVal;

    CommandArg() = default;
    ~CommandArg();
};

void FreeCommandArgs(CommandArg* first);

struct CustomCommand {
    // all commands are stored as linked list
    struct CustomCommand* next = nullptr;

    // the command id like CmdOpenFile
    int origId = 0;

    // for debugging, the full definition of the command
    // as given by the user
    const char* definition = nullptr;

    // optional name, if given this shows up in command palette
    const char* name = nullptr;

    // optional keyboard shortcut
    const char* key = nullptr;

    // a unique command id generated by us, starting with CmdFirstCustom
    // it identifies a command with their fixed set of arguments
    int id = 0;

    // optional
    const char* idStr = nullptr;

    CommandArg* firstArg = nullptr;
    CustomCommand() = default;
    ~CustomCommand();
};

extern CustomCommand* gFirstCustomCommand;
extern SeqStrings gCommandDescriptions;

int GetCommandIdByName(const char*);
int GetCommandIdByDesc(const char*);

CustomCommand* CreateCustomCommand(const char* definition, int origCmdId, CommandArg* args);
CustomCommand* FindCustomCommand(int cmdId);
void FreeCustomCommands();
CommandArg* NewStringArg(const char* name, const char* val);
CommandArg* NewFloatArg(const char* name, float val);
void InsertArg(CommandArg** firstPtr, CommandArg* arg);

CustomCommand* CreateCommandFromDefinition(const char* definition);
CommandArg* GetCommandArg(CustomCommand*, const char* argName);
int GetCommandIntArg(CustomCommand* cmd, const char* name, int defValue);
bool GetCommandBoolArg(CustomCommand* cmd, const char* name, bool defValue);
const char* GetCommandStringArg(CustomCommand* cmd, const char* name, const char* defValue);
void GetCommandsWithOrigId(Vec<CustomCommand*>& commands, int origId);

constexpr const char* kCmdArgColor = "color";
constexpr const char* kCmdArgBgColor = "bgcolor";
constexpr const char* kCmdArgOpacity = "opacity";
constexpr const char* kCmdArgOpenEdit = "openedit";
constexpr const char* kCmdArgTextSize = "textsize";
constexpr const char* kCmdArgBorderWidth = "borderwidth";
constexpr const char* kCmdArgInteriorColor = "interiorcolor";

constexpr const char* kCmdArgCopyToClipboard = "copytoclipboard";
constexpr const char* kCmdArgSetContent = "setcontent";
constexpr const char* kCmdArgExe = "exe";
constexpr const char* kCmdArgURL = "url";
constexpr const char* kCmdArgLevel = "level";
constexpr const char* kCmdArgFilter = "filter";
constexpr const char* kCmdArgN = "n";
constexpr const char* kCmdArgMode = "mode";
constexpr const char* kCmdArgTheme = "theme";
constexpr const char* kCmdArgCommandLine = "cmdline";
constexpr const char* kCmdArgToolbarText = "toolbartext";
constexpr const char* kCmdArgFocusEdit = "focusedit";
constexpr const char* kCmdArgFocusList = "focuslist";
