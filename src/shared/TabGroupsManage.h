/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;

void ShowSaveTabGroupDialog(MainWindow* win);
void ShowOpenTabGroupDialog(MainWindow* win);
bool IsTabGroupsDialogVisible();
void CloseTabGroupsDialog();
gpui::El* TabGroupsDialogBuild(MainWindow*, gpui::Ctx*);
