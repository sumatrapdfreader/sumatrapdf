/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct PropertiesLayout;

void ShowProperties(HWND parent, DocController* ctrl, bool extended);
void DeletePropertiesWindow(HWND hwndParent);
PropertiesLayout* FindPropertyWindowByHwnd(HWND hwnd);
