/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct MainWindow;

constexpr const char* kPalettePrefixCommands = ">";
constexpr const char* kPalettePrefixFileHistory = "#";
constexpr const char* kPalettePrefixTabs = "@";
constexpr const char* kPalettePrefixEverything = ":";
constexpr const char* kPalettePrefixTOC = "%";
constexpr const char* kPalettePrefixFavorites = "$";
constexpr const char* kPalettePrefixAnnotations = "*";
constexpr const char* kPalettePrefixBoolSettings = "=";
constexpr const char* kPalettePrefixThumbnails = "&";

void RunCommandPalette(MainWindow*, Str prefix, int smartTabAdvance);
HWND CommandPaletteHwndForAccelerator(HWND hwnd);
TempStr CommandPaletteStateTemp(int* exitCodeOut);
void CommandPaletteOnAnnotationsChanged();
