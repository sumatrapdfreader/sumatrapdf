/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "gui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "RefHover.h"

bool RefHoverIsLaunchLink(IPageDestination* dest) {
    if (!dest) {
        return false;
    }
    Kind k = dest->GetKind();
    return k == kindDestinationLaunchURL || k == kindDestinationLaunchFile;
}

static RefHoverState* gLiveStates[kRefHoverMaxLiveStates];

bool RefHoverIsLiveState(RefHoverState* s) {
    for (RefHoverState* live : gLiveStates) {
        if (live == s) {
            return true;
        }
    }
    return false;
}

void RefHoverRegisterLiveState(RefHoverState* s) {
    for (RefHoverState*& slot : gLiveStates) {
        if (!slot) {
            slot = s;
            return;
        }
    }
}

void RefHoverUnregisterLiveState(RefHoverState* s) {
    for (RefHoverState*& slot : gLiveStates) {
        if (slot == s) {
            slot = nullptr;
            return;
        }
    }
}

void RefHoverDropQueuedRender(RefHoverState* s) {
    if (s->queuedRender.valid && s->queuedRender.engine) {
        s->queuedRender.engine->Release();
    }
    s->queuedRender.valid = false;
    s->queuedRender.engine = nullptr;
}