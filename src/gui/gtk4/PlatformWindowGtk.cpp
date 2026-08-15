/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "gui/UIModels.h"
#include "gui/Gfx.h"
#include "gui/PlatformWindow.h"

#include <gtk/gtk.h>

struct PlatformWindowGtkData {
    GtkWidget* drawing = nullptr;
    GtkEventController* activeController = nullptr;
};

static PlatformWindowGtkData* GtkData(PlatformWindow* window) {
    return (PlatformWindowGtkData*)window->platformData;
}

static GtkWindow* NativeAsGtkWindow(NativeWnd native) {
    return native ? GTK_WINDOW(native) : nullptr;
}

static void OnDraw(GtkDrawingArea*, cairo_t* cr, int width, int height, gpointer data) {
    auto* window = (PlatformWindow*)data;
    Gfx* gfx = GfxCreate(cr);
    PlatformWindowPaintEvent ev{window, gfx, {0, 0, width, height}};
    window->onPaint.Call(&ev);
    delete gfx;
}

static void FillModifiers(PlatformPointerEvent& ev, GtkEventController* controller) {
    GdkModifierType state = gtk_event_controller_get_current_event_state(controller);
    ev.isCtrl = (state & GDK_CONTROL_MASK) != 0;
    ev.isShift = (state & GDK_SHIFT_MASK) != 0;
    ev.isAlt = (state & GDK_ALT_MASK) != 0;
}

static void OnMotion(GtkEventControllerMotion* controller, double x, double y, gpointer data) {
    auto* window = (PlatformWindow*)data;
    PlatformPointerEvent ev;
    ev.window = window;
    ev.type = PlatformPointerEventType::Move;
    ev.pos = {(int)x, (int)y};
    FillModifiers(ev, GTK_EVENT_CONTROLLER(controller));
    window->onPointer.Call(&ev);
}

static void OnLeave(GtkEventControllerMotion*, gpointer data) {
    auto* window = (PlatformWindow*)data;
    PlatformPointerEvent ev;
    ev.window = window;
    ev.type = PlatformPointerEventType::Leave;
    window->onPointer.Call(&ev);
}

static void OnButton(GtkGestureClick* gesture, int, double x, double y, gpointer data, PlatformPointerEventType type) {
    auto* window = (PlatformWindow*)data;
    auto* gtkData = GtkData(window);
    PlatformPointerEvent ev;
    ev.window = window;
    ev.type = type;
    ev.pos = {(int)x, (int)y};
    ev.button = (int)gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
    FillModifiers(ev, GTK_EVENT_CONTROLLER(gesture));
    gtkData->activeController = GTK_EVENT_CONTROLLER(gesture);
    window->onPointer.Call(&ev);
    gtkData->activeController = nullptr;
    if (ev.didHandle) {
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
}

static void OnPressed(GtkGestureClick* gesture, int nPress, double x, double y, gpointer data) {
    OnButton(gesture, nPress, x, y, data, PlatformPointerEventType::Down);
}

static void OnReleased(GtkGestureClick* gesture, int nPress, double x, double y, gpointer data) {
    OnButton(gesture, nPress, x, y, data, PlatformPointerEventType::Up);
}

static gboolean OnKey(GtkEventControllerKey*, guint keyval, guint, GdkModifierType state, gpointer data) {
    auto* window = (PlatformWindow*)data;
    PlatformKeyEvent ev;
    ev.window = window;
    ev.codepoint = (int)gdk_keyval_to_unicode(keyval);
    ev.isCtrl = (state & GDK_CONTROL_MASK) != 0;
    ev.isShift = (state & GDK_SHIFT_MASK) != 0;
    ev.isAlt = (state & GDK_ALT_MASK) != 0;
    window->onKey.Call(&ev);
    return ev.didHandle;
}

static gboolean OnCloseRequest(GtkWindow*, gpointer data) {
    auto* window = (PlatformWindow*)data;
    window->onCloseRequest.Call();
    return TRUE;
}

PlatformWindow* PlatformWindow::Create(const CreateArgs& args) {
    auto* window = new PlatformWindow();
    auto* gtkData = new PlatformWindowGtkData();
    window->platformData = gtkData;
    window->userData = args.userData;

    GtkWidget* native = gtk_window_new();
    window->native = native;
    gtk_window_set_title(GTK_WINDOW(native), CStrTemp(args.title));
    gtk_window_set_decorated(GTK_WINDOW(native), !args.frameless);
    gtk_window_set_resizable(GTK_WINDOW(native), args.resizable);
    gtk_window_set_default_size(GTK_WINDOW(native), args.initialSize.dx, args.initialSize.dy);
    if (args.parent) {
        gtk_window_set_transient_for(GTK_WINDOW(native), NativeAsGtkWindow(args.parent));
    }

    GtkWidget* drawing = gtk_drawing_area_new();
    gtkData->drawing = drawing;
    gtk_widget_set_focusable(drawing, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing), OnDraw, window, nullptr);
    gtk_window_set_child(GTK_WINDOW(native), drawing);

    GtkEventController* motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(OnMotion), window);
    g_signal_connect(motion, "leave", G_CALLBACK(OnLeave), window);
    gtk_widget_add_controller(drawing, motion);

    GtkGesture* click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
    g_signal_connect(click, "pressed", G_CALLBACK(OnPressed), window);
    g_signal_connect(click, "released", G_CALLBACK(OnReleased), window);
    gtk_widget_add_controller(drawing, GTK_EVENT_CONTROLLER(click));

    GtkEventController* key = gtk_event_controller_key_new();
    g_signal_connect(key, "key-pressed", G_CALLBACK(OnKey), window);
    gtk_widget_add_controller(drawing, key);

    g_signal_connect(native, "close-request", G_CALLBACK(OnCloseRequest), window);
    if (args.visible) {
        gtk_window_present(GTK_WINDOW(native));
    }
    return window;
}

PlatformWindow::~PlatformWindow() {
    GtkWindow* window = NativeAsGtkWindow(native);
    native = nullptr;
    if (window) {
        g_signal_handlers_disconnect_by_data(window, this);
        gtk_window_destroy(window);
    }
    delete GtkData(this);
    platformData = nullptr;
}

Rect PlatformWindow::ClientRect() const {
    auto* data = GtkData((PlatformWindow*)this);
    if (!data || !data->drawing) {
        return {};
    }
    return {0, 0, gtk_widget_get_width(data->drawing), gtk_widget_get_height(data->drawing)};
}

Rect PlatformWindow::ScreenRect() const {
    return PlatformWindowRect(native);
}

void PlatformWindow::SetBounds(Rect r) {
    GtkWindow* window = NativeAsGtkWindow(native);
    if (window) {
        // GTK 4 intentionally leaves top-level placement to the compositor.
        // Transient windows are still placed near their parent.
        gtk_window_set_default_size(window, r.dx, r.dy);
    }
}

void PlatformWindow::Show(bool show) {
    GtkWindow* window = NativeAsGtkWindow(native);
    if (!window) {
        return;
    }
    if (show) {
        gtk_window_present(window);
    } else {
        gtk_widget_set_visible(GTK_WIDGET(window), FALSE);
    }
}

void PlatformWindow::Focus() {
    auto* data = GtkData(this);
    if (data && data->drawing) {
        gtk_widget_grab_focus(data->drawing);
    }
}

void PlatformWindow::Invalidate() {
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

void PlatformWindow::SetCursor(CursorId id) {
    auto* data = GtkData(this);
    if (data && data->drawing) {
        gtk_widget_set_cursor_from_name(data->drawing, CursorName(id));
    }
}

void PlatformWindow::BeginMove(const PlatformPointerEvent& ev) {
    auto* data = GtkData(this);
    if (!data || !data->drawing) {
        return;
    }
    GtkNative* nativeWidget = gtk_widget_get_native(data->drawing);
    GdkSurface* surface = nativeWidget ? gtk_native_get_surface(nativeWidget) : nullptr;
    if (!surface || !GDK_IS_TOPLEVEL(surface)) {
        return;
    }
    GtkEventController* controller = data->activeController;
    GdkDevice* device = controller ? gtk_event_controller_get_current_event_device(controller) : nullptr;
    guint32 time = controller ? gtk_event_controller_get_current_event_time(controller) : GDK_CURRENT_TIME;
    if (device) {
        gdk_toplevel_begin_move(GDK_TOPLEVEL(surface), device, ev.button, ev.pos.x, ev.pos.y, time);
    }
}

Rect PlatformWindowRect(NativeWnd native) {
    GtkWindow* window = NativeAsGtkWindow(native);
    if (!window) {
        return {};
    }
    GtkWidget* widget = GTK_WIDGET(window);
    return {0, 0, gtk_widget_get_width(widget), gtk_widget_get_height(widget)};
}

static GdkMonitor* MonitorForWindow(NativeWnd native, bool* owned) {
    *owned = false;
    GtkWindow* window = NativeAsGtkWindow(native);
    GdkDisplay* display = window ? gtk_widget_get_display(GTK_WIDGET(window)) : gdk_display_get_default();
    if (!display) {
        return nullptr;
    }
    if (window) {
        GtkNative* gtkNative = gtk_widget_get_native(GTK_WIDGET(window));
        GdkSurface* surface = gtkNative ? gtk_native_get_surface(gtkNative) : nullptr;
        if (surface) {
            GdkMonitor* monitor = gdk_display_get_monitor_at_surface(display, surface);
            if (monitor) {
                return monitor;
            }
        }
    }
    GListModel* monitors = gdk_display_get_monitors(display);
    GdkMonitor* monitor =
        g_list_model_get_n_items(monitors) > 0 ? (GdkMonitor*)g_list_model_get_item(monitors, 0) : nullptr;
    *owned = monitor != nullptr;
    return monitor;
}

Rect PlatformWindowWorkArea(NativeWnd native) {
    bool owned = false;
    GdkMonitor* monitor = MonitorForWindow(native, &owned);
    if (!monitor) {
        return {};
    }
    GdkRectangle r{};
    gdk_monitor_get_geometry(monitor, &r);
    if (owned) {
        g_object_unref(monitor);
    }
    return {r.x, r.y, r.width, r.height};
}

bool PlatformWindowIsMaximized(NativeWnd native) {
    GtkWindow* window = NativeAsGtkWindow(native);
    return window && gtk_window_is_maximized(window);
}

void PlatformWindowActivateIfForeground(NativeWnd native) {
    GtkWindow* window = NativeAsGtkWindow(native);
    if (window) {
        gtk_window_present(window);
    }
}

struct PostedTask {
    Func0 fn;
};

static gboolean RunPostedTask(gpointer data) {
    auto* task = (PostedTask*)data;
    task->fn.Call();
    delete task;
    return G_SOURCE_REMOVE;
}

void PlatformPostTask(const Func0& fn) {
    auto* task = new PostedTask{fn};
    g_idle_add(RunPostedTask, task);
}
