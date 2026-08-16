/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "gui/DocumentView.h"

#include <gtk/gtk.h>

#include "linux/LinuxWindow.h"

struct LinuxWindow {
    GtkWidget* window = nullptr;
    GtkWidget* stack = nullptr;
    GtkWidget* status = nullptr;
    DocumentView* view = nullptr;
};

static void FreeLinuxWindow(gpointer data) {
    auto* window = (LinuxWindow*)data;
    delete window->view;
    delete window;
}

LinuxWindow* LinuxWindowCreate(GtkApplication* app) {
    auto* result = new LinuxWindow();
    result->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(result->window), "SumatraPDF");
    gtk_window_set_default_size(GTK_WINDOW(result->window), 900, 700);

    result->stack = gtk_stack_new();
    result->status = gtk_label_new("Open a document to start reading.");
    gtk_widget_set_hexpand(result->status, TRUE);
    gtk_widget_set_vexpand(result->status, TRUE);
    gtk_widget_set_halign(result->status, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(result->status, GTK_ALIGN_CENTER);
    gtk_stack_add_named(GTK_STACK(result->stack), result->status, "status");

    result->view = DocumentView::Create();
    if (result->view) {
        gtk_stack_add_named(GTK_STACK(result->stack), GTK_WIDGET(result->view->NativeWidget()), "document");
    }
    gtk_stack_set_visible_child_name(GTK_STACK(result->stack), "status");
    gtk_window_set_child(GTK_WINDOW(result->window), result->stack);

    g_object_set_data_full(G_OBJECT(result->window), "sumatra-linux-window", result, FreeLinuxWindow);
    return result;
}

void LinuxWindowOpenFile(LinuxWindow* window, GFile* file) {
    if (!window || !file) {
        return;
    }
    char* path = g_file_get_path(file);
    char* name = g_file_get_parse_name(file);
    if (path && window->view && window->view->Open(Str(path))) {
        gtk_stack_set_visible_child_name(GTK_STACK(window->stack), "document");
        window->view->Focus();
    } else {
        TempStr status = fmt("Could not open %s", Str(path ? path : name));
        gtk_label_set_text(GTK_LABEL(window->status), CStrTemp(status));
        gtk_stack_set_visible_child_name(GTK_STACK(window->stack), "status");
    }
    gtk_window_set_title(GTK_WINDOW(window->window), name);
    g_free(name);
    g_free(path);
}

void LinuxWindowPresent(LinuxWindow* window) {
    if (window) {
        gtk_window_present(GTK_WINDOW(window->window));
    }
}
