/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern bool gAddCrashMeMenu;

constexpr int kPermFlagOffset = 9;
enum MenuToolbarFlags {
    MF_NO_TRANSLATE = 1 << 0,
    MF_NOT_FOR_CHM = 1 << 1,
    MF_NOT_FOR_EBOOK_UI = 1 << 2,
    MF_CBX_ONLY = 1 << 3,
    MF_NEEDS_CURSOR_ON_PAGE = 1 << 4, // cursor must be withing page boundaries
    MF_NEEDS_SELECTION = 1 << 5,      // user must have text selection active
    MF_NEEDS_ANNOTS = 1 << 6,         // engine needs to support annotations
    MF_NEEDS_ANNOT_UNDER_CURSOR = 1 << 7,
    MF_REQ_INET_ACCESS = Perm::InternetAccess << kPermFlagOffset,
    MF_REQ_DISK_ACCESS = Perm::DiskAccess << kPermFlagOffset,
    MF_REQ_PREF_ACCESS = Perm::SavePreferences << kPermFlagOffset,
    MF_REQ_PRINTER_ACCESS = Perm::PrinterAccess << kPermFlagOffset,
    MF_REQ_ALLOW_COPY = Perm::CopySelection << kPermFlagOffset,
    MF_REQ_FULLSCREEN = Perm::FullscreenAccess << kPermFlagOffset,
};

struct BuildMenuCtx;
struct MenuOwnerDrawInfo;
struct MenuDef;

extern MenuDef menuDefContextToc[];
extern MenuDef menuDefContextFav[];

void FreeAllMenuDrawInfos();
void FreeMenuOwnerDrawInfo(MenuOwnerDrawInfo*);
void MarkMenuOwnerDraw(HMENU);
void FreeMenuOwnerDrawInfoData(HMENU);
void MenuOwnerDrawnMesureItem(HWND, MEASUREITEMSTRUCT*);
void MenuOwnerDrawnDrawItem(HWND, DRAWITEMSTRUCT*);
HFONT GetMenuFont();

HMENU BuildMenuFromMenuDef(MenuDef* menuDefs, HMENU menu, BuildMenuCtx* ctx);
HMENU BuildMenu(WindowInfo* win);
void OnWindowContextMenu(WindowInfo* win, int x, int y);
void OnAboutContextMenu(WindowInfo* win, int x, int y);
void OnMenuZoom(WindowInfo* win, int menuId);
void OnMenuCustomZoom(WindowInfo* win);
int MenuIdFromVirtualZoom(float virtualZoom);
void UpdateAppMenu(WindowInfo* win, HMENU m);
void ShowHideMenuBar(WindowInfo* win, bool showTemporarily = false);
