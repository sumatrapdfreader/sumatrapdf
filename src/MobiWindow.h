#ifndef MobiWindow_h
#define MobiWindow_h

#include "BaseUtil.h"
#include "Mui.h"
#include "ThreadUtil.h"

struct EbookControls;
class  EbookController;

class ThreadLoadMobi : public ThreadBase {
    TCHAR *             fileName; // we own this memory
    EbookController *   controller;
public:

    ThreadLoadMobi(const TCHAR *fileName, EbookController *controller);
    virtual ~ThreadLoadMobi() { free(fileName); }

    // ThreadBase
    virtual void Run();
};

class MobiWindow {
public:
    const TCHAR *       loadedFilePath;
    HWND                hwndFrame;
    EbookControls *     ebookControls;
    mui::HwndWrapper *  hwndWrapper;
    EbookController *   ebookController;
};

void LoadMobi(const TCHAR *fileName);

bool RegisterMobikWinClass(HINSTANCE hinst);

#endif
