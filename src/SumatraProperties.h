/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraProperties_h
#define SumatraProperties_h

#define PROPERTIES_CLASS_NAME   _T("SUMATRA_PDF_PROPERTIES")

enum Magnitudes { KB = 1024, MB = 1024 * KB, GB = 1024 * MB };

typedef struct PdfPropertyEl PdfPropertyEl;
struct PdfPropertyEl {
    /* A property is always in format:
    Name (left): Value (right) */
    const TCHAR *   leftTxt;
    const TCHAR *   rightTxt;

    /* data calculated by the layout */
    RectI           leftPos;
    RectI           rightPos;

    /* next element in linked list */
    PdfPropertyEl * next;
};

typedef struct PdfPropertiesLayout {
    PdfPropertyEl * first;
    PdfPropertyEl * last;
} PdfPropertiesLayout;

void OnMenuProperties(WindowInfo *win);
void CopyPropertiesToClipboard(HWND hwnd);
LRESULT CALLBACK WndProcProperties(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

#endif
