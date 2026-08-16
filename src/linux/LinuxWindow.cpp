/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include <gtk/gtk.h>

#include "linux/LinuxWindow.h"

struct LinuxWindow {
    GtkWidget* window = nullptr;
    GtkWidget* status = nullptr;
};

static void FreeLinuxWindow(gpointer data) {
    delete (LinuxWindow*)data;
}

LinuxWindow* LinuxWindowCreate(GtkApplication* app) {
    auto* result = new LinuxWindow();
    result->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(result->window), "SumatraPDF");
    gtk_window_set_default_size(GTK_WINDOW(result->window), 900, 700);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    result->status = gtk_label_new("Open a document to start reading.");
    gtk_widget_set_hexpand(result->status, TRUE);
    gtk_widget_set_vexpand(result->status, TRUE);
    gtk_widget_set_halign(result->status, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(result->status, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), result->status);
    gtk_window_set_child(GTK_WINDOW(result->window), box);

    g_object_set_data_full(G_OBJECT(result->window), "sumatra-linux-window", result, FreeLinuxWindow);
    return result;
}

void LinuxWindowOpenFile(LinuxWindow* window, GFile* file) {
    if (!window || !file) {
        return;
    }
    char* path = g_file_get_path(file);
    char* name = g_file_get_parse_name(file);
    gtk_label_set_text(GTK_LABEL(window->status), path ? path : name);
    gtk_window_set_title(GTK_WINDOW(window->window), name);
    g_free(name);
    g_free(path);
}

void LinuxWindowPresent(LinuxWindow* window) {
    if (window) {
        gtk_window_present(GTK_WINDOW(window->window));
    }
}
