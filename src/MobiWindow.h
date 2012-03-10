#ifndef MobiWindow_h
#define MobiWindow_h

#include "BaseUtil.h"
#include "Mui.h"
#include "SumatraWindow.h"
#include "ThreadUtil.h"
#include "WindowInfo.h"

struct EbookControls;
class  EbookController;
class  MobiDoc;

class ThreadLoadMobi : public ThreadBase {
    TCHAR *             fileName; // we own this memory
    EbookController *   controller;
public:
    SumatraWindow       win;

    ThreadLoadMobi(const TCHAR *fileName, EbookController *controller, const SumatraWindow& sumWin);
    virtual ~ThreadLoadMobi() { free(fileName); }

    // ThreadBase
    virtual void Run();
};

class MobiWindow {
public:
    MobiWindow() : 
        hwndFrame(NULL), ebookControls(NULL), 
        hwndWrapper(NULL), ebookController(NULL) 
    { touchState.panStarted = false; }
    ~MobiWindow() {}
    HWND                hwndFrame;
    EbookControls *     ebookControls;
    mui::HwndWrapper *  hwndWrapper;
    EbookController *   ebookController;
    TouchState          touchState;
    TCHAR *             LoadedFilePath() const;
};

MobiWindow* FindMobiWindowByController(EbookController *controller);
void   OpenMobiInWindow(MobiDoc *mobiDoc, SumatraWindow& winToReplace);
bool   RegisterMobiWinClass(HINSTANCE hinst);
void   DeleteMobiWindows();
void   RebuildMenuBarForMobiWindows();
size_t MobiWindowsCount();

#endif
