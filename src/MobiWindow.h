#ifndef MobiWindow_h
#define MobiWindow_h

#include "BaseUtil.h"
#include "Mui.h"
#include "ThreadUtil.h"

struct EbookControls;
class  EbookController;
class  MobiDoc;

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
    MobiWindow() : 
        hwndFrame(NULL), ebookControls(NULL), 
        hwndWrapper(NULL), ebookController(NULL) 
    { }
    ~MobiWindow() {}
    HWND                hwndFrame;
    EbookControls *     ebookControls;
    mui::HwndWrapper *  hwndWrapper;
    EbookController *   ebookController;

    TCHAR *             LoadedFilePath() const;
};

MobiWindow* FindMobiWindowByController(EbookController *controller);
void OpenMobiInWindow(MobiDoc *mobiDoc);
bool RegisterMobiWinClass(HINSTANCE hinst);
void DeleteMobiWindows();

#endif
