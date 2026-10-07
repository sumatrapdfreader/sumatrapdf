/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct MainWindow;

void RunCommandPalette(MainWindow*, Str prefix, int smartTabAdvance);
HWND CommandPaletteHwndForAccelerator(HWND hwnd);
TempStr CommandPaletteStateTemp(int* exitCodeOut);
void CommandPaletteOnAnnotationsChanged();
void CommandPaletteUpdateTheme();
