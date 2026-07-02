/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Shared internals for the RefHover module split across multiple .cpp files.
// Not part of the public API — include only from RefHover*.cpp.

#include "RefHover.h"

#define REF_HOVER_CLASS L"SumatraPDFRefHover"

constexpr float kRefHoverRenderZoom = 1.5f;
constexpr int kRefHoverMaxPopupWidth = 1200;
constexpr int kRefHoverMaxPopupHeight = 600;
constexpr int kRefHoverBorder = 4;
constexpr int kRefHoverCursorPad = 30;
constexpr int kRefHoverScrollStepPx = 60;
constexpr float kRefHoverMinUserZoom = 0.4f;
constexpr float kRefHoverMaxUserZoom = 3.0f;
constexpr float kRefHoverUserZoomStep = 1.15f;

constexpr int kRefHoverMaxLiveStates = 32;

class EngineBase;
struct IPageDestination;

bool RefHoverIsLaunchLink(IPageDestination* dest);

bool RefHoverIsLiveState(RefHoverState* s);
void RefHoverRegisterLiveState(RefHoverState* s);
void RefHoverUnregisterLiveState(RefHoverState* s);
void RefHoverDropQueuedRender(RefHoverState* s);

bool RefHoverPopupCreate(RefHoverState* s, HWND hwndCanvas);

void RefHoverShowPopup(RefHoverState* s, Point screenPt);
void RefHoverRequestRender(RefHoverState* s, EngineBase* engine, RefHoverState::RenderRequest req);
bool RefHoverRerenderDisplayedRegion(RefHoverState* s, EngineBase* engine, int page, RectF region);
