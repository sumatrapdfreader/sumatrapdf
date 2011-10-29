/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef AppTools_h
#define AppTools_h

#include "Vec.h"
#include "Scopes.h"

class WindowInfo;

// Base class for code that has to be executed on UI thread. Derive your class
// from UIThreadWorkItem and call QueueWorkItem to schedule execution
// of its Execute() method on UI thread.
class UIThreadWorkItem
{
public:
    WindowInfo *win;

    UIThreadWorkItem(WindowInfo *win) : win(win) {}
    virtual ~UIThreadWorkItem() {}
    virtual void Execute() = 0;
};

void QueueWorkItem(UIThreadWorkItem *wi);

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

    void Queue(UIThreadWorkItem *item);

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

bool IsRunningInPortableMode();
TCHAR *AppGenDataFilename(TCHAR *pFilename);
void AdjustRemovableDriveLetter(TCHAR *path);

void DoAssociateExeWithPdfExtension(HKEY hkey);
bool IsExeAssociatedWithPdfExtension();

TCHAR *ExtractFilenameFromURL(const TCHAR *url);
bool IsUntrustedFile(const TCHAR *filePath, const TCHAR *fileUrl=NULL);

LPTSTR AutoDetectInverseSearchCommands(HWND hwndCombo=NULL);
void   DDEExecute(LPCTSTR server, LPCTSTR topic, LPCTSTR command);

bool ExtendedEditWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void EnsureAreaVisibility(RectI& rect);
RectI GetDefaultWindowPos();

#endif
