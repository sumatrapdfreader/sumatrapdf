/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include <gtk/gtk.h>

#include "linux/LinuxWindow.h"
#include "linux/LinuxApp.h"

struct LinuxAppState {
    LinuxWindow* window = nullptr;
};

static LinuxAppState* GetState(GtkApplication* app) {
    return (LinuxAppState*)g_object_get_data(G_OBJECT(app), "sumatra-linux-state");
}

static LinuxWindow* EnsureWindow(GtkApplication* app) {
    LinuxAppState* state = GetState(app);
    if (!state->window) {
        state->window = LinuxWindowCreate(app);
    }
    return state->window;
}

static void OnActivate(GtkApplication* app, gpointer) {
    LinuxWindowPresent(EnsureWindow(app));
}

static void OnOpen(GtkApplication* app, GFile** files, int nFiles, const char*, gpointer) {
    LinuxWindow* window = EnsureWindow(app);
    if (nFiles > 0) {
        LinuxWindowOpenFile(window, files[0]);
    }
    LinuxWindowPresent(window);
}

static void FreeState(gpointer data) {
    delete (LinuxAppState*)data;
}

int RunLinuxApp(int argc, char** argv) {
    GtkApplication* app = gtk_application_new("org.sumatrapdf.SumatraPDF", G_APPLICATION_HANDLES_OPEN);
    auto* state = new LinuxAppState();
    g_object_set_data_full(G_OBJECT(app), "sumatra-linux-state", state, FreeState);
    g_signal_connect(app, "activate", G_CALLBACK(OnActivate), nullptr);
    g_signal_connect(app, "open", G_CALLBACK(OnOpen), nullptr);

    int code = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return code;
}
