/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraProperties_h
#define SumatraProperties_h

#include "Vec.h"

#define PROPERTIES_CLASS_NAME   _T("SUMATRA_PDF_PROPERTIES")

enum Magnitudes { KB = 1024, MB = 1024 * KB, GB = 1024 * MB };

class PropertyEl {
public:
    PropertyEl(const TCHAR *leftTxt, TCHAR *rightTxt)
        : leftTxt(leftTxt), rightTxt(rightTxt) { }
    ~PropertyEl() {
        // free the text on the right. Text on left is static, so doesn't need to be freed
        free(rightTxt);
    }

    /* A property is always in format:
    Name (left): Value (right) */
    const TCHAR *   leftTxt;
    TCHAR *         rightTxt;

    /* data calculated by the layout */
    RectI           leftPos;
    RectI           rightPos;
};

class PropertiesLayout : public Vec<PropertyEl *> {
public:
    ~PropertiesLayout() { DeleteVecMembers(*this); }
    void AddProperty(const TCHAR *key, TCHAR *value);
};

void OnMenuProperties(WindowInfo& win);
void CopyPropertiesToClipboard(HWND hwnd);
LRESULT CALLBACK WndProcProperties(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

#endif
