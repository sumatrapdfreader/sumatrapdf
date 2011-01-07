class WindowInfo;

// in plugin mode, the window's frame isn't drawn and closing and
// fullscreen are disabled, so that SumatraPDF can be displayed
// embedded (e.g. in a web browser)
extern bool gPluginMode;

void MakePluginWindow(WindowInfo *win, HWND hwndParent);
void UpdateMMapForIndexing(TCHAR *IFilterMMap);
