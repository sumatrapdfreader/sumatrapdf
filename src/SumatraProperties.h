/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraProperties_h
#define SumatraProperties_h

#include "SumatraWindow.h"

#define PROPERTIES_CLASS_NAME   L"SUMATRA_PDF_PROPERTIES"

enum Magnitudes { KB = 1024, MB = 1024 * KB, GB = 1024 * MB };

class PropertyEl {
public:
    PropertyEl(const WCHAR *leftTxt, WCHAR *rightTxt)
        : leftTxt(leftTxt), rightTxt(rightTxt) { }
    ~PropertyEl() {
        // free the text on the right. Text on left is static, so doesn't need to be freed
        free(rightTxt);
    }

    /* A property is always in format:
    Name (left): Value (right) */
    const WCHAR *   leftTxt;
    WCHAR *         rightTxt;

    /* data calculated by the layout */
    RectI           leftPos;
    RectI           rightPos;
};

class PropertiesLayout : public Vec<PropertyEl *> {
public:
    PropertiesLayout() : hwnd(NULL), hwndParent(NULL) { }
    ~PropertiesLayout() { DeleteVecMembers(*this); }
    void AddProperty(const WCHAR *key, WCHAR *value);
    bool HasProperty(const WCHAR *key);

    HWND    hwnd;
    HWND    hwndParent;
};

void OnMenuProperties(const SumatraWindow& win);
void DeletePropertiesWindow(HWND hwndParent);
LRESULT CALLBACK WndProcProperties(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

#endif
