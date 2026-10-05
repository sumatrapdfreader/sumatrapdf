/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the visibility policy over a filled-in AppCommandCtx is portable and
// live - the menus ask it. Filling the context in (NewAppCommandCtx) needs
// MainWindow / WindowTab, so it moved to src/MainWindow.cpp and this file
// links into the console tools. The rules that ask a service which is not
// ported yet (external viewers, AI chat, the installer, the update check) are
// behind NG_HAS_UI
#define NG_HAS_UI 0

#include "base/Base.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "TextSelection.h"
#include "Annotation.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#if NG_HAS_UI
#include "MainWindow.h"
#include "WindowTab.h"
#endif
#include "Commands.h"
#include "ExternalViewers.h"
#include "FileHistory.h"
#if NG_HAS_UI
#include "Installer.h"
#include "ReadAloud.h"
#endif
#include "Favorites.h"
#include "UpdateCheck.h"
#include "CommandAvailability.h"

// clang-format off

static uintptr_t gNoDocWhitelist[] = {
    CmdOpenFile,
    CmdOpenFileNoHistory,
    CmdOpenFileWithOSFilePicker,
    CmdOpenFileWithSumatraFilePicker,
    CmdToggleFilePicker,
    CmdToggleBoolSetting,
    CmdNavigateFilesInFolder,
    CmdExit,
    CmdNewWindow,
    CmdContributeTranslation,
    CmdOptions,
    CmdSetInverseSearch,
    CmdAdvancedSettings,
    CmdOpenSettingsFile,
    CmdChangeLanguage,
    CmdChangeTheme,
    CmdCheckUpdate,
    CmdHelpOpenManual,
    CmdHelpOpenManualOnWebsite,
    CmdHelpOpenKeyboardShortcuts,
    CmdToggleKeyboardHelp,
    CmdHelpVisitWebsite,
    CmdHelpAbout,
    CmdDebugShowNotif,
    CmdDebugStartStressTest,
    CmdDebugTogglePredictiveRender,
    CmdDebugToggleRenderInfo,
    CmdDebugToggleCacheInfo,
    CmdDebugToggleRtl,
    CmdDebugToggleDpiOverride,
    CmdChangeScrollbar,
    CmdToggleFullscreen,
    CmdToggleMenuBar,
    CmdToggleToolbar,
    CmdToggleInverseSearch,
    CmdToggleLinks,
    CmdToggleHighlightFormFields,
    CmdToggleDisableLinks,
    CmdToggleImages,
    CmdToggleTransparencyGrid,
    CmdTogglePageGrid,
    CmdConfigurePageGrid,
    CmdToggleHoverPreview,
    CmdToggleWindowsPreviewer,
    CmdToggleWindowsSearchFilter,
    CmdInvertColors,
    CmdToggleGrayscale,
    CmdFavoriteToggle,
    CmdFavoriteShowInTab,
    CmdGoToHomePage,
    CmdShowLog,
    CmdClearHistory,
    CmdRemoveDeletedFilesFromHistory,
    CmdDeleteCachedFiles,
    CmdReopenLastClosedFile,
    CmdListPrinters,
    CmdDebugCrashMe,
    CmdDebugCorruptMemory,
    CmdScreenshot,
    CmdPasteClipboardImage,
    CmdTabGroupRestore,
    CmdSetScreenshotHotkey,
    CmdStopReadAloud,
    0,
};

uintptr_t disableIfNoSelection[] = {
    CmdCopySelection,
    CmdZoomToSelection,
    CmdFindNextSel,
    CmdFindPrevSel,
    CmdTranslateSelection,
    CmdTranslateSelectionWithDeepL,
    CmdTranslateSelectionWithGoogle,
    CmdTranslateSelectionWithGrokBuild,
    CmdTranslateSelectionWithClaudeCode,
    CmdTranslateSelectionWithOpenAICodex,
    CmdTranslateSelectionWithAntiGravity,
    CmdSearchSelectionWithWikipedia,
    CmdSearchSelectionWithGoogleScholar,
    CmdSearchSelectionWithBing,
    CmdSearchSelectionWithGoogle,
    0,
};

// annotations created from a text selection; a rectangular selection has no
// text to mark up. checked after !supportsAnnots already hid them for non-PDF
static uintptr_t createAnnotFromSelection[] = {
    CmdCreateAnnotHighlight,
    CmdCreateAnnotSquiggly,
    CmdCreateAnnotStrikeOut,
    CmdCreateAnnotUnderline,
    0,
};

static uintptr_t removeIfNoInternetPerms[] = {
    CmdCheckUpdate,
    CmdTranslateSelectionWithGoogle,
    CmdTranslateSelectionWithDeepL,
    CmdSearchSelectionWithGoogle,
    CmdSearchGoogleLens,
    CmdSearchGoogleLensPage,
    CmdSearchGoogleLensImage,
    CmdSearchSelectionWithBing,
    CmdSearchSelectionWithWikipedia,
    CmdSearchSelectionWithGoogleScholar,
    CmdHelpVisitWebsite,
    CmdHelpOpenManualOnWebsite,
    CmdHelpOpenKeyboardShortcuts,
    CmdContributeTranslation,
    0,
};

static uintptr_t removeIfNoFullscreenPerms[] = {
    CmdTogglePresentationMode,
    CmdToggleFullscreen,
    0,
};

static uintptr_t removeIfNoPrefsPerms[] = {
    CmdOptions,
    CmdSetInverseSearch,
    CmdAdvancedSettings,
    CmdPinSelectedDocument,
    CmdForgetSelectedDocument,
    CmdFavoriteAdd,
    CmdFavoriteDel,
    CmdFavoriteToggle,
    CmdFavoriteShowInTab,
    CmdToggleFavoritesSort,
    CmdGoToNextFavorite,
    CmdGoToPrevFavorite,
    0,
};

static uintptr_t removeIfNoCopyPerms[] = {
    CmdTranslateSelection,
    CmdTranslateSelectionWithGoogle,
    CmdTranslateSelectionWithDeepL,
    CmdSearchSelectionWithGoogle,
    CmdSearchSelectionWithBing,
    CmdSearchSelectionWithWikipedia,
    CmdSearchSelectionWithGoogleScholar,
    CmdSelectAll,
    CmdCopySelection,
    CmdCopyLinkTarget,
    CmdCopyComment,
    CmdCopyImage,
    CmdCopySelectionAsImage,
    CmdSaveSelectionAsImage,
    CmdSearchGoogleLens,
    CmdSearchGoogleLensPage,
    CmdSearchGoogleLensImage,
    CmdCutAnnotation,
    CmdCopyAnnotation,
    CmdPasteAnnotation,
    0,
};

static uintptr_t removeIfNoDiskAccessPerm[] = {
    CmdNewWindow,
    CmdOpenFile,
    CmdOpenFileNoHistory,
    CmdOpenFileWithOSFilePicker,
    CmdOpenFileWithSumatraFilePicker,
    CmdToggleFilePicker,
    CmdOpenNextFileInFolder,
    CmdOpenPrevFileInFolder,
    CmdNavigateFilesInFolder,
    CmdClose,
    CmdShowInFolder,
    CmdSaveAs,
    CmdSaveSelectionAsImage,
    CmdRenameFile,
    CmdDeleteFile,
    CmdDeleteFileAndOpenNext,
    CmdSendByEmail,
    CmdContributeTranslation,
    CmdAdvancedSettings,
    CmdOpenSettingsFile,
    CmdFavoriteAdd,
    CmdFavoriteDel,
    CmdFavoriteToggle,
    CmdFavoriteShowInTab,
    CmdToggleFavoritesSort,
    CmdOpenSelectedDocument,
    CmdPinSelectedDocument,
    CmdForgetSelectedDocument,
    CmdInvokeInverseSearch,
    CmdSetInverseSearch,
    CmdPasteClipboardImage,
    CmdCreateShortcutToFile,
    CmdSaveEmbeddedFile,
    CmdShowLog,
    CmdShowGeneratedHTML,
    0,
};

static uintptr_t removeIfAnnotsNotSupported[] = {
    CmdSaveAnnotations,
    CmdSaveAnnotationsNewFile,
    CmdDiscardChanges,
    CmdApplyRedactions,
    // signing writes a signature widget into the PDF, so it needs the same
    // "this engine can be edited and re-saved" support annotations do
    CmdSignDocument,
    CmdDeleteAnnotation,
    CmdShowAnnotations,
    CmdHideAnnotations,
    CmdToggleShowAnnotations,
    CmdToggleEditPDF,
    // added past the CmdCreateAnnotFirst..CmdCreateAnnotLast range, so the
    // range check doesn't catch it
    CmdCreateAnnotImageFromClipboard,
    CmdInsertImage,
    CmdSignWithImage,
    CmdAnnotationHighlightBrush,
    CmdFindAnnotation,
    CmdCutAnnotation,
    CmdCopyAnnotation,
    CmdPasteAnnotation,
    CmdUndo,
    CmdRedo,
    0,
};

static uintptr_t removeIfChm[] = {
    CmdSinglePageView,
    CmdFacingView,
    CmdBookView,
    CmdToggleContinuousView,
    CmdRotateLeft,
    CmdRotateRight,
    CmdTogglePresentationMode,
    CmdZoomFitPage,
    CmdZoomActualSize,
    CmdZoomFitWidth,
    CmdZoomFitHeight,
    CmdZoomFitContent,
    CmdZoomFitVisible,
    CmdDebugShowFitContentArea,
    CmdZoomShrinkToFit,
    CmdZoom6400,
    CmdZoom3200,
    CmdZoom1600,
    CmdZoom800,
    CmdZoom12_5,
    CmdZoom8_33,
    CmdInvokeInverseSearch,
    0,
};

static i32 gBlacklistCommandsFromPalette[] = {
    CmdNone,
    CmdCommandPalette,
    CmdCommandPaletteTOC,
    CmdCommandPaletteFavorites,
    CmdNextTabSmart,
    CmdPrevTabSmart,
    CmdSetTheme,
    CmdOpenSelectedDocument,
    CmdPinSelectedDocument,
    CmdForgetSelectedDocument,
    CmdExpandAll,
    CmdCollapseAll,
    CmdTocExpandToLevel1,
    CmdTocExpandToLevel2,
    CmdTocExpandToLevel3,
    CmdTocCollapseSameLevel,
    CmdMoveFrameFocus,
    CmdFavoriteDel,
    CmdPresentationWhiteBackground,
    CmdPresentationBlackBackground,
    CmdSaveEmbeddedFile,
    CmdOpenEmbeddedPDF,
    CmdSaveAttachment,
    CmdOpenAttachment,
    CmdCreateShortcutToFile,
    CmdSetDocumentColorsFollowTheme,
    CmdFileHistory,
    CmdFavorite,
    CmdGoToHomePage,
    0,
};

static i32 gCommandsDebugOnly[] = {
    CmdDebugCorruptMemory,
    CmdDebugCrashMe,
    CmdDebugShowNotif,
    CmdDebugStartStressTest,
    CmdDebugToggleDpiOverride,
    0,
};

static uintptr_t gUnimplementedCommands[] = {
    CmdDebugToggleDpiOverride,
    0,
};

// clang-format on

static bool CmdIdInList(int cmdId, uintptr_t* ids) {
    for (int i = 0; ids[i]; i++) {
        if ((int)ids[i] == cmdId) {
            return true;
        }
    }
    return false;
}

static bool CmdIdInI32List(int cmdId, i32* ids) {
    while (*ids) {
        if (cmdId == *ids) {
            return true;
        }
        ids++;
    }
    return false;
}

static CommandVisibility MapForSurface(CommandVisibility v, CommandSurface surface) {
    if (surface == CommandSurface::Palette && v == CommandVisibility::Disable) {
        return CommandVisibility::Hide;
    }
    return v;
}

bool CmdWorksWithoutDocument(int cmdId) {
    return CmdIdInList(cmdId, gNoDocWhitelist);
}

// ng: NewAppCommandCtx / NewBuildMenuCtx / PopulateTabCloseFlags need
// MainWindow and WindowTab; they live in src/MainWindow.cpp

CommandVisibility GetCommandVisibility(int cmdId, const AppCommandCtx& ctx, CommandSurface surface) {
    if (cmdId <= CmdFirst) {
        return CommandVisibility::Hide;
    }
    if (CmdIdInList(cmdId, gUnimplementedCommands)) {
        return CommandVisibility::Hide;
    }
#if !OS_WIN && !defined(SUMATRA_HAVE_OPENSSL)
    if (cmdId == CmdSignDocument) {
        return CommandVisibility::Hide;
    }
#endif

    CustomCommand* cmd = FindCustomCommand(cmdId);
    int origCmdId = cmd ? cmd->origId : 0;
    if (origCmdId == CmdSetTheme) {
        if (surface == CommandSurface::Palette) {
            return CommandVisibility::Hide;
        }
        return CommandVisibility::Show;
    }

    if (cmdId == CmdInsertTextSnippet) {
        return CommandVisibility::Hide;
    }
    if (origCmdId == CmdInsertTextSnippet && !ctx.supportsAnnots) {
        return CommandVisibility::Hide;
    }

    if (cmdId == CmdAIChatWithClaudeCode || cmdId == CmdAIChatWithGrokBuild || cmdId == CmdAIChatWithOpenAICodex ||
        cmdId == CmdAIChatWithAntiGravity) {
        if (!ctx.aiChatAvailable) {
            return CommandVisibility::Hide;
        }
        // Hide (not disable) so the "AI chat with document" context submenu is
        // empty and dropped for unsupported types (images, comics, DjVu, …).
        if (!ctx.aiChatSupported) {
            return CommandVisibility::Hide;
        }
    }
    if (cmdId == CmdTranslateSelectionWithGrokBuild && !ctx.grokInstalled) {
        return CommandVisibility::Hide;
    }
    if (cmdId == CmdTranslateSelectionWithClaudeCode && !ctx.claudeInstalled) {
        return CommandVisibility::Hide;
    }
    if (cmdId == CmdTranslateSelectionWithOpenAICodex && !ctx.codexInstalled) {
        return CommandVisibility::Hide;
    }
    if (cmdId == CmdTranslateSelectionWithAntiGravity && !ctx.antiGravityInstalled) {
        return CommandVisibility::Hide;
    }

    if (surface == CommandSurface::Palette) {
        if (CmdIdInI32List(cmdId, gCommandsDebugOnly)) {
            if (!gIsDebugBuild) {
                return CommandVisibility::Hide;
            }
        }
        if (CmdIdInI32List(cmdId, gBlacklistCommandsFromPalette)) {
            return CommandVisibility::Hide;
        }
        // context menu keeps the page element under the cursor; palette dispatch
        // has none, so these would no-op
        if (cmdId == CmdCopyImage || cmdId == CmdCopyLinkTarget || cmdId == CmdCopyComment ||
            cmdId == CmdShowAnnotationText) {
            return CommandVisibility::Hide;
        }
        if (cmdId == CmdFixDefaultApp) {
            return CommandVisibility::Hide;
        }
        if (origCmdId == CmdFixDefaultApp) {
            Str ext = GetCommandStringArg(cmd, kCmdArgExt, {});
            if (len(ext) == 0) {
                return CommandVisibility::Hide;
            }
        }
        if (cmdId == CmdInstallPrereleaseUpdate && !HasPendingPreReleaseUpdate()) {
            return CommandVisibility::Hide;
        }
    }

    if (CmdCloseOtherTabs == cmdId) {
        return ctx.canCloseOtherTabs ? CommandVisibility::Show : CommandVisibility::Hide;
    }
    if (CmdCloseTabsToTheRight == cmdId) {
        return ctx.canCloseTabsToRight ? CommandVisibility::Show : CommandVisibility::Hide;
    }
    if (CmdCloseTabsToTheLeft == cmdId) {
        return ctx.canCloseTabsToLeft ? CommandVisibility::Show : CommandVisibility::Hide;
    }
    if (CmdReopenLastClosedFile == cmdId) {
        return RecentlyCloseDocumentsCount() > 0 ? CommandVisibility::Show : CommandVisibility::Hide;
    }
    if (cmdId == CmdTabGroupSave) {
        if (surface == CommandSurface::Palette) {
            return ctx.hasDocTabs ? CommandVisibility::Show : CommandVisibility::Hide;
        }
        // ng: orig asks ctx.tab->win; NewAppCommandCtx answers it up front
        return ctx.hasOpenDocuments ? CommandVisibility::Show : CommandVisibility::Disable;
    }
    if (cmdId == CmdGoToHomePage) {
        return SettingsUseTabs() ? CommandVisibility::Show : CommandVisibility::Hide;
    }
    if (cmdId == CmdNextTab || cmdId == CmdPrevTab || cmdId == CmdNextTabSmart || cmdId == CmdPrevTabSmart ||
        cmdId == CmdMoveTabLeft || cmdId == CmdMoveTabRight) {
        return ctx.nTabs >= 2 ? CommandVisibility::Show : CommandVisibility::Hide;
    }
    // ng: the previewer / search filter and the installer are step 17
    if (cmdId == CmdToggleWindowsPreviewer || cmdId == CmdToggleWindowsSearchFilter ||
        cmdId == CmdTogglePdfPreviewLogging) {
#if NG_HAS_UI
        // toggles a registry flag the installed PdfPreview.dll reads; pointless
        // for a portable build that has no registered preview handler
        if (!IsOurExeInstalled()) {
            return CommandVisibility::Hide;
        }
        if (cmdId == CmdTogglePdfPreviewLogging) {
            return CommandVisibility::Show;
        }
#else
        return CommandVisibility::Hide;
#endif
    }

    if (cmdId == CmdStopReadAloud) {
        if (!ctx.ttsAvailable) {
            return CommandVisibility::Hide;
        }
        Kind k = ctx.engineKind;
        bool isImage =
            k == kindEngineImage || k == kindEngineImageDir || k == kindEngineComicBooks || ctx.isImageCollection;
        if (isImage) {
            return CommandVisibility::Hide;
        }
        // listed while a session is active (speaking or paused) so speech can be
        // stopped when the playback bar is gone; grayed otherwise
        if (ctx.isSpeaking || ctx.canContinueReadAloud) {
            return CommandVisibility::Show;
        }
        return MapForSurface(CommandVisibility::Disable, surface);
    }

    if (CmdWorksWithoutDocument(cmdId)) {
        return MapForSurface(CommandVisibility::Show, surface);
    }

    if (!ctx.isDocLoaded) {
        return CommandVisibility::Hide;
    }

    if ((cmdId == CmdNavigateThumbnail || cmdId == CmdToggleThumbnails) && !ctx.isFixedPage) {
        return CommandVisibility::Hide;
    }

    if (cmdId == CmdShowGeneratedHTML && !ctx.isMarkdown) {
        return CommandVisibility::Hide;
    }

    // a Shortcuts / toolbar entry is a clone with its own id, so it's the
    // command it stands for that decides
    int knownEVCmdId = 0;
    if (IsOpenWithKnownExternalViewerCmd(cmdId)) {
        knownEVCmdId = cmdId;
    } else if (IsOpenWithKnownExternalViewerCmd(cmd)) {
        knownEVCmdId = origCmdId;
    }

    if (ctx.tab && knownEVCmdId) {
        bool canView = CanViewWithKnownExternalViewer(ctx.tab, knownEVCmdId);
        return canView ? CommandVisibility::Show : CommandVisibility::Hide;
    }

    if (origCmdId == CmdViewWithExternalViewer || knownEVCmdId) {
        if (knownEVCmdId) {
            bool canView = HasKnownExternalViewerForCmd(knownEVCmdId);
            return canView ? CommandVisibility::Show : CommandVisibility::Hide;
        }
        Str filter = GetCommandStringArg(cmd, kCmdArgFilter, {});
        // ctx.filePath can be null for an in-memory doc (loaded but no file on
        // disk); such a doc can't match an external-viewer file filter
        bool matches = ctx.filePath && PathMatchFilter(ctx.filePath, filter);
        return matches ? CommandVisibility::Show : CommandVisibility::Hide;
    }

    if ((cmdId == CmdSelectionHandler) || (origCmdId == CmdSelectionHandler) ||
        CmdIdInList(cmdId, disableIfNoSelection)) {
        return ctx.hasSelection ? CommandVisibility::Show : MapForSurface(CommandVisibility::Disable, surface);
    }

    if (cmdId == CmdToggleMenuBar) {
        return ctx.allowToggleMenuBar ? CommandVisibility::Show : CommandVisibility::Hide;
    }

    if (!ctx.supportsAnnots) {
        if ((cmdId >= (int)CmdCreateAnnotFirst) && (cmdId <= (int)CmdCreateAnnotLast)) {
            return CommandVisibility::Hide;
        }
        if (CmdIdInList(cmdId, removeIfAnnotsNotSupported)) {
            return CommandVisibility::Hide;
        }
    }

    if (!ctx.hasTextSelection && CmdIdInList(cmdId, createAnnotFromSelection)) {
        return MapForSurface(CommandVisibility::Disable, surface);
    }

    if (ctx.isChm && CmdIdInList(cmdId, removeIfChm)) {
        return CommandVisibility::Hide;
    }

    if (!ctx.canSendEmail && cmdId == CmdSendByEmail) {
        return CommandVisibility::Hide;
    }

    if (!ctx.isReflowable && cmdId == CmdChangeEbookSettings) {
        // font, line spacing and CSS only mean something for a reflowed document
        return CommandVisibility::Hide;
    }

    if (!ctx.isPdf) {
        if (cmdId == CmdPdfShowInfo || cmdId == CmdPdfBake || cmdId == CmdPdfCompress || cmdId == CmdPdfDecompress ||
            cmdId == CmdPdfEncrypt || cmdId == CmdPdfDecrypt || cmdId == CmdPdfDeletePages ||
            cmdId == CmdPdfExtractPages || cmdId == CmdTogglePageBoxes || cmdId == CmdConvertPdfToImages ||
            cmdId == CmdToggleEditPDF || cmdId == CmdMergePDF) {
            return CommandVisibility::Hide;
        }
    }
    if (ctx.pageCount < 2) {
        if (cmdId == CmdPdfDeletePages || cmdId == CmdPdfExtractPages) {
            return CommandVisibility::Hide;
        }
    }
    if (ctx.isPdf && ctx.isPdfEncrypted && cmdId == CmdPdfEncrypt) {
        return CommandVisibility::Hide;
    }
    if (ctx.isPdf && !ctx.isPdfEncrypted && cmdId == CmdPdfDecrypt) {
        return CommandVisibility::Hide;
    }

    if (!ctx.hasToc && (cmdId == CmdDocumentShowOutline || cmdId == CmdExpandToCurrentPage)) {
        return CommandVisibility::Hide;
    }

    if (cmdId == CmdAutoGenerateTOC && ctx.engineKind != kindEngineMupdf) {
        return CommandVisibility::Hide;
    }

    // ng: orig asks the tab's engine; NewAppCommandCtx answers it up front
    if (cmdId == CmdShowErrors && !ctx.engineHasErrors) {
        return CommandVisibility::Hide;
    }

    if ((cmdId == CmdGoToNextFavorite || cmdId == CmdGoToPrevFavorite) && !HasFavorites()) {
        return CommandVisibility::Hide;
    }

    if (cmdId == CmdDocumentExtractText) {
        bool canExtract = ctx.engineKind == kindEngineMupdf || ctx.engineKind == kindEngineDjVu;
        if (!canExtract || ctx.isImageCollection) {
            return CommandVisibility::Hide;
        }
    }

    bool isTextSelectCmd = cmdId == CmdSelectTextViaKeyboard || cmdId == CmdExtendSelectionCharLeft ||
                           cmdId == CmdExtendSelectionCharRight || cmdId == CmdExtendSelectionWordLeft ||
                           cmdId == CmdExtendSelectionWordRight;
    if (isTextSelectCmd) {
        // needs a fixed-page engine with extractable text: image collections
        // have none and CHM / markdown do their own selection (#4684, #4116)
        Kind k = ctx.engineKind;
        bool isImage = k == kindEngineImage || k == kindEngineImageDir || k == kindEngineComicBooks;
        if (ctx.isImageCollection || isImage || ctx.isChm || !k) {
            return CommandVisibility::Hide;
        }
    }

    if (cmdId == CmdToggleKeyboardLinkFollowing) {
        // pages of image collections (comic books, image folders, single
        // images) can't carry links; CHM / markdown handle their own (#2629)
        Kind k = ctx.engineKind;
        bool isImage = k == kindEngineImage || k == kindEngineImageDir || k == kindEngineComicBooks;
        if (ctx.isImageCollection || isImage || ctx.isChm || !k) {
            return CommandVisibility::Hide;
        }
    }

    if (cmdId == CmdToggleMangaMode) {
        if (!ctx.isFixedPage) {
            return CommandVisibility::Hide;
        }
        // available in single page view too: right-to-left also decides which
        // way the page turns and which side of the canvas advances (#1264)
    }

    if (cmdId == CmdToggleUniformPageWidth && !ctx.isFixedPage) {
        return CommandVisibility::Hide;
    }

    if (cmdId == CmdToggleTrimEmptyMargins && !ctx.isFixedPage) {
        return CommandVisibility::Hide;
    }

    if (cmdId == CmdToggleFreePan && !ctx.isFixedPage) {
        return CommandVisibility::Hide;
    }

    if (cmdId == CmdConvertToPDF) {
        // comic books, image folders, single images (issue #4118)
        Kind k = ctx.engineKind;
        bool isImage =
            k == kindEngineImage || k == kindEngineImageDir || k == kindEngineComicBooks || ctx.isImageCollection;
        if (!ctx.isDocLoaded || !isImage) {
            return CommandVisibility::Hide;
        }
    }

    if (surface == CommandSurface::Palette && ctx.engineKind != kindEngineImage) {
        // The context menu can edit an image embedded in another document because it
        // retains the page element under the cursor. Palette commands only receive the
        // current tab, so these operations are available for standalone images only.
        if (cmdId == CmdSaveImage || cmdId == CmdCropImage || cmdId == CmdResizeImage ||
            cmdId == CmdConvertImageToPdf) {
            return CommandVisibility::Hide;
        }
    }

    if (!ctx.annotationUnderCursor && cmdId == CmdDeleteAnnotation) {
        return MapForSurface(CommandVisibility::Disable, surface);
    }

    if (cmdId == CmdUndo || cmdId == CmdRedo) {
        bool can = (cmdId == CmdUndo) ? ctx.canUndo : ctx.canRedo;
        return can ? CommandVisibility::Show : MapForSurface(CommandVisibility::Disable, surface);
    }

    if (cmdId == CmdCopyAnnotation || cmdId == CmdCutAnnotation) {
        // same targeting as CmdDeleteAnnotation: the annotation under the cursor
        // (right-click doesn't select), otherwise the selected one
        // ng: NewAppCommandCtx already falls back to the tab's selected
        // annotation, which this file cannot reach (no WindowTab here)
        Annotation* annot = ctx.annotationUnderCursor;
        bool can = AnnotationIsLive(annot) && AnnotationCanBeCopied(annot->type);
        return can ? CommandVisibility::Show : MapForSurface(CommandVisibility::Disable, surface);
    }
    if (cmdId == CmdPasteAnnotation) {
        return HasCopiedAnnotation() ? CommandVisibility::Show : MapForSurface(CommandVisibility::Disable, surface);
    }

    if ((cmdId == CmdSaveAnnotations) || (cmdId == CmdSaveAnnotationsNewFile) || (cmdId == CmdDiscardChanges)) {
        return ctx.hasUnsavedAnnotations ? CommandVisibility::Show : MapForSurface(CommandVisibility::Disable, surface);
    }

    if (cmdId == CmdShowAnnotations) {
        return ctx.hideAnnotations ? CommandVisibility::Show : MapForSurface(CommandVisibility::Disable, surface);
    }
    if (cmdId == CmdHideAnnotations) {
        return ctx.hideAnnotations ? MapForSurface(CommandVisibility::Disable, surface) : CommandVisibility::Show;
    }

    if (cmdId == CmdCreateAnnotImageFromClipboard && !ctx.clipboardHasImage && !ctx.clipboardReadAsync) {
        return MapForSurface(CommandVisibility::Disable, surface);
    }

    if (cmdId == CmdApplyRedactions) {
        // the toolbar button is for marks made in this session: marks that came
        // with the file only surface as their page gets loaded, so the button
        // would pop up out of nowhere (e.g. when an annotation is selected).
        // The menu and the palette still offer to apply those
        bool marks = (surface == CommandSurface::Toolbar) ? ctx.hasUserRedactMarks : ctx.hasRedactMarks;
        if (marks) {
            return CommandVisibility::Show;
        }
        // the annotation toolbar omits a greyed button; the menu keeps the
        // item disabled, like Save / Discard with nothing to write
        if (surface == CommandSurface::Toolbar || surface == CommandSurface::Palette) {
            return CommandVisibility::Hide;
        }
        return CommandVisibility::Disable;
    }

    if ((cmdId == CmdCheckUpdate) && gIsStoreBuild) {
        return CommandVisibility::Hide;
    }

    if (!HasPermission(Perm::InternetAccess) && CmdIdInList(cmdId, removeIfNoInternetPerms)) {
        return CommandVisibility::Hide;
    }
    if (!HasPermission(Perm::FullscreenAccess) && CmdIdInList(cmdId, removeIfNoFullscreenPerms)) {
        return CommandVisibility::Hide;
    }
    if (!HasPermission(Perm::SavePreferences) && CmdIdInList(cmdId, removeIfNoPrefsPerms)) {
        return CommandVisibility::Hide;
    }
    if (!HasPermission(Perm::PrinterAccess) && (cmdId == CmdPrint || cmdId == CmdPrintSelection)) {
        return CommandVisibility::Hide;
    }
    // orig's MenuUpdatePrintItem also relabels the row "&Print... (denied)"
    if (!ctx.allowsPrinting && cmdId == CmdPrint) {
        return CommandVisibility::Disable;
    }
    if (!CanAccessDisk()) {
        if (CmdIdInList(cmdId, removeIfNoDiskAccessPerm)) {
            return CommandVisibility::Hide;
        }
        if (CmdIdInList(cmdId, removeIfAnnotsNotSupported)) {
            return CommandVisibility::Hide;
        }
        if (IsOpenWithKnownExternalViewerCmd(cmdId)) {
            return CommandVisibility::Hide;
        }
    }
    if (!HasPermission(Perm::CopySelection) && CmdIdInList(cmdId, removeIfNoCopyPerms)) {
        return CommandVisibility::Hide;
    }

    if (!ctx.cursorOnLinkTarget && cmdId == CmdCopyLinkTarget) {
        return CommandVisibility::Hide;
    }
    if (!ctx.cursorOnComment && (cmdId == CmdCopyComment || cmdId == CmdShowAnnotationText)) {
        return CommandVisibility::Hide;
    }
    if (!ctx.cursorOnImage && cmdId == CmdCopyImage) {
        return CommandVisibility::Hide;
    }
    if (cmdId == CmdPrintSelection) {
        bool isRect = ctx.hasSelection && !ctx.hasTextSelection;
        if (!isRect) {
            return CommandVisibility::Hide;
        }
        return ctx.allowsPrinting ? CommandVisibility::Show : CommandVisibility::Disable;
    }
    if (cmdId == CmdCopySelectionAsImage || cmdId == CmdSaveSelectionAsImage) {
        bool isRect = ctx.hasSelection && !ctx.hasTextSelection;
        return isRect ? CommandVisibility::Show : CommandVisibility::Hide;
    }
    if (cmdId == CmdSearchGoogleLens) {
        bool can = ctx.isFixedPage && ctx.hasSelection;
        return can ? CommandVisibility::Show : MapForSurface(CommandVisibility::Disable, surface);
    }
    if (cmdId == CmdSearchGoogleLensPage) {
        if (surface == CommandSurface::Palette) {
            return ctx.isFixedPage ? CommandVisibility::Show : CommandVisibility::Hide;
        }
        return ctx.isCursorOnPage ? CommandVisibility::Show : CommandVisibility::Hide;
    }
    if (cmdId == CmdSearchGoogleLensImage) {
        bool onImage = ctx.cursorOnImage || ctx.engineKind == kindEngineImage;
        return onImage ? CommandVisibility::Show : CommandVisibility::Hide;
    }
    if ((cmdId == CmdToggleBookmarks) || (cmdId == CmdToggleTableOfContents)) {
        return ctx.hasToc ? CommandVisibility::Show : CommandVisibility::Hide;
    }

    // ng: no TTS engine (any platform but Windows)
    if (cmdId == CmdToggleToolbarShowReadAloud && !ctx.ttsAvailable) {
        return CommandVisibility::Hide;
    }

    // No extractable text on comics, image folders, or single images.
    if (cmdId == CmdToggleReadAloud || cmdId == CmdReadAloudFromTopPage || cmdId == CmdReadAloudSelection ||
        cmdId == CmdReadAloudFromCursorPosition || cmdId == CmdPauseReadAloud || cmdId == CmdContinueReadAloud) {
        if (!ctx.ttsAvailable) {
            return CommandVisibility::Hide;
        }
        Kind k = ctx.engineKind;
        bool isImage =
            k == kindEngineImage || k == kindEngineImageDir || k == kindEngineComicBooks || ctx.isImageCollection;
        if (isImage) {
            return CommandVisibility::Hide;
        }
    }
    if (cmdId == CmdPauseReadAloud) {
        return ctx.isSpeaking ? CommandVisibility::Show : CommandVisibility::Hide;
    }
    if (cmdId == CmdContinueReadAloud) {
        return (ctx.canContinueReadAloud && !ctx.isSpeaking) ? CommandVisibility::Show : CommandVisibility::Hide;
    }
    if (cmdId == CmdReadAloudSelection) {
        return ctx.hasSelection ? CommandVisibility::Show : CommandVisibility::Hide;
    }

    if (cmdId == CmdToggleAutomaticallyScroll || cmdId == CmdAutomaticallyScrollFaster ||
        cmdId == CmdAutomaticallyScrollSlower) {
        if (!ctx.isFixedPage) {
            return CommandVisibility::Hide;
        }
        if (cmdId != CmdToggleAutomaticallyScroll && !ctx.autoScrollOn) {
            return CommandVisibility::Hide;
        }
    }
    if (cmdId == CmdToggleReadingBar || cmdId == CmdToggleReadingBarInvert) {
        if (!ctx.isFixedPage) {
            return CommandVisibility::Hide;
        }
        if (cmdId == CmdToggleReadingBarInvert && !ctx.readingBarOn) {
            return CommandVisibility::Hide;
        }
    }

    return MapForSurface(CommandVisibility::Show, surface);
}

void GetCommandIdState(AppCommandCtx* ctx, int cmdId, bool* removeOut, bool* disableOut) {
    AppCommandCtx empty;
    CommandVisibility v = GetCommandVisibility(cmdId, ctx ? *ctx : empty, CommandSurface::Menu);
    *removeOut = CommandShouldRemove(v);
    *disableOut = CommandShouldDisable(v);
}
