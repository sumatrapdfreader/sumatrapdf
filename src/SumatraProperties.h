/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define PROPERTIES_CLASS_NAME   L"SUMATRA_PDF_PROPERTIES"

enum Magnitudes { KB = 1024, MB = 1024 * KB, GB = 1024 * MB };

class PropertyEl {
public:
    PropertyEl(const WCHAR *leftTxt, WCHAR *rightTxt, bool isPath=false)
        : leftTxt(leftTxt), rightTxt(rightTxt), isPath(isPath) { }

    // A property is always in format: Name (left): Value (right)
    // (leftTxt is static, rightTxt will be freed)
    const WCHAR *   leftTxt;
    ScopedMem<WCHAR>rightTxt;

    // data calculated by the layout
    RectI           leftPos;
    RectI           rightPos;

    // overlong paths get the ellipsis in the middle instead of at the end
    bool            isPath;
};

class PropertiesLayout : public Vec<PropertyEl *> {
public:
    PropertiesLayout() : hwnd(NULL), hwndParent(NULL) { }
    ~PropertiesLayout() { DeleteVecMembers(*this); }
    void AddProperty(const WCHAR *key, WCHAR *value, bool isPath=false);
    bool HasProperty(const WCHAR *key);

    HWND    hwnd;
    HWND    hwndParent;
};

void OnMenuProperties(WindowInfo *win);
void DeletePropertiesWindow(HWND hwndParent);
LRESULT CALLBACK WndProcProperties(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
