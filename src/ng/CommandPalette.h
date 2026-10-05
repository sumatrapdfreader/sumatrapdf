/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;
struct WindowTab;

constexpr const char* kPalettePrefixCommands = ">";
constexpr const char* kPalettePrefixFileHistory = "#";
constexpr const char* kPalettePrefixTabs = "@";
constexpr const char* kPalettePrefixEverything = ":";
constexpr const char* kPalettePrefixTOC = "%";
constexpr const char* kPalettePrefixFavorites = "$";
constexpr const char* kPalettePrefixAnnotations = "*";
constexpr const char* kPalettePrefixBoolSettings = "=";
constexpr const char* kPalettePrefixThumbnails = "&";

// which of the seven lists a query selects; the prefix is what picks it
enum class PaletteMode {
    Commands,
    Tabs,
    FileHistory,
    Everything,
    Toc,
    Favorites,
    Annotations,
    Settings,
    Thumbnails,
};

// ng: orig decides this inline in FilterStringsForQuery(); it is a function of
// its own here so `test_util` can pin the prefixes without a window
PaletteMode PaletteModeFromQuery(Str query, Str* restOut = nullptr);

// smartTabAdvance != 0 opens the tab list in orig's "smart tab" mode: the
// selection starts that many steps away from the current tab and releasing
// Ctrl commits it
void RunCommandPalette(MainWindow* win, Str prefix, int smartTabAdvance);
void CloseCommandPalette();
bool IsCommandPaletteVisible();
// the window the open palette belongs to, or null
MainWindow* CommandPaletteWindow();
gpui::El* CommandPaletteBuild(MainWindow* win, gpui::Ctx* cx);
// ng: the palette's own window where the platform can have one (orig's owned
// popup), for -dbg-control: TestInput and TestUiState follow the input to it
namespace gpui {
struct Window;
}
gpui::Window* CommandPaletteInputWindow(MainWindow* win);

// ng: the palette is not a window of its own here, so the shell's key handler
// routes the keys it owns. True when the palette consumed the key
bool CommandPaletteOnKeyDown(MainWindow* win, int vkey, bool ctrl, bool shift);
bool CommandPaletteOnKeyUp(MainWindow* win, int vkey);
bool CommandPaletteOnMouseDown(MainWindow* win, float x, float y);
// smart-tab mode: the tab the list points at, so the strip can preview it
WindowTab* CommandPaletteHighlightedTab(MainWindow* win);

// the tab rows the palette lists, in orig's order; the Ctrl+Tab switcher
// (gui/TabSwitcher.cpp) is this same list
void PaletteCollectTabs(MainWindow* win, bool mru, Vec<WindowTab*>& out, int& currTabIdx);

// the document's annotations changed: the `*` list is stale
void CommandPaletteOnAnnotationsChanged();
void CommandPaletteOnSettingsReloaded();
