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
    CmdCopySelection = 234,
    CmdTranslateSelectionWithGoogle = 235,
    CmdTranslateSelectionWithDeepL = 236,
    CmdSearchSelectionWithGoogle = 237,
    CmdSearchSelectionWithBing = 238,
    CmdSearchSelectionWithWikipedia = 239,
    CmdSearchSelectionWithGoogleScholar = 240,
    CmdSelectAll = 241,
    CmdNewWindow = 242,
    CmdDuplicateInNewWindow = 243,
    CmdDuplicateInNewTab = 244,
    CmdCopyImage = 245,
    CmdCopyLinkTarget = 246,
    CmdCopyComment = 247,
    CmdCopyFilePath = 248,
    CmdScrollUp = 249,
    CmdScrollDown = 250,
    CmdScrollLeft = 251,
    CmdScrollRight = 252,
    CmdScrollLeftPage = 253,
    CmdScrollRightPage = 254,
    CmdScrollUpPage = 255,
    CmdScrollDownPage = 256,
    CmdScrollDownHalfPage = 257,
    CmdScrollUpHalfPage = 258,
    CmdGoToNextPage = 259,
    CmdGoToPrevPage = 260,
    CmdGoToFirstPage = 261,
    CmdGoToLastPage = 262,
    CmdGoToPage = 263,
    CmdFindFirst = 264,
    CmdFindNext = 265,
    CmdFindPrev = 266,
    CmdFindNextSel = 267,
    CmdFindPrevSel = 268,
    CmdFindToggleMatchCase = 269,
    CmdSaveAnnotations = 270,
    CmdSaveAnnotationsNewFile = 271,
    CmdDiscardAnnotations = 272,
    CmdEditAnnotations = 273,
    CmdDeleteAnnotation = 274,
    CmdZoomFitPage = 275,
    CmdZoomActualSize = 276,
    CmdZoomFitWidth = 277,
    CmdZoomFitByOrientation = 278,
    CmdZoom6400 = 279,
    CmdZoom3200 = 280,
    CmdZoom1600 = 281,
    CmdZoom800 = 282,
    CmdZoom400 = 283,
    CmdZoom200 = 284,
    CmdZoom150 = 285,
    CmdZoom125 = 286,
    CmdZoom100 = 287,
    CmdZoom50 = 288,
    CmdZoom25 = 289,
    CmdZoom12_5 = 290,
    CmdZoom8_33 = 291,
    CmdZoomFitContent = 292,
    CmdZoomShrinkToFit = 293,
    CmdZoomCustom = 294,
    CmdZoomIn = 295,
    CmdZoomOut = 296,
    CmdZoomFitWidthAndContinuous = 297,
    CmdZoomFitPageAndSinglePage = 298,
    CmdContributeTranslation = 299,
    CmdOpenWithKnownExternalViewerFirst = 300,
    CmdOpenWithExplorer = 301,
    CmdOpenWithDirectoryOpus = 302,
    CmdOpenWithTotalCommander = 303,
    CmdOpenWithDoubleCommander = 304,
    CmdOpenWithAcrobat = 305,
    CmdOpenWithFoxIt = 306,
    CmdOpenWithFoxItPhantom = 307,
    CmdOpenWithPdfXchange = 308,
    CmdOpenWithXpsViewer = 309,
    CmdOpenWithHtmlHelp = 310,
    CmdOpenWithPdfDjvuBookmarker = 311,
    CmdOpenWithKnownExternalViewerLast = 312,
    CmdOpenSelectedDocument = 313,
    CmdPinSelectedDocument = 314,
    CmdForgetSelectedDocument = 315,
    CmdExpandAll = 316,
    CmdCollapseAll = 317,
    CmdSaveEmbeddedFile = 318,
    CmdOpenEmbeddedPDF = 319,
    CmdSaveAttachment = 320,
    CmdOpenAttachment = 321,
    CmdOptions = 322,
    CmdAdvancedOptions = 323,
    CmdAdvancedSettings = 324,
    CmdChangeLanguage = 325,
    CmdCheckUpdate = 326,
    CmdInstallPrereleaseUpdate = 327,
    CmdTogglePdfPreviewLogging = 328,
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
    CmdInvokeInverseSearch = 379,
    CmdExec = 380,
    CmdViewWithExternalViewer = 381,
    CmdSelectionHandler = 382,
    CmdSetTheme = 383,
    CmdToggleInverseSearch = 384,
    CmdDebugCorruptMemory = 385,
    CmdDebugCrashMe = 386,
    CmdDebugDownloadSymbols = 387,
    CmdDebugTestApp = 388,
    CmdDebugShowNotif = 389,
    CmdDebugStartStressTest = 390,
    CmdDebugTogglePredictiveRender = 391,
    CmdDebugToggleRtl = 392,
    CmdListPrinters = 393,
    CmdToggleWindowsPreviewer = 394,
    CmdToggleWindowsSearchFilter = 395,
    CmdScreenshot = 396,
    CmdCropImage = 397,
    CmdResizeImage = 398,
    CmdSaveImage = 399,
    CmdPasteClipboardImage = 400,
    CmdTabGroupSave = 401,
    CmdTabGroupRestore = 402,
    CmdChangeBackgroundColor = 403,
    CmdSetTabColor = 404,
    CmdPdfCompress = 405,
    CmdPdfDecompress = 406,
    CmdPdfDeletePages = 407,
    CmdPdfExtractPages = 408,
    CmdPdfEncrypt = 409,
    CmdPdfDecrypt = 410,
    CmdPdfBake = 411,
    CmdPdShowInfo = 412,
    CmdDocumentExtractText = 413,
    CmdDocumentShowOutline = 414,
    CmdSetScreenshotHotkey = 415,
    CmdReadAloud = 416,
    CmdPauseReadAloud = 417,
    CmdContinueReadAloud = 418,
    CmdStopReadAloud = 419,
    CmdReadAloudFromTopPage = 420,
    CmdReadAloudSelection = 421,
    CmdRemoveDeletedFilesFromHistory = 422,
    CmdCommandPaletteTOC = 423,
    CmdDebugToggleRenderInfo = 424,
    CmdConvertImageToPdf = 425,
    CmdExpandToCurrentPage = 426,
    CmdStartAutoScroll = 427,
    CmdAIChatWithClaudeCode = 428,
    CmdAIChatWithGrokBuild = 429,
    CmdAIChatWithOpenAICodex = 430,
    CmdTranslateSelectionWithGrokBuild = 431,
    CmdTranslateSelectionWithClaudeCode = 432,
    CmdTranslateSelectionWithOpenAICodex = 433,
    CmdFindToggleMatchWholeWord = 434,
    CmdGoToNextFavorite = 435,
    CmdGoToPrevFavorite = 436,
    CmdCreateAnnotImageFromClipboard = 437,
    CmdSetInverseSearch = 438,
    CmdCommandPaletteFavorites = 439,
    CmdNavigateFilesInFolder = 440,
    CmdDebugToggleCacheInfo = 441,
    CmdToggleEngineeringDrawingEnhance = 442,
    CmdSetDocumentColorsFollowTheme = 443,
    CmdTogglePreservePdfImages = 444,
    CmdToggleLightDarkTheme = 445,
    CmdChangeTheme = 446,
    CmdNone = 447,

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
