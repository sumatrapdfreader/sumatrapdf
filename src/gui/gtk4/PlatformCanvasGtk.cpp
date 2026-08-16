/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "gui/UIModels.h"
#include "gui/Gfx.h"

#include <gtk/gtk.h>

#include "gui/PlatformCanvas.h"

struct PlatformCanvasGtkData {
    GtkWidget* drawing = nullptr;
};

static PlatformCanvasGtkData* GtkData(PlatformCanvas* canvas) {
    return (PlatformCanvasGtkData*)canvas->platformData;
}

static void FillModifiers(GtkEventController* controller, bool& isCtrl, bool& isShift, bool& isAlt) {
    GdkModifierType state = gtk_event_controller_get_current_event_state(controller);
    isCtrl = (state & GDK_CONTROL_MASK) != 0;
    isShift = (state & GDK_SHIFT_MASK) != 0;
    isAlt = (state & GDK_ALT_MASK) != 0;
}

static void OnDraw(GtkDrawingArea*, cairo_t* cr, int width, int height, gpointer data) {
    auto* canvas = (PlatformCanvas*)data;
    Gfx* gfx = GfxCreate(cr);
    PlatformCanvasPaintEvent ev{canvas, gfx, {0, 0, width, height}};
    canvas->onPaint.Call(&ev);
    delete gfx;
}

static void OnMotion(GtkEventControllerMotion* controller, double x, double y, gpointer data) {
    auto* canvas = (PlatformCanvas*)data;
    PlatformCanvasPointerEvent ev;
    ev.canvas = canvas;
    ev.type = PlatformCanvasPointerEventType::Move;
    ev.pos = {(int)x, (int)y};
    FillModifiers(GTK_EVENT_CONTROLLER(controller), ev.isCtrl, ev.isShift, ev.isAlt);
    canvas->onPointer.Call(&ev);
}

static void OnLeave(GtkEventControllerMotion*, gpointer data) {
    auto* canvas = (PlatformCanvas*)data;
    PlatformCanvasPointerEvent ev;
    ev.canvas = canvas;
    ev.type = PlatformCanvasPointerEventType::Leave;
    canvas->onPointer.Call(&ev);
}

static void OnButton(GtkGestureClick* gesture, double x, double y, gpointer data, PlatformCanvasPointerEventType type) {
    auto* canvas = (PlatformCanvas*)data;
    PlatformCanvasPointerEvent ev;
    ev.canvas = canvas;
    ev.type = type;
    ev.pos = {(int)x, (int)y};
    ev.button = (int)gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
    FillModifiers(GTK_EVENT_CONTROLLER(gesture), ev.isCtrl, ev.isShift, ev.isAlt);
    canvas->onPointer.Call(&ev);
    if (ev.didHandle) {
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
}

static void OnPressed(GtkGestureClick* gesture, int, double x, double y, gpointer data) {
    OnButton(gesture, x, y, data, PlatformCanvasPointerEventType::Down);
}

static void OnReleased(GtkGestureClick* gesture, int, double x, double y, gpointer data) {
    OnButton(gesture, x, y, data, PlatformCanvasPointerEventType::Up);
}

static gboolean OnScroll(GtkEventControllerScroll* controller, double dx, double dy, gpointer data) {
    auto* canvas = (PlatformCanvas*)data;
    PlatformCanvasScrollEvent ev;
    ev.canvas = canvas;
    ev.deltaX = dx;
    ev.deltaY = dy;
    FillModifiers(GTK_EVENT_CONTROLLER(controller), ev.isCtrl, ev.isShift, ev.isAlt);
    GdkEvent* event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
    double x = 0;
    double y = 0;
    if (event) {
        gdk_event_get_position(event, &x, &y);
    }
    ev.pos = {(int)x, (int)y};
    canvas->onScroll.Call(&ev);
    return ev.didHandle;
}

static PlatformKey KeyFromGdk(guint keyval) {
    switch (keyval) {
        case GDK_KEY_Escape:
            return PlatformKey::Escape;
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            return PlatformKey::Enter;
        case GDK_KEY_Home:
            return PlatformKey::Home;
        case GDK_KEY_End:
            return PlatformKey::End;
        case GDK_KEY_Page_Up:
            return PlatformKey::PageUp;
        case GDK_KEY_Page_Down:
            return PlatformKey::PageDown;
        case GDK_KEY_Left:
            return PlatformKey::Left;
        case GDK_KEY_Up:
            return PlatformKey::Up;
        case GDK_KEY_Right:
            return PlatformKey::Right;
        case GDK_KEY_Down:
            return PlatformKey::Down;
        case GDK_KEY_plus:
        case GDK_KEY_equal:
        case GDK_KEY_KP_Add:
            return PlatformKey::Plus;
        case GDK_KEY_minus:
        case GDK_KEY_KP_Subtract:
            return PlatformKey::Minus;
        case GDK_KEY_0:
        case GDK_KEY_KP_0:
            return PlatformKey::Zero;
    }
    return PlatformKey::None;
}

static gboolean OnKey(GtkEventControllerKey*, guint keyval, guint, GdkModifierType state, gpointer data) {
    auto* canvas = (PlatformCanvas*)data;
    PlatformCanvasKeyEvent ev;
    ev.canvas = canvas;
    ev.key = KeyFromGdk(keyval);
    ev.codepoint = (int)gdk_keyval_to_unicode(keyval);
    ev.isCtrl = (state & GDK_CONTROL_MASK) != 0;
    ev.isShift = (state & GDK_SHIFT_MASK) != 0;
    ev.isAlt = (state & GDK_ALT_MASK) != 0;
    canvas->onKey.Call(&ev);
    return ev.didHandle;
}

PlatformCanvas* PlatformCanvas::Create(void* userData) {
    auto* canvas = new PlatformCanvas();
    auto* data = new PlatformCanvasGtkData();
    canvas->platformData = data;
    canvas->userData = userData;

    GtkWidget* drawing = gtk_drawing_area_new();
    g_object_ref_sink(drawing);
    data->drawing = drawing;
    canvas->native = drawing;
    gtk_widget_set_focusable(drawing, TRUE);
    gtk_widget_set_hexpand(drawing, TRUE);
    gtk_widget_set_vexpand(drawing, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing), OnDraw, canvas, nullptr);

    GtkEventController* motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(OnMotion), canvas);
    g_signal_connect(motion, "leave", G_CALLBACK(OnLeave), canvas);
    gtk_widget_add_controller(drawing, motion);

    GtkGesture* click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
    g_signal_connect(click, "pressed", G_CALLBACK(OnPressed), canvas);
    g_signal_connect(click, "released", G_CALLBACK(OnReleased), canvas);
    gtk_widget_add_controller(drawing, GTK_EVENT_CONTROLLER(click));

    GtkEventController* scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    g_signal_connect(scroll, "scroll", G_CALLBACK(OnScroll), canvas);
    gtk_widget_add_controller(drawing, scroll);

    GtkEventController* key = gtk_event_controller_key_new();
    g_signal_connect(key, "key-pressed", G_CALLBACK(OnKey), canvas);
    gtk_widget_add_controller(drawing, key);
    return canvas;
}

PlatformCanvas::~PlatformCanvas() {
    auto* data = GtkData(this);
    native = nullptr;
    if (data && data->drawing) {
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(data->drawing), nullptr, nullptr, nullptr);
        g_signal_handlers_disconnect_by_data(data->drawing, this);
        g_object_unref(data->drawing);
    }
    delete data;
    platformData = nullptr;
}

void* PlatformCanvas::NativeWidget() const {
    return native;
}

Rect PlatformCanvas::ClientRect() const {
    auto* data = GtkData((PlatformCanvas*)this);
    if (!data || !data->drawing) {
        return {};
    }
    return {0, 0, gtk_widget_get_width(data->drawing), gtk_widget_get_height(data->drawing)};
}

void PlatformCanvas::Focus() {
    auto* data = GtkData(this);
    if (data && data->drawing) {
        gtk_widget_grab_focus(data->drawing);
    }
}

void PlatformCanvas::Invalidate() {
    auto* data = GtkData(this);
    if (data && data->drawing) {
        gtk_widget_queue_draw(data->drawing);
    }
}

static const char* CursorName(CursorId id) {
    switch (id) {
        case CursorId::Arrow:
            return "default";
        case CursorId::IBeam:
            return "text";
        case CursorId::Hand:
            return "pointer";
        case CursorId::Cross:
            return "crosshair";
        case CursorId::Move:
            return "move";
        case CursorId::SizeNS:
            return "ns-resize";
        case CursorId::SizeWE:
            return "ew-resize";
        case CursorId::No:
            return "not-allowed";
        case CursorId::None:
            return nullptr;
    }
    return nullptr;
}

void PlatformCanvas::SetCursor(CursorId id) {
    auto* data = GtkData(this);
    if (data && data->drawing) {
        gtk_widget_set_cursor_from_name(data->drawing, CursorName(id));
    }
}
