/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Native frame of a gpui window on macOS and Linux. Coordinates are top-left
// origin, in the units gpui sizes windows with (points on macOS, pixels on X11).

namespace gpui {
struct Window;
}

// outer frame; empty when the window has no native frame yet
Rect ToolWinNativeFrame(gpui::Window* gw);
// the client area, in the same coordinates
Rect ToolWinNativeContentRect(gpui::Window* gw);
// work area and full bounds of the monitor the window is on
Rect ToolWinNativeWorkArea(gpui::Window* gw);
Rect ToolWinNativeMonitor(gpui::Window* gw);
// extra size around a client area for this kind of frame. dx is left+right,
// dy is top+bottom
Size ToolWinNativeChrome(bool titled, bool resizable, bool utility);
void ToolWinNativeSetFrame(gpui::Window* gw, Rect outer, bool titled);
void ToolWinNativeApplyStyle(gpui::Window* gw, bool titled, bool resizable, bool utility, bool borderless,
                             bool wantsKey);
void ToolWinNativeSetOwner(gpui::Window* gw, gpui::Window* owner, bool owned);
void ToolWinNativeShow(gpui::Window* gw, bool visible, bool activate);
// macOS: deliver one key through AppKit. Elsewhere a no-op. keyCode is a virtual key.
void ToolWinNativeInjectKey(gpui::Window* gw, int keyCode);
void ToolWinNativeActivate(gpui::Window* gw);
bool ToolWinNativeIsActive(gpui::Window* gw);
void ToolWinNativeSetMinClient(gpui::Window* gw, int dx, int dy);
// the owner takes no clicks or keys while `on`
void ToolWinNativeSetModal(gpui::Window* gw, gpui::Window* owner, bool on);
bool ToolWinNativeMouseDown();
