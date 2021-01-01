/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TocEditorArgs {
    // file path for either .pdf or .vbkm file
    AutoFreeWstr filePath;
    VbkmFile* bookmarks = nullptr;
    HWND hwndRelatedTo = nullptr;

    ~TocEditorArgs();
};

void StartTocEditor(TocEditorArgs*);
void StartTocEditorForWindowInfo(WindowInfo*);
bool IsTocEditorEnabledForWindowInfo(TabInfo*);
