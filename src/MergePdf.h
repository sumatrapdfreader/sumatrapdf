/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

void ShowMergePdfDialog(MainWindow* win);
bool IsHwndInMergePdfWindow(HWND hwnd);
TempStr MergePdfResultTemp(Str action, Str arg, int n, int* exitCodeOut = nullptr);
