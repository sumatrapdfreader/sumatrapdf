#pragma once

namespace prettysumatra {
namespace bridge {

enum class DispatchResult {
    Disabled,
    InvalidMessage,
    UnknownCommand,
    Accepted,
};

bool UseHybridShell();
bool UseHybridToolbar();
bool UseHybridSidebar();
bool LogBridgeMessages();
bool HybridThemeFollowsWindows();
bool HasHybridToolbar(HWND hwndFrame);
void InitHybridToolbarTheme(HWND hwndFrame);
void SyncHybridToolbarTheme(HWND hwndFrame);
void SyncHomePageTheme(HWND hwndFrame);
void InitHybridToolbarText(HWND hwndFrame);
void SyncHybridToolbarText(HWND hwndFrame);
void SyncHybridToolbarSearchText(HWND hwndFrame, const char* text);
void FocusHybridToolbarSearch(HWND hwndFrame);
void SyncHybridToolbarPageState(HWND hwndFrame, int currentPage, int totalPages);
void SyncHybridToolbarZoomState(HWND hwndFrame, float zoomPercent);

DispatchResult DispatchShellMessage(const char* msg);

} // namespace bridge
} // namespace prettysumatra
