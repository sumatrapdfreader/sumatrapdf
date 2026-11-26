/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's Menu.cpp builds win32 HMENUs and owner-draws them. The tables
// below are orig's, unchanged; BuildMenuFromDef fills a portable MenuModel
// instead of an HMENU and the gpui shell renders it (src/gui/AppShell.cpp).
// The context menus, the external viewers, the theme / read-aloud / selection
// submenus and the custom menu-bar rebar come with steps 8-14.

#include "base/Base.h"
#include "base/File.h"
#include "base/Launch.h"
#include "base/Pixmap.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "PagePosition.h"
#include "FileHistory.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "ImageReplace.h"
#include "ReadingBar.h"
#include "ReadingAutoScroll.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "Commands.h"
#include "Favorites.h"
#include "Translations.h"
#include "ShortcutParse.h"
#include "Accelerators.h"
#include "CommandAvailability.h"
#include "ExternalViewers.h"
#include "SumatraDialogs.h"
#include "Annotation.h"
#include "AnnotTextPopup.h"
#include "ImageSaveCropResize.h"
#include "ReadAloud.h"
#include "GoogleLens.h"
#include "gui/DocCanvas.h"
#include "gui/Sidebar.h"
#include "gui/AppShell.h"
#include "Menu.h"
#include "MenuCommon.h"
#include "MenuDefs.h"
#include "HomePage.h"
#include "HomePageCommon.h"
#include "ServerFiles.h"

#include "SumatraLog.h"

// clang-format off
//[ ACCESSKEY_GROUP Context Menu (Main)
static MenuDef menuDefContext[] = {
    {
        TrN("S&election"),
        menuDefSelection,
    },
    {
        TrN("Visual Search with Google &Lens"),
        menuDefGoogleLens,
    },
    {
        TrN("Copy Link &Address"),
        CmdCopyLinkTarget,
    },
    {
        TrN("Copy Co&mment"),
        CmdCopyComment,
    },
    {
        TrN("Sho&w Comment"),
        CmdShowAnnotationText,
    },
    {
        TrN("Save Attachment..."),
        CmdSaveAttachment,
    },
    {
        TrN("Selected &Image"),
        menuDefContextImage,
    },
    // note: strings cannot be "" or else items are not there
    {
        StrL("Add to favorites"),
        CmdFavoriteAdd,
    },
    {
        StrL("Remove from favorites"),
        CmdFavoriteDel,
    },
    {
        TrN("Sh&ow"),
        menuDefShow,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("AI Chat with Document Using"),
        menuDefDocumentAIChat,
    },
    {
        TrN("Document"),
        menuDefDocumentOperations,
    },
    {
        TrN("Read Aloud"),
        menuDefContextReadAloud,
    },
    {
        TrN("Annotations"),
        menuDefContextAnnotations,
    },
    {
        TrN("Show Errors..."),
        CmdShowErrors,
    },
    {
        TrN("E&xit Fullscreen"),
        CmdToggleFullscreen, // only seen in full-screen mode
    },
    {
        {},
        0,
    },
};
//] ACCESSKEY_GROUP Context Menu (Main)

// clang-format on

// clang-format off
// translate / search selection commands need selected text to operate on
static uintptr_t selectionTextCmds[] = {
    CmdTranslateSelectionWithGoogle,
    CmdTranslateSelectionWithDeepL,
    CmdTranslateSelectionWithGrokBuild,
    CmdTranslateSelectionWithClaudeCode,
    CmdTranslateSelectionWithOpenAICodex,
    CmdTranslateSelectionWithAntiGravity,
    CmdSearchSelectionWithGoogle,
    CmdSearchSelectionWithBing,
    CmdSearchSelectionWithWikipedia,
    CmdSearchSelectionWithGoogleScholar,
};

static uintptr_t menusNoTranslate[] = {
    CmdZoom6400,
    CmdZoom3200,
    CmdZoom1600,
    CmdZoom800,
    CmdZoom400,
    CmdZoom200,
    CmdZoom150,
    CmdZoom125,
    CmdZoom100,
    CmdZoom50,
    CmdZoom25,
    CmdZoom12_5,
    CmdZoom8_33,
};

// clang-format on

#define cmdIdInList(name) CmdIdInList((uintptr_t)cmdId, name, dimofi(name))

void DeleteMenuModel(MenuModel* m) {
    if (!m) {
        return;
    }
    for (MenuItemModel& it : m->items) {
        DeleteMenuModel(it.submenu);
        str::Free(it.accel);
        if (it.titleOwned) {
            str::Free(it.title);
        }
    }
    delete m;
}

static MenuItemModel* MenuAppend(MenuModel* m) {
    MenuItemModel item;
    VecAppend(m->items, item);
    return &m->items[m->items.len - 1];
}

// ng: win32's MF_BYCOMMAND searches submenus too, and the context menu code
// relies on that (it edits rows of the Image / Annotations submenus through
// the top-level popup), so MenuFind / MenuRemove recurse
static MenuItemModel* MenuFind(MenuModel* m, int cmdId) {
    for (MenuItemModel& it : m->items) {
        if (it.submenu) {
            MenuItemModel* found = MenuFind(it.submenu, cmdId);
            if (found) {
                return found;
            }
            continue;
        }
        if (it.cmdId == cmdId && !it.separator) {
            return &it;
        }
    }
    return nullptr;
}

void MenuRemove(MenuModel* m, int cmdId) {
    for (int i = 0; i < m->items.len; i++) {
        MenuItemModel& it = m->items[i];
        if (it.submenu) {
            MenuRemove(it.submenu, cmdId);
            continue;
        }
        if (it.cmdId != cmdId || it.separator) {
            continue;
        }
        str::Free(it.accel);
        if (it.titleOwned) {
            str::Free(it.title);
        }
        VecRemoveAt(m->items, i);
        return;
    }
}

// orig removes an Image submenu whose rows were all removed by scanning the
// top level for an empty popup; the same, but for every level
static void MenuRemoveEmptySubmenus(MenuModel* m) {
    for (int i = 0; i < m->items.len; i++) {
        MenuItemModel& it = m->items[i];
        if (!it.submenu) {
            continue;
        }
        MenuRemoveEmptySubmenus(it.submenu);
        if (it.submenu->items.len > 0) {
            continue;
        }
        DeleteMenuModel(it.submenu);
        if (it.titleOwned) {
            str::Free(it.title);
        }
        str::Free(it.accel);
        VecRemoveAt(m->items, i);
        i--;
    }
}

void MenuSetText(MenuModel* m, int cmdId, Str s) {
    MenuItemModel* it = MenuFind(m, cmdId);
    if (!it) {
        return;
    }
    if (it->titleOwned) {
        str::Free(it->title);
    }
    it->title = str::Dup(s);
    it->titleOwned = true;
}

void MenuSetEnabled(MenuModel* m, int cmdId, bool enabled) {
    MenuItemModel* it = MenuFind(m, cmdId);
    if (it) {
        it->disabled = !enabled;
    }
}

void MenuSetChecked(MenuModel* m, int cmdId, bool checked) {
    MenuItemModel* it = MenuFind(m, cmdId);
    if (it) {
        it->checked = checked;
    }
}

void MenuAppendString(MenuModel* m, Str title, int cmdId) {
    MenuItemModel* it = MenuAppend(m);
    it->title = str::Dup(title);
    it->titleOwned = true;
    it->cmdId = cmdId;
}

void MenuAppendSeparator(MenuModel* m) {
    MenuAppend(m)->separator = true;
}

// orig's MenuEmpty(HMENU): drop every row, keeping the model itself
void MenuEmpty(MenuModel* m) {
    for (MenuItemModel& it : m->items) {
        DeleteMenuModel(it.submenu);
        str::Free(it.accel);
        if (it.titleOwned) {
            str::Free(it.title);
        }
    }
    m->items.len = 0;
}

MenuModel* MenuAppendSubmenu(MenuModel* m, Str title) {
    auto* sub = new MenuModel();
    MenuItemModel* it = MenuAppend(m);
    it->title = str::Dup(title);
    it->titleOwned = true;
    it->submenu = sub;
    return sub;
}

// ng: orig removes separators from an HMENU by position; same three rules
void RemoveBadMenuSeparators(MenuModel* menu) {
    Vec<MenuItemModel>& items = menu->items;
    // remove separator items at the beginning
    while (items.len > 0 && items[0].separator) {
        VecRemoveAt(items, 0);
    }
    // remove separator items at the end
    while (items.len > 0 && items[items.len - 1].separator) {
        items.len--;
    }
    // remove 2 or more consecutive separator items
    for (int i = 1; i < items.len; i++) {
        if (items[i].separator && items[i - 1].separator) {
            VecRemoveAt(items, i);
            i--;
        }
    }
}

// orig's AppendCommandsToMenu: the custom commands with a given origId
static void AppendCommandsToMenu(MenuModel* menu, const Vec<CustomCommand*>& cmds, bool isEnabled) {
    for (CustomCommand* cmd : cmds) {
        if (len(cmd->name) == 0) {
            continue;
        }
        MenuItemModel* item = MenuAppend(menu);
        item->title = str::Dup(cmd->name);
        item->titleOwned = true;
        item->cmdId = cmd->id;
        item->disabled = !isEnabled;
        item->accel = str::Dup(ShortcutsForCmdTemp(cmd->id, 1));
    }
}

static void AppendRecentFilesToMenu(MenuModel* menu) {
    if (!CanAccessDisk()) {
        return;
    }
    Vec<FileHistoryEntry> files;
    for (int i = 0; i < kFileHistoryMaxRecent; i++) {
        FileState* fs = FileHistoryGet(i);
        if (!fs || fs->isMissing) {
            break;
        }
        if (len(fs->filePath) > 0) {
            VecAppend(files, FileHistoryEntry{fs->filePath, 0});
        }
    }
    if (len(files) == 0) {
        return;
    }
    SetFileHistoryCmdIds(files);
    for (int i = 0; i < len(files); i++) {
        TempStr name = ShortenStringUtf8InTheMiddleTemp(path::GetBaseNameTemp(files[i].path), 70);
        name = str::ReplaceTemp(name, StrL("&"), StrL("&&"));
        MenuAppendString(menu, fmt("&%d) %s", (i + 1) % 10, name), files[i].cmdId);
    }
    MenuAppendSeparator(menu);
}

static void AppendThemesToMenu(MenuModel* menu) {
    Vec<CustomCommand*> cmds;
    GetCommandsWithOrigId(cmds, CmdSetTheme);
    AppendCommandsToMenu(menu, cmds, true);
}

// the SelectionHandlers entries from the settings file
static void AppendSelectionHandlersToMenu(MenuModel* menu, bool isEnabled) {
    Vec<CustomCommand*> cmds;
    GetCommandsWithOrigId(cmds, CmdSelectionHandler);
    AppendCommandsToMenu(menu, cmds, isEnabled);
}

// one snippet is a row of its own; several share an Insert Text submenu
static void AppendTextSnippetsToMenu(MenuModel* menu) {
    Vec<CustomCommand*> cmds;
    GetCommandsWithOrigId(cmds, CmdInsertTextSnippet);
    if (len(cmds) < 2) {
        AppendCommandsToMenu(menu, cmds, true);
        return;
    }
    MenuModel* sub = MenuAppendSubmenu(menu, Tr("Insert Te&xt"));
    AppendCommandsToMenu(sub, cmds, true);
}

// orig's AppendExternalViewersToMenu: the ExternalViewers entries from the
// settings file, added after the last built-in "Open in ..." row
static void AppendExternalViewersToMenu(MenuModel* menu, Str filePath) {
    if (!CanAccessDisk() || (len(filePath) > 0 && !file::Exists(filePath))) {
        return;
    }
    Vec<CustomCommand*> cmds;
    GetCommandsWithOrigId(cmds, CmdViewWithExternalViewer);
    for (CustomCommand* cmd : cmds) {
        Str commandLine = GetCommandStringArg(cmd, kCmdArgCommandLine, {});
        Str filter = GetCommandStringArg(cmd, kCmdArgFilter, {});
        if (str::IsEmptyOrWhiteSpace(commandLine)) {
            continue;
        }
        if (len(filter) > 0 && !(len(filePath) > 0 && PathMatchFilter(filePath, filter))) {
            continue;
        }
        TempStr name = cmd->name;
        if (str::IsEmptyOrWhiteSpace(name)) {
            name = path::GetBaseNameTemp(commandLine);
            int dotPos = str::IndexOfChar(name, '.');
            if (dotPos >= 0) {
                name = str::DupTemp(Str(name.s, dotPos));
            }
        }
        if (str::IsEmptyOrWhiteSpace(name)) {
            continue;
        }
        MenuItemModel* item = MenuAppend(menu);
        item->title = str::Dup(name);
        item->titleOwned = true;
        item->cmdId = cmd->id;
        item->disabled = len(filePath) == 0;
    }
}

MenuModel* BuildMenuFromDef(MenuDef* menuDef, BuildMenuCtx* ctx) {
    MenuModel* menu = new MenuModel();
    bool isDebugMenu = menuDef == menuDefDebug;
    int i = 0;
    bool addExternalViewersNext = false;

    if (menuDef == menuDefThemes) {
        AppendThemesToMenu(menu);
    }

    while (true) {
        MenuDef md = menuDef[i];
        if (len(md.title) == 0) { // sentinel
            break;
        }
        i++;

        int cmdId = md.cmdId;
        // orig appends the user's external viewers after CmdOpenWithHtmlHelp
        if (addExternalViewersNext && ctx) {
            WindowTab* evTab = ctx->tab;
            AppendExternalViewersToMenu(menu, evTab ? evTab->filePath : Str{});
            addExternalViewersNext = false;
        }
        if (cmdId == CmdOpenWithHtmlHelp) {
            addExternalViewersNext = true;
        }
        // custom selection handlers go before the built-in translate / search submenus
        if (md.submenu == menuDefTranslateWith) {
            // handlers run on selected text; comics and images have none
            bool canSelectText = !ctx || !ctx->isImageCollection;
            if (canSelectText && menuDef == menuDefMainSelection) {
                AppendSelectionHandlersToMenu(menu, true);
            } else if (canSelectText && menuDef == menuDefSelection) {
                AppendSelectionHandlersToMenu(menu, ctx ? ctx->hasSelection : false);
            }
        }
        if (menuDef == menuDefCreateAnnotUnderCursor && cmdId == CmdAnnotationHighlightBrush && ctx &&
            ctx->supportsAnnots) {
            AppendTextSnippetsToMenu(menu);
        }

        if (menuDef == menuDefFile && cmdId == CmdExit) {
            AppendRecentFilesToMenu(menu);
        }

        MenuDef* subMenuDef = md.submenu;
        bool isSubMenu = subMenuDef != nullptr;

        // handle separators before command state checks
        if (str::Eq(md.title, StrL(kMenuSeparator))) {
            MenuAppend(menu)->separator = true;
            continue;
        }

        // Command availability applies to leaf items.
        bool removeMenu = false;
        bool disableMenu = false;
        if (!isSubMenu && ctx) {
            GetCommandIdState(ctx, cmdId, &removeMenu, &disableMenu);
        }
        if (ctx) {
            removeMenu |= (menuDef == menuDefMainSelection) && !ctx->hasTextSelection && cmdIdInList(selectionTextCmds);
            removeMenu |= HideContextMenuEntry(menuDef, md, *ctx);
        }
        removeMenu |= !isSubMenu && cmdId == CmdSignWithImage && len(gSettings->annotations.signatureImage) == 0;
        removeMenu |= ((subMenuDef == menuDefDebug) && !ShowDebugMenu());
        // orig builds the home page's menu bar without a tab, which hides all of
        // Read Aloud; with a window its disabled Stop row would keep the menu
        removeMenu |= ctx && !ctx->isDocLoaded && (subMenuDef == menuDefReadAloud);
        if (removeMenu) {
            continue;
        }

        bool noTranslate = isDebugMenu || (!isSubMenu && cmdIdInList(menusNoTranslate));
        noTranslate |= (subMenuDef == menuDefDebug);
        Str title = md.title;
        if (!noTranslate) {
            title = trans::GetTranslation(md.title);
        }

        if (isSubMenu) {
            MenuModel* subMenu = BuildMenuFromDef(subMenuDef, ctx);
            // orig fills the Favorites menu when the user opens it
            // (WM_INITMENUPOPUP); the model is rebuilt on every change instead
            if (subMenuDef == menuDefFavorites && ctx && ctx->win) {
                RebuildFavMenu(ctx->win, subMenu);
            }
            // same for the Read Aloud submenus: the voices, the speeds and what
            // "Pause" / "Continue" should read are only known now. Their
            // placeholder rows went through GetCommandIdState, so an empty
            // submenu means read aloud is not offered here (image document, no
            // TTS engine) and must stay empty
            if (subMenu->items.len > 0 && ctx && ctx->win) {
                if (subMenuDef == menuDefReadAloud) {
                    RebuildReadAloudMenu(ctx->win, subMenu);
                } else if (subMenuDef == menuDefContextReadAloud) {
                    // a context menu is built as it opens
                    ReadAloudLoadVoices();
                    RebuildReadAloudMenu(ctx->win, subMenu, true, ctx->win->contextMenuPtValid);
                }
            }
            if (subMenu->items.len == 0) {
                DeleteMenuModel(subMenu);
                continue;
            }
            MenuItemModel* item = MenuAppend(menu);
            item->title = title;
            item->submenu = subMenu;
            item->disabled = disableMenu;
        } else {
            MenuItemModel* item = MenuAppend(menu);
            item->title = title;
            item->cmdId = cmdId;
            item->disabled = disableMenu;
            // Ctrl+C / Ctrl+V are bound to CmdCopySelection and
            // CmdPasteClipboardImage, which hand off to these.
            int accelCmd = cmdId;
            if (cmdId == CmdCopyAnnotation) {
                accelCmd = CmdCopySelection;
            } else if (cmdId == CmdPasteAnnotation) {
                accelCmd = CmdPasteClipboardImage;
            }
            item->accel = str::Dup(ShortcutsForCmdTemp(accelCmd, 1));
        }
    }
    RemoveBadMenuSeparators(menu);
    return menu;
}

// true if `it` is one of the page-layout toggles orig's MenuUpdateDisplayMode updates
static bool SetFixedLayoutState(MenuItemModel& it, DisplayModel* dm) {
    bool hasCrop = !dm->GetUserCrop().IsEmpty();
    switch (it.cmdId) {
        case CmdToggleMangaMode:
            it.checked = dm->GetDisplayR2L();
            return true;
        case CmdToggleUniformPageWidth:
            it.checked = dm->GetUniformPageWidth();
            return true;
        case CmdToggleTrimEmptyMargins:
            it.checked = dm->GetTrimEmptyMargins();
            return true;
        case CmdShowWholePages:
            it.disabled = !hasCrop;
            return true;
        case CmdSaveTrimmedPagesAs:
            it.disabled = !hasCrop && !dm->GetTrimEmptyMargins();
            return true;
        case CmdToggleReadColumns:
            it.checked = dm->GetReadColumns();
            return true;
        case CmdToggleColumnsWholePage:
            it.disabled = !dm->GetReadColumns();
            it.checked = dm->ShowsColumnsWholePage();
            return true;
    }
    return false;
}

// ng: orig checks the radio items through MENUITEMINFO; here the model carries
// the flag and the shell draws the tick
// the Show submenu's items are on/off toggles: check the ones that are on
static void SetShowMenuChecks(MenuModel* m, MainWindow* win) {
    for (MenuDef* d = menuDefShow; d->title.s; d++) {
        int cmdId = d->cmdId;
        if (cmdId == 0) {
            continue; // separator
        }
        bool on = false;
        if (ToggleCmdState(win, nullptr, cmdId, on)) {
            MenuSetChecked(m, cmdId, on);
        }
    }
}

static void SetMenuChecks(MenuModel* menu, MainWindow* win) {
    DocController* ctrl = win->ctrl;
    DisplayMode displayMode = gSettings->defaultDisplayModeEnum;
    if (ctrl) {
        displayMode = ctrl->GetDisplayMode();
    }
    int layoutCmd = 0;
    if (IsSingle(displayMode)) {
        layoutCmd = CmdSinglePageView;
    } else if (IsFacing(displayMode)) {
        layoutCmd = CmdFacingView;
    } else if (IsBookView(displayMode)) {
        layoutCmd = CmdBookView;
    }
    int zoomCmd = ctrl ? CmdIdFromVirtualZoom(ctrl->GetZoomVirtual()) : 0;

    for (MenuItemModel& it : menu->items) {
        if (it.submenu) {
            SetMenuChecks(it.submenu, win);
            continue;
        }
        if (it.cmdId == 0) {
            continue;
        }
        if (it.cmdId == CmdNavigateBack) {
            it.disabled = !ctrl || !ctrl->CanNavigate(-1);
            continue;
        }
        if (it.cmdId == CmdNavigateForward) {
            it.disabled = !ctrl || !ctrl->CanNavigate(1);
            continue;
        }
        if (it.cmdId == CmdToggleFilePicker) {
            it.checked = gSettings && str::EqI(gSettings->filePicker, StrL("sumatrapdf"));
            continue;
        }
        if (it.cmdId == gCurrSetThemeCmdId) {
            it.checked = true;
            continue;
        }
        if (it.cmdId >= CmdViewLayoutFirst && it.cmdId <= CmdViewLayoutLast) {
            it.checked = it.cmdId == layoutCmd;
            continue;
        }
        if (it.cmdId == CmdToggleContinuousView) {
            it.checked = IsContinuous(displayMode);
            continue;
        }
        if (it.cmdId == CmdToggleAutoRotateWidePages) {
            it.checked = gSettings && gSettings->comicBookUI.autoRotateWidePages;
            continue;
        }
        if (it.cmdId == CmdToggleMirror) {
            DisplayModel* dmMirror = win->AsFixed();
            it.checked = dmMirror && dmMirror->GetMirror();
            continue;
        }
        // orig's MenuUpdateDisplayMode: the page-layout toggles of a fixed document
        DisplayModel* dm = win->CurrentTab() ? win->AsFixed() : nullptr;
        if (dm && SetFixedLayoutState(it, dm)) {
            continue;
        }
        if (it.cmdId == CmdTogglePageGrid) {
            it.checked = ShowPageGrid();
            continue;
        }
        if (it.cmdId == CmdToggleCaption) {
            it.checked = gSettings->showCaption;
            continue;
        }
        if (it.cmdId == CmdToggleSidebarAutoHide) {
            it.checked = gSettings->sidebarAutoHide;
            continue;
        }
        if (it.cmdId == CmdToggleBookmarks || it.cmdId == CmdToggleTableOfContents) {
            it.checked = SidebarContentVisible(win, SidebarContent::Bookmarks);
            continue;
        }
        if (it.cmdId == CmdToggleThumbnails) {
            it.checked = SidebarContentVisible(win, SidebarContent::Thumbnails);
            continue;
        }
        if (it.cmdId == CmdFavoriteToggle) {
            it.checked = SidebarContentVisible(win, SidebarContent::Favorites);
            continue;
        }
        if (it.cmdId == CmdToggleLinks) {
            it.checked = gSettings->showLinks;
            continue;
        }
        if (it.cmdId == CmdTogglePageBoxes) {
            it.checked = win->showPageBoxes;
            continue;
        }
        if (it.cmdId == CmdToggleTransparencyGrid) {
            it.checked = ShowTransparencyGrid();
            continue;
        }
        if (it.cmdId == CmdMeasure) {
            it.checked = win->measurementToolbar != nullptr;
            continue;
        }
        if (it.cmdId == CmdToggleImages) {
            it.checked = ShowImageOutlines();
            continue;
        }
        if (it.cmdId == CmdDebugShowFitContentArea) {
            it.checked = ShowFitContentArea();
            continue;
        }
        if (it.cmdId == CmdToggleAutomaticallyScroll) {
            it.checked = ReadingAutoScrollIsOn(win);
            continue;
        }
        if (it.cmdId == CmdToggleReadingBar) {
            it.checked = ReadingBarIsOn(win);
            continue;
        }
        if (it.cmdId == CmdToggleReadingBarInvert) {
            it.checked = gSettings && gSettings->readingBar.invert;
            continue;
        }
        if (ctrl && it.cmdId >= CmdZoomFirst && it.cmdId <= CmdZoomLast) {
            it.checked = it.cmdId == zoomCmd;
        }
    }
}

#if OS_DARWIN
// The Edit menu of every Mac application. In a text field its rows edit the
// text (SendEditToInput in AppShell.cpp).
// clang-format off
static MenuDef menuDefEditMac[] = {
    {TrN("&Undo"), CmdUndo},
    {TrN("&Redo"), CmdRedo},
    {StrL(kMenuSeparator), 0},
    {TrN("Cu&t"), CmdCutAnnotation},
    {TrN("&Copy"), CmdCopySelection},
    {TrN("&Paste"), CmdPasteClipboardImage},
    {TrN("Select &All"), CmdSelectAll},
    {{}, 0},
};
// clang-format on

// Rows for the commands, after the row of afterCmd, wherever that is.
static void InsertMenuRowsAfter(MenuModel* menu, int afterCmd, const int* cmds, int nCmds) {
    MenuItemModel* after = MenuFind(menu, afterCmd);
    for (MenuItemModel& top : menu->items) {
        MenuModel* sub = top.submenu;
        int idx = -1;
        for (int i = 0; sub && after && i < len(sub->items); i++) {
            if (&sub->items[i] == after) {
                idx = i;
            }
        }
        if (idx < 0) {
            continue;
        }
        for (int i = 0; i < nCmds; i++) {
            MenuItemModel row;
            row.title = trans::GetTranslation(GetCommandDescription(cmds[i]));
            row.cmdId = cmds[i];
            VecInsertAt(sub->items, ++idx, row);
        }
        return;
    }
}

// The macOS menu bar has About, Settings and Quit in the application menu
// (AppShell.cpp's InstallNativeMenu) and Copy and Select All in Edit. The
// menu bar is the system's: there is no Show Menu.
static void AdaptMenuToMac(MenuModel* menu, BuildMenuCtx* ctx) {
    static const int kMoved[] = {CmdExit, CmdOptions, CmdHelpAbout, CmdToggleMenuBar, CmdCopySelection, CmdSelectAll};
    for (int cmdId : kMoved) {
        MenuRemove(menu, cmdId);
    }
    MenuSetText(menu, CmdShowInFolder, Tr("Show in Finder"));
    MenuSetText(menu, CmdOpenFileWithOSFilePicker, Tr("Open Using macOS File Picker..."));
    for (MenuItemModel& it : menu->items) {
        if (it.submenu) {
            RemoveBadMenuSeparators(it.submenu);
        }
    }

    // Find Next (⌘G) and the rest after Find, so the menu shows their keys
    static const int kFindCmds[] = {CmdFindNext, CmdFindPrev, CmdFindNextSel};
    InsertMenuRowsAfter(menu, CmdFindFirst, kFindCmds, dimof(kFindCmds));
    // File > Close Window (⇧⌘W) after Close (⌘W)
    static const int kCloseCmds[] = {CmdCloseWindow};
    InsertMenuRowsAfter(menu, CmdClose, kCloseCmds, dimof(kCloseCmds));

    MenuItemModel edit;
    edit.title = Tr("&Edit");
    edit.submenu = BuildMenuFromDef(menuDefEditMac, ctx);
    constexpr int kAfterFile = 1;
    VecInsertAt(menu->items, kAfterFile, edit);
}
#endif

MenuModel* BuildMenu(MainWindow* win) {
    AppCommandCtx ctx = NewAppCommandCtx(win);
    MenuModel* menu = BuildMenuFromDef(menuDefMenubar, &ctx);
#if OS_DARWIN
    if (AppShellNativeMenu()) {
        AdaptMenuToMac(menu, &ctx);
    }
#endif
    SetMenuChecks(menu, win);
    SetShowMenuChecks(menu, win);
    return menu;
}

static void AppendMenuRows(str::Builder& out, MenuModel* menu, Str parent) {
    for (int i = 0; i < menu->items.len; i++) {
        const MenuItemModel& item = menu->items[i];
        TempStr path = len(parent) > 0 ? fmt("%s.%d", parent, i) : fmt("%d", i);
        if (item.separator) {
            out.Append(fmt("%s\t-\t\t0\t0\t\t\n", path));
            continue;
        }
        Str kind = item.submenu ? StrL("S") : StrL("I");
        Str cmd = item.cmdId > 0 ? GetCommandName(item.cmdId) : Str{};
        Str title = ParseMenuAccelTextTemp(item.title).display;
        out.Append(fmt("%s\t%s\t%s\t%d\t%d\t%s\t%s\n", path, kind, cmd, item.disabled ? 1 : 0, item.checked ? 1 : 0,
                       title, item.accel));
        if (item.submenu) {
            AppendMenuRows(out, item.submenu, path);
        }
    }
}

static void AppendFavIds(str::Builder& out, MenuModel* menu) {
    if (!menu) {
        return;
    }
    for (int i = 0; i < len(menu->items); i++) {
        const MenuItemModel& item = menu->items[i];
        if (item.submenu) {
            AppendFavIds(out, item.submenu);
            continue;
        }
        CustomCommand* cmd = FindCustomCommand(item.cmdId);
        if (!cmd || cmd->origId != CmdFavorite) {
            continue;
        }
        Str title = ParseMenuAccelTextTemp(item.title).display;
        out.Append(fmt("id=%d text=%s\n", item.cmdId, title));
    }
}

static void AppendHistoryIds(str::Builder& out, MenuModel* menu) {
    if (!menu) {
        return;
    }
    for (int i = 0; i < len(menu->items); i++) {
        const MenuItemModel& item = menu->items[i];
        if (item.submenu) {
            AppendHistoryIds(out, item.submenu);
            continue;
        }
        CustomCommand* cmd = FindCustomCommand(item.cmdId);
        if (!cmd || cmd->origId != CmdFileHistory) {
            continue;
        }
        Str title = ParseMenuAccelTextTemp(item.title).display;
        out.Append(fmt("id=%d text=%s\n", item.cmdId, title));
    }
}

// Recent-file rows as "id=N text=1) name.pdf". Same ids a WM_COMMAND uses.
TempStr FileHistoryMenuIdsTemp(MainWindow* win) {
    if (!win) {
        return {};
    }
    MenuModel* menu = BuildMenu(win);
    str::Builder out;
    AppendHistoryIds(out, menu);
    DeleteMenuModel(menu);
    return ToStrTemp(out);
}

// Favorite rows as "id=N text=Page 1". Ids are the custom commands a
// WM_COMMAND uses, and a later rebuild reuses them.
TempStr FavoritesMenuIdsTemp(MainWindow* win) {
    if (!win) {
        return {};
    }
    MenuModel* menu = BuildMenu(win);
    str::Builder out;
    AppendFavIds(out, menu);
    DeleteMenuModel(menu);
    return ToStrTemp(out);
}

// Stable main-menu dump for debug-control parity checks.
TempStr MainMenuResultTemp(MainWindow* win) {
    MenuModel* menu = BuildMenu(win);
    str::Builder out;
    AppendMenuRows(out, menu, {});
    DeleteMenuModel(menu);
    return ToStrTemp(out);
}

// The page context menu at a canvas point, in the same row format.
TempStr ContextMenuAtPointResultTemp(MainWindow* win, int x, int y, int invoke) {
    if (!win) {
        return StrL("NOTREADY no-window\n");
    }
    MenuModel* menu = nullptr;
    if (win->IsCurrentTabAbout()) {
        // the home page: the menu of the selected entry, else the first
        StrVec paths;
        HomePageMenuPaths(win, {}, paths);
        menu = BuildHomeContextMenu(win, paths);
        if (invoke != 0) {
            const MenuItemModel* item = menu ? MenuFind(menu, invoke) : nullptr;
            bool enabled = item && !item->disabled;
            DeleteMenuModel(menu);
            if (!enabled) {
                return StrL("ERR command unavailable");
            }
            bool blocked = gBlockExternalLaunch;
            if (invoke == CmdShowInFolder) {
                gBlockExternalLaunch = true;
            }
            int before = gExternalLaunchesBlocked;
            HomeContextMenuCommand(win, paths, invoke);
            gBlockExternalLaunch = blocked;
            return fmt("OK launchesBlocked=%d", gExternalLaunchesBlocked - before);
        }
    } else {
        menu = BuildWindowContextMenu(win, Point{x, y});
    }
    if (!menu) {
        return Str{};
    }
    str::Builder out;
    AppendMenuRows(out, menu, {});
    DeleteMenuModel(menu);
    return ToStrTemp(out);
}

// --- the page context menu (orig's OnWindowContextMenu) ---------------------

// orig puts the whole menu bar under a "Menu" row at the top while full screen
static void PrependMenuBarSubmenu(MenuModel* popup, MenuModel* bar) {
    MenuItemModel item;
    item.title = str::Dup(Tr("Menu"));
    item.titleOwned = true;
    item.submenu = bar;
    VecInsertAt(popup->items, 0, item);
}

// one line naming every row, so a scripted session can check the menu against
// orig's without a screenshot (a popup is not in what PrintWindow captures)
// ng: recurses into submenus so one log line shows the whole popup
static TempStr MenuRowsTemp(MenuModel* m) {
    str::Builder b;
    for (const MenuItemModel& it : m->items) {
        if (len(ToStr(b)) > 0) {
            b.Append(StrL(", "));
        }
        if (it.separator) {
            b.Append(StrL("---"));
            continue;
        }
        b.Append(ParseMenuAccelTextTemp(it.title).display);
        if (it.submenu) {
            b.AppendChar('>');
            b.Append(fmt(" (%s)", MenuRowsTemp(it.submenu)));
        }
        if (it.disabled) {
            b.Append(StrL("(off)"));
        }
        if (it.checked) {
            b.Append(StrL("(on)"));
        }
    }
    return ToStrTemp(b);
}

// ng: orig tracks the popup and acts on what TrackPopupMenu returns. gpui
// builds the popup from this model in the frame after the right button went
// down and reports the pick as an action, so the two halves are separate:
// this makes the model and remembers the point, WindowContextMenuCommand()
// runs the command against that point.
MenuModel* BuildWindowContextMenu(MainWindow* win, Point cursorPos) {
    DisplayModel* dm = win->AsFixed();
    if (!dm || !win->ctrl) {
        return nullptr;
    }
    WindowTab* tab = win->CurrentTab();
    int pageNoUnderEl = 0;
    IPageElement* pageEl = dm->GetElementAtPos(cursorPos, &pageNoUnderEl);

    // ng: orig rebuilds the Read Aloud context submenu after the popup is
    // built; here BuildMenuFromDef fills it, so the point it reads has to be
    // on the window first
    EngineBase* engine = dm->GetEngine();
    bool isImageDoc = engine && (engine->isImageCollection || engine->kind == kindEngineImage ||
                                 engine->kind == kindEngineImageDir || engine->kind == kindEngineComicBooks);
    win->contextMenuPt = cursorPos;
    win->contextMenuPtValid = !isImageDoc && ReadAloudCanReadFromCursor(dm, cursorPos);

    BuildMenuCtx* ctx = NewBuildMenuCtx(tab, cursorPos);
    MenuModel* popup = BuildMenuFromDef(menuDefContext, ctx);
    // before the fullscreen menu bar goes in front: MenuSetChecked sets the first match
    SetShowMenuChecks(popup, win);

    // in fullscreen, add "Menu" as first item containing the full menu bar
    bool isFullScreen = win->IsFullScreenOrPresentation();
    if (isFullScreen) {
        MenuModel* bar = BuildMenuFromDef(menuDefMenubar, ctx);
        SetMenuChecks(bar, win);
        SetShowMenuChecks(bar, win);
        PrependMenuBarSubmenu(popup, bar);
    }

    int pageNoUnderCursor = dm->GetPageNoByPoint(cursorPos);
    win->contextMenuPageNo = pageNoUnderCursor;
    if (pageNoUnderCursor > 0) {
        win->contextMenuPagePt = dm->CvtFromScreen(cursorPos, pageNoUnderCursor);
    }

    bool onImage = pageEl && pageEl->Is(kindPageElementImage);
    onImage = onImage || (engine && engine->kind == kindEngineImage);
    if (pageNoUnderCursor > 0) {
        TempStr pageItem;
        if (ShowChapterUi(win->ctrl)) {
            Location loc = win->ctrl->LocationFromPageNo(pageNoUnderCursor);
            pageItem = fmt(Tr("Chapter %d Page %d").s, loc.chapter, loc.page);
        } else {
            TempStr pageLabel = win->ctrl->GetPageLabelTemp(pageNoUnderCursor);
            pageItem = fmt(Tr("Page %s").s, pageLabel);
        }
        MenuSetText(popup, CmdSearchGoogleLensPage, pageItem);
    }

    // show "Save Attachment" only for file attachment annotations
    {
        bool isFileAttachment = false;
        if (pageEl && pageEl->Is(kindPageElementDest)) {
            IPageDestination* elDest = pageEl->AsLink();
            if (elDest && elDest->GetKind() == kindDestinationLaunchEmbedded) {
                isFileAttachment = true;
            }
        }
        if (!isFileAttachment) {
            MenuRemove(popup, CmdSaveAttachment);
        }
    }
    {
        bool isImageEngine = tab && tab->GetEngineType() == kindEngineImage;
        if (!onImage && !isImageEngine) {
            MenuRemove(popup, CmdCopyImage);
        }
        if (!onImage) {
            MenuRemove(popup, CmdSaveImage);
        }
        if (!pageEl || !pageEl->Is(kindPageElementImage)) {
            MenuRemove(popup, CmdReplaceImage);
            MenuRemove(popup, CmdEditImageExternally);
        }
        if (!isImageEngine && !onImage) {
            MenuRemove(popup, CmdCropImage);
            MenuRemove(popup, CmdResizeImage);
            MenuRemove(popup, CmdConvertImageToPdf);
        }
    }

    if (!engine || !engine->HasErrors()) {
        MenuRemove(popup, CmdShowErrors);
    }
    if (!isFullScreen) {
        MenuRemove(popup, CmdToggleFullscreen);
    }

    MenuSetEnabled(popup, CmdToggleBookmarks, win->ctrl->HasToc());
    MenuSetChecked(popup, CmdToggleBookmarks, SidebarContentVisible(win, SidebarContent::Bookmarks));
    MenuSetChecked(popup, CmdToggleThumbnails, SidebarContentVisible(win, SidebarContent::Thumbnails));

    MenuSetEnabled(popup, CmdFavoriteToggle, HasFavorites());
    MenuSetEnabled(popup, CmdEditFavorites, HasFavorites());
    MenuSetChecked(popup, CmdFavoriteToggle, SidebarContentVisible(win, SidebarContent::Favorites));

    Str filePath = win->ctrl->GetFilePath();
    bool favsSupported = HasPermission(Perm::SavePreferences) && CanAccessDisk();
    if (favsSupported) {
        if (pageNoUnderCursor > 0) {
            bool isBookmarked = IsPageInFavorites(filePath, pageNoUnderCursor, win->ctrl);

            TempStr addText;
            TempStr delText;
            if (ShowChapterUi(win->ctrl)) {
                Location loc = win->ctrl->LocationFromPageNo(pageNoUnderCursor);
                addText = fmt(Tr("Add Chapter %d Page %d to Favorites...").s, loc.chapter, loc.page);
                delText = fmt(Tr("Remove Chapter %d Page %d from Favorites").s, loc.chapter, loc.page);
            } else {
                TempStr pageLabel = win->ctrl->GetPageLabelTemp(pageNoUnderCursor);
                addText = fmt(Tr("Add Page %s to Favorites...").s, pageLabel);
                delText = fmt(Tr("Remove Page %s from Favorites").s, pageLabel);
            }

            if (isBookmarked) {
                MenuRemove(popup, CmdFavoriteAdd);
                MenuSetText(popup, CmdFavoriteDel, delText);
            } else {
                MenuRemove(popup, CmdFavoriteDel);
                MenuSetText(popup, CmdFavoriteAdd, addText);
            }
        } else {
            MenuRemove(popup, CmdFavoriteAdd);
            MenuRemove(popup, CmdFavoriteDel);
        }
    }

    MenuRemoveEmptySubmenus(popup);
    RemoveBadMenuSeparators(popup);
    DeleteBuildMenuCtx(ctx);

    logf("BuildWindowContextMenu: at %d,%d: %s\n", cursorPos.x, cursorPos.y, MenuRowsTemp(popup));

    // highlight the element under cursor while the context menu is open
    if (pageEl && pageNoUnderEl > 0) {
        win->contextMenuHighlightRect = pageEl->GetRect();
        win->contextMenuHighlightPageNo = pageNoUnderEl;
    }
    return popup;
}

// The image the context menu's Save / Crop / Resize / Convert to PDF act on:
// a standalone image file is loaded from disk (so Save can write the original
// bytes back), an image inside a document is rendered out of the engine.
static void ContextMenuEditImage(MainWindow* win, IPageElement* pageEl, int pageNo, int cmdId) {
    DisplayModel* dm = win->AsFixed();
    if (!dm || !pageEl || !pageEl->Is(kindPageElementImage)) {
        ExecuteCmd(win, cmdId);
        return;
    }
    EngineBase* imgEngine = dm->GetEngine();
    ImageEditMode m = ImageEditMode::Save;
    bool selectPdf = false;
    if (cmdId == CmdCropImage) {
        m = ImageEditMode::Crop;
    } else if (cmdId == CmdResizeImage) {
        m = ImageEditMode::Resize;
    } else if (cmdId == CmdConvertImageToPdf) {
        selectPdf = true;
    }
    if (imgEngine->kind == kindEngineImage && len(imgEngine->FilePath()) > 0) {
        ShowImageEditWindow(win, m, imgEngine->FilePath(), nullptr, selectPdf);
        return;
    }
    Pixmap* px = PageElementImagePixmap(imgEngine, pageEl);
    if (!px) {
        return;
    }
    Str filePath = win->ctrl ? win->ctrl->GetFilePath() : Str{};
    TempStr dir = path::GetDirTemp(filePath);
    TempStr base = path::GetBaseNameTemp(filePath);
    TempStr noExt = path::GetPathNoExtTemp(base);
    Str origData = imgEngine->GetImageDataForPageElement(pageEl);
    Str ext = ImageSaveExtFromData(origData);
    if (len(ext) == 0) {
        ext = StrL(".png");
    }
    TempStr destPath = path::JoinTemp(dir, fmt("%s_page_%d%s", noExt, pageNo, ext));
    // the editor takes ownership of the bitmap
    ShowImageEditWindowPixmap(win, m, destPath, px, selectPdf, origData);
    str::Free(origData);
}

// the commands below that act on the link, image or annotation under the menu
static bool ActsOnElementUnderMenu(int cmdId) {
    switch (cmdId) {
        case CmdSearchGoogleLensImage:
        case CmdReplaceImage:
        case CmdEditImageExternally:
        case CmdSaveImage:
        case CmdCropImage:
        case CmdResizeImage:
        case CmdConvertImageToPdf:
        case CmdCopyLinkTarget:
        case CmdShowAnnotationText:
        case CmdCopyComment:
        case CmdSaveAttachment:
        case CmdCopyImage:
            return true;
    }
    return false;
}

// orig's switch after TrackPopupMenu returned
void WindowContextMenuCommand(MainWindow* win, int cmdId) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    win->contextMenuHighlightPageNo = 0;
    DisplayModel* dm = win->AsFixed();
    // the popup isn't modal: move the point to where its page is now. If the
    // page scrolled out of view, nothing is under the point the menu was for.
    int pageNoUnderCursor = win->contextMenuPageNo;
    bool pointGone = false;
    if (dm && pageNoUnderCursor > 0 && dm->ValidPageNo(pageNoUnderCursor)) {
        win->contextMenuPt = dm->CvtToScreen(pageNoUnderCursor, win->contextMenuPagePt);
        pointGone = !dm->PageVisible(pageNoUnderCursor);
    } else {
        pageNoUnderCursor = 0;
    }
    if (pointGone) {
        win->contextMenuPtValid = false;
    }
    if (HandleReadAloudMenuCommand(win, cmdId)) {
        return;
    }
    if (!dm || !win->ctrl) {
        return;
    }
    logf("WindowContextMenuCommand: %d (%s)\n", cmdId, GetCommandName(cmdId));
    if (pointGone && (CommandUsesContextMenuPoint(cmdId) || ActsOnElementUnderMenu(cmdId))) {
        logf("WindowContextMenuCommand: page %d scrolled out of view\n", pageNoUnderCursor);
        return;
    }
    Point cursorPos = win->contextMenuPt;
    WindowTab* tab = win->CurrentTab();
    Str filePath = win->ctrl->GetFilePath();
    // ng: orig keeps the IPageElement it built the menu from; a frame has
    // passed here, so it is looked up again at the point the menu opened on
    int pageNoUnderEl = 0;
    IPageElement* pageEl = dm->GetElementAtPos(cursorPos, &pageNoUnderEl);
    Str value = pageEl ? pageEl->GetValue() : Str{};
    EngineBase* engine = dm->GetEngine();
    BuildMenuCtx* ctx = NewBuildMenuCtx(tab, cursorPos);
    AutoDelete<AppCommandCtx> delCtx(ctx);

    // handled by ExecuteAnnotCreateCmd & co, which need the canvas point
    if (CommandUsesContextMenuPoint(cmdId)) {
        ExecuteCmdAtPoint(win, cmdId, cursorPos);
        return;
    }

    switch (cmdId) {
        case CmdSearchGoogleLens:
            SearchGoogleLensSelection(tab);
            return;
        case CmdSearchGoogleLensPage:
            SearchGoogleLensPage(tab, pageNoUnderCursor);
            return;
        case CmdSearchGoogleLensImage:
            SearchGoogleLensImage(tab, pageEl);
            return;
        case CmdReplaceImage:
            ReplaceImageFromFile(win, pageEl);
            return;
        case CmdEditImageExternally:
            EditImageExternally(win, pageEl);
            return;

        case CmdSaveImage:
        case CmdCropImage:
        case CmdResizeImage:
        case CmdConvertImageToPdf:
            ContextMenuEditImage(win, pageEl, pageNoUnderCursor, cmdId);
            return;

        case CmdCopyLinkTarget: {
            if (len(value) > 0) {
                TempStr tmp = CleanupURLForClipboardCopyTemp(value);
                CopyTextToClipboard(win, tmp);
            }
            return;
        }
        case CmdShowAnnotationText: {
            Annotation* annot = ctx->annotationUnderCursor;
            if (annot) {
                ShowAnnotationTextPopup(win, annot);
            }
            return;
        }
        case CmdCopyComment: {
            Str comment = value;
            if (ctx->annotationUnderCursor) {
                // The page element's value is hover text. For FreeText that is
                // only the author because the contents are already on the page.
                comment = Contents(ctx->annotationUnderCursor);
            }
            if (!str::IsEmptyOrWhiteSpace(comment)) {
                CopyTextToClipboard(win, comment);
            }
            return;
        }
        case CmdSaveAttachment: {
            if (!pageEl || !pageEl->Is(kindPageElementDest)) {
                return;
            }
            IPageDestination* elDest = pageEl->AsLink();
            auto* pd = (PageDestination*)elDest;
            if (!pd) {
                return;
            }
            // attachments are arbitrary binary
            Str data = EngineMupdfLoadEmbedded(engine, pd);
            if (len(data) == 0) {
                return;
            }
            Str fileName = pd->GetValue();
            TempStr dir = path::GetDirTemp(filePath);
            fileName = path::GetBaseNameTemp(fileName);
            TempStr dstPath = path::JoinTemp(dir, fileName);
            SaveDataToFile(win, dstPath, data);
            str::Free(data);
            return;
        }
        case CmdCopyImage: {
            if (pageEl) {
#if OS_WIN
                RenderedBitmap* bmp = engine->GetImageForPageElement(pageEl);
                if (bmp) {
                    CopyRenderedBitmapToClipboard(win, bmp);
                }
#else
                // ng: off Windows the clipboard takes the image as a PNG
                Str png = engine->GetImagePngForPageElement(pageEl);
                CopyPngToClipboard(win, png);
                str::Free(png);
#endif
            }
            return;
        }
        case CmdFavoriteAdd: {
            if (pageNoUnderCursor > 0) {
                AddFavoriteForPage(win, pageNoUnderCursor);
            }
            return;
        }
        case CmdFavoriteDel: {
            DelFavorite(filePath, pageNoUnderCursor, win->ctrl);
            return;
        }
    }
    ExecuteCmd(win, cmdId);
}

// --- the home page's context menu (orig's OnAboutContextMenu) ---------------

// ng: the website's Upload to server / Delete from server. While linked,
// Delete File reads Delete local. A changed server copy can be replaced.
static void AppendServerItems(MenuModel* popup, Str filePath) {
    if (!ServerFilesListed()) {
        return;
    }
    MenuSetText(popup, CmdDeleteFile, Tr("Delete Local"));
    MenuAppendSeparator(popup);
    ServerFileState st = GetServerFileState(filePath, file::GetSize(filePath));
    if (st != ServerFileState::Same) {
        MenuAppendString(popup, Tr("Upload to Server"), CmdUploadToServer);
    }
    if (st != ServerFileState::None) {
        MenuAppendString(popup, Tr("Delete from Server"), CmdDeleteFromServer);
    }
}

static MenuModel* BuildHomeContextMenu(MainWindow* win, Str filePath) {
    if (!HasPermission(Perm::SavePreferences | Perm::DiskAccess) || !SettingsRememberOpenedFiles() ||
        !gSettings->showStartPage) {
        return nullptr;
    }
    if (len(filePath) == 0 || !path::IsAbsolute(filePath)) {
        return nullptr;
    }
    // ng: a server-only file: open (downloads it) or delete the server copy
    if (IsServerOnlyFile(filePath)) {
        auto* popup = new MenuModel();
        MenuAppendString(popup, Tr("&Open Document"), CmdOpenSelectedDocument);
        MenuAppendString(popup, Tr("Delete from Server"), CmdDeleteFromServer);
        return popup;
    }
    FileState* fs = FileHistoryFindByPath(filePath);
    if (!fs) {
        return nullptr;
    }
    AppCommandCtx ctx;
    ctx.win = win;
    ctx.isDocLoaded = true;
    ctx.filePath = filePath;
    MenuModel* popup = BuildMenuFromDef(menuDefContextStart, &ctx);
    MenuSetChecked(popup, CmdPinSelectedDocument, fs->isPinned);
    // Del is home-page-only (not a global accelerator), so the accelerator
    // table won't pick it up - show it next to Remove From History explicitly
    MenuItemModel* it = MenuFind(popup, CmdForgetSelectedDocument);
    if (it) {
        str::Free(it->accel);
        it->accel = str::Dup(StrL("Del"));
    }
    AppendServerItems(popup, filePath);
    // the file moved: point its history entry at where it is now
    if (!file::Exists(filePath)) {
        MenuAppendSeparator(popup);
        MenuAppendString(popup, Tr("Locate File..."), CmdLocateFile);
    }
    logf("BuildHomeContextMenu: '%s': %s\n", path::GetBaseNameTemp(filePath), MenuRowsTemp(popup));
    return popup;
}

MenuModel* BuildHomeContextMenu(MainWindow* win, const StrVec& paths) {
    if (paths.IsEmpty()) {
        return nullptr;
    }
    MenuModel* menu = BuildHomeContextMenu(win, paths[0]);
    if (!menu || len(paths) == 1) {
        return menu;
    }
    for (int i = 1; i < len(paths); i++) {
        MenuModel* other = BuildHomeContextMenu(win, paths[i]);
        for (int j = len(menu->items) - 1; j >= 0; j--) {
            const MenuItemModel& it = menu->items[j];
            if (it.separator || it.submenu) {
                continue;
            }
            const MenuItemModel* match = other ? MenuFind(other, it.cmdId) : nullptr;
            if (!match || match->disabled) {
                MenuRemove(menu, it.cmdId);
            }
        }
        DeleteMenuModel(other);
    }
    MenuSetChecked(menu, CmdPinSelectedDocument, HomeFilesAllPinned(paths));
    RemoveBadMenuSeparators(menu);
    return menu;
}

static void HomeContextMenuCommand(MainWindow* win, Str filePathIn, int cmdId) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    // own the path: deleting closes tabs and rewrites the history
    TempStr path = str::DupTemp(filePathIn);
    logf("HomeContextMenuCommand: %d (%s) on '%s'\n", cmdId, GetCommandName(cmdId), path);
    FileState* fs = FileHistoryFindByPath(path);
    switch (cmdId) {
        case CmdOpenSelectedDocument:
            if (IsServerOnlyFile(path)) {
                OpenServerFile(win, path);
                return;
            }
            StartLoadDocument(win, path);
            return;
        case CmdShowInFolder:
            ShowFileInFolder(win, path);
            return;
        case CmdPinSelectedDocument:
            if (fs) {
                fs->isPinned = !fs->isPinned;
                ScheduleSaveSettings();
                win->RedrawAll(true);
            }
            return;
        case CmdForgetSelectedDocument:
            ForgetFileFromFrequentlyRead(win, path);
            return;
        case CmdDeleteFile:
            DeleteFileFromHomePage(win, path);
            return;
        case CmdUploadToServer:
            UploadFileToServer(win, path);
            return;
        case CmdLocateFile:
            HomePageLocateFile(win, path);
            return;
        case CmdDeleteFromServer:
            DeleteFileFromServer(win, path);
            return;
    }
    ExecuteCmd(win, cmdId);
}

void HomeContextMenuCommand(MainWindow* win, const StrVec& paths, int cmdId) {
    if (!IsMainWindowValidAndNotClosing(win) || RunHomeFilesCommand(win, paths, cmdId)) {
        return;
    }
    if (cmdId == CmdDeleteFile && len(paths) > 1) {
        DeleteFilesFromHomePage(win, paths);
        return;
    }
    StrVec serverPaths;
    for (Str path : paths) {
        if (len(paths) > 1 && cmdId == CmdOpenSelectedDocument && IsServerOnlyFile(path)) {
            serverPaths.Append(path);
        } else {
            HomeContextMenuCommand(win, path, cmdId);
        }
    }
    if (!serverPaths.IsEmpty()) {
        RunServerFilesBatch(win, serverPaths, ServerFileAction::Open);
    }
}

// orig's ToggleMenuBar, without the win32 menu / rebar handling: in fullscreen
// F9 flips Fullscreen.ShowMenubar, otherwise ShowMenubar / ShowMenubarWithTabs,
// so the choice is remembered. ng: showTemporarily (Alt alone) has nothing to
// do here, the Alt access keys are handled by the shell.
void ToggleMenuBar(MainWindow* win, bool showTemporarily) {
    if (showTemporarily) {
        return;
    }
    if (win->IsFullScreen()) {
        gSettings->fullscreen.showMenubar = !gSettings->fullscreen.showMenubar;
        win->isMenuBarVisible = gSettings->fullscreen.showMenubar;
    } else {
        bool hideMenu = win->isMenuBarVisible;
        win->isMenuBarVisible = !hideMenu;
        gSettings->showMenubar = !hideMenu;
        gSettings->showMenubarWithTabs = !hideMenu;
    }
    win->RedrawAll();
}

// the ASCII letter that opens this menu with Alt, lower-cased; 0 if it has none
char MenuAccessKey(Str title) {
    MenuAccelText parsed = ParseMenuAccelTextTemp(title);
    if (parsed.underlineOff < 0 || parsed.underlineLen != 1) {
        return 0;
    }
    char c = parsed.display.s[parsed.underlineOff];
    if (c >= 'A' && c <= 'Z') {
        c = (char)(c - 'A' + 'a');
    }
    return (c >= 'a' && c <= 'z') ? c : 0;
}
