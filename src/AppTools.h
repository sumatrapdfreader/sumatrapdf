/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef AppTools_h
#define AppTools_h

#include "Vec.h"
#include "WindowInfo.h"

// Base class for code that has to be executed on UI thread. Derive your class
// from UIThreadWorkItem and call gUIThreadMarshaller.Queue() to schedule execution
// of its Execute() method on UI thread.
class UIThreadWorkItem
{
public:
    WindowInfo *win;

    UIThreadWorkItem(WindowInfo *win) : win(win) {}
    virtual ~UIThreadWorkItem() {}
    virtual void Execute() = 0;
};

class UIThreadWorkItemQueue
{
    CRITICAL_SECTION        cs;
    Vec<UIThreadWorkItem *> items;

public:
    UIThreadWorkItemQueue() {
        InitializeCriticalSection(&cs);
    }

    ~UIThreadWorkItemQueue() {
        DeleteCriticalSection(&cs);
        DeleteVecMembers(items);
    }

    void Queue(UIThreadWorkItem *item) {
        if (!item)
            return;

        ScopedCritSec scope(&cs);
        items.Append(item);

        if (item->win) {
            // hwndCanvas is less likely to enter internal message pump (during which
            // the messages are not visible to our processing in top-level message pump)
            PostMessage(item->win->hwndCanvas, WM_NULL, 0, 0);
        }
    }

    void Execute() {
        // no need to acquire a lock for this check
        if (items.Count() == 0)
            return;

        ScopedCritSec scope(&cs);
        while (items.Count() > 0) {
            UIThreadWorkItem *wi = items.At(0);
            items.RemoveAt(0);
            wi->Execute();
            delete wi;
        }
    }
};

bool IsValidProgramVersion(char *txt);
int CompareVersion(TCHAR *txt1, TCHAR *txt2);
const char *GuessLanguage();

TCHAR *ExePathGet();
bool IsRunningInPortableMode();
TCHAR *AppGenDataDir();
TCHAR *AppGenDataFilename(TCHAR *pFilename);
void AdjustRemovableDriveLetter(TCHAR *path);

void DoAssociateExeWithPdfExtension(HKEY hkey);
bool IsExeAssociatedWithPdfExtension();

TCHAR* GetAcrobatPath();
TCHAR* GetFoxitPath();
TCHAR* GetPDFXChangePath();

LPTSTR AutoDetectInverseSearchCommands(HWND hwndCombo=NULL);
void   DDEExecute(LPCTSTR server, LPCTSTR topic, LPCTSTR command);

#endif
