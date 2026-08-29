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
    CmdDeleteAnnotation = 273,
    CmdZoomFitPage = 274,
    CmdZoomActualSize = 275,
    CmdZoomFitWidth = 276,
    CmdZoomFitByOrientation = 277,
    CmdZoom6400 = 278,
    CmdZoom3200 = 279,
    CmdZoom1600 = 280,
    CmdZoom800 = 281,
    CmdZoom400 = 282,
    CmdZoom200 = 283,
    CmdZoom150 = 284,
    CmdZoom125 = 285,
    CmdZoom100 = 286,
    CmdZoom50 = 287,
    CmdZoom25 = 288,
    CmdZoom12_5 = 289,
    CmdZoom8_33 = 290,
    CmdZoomFitContent = 291,
    CmdZoomShrinkToFit = 292,
    CmdZoomCustom = 293,
    CmdZoomIn = 294,
    CmdZoomOut = 295,
    CmdZoomFitWidthAndContinuous = 296,
    CmdZoomFitPageAndSinglePage = 297,
    CmdContributeTranslation = 298,
    CmdOpenWithKnownExternalViewerFirst = 299,
    CmdOpenWithExplorer = 300,
    CmdOpenWithDirectoryOpus = 301,
    CmdOpenWithTotalCommander = 302,
    CmdOpenWithDoubleCommander = 303,
    CmdOpenWithAcrobat = 304,
    CmdOpenWithFoxIt = 305,
    CmdOpenWithFoxItPhantom = 306,
    CmdOpenWithPdfXchange = 307,
    CmdOpenWithXpsViewer = 308,
    CmdOpenWithHtmlHelp = 309,
    CmdOpenWithPdfDjvuBookmarker = 310,
    CmdOpenWithKnownExternalViewerLast = 311,
    CmdOpenSelectedDocument = 312,
    CmdPinSelectedDocument = 313,
    CmdForgetSelectedDocument = 314,
    CmdExpandAll = 315,
    CmdCollapseAll = 316,
    CmdSaveEmbeddedFile = 317,
    CmdOpenEmbeddedPDF = 318,
    CmdSaveAttachment = 319,
    CmdOpenAttachment = 320,
    CmdOptions = 321,
    CmdAdvancedOptions = 322,
    CmdAdvancedSettings = 323,
    CmdChangeLanguage = 324,
    CmdCheckUpdate = 325,
    CmdInstallPrereleaseUpdate = 326,
    CmdTogglePdfPreviewLogging = 327,
    CmdHelpOpenManual = 328,
    CmdHelpOpenManualOnWebsite = 329,
    CmdHelpOpenKeyboardShortcuts = 330,
    CmdToggleKeyboardHelp = 331,
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
    CmdChangeEbookSettings = 404,
    CmdSetTabColor = 405,
    CmdPdfCompress = 406,
    CmdPdfDecompress = 407,
    CmdPdfDeletePages = 408,
    CmdPdfExtractPages = 409,
    CmdPdfEncrypt = 410,
    CmdPdfDecrypt = 411,
    CmdPdfBake = 412,
    CmdPdShowInfo = 413,
    CmdDocumentExtractText = 414,
    CmdDocumentShowOutline = 415,
    CmdSetScreenshotHotkey = 416,
    CmdReadAloud = 417,
    CmdPauseReadAloud = 418,
    CmdContinueReadAloud = 419,
    CmdStopReadAloud = 420,
    CmdReadAloudFromTopPage = 421,
    CmdReadAloudSelection = 422,
    CmdToggleToolbarShowReadAloud = 423,
    CmdRemoveDeletedFilesFromHistory = 424,
    CmdCommandPaletteTOC = 425,
    CmdDebugToggleRenderInfo = 426,
    CmdConvertImageToPdf = 427,
    CmdExpandToCurrentPage = 428,
    CmdStartAutoScroll = 429,
    CmdAIChatWithClaudeCode = 430,
    CmdAIChatWithGrokBuild = 431,
    CmdAIChatWithOpenAICodex = 432,
    CmdTranslateSelectionWithGrokBuild = 433,
    CmdTranslateSelectionWithClaudeCode = 434,
    CmdTranslateSelectionWithOpenAICodex = 435,
    CmdFindToggleMatchWholeWord = 436,
    CmdGoToNextFavorite = 437,
    CmdGoToPrevFavorite = 438,
    CmdCreateAnnotImageFromClipboard = 439,
    CmdSetInverseSearch = 440,
    CmdCommandPaletteFavorites = 441,
    CmdNavigateFilesInFolder = 442,
    CmdDebugToggleCacheInfo = 443,
    CmdToggleEngineeringDrawingEnhance = 444,
    CmdSetDocumentColorsFollowTheme = 445,
    CmdTogglePreservePdfImages = 446,
    CmdToggleLightDarkTheme = 447,
    CmdChangeTheme = 448,
    CmdTranslateSelection = 449,
    CmdFavoriteShowInTab = 450,
    CmdTocExpandToLevel1 = 451,
    CmdTocExpandToLevel2 = 452,
    CmdTocExpandToLevel3 = 453,
    CmdTocCollapseSameLevel = 454,
    CmdToggleFavoritesSort = 455,
    CmdZoomFitHeight = 456,
    CmdDeleteFileAndOpenNext = 457,
    CmdShowGeneratedHTML = 458,
    CmdDeleteCachedFiles = 459,
    CmdToggleKeyboardLinkFollowing = 460,
    CmdDebugToggleDpiOverride = 461,
    CmdToggleImages = 462,
    CmdSelectTextViaKeyboard = 463,
    CmdOpenFileWithOSFilePicker = 464,
    CmdToggleFilePicker = 465,
    CmdToggleBoolSetting = 466,
    CmdFixDefaultApp = 467,
    CmdAIChatWithAntiGravity = 468,
    CmdTranslateSelectionWithAntiGravity = 469,
    CmdConvertToPDF = 470,
    CmdDebugShowFitContentArea = 471,
    CmdExtendSelectionCharLeft = 472,
    CmdExtendSelectionCharRight = 473,
    CmdExtendSelectionWordLeft = 474,
    CmdExtendSelectionWordRight = 475,
    CmdToggleLaserPointer = 476,
    CmdZoomToSelection = 477,
    CmdToggleHoverPreview = 478,
    CmdToggleDisableLinks = 479,
    CmdSignDocument = 480,
    CmdInsertImage = 481,
    CmdToggleHighlightFormFields = 482,
    CmdTogglePageBoxes = 483,
    CmdConvertPdfToImages = 484,
    CmdToggleUniformPageWidth = 485,
    CmdToggleTransparencyGrid = 486,
    CmdTogglePageGrid = 487,
    CmdConfigurePageGrid = 488,
    CmdToggleEditPDF = 489,
    CmdApplyRedactions = 490,
    CmdUndo = 491,
    CmdRedo = 492,
    CmdCutAnnotation = 493,
    CmdCopyAnnotation = 494,
    CmdPasteAnnotation = 495,
    CmdSearchGoogleLens = 496,
    CmdNavigateThumbnail = 497,
    CmdShowAnnotationText = 498,
    CmdNone = 499,

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
