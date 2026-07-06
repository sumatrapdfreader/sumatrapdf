/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct WindowTab;
struct Annotation;
struct Point;

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
    bool isCbx = false;
    bool isImageCollection = false;
    bool isSinglePage = false;
    bool hasToc = false;

    Point cursorPos = {};
    bool hasSelection = false;
    // selection is text (as opposed to rectangular block selection)
    bool hasTextSelection = false;
    bool isCursorOnPage = false;
    Annotation* annotationUnderCursor = nullptr;
    bool cursorOnLinkTarget = false;
    bool cursorOnComment = false;
    bool cursorOnImage = false;

    bool supportsAnnots = false;
    bool hasUnsavedAnnotations = false;

    int nTabs = 0;
    bool hasDocTabs = false;
    bool canCloseOtherTabs = false;
    bool canCloseTabsToRight = false;
    bool canCloseTabsToLeft = false;

    bool canSendEmail = false;
    bool allowToggleMenuBar = true;
    bool isSpeaking = false;
    bool canContinueReadAloud = false;
};

using BuildMenuCtx = AppCommandCtx;

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
extern UINT_PTR disableIfNoSelection[];