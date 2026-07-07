/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "TextSelection.h"
#include "GlobalPrefs.h"
#include "Annotation.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "ExternalViewers.h"
#include "FileHistory.h"
#include "Installer.h"
#include "ClaudeCode.h"
#include "GrokBuild.h"
#include "CodexBuild.h"
#include "TextToSpeech.h"
#include "CommandAvailability.h"

// clang-format off

static UINT_PTR gNoDocWhitelist[] = {
    CmdOpenFile,
    CmdExit,
    CmdNewWindow,
    CmdContributeTranslation,
    CmdOptions,
    CmdSetInverseSearch,
    CmdAdvancedOptions,
    CmdAdvancedSettings,
    CmdChangeLanguage,
    CmdCheckUpdate,
    CmdHelpOpenManual,
    CmdHelpOpenManualOnWebsite,
    CmdHelpOpenKeyboardShortcuts,
    CmdHelpVisitWebsite,
    CmdHelpAbout,
    CmdDebugDownloadSymbols,
    CmdDebugShowNotif,
    CmdDebugStartStressTest,
    CmdDebugTestApp,
    CmdDebugTogglePredictiveRender,
    CmdDebugToggleRenderInfo,
    CmdDebugToggleCacheInfo,
    CmdDebugToggleRtl,
    CmdChangeScrollbar,
    CmdToggleAntiAlias,
    CmdToggleSmoothScroll,
    CmdToggleScrollbarInSinglePage,
    CmdToggleLazyLoading,
    CmdToggleEscToExit,
    CmdToggleFullscreen,
    CmdToggleMenuBar,
    CmdToggleToolbar,
    CmdToggleToolbarPosition,
    CmdToggleUseTabs,
    CmdToggleTabsMru,
    CmdToggleTips,
    CmdToggleFrequentlyRead,
    CmdToggleChmUI,
    CmdToggleMarkdownUI,
    CmdToggleReuseInstance,
    CmdToggleHoverPreview,
    CmdToggleInverseSearch,
    CmdToggleLinks,
    CmdToggleWindowsPreviewer,
    CmdToggleWindowsSearchFilter,
    CmdInvertColors,
    CmdFavoriteToggle,
    CmdShowLog,
    CmdClearHistory,
    CmdRemoveDeletedFilesFromHistory,
    CmdReopenLastClosedFile,
    CmdSelectNextTheme,
    CmdListPrinters,
    CmdDebugCrashMe,
    CmdDebugCorruptMemory,
    CmdScreenshot,
    CmdPasteClipboardImage,
    CmdTabGroupRestore,
    CmdSetScreenshotHotkey,
    0,
};

UINT_PTR disableIfNoSelection[] = {
    CmdCopySelection,
    CmdFindNextSel,
    CmdFindPrevSel,
    CmdTranslateSelectionWithDeepL,
    CmdTranslateSelectionWithGoogle,
    CmdTranslateSelectionWithGrokBuild,
    CmdTranslateSelectionWithClaudeCode,
    CmdTranslateSelectionWithOpenAICodex,
    CmdSearchSelectionWithWikipedia,
    CmdSearchSelectionWithGoogleScholar,
    CmdSearchSelectionWithBing,
    CmdSearchSelectionWithGoogle,
    0,
};

// annotations created from a text selection; a rectangular selection has no
// text to mark up. checked after !supportsAnnots already hid them for non-PDF
static UINT_PTR createAnnotFromSelection[] = {
    CmdCreateAnnotHighlight,
    CmdCreateAnnotSquiggly,
    CmdCreateAnnotStrikeOut,
    CmdCreateAnnotUnderline,
    0,
};

static UINT_PTR removeIfNoInternetPerms[] = {
    CmdCheckUpdate,
    CmdTranslateSelectionWithGoogle,
    CmdTranslateSelectionWithDeepL,
    CmdSearchSelectionWithGoogle,
    CmdSearchSelectionWithBing,
    CmdSearchSelectionWithWikipedia,
    CmdSearchSelectionWithGoogleScholar,
    CmdHelpVisitWebsite,
    CmdHelpOpenManualOnWebsite,
    CmdHelpOpenKeyboardShortcuts,
    CmdContributeTranslation,
    0,
};

static UINT_PTR removeIfNoFullscreenPerms[] = {
    CmdTogglePresentationMode,
    CmdToggleFullscreen,
    0,
};

static UINT_PTR removeIfNoPrefsPerms[] = {
    CmdOptions,
    CmdSetInverseSearch,
    CmdAdvancedOptions,
    CmdPinSelectedDocument,
    CmdForgetSelectedDocument,
    CmdFavoriteAdd,
    CmdFavoriteDel,
    CmdFavoriteToggle,
    CmdGoToNextFavorite,
    CmdGoToPrevFavorite,
    0,
};

static UINT_PTR removeIfNoCopyPerms[] = {
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
    0,
};

static UINT_PTR removeIfNoDiskAccessPerm[] = {
    CmdNewWindow,
    CmdOpenFile,
    CmdOpenNextFileInFolder,
    CmdOpenPrevFileInFolder,
    CmdNavigateFilesInFolder,
    CmdClose,
    CmdShowInFolder,
    CmdSaveAs,
    CmdRenameFile,
    CmdDeleteFile,
    CmdSendByEmail,
    CmdContributeTranslation,
    CmdAdvancedOptions,
    CmdAdvancedSettings,
    CmdFavoriteAdd,
    CmdFavoriteDel,
    CmdFavoriteToggle,
    CmdOpenSelectedDocument,
    CmdPinSelectedDocument,
    CmdForgetSelectedDocument,
    CmdInvokeInverseSearch,
    CmdSetInverseSearch,
    CmdPasteClipboardImage,
    CmdCreateShortcutToFile,
    CmdSaveEmbeddedFile,
    CmdShowLog,
    0,
};

static UINT_PTR removeIfAnnotsNotSupported[] = {
    CmdSaveAnnotations,
    CmdSaveAnnotationsNewFile,
    CmdEditAnnotations,
    CmdDeleteAnnotation,
    CmdShowAnnotations,
    CmdHideAnnotations,
    CmdToggleShowAnnotations,
    // added past the CmdCreateAnnotFirst..CmdCreateAnnotLast range, so the
    // range check doesn't catch it
    CmdCreateAnnotImageFromClipboard,
    0,
};

static UINT_PTR removeIfChm[] = {
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
    CmdZoomFitContent,
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
    CmdOpenWithKnownExternalViewerFirst,
    CmdOpenWithKnownExternalViewerLast,
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
    CmdMoveFrameFocus,
    CmdFavoriteDel,
    CmdPresentationWhiteBackground,
    CmdPresentationBlackBackground,
    CmdSaveEmbeddedFile,
    CmdOpenEmbeddedPDF,
    CmdSaveAttachment,
    CmdOpenAttachment,
    CmdCreateShortcutToFile,
    0,
};

static i32 gCommandsDebugOnly[] = {
    CmdDebugCorruptMemory,
    CmdDebugCrashMe,
    CmdDebugDownloadSymbols,
    CmdDebugTestApp,
    CmdDebugShowNotif,
    CmdDebugStartStressTest,
    0,
};

// clang-format on

static bool CmdIdInList(int cmdId, UINT_PTR* ids) {
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

static void PopulateTabCloseFlags(AppCommandCtx& ctx) {
    if (!ctx.win) {
        return;
    }
    int nTabs = ctx.win->TabCount();
    ctx.nTabs = nTabs;
    WindowTab* currTab = ctx.tab;
    int tabIdx = ctx.win->GetTabIdx(currTab);
    ctx.canCloseTabsToRight = tabIdx < (nTabs - 1);
    ctx.canCloseTabsToLeft = false;
    int nFirstDocTab = 0;
    for (int i = 0; i < nTabs; i++) {
        WindowTab* t = ctx.win->GetTab(i);
        if (t->IsAboutTab()) {
            nFirstDocTab = 1;
            continue;
        }
        ctx.hasDocTabs = true;
        if (t == currTab) {
            if (i > nFirstDocTab) {
                ctx.canCloseTabsToLeft = true;
            }
            continue;
        }
        ctx.canCloseOtherTabs = true;
    }
}

AppCommandCtx NewAppCommandCtx(MainWindow* win, Point cursorPos) {
    AppCommandCtx ctx;
    ctx.win = win;
    ctx.cursorPos = cursorPos;
    if (!win) {
        return ctx;
    }

    ctx.tab = win->CurrentTab();
    ctx.isDocLoaded = win->IsDocLoaded();
    ctx.filePath = ctx.tab ? ctx.tab->filePath : Str();
    ctx.allowToggleMenuBar = true;

    if (ctx.tab) {
        ctx.isChm = ctx.tab->AsChm() || ctx.tab->AsMarkdown();
        EngineBase* engine = ctx.tab->GetEngine();
        if (engine && engine->kind == kindEngineComicBooks) {
            ctx.isCbx = true;
        }
        if (engine && engine->IsImageCollection()) {
            ctx.isImageCollection = true;
        }
        ctx.engineKind = ctx.tab->GetEngineType();
        ctx.canSendEmail = CanSendAsEmailAttachment(ctx.tab);
        ctx.isPdf = CouldBePDFDoc(ctx.tab);
        if (ctx.isPdf && engine) {
            ctx.isPdfEncrypted = EngineMupdfIsEncrypted(engine);
        }
        ctx.canContinueReadAloud = CanContinueReadAloud(ctx.tab);
    }

    ctx.hasSelection = ctx.isDocLoaded && ctx.tab && win->showSelection && ctx.tab->selectionOnPage;

    if (ctx.isDocLoaded && win->ctrl) {
        ctx.isSinglePage = IsSingle(win->ctrl->GetDisplayMode());
        ctx.pageCount = win->ctrl->PageCount();
        ctx.hasToc = win->ctrl->HasToc();
    }

    DisplayModel* dm = win->AsFixed();
    if (dm) {
        auto engine = dm->GetEngine();
        ctx.hasTextSelection = ctx.hasSelection && dm->textSelection->result.len > 0;
        ctx.supportsAnnots = EngineSupportsAnnotations(engine) && !win->isFullScreen;
        ctx.hasUnsavedAnnotations = EngineHasUnsavedAnnotations(engine);
        int pageNoUnderCursor = dm->GetPageNoByPoint(cursorPos);
        if (pageNoUnderCursor > 0) {
            ctx.isCursorOnPage = true;
        }
        ctx.annotationUnderCursor = dm->GetAnnotationAtPos(cursorPos, nullptr);
        IPageElement* pageEl = dm->GetElementAtPos(cursorPos, nullptr);
        if (pageEl) {
            Str value = pageEl->GetValue();
            ctx.cursorOnLinkTarget = value && pageEl->Is(kindPageElementDest);
            ctx.cursorOnComment = value && pageEl->Is(kindPageElementComment);
            ctx.cursorOnImage = pageEl->Is(kindPageElementImage);
        }
    }

    if (!CanAccessDisk()) {
        ctx.supportsAnnots = false;
        ctx.hasUnsavedAnnotations = false;
    }

    ctx.isSpeaking = TtsIsSpeaking();
    PopulateTabCloseFlags(ctx);
    return ctx;
}

BuildMenuCtx* NewBuildMenuCtx(WindowTab* tab, Point pt) {
    auto* ctx = new AppCommandCtx;
    if (tab && tab->win) {
        *ctx = NewAppCommandCtx(tab->win, pt);
    } else if (tab) {
        ctx->tab = tab;
    }
    return ctx;
}

void DeleteBuildMenuCtx(BuildMenuCtx* ctx) {
    delete ctx;
}

CommandVisibility GetCommandVisibility(int cmdId, const AppCommandCtx& ctx, CommandSurface surface) {
    if (cmdId <= CmdFirst) {
        return CommandVisibility::Hide;
    }

    CustomCommand* cmd = FindCustomCommand(cmdId);
    int origCmdId = cmd ? cmd->origId : 0;
    if (origCmdId == CmdSetTheme) {
        return CommandVisibility::Show;
    }

    if (cmdId == CmdAIChatWithClaudeCode) {
        if (!IsClaudeCodeAvailable()) {
            return CommandVisibility::Hide;
        }
        if (!IsClaudeCodeSupportedForTab(ctx.tab)) {
            return MapForSurface(CommandVisibility::Disable, surface);
        }
    }
    if (cmdId == CmdAIChatWithGrokBuild) {
        if (!IsGrokBuildAvailable()) {
            return CommandVisibility::Hide;
        }
        if (!IsGrokBuildSupportedForTab(ctx.tab)) {
            return MapForSurface(CommandVisibility::Disable, surface);
        }
    }
    if (cmdId == CmdAIChatWithOpenAICodex) {
        if (!IsCodexBuildAvailable()) {
            return CommandVisibility::Hide;
        }
        if (!IsCodexBuildSupportedForTab(ctx.tab)) {
            return MapForSurface(CommandVisibility::Disable, surface);
        }
    }
    if (cmdId == CmdTranslateSelectionWithGrokBuild && !IsGrokBuildInstalled()) {
        return CommandVisibility::Hide;
    }
    if (cmdId == CmdTranslateSelectionWithClaudeCode && !IsClaudeCodeInstalled()) {
        return CommandVisibility::Hide;
    }
    if (cmdId == CmdTranslateSelectionWithOpenAICodex && !IsCodexBuildInstalled()) {
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
        bool enabled = ctx.tab && ctx.tab->win && HasOpenedDocuments(ctx.tab->win);
        return enabled ? CommandVisibility::Show : CommandVisibility::Disable;
    }
    if (cmdId == CmdNextTab || cmdId == CmdPrevTab || cmdId == CmdNextTabSmart || cmdId == CmdPrevTabSmart ||
        cmdId == CmdMoveTabLeft || cmdId == CmdMoveTabRight) {
        return ctx.nTabs >= 2 ? CommandVisibility::Show : CommandVisibility::Hide;
    }
    if ((cmdId == CmdToggleWindowsPreviewer || cmdId == CmdToggleWindowsSearchFilter) && !IsOurExeInstalled()) {
        return CommandVisibility::Hide;
    }
    if (cmdId == CmdTogglePdfPreviewLogging) {
        // toggles a registry flag the installed PdfPreview.dll reads; pointless
        // for a portable build that has no registered preview handler
        return IsOurExeInstalled() ? CommandVisibility::Show : CommandVisibility::Hide;
    }

    if (CmdWorksWithoutDocument(cmdId)) {
        return MapForSurface(CommandVisibility::Show, surface);
    }

    if (!ctx.isDocLoaded) {
        return CommandVisibility::Hide;
    }

    if (ctx.tab) {
        int idFirst = CmdOpenWithKnownExternalViewerFirst + 1;
        int idLast = CmdOpenWithKnownExternalViewerLast;
        if (cmdId >= idFirst && cmdId <= idLast) {
            bool canView = CanViewWithKnownExternalViewer(ctx.tab, cmdId);
            return canView ? CommandVisibility::Show : CommandVisibility::Hide;
        }
    }

    bool isKnownEV = (cmdId >= CmdOpenWithKnownExternalViewerFirst) && (cmdId <= CmdOpenWithKnownExternalViewerLast);
    if (origCmdId == CmdViewWithExternalViewer || isKnownEV) {
        if (isKnownEV) {
            bool canView = HasKnownExternalViewerForCmd(cmdId);
            return canView ? CommandVisibility::Show : CommandVisibility::Hide;
        }
        Str filter = GetCommandStringArg(cmd, kCmdArgFilter, nullptr);
        // ctx.filePath can be null for an in-memory doc (loaded but no file on
        // disk); such a doc can't match an external-viewer file filter
        bool matches = ctx.filePath && PathMatchFilter(ctx.filePath, filter);
        return matches ? CommandVisibility::Show : CommandVisibility::Hide;
    }

    if ((cmdId == CmdSelectionHandler) || (origCmdId == CmdSelectionHandler) ||
        CmdIdInList(cmdId, disableIfNoSelection)) {
        return ctx.hasSelection ? CommandVisibility::Show : MapForSurface(CommandVisibility::Disable, surface);
    }

    if (surface == CommandSurface::Palette && cmdId == CmdToggleFrequentlyRead) {
        return CommandVisibility::Hide;
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

    if (!ctx.isPdf) {
        if (cmdId == CmdPdShowInfo || cmdId == CmdPdfBake || cmdId == CmdPdfCompress || cmdId == CmdPdfDecompress ||
            cmdId == CmdPdfEncrypt || cmdId == CmdPdfDecrypt || cmdId == CmdPdfDeletePages ||
            cmdId == CmdPdfExtractPages) {
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

    if (!ctx.hasToc && cmdId == CmdDocumentShowOutline) {
        return CommandVisibility::Hide;
    }

    if (cmdId == CmdDocumentExtractText) {
        bool canExtract = ctx.engineKind == kindEngineMupdf || ctx.engineKind == kindEngineDjVu;
        if (!canExtract || ctx.isImageCollection) {
            return CommandVisibility::Hide;
        }
    }

    if (cmdId == CmdToggleMangaMode) {
        if (surface == CommandSurface::Palette && ctx.isSinglePage) {
            return CommandVisibility::Hide;
        }
        if (!ctx.isCbx) {
            return CommandVisibility::Hide;
        }
    }

    if (!ctx.annotationUnderCursor && cmdId == CmdDeleteAnnotation) {
        return CommandVisibility::Disable;
    }

    if ((cmdId == CmdSaveAnnotations) || (cmdId == CmdSaveAnnotationsNewFile)) {
        return ctx.hasUnsavedAnnotations ? CommandVisibility::Show : CommandVisibility::Disable;
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
    if (!HasPermission(Perm::PrinterAccess) && cmdId == CmdPrint) {
        return CommandVisibility::Hide;
    }
    if (!CanAccessDisk()) {
        if (CmdIdInList(cmdId, removeIfNoDiskAccessPerm)) {
            return CommandVisibility::Hide;
        }
        if (CmdIdInList(cmdId, removeIfAnnotsNotSupported)) {
            return CommandVisibility::Hide;
        }
        if (cmdId >= CmdOpenWithKnownExternalViewerFirst && cmdId <= CmdOpenWithKnownExternalViewerLast) {
            return CommandVisibility::Hide;
        }
    }
    if (!HasPermission(Perm::CopySelection) && CmdIdInList(cmdId, removeIfNoCopyPerms)) {
        return CommandVisibility::Hide;
    }

    if (!ctx.cursorOnLinkTarget && cmdId == CmdCopyLinkTarget) {
        return CommandVisibility::Hide;
    }
    if (!ctx.cursorOnComment && cmdId == CmdCopyComment) {
        return CommandVisibility::Hide;
    }
    if (!ctx.cursorOnImage && cmdId == CmdCopyImage) {
        return CommandVisibility::Hide;
    }
    if ((cmdId == CmdToggleBookmarks) || (cmdId == CmdToggleTableOfContents)) {
        return ctx.hasToc ? CommandVisibility::Show : CommandVisibility::Hide;
    }

    if (cmdId == CmdPauseReadAloud) {
        return ctx.isSpeaking ? CommandVisibility::Show : CommandVisibility::Hide;
    }
    if (cmdId == CmdContinueReadAloud) {
        return (ctx.canContinueReadAloud && !ctx.isSpeaking) ? CommandVisibility::Show : CommandVisibility::Hide;
    }
    if (cmdId == CmdStopReadAloud) {
        return (ctx.isSpeaking || ctx.canContinueReadAloud) ? CommandVisibility::Show : CommandVisibility::Hide;
    }
    if (cmdId == CmdReadAloudSelection) {
        return ctx.hasSelection ? CommandVisibility::Show : CommandVisibility::Hide;
    }

    return MapForSurface(CommandVisibility::Show, surface);
}

void GetCommandIdState(AppCommandCtx* ctx, int cmdId, bool* removeOut, bool* disableOut) {
    AppCommandCtx empty;
    CommandVisibility v = GetCommandVisibility(cmdId, ctx ? *ctx : empty, CommandSurface::Menu);
    *removeOut = CommandShouldRemove(v);
    *disableOut = CommandShouldDisable(v);
}