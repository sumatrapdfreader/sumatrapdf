/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig's WM_GESTURE handling (OnGesture, Canvas.cpp): on a touch screen two
// fingers pinch to zoom, a finger pans with inertia, a sideways flick turns the
// page, a two-finger rotation rotates the view, a two-finger tap toggles
// fullscreen and press-and-tap cycles the zoom.
// ng: gpui's Windows backend handles no touch message, so this is done in the
// frame's subclass (gui/NativeWindow.cpp). It is inert until Windows sends a
// gesture message. Not ported: what orig recognizes from the pointer messages
// around a pan (long press selects a word, selection handles); those need
// WM_POINTER, which gpui does not pass on either.
// Written without a touch screen to try it on.

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "ReadAloud.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "gui/AppShell.h"
#include "gui/TouchGestures.h"

#include "SumatraLog.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// WM_GESTURE handling
struct TouchState {
    MainWindow* win = nullptr;
    // ng: the gesture began on the canvas of a document; the rest of its
    // messages are ours too
    bool onCanvas = false;
    bool panStarted = false;
    POINTS panPos{};
    float zoomIntermediate = 0;
};

static Vec<TouchState> gTouchStates;

static TouchState& TouchStateOf(MainWindow* win) {
    for (TouchState& ts : gTouchStates) {
        if (ts.win == win) {
            return ts;
        }
    }
    TouchState ts;
    ts.win = win;
    VecAppend(gTouchStates, ts);
    return gTouchStates[len(gTouchStates) - 1];
}

void TouchGesturesForget(MainWindow* win) {
    for (int i = len(gTouchStates) - 1; i >= 0; i--) {
        if (gTouchStates[i].win == win) {
            VecRemoveAt(gTouchStates, i);
        }
    }
}

// screen pixels -> canvas (document) pixels; false when the point is not on
// the canvas
static bool ScreenToCanvas(MainWindow* win, HWND hwnd, POINTS ptScreen, Point* out) {
    POINT p{ptScreen.x, ptScreen.y};
    ScreenToClient(hwnd, &p);
    float scale = (float)DpiGetForHwnd(hwnd) / 96.f;
    // canvasRc is in dips
    Rect rc = win->canvasRc;
    int x0 = (int)((float)rc.x * scale);
    int y0 = (int)((float)rc.y * scale);
    int dx = (int)((float)rc.dx * scale);
    int dy = (int)((float)rc.dy * scale);
    *out = Point(p.x - x0, p.y - y0);
    return p.x >= x0 && p.y >= y0 && p.x < x0 + dx && p.y < y0 + dy;
}

static u32 LowerU64(ULONGLONG v) {
    u32 res = (u32)v;
    return res;
}

static float ScaleZoomBy(MainWindow* win, float factor) {
    auto zoomVirt = win->ctrl->GetZoomVirtual(true);
    return factor * zoomVirt;
}

// ng: orig turns every gesture on for its canvas window once
// (SetGestureConfig, GC_ALLGESTURES). The frame is everything here, so the
// choice is made per touch: all gestures for one that lands on a document,
// Windows' defaults (no rotation, no sideways single-finger pan) anywhere else,
// where gpui's elements still get what they got before.
static void OnGestureNotify(MainWindow* win, HWND hwnd, LPARAM lp) {
    auto* gns = (GESTURENOTIFYSTRUCT*)lp;
    Point pt;
    bool onCanvas = gns && win->AsFixed() && ScreenToCanvas(win, hwnd, gns->ptsLocation, &pt);
    if (onCanvas) {
        GESTURECONFIG gc = {0, GC_ALLGESTURES, 0};
        SetGestureConfig(hwnd, 0, 1, &gc, sizeof(GESTURECONFIG));
        return;
    }
    DWORD panWant = GC_PAN | GC_PAN_WITH_SINGLE_FINGER_VERTICALLY | GC_PAN_WITH_GUTTER | GC_PAN_WITH_INERTIA;
    GESTURECONFIG gcs[] = {
        {GID_ZOOM, GC_ZOOM, 0},
        {GID_PAN, panWant, GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY},
        {GID_ROTATE, 0, GC_ROTATE},
        {GID_TWOFINGERTAP, GC_TWOFINGERTAP, 0},
        {GID_PRESSANDTAP, GC_PRESSANDTAP, 0},
    };
    SetGestureConfig(hwnd, 0, dimof(gcs), gcs, sizeof(GESTURECONFIG));
}

// returns false when the gesture isn't the canvas' and the default handling
// should have it
static bool OnGesture(MainWindow* win, HWND hwnd, LPARAM lp) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }

    HGESTUREINFO hgi = (HGESTUREINFO)lp;
    GESTUREINFO gi{};
    gi.cbSize = sizeof(GESTUREINFO);
    TouchState& touchState = TouchStateOf(win);

    BOOL ok = GetGestureInfo(hgi, &gi);
    if (!ok) {
        return false;
    }

    Point pt;
    bool onCanvas = ScreenToCanvas(win, hwnd, gi.ptsLocation, &pt);
    if (gi.dwID == GID_BEGIN) {
        touchState.onCanvas = onCanvas;
    }
    if (gi.dwID == GID_BEGIN || gi.dwID == GID_END || !touchState.onCanvas) {
        // GID_BEGIN / GID_END have to reach DefWindowProc
        return false;
    }

    switch (gi.dwID) {
        case GID_ZOOM: {
            auto curr = (float)LowerU64(gi.ullArguments);
            bool isBegin = gi.dwFlags & GF_BEGIN;
            // ng: the > 0 is for a GID_ZOOM that never had its GF_BEGIN here
            if (!isBegin && touchState.zoomIntermediate > 0) {
                auto prev = touchState.zoomIntermediate;
                float factor = curr / prev;
                float newZoom = ScaleZoomBy(win, factor);
                SmartZoom(win, newZoom, &pt, false);
            }
            touchState.zoomIntermediate = curr;
            break;
        }

        case GID_PAN: {
            // Flicking left or right changes the page,
            // panning moves the document in the scroll window
            if (gi.dwFlags == GF_BEGIN) {
                touchState.panStarted = true;
                touchState.panPos = gi.ptsLocation;
            } else if (touchState.panStarted) {
                int deltaX = touchState.panPos.x - gi.ptsLocation.x;
                int deltaY = touchState.panPos.y - gi.ptsLocation.y;
                touchState.panPos = gi.ptsLocation;

                // on left / right flick, go to next / prev page
                // unless we can pan/scroll the document
                bool isFlickX = (gi.dwFlags & GF_INERTIA) && (abs(deltaX) > abs(deltaY)) && (abs(deltaX) > 26);
                bool flipPage = false;
                if (!dm->NeedHScroll()) {
                    // if the page is fully visible
                    flipPage = true;
                }
                if (deltaX > 0 && !dm->CanScrollRight()) {
                    flipPage = true;
                }
                if (deltaX < 0 && !dm->CanScrollLeft()) {
                    flipPage = true;
                }

                if (isFlickX && flipPage) {
                    // deltaX < 0: finger moved left (content follows) -> leftward spatial nav
                    // In manga (R2L) mode, left advances (issue #3964)
                    if (deltaX < 0) {
                        bool goNext = dm->GetDisplayR2L();
                        dm->GoToPageHorizontal(false);
                        // show the right-hand part of the page we land on
                        int x = dm->canvasSize.dx - dm->viewPort.dx;
                        dm->ScrollXTo(x);
                        OnDocumentVerticalScrollIntent(win, goNext);
                    } else if (deltaX > 0) {
                        bool goNext = !dm->GetDisplayR2L();
                        dm->GoToPageHorizontal(true);
                        dm->ScrollXTo(0);
                        OnDocumentVerticalScrollIntent(win, goNext);
                    }
                    ReadAloudOnUserViewChanged(win);
                    // When we switch pages prevent further pan movement
                    // caused by the inertia
                    touchState.panStarted = false;
                } else {
                    // pan / scroll
                    bool canScrollRightBefore = dm->CanScrollRight();
                    bool canScrollLeftBefore = dm->CanScrollLeft();
                    win->MoveDocBy(deltaX, deltaY);

                    // if pan to the rigth edge, we want to "sticK" to it
                    // and only flip page on the next flick motion
                    bool stopPanning = false;
                    if (canScrollRightBefore != dm->CanScrollRight()) {
                        stopPanning = true;
                    }
                    if (canScrollLeftBefore != dm->CanScrollLeft()) {
                        stopPanning = true;
                    }
                    if (stopPanning) {
                        touchState.panStarted = false;
                    }
                }
            }
            break;
        }

        case GID_ROTATE:
            // Rotate the PDF 90 degrees in one direction
            if (gi.dwFlags == GF_END && dm) {
                // This is in radians
                double rads = GID_ROTATE_ANGLE_FROM_ARGUMENT(LowerU64(gi.ullArguments));
                // The angle from the rotate is the opposite of the Sumatra rotate, thus the negative
                double degrees = -rads * 180 / M_PI;

                // Playing with the app, I found that I often didn't go quit a full 90 or 180
                // degrees. Allowing rotate without a full finger rotate seemed more natural.
                if (degrees < -120 || degrees > 120) {
                    dm->RotateBy(180);
                } else if (degrees < -45) {
                    dm->RotateBy(-90);
                } else if (degrees > 45) {
                    dm->RotateBy(90);
                }
            }
            break;

        case GID_TWOFINGERTAP:
            // Two-finger tap toggles fullscreen mode
            ToggleFullScreen(win);
            break;

        case GID_PRESSANDTAP:
            // Toggle between Fit Page, Fit Width and Fit Content (same as 'z')
            if (gi.dwFlags == GF_BEGIN) {
                win->ToggleZoom();
            }
            break;

        default:
            // A gesture was not recognized
            break;
    }

    CloseGestureInfoHandle(hgi);
    AppShellInvalidate(win);
    return true;
}

bool TouchGesturesOnMessage(MainWindow* win, HWND hwnd, UINT msg, LPARAM lp) {
    if (msg == WM_GESTURENOTIFY) {
        OnGestureNotify(win, hwnd, lp);
        // the message itself always goes on to DefWindowProc
        return false;
    }
    if (msg == WM_GESTURE) {
        return OnGesture(win, hwnd, lp);
    }
    return false;
}
