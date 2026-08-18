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
    CmdDiscardChanges = 272,
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
    CmdToggleKeyboardHelp = 332,
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
    CmdListPrinters = 394,
    CmdToggleWindowsPreviewer = 395,
    CmdToggleWindowsSearchFilter = 396,
    CmdScreenshot = 397,
    CmdCropImage = 398,
    CmdResizeImage = 399,
    CmdSaveImage = 400,
    CmdPasteClipboardImage = 401,
    CmdTabGroupSave = 402,
    CmdTabGroupRestore = 403,
    CmdChangeBackgroundColor = 404,
    CmdChangeEbookSettings = 405,
    CmdSetTabColor = 406,
    CmdPdfCompress = 407,
    CmdPdfDecompress = 408,
    CmdPdfDeletePages = 409,
    CmdPdfExtractPages = 410,
    CmdPdfEncrypt = 411,
    CmdPdfDecrypt = 412,
    CmdPdfBake = 413,
    CmdPdShowInfo = 414,
    CmdDocumentExtractText = 415,
    CmdDocumentShowOutline = 416,
    CmdSetScreenshotHotkey = 417,
    CmdReadAloud = 418,
    CmdPauseReadAloud = 419,
    CmdContinueReadAloud = 420,
    CmdStopReadAloud = 421,
    CmdReadAloudFromTopPage = 422,
    CmdReadAloudSelection = 423,
    CmdToggleToolbarShowReadAloud = 424,
    CmdRemoveDeletedFilesFromHistory = 425,
    CmdCommandPaletteTOC = 426,
    CmdDebugToggleRenderInfo = 427,
    CmdConvertImageToPdf = 428,
    CmdExpandToCurrentPage = 429,
    CmdStartAutoScroll = 430,
    CmdAIChatWithClaudeCode = 431,
    CmdAIChatWithGrokBuild = 432,
    CmdAIChatWithOpenAICodex = 433,
    CmdTranslateSelectionWithGrokBuild = 434,
    CmdTranslateSelectionWithClaudeCode = 435,
    CmdTranslateSelectionWithOpenAICodex = 436,
    CmdFindToggleMatchWholeWord = 437,
    CmdGoToNextFavorite = 438,
    CmdGoToPrevFavorite = 439,
    CmdCreateAnnotImageFromClipboard = 440,
    CmdSetInverseSearch = 441,
    CmdCommandPaletteFavorites = 442,
    CmdNavigateFilesInFolder = 443,
    CmdDebugToggleCacheInfo = 444,
    CmdToggleEngineeringDrawingEnhance = 445,
    CmdSetDocumentColorsFollowTheme = 446,
    CmdTogglePreservePdfImages = 447,
    CmdToggleLightDarkTheme = 448,
    CmdChangeTheme = 449,
    CmdTranslateSelection = 450,
    CmdFavoriteShowInTab = 451,
    CmdTocExpandToLevel1 = 452,
    CmdTocExpandToLevel2 = 453,
    CmdTocExpandToLevel3 = 454,
    CmdTocCollapseSameLevel = 455,
    CmdToggleFavoritesSort = 456,
    CmdZoomFitHeight = 457,
    CmdDeleteFileAndOpenNext = 458,
    CmdShowGeneratedHTML = 459,
    CmdDeleteCachedFiles = 460,
    CmdToggleKeyboardLinkFollowing = 461,
    CmdDebugToggleDpiOverride = 462,
    CmdToggleImages = 463,
    CmdSelectTextViaKeyboard = 464,
    CmdOpenFileWithOSFilePicker = 465,
    CmdToggleFilePicker = 466,
    CmdToggleBoolSetting = 467,
    CmdFixDefaultApp = 468,
    CmdAIChatWithAntiGravity = 469,
    CmdTranslateSelectionWithAntiGravity = 470,
    CmdConvertToPDF = 471,
    CmdDebugShowFitContentArea = 472,
    CmdExtendSelectionCharLeft = 473,
    CmdExtendSelectionCharRight = 474,
    CmdExtendSelectionWordLeft = 475,
    CmdExtendSelectionWordRight = 476,
    CmdToggleLaserPointer = 477,
    CmdZoomToSelection = 478,
    CmdToggleHoverPreview = 479,
    CmdToggleDisableLinks = 480,
    CmdSignDocument = 481,
    CmdInsertImage = 482,
    CmdNone = 483,

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
};

CommandArg* AllocCommandArg(Str name, Str strVal);
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

    CommandArg* firstArg = nullptr;
};

CustomCommand* AllocCustomCommand(Str definition, Str name, Str key);
void FreeCustomCommand(CustomCommand* cmd);

extern CustomCommand* gFirstCustomCommand;
extern SeqStrings gCommandDescriptions;

int GetCommandIdByName(Str);
int GetCommandIdByDesc(Str);
Str GetCommandDescription(int commandId);

CustomCommand* CreateCustomCommand(Str definition, int origCmdId, CommandArg* args, Str name = {}, Str key = {});
CustomCommand* CloneCustomCommand(CustomCommand* cmd, Str name = {}, Str key = {});
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
#define kCmdArgAlignment StrL("alignment")
#define kCmdArgInteriorColor StrL("interiorcolor")

#define kCmdArgCopyToClipboard StrL("copytoclipboard")
#define kCmdArgSetContent StrL("setcontent")
#define kCmdArgExe StrL("exe")
#define kCmdArgURL StrL("url")
// SelectionHandlers: how and what to send (see Customize-search-translation-services.md)
#define kCmdArgMethod StrL("method")
#define kCmdArgBody StrL("body")
#define kCmdArgContentType StrL("contenttype")
#define kCmdArgHeaders StrL("headers")
#define kCmdArgLevel StrL("level")
#define kCmdArgFilter StrL("filter")
#define kCmdArgN StrL("n")
#define kCmdArgMode StrL("mode")
#define kCmdArgTheme StrL("theme")
#define kCmdArgCommandLine StrL("cmdline")
#define kCmdArgToolbarText StrL("toolbartext")
#define kCmdArgToolbarSvgIcon StrL("toolbarsvgicon")
// text (or, when it starts with "<svg", an icon) for a button on the toolbar
// that pops up over a text selection
#define kCmdArgSelectToolbar StrL("selecttoolbar")
#define kCmdArgFocusEdit StrL("focusedit")
#define kCmdArgFocusList StrL("focuslist")
// optional bool to force a state on a toggle command instead of flipping it (#5067)
#define kCmdArgState StrL("state")
#define kCmdArgName StrL("name")
#define kCmdArgExt StrL("ext")
