/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

void ShowAdvancedSettingsDialog(MainWindow* win);

TempStr AdvSettingsRowsResultTemp(Str action, int arg, int* exitCodeOut);
