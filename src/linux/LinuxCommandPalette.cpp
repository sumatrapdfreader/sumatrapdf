/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "gui/CommandPaletteModel.h"

#include <gtk/gtk.h>

#include "linux/LinuxCommandPalette.h"

struct LinuxCommandPalette {
    GtkWidget* window = nullptr;
    GtkWidget* entry = nullptr;
    GtkWidget* list = nullptr;
    CommandPaletteModel model;
    Func1<int> onCommand;
};

static GtkWidget* gPaletteWindow = nullptr;

static void FreePalette(gpointer data) {
    delete (LinuxCommandPalette*)data;
}

static void ExecuteRow(LinuxCommandPalette* palette, GtkListBoxRow* row) {
    if (!palette || !row) {
        return;
    }
    int commandId = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "sumatra-command"));
    Func1<int> onCommand = palette->onCommand;
    gtk_window_destroy(GTK_WINDOW(palette->window));
    onCommand.Call(commandId);
}

static void RebuildList(LinuxCommandPalette* palette) {
    GtkWidget* child = gtk_widget_get_first_child(palette->list);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(palette->list), child);
        child = next;
    }
    for (int i = 0; i < palette->model.Count(); i++) {
        GtkWidget* label = gtk_label_new(CStrTemp(palette->model.ItemText(i)));
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_widget_set_margin_start(label, 10);
        gtk_widget_set_margin_end(label, 10);
        gtk_widget_set_margin_top(label, 6);
        gtk_widget_set_margin_bottom(label, 6);
        GtkWidget* row = gtk_list_box_row_new();
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data(G_OBJECT(row), "sumatra-command", GINT_TO_POINTER(palette->model.ItemCommandId(i)));
        gtk_list_box_append(GTK_LIST_BOX(palette->list), row);
    }
    GtkListBoxRow* first = gtk_list_box_get_row_at_index(GTK_LIST_BOX(palette->list), 0);
    if (first) {
        gtk_list_box_select_row(GTK_LIST_BOX(palette->list), first);
    }
}

static void OnQueryChanged(GtkEditable* entry, gpointer data) {
    auto* palette = (LinuxCommandPalette*)data;
    palette->model.Filter(Str(gtk_editable_get_text(entry)));
    RebuildList(palette);
}

static void OnRowActivated(GtkListBox*, GtkListBoxRow* row, gpointer data) {
    ExecuteRow((LinuxCommandPalette*)data, row);
}

static void ExecuteSelection(LinuxCommandPalette* palette) {
    GtkListBoxRow* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(palette->list));
    ExecuteRow(palette, row);
}

static gboolean OnPaletteKey(GtkEventControllerKey*, guint keyval, guint, GdkModifierType, gpointer data) {
    auto* palette = (LinuxCommandPalette*)data;
    if (keyval == GDK_KEY_Escape) {
        gtk_window_destroy(GTK_WINDOW(palette->window));
        return TRUE;
    }
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        ExecuteSelection(palette);
        return TRUE;
    }
    int direction = keyval == GDK_KEY_Down ? 1 : keyval == GDK_KEY_Up ? -1 : 0;
    if (!direction) {
        return FALSE;
    }
    GtkListBoxRow* selected = gtk_list_box_get_selected_row(GTK_LIST_BOX(palette->list));
    int index = selected ? gtk_list_box_row_get_index(selected) : 0;
    int count = palette->model.Count();
    if (count > 0) {
        index = limitValue(index + direction, 0, count - 1);
        gtk_list_box_select_row(GTK_LIST_BOX(palette->list),
                                gtk_list_box_get_row_at_index(GTK_LIST_BOX(palette->list), index));
    }
    return TRUE;
}

void ShowLinuxCommandPalette(GtkWindow* parent, const int* commandIds, int count, const Func1<int>& onCommand) {
    if (gPaletteWindow) {
        gtk_window_present(GTK_WINDOW(gPaletteWindow));
        return;
    }

    auto* palette = new LinuxCommandPalette();
    palette->model.SetCommands(commandIds, count);
    palette->onCommand = onCommand;
    palette->window = gtk_window_new();
    gPaletteWindow = palette->window;
    g_object_add_weak_pointer(G_OBJECT(palette->window), (gpointer*)&gPaletteWindow);
    g_object_set_data_full(G_OBJECT(palette->window), "sumatra-command-palette", palette, FreePalette);
    gtk_window_set_title(GTK_WINDOW(palette->window), "Command Palette");
    gtk_window_set_transient_for(GTK_WINDOW(palette->window), parent);
    gtk_window_set_modal(GTK_WINDOW(palette->window), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(palette->window), 560, 440);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(root, 12);
    gtk_widget_set_margin_end(root, 12);
    gtk_widget_set_margin_top(root, 12);
    gtk_widget_set_margin_bottom(root, 12);
    palette->entry = gtk_search_entry_new();
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(palette->entry), "Type a command");
    g_signal_connect(palette->entry, "changed", G_CALLBACK(OnQueryChanged), palette);
    gtk_box_append(GTK_BOX(root), palette->entry);

    palette->list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(palette->list), GTK_SELECTION_SINGLE);
    g_signal_connect(palette->list, "row-activated", G_CALLBACK(OnRowActivated), palette);
    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), palette->list);
    gtk_box_append(GTK_BOX(root), scroll);
    gtk_window_set_child(GTK_WINDOW(palette->window), root);

    GtkEventController* keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
    g_signal_connect(keys, "key-pressed", G_CALLBACK(OnPaletteKey), palette);
    gtk_widget_add_controller(palette->window, keys);

    RebuildList(palette);
    gtk_window_present(GTK_WINDOW(palette->window));
    gtk_widget_grab_focus(palette->entry);
}
