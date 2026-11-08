/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

void ViewTabAsHex(WindowTab* tab);
gpui::El* HexViewBuild(MainWindow* win, gpui::Ctx* cx);
bool HexViewOnKeyDown(MainWindow* win, int vk);
