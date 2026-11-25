/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by MenuCommon.cpp and each app's Menu.cpp ---

struct MenuDef;
struct AppCommandCtx;

bool HideContextMenuEntry(const MenuDef* menu, const MenuDef& item, const AppCommandCtx& ctx);
bool ShowDebugMenu();
struct FileHistoryEntry {
    Str path;
    int cmdId;
};
void SetFileHistoryCmdIds(Vec<FileHistoryEntry>& files);
TempStr CleanupURLForClipboardCopyTemp(Str s);

// the fixed entries of the zoom menu
struct ZoomMenuId {
    int cmdId;
    float zoom;
};
extern const ZoomMenuId gZoomMenuIds[];
extern const int gZoomMenuIdsCount;
int CmdIdFromVirtualZoom(float virtualZoom);
float ZoomMenuItemToZoom(int menuItemId);

bool CmdIdInList(uintptr_t cmdId, uintptr_t* idsList, int n);
