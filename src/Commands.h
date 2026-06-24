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
    CmdDiscardAnnotations = 274,
    CmdEditAnnotations = 275,
    CmdDeleteAnnotation = 276,
    CmdZoomFitPage = 277,
    CmdZoomActualSize = 278,
    CmdZoomFitWidth = 279,
    CmdZoomFitByOrientation = 280,
    CmdZoom6400 = 281,
    CmdZoom3200 = 282,
    CmdZoom1600 = 283,
    CmdZoom800 = 284,
    CmdZoom400 = 285,
    CmdZoom200 = 286,
    CmdZoom150 = 287,
    CmdZoom125 = 288,
    CmdZoom100 = 289,
    CmdZoom50 = 290,
    CmdZoom25 = 291,
    CmdZoom12_5 = 292,
    CmdZoom8_33 = 293,
    CmdZoomFitContent = 294,
    CmdZoomShrinkToFit = 295,
    CmdZoomCustom = 296,
    CmdZoomIn = 297,
    CmdZoomOut = 298,
    CmdZoomFitWidthAndContinuous = 299,
    CmdZoomFitPageAndSinglePage = 300,
    CmdContributeTranslation = 301,
    CmdOpenWithKnownExternalViewerFirst = 302,
    CmdOpenWithExplorer = 303,
    CmdOpenWithDirectoryOpus = 304,
    CmdOpenWithTotalCommander = 305,
    CmdOpenWithDoubleCommander = 306,
    CmdOpenWithAcrobat = 307,
    CmdOpenWithFoxIt = 308,
    CmdOpenWithFoxItPhantom = 309,
    CmdOpenWithPdfXchange = 310,
    CmdOpenWithXpsViewer = 311,
    CmdOpenWithHtmlHelp = 312,
    CmdOpenWithPdfDjvuBookmarker = 313,
    CmdOpenWithKnownExternalViewerLast = 314,
    CmdOpenSelectedDocument = 315,
    CmdPinSelectedDocument = 316,
    CmdForgetSelectedDocument = 317,
    CmdExpandAll = 318,
    CmdCollapseAll = 319,
    CmdSaveEmbeddedFile = 320,
    CmdOpenEmbeddedPDF = 321,
    CmdSaveAttachment = 322,
    CmdOpenAttachment = 323,
    CmdOptions = 324,
    CmdAdvancedOptions = 325,
    CmdAdvancedSettings = 326,
    CmdChangeLanguage = 327,
    CmdCheckUpdate = 328,
    CmdInstallPrereleaseUpdate = 329,
    CmdHelpOpenManual = 330,
    CmdHelpOpenManualOnWebsite = 331,
    CmdHelpOpenKeyboardShortcuts = 332,
    CmdHelpVisitWebsite = 333,
    CmdHelpAbout = 334,
    CmdMoveFrameFocus = 335,
    CmdFavoriteAdd = 336,
    CmdFavoriteDel = 337,
    CmdFavoriteToggle = 338,
    CmdToggleLinks = 339,
    CmdToggleShowAnnotations = 340,
    CmdShowAnnotations = 341,
    CmdHideAnnotations = 342,
    CmdCreateAnnotText = 343,
    CmdCreateAnnotLink = 344,
    CmdCreateAnnotFreeText = 345,
    CmdCreateAnnotLine = 346,
    CmdCreateAnnotSquare = 347,
    CmdCreateAnnotCircle = 348,
    CmdCreateAnnotPolygon = 349,
    CmdCreateAnnotPolyLine = 350,
    CmdCreateAnnotHighlight = 351,
    CmdCreateAnnotUnderline = 352,
    CmdCreateAnnotSquiggly = 353,
    CmdCreateAnnotStrikeOut = 354,
    CmdCreateAnnotRedact = 355,
    CmdCreateAnnotStamp = 356,
    CmdCreateAnnotCaret = 357,
    CmdCreateAnnotInk = 358,
    CmdCreateAnnotPopup = 359,
    CmdCreateAnnotFileAttachment = 360,
    CmdInvertColors = 361,
    CmdTogglePageInfo = 362,
    CmdToggleZoom = 363,
    CmdNavigateBack = 364,
    CmdNavigateForward = 365,
    CmdToggleCursorPosition = 366,
    CmdOpenNextFileInFolder = 367,
    CmdOpenPrevFileInFolder = 368,
    CmdCommandPalette = 369,
    CmdShowLog = 370,
    CmdShowErrors = 371,
    CmdClearHistory = 372,
    CmdReopenLastClosedFile = 373,
    CmdNextTab = 374,
    CmdPrevTab = 375,
    CmdNextTabSmart = 376,
    CmdPrevTabSmart = 377,
    CmdMoveTabLeft = 378,
    CmdMoveTabRight = 379,
    CmdSelectNextTheme = 380,
    CmdToggleFrequentlyRead = 381,
    CmdInvokeInverseSearch = 382,
    CmdExec = 383,
    CmdViewWithExternalViewer = 384,
    CmdSelectionHandler = 385,
    CmdSetTheme = 386,
    CmdToggleInverseSearch = 387,
    CmdDebugCorruptMemory = 388,
    CmdDebugCrashMe = 389,
    CmdDebugDownloadSymbols = 390,
    CmdDebugTestApp = 391,
    CmdDebugShowNotif = 392,
    CmdDebugStartStressTest = 393,
    CmdDebugTogglePredictiveRender = 394,
    CmdDebugToggleRtl = 395,
    CmdToggleAntiAlias = 396,
    CmdToggleSmoothScroll = 397,
    CmdToggleScrollbarInSinglePage = 398,
    CmdToggleLazyLoading = 399,
    CmdToggleEscToExit = 400,
    CmdListPrinters = 401,
    CmdToggleWindowsPreviewer = 402,
    CmdToggleWindowsSearchFilter = 403,
    CmdScreenshot = 404,
    CmdCropImage = 405,
    CmdResizeImage = 406,
    CmdSaveImage = 407,
    CmdPasteClipboardImage = 408,
    CmdTabGroupSave = 409,
    CmdTabGroupRestore = 410,
    CmdToggleTips = 411,
    CmdChangeBackgroundColor = 412,
    CmdSetTabColor = 413,
    CmdPdfCompress = 414,
    CmdPdfDecompress = 415,
    CmdPdfDeletePages = 416,
    CmdPdfExtractPages = 417,
    CmdPdfEncrypt = 418,
    CmdPdfDecrypt = 419,
    CmdPdfBake = 420,
    CmdPdShowInfo = 421,
    CmdDocumentExtractText = 422,
    CmdDocumentShowOutline = 423,
    CmdSetScreenshotHotkey = 424,
    CmdToggleReuseInstance = 425,
    CmdToggleChmUI = 426,
    CmdReadAloud = 427,
    CmdPauseReadAloud = 428,
    CmdContinueReadAloud = 429,
    CmdStopReadAloud = 430,
    CmdReadAloudFromTopPage = 431,
    CmdReadAloudSelection = 432,
    CmdToggleHoverPreview = 433,
    CmdRemoveDeletedFilesFromHistory = 434,
    CmdCommandPaletteTOC = 435,
    CmdDebugToggleRenderInfo = 436,
    CmdConvertImageToPdf = 437,
    CmdExpandToCurrentPage = 438,
    CmdStartAutoScroll = 439,
    CmdAIChatWithClaudeCode = 440,
    CmdAIChatWithGrokBuild = 441,
    CmdAIChatWithOpenAICodex = 442,
    CmdTranslateSelectionWithGrokBuild = 443,
    CmdTranslateSelectionWithClaudeCode = 444,
    CmdTranslateSelectionWithOpenAICodex = 445,
    CmdFindToggleMatchWholeWord = 446,
    CmdGoToNextFavorite = 447,
    CmdGoToPrevFavorite = 448,
    CmdCreateAnnotImageFromClipboard = 449,
    CmdSetInverseSearch = 450,
    CmdNone = 451,

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
    // we could use SeqStrings and use u16 for arg name id
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
