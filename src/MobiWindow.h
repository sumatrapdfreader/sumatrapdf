#ifndef MobiWindow_h
#define MobiWindow_h

#include "BaseUtil.h"

struct EbookControls;
class HwndWrapper;
class EbookController;

class MobiWindow {
public:
    const TCHAR *       loadedFilePath;
    HWND                hwndFrame;
    EbookControls *     ebookControls;
    HwndWrapper *       mainWnd; // TODO: rename to hwndWrapper
    EbookController *   ebookController;
};


bool RegisterMobikWinClass(HINSTANCE hinst);

#endif
