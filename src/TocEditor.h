/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TocEditorArgs {
    // file path for either .pdf or .vbkm file
    AutoFreeWstr filePath;
    Vec<Bookmarks*> bookmarks;

    ~TocEditorArgs();
};

void StartTocEditor(TocEditorArgs*);
