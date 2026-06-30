/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/Thread.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "RefHoverInternal.h"

struct RefHoverRenderJob {
    RefHoverState* s = nullptr;
    RefHoverState::RenderRequest req;
    Pixmap* bmp = nullptr;
};

static void RefHoverStartRenderJob(RefHoverRenderJob* job);

static void RefHoverRenderDone(RefHoverRenderJob* job) {
    RefHoverState* s = job->s;
    if (!RefHoverIsLiveState(s)) {
        FreePixmap(job->bmp);
        delete job;
        return;
    }
    s->renderInFlight = false;
    if (job->bmp && job->req.gen == s->renderGen) {
        FreePixmap(s->bmp);
        s->bmp = job->bmp;
        if (job->req.showPopup) {
            s->displayed.destPage = job->req.pageNo;
            s->displayed.destX = job->req.destXRaw;
            s->displayed.destY = job->req.destYRaw;
            s->displayed.srcPage = job->req.srcPageRaw;
            s->displayed.srcRect = job->req.srcRectRaw;
            s->displayed.region = job->req.region;
            RefHoverShowPopup(s, job->req.screenPt);
        } else {
            InvalidateRect(s->hwndPopup, nullptr, TRUE);
        }
    } else {
        FreePixmap(job->bmp);
    }
    delete job;
    if (s->queuedRender.valid) {
        if (s->queuedRender.gen == s->renderGen) {
            auto* next = new RefHoverRenderJob();
            next->s = s;
            next->req = s->queuedRender;
            s->queuedRender.valid = false;
            s->queuedRender.engine = nullptr;
            RefHoverStartRenderJob(next);
        } else {
            RefHoverDropQueuedRender(s);
        }
    }
}

static void RefHoverRenderThread(RefHoverRenderJob* job) {
    RenderPageArgs args(job->req.pageNo, job->req.zoom, 0, &job->req.region);
    job->bmp = job->req.engine->RenderPage(args);
    job->req.engine->Release();
    job->req.engine = nullptr;
    auto fn = MkFunc0<RefHoverRenderJob>(RefHoverRenderDone, job);
    uitask::Post(fn, "RefHoverRenderDone");
}

static void RefHoverStartRenderJob(RefHoverRenderJob* job) {
    job->s->renderInFlight = true;
    auto fn = MkFunc0<RefHoverRenderJob>(RefHoverRenderThread, job);
    RunAsync(fn, "RefHoverRender");
}

void RefHoverRequestRender(RefHoverState* s, EngineBase* engine, RefHoverState::RenderRequest req) {
    s->renderGen++;
    req.valid = true;
    req.gen = s->renderGen;
    engine->AddRef();
    req.engine = engine;
    if (s->renderInFlight) {
        RefHoverDropQueuedRender(s);
        s->queuedRender = req;
        return;
    }
    auto* job = new RefHoverRenderJob();
    job->s = s;
    job->req = req;
    RefHoverStartRenderJob(job);
}