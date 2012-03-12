#ifndef SumatraWindow_h
#define SumatraWindow_h

class MobiWindow;
class WindowInfo;

// This is a wrapper that allows functions to work with both window types.
// TODO: not sure if that's how the things should work ultimately but
// for now it'll do
struct SumatraWindow {
    enum Type { Info, Mobi };
    Type type;
    union {
        WindowInfo *winInfo;
        MobiWindow *winMobi;
    };
    WindowInfo *AsWindowInfo() const { return (Info == type) ? winInfo : NULL; }
    MobiWindow *AsMobiWindow() const { return (Mobi == type) ? winMobi : NULL; }
};

// helpers for making sure type agrees with data
SumatraWindow MakeSumatraWindow(WindowInfo *winInfo);
SumatraWindow MakeSumatraWindow(MobiWindow *winMobi);

#endif
