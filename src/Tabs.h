/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Tabs_h
#define Tabs_h

class WindowInfo;
class Controller;
struct WatchedFile;

// This is the data, for every opened document, which is preserved between
// tab selections. It's loaded back in the WindowInfo for the currently active document.
struct TabData
{
    Controller *ctrl;
    bool        showToc;
    Vec<int>    tocState;
    WCHAR *     title;
    WCHAR *     filePath;
    // canvas dimensions when the document was last visible
    RectI       canvasRc;
    WatchedFile*watcher;
    // if true, the document should be auto-reloaded when it's visible again
    bool        reloadOnFocus;

    TabData(): ctrl(NULL), showToc(false), title(NULL), filePath(NULL),
        watcher(NULL), reloadOnFocus(false) { }
};

int GetTabbarHeight(WindowInfo *win, float factor=1.f);

void SaveCurrentTabData(WindowInfo *win);
int TabsGetCount(WindowInfo *win);
TabData *GetTabData(WindowInfo *win, int tabIndex);
TabData *GetTabDataByCtrl(WindowInfo *win, Controller *ctrl);
void LoadModelIntoTab(WindowInfo *win, TabData *tdata);

void CreateTabbar(WindowInfo *win);
void TabsOnLoadedDoc(WindowInfo *win);
void TabsOnCloseDoc(WindowInfo *win);
void TabsOnCloseWindow(WindowInfo *win);
void TabsOnChangedDoc(WindowInfo *win);
LRESULT TabsOnNotify(WindowInfo *win, LPARAM lparam, int tab1=-1, int tab2=-1);
void TabsSelect(WindowInfo *win, int tabIndex);
void TabsOnCtrlTab(WindowInfo *win, bool reverse);
// also shows/hides the tabbar when necessary
void UpdateTabWidth(WindowInfo *win);
void SetTabsInTitlebar(WindowInfo *win, bool set);
void SetCurrentTabBgCol(WindowInfo *win, COLORREF bgCol);

COLORREF AdjustLightness2(COLORREF c, float units);

#endif
