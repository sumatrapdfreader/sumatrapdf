/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define PROPERTIES_CLASS_NAME L"SUMATRA_PDF_PROPERTIES"

enum class PaperFormat { Other, A2, A3, A4, A5, A6, Letter, Legal, Tabloid, Statement };
PaperFormat GetPaperFormat(SizeD size);

void OnMenuProperties(WindowInfo* win);
void DeletePropertiesWindow(HWND hwndParent);
LRESULT CALLBACK WndProcProperties(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
