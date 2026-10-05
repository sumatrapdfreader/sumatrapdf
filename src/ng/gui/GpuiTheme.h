/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: what orig's UpdateGuiColorsFromTheme() does for gui/GuiColors, done for
// gpui: the current theme's colors become the gpui Theme every component reads
// through ThemeNow().

void ThemeInstallInGpui();
// the canvas background around the pages (orig's ThemeDocumentColors bg)
gp::Rgba ThemeGpuiCanvasBg();
