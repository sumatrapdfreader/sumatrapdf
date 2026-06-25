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
    CmdTogglePdfPreviewLogging = 330,
    CmdHelpOpenManual = 331,
    CmdHelpOpenManualOnWebsite = 332,
    CmdHelpOpenKeyboardShortcuts = 333,
    CmdHelpVisitWebsite = 334,
    CmdHelpAbout = 335,
    CmdMoveFrameFocus = 336,
    CmdFavoriteAdd = 337,
    CmdFavoriteDel = 338,
    CmdFavoriteToggle = 339,
    CmdToggleLinks = 340,
    CmdToggleShowAnnotations = 341,
    CmdShowAnnotations = 342,
    CmdHideAnnotations = 343,
    CmdCreateAnnotText = 344,
    CmdCreateAnnotLink = 345,
    CmdCreateAnnotFreeText = 346,
    CmdCreateAnnotLine = 347,
    CmdCreateAnnotSquare = 348,
    CmdCreateAnnotCircle = 349,
    CmdCreateAnnotPolygon = 350,
    CmdCreateAnnotPolyLine = 351,
    CmdCreateAnnotHighlight = 352,
    CmdCreateAnnotUnderline = 353,
    CmdCreateAnnotSquiggly = 354,
    CmdCreateAnnotStrikeOut = 355,
    CmdCreateAnnotRedact = 356,
    CmdCreateAnnotStamp = 357,
    CmdCreateAnnotCaret = 358,
    CmdCreateAnnotInk = 359,
    CmdCreateAnnotPopup = 360,
    CmdCreateAnnotFileAttachment = 361,
    CmdInvertColors = 362,
    CmdTogglePageInfo = 363,
    CmdToggleZoom = 364,
    CmdNavigateBack = 365,
    CmdNavigateForward = 366,
    CmdToggleCursorPosition = 367,
    CmdOpenNextFileInFolder = 368,
    CmdOpenPrevFileInFolder = 369,
    CmdCommandPalette = 370,
    CmdShowLog = 371,
    CmdShowErrors = 372,
    CmdClearHistory = 373,
    CmdReopenLastClosedFile = 374,
    CmdNextTab = 375,
    CmdPrevTab = 376,
    CmdNextTabSmart = 377,
    CmdPrevTabSmart = 378,
    CmdMoveTabLeft = 379,
    CmdMoveTabRight = 380,
    CmdSelectNextTheme = 381,
    CmdToggleFrequentlyRead = 382,
    CmdInvokeInverseSearch = 383,
    CmdExec = 384,
    CmdViewWithExternalViewer = 385,
    CmdSelectionHandler = 386,
    CmdSetTheme = 387,
    CmdToggleInverseSearch = 388,
    CmdDebugCorruptMemory = 389,
    CmdDebugCrashMe = 390,
    CmdDebugDownloadSymbols = 391,
    CmdDebugTestApp = 392,
    CmdDebugShowNotif = 393,
    CmdDebugStartStressTest = 394,
    CmdDebugTogglePredictiveRender = 395,
    CmdDebugToggleRtl = 396,
    CmdToggleAntiAlias = 397,
    CmdToggleSmoothScroll = 398,
    CmdToggleScrollbarInSinglePage = 399,
    CmdToggleLazyLoading = 400,
    CmdToggleEscToExit = 401,
    CmdListPrinters = 402,
    CmdToggleWindowsPreviewer = 403,
    CmdToggleWindowsSearchFilter = 404,
    CmdScreenshot = 405,
    CmdCropImage = 406,
    CmdResizeImage = 407,
    CmdSaveImage = 408,
    CmdPasteClipboardImage = 409,
    CmdTabGroupSave = 410,
    CmdTabGroupRestore = 411,
    CmdToggleTips = 412,
    CmdChangeBackgroundColor = 413,
    CmdSetTabColor = 414,
    CmdPdfCompress = 415,
    CmdPdfDecompress = 416,
    CmdPdfDeletePages = 417,
    CmdPdfExtractPages = 418,
    CmdPdfEncrypt = 419,
    CmdPdfDecrypt = 420,
    CmdPdfBake = 421,
    CmdPdShowInfo = 422,
    CmdDocumentExtractText = 423,
    CmdDocumentShowOutline = 424,
    CmdSetScreenshotHotkey = 425,
    CmdToggleReuseInstance = 426,
    CmdToggleChmUI = 427,
    CmdReadAloud = 428,
    CmdPauseReadAloud = 429,
    CmdContinueReadAloud = 430,
    CmdStopReadAloud = 431,
    CmdReadAloudFromTopPage = 432,
    CmdReadAloudSelection = 433,
    CmdToggleHoverPreview = 434,
    CmdRemoveDeletedFilesFromHistory = 435,
    CmdCommandPaletteTOC = 436,
    CmdDebugToggleRenderInfo = 437,
    CmdConvertImageToPdf = 438,
    CmdExpandToCurrentPage = 439,
    CmdStartAutoScroll = 440,
    CmdAIChatWithClaudeCode = 441,
    CmdAIChatWithGrokBuild = 442,
    CmdAIChatWithOpenAICodex = 443,
    CmdTranslateSelectionWithGrokBuild = 444,
    CmdTranslateSelectionWithClaudeCode = 445,
    CmdTranslateSelectionWithOpenAICodex = 446,
    CmdFindToggleMatchWholeWord = 447,
    CmdGoToNextFavorite = 448,
    CmdGoToPrevFavorite = 449,
    CmdCreateAnnotImageFromClipboard = 450,
    CmdSetInverseSearch = 451,
    CmdToggleToolbarPosition = 452,
    CmdNone = 453,

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
constexpr const char* kCmdArgToolbarSvgIcon = "toolbarsvgicon";
constexpr const char* kCmdArgFocusEdit = "focusedit";
constexpr const char* kCmdArgFocusList = "focuslist";
// optional bool to force a state on a toggle command instead of flipping it (#5067)
constexpr const char* kCmdArgState = "state";
