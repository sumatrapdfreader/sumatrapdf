/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct WindowTab;
struct Annotation;

enum class CommandVisibility {
    Show,
    Disable,
    Hide,
};

enum class CommandSurface {
    Menu,
    Palette,
    Toolbar,
};

struct AppCommandCtx {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;

    bool isDocLoaded = false;
    Str filePath;
    Kind engineKind = nullptr;
    int pageCount = 0;
    bool isPdf = false;
    bool isPdfEncrypted = false;
    bool isChm = false;
    bool isMarkdown = false;
    bool isCbx = false;
    bool isImageCollection = false;
    // reflowable document (epub, mobi, fb2, ...): the ebook settings apply
    bool isReflowable = false;
    bool isFixedPage = false;
    bool isSinglePage = false;
    bool hasToc = false;

    Point cursorPos;
    bool hasSelection = false;
    // selection is text (as opposed to rectangular block selection)
    bool hasTextSelection = false;
    bool isCursorOnPage = false;
    Annotation* annotationUnderCursor = nullptr;
    bool cursorOnLinkTarget = false;
    bool cursorOnComment = false;
    bool cursorOnImage = false;

    bool supportsAnnots = false;
    bool hideAnnotations = false;
    bool hasUnsavedAnnotations = false;
    // any redaction mark in the loaded pages, including marks that came with
    // the file
    bool hasRedactMarks = false;
    // redaction marks made in this session (see EngineHasUserRedactMarks)
    bool hasUserRedactMarks = false;
    bool canUndo = false;
    bool canRedo = false;
    bool clipboardHasImage = false;
    bool clipboardReadAsync = false;

    int nTabs = 0;
    bool hasDocTabs = false;
    // ng: orig asks win / the engine at the point of the rule; keeping the
    // answers here keeps this file free of MainWindow / WindowTab
    bool hasOpenDocuments = false;
    bool engineHasErrors = false;
    bool canCloseOtherTabs = false;
    bool canCloseTabsToRight = false;
    bool canCloseTabsToLeft = false;

    bool canSendEmail = false;
    // the document's own permission (orig's MenuUpdatePrintItem)
    bool allowsPrinting = true;
    bool allowToggleMenuBar = true;
    bool isSpeaking = false;
    bool canContinueReadAloud = false;
    // ng: no TTS engine on this platform: every read aloud command is hidden
    bool ttsAvailable = false;
    // ng: the AI chat answers (an embedded browser, a supported document and
    // one installed provider CLI per backend) are filled in by the shell so
    // this file stays free of AIChat*
    bool aiChatAvailable = false;
    bool aiChatSupported = false;
    bool grokInstalled = false;
    bool claudeInstalled = false;
    bool codexInstalled = false;
    bool antiGravityInstalled = false;
    // WindowTab::autoScroll / readingBar, answered up front for the same reason
    bool autoScrollOn = false;
    bool readingBarOn = false;
};

using BuildMenuCtx = AppCommandCtx;

// ng: filling an AppCommandCtx in needs MainWindow / WindowTab, so it lives in
// the shell (src/MainWindow.cpp); the policy over a filled-in one is here
AppCommandCtx NewAppCommandCtx(MainWindow* win, Point cursorPos = {});

CommandVisibility GetCommandVisibility(int cmdId, const AppCommandCtx& ctx, CommandSurface surface);

bool CmdWorksWithoutDocument(int cmdId);

void GetCommandIdState(AppCommandCtx* ctx, int cmdId, bool* removeOut, bool* disableOut);

BuildMenuCtx* NewBuildMenuCtx(WindowTab* tab, Point pt);
void DeleteBuildMenuCtx(BuildMenuCtx* ctx);

inline bool CommandShouldRemove(CommandVisibility v) {
    return v == CommandVisibility::Hide;
}

inline bool CommandShouldDisable(CommandVisibility v) {
    return v == CommandVisibility::Disable;
}

inline bool CommandShouldShow(CommandVisibility v) {
    return v != CommandVisibility::Hide;
}

// used by Menu.cpp for live menu updates (not visibility policy)
// ng: UINT_PTR -> uintptr_t (same type; win32 spelling is not portable)
extern uintptr_t disableIfNoSelection[];
