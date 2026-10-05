/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/UITask.h"

#include "gui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "MainWindow.h"
#include "gui/AppShell.h"
#include "RefHover.h"

struct RefHoverRenderJob {
    RefHoverState* s = nullptr;
    RefHoverState::RenderRequest req;
    Pixmap* bmp = nullptr;
};

static void RefHoverStartRenderJob(RefHoverRenderJob* job);

// ng: orig stacks the two crops with GDI BitBlt because EngineBase::RenderPage
// may hand back a DIB at a bit depth other than 32bpp BGRA8 (an 8bpp palette
// DIB for text-only content) that only the HBITMAP knows how to read. There is
// no GDI off Windows, so the odd format is normalized to 32bpp first and the
// stacking itself is a row copy.
static Pixmap* PixmapAsReadable32(Pixmap* px) {
    if (!px) {
        return nullptr;
    }
    if (px->format == PixmapFormat::BGRA8 || px->format == PixmapFormat::BGR8) {
        return nullptr;
    }
#if OS_WIN
    return PixmapCopyAs32bppDIB(px);
#else
    return nullptr;
#endif
}

static void BlitPixmapRows(Pixmap* dst, Pixmap* src, int atY) {
    int bpp = PixmapBytesPerPixel(src->format);
    int rowBytes = src->width * bpp;
    for (int y = 0; y < src->height; y++) {
        u8* s = src->data + ((size_t)y * src->stride);
        u8* d = dst->data + ((size_t)(y + atY) * dst->stride);
        if (bpp == 4) {
            memcpy(d, s, (size_t)rowBytes);
            continue;
        }
        for (int x = 0; x < src->width; x++) {
            d[(x * 4) + 0] = s[(x * 3) + 0];
            d[(x * 4) + 1] = s[(x * 3) + 1];
            d[(x * 4) + 2] = s[(x * 3) + 2];
            d[(x * 4) + 3] = 0xff;
        }
    }
}

// Stack `top` above `bottom` into one new Pixmap. Left-aligned — the two crops
// come from different columns with unrelated absolute page-x ranges (a
// right-column continuation sits ~200+pt right of a left-column entry), so
// aligning by page coordinates would insert a large, arbitrary gap rather than
// a small nudge. The narrower crop is padded on the right with opaque white so
// it isn't stretched. Consumes neither input; caller frees both. Returns
// nullptr on OOM.
static Pixmap* StackPixmapsVertically(Pixmap* top, Pixmap* bottom) {
    Pixmap* topConv = PixmapAsReadable32(top);
    Pixmap* bottomConv = PixmapAsReadable32(bottom);
    Pixmap* t = topConv ? topConv : top;
    Pixmap* b = bottomConv ? bottomConv : bottom;
    Pixmap* out = nullptr;
    bool ok = t->format != PixmapFormat::Native && b->format != PixmapFormat::Native && t->data && b->data;
    if (ok) {
        int w = t->width > b->width ? t->width : b->width;
        int h = t->height + b->height;
        out = AllocPixmap(w, h);
    }
    if (out) {
        memset(out->data, 0xff, (size_t)out->stride * (size_t)out->height);
        BlitPixmapRows(out, t, 0);
        BlitPixmapRows(out, b, t->height);
    }
    FreePixmap(topConv);
    FreePixmap(bottomConv);
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
        RefHoverFreeRenderImage(s);
        FreePixmap(s->bmp);
        s->bmp = job->bmp;
        if (job->req.showPopup) {
            s->displayed.destPageRaw = job->req.destPageRaw;
            s->displayed.destPage = job->req.pageNo;
            s->displayed.destX = job->req.destXRaw;
            s->displayed.destY = job->req.destYRaw;
            s->displayed.srcPage = job->req.srcPageRaw;
            s->displayed.srcRect = job->req.srcRectRaw;
            s->displayed.region = job->req.region;
            RefHoverShowPopup(s, job->req.screenPt);
        } else {
            AppShellInvalidate(s->win);
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
    RunAsync(fn, StrL("RefHoverRender"));
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
