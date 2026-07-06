/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
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

// Stack `top` above `bottom` into one new DIB-backed Pixmap. Left-aligned —
// the two crops come from different columns with unrelated absolute page-x
// ranges (a right-column continuation sits ~200+pt right of a left-column
// entry), so aligning by page coordinates would insert a large, arbitrary gap
// rather than a small nudge. The narrower crop is padded on the right with
// opaque white so it isn't stretched. Consumes neither input; caller frees
// both. Returns nullptr on OOM.
//
// Uses GDI BitBlt rather than a raw pixel memcpy: EngineBase::RenderPage can
// return a DIB at a bit depth other than 32bpp BGRA8 for near-monochrome
// content (e.g. an 8bpp palette DIB for a text-only crop) — a straight
// memcpy assuming 4 bytes/pixel then reads past each row's actual data and
// produces colorful noise. BitBlt handles the source/dest bit-depth
// conversion regardless of what RenderPage chose.
static Pixmap* StackPixmapsVertically(Pixmap* top, Pixmap* bottom) {
    int w = top->width > bottom->width ? top->width : bottom->width;
    int h = top->height + bottom->height;
    Pixmap* out = AllocPixmapDIB(w, h);
    if (!out) {
        return nullptr;
    }
    HDC screenDC = GetDC(nullptr);
    HDC outDC = CreateCompatibleDC(screenDC);
    HGDIOBJ oldOut = outDC ? SelectObject(outDC, out->hbmp) : nullptr;
    if (outDC && oldOut) {
        RECT full{0, 0, w, h};
        HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(outDC, &full, white);
        DeleteObject(white);

        if (top->hbmp) {
            HDC topDC = CreateCompatibleDC(screenDC);
            if (topDC) {
                HGDIOBJ oldTop = SelectObject(topDC, top->hbmp);
                BitBlt(outDC, 0, 0, top->width, top->height, topDC, 0, 0, SRCCOPY);
                SelectObject(topDC, oldTop);
                DeleteDC(topDC);
            }
        }
        if (bottom->hbmp) {
            HDC bottomDC = CreateCompatibleDC(screenDC);
            if (bottomDC) {
                HGDIOBJ oldBottom = SelectObject(bottomDC, bottom->hbmp);
                BitBlt(outDC, 0, top->height, bottom->width, bottom->height, bottomDC, 0, 0, SRCCOPY);
                SelectObject(bottomDC, oldBottom);
                DeleteDC(bottomDC);
            }
        }
        SelectObject(outDC, oldOut);
    }
    if (outDC) {
        DeleteDC(outDC);
    }
    ReleaseDC(nullptr, screenDC);
    return out;
}

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
    RectF cont = job->req.continuationRegion;
    if (job->bmp && cont.dx > 0.f && cont.dy > 0.f) {
        RenderPageArgs contArgs(job->req.pageNo, job->req.zoom, 0, &cont);
        Pixmap* contBmp = job->req.engine->RenderPage(contArgs);
        if (contBmp) {
            Pixmap* stacked = StackPixmapsVertically(job->bmp, contBmp);
            FreePixmap(contBmp);
            if (stacked) {
                FreePixmap(job->bmp);
                job->bmp = stacked;
            }
        }
    }
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