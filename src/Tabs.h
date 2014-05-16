/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Tabs_h
#define Tabs_h

#define TABBAR_HEIGHT    24
#define TAB_WIDTH        200
#define TAB_HEIGHT       TABBAR_HEIGHT - 3

// Comment this for default drawing.
#define OWN_TAB_DRAWING

#ifdef OWN_TAB_DRAWING
#define TAB_COLOR_BG     COLOR_BTNFACE
#define TAB_COLOR_TEXT   COLOR_BTNTEXT

#define T_CLOSING   (TCN_LAST + 1)
#define T_CLOSE     (TCN_LAST + 2)
#define T_DRAG      (TCN_LAST + 3)
#endif //OWN_TAB_DRAWING

class DisplayModel;
struct PageAnnotation;
class WindowInfo;

// This is the data, for every opened document, which is preserved between
// tab selections. It's loaded back in the WindowInfo for the currently active document.
struct TabData
{
    DisplayModel *        dm;
    bool                  showToc;
    Vec<int>              tocState;
    WCHAR *               title;
    Vec<PageAnnotation> * userAnnots;
    bool                  userAnnotsModified;

    TabData(): dm(NULL), showToc(false), tocState(0,NULL), title(NULL), userAnnots(NULL), userAnnotsModified(false) {}
};

void SaveTabData(WindowInfo *win, TabData **tdata);
void SaveCurrentTabData(WindowInfo *win);
TabData *GetTabData(HWND tabbarHwnd, int tabIndex);
void DeleteTabData(TabData *tdata, bool deleteModel);
void LoadModelIntoTab(WindowInfo *win, TabData *tdata);

void CreateTabbar(WindowInfo *win);
void TabsOnLoadedDoc(WindowInfo *win);
void TabsOnCloseWindow(WindowInfo *win, bool cleanUp);
LRESULT TabsOnNotify(WindowInfo *win, LPARAM lparam);
void TabsOnCtrlTab(WindowInfo *win);

void ShowOrHideTabbar(WindowInfo *win, int command);
void UpdateTabWidth(WindowInfo *win);
void ManageFullScreen(WindowInfo *win, bool exitFullScreen);
void SwapTabs(WindowInfo *win, int tab1, int tab2);

#endif
