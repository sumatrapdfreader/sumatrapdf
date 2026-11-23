/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "gui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "RefHover.h"

// one per window; a render result for a state not in here is dropped
static Vec<RefHoverState*> gLiveStates;

bool RefHoverIsLiveState(RefHoverState* s) {
    return VecContains(gLiveStates, s);
}

void RefHoverRegisterLiveState(RefHoverState* s) {
    if (!VecContains(gLiveStates, s)) {
        VecAppend(gLiveStates, s);
    }
}

void RefHoverUnregisterLiveState(RefHoverState* s) {
    VecRemove(gLiveStates, s);
}

void RefHoverDropQueuedRender(RefHoverState* s) {
    if (s->queuedRender.valid && s->queuedRender.engine) {
        s->queuedRender.engine->Release();
    }
    s->queuedRender.valid = false;
    s->queuedRender.engine = nullptr;
}
