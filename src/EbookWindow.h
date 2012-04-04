#ifndef EbookWindow_h
#define EbookWindow_h

#include "BaseUtil.h"
#include "Doc.h"
#include "Mui.h"
#include "SumatraWindow.h"
#include "ThreadUtil.h"
#include "WindowInfo.h"

struct EbookControls;
class  EbookController;

class ThreadLoadEbook : public ThreadBase {
    TCHAR *             fileName; // we own this memory
    EbookController *   controller;
public:
    SumatraWindow       win;

    ThreadLoadEbook(const TCHAR *fileName, EbookController *controller, const SumatraWindow& sumWin);
    virtual ~ThreadLoadEbook() { free(fileName); }

    // ThreadBase
    virtual void Run();
};

class EbookWindow {
public:
    EbookWindow() : 
        hwndFrame(NULL), ebookControls(NULL), 
        hwndWrapper(NULL), ebookController(NULL) 
    { touchState.panStarted = false; }
    ~EbookWindow() {}
    HWND                hwndFrame;
    EbookControls *     ebookControls;
    mui::HwndWrapper *  hwndWrapper;
    EbookController *   ebookController;
    TouchState          touchState;
    TCHAR *             LoadedFilePath() const;
};

EbookWindow* FindEbookWindowByController(EbookController *controller);
void   OpenMobiInWindow(Doc doc, SumatraWindow& winToReplace);
bool   RegisterMobiWinClass(HINSTANCE hinst);
void   RebuildMenuBarForEbookWindows();
void   DeleteEbookWindow(EbookWindow *win, bool forceDelete = false);

#endif
