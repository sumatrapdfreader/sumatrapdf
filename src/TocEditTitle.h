/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TocEditArgs {
    AutoFreeStr title;
    bool bold = false;
    bool italic = false;
    COLORREF color = ColorUnset;
    int nPages = 0; // max pages in the doc
    int page = 0;   // valid: 1-nPages, 0 means not set
};

// if EditTitleArgs is null, editing was cancelled
typedef std::function<void(TocEditArgs*)> TocEditFinishedHandler;

// <args>> should live untill <onFinished> is called
bool StartTocEditTitle(HWND, TocEditArgs* args, const TocEditFinishedHandler& onFinished);
