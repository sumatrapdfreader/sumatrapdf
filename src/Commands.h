/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// @gen-start cmd-enum
// clang-format off
enum {
    // commands are integers sent with WM_COMMAND so start them
    // at some number higher than 0
    CmdFirst = 200,
    CmdSeparator = CmdFirst,

    CmdOpenFile = 201,
    CmdClose = 202,
    CmdCloseCurrentDocument = 203,
    CmdCloseOtherTabs = 204,
    CmdCloseTabsToTheRight = 205,
    CmdCloseTabsToTheLeft = 206,
    CmdCloseAllTabs = 207,
    CmdSaveAs = 208,
    CmdPrint = 209,
    CmdShowInFolder = 210,
    CmdRenameFile = 211,
    CmdDeleteFile = 212,
    CmdExit = 213,
    CmdReloadDocument = 214,
    CmdCreateShortcutToFile = 215,
    CmdSendByEmail = 216,
    CmdProperties = 217,
    CmdSinglePageView = 218,
    CmdFacingView = 219,
    CmdBookView = 220,
    CmdToggleContinuousView = 221,
    CmdToggleMangaMode = 222,
    CmdRotateLeft = 223,
    CmdRotateRight = 224,
    CmdToggleBookmarks = 225,
    CmdToggleTableOfContents = 226,
    CmdToggleFullscreen = 227,
    CmdPresentationWhiteBackground = 228,
    CmdPresentationBlackBackground = 229,
    CmdTogglePresentationMode = 230,
    CmdToggleToolbar = 231,
    CmdChangeScrollbar = 232,
    CmdToggleMenuBar = 233,
    CmdToggleUseTabs = 234,
    CmdToggleTabsMru = 235,
    CmdCopySelection = 236,
    CmdTranslateSelectionWithGoogle = 237,
    CmdTranslateSelectionWithDeepL = 238,
    CmdSearchSelectionWithGoogle = 239,
    CmdSearchSelectionWithBing = 240,
    CmdSearchSelectionWithWikipedia = 241,
    CmdSearchSelectionWithGoogleScholar = 242,
    CmdSelectAll = 243,
    CmdNewWindow = 244,
    CmdDuplicateInNewWindow = 245,
    CmdDuplicateInNewTab = 246,
    CmdCopyImage = 247,
    CmdCopyLinkTarget = 248,
    CmdCopyComment = 249,
    CmdCopyFilePath = 250,
    CmdScrollUp = 251,
    CmdScrollDown = 252,
    CmdScrollLeft = 253,
    CmdScrollRight = 254,
    CmdScrollLeftPage = 255,
    CmdScrollRightPage = 256,
    CmdScrollUpPage = 257,
    CmdScrollDownPage = 258,
    CmdScrollDownHalfPage = 259,
    CmdScrollUpHalfPage = 260,
    CmdGoToNextPage = 261,
    CmdGoToPrevPage = 262,
    CmdGoToFirstPage = 263,
    CmdGoToLastPage = 264,
    CmdGoToPage = 265,
    CmdFindFirst = 266,
    CmdFindNext = 267,
    CmdFindPrev = 268,
    CmdFindNextSel = 269,
    CmdFindPrevSel = 270,
    CmdFindToggleMatchCase = 271,
    CmdSaveAnnotations = 272,
    CmdSaveAnnotationsNewFile = 273,
    CmdEditAnnotations = 274,
    CmdDeleteAnnotation = 275,
    CmdZoomFitPage = 276,
    CmdZoomActualSize = 277,
    CmdZoomFitWidth = 278,
    CmdZoomFitByOrientation = 279,
    CmdZoom6400 = 280,
    CmdZoom3200 = 281,
    CmdZoom1600 = 282,
    CmdZoom800 = 283,
    CmdZoom400 = 284,
    CmdZoom200 = 285,
    CmdZoom150 = 286,
    CmdZoom125 = 287,
    CmdZoom100 = 288,
    CmdZoom50 = 289,
    CmdZoom25 = 290,
    CmdZoom12_5 = 291,
    CmdZoom8_33 = 292,
    CmdZoomFitContent = 293,
    CmdZoomShrinkToFit = 294,
    CmdZoomCustom = 295,
    CmdZoomIn = 296,
    CmdZoomOut = 297,
    CmdZoomFitWidthAndContinuous = 298,
    CmdZoomFitPageAndSinglePage = 299,
    CmdContributeTranslation = 300,
    CmdOpenWithKnownExternalViewerFirst = 301,
    CmdOpenWithExplorer = 302,
    CmdOpenWithDirectoryOpus = 303,
    CmdOpenWithTotalCommander = 304,
    CmdOpenWithDoubleCommander = 305,
    CmdOpenWithAcrobat = 306,
    CmdOpenWithFoxIt = 307,
    CmdOpenWithFoxItPhantom = 308,
    CmdOpenWithPdfXchange = 309,
    CmdOpenWithXpsViewer = 310,
    CmdOpenWithHtmlHelp = 311,
    CmdOpenWithPdfDjvuBookmarker = 312,
    CmdOpenWithKnownExternalViewerLast = 313,
    CmdOpenSelectedDocument = 314,
    CmdPinSelectedDocument = 315,
    CmdForgetSelectedDocument = 316,
    CmdExpandAll = 317,
    CmdCollapseAll = 318,
    CmdSaveEmbeddedFile = 319,
    CmdOpenEmbeddedPDF = 320,
    CmdSaveAttachment = 321,
    CmdOpenAttachment = 322,
    CmdOptions = 323,
    CmdAdvancedOptions = 324,
    CmdAdvancedSettings = 325,
    CmdChangeLanguage = 326,
    CmdCheckUpdate = 327,
    CmdHelpOpenManual = 328,
    CmdHelpOpenManualOnWebsite = 329,
    CmdHelpOpenKeyboardShortcuts = 330,
    CmdHelpVisitWebsite = 331,
    CmdHelpAbout = 332,
    CmdMoveFrameFocus = 333,
    CmdFavoriteAdd = 334,
    CmdFavoriteDel = 335,
    CmdFavoriteToggle = 336,
    CmdToggleLinks = 337,
    CmdToggleShowAnnotations = 338,
    CmdShowAnnotations = 339,
    CmdHideAnnotations = 340,
    CmdCreateAnnotText = 341,
    CmdCreateAnnotLink = 342,
    CmdCreateAnnotFreeText = 343,
    CmdCreateAnnotLine = 344,
    CmdCreateAnnotSquare = 345,
    CmdCreateAnnotCircle = 346,
    CmdCreateAnnotPolygon = 347,
    CmdCreateAnnotPolyLine = 348,
    CmdCreateAnnotHighlight = 349,
    CmdCreateAnnotUnderline = 350,
    CmdCreateAnnotSquiggly = 351,
    CmdCreateAnnotStrikeOut = 352,
    CmdCreateAnnotRedact = 353,
    CmdCreateAnnotStamp = 354,
    CmdCreateAnnotCaret = 355,
    CmdCreateAnnotInk = 356,
    CmdCreateAnnotPopup = 357,
    CmdCreateAnnotFileAttachment = 358,
    CmdInvertColors = 359,
    CmdTogglePageInfo = 360,
    CmdToggleZoom = 361,
    CmdNavigateBack = 362,
    CmdNavigateForward = 363,
    CmdToggleCursorPosition = 364,
    CmdOpenNextFileInFolder = 365,
    CmdOpenPrevFileInFolder = 366,
    CmdCommandPalette = 367,
    CmdShowLog = 368,
    CmdShowErrors = 369,
    CmdClearHistory = 370,
    CmdReopenLastClosedFile = 371,
    CmdNextTab = 372,
    CmdPrevTab = 373,
    CmdNextTabSmart = 374,
    CmdPrevTabSmart = 375,
    CmdMoveTabLeft = 376,
    CmdMoveTabRight = 377,
    CmdSelectNextTheme = 378,
    CmdToggleFrequentlyRead = 379,
    CmdInvokeInverseSearch = 380,
    CmdExec = 381,
    CmdViewWithExternalViewer = 382,
    CmdSelectionHandler = 383,
    CmdSetTheme = 384,
    CmdToggleInverseSearch = 385,
    CmdDebugCorruptMemory = 386,
    CmdDebugCrashMe = 387,
    CmdDebugDownloadSymbols = 388,
    CmdDebugTestApp = 389,
    CmdDebugShowNotif = 390,
    CmdDebugStartStressTest = 391,
    CmdDebugTogglePredictiveRender = 392,
    CmdDebugToggleRtl = 393,
    CmdToggleAntiAlias = 394,
    CmdToggleSmoothScroll = 395,
    CmdToggleScrollbarInSinglePage = 396,
    CmdToggleLazyLoading = 397,
    CmdToggleEscToExit = 398,
    CmdListPrinters = 399,
    CmdToggleWindowsPreviewer = 400,
    CmdToggleWindowsSearchFilter = 401,
    CmdScreenshot = 402,
    CmdCropImage = 403,
    CmdResizeImage = 404,
    CmdSaveImage = 405,
    CmdPasteClipboardImage = 406,
    CmdTabGroupSave = 407,
    CmdTabGroupRestore = 408,
    CmdToggleTips = 409,
    CmdChangeBackgroundColor = 410,
    CmdSetTabColor = 411,
    CmdPdfCompress = 412,
    CmdPdfDecompress = 413,
    CmdPdfDeletePages = 414,
    CmdPdfExtractPages = 415,
    CmdPdfEncrypt = 416,
    CmdPdfDecrypt = 417,
    CmdPdfBake = 418,
    CmdPdShowInfo = 419,
    CmdDocumentExtractText = 420,
    CmdDocumentShowOutline = 421,
    CmdSetScreenshotHotkey = 422,
    CmdToggleReuseInstance = 423,
    CmdToggleChmUI = 424,
    CmdReadAloud = 425,
    CmdPauseReadAloud = 426,
    CmdContinueReadAloud = 427,
    CmdToggleHoverPreview = 428,
    CmdRemoveDeletedFilesFromHistory = 429,
    CmdCommandPaletteTOC = 430,
    CmdDebugToggleRenderInfo = 431,
    CmdConvertImageToPdf = 432,
    CmdExpandToCurrentPage = 433,
    CmdStartAutoScroll = 434,
    CmdClaudeCode = 435,
    CmdNone = 436,

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
// optional bool to force a state on a toggle command instead of flipping it (#5067)
constexpr const char* kCmdArgState = "state";
