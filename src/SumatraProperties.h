/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define PROPERTIES_CLASS_NAME   L"SUMATRA_PDF_PROPERTIES"

enum PaperFormat { Paper_Other, Paper_A4, Paper_A3, Paper_A5, Paper_Letter, Paper_Legal, Paper_Tabloid };
PaperFormat GetPaperFormat(SizeD size);

void OnMenuProperties(WindowInfo *win);
void DeletePropertiesWindow(HWND hwndParent);
LRESULT CALLBACK WndProcProperties(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
