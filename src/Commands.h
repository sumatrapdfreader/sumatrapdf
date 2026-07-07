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
    CmdToggleMarkdownUI = 428,
    CmdReadAloud = 429,
    CmdPauseReadAloud = 430,
    CmdContinueReadAloud = 431,
    CmdStopReadAloud = 432,
    CmdReadAloudFromTopPage = 433,
    CmdReadAloudSelection = 434,
    CmdToggleHoverPreview = 435,
    CmdRemoveDeletedFilesFromHistory = 436,
    CmdCommandPaletteTOC = 437,
    CmdDebugToggleRenderInfo = 438,
    CmdConvertImageToPdf = 439,
    CmdExpandToCurrentPage = 440,
    CmdStartAutoScroll = 441,
    CmdAIChatWithClaudeCode = 442,
    CmdAIChatWithGrokBuild = 443,
    CmdAIChatWithOpenAICodex = 444,
    CmdTranslateSelectionWithGrokBuild = 445,
    CmdTranslateSelectionWithClaudeCode = 446,
    CmdTranslateSelectionWithOpenAICodex = 447,
    CmdFindToggleMatchWholeWord = 448,
    CmdGoToNextFavorite = 449,
    CmdGoToPrevFavorite = 450,
    CmdCreateAnnotImageFromClipboard = 451,
    CmdSetInverseSearch = 452,
    CmdToggleToolbarPosition = 453,
    CmdCommandPaletteFavorites = 454,
    CmdNavigateFilesInFolder = 455,
    CmdNone = 456,

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
    Str name;

    // TODO: could be a union
    Str strVal;
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
    Str definition;

    // optional name, if given this shows up in command palette
    Str name;

    // optional keyboard shortcut
    Str key;

    // a unique command id generated by us, starting with CmdFirstCustom
    // it identifies a command with their fixed set of arguments
    int id = 0;

    // optional
    Str idStr;

    CommandArg* firstArg = nullptr;
    CustomCommand() = default;
    ~CustomCommand();
};

extern CustomCommand* gFirstCustomCommand;
extern SeqStrings gCommandDescriptions;

int GetCommandIdByName(Str);
int GetCommandIdByDesc(Str);

CustomCommand* CreateCustomCommand(Str definition, int origCmdId, CommandArg* args);
CustomCommand* FindCustomCommand(int cmdId);
void FreeCustomCommands();
CommandArg* NewStringArg(Str name, Str val);
CommandArg* NewFloatArg(Str name, float val);
void InsertArg(CommandArg** firstPtr, CommandArg* arg);

CustomCommand* CreateCommandFromDefinition(Str definition);
CommandArg* GetCommandArg(CustomCommand*, Str argName);
int GetCommandIntArg(CustomCommand* cmd, Str name, int defValue);
bool GetCommandBoolArg(CustomCommand* cmd, Str name, bool defValue);
Str GetCommandStringArg(CustomCommand* cmd, Str name, Str defValue);
void GetCommandsWithOrigId(Vec<CustomCommand*>& commands, int origId);

#define kCmdArgColor StrL("color")
#define kCmdArgBgColor StrL("bgcolor")
#define kCmdArgOpacity StrL("opacity")
#define kCmdArgOpenEdit StrL("openedit")
#define kCmdArgTextSize StrL("textsize")
#define kCmdArgBorderWidth StrL("borderwidth")
#define kCmdArgInteriorColor StrL("interiorcolor")

#define kCmdArgCopyToClipboard StrL("copytoclipboard")
#define kCmdArgSetContent StrL("setcontent")
#define kCmdArgExe StrL("exe")
#define kCmdArgURL StrL("url")
#define kCmdArgLevel StrL("level")
#define kCmdArgFilter StrL("filter")
#define kCmdArgN StrL("n")
#define kCmdArgMode StrL("mode")
#define kCmdArgTheme StrL("theme")
#define kCmdArgCommandLine StrL("cmdline")
#define kCmdArgToolbarText StrL("toolbartext")
#define kCmdArgToolbarSvgIcon StrL("toolbarsvgicon")
#define kCmdArgFocusEdit StrL("focusedit")
#define kCmdArgFocusList StrL("focuslist")
// optional bool to force a state on a toggle command instead of flipping it (#5067)
#define kCmdArgState StrL("state")
