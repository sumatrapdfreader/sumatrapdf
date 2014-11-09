#include "BaseUtil.h"
#include "Dpi.h"

/* Info from https://code.msdn.microsoft.com/DPI-Tutorial-sample-64134744

DPI Unaware: virtualized to 96 DPI and scaled by the system for the DPI of the monitor where shown

System DPI Aware:
 These apps render themselves according to the DPI of the display where they
 are launched, and they expect that scaling to remain constant for all displays on the system. 
 These apps are scaled up or down when moved to a display with a different DPI from the system DPI. 

Per-Monitor DPI Aware:
 These apps render themselves for any DPI, and re-render when the DPI changes
 (as indicated by the WM_DPICHANGED window message). 
*/

/*
If you have HWND, call DpiScaleX(HWND, x) or DpiScaleY(HWND, y).
If we don't have dpi information for this HWND, we'll create it.

On WM_DPICHANGED call DpiUpdate(HWND) so that we can update
dpi information for that window.

On WM_DESTROY call DpiRemove(HWND) so that we remove it.

For even faster access you can cache struct Dpi somewhere.

Note: maybe I don't need keep a per-HWND cache and instead
always call GetDpiXY() ?
*/

struct DpiNode {
    DpiNode *next;
    Dpi dpi;
};

static DpiNode *g_dpis = NULL;

static void GetDpiXY(HWND hwnd, int& scaleX, int& scaleY) {
#if 0
    // TODO: only available in 8.1
    UINT dpiX = 96, dpiY = 96;
    HMONITOR h = MonitorFromWindow(hwnd, 0);
    if (h != NULL ) {
        HRESULT hr = GetDpiForMonitor(h, 0 /* MDT_Effective_DPI */, &dpiX, &dpiY);
        if (hr == S_OK) {
            scaleX = (int)dpiX;
            scaleY = (int)dpiY;
            return;
        }
#endif
    HDC dc = GetDC(hwnd);
    scaleX = (UINT)GetDeviceCaps(dc, LOGPIXELSX);
    scaleY = (UINT)GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(hwnd, dc);
}

// return the top-level parent of hwnd or NULL if hwnd is top-level
static HWND GetTopLevelParent(HWND hwnd) {
    HWND topLevel = hwnd;
    while (GetParent(topLevel) != NULL) {
        topLevel = GetParent(topLevel);
    }
    if (topLevel == hwnd) {
        return NULL;
    }
    return topLevel;
}

static DpiNode *DpiNodeFindByHwnd(HWND hwnd) {
    DpiNode *n = g_dpis;
    while (n != NULL) {
        if (n->dpi.hwnd == hwnd) {
            return n;
        }
        n = n->next;
    }
    return NULL;
}

static Dpi *DpiFindByHwnd(HWND hwnd) {
    DpiNode *n = DpiNodeFindByHwnd(hwnd);
    if (n == NULL ) {
        return NULL;
    }
    return &n->dpi;
}

void DpiUpdate(Dpi *dpi) {
    int dpiX = 96;
    int dpiY = 96;
    GetDpiXY(dpi->hwnd, dpiX, dpiY);
    dpi->scaleX = MulDiv(dpiX, 100, 96);
    dpi->scaleY = MulDiv(dpiY, 100, 96);
}

Dpi *DpiGet(HWND hwnd) {
    Dpi *dpi = DpiFindByHwnd(hwnd);
    if (NULL != dpi) {
        return dpi;
    }
    // try the parent
    HWND topLevel = GetTopLevelParent(hwnd);
    if (topLevel != NULL) {
        hwnd = topLevel;
        dpi = DpiFindByHwnd(topLevel);
        if (NULL != dpi) {
            return dpi;
        }
    }
    // create if doesn't exist
    DpiNode *n = AllocStruct<DpiNode>();
    n->dpi.hwnd = hwnd;
    DpiUpdate(&n->dpi);
    n->next = g_dpis;
    g_dpis = n;
    return &n->dpi;
}

void DpiRemove(HWND hwnd) {
    DpiNode *n = DpiNodeFindByHwnd(hwnd);
    CrashIf(NULL == n);
    ListRemove(&g_dpis, n);
}

