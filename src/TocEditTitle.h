/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TocEditArgs {
    // provided by the user
    AutoFreeStr title;
    bool bold = false;
    bool italic = false;
    COLORREF color = ColorUnset;
};

// if EditTitleArgs is null, editing was cancelled
typedef std::function<void(TocEditArgs*)> TocEditFinishedHandler;

// <args>> should live untill <onFinished> is called
bool StartTocEditTitle(HWND, TocEditArgs* args, const TocEditFinishedHandler& onFinished);
