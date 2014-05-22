/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraWindow_h
#define SumatraWindow_h

class EbookWindow;
class WindowInfo;

// This is a wrapper that allows functions to work with both window types.
// TODO: not sure if that's how the things should work ultimately but
// for now it'll do
struct SumatraWindow {

    WindowInfo *AsWindowInfo() const { return (Info == type) ? winInfo : NULL; }
    EbookWindow *AsEbookWindow() const { return (Ebook == type) ? winEbook : NULL; }

    static SumatraWindow Make(WindowInfo *win) {
        SumatraWindow w; w.type = Info; w.winInfo = win;
        return w;
    }

    static SumatraWindow Make(EbookWindow *win) {
        SumatraWindow w; w.type = Ebook; w.winEbook = win;
        return w;
    }

    HWND HwndFrame() const { return NULL; }

private:
    enum Type { Info, Ebook };
    Type type;
    union {
        WindowInfo *winInfo;
        EbookWindow *winEbook;
    };
};

#endif
