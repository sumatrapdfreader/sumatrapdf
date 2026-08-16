/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "gui/DocumentView.h"

#include <gtk/gtk.h>

#include "linux/LinuxTab.h"

struct LinuxTab {
    GtkWidget* stack = nullptr;
    GtkWidget* status = nullptr;
    DocumentView* view = nullptr;
    Str title;
    Str path;
};

LinuxTab* LinuxTabCreate(Str title, const Func0& onStateChanged, const Func1<Str>& onOpenUrl,
                         const Func1<Str>& onOpenFile, const Func1<Str>& onCopyText) {
    auto* tab = new LinuxTab();
    tab->title = str::Dup(title);
    tab->stack = gtk_stack_new();
    tab->status = gtk_label_new("Opening document...");
    gtk_widget_set_hexpand(tab->status, TRUE);
    gtk_widget_set_vexpand(tab->status, TRUE);
    gtk_widget_set_halign(tab->status, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(tab->status, GTK_ALIGN_CENTER);
    gtk_stack_add_named(GTK_STACK(tab->stack), tab->status, "status");

    tab->view = DocumentView::Create();
    if (tab->view) {
        tab->view->onStateChanged = onStateChanged;
        tab->view->onOpenUrl = onOpenUrl;
        tab->view->onOpenFile = onOpenFile;
        tab->view->onCopyText = onCopyText;
        gtk_stack_add_named(GTK_STACK(tab->stack), GTK_WIDGET(tab->view->NativeWidget()), "document");
    }
    gtk_stack_set_visible_child_name(GTK_STACK(tab->stack), "status");
    g_object_set_data(G_OBJECT(tab->stack), "sumatra-linux-tab", tab);
    return tab;
}

void LinuxTabDestroy(LinuxTab* tab) {
    if (!tab) {
        return;
    }
    delete tab->view;
    str::Free(tab->title);
    str::Free(tab->path);
    delete tab;
}

bool LinuxTabOpenFile(LinuxTab* tab, GFile* file) {
    if (!tab || !file) {
        return false;
    }
    char* path = g_file_get_path(file);
    char* displayName = g_file_get_parse_name(file);
    bool ok = path && tab->view && tab->view->Open(Str(path));
    if (ok) {
        str::Free(tab->path);
        tab->path = str::Dup(Str(path));
        gtk_stack_set_visible_child_name(GTK_STACK(tab->stack), "document");
        tab->view->Focus();
    } else {
        TempStr text = fmt("Could not open %s", Str(path ? path : displayName));
        gtk_label_set_text(GTK_LABEL(tab->status), CStrTemp(text));
        gtk_stack_set_visible_child_name(GTK_STACK(tab->stack), "status");
    }
    g_free(displayName);
    g_free(path);
    return ok;
}

GtkWidget* LinuxTabWidget(LinuxTab* tab) {
    return tab ? tab->stack : nullptr;
}

DocumentView* LinuxTabView(LinuxTab* tab) {
    return tab ? tab->view : nullptr;
}

Str LinuxTabTitle(LinuxTab* tab) {
    return tab ? tab->title : Str{};
}

Str LinuxTabPath(LinuxTab* tab) {
    return tab ? tab->path : Str{};
}
