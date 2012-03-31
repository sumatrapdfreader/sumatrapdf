#ifndef SumatraWindow_h
#define SumatraWindow_h

class MobiWindow;
class WindowInfo;

// This is a wrapper that allows functions to work with both window types.
// TODO: not sure if that's how the things should work ultimately but
// for now it'll do
struct SumatraWindow {

    WindowInfo *AsWindowInfo() const { return (Info == type) ? winInfo : NULL; }
    MobiWindow *AsMobiWindow() const { return (Mobi == type) ? winMobi : NULL; }

    static SumatraWindow Make(WindowInfo *win) {
        SumatraWindow w; w.type = Info; w.winInfo = win;
        return w;
    }

    static SumatraWindow Make(MobiWindow *win) {
        SumatraWindow w; w.type = Mobi; w.winMobi = win;
        return w;
    }

#if 0
    HWND HwndFrame() const {
        if (Info == type)
            return winInfo->hwndFrame;
        if (Mobi == type)
            return winMobi->hwndFrame;
        return NULL;
    }
#endif

private:
    enum Type { Info, Mobi };
    Type type;
    union {
        WindowInfo *winInfo;
        MobiWindow *winMobi;
    };
};

#endif
