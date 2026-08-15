/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "gui/UIModels.h"
#include "gui/Gfx.h"
#include "gui/mac/GuiMacBridge.h"
#include "gui/PlatformWindow.h"

static void OnPaint(void* data, void* context, int width, int height) {
    auto* window = (PlatformWindow*)data;
    Gfx* gfx = GfxCreate(context);
    PlatformWindowPaintEvent ev{window, gfx, {0, 0, width, height}};
    window->onPaint.Call(&ev);
    delete gfx;
}

static bool OnPointer(void* data, const MacGuiPointerEvent* nativeEvent) {
    auto* window = (PlatformWindow*)data;
    PlatformPointerEvent ev;
    ev.window = window;
    ev.type = (PlatformPointerEventType)nativeEvent->type;
    ev.pos = {nativeEvent->x, nativeEvent->y};
    ev.button = nativeEvent->button;
    ev.isCtrl = nativeEvent->isCtrl;
    ev.isShift = nativeEvent->isShift;
    ev.isAlt = nativeEvent->isAlt;
    window->onPointer.Call(&ev);
    return ev.didHandle;
}

static bool OnKey(void* data, const MacGuiKeyEvent* nativeEvent) {
    auto* window = (PlatformWindow*)data;
    PlatformKeyEvent ev;
    ev.window = window;
    ev.codepoint = nativeEvent->codepoint;
    ev.isCtrl = nativeEvent->isCtrl;
    ev.isShift = nativeEvent->isShift;
    ev.isAlt = nativeEvent->isAlt;
    window->onKey.Call(&ev);
    return ev.didHandle;
}

static void OnClose(void* data) {
    auto* window = (PlatformWindow*)data;
    window->onCloseRequest.Call();
}

PlatformWindow* PlatformWindow::Create(const CreateArgs& args) {
    auto* window = new PlatformWindow();
    window->userData = args.userData;
    MacGuiWindowCallbacks callbacks{OnPaint, OnPointer, OnKey, OnClose};
    window->native =
        MacGuiWindowCreate(args.parent, args.title.s, len(args.title), args.initialSize.dx, args.initialSize.dy,
                           args.visible, args.frameless, args.resizable, window, &callbacks);
    if (!window->native) {
        delete window;
        return nullptr;
    }
    return window;
}

PlatformWindow::~PlatformWindow() {
    NativeWnd window = native;
    native = nullptr;
    if (window) {
        MacGuiWindowDestroy(window);
    }
}

static Rect ToRect(MacGuiRect r) {
    return {r.x, r.y, r.dx, r.dy};
}

static MacGuiRect ToMacRect(Rect r) {
    return {r.x, r.y, r.dx, r.dy};
}

Rect PlatformWindow::ClientRect() const {
    return ToRect(MacGuiWindowClientRect(native));
}

Rect PlatformWindow::ScreenRect() const {
    return PlatformWindowRect(native);
}

void PlatformWindow::SetBounds(Rect r) {
    MacGuiWindowSetBounds(native, ToMacRect(r));
}

void PlatformWindow::Show(bool show) {
    MacGuiWindowShow(native, show);
}

void PlatformWindow::Focus() {
    MacGuiWindowFocus(native);
}

void PlatformWindow::Invalidate() {
    MacGuiWindowInvalidate(native);
}

void PlatformWindow::SetCursor(CursorId id) {
    MacGuiWindowSetCursor(native, (int)id);
}

void PlatformWindow::BeginMove(const PlatformPointerEvent&) {
    MacGuiWindowBeginMove(native);
}

Rect PlatformWindowRect(NativeWnd native) {
    return ToRect(MacGuiWindowScreenRect(native));
}

Rect PlatformWindowWorkArea(NativeWnd native) {
    return ToRect(MacGuiWindowWorkArea(native));
}

bool PlatformWindowIsMaximized(NativeWnd native) {
    return MacGuiWindowIsMaximized(native);
}

void PlatformWindowActivateIfForeground(NativeWnd native) {
    MacGuiWindowActivateIfForeground(native);
}

struct PostedTask {
    Func0 fn;
};

static void RunPostedTask(void* data) {
    auto* task = (PostedTask*)data;
    task->fn.Call();
    delete task;
}

void PlatformPostTask(const Func0& fn) {
    MacGuiPostTask(RunPostedTask, new PostedTask{fn});
}
