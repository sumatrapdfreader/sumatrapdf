/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct PropertiesLayout;

void ShowPropertiesWindow(MainWindow* win);
void DeletePropertiesWindow(HWND hwndParent);

PropertiesLayout* FindPropertyWindowByHwnd(HWND hwnd);
