#include <iostream>
#include <fstream>

#include "BaseUtil.h"
// layout controllers
#include "BaseEngine.h"
#include "SettingsStructs.h" // must be included before Controller.h
#include "EngineManager.h"
#include "Controller.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "Timer.h"
#include "DisplayModel.h"

#include "TestTextSearch.h"

/*
EngineType engineType;
BaseEngine *engine = EngineManager::CreateEngine(filePath, pwdUI, &engineType,
gGlobalPrefs->chmUI.useFixedPageUI,
gGlobalPrefs->ebookUI.useFixedPageUI);

if (engine) {
LoadEngineInFixedPageUI:
ctrl = new DisplayModel(engine, engineType, win->cbHandler);
*/

class DummyControllerCallback : public ControllerCallback {
public:
    virtual ~DummyControllerCallback() { }
    // tell the UI to show the pageNo as current page (which also syncs
    // the toc with the curent page). Needed for when a page change happens
    // indirectly or is initiated from within the model
    virtual void PageNoChanged(Controller *ctrl, int pageNo) { ctrl = nullptr; pageNo = 0; };
    // tell the UI to open the linked document or URL
    virtual void GotoLink(PageDestination *dest) { dest = nullptr; };
    // DisplayModel //
    virtual void Repaint() { };
    virtual void UpdateScrollbars(SizeI canvas) { canvas; };
    virtual void RequestRendering(int pageNo) { pageNo = 0; };
    virtual void CleanUp(DisplayModel *dm) { dm = nullptr;  };
    virtual void RenderThumbnail(DisplayModel *dm, SizeI size, const std::function<void(RenderedBitmap*)>&) { dm = nullptr; size; };
    // ChmModel //
    // tell the UI to move focus back to the main window
    // (if always == false, then focus is only moved if it's inside
    // an HtmlWindow and thus outside the reach of the main UI)
    virtual void FocusFrame(bool always) { always; };
    // tell the UI to let the user save the provided data to a file
    virtual void SaveDownload(const WCHAR *url, const unsigned char *data, size_t len) { url; data; len; };
    // EbookController //
    virtual void HandleLayoutedPages(EbookController *ctrl, EbookFormattingData *data) { ctrl; data; };
    virtual void RequestDelayedLayout(int delay) { delay; };
};

void _lossyWcharOut(std::ofstream &lf, const WCHAR *s)
{
    for (const WCHAR *p = s; *p; ++p) {
        lf << (((*p >= 32) && (*p <= 127)) ? ((char)(*p & 0x7f)) : '?');
    }
}

void SearchTestWithDir(WCHAR *searchFile, WCHAR *searchTerm, std::ofstream &logFile, TextSearchDirection direction)
{
    DummyControllerCallback dummyCb = DummyControllerCallback();
    EngineType engineType;
    BaseEngine *engine = EngineManager::CreateEngine(searchFile,
        /* pwdUI */ nullptr,
        &engineType);
    // Controller *ctrl = new DisplayModel(engine, engineType, &dummyCb);
    PageTextCache *textCache = new PageTextCache(engine);
    TextSearch *tsrch = new TextSearch(engine, textCache);
    tsrch->SetDirection(direction);
    TextSel *tsel = nullptr;
    Timer t1;
    tsel = tsrch->FindFirst((FIND_FORWARD == direction)?1:engine->PageCount(), searchTerm);
    int findCount = 0;
    if (nullptr != tsel) {
        do {
            ++findCount;
            logFile << "findCount=" << findCount << "\n";
            for (int i = 0; i < tsel->len; ++i) {
                RectI rect = tsel->rects[i];
                logFile << " pages[" << i << "]=" << tsel->pages[i]
                        << " rects[i]=(" << rect.x << ", " << rect.y << ", "
                        << rect.dx << ", " << rect.dy << ")\n";
            }
        } while ((tsel = tsrch->FindNext()) != nullptr);
    }
    double dur = t1.GetTimeInMs();
    logFile << "total=" << findCount << "\n";
    logFile << "Time: " << dur << "ms\n";
    delete tsrch;
    delete textCache;
}

void SearchTest(WCHAR *searchFile, WCHAR *searchTerm, WCHAR* resultFile)
{
    std::ofstream logFile(resultFile);
    _lossyWcharOut(logFile, searchTerm);
    logFile << " in ";
    _lossyWcharOut(logFile, searchFile);
    logFile << "\n";
    SearchTestWithDir(searchFile, searchTerm, logFile, FIND_FORWARD);
    logFile << "\n\nBackward:\n";
    SearchTestWithDir(searchFile, searchTerm, logFile, FIND_BACKWARD);
    logFile.close();
}