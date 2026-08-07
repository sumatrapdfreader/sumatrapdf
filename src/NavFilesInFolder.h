/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

// Opens the navigate-files-in-folder picker. When selectPath is set (absolute
// path to a file), browses that file's directory and selects it; otherwise uses
// the current document (or the newest history entry on the home page).
void ShowNavFilesInFolder(MainWindow* win, Str selectPath = {});
