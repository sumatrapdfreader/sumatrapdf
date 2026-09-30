/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

void ShowMergePdfDialog(MainWindow* win);
TempStr MergePdfResultTemp(Str action, Str arg, int n, int* exitCodeOut = nullptr);
