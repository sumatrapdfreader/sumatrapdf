/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct AppCommandCtx;
using BuildMenuCtx = AppCommandCtx;
struct MainWindow;

struct MenuDef {
    Str title;
    // ng: UINT_PTR -> uintptr_t (same type, portable spelling)
    uintptr_t idOrSubmenu = 0;
};

#define kMenuSeparator "-----"

// ng: orig builds an HMENU. We build this tree instead and the gpui shell
// turns it into AppMenuBar / PopupMenu elements. Same labels, same order,
// same separators, same accelerator hints.
struct MenuModel;

struct MenuItemModel {
    // translated label, still with the '&' access-key marker; freed by
    // DeleteMenuModel when titleOwned (a label built at menu-build time)
    Str title;
    // the accelerator as the menu shows it ("Ctrl + O"), empty if none
    Str accel;
    int cmdId = 0;
    bool separator = false;
    bool disabled = false;
    bool checked = false;
    bool titleOwned = false;
    MenuModel* submenu = nullptr;
};

struct MenuModel {
    Vec<MenuItemModel> items;
};

void DeleteMenuModel(MenuModel*);
MenuModel* BuildMenuFromDef(MenuDef* menuDefs, BuildMenuCtx* ctx);
// the menu bar: one top-level entry per menu, each with a submenu
MenuModel* BuildMenu(MainWindow* win);
TempStr MainMenuResultTemp(MainWindow* win);
TempStr FavoritesMenuIdsTemp(MainWindow* win);
TempStr FileHistoryMenuIdsTemp(MainWindow* win);
TempStr ContextMenuAtPointResultTemp(MainWindow* win, int x, int y);
void RemoveBadMenuSeparators(MenuModel* menu);

// orig edits an HMENU with these; here they edit the model
void MenuRemove(MenuModel*, int cmdId);
void MenuSetText(MenuModel*, int cmdId, Str s);
void MenuSetEnabled(MenuModel*, int cmdId, bool enabled);
void MenuSetChecked(MenuModel*, int cmdId, bool checked);
void MenuAppendString(MenuModel*, Str title, int cmdId);
void MenuAppendSeparator(MenuModel*);
void MenuEmpty(MenuModel*);
MenuModel* MenuAppendSubmenu(MenuModel*, Str title);

// orig's Win32 '&' access-key markup, parsed out of a menu label
struct MenuAccelText {
    Str display;
    int underlineOff = -1;
    int underlineLen = 0;
};
MenuAccelText ParseMenuAccelTextTemp(Str s);
char MenuAccessKey(Str title);

// ng: orig's OnWindowContextMenu builds the popup, tracks it and runs what
// came back in one call. gpui builds the popup from a MenuModel in the frame
// after the right button went down and reports the pick as an action, so it is
// two calls: the first remembers the canvas point on the window.
MenuModel* BuildWindowContextMenu(MainWindow* win, Point cursorPos);
void WindowContextMenuCommand(MainWindow* win, int cmdId);
// commands whose handler needs the point the menu was opened on
bool CommandUsesContextMenuPoint(int cmdId);

// orig's OnAboutContextMenu: the home page's per-file menu
MenuModel* BuildHomeContextMenu(MainWindow* win, Str filePath);
void HomeContextMenuCommand(MainWindow* win, Str filePath, int cmdId);

int CmdIdFromVirtualZoom(float virtualZoom);
float ZoomMenuItemToZoom(int menuItemId);
void ToggleMenuBar(MainWindow* win, bool showTemporarily);
