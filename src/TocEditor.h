/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TocEditorArgs {
    // file path for either .pdf or .vbkm file
    AutoFreeWstr filePath;
    Vec<VbkmForFile*> bookmarks;
    HWND hwndRelatedTo = nullptr;

    ~TocEditorArgs();
};

void StartTocEditor(TocEditorArgs*);
