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
    CmdHelpOpenManual = 329,
    CmdHelpOpenManualOnWebsite = 330,
    CmdHelpOpenKeyboardShortcuts = 331,
    CmdHelpVisitWebsite = 332,
    CmdHelpAbout = 333,
    CmdMoveFrameFocus = 334,
    CmdFavoriteAdd = 335,
    CmdFavoriteDel = 336,
    CmdFavoriteToggle = 337,
    CmdToggleLinks = 338,
    CmdToggleShowAnnotations = 339,
    CmdShowAnnotations = 340,
    CmdHideAnnotations = 341,
    CmdCreateAnnotText = 342,
    CmdCreateAnnotLink = 343,
    CmdCreateAnnotFreeText = 344,
    CmdCreateAnnotLine = 345,
    CmdCreateAnnotSquare = 346,
    CmdCreateAnnotCircle = 347,
    CmdCreateAnnotPolygon = 348,
    CmdCreateAnnotPolyLine = 349,
    CmdCreateAnnotHighlight = 350,
    CmdCreateAnnotUnderline = 351,
    CmdCreateAnnotSquiggly = 352,
    CmdCreateAnnotStrikeOut = 353,
    CmdCreateAnnotRedact = 354,
    CmdCreateAnnotStamp = 355,
    CmdCreateAnnotCaret = 356,
    CmdCreateAnnotInk = 357,
    CmdCreateAnnotPopup = 358,
    CmdCreateAnnotFileAttachment = 359,
    CmdInvertColors = 360,
    CmdTogglePageInfo = 361,
    CmdToggleZoom = 362,
    CmdNavigateBack = 363,
    CmdNavigateForward = 364,
    CmdToggleCursorPosition = 365,
    CmdOpenNextFileInFolder = 366,
    CmdOpenPrevFileInFolder = 367,
    CmdCommandPalette = 368,
    CmdShowLog = 369,
    CmdShowErrors = 370,
    CmdClearHistory = 371,
    CmdReopenLastClosedFile = 372,
    CmdNextTab = 373,
    CmdPrevTab = 374,
    CmdNextTabSmart = 375,
    CmdPrevTabSmart = 376,
    CmdMoveTabLeft = 377,
    CmdMoveTabRight = 378,
    CmdSelectNextTheme = 379,
    CmdToggleFrequentlyRead = 380,
    CmdInvokeInverseSearch = 381,
    CmdExec = 382,
    CmdViewWithExternalViewer = 383,
    CmdSelectionHandler = 384,
    CmdSetTheme = 385,
    CmdToggleInverseSearch = 386,
    CmdDebugCorruptMemory = 387,
    CmdDebugCrashMe = 388,
    CmdDebugDownloadSymbols = 389,
    CmdDebugTestApp = 390,
    CmdDebugShowNotif = 391,
    CmdDebugStartStressTest = 392,
    CmdDebugTogglePredictiveRender = 393,
    CmdDebugToggleRtl = 394,
    CmdToggleAntiAlias = 395,
    CmdToggleSmoothScroll = 396,
    CmdToggleScrollbarInSinglePage = 397,
    CmdToggleLazyLoading = 398,
    CmdToggleEscToExit = 399,
    CmdListPrinters = 400,
    CmdToggleWindowsPreviewer = 401,
    CmdToggleWindowsSearchFilter = 402,
    CmdScreenshot = 403,
    CmdCropImage = 404,
    CmdResizeImage = 405,
    CmdSaveImage = 406,
    CmdPasteClipboardImage = 407,
    CmdTabGroupSave = 408,
    CmdTabGroupRestore = 409,
    CmdToggleTips = 410,
    CmdChangeBackgroundColor = 411,
    CmdSetTabColor = 412,
    CmdPdfCompress = 413,
    CmdPdfDecompress = 414,
    CmdPdfDeletePages = 415,
    CmdPdfExtractPages = 416,
    CmdPdfEncrypt = 417,
    CmdPdfDecrypt = 418,
    CmdPdfBake = 419,
    CmdPdShowInfo = 420,
    CmdDocumentExtractText = 421,
    CmdDocumentShowOutline = 422,
    CmdSetScreenshotHotkey = 423,
    CmdToggleReuseInstance = 424,
    CmdToggleChmUI = 425,
    CmdReadAloud = 426,
    CmdPauseReadAloud = 427,
    CmdContinueReadAloud = 428,
    CmdStopReadAloud = 429,
    CmdReadAloudFromTopPage = 430,
    CmdReadAloudSelection = 431,
    CmdToggleHoverPreview = 432,
    CmdRemoveDeletedFilesFromHistory = 433,
    CmdCommandPaletteTOC = 434,
    CmdDebugToggleRenderInfo = 435,
    CmdConvertImageToPdf = 436,
    CmdExpandToCurrentPage = 437,
    CmdStartAutoScroll = 438,
    CmdAIChatWithClaudeCode = 439,
    CmdAIChatWithGrokBuild = 440,
    CmdAIChatWithOpenAICodex = 441,
    CmdTranslateSelectionWithGrokBuild = 442,
    CmdTranslateSelectionWithClaudeCode = 443,
    CmdTranslateSelectionWithOpenAICodex = 444,
    CmdFindToggleMatchWholeWord = 445,
    CmdNone = 446,

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
