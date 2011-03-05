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

    void MarshallOnUIThread();
};

class UIThreadWorkItemQueue : Vec<UIThreadWorkItem *>
{
    CRITICAL_SECTION cs;

public:
    UIThreadWorkItemQueue() : Vec<UIThreadWorkItem *>() {
        InitializeCriticalSection(&cs);
    }

    ~UIThreadWorkItemQueue() {
        DeleteCriticalSection(&cs);
        DeleteVecMembers(*this);
    }

    void Queue(UIThreadWorkItem *item) {
        ScopedCritSec scope(&cs);
        Append(item);
        // make sure to spin the message loop once
        PostMessage(item->hwnd, WM_NULL, 0, 0);
    }

    void Execute() {
        // no need to acquire a lock for this check
        if (Count() == 0)
            return;

        ScopedCritSec scope(&cs);
        for (size_t i = 0; i < Count(); i++) {
            UIThreadWorkItem *wi = At(i);
            wi->Execute();
        }
        DeleteVecMembers(*this);
    }
};

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
