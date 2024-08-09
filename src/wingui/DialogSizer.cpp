/* Modified version of DialogSizer_Set.cpp. See DialogSizer.h for the original Copyright */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "DialogSizer.h"

#define DIALOG_DATA_PROPERTY L"GipsySoftDialogSizerData"

static LRESULT CALLBACK SizingProc(HWND, UINT, WPARAM, LPARAM);

class DialogData {
  public:
    DialogData(HWND hwnd, const DialogSizerSizingItem* psd, bool bShowSizingGrip)
        : hwnd(hwnd), bMaximised(false), bShowSizingGrip(bShowSizingGrip) {
        // Given an array of dialog item structures determine how many of them there
        // are by scanning along them until we reach the last.
        nItemCount = 0;
        for (const DialogSizerSizingItem* psi = psd; psi->uSizeInfo != 0xFFFFFFFF; psi++) {
            nItemCount++;
        }

        // Copy all of the user controls etc. for later, this way the user can quite happily
        // let the structure go out of scope.
        this->psd = (DialogSizerSizingItem*)memdup((void*)psd, nItemCount * sizeof(DialogSizerSizingItem));
        if (!this->psd) {
            nItemCount = 0;
        }

        // Store some sizes etc. for later.
        Rect rectWnd = WindowRect(hwnd);
        ptSmallest.x = rectWnd.dx;
        ptSmallest.y = rectWnd.dy;

        Rect rectClient = ClientRect(hwnd);
        sizeClient = rectClient.Size();
        UpdateGripperRect();

        // Because we have successfully created our data we need to subclass the control now, if not
        // we could end up in a situation where our data was never freed.
        SetProp(hwnd, DIALOG_DATA_PROPERTY, (HANDLE)this);
        wndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)SizingProc);
    }
    ~DialogData() {
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)wndProc);
        RemoveProp(hwnd, DIALOG_DATA_PROPERTY);
        free(this->psd);
    }

    // The number of items contained in the psd member.
    int nItemCount;
    DialogSizerSizingItem* psd;

    // We need the smallest to respond to the WM_GETMINMAXINFO message
    POINT ptSmallest;

    // We need this to decide how much the window has changed size when we get a WM_SIZE message
    Size sizeClient;
    bool bMaximised;

    void UpdateGripper() {
        if (!bShowSizingGrip) {
            return;
        }

        Rect rcOld = rcGrip;
        UpdateGripperRect();
        // We also need to invalidate the combined area of the old and new rectangles
        // otherwise we would have trail of grippers when we sized the dialog larger
        // in any axis
        RECT tmpRect = ToRECT(rcGrip.Union(rcOld));
        InvalidateRect(hwnd, &tmpRect, TRUE);
    }

    void DrawGripper(HDC hdc) {
        if (bShowSizingGrip && !bMaximised) {
            RECT tmpRect = ToRECT(rcGrip);
            DrawFrameControl(hdc, &tmpRect, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
        }
    }

    bool InsideGripper(Point pt) {
        return bShowSizingGrip && rcGrip.Contains(pt);
    }

    // The previous window procedure
    WNDPROC wndProc;

  private:
    HWND hwnd;
    Rect rcGrip;
    // Draw the sizing grip...or not
    bool bShowSizingGrip;

    void UpdateGripperRect() {
        int width = GetSystemMetrics(SM_CXVSCROLL);
        int height = GetSystemMetrics(SM_CYHSCROLL);
        rcGrip = Rect(sizeClient.dx - width, sizeClient.dy - height, width, height);
    }
};

// Setting a dialog sizeable involves subclassing the window and handling it's
// WM_SIZE messages.
//
// Returns non-zero for success and zero if it fails
extern "C" BOOL DialogSizer_Set(HWND hwnd, const DialogSizerSizingItem* psd, BOOL bShowSizingGrip) {
    DialogData* pdd = (DialogData*)GetProp(hwnd, DIALOG_DATA_PROPERTY);
    // Overwrite previous settings (if there are any)
    delete pdd;

    pdd = new DialogData(hwnd, psd, bShowSizingGrip);
    if (!pdd || !pdd->psd) {
        delete pdd;
        return FALSE;
    }

    return TRUE;
}

void UpdateWindowSize(DialogData* pdd, const int cx, const int cy, HWND hwnd) {
    const int nDeltaX = cx - pdd->sizeClient.dx;
    const int nDeltaY = cy - pdd->sizeClient.dy;

    HDWP hdwp = BeginDeferWindowPos(pdd->nItemCount);
    for (int i = 0; i < pdd->nItemCount; i++) {
        const DialogSizerSizingItem* psd = pdd->psd + i;
        HWND hwndChild = GetDlgItem(hwnd, psd->uControlID);
        Rect rect = MapRectToWindow(WindowRect(hwndChild), HWND_DESKTOP, hwnd);

        // Adjust the window horizontally
        if (psd->uSizeInfo & DS_MoveX) {
            rect.x += nDeltaX;
        }
        // Adjust the window vertically
        if (psd->uSizeInfo & DS_MoveY) {
            rect.y += nDeltaY;
        }
        // Size the window horizontally
        if (psd->uSizeInfo & DS_SizeX) {
            rect.dx += nDeltaX;
        }
        // Size the window vertically
        if (psd->uSizeInfo & DS_SizeY) {
            rect.dy += nDeltaY;
        }

        DeferWindowPos(hdwp, hwndChild, nullptr, rect.x, rect.y, rect.dx, rect.dy, SWP_NOACTIVATE | SWP_NOZORDER);
    }
    EndDeferWindowPos(hdwp);

    pdd->sizeClient = Size(cx, cy);
    // If we have a sizing grip enabled then adjust it's position
    pdd->UpdateGripper();
}

// Actual window procedure that will handle saving window size/position and moving
// the controls whilst the window sizes.
static LRESULT CALLBACK SizingProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    DialogData* pdd = (DialogData*)GetProp(hwnd, DIALOG_DATA_PROPERTY);
    if (!pdd) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_ERASEBKGND: {
            LRESULT lr = CallWindowProc(pdd->wndProc, hwnd, msg, wp, lp);
            pdd->DrawGripper((HDC)wp);
            return lr;
        }

        case WM_SIZE: {
            if (wp != SIZE_MINIMIZED) {
                pdd->bMaximised = wp == SIZE_MAXIMIZED;
                UpdateWindowSize(pdd, LOWORD(lp), HIWORD(lp), hwnd);
            }
        } break;

        case WM_NCHITTEST: {
            // If the gripper is enabled then perform a simple hit test on our gripper area.
            POINT pt = {LOWORD(lp), HIWORD(lp)};
            ScreenToClient(hwnd, &pt);
            if (pdd->InsideGripper(Point(pt.x, pt.y))) {
                return HTBOTTOMRIGHT;
            }
        } break;

        case WM_GETMINMAXINFO: {
            // Our opportunity to say that we do not want the dialog to grow or shrink any more.
            LPMINMAXINFO lpmmi = (LPMINMAXINFO)lp;
            lpmmi->ptMinTrackSize = pdd->ptSmallest;
        }
            return 0;

        case WM_DESTROY: {
            WNDPROC wndProc = pdd->wndProc;
            delete pdd;
            return CallWindowProc(wndProc, hwnd, msg, wp, lp);
        }
    }

    return CallWindowProc(pdd->wndProc, hwnd, msg, wp, lp);
}
