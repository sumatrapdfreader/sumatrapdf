/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct PropertiesWnd;

void ShowProperties(HWND parent, DocController* ctrl);
void DeletePropertiesWindow(HWND hwndParent);
PropertiesWnd* FindPropertyWindowByHwnd(HWND hwnd);