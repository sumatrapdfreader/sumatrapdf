/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef APP_TOOLS_H_
#define APP_TOOLS_H_

#include "Vec.h"

// Class to encapsulate code that has to be executed on UI thread but can be
// triggered from other threads. Instead of defining a new message for
// each piece of code, derive from UIThreadWorkItem and implement
// Execute() method
class UIThreadWorkItem
{
public:
    HWND hwnd;
    UIThreadWorkItem(HWND hwnd) : hwnd(hwnd) {}
    virtual ~UIThreadWorkItem() {}
    virtual void Execute() = 0;
};

class UIThreadWorkItemQueue
{
    Vec<UIThreadWorkItem *>  items;
    CRITICAL_SECTION         cs;

public:
    UIThreadWorkItemQueue() {
        InitializeCriticalSection(&cs);
    }

    ~UIThreadWorkItemQueue() {
        DeleteCriticalSection(&cs);
        while (items.Count() > 0) {
            delete items[0];
            items.RemoveAt(0);
        }
    }

    void Push(UIThreadWorkItem *item) {
        ScopedCritSec scope(&cs);
        items.Push(item);
    }

    bool ExecuteNextAndRemove() {
        ScopedCritSec scope(&cs);
        if (items.Count() > 0) {
            UIThreadWorkItem *item = items[0];
            item->Execute();
            delete item;
            items.RemoveAt(0);
            return true;
        }
        return false;
    }
};

void MarshallOnUIThread(UIThreadWorkItem *wi);
bool ExecuteAndRemoveNextUIThreadWorkItem();

bool IsValidProgramVersion(char *txt);
int CompareVersion(TCHAR *txt1, TCHAR *txt2);
const char *GuessLanguage();

TCHAR *ExePathGet();
bool IsRunningInPortableMode();
TCHAR *AppGenDataFilename(TCHAR *pFilename);
void AdjustRemovableDriveLetter(TCHAR *path);

void DoAssociateExeWithPdfExtension(HKEY hkey);
bool IsExeAssociatedWithPdfExtension();

bool GetAcrobatPath(TCHAR *bufOut=NULL, int bufCchSize=0);
bool GetFoxitPath(TCHAR *buffer=NULL, int bufCchSize=0);
bool GetPDFXChangePath(TCHAR *bufOut=NULL, int bufCchSize=0);

LPTSTR AutoDetectInverseSearchCommands(HWND hwndCombo=NULL);
void DDEExecute(LPCTSTR server, LPCTSTR topic, LPCTSTR command);

HFONT Win32_Font_GetSimple(HDC hdc, TCHAR *fontName, int fontSize);
void Win32_Font_Delete(HFONT font);

#endif
