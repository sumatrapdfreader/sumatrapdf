/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"
#include "WindowsX.h"
#include "DebugLog.h"

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

void EventMgr::UnRegisterEventHandler(EventHandler::Type type, void *handler)
{
    size_t i = 0;
    while (i < eventHandlers.Count()) {
        EventHandler h = eventHandlers.At(i);
        if (h.type != type) {
            i++;
            continue;
        }
        if (handler == h.handler)
            eventHandlers.RemoveAtFast(i);
        else
            i++;
    }
}

void EventMgr::UnRegisterClicked(IClicked *handler)
{
    void *tmp = reinterpret_cast<void*>(handler);
    UnRegisterEventHandler(EventHandler::Clicked, tmp);
}

void EventMgr::RegisterClicked(Control *ctrlSource, IClicked *handler)
{
    EventHandler h = { EventHandler::Clicked, ctrlSource, handler };
    eventHandlers.Append(h);
}

void EventMgr::NotifyClicked(Control *c, int x, int y)
{
    EventHandler *h;
    for (h = eventHandlers.IterStart(); h; h = eventHandlers.IterNext()) {
        if ((h->ctrlSource == c) && (h->type == EventHandler::Clicked)) {
            h->clicked->Clicked(c, x, y);
        }
    }
}

void EventMgr::UnRegisterSizeChanged(ISizeChanged *handler)
{
    void *tmp = reinterpret_cast<void*>(handler);
    UnRegisterEventHandler(EventHandler::SizeChanged, tmp);
}

void EventMgr::RegisterSizeChanged(Control *ctrlSource, ISizeChanged *handler)
{
    EventHandler h = { EventHandler::SizeChanged, ctrlSource, NULL };
    h.sizeChanged = handler;
    eventHandlers.Append(h);
}

void EventMgr::NotifySizeChanged(Control *c, int dx, int dy)
{
    EventHandler *h;
    for (h = eventHandlers.IterStart(); h; h = eventHandlers.IterNext()) {
        if ((h->ctrlSource == c) && (h->type == EventHandler::SizeChanged)) {
            h->sizeChanged->SizeChanged(c, dx, dy);
        }
    }
}

// TODO: optimize by getting both mouse over and mouse move windows in one call
// x, y is a position in the root window
LRESULT EventMgr::OnMouseMove(WPARAM keys, int x, int y, bool& wasHandled)
{
    Vec<CtrlAndOffset> windows;
    Control *c;

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
        c = windows.Last().c;
        if (c != currOver) {
            if (currOver) {
                currOver->SetIsMouseOver(false);
                currOver->NotifyMouseLeave();
            }
            currOver = c;
            currOver->SetIsMouseOver(true);
            currOver->NotifyMouseEnter();
        }
    }

    wantedInputMask = bit::FromBit<uint16>(Control::WantsMouseMoveBit);
    count = CollectWindowsAt(wndRoot, x, y, wantedInputMask, &windows);
    if (0 == count)
        return 0;
    c = windows.Last().c;
    c->MapRootToMyPos(x, y);
    c->NotifyMouseMove(x, y);
    return 0;
}

// TODO: quite possibly the real logic for generating "click" events is
// more complicated
// (x, y) is in the coordinates of the root window
LRESULT EventMgr::OnLButtonUp(WPARAM keys, int x, int y, bool& wasHandled)
{
    Vec<CtrlAndOffset> controls;
    uint16 wantedInputMask = bit::FromBit<uint16>(Control::WantsMouseClickBit);
    size_t count = CollectWindowsAt(wndRoot, x, y, wantedInputMask, &controls);
    if (0 == count)
        return 0;
    // TODO: should this take z-order into account?
    Control *c = controls.Last().c;
    c->MapRootToMyPos(x, y);
    NotifyClicked(c, x, y);
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
    if (WM_SIZE == msg) {
        int dx = LOWORD(lParam);
        int dy = HIWORD(lParam);
        lf("(%3d,%3d) EventMgr::OnMessage() WM_SIZE", dx, dy);
        wndRoot->RequestLayout();
    }

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
