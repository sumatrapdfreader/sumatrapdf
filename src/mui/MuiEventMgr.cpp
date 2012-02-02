/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"
#include "WindowsX.h"

namespace mui {

EventMgr::EventMgr(HwndWrapper *wndRoot)
    : wndRoot(wndRoot), currOver(NULL)
{
    CrashIf(!wndRoot);
    //CrashIf(wndRoot->hwnd);
}

EventMgr::~EventMgr()
{
}

// Set minimum size that will be enforced by handling WM_GETMINMAXINFO
// Default is (0,0), which is unlimited
void EventMgr::SetMinSize(Size s)
{
    minSize = s;
}

// Set maximum size that will be enforced by handling WM_GETMINMAXINFO
// Default is (0,0), which is unlimited
void EventMgr::SetMaxSize(Size s)
{
    maxSize = s;
}

void EventMgr::UnRegisterClickHandlers(IClickHandler *clickHandler)
{
    size_t i = 0;
    while (i < clickHandlers.Count()) {
        ClickHandler h = clickHandlers.At(i);
        if (h.clickHandler == clickHandler)
            clickHandlers.RemoveAtFast(i);
        else
            i++;
    }
}

void EventMgr::RegisterClickHandler(Control *wndSource, IClickHandler *clickHandler)
{
    ClickHandler ch = { wndSource, clickHandler };
    clickHandlers.Append(ch);
}

IClickHandler *EventMgr::GetClickHandlerFor(Control *wndSource)
{
    for (size_t i = 0; i < clickHandlers.Count(); i++) {
        ClickHandler ch = clickHandlers.At(i);
        if (ch.wndSource == wndSource)
            return ch.clickHandler;
    }
    return NULL;
}

// TODO: optimize by getting both mouse over and mouse move windows in one call
// x, y is a position in the root window
LRESULT EventMgr::OnMouseMove(WPARAM keys, int x, int y, bool& wasHandled)
{
    Vec<WndAndOffset> windows;
    Control *w;

    uint16 wantedInputMask = bit::FromBit<uint16>(Control::WantsMouseOverBit);
    size_t count = CollectWindowsAt(wndRoot, x, y, wantedInputMask, &windows);
    if (0 == count) {
        if (currOver) {
            currOver->SetIsMouseOver(false);
            currOver->NotifyMouseLeave();
            currOver = NULL;
        }
    } else {
        // TODO: should this take z-order into account ?
        w = windows.Last().wnd;
        if (w != currOver) {
            if (currOver) {
                currOver->SetIsMouseOver(false);
                currOver->NotifyMouseLeave();
            }
            currOver = w;
            currOver->SetIsMouseOver(true);
            currOver->NotifyMouseEnter();
        }
    }

    wantedInputMask = bit::FromBit<uint16>(Control::WantsMouseMoveBit);
    count = CollectWindowsAt(wndRoot, x, y, wantedInputMask, &windows);
    if (0 == count)
        return 0;
    w = windows.Last().wnd;
    w->MapRootToMyPos(x, y);
    w->NotifyMouseMove(x, y);
    return 0;
}

// TODO: quite possibly the real logic for generating "click" events is
// more complicated
// (x, y) is in the coordinates of the root window
LRESULT EventMgr::OnLButtonUp(WPARAM keys, int x, int y, bool& wasHandled)
{
    Vec<WndAndOffset> windows;
    uint16 wantedInputMask = bit::FromBit<uint16>(Control::WantsMouseClickBit);
    size_t count = CollectWindowsAt(wndRoot, x, y, wantedInputMask, &windows);
    if (0 == count)
        return 0;
    // TODO: should this take z-order into account?
    Control *w = windows.Last().wnd;
    w->MapRootToMyPos(x, y);
    IClickHandler *clickHandler = GetClickHandlerFor(w);
    if (clickHandler)
        clickHandler->Clicked(w, x, y);
    return 0;
}

static void SetIfNotZero(LONG& l, int i, bool& didSet)
{
    if (i != 0) {
        l = i;
        didSet = true;
    }
}

LRESULT EventMgr::OnGetMinMaxInfo(MINMAXINFO *info, bool& wasHandled)
{
    SetIfNotZero(info->ptMinTrackSize.x, minSize.Width,  wasHandled);
    SetIfNotZero(info->ptMinTrackSize.y, minSize.Height, wasHandled);
    SetIfNotZero(info->ptMaxTrackSize.x, maxSize.Width,  wasHandled);
    SetIfNotZero(info->ptMaxTrackSize.y, maxSize.Height, wasHandled);
    return 0;
}

LRESULT EventMgr::OnSetCursor(int x, int y, bool& wasHandled)
{
    if (currOver && currOver->hCursor) {
        SetCursor(currOver->hCursor);
        wasHandled = true;
    }
    return TRUE;
}

LRESULT EventMgr::OnMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& wasHandled)
{
    if (WM_SIZE == msg)
        wndRoot->RequestLayout();

    wndRoot->LayoutIfRequested();

    wasHandled = false;
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);

    if (WM_SETCURSOR == msg) {
        POINT pt;
        if (GetCursorPos(&pt) && ScreenToClient(wndRoot->hwndParent, &pt))
            return OnSetCursor(pt.x, pt.y, wasHandled);
        return 0;
    }

    if (WM_MOUSEMOVE == msg)
        return OnMouseMove(wParam, x, y, wasHandled);

    if (WM_LBUTTONUP == msg)
        return OnLButtonUp(wParam, x, y, wasHandled);

    if (WM_GETMINMAXINFO == msg)
        return OnGetMinMaxInfo((MINMAXINFO*)lParam, wasHandled);

    return 0;
}

}
