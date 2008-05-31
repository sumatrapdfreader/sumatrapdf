/*----------------------------------------------------------------------
Copyright (c)  Gipsysoft. All Rights Reserved.
File:	DialogSizer_Set.cpp
Web site: http://gipsysoft.com

This software is provided 'as-is', without any express or implied warranty.

In no event will the author be held liable for any damages arising from the
use of this software.

Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely, subject
to the following restrictions: 

1) The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a product,
	 an acknowledgment in the product documentation is requested but not required. 
2) Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software. Altered source is encouraged
	 to be submitted back to the original author so it can be shared with the
	 community. Please share your changes.
3) This notice may not be removed or altered from any source distribution.

Owner:	russf@gipsysoft.com
Purpose:	Main functionality for sizeable dialogs

	Store a local copy of the user settings
	Subclass the window
	Respond to various messages withinn the subclassed window.

----------------------------------------------------------------------*/
#include <windows.h>
#include <assert.h>
#include <prsht.h>
#include "WinHelper.h"
#include "DialogSizer.h"

#ifndef ASSERT
#define ASSERT assert
#endif

#define pcszDialogDataPropertyName _T("GipsySoftDialogSizerProperty")
#define pcszWindowProcPropertyName _T("GipsySoftDialogSizerWindowProc")

struct RegistryData
{
    WINDOWPLACEMENT	m_wpl;
};

struct DialogData
{
    LPCTSTR pcszName;

    //	The number of items contained in the psd member.
    //	Used in the DeferWindowPos structure and in allocating memory
    int nItemCount;
    DialogSizerSizingItem *psd;

    //	We need the smallest to respond to the WM_GETMINMAXINFO message
    POINT m_ptSmallest;

    //	We don't strictly speaking need to say how big the biggest can be but
    POINT m_ptLargest;
    bool m_bLargestSet;

    //	we need this to decide how much the window has changed size when we get a WM_SIZE message
    SIZE m_sizeClient;

    //	Draw the sizing grip...or not
    bool m_bMaximised;
    BOOL m_bShowSizingGrip;

    WinHelper::CRect m_rcGrip;
};

static LRESULT CALLBACK SizingProc(HWND, UINT, WPARAM, LPARAM);

// Given an array of dialog item structures determine how many of them there
// are by scanning along them until I reach the last.
static inline int GetItemCount(const DialogSizerSizingItem *psd)
{
    int nCount = 0;
    while (psd->uSizeInfo != 0xFFFFFFFF)
    {
        nCount++;
        psd++;
    }
    return nCount;
}

static void UpdateGripperRect(const int cx, const int cy, WinHelper::CRect &rcGrip)
{
    const int nGripWidth = GetSystemMetrics(SM_CYVSCROLL);
    const int nGripHeight = GetSystemMetrics(SM_CXVSCROLL);
    rcGrip.left = cx - nGripWidth;
    rcGrip.top = cy - nGripHeight;
    rcGrip.right = cx;
    rcGrip.bottom = cy;
}

static void UpdateGripper(HWND hwnd, DialogData *pdd)
{
    if (!pdd->m_bShowSizingGrip)
        return;

    WinHelper::CRect rcOld(pdd->m_rcGrip);
    UpdateGripperRect(pdd->m_sizeClient.cx, pdd->m_sizeClient.cy, pdd->m_rcGrip);
    // We also need to invalidate the combined area of the old and new rectangles
    // otherwise we would have trail of grippers when we sized the dialog larger
    // in any axis
    UnionRect(&rcOld, &rcOld, &pdd->m_rcGrip);
    InvalidateRect(hwnd, &rcOld, TRUE);
}

// Will copy all of the items in psdSource into psdDest.
static inline void CopyItems(DialogSizerSizingItem *psdDest, const DialogSizerSizingItem *psdSource)
{
    while (psdSource->uSizeInfo != 0xFFFFFFFF)
    {
        *psdDest = *psdSource;
        psdDest++;
        psdSource++;
    }
    *psdDest = *psdSource;
}

// Determine if the data already exists. If it does then return that, if not then we will
// create and initialise a brand new structure.
static DialogData * AddDialogData(HWND hwnd)
{
    DialogData *pdd = (DialogData *)GetProp(hwnd, pcszDialogDataPropertyName);
    if (!pdd)
    {
        pdd = (DialogData *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DialogData));
    }

    if (!pdd)
        return NULL;

    // Store some sizes etc. for later.
    WinHelper::CRect rc;
    GetWindowRect(hwnd, rc);
    pdd->m_ptSmallest.x = rc.Width();
    pdd->m_ptSmallest.y = rc.Height();

    GetClientRect(hwnd, rc);
    pdd->m_sizeClient = rc.Size();
    SetProp(hwnd, pcszDialogDataPropertyName, reinterpret_cast<HANDLE>(pdd));
    UpdateGripperRect(pdd->m_sizeClient.cx, pdd->m_sizeClient.cy, pdd->m_rcGrip);

    // Because we have successffuly created our data we need to subclass the control now, if not
    // we could end up in a situation where our data was never freed.
    SetProp(hwnd, pcszWindowProcPropertyName, reinterpret_cast<HANDLE>(SetWindowLong(hwnd, GWL_WNDPROC, reinterpret_cast<LONG>(SizingProc)))) ;
    return pdd;
}

// Setting a dialog sizeable involves subclassing the window and handling it's
// WM_SIZE messages, if we have a hkRootSave and pcszName then we will also be loading/saving
// the size and position of the window from the registry. We load from the registry when we 
// subclass the window and we save to the registry when we get a WM_DESTROY.
//
// Returns non-zero for success and zero if it fails
extern "C" BOOL DialogSizer_Set(HWND hwnd, const DialogSizerSizingItem *psd, BOOL bShowSizingGrip, SIZE *psizeMax)
{
    HANDLE heap = GetProcessHeap();
    DialogData *pdd = AddDialogData(hwnd);
    if (!pdd)
        return FALSE;

    pdd->m_bShowSizingGrip = bShowSizingGrip;
    pdd->nItemCount = GetItemCount(psd) + 1;
    pdd->psd = (DialogSizerSizingItem*)HeapAlloc(heap, 0, sizeof(DialogSizerSizingItem) * pdd->nItemCount);
    if (!pdd->psd) {
        HeapFree(heap, 0, pdd);
        return FALSE;
    }
    // Copy all of the user controls etc. for later, this way the user can quite happily
    // let the structure go out of scope.
    CopyItems(pdd->psd, psd);
    if (psizeMax)
    {
        pdd->m_ptLargest.x = psizeMax->cx;
        pdd->m_ptLargest.y = psizeMax->cy;
        pdd->m_bLargestSet = true;
    }
    return TRUE;
}

void UpdateWindowSize(const int cx, const int cy, HWND hwnd)
{
    DialogData *pdd = reinterpret_cast<DialogData *>(GetProp(hwnd, pcszDialogDataPropertyName));
    if (!pdd)
        return;

    const int nDeltaX = cx - pdd->m_sizeClient.cx;
    const int nDeltaY = cy - pdd->m_sizeClient.cy;
    WinHelper::CDeferWindowPos def(pdd->nItemCount);
    WinHelper::CRect rc;
    const DialogSizerSizingItem *psd = pdd->psd;
    while (psd->uSizeInfo != 0xFFFFFFFF)
    {
        HWND hwndChild = GetDlgItem(hwnd, psd->uControlID);
        ::GetWindowRect(hwndChild, rc);
        MapWindowPoints(GetDesktopWindow(), hwnd, (LPPOINT)&rc, 2);

        // Adjust the window horizontally
        if (psd->uSizeInfo & DS_MoveX)
        {
            rc.left += nDeltaX;
            rc.right += nDeltaX;
        }

        // Adjust the window vertically
        if (psd->uSizeInfo & DS_MoveY)
        {
            rc.top += nDeltaY;
            rc.bottom += nDeltaY;
        }

        // Size the window horizontally
        if (psd->uSizeInfo & DS_SizeX)
        {
            rc.right += nDeltaX;
        }

        // Size the window vertically
        if (psd->uSizeInfo & DS_SizeY)
        {
            rc.bottom += nDeltaY;
        }

        def.DeferWindowPos(hwndChild, NULL, rc, SWP_NOACTIVATE | SWP_NOZORDER);
        psd++;
    }

    pdd->m_sizeClient.cx = cx;
    pdd->m_sizeClient.cy = cy;

    // If we have a sizing grip enabled then adjust it's position
    UpdateGripper(hwnd, pdd);
}

// Actual window procedure that will handle saving window size/position and moving
// the controls whilst the window sizes.
static LRESULT CALLBACK SizingProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Using C style cast instead of reinterpret_cast to work around data to
    // function pointer cast error in GCC.
    WNDPROC pOldProc = (WNDPROC)GetProp(hwnd, pcszWindowProcPropertyName);
    switch (msg)
    {
        case WM_ERASEBKGND:
        {
            LRESULT lr = CallWindowProc(pOldProc, hwnd, msg, wParam, lParam);
            DialogData *pdd = reinterpret_cast<DialogData *>(GetProp(hwnd, pcszDialogDataPropertyName));
            if (pdd && pdd->m_bShowSizingGrip && !pdd->m_bMaximised)
            {
                DrawFrameControl(reinterpret_cast<HDC>(wParam), pdd->m_rcGrip, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
            }
            return lr;
        }

        case WM_SIZE:
        {
            DialogData *pdd = reinterpret_cast<DialogData *>(GetProp(hwnd, pcszDialogDataPropertyName));
            if (pdd && wParam != SIZE_MINIMIZED)
            {
                pdd->m_bMaximised = (wParam == SIZE_MAXIMIZED ? true : false);
                UpdateWindowSize(LOWORD(lParam), HIWORD(lParam), hwnd);
            }
        }
        break;

        case WM_NCHITTEST:
        {
            // If the gripper is enabled then perform a simple hit test on our gripper area.
            DialogData *pdd = reinterpret_cast<DialogData *>(GetProp(hwnd, pcszDialogDataPropertyName));
            if (pdd && pdd->m_bShowSizingGrip)
            {
                POINT pt = { LOWORD(lParam), HIWORD(lParam) };
                ScreenToClient(hwnd, &pt);
                if (PtInRect(pdd->m_rcGrip, pt))
                    return HTBOTTOMRIGHT;
            }
        }
        break;

        case WM_GETMINMAXINFO:
        {
            // Our opportunity to say that we do not want the dialog to grow or shrink any more.
            DialogData *pdd = reinterpret_cast<DialogData *>(GetProp(hwnd, pcszDialogDataPropertyName));
            LPMINMAXINFO lpmmi = reinterpret_cast<LPMINMAXINFO>(lParam);
            lpmmi->ptMinTrackSize = pdd->m_ptSmallest;
            if (pdd->m_bLargestSet)
            {
                lpmmi->ptMaxTrackSize = pdd->m_ptLargest;
            }
        }
        return 0;

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPNMHDR>(lParam)->code == PSN_SETACTIVE)
            {
                WinHelper::CRect rc;
                GetClientRect(GetParent(hwnd), &rc);
                UpdateWindowSize(rc.Width(), rc.Height(), GetParent(hwnd));
            }
        }
        break;

        case WM_DESTROY:
        {
            // Our opportunty for cleanup.
            // Simply acquire all of our objects, free the appropriate memory and remove the 
            // properties from the window. If we do not remove the properties then they will constitute
            // a resource leak.
            DialogData *pdd = reinterpret_cast<DialogData *>(GetProp(hwnd, pcszDialogDataPropertyName));
            if (pdd)
            {
                RegistryData rd;
                rd.m_wpl.length = sizeof(rd.m_wpl);
                GetWindowPlacement(hwnd, &rd.m_wpl);

                if (pdd->psd)
                {
                    HeapFree(GetProcessHeap(), 0, pdd->psd);
                }
                HeapFree(GetProcessHeap(), 0, pdd);
                RemoveProp(hwnd, pcszDialogDataPropertyName);
            }

            SetWindowLong(hwnd, GWL_WNDPROC, (LONG)pOldProc);
            RemoveProp(hwnd, pcszWindowProcPropertyName);
        }
        break;
    }
    return CallWindowProc(pOldProc, hwnd, msg, wParam, lParam);
}

