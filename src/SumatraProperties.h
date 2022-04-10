/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct PropertiesLayout;

enum class PaperFormat { Other, A2, A3, A4, A5, A6, Letter, Legal, Tabloid, Statement };
PaperFormat GetPaperFormat(SizeF size);

void ShowPropertiesWindow(WindowInfo* win);
void DeletePropertiesWindow(HWND hwndParent);

PropertiesLayout* FindPropertyWindowByHwnd(HWND hwnd);
