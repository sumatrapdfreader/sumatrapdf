/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "gui/UIModels.h"
#include "EngineBase.h"
#include "gui/PlatformWindow.h"

#include <gtk/gtk.h>

#include "gui/PasswordDialog.h"

struct GtkPasswordDialogState {
    GMainLoop* loop = nullptr;
    GtkWidget* window = nullptr;
    GtkWidget* entry = nullptr;
    GtkWidget* show = nullptr;
    GtkWidget* remember = nullptr;
    PasswordDialogResult* result = nullptr;
    bool done = false;
};

static void FinishPasswordDialog(GtkPasswordDialogState* state, bool accepted) {
    if (state->done) {
        return;
    }
    state->done = true;
    state->result->accepted = accepted;
    state->result->showPassword = gtk_check_button_get_active(GTK_CHECK_BUTTON(state->show));
    if (accepted) {
        const char* password = gtk_editable_get_text(GTK_EDITABLE(state->entry));
        state->result->password = str::Dup(Str((char*)password));
        if (state->remember) {
            state->result->rememberPassword = gtk_check_button_get_active(GTK_CHECK_BUTTON(state->remember));
        }
    }
    g_main_loop_quit(state->loop);
}

static void OnPasswordAccept(GtkButton*, gpointer data) {
    FinishPasswordDialog((GtkPasswordDialogState*)data, true);
}

static void OnPasswordCancel(GtkButton*, gpointer data) {
    FinishPasswordDialog((GtkPasswordDialogState*)data, false);
}

static void OnPasswordEntryActivate(GtkEntry*, gpointer data) {
    FinishPasswordDialog((GtkPasswordDialogState*)data, true);
}

static void OnPasswordVisibilityChanged(GtkCheckButton* button, gpointer data) {
    auto* state = (GtkPasswordDialogState*)data;
    gtk_entry_set_visibility(GTK_ENTRY(state->entry), gtk_check_button_get_active(button));
}

static gboolean OnPasswordClose(GtkWindow*, gpointer data) {
    FinishPasswordDialog((GtkPasswordDialogState*)data, false);
    return TRUE;
}

static gboolean OnPasswordKey(GtkEventControllerKey*, guint keyval, guint, GdkModifierType, gpointer data) {
    if (keyval == GDK_KEY_Escape) {
        FinishPasswordDialog((GtkPasswordDialogState*)data, false);
        return TRUE;
    }
    return FALSE;
}

bool ShowPasswordDialog(const PasswordDialogArgs& args, PasswordDialogResult* result) {
    if (!result) {
        return false;
    }
    *result = {};
    result->showPassword = args.showPassword;
    result->rememberPassword = args.rememberPassword;

    GtkPasswordDialogState state;
    state.result = result;
    state.loop = g_main_loop_new(nullptr, FALSE);
    state.window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(state.window), "Enter password");
    gtk_window_set_modal(GTK_WINDOW(state.window), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(state.window), FALSE);
    if (args.parent) {
        gtk_window_set_transient_for(GTK_WINDOW(state.window), GTK_WINDOW(args.parent));
    }

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(root, 18);
    gtk_widget_set_margin_end(root, 18);
    gtk_widget_set_margin_top(root, 18);
    gtk_widget_set_margin_bottom(root, 18);

    TempStr prompt = fmt("Enter password for %s", args.fileName);
    GtkWidget* label = gtk_label_new(CStrTemp(prompt));
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 60);
    gtk_box_append(GTK_BOX(root), label);

    state.entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(state.entry), args.showPassword);
    gtk_entry_set_input_purpose(GTK_ENTRY(state.entry), GTK_INPUT_PURPOSE_PASSWORD);
    gtk_entry_set_activates_default(GTK_ENTRY(state.entry), TRUE);
    g_signal_connect(state.entry, "activate", G_CALLBACK(OnPasswordEntryActivate), &state);
    gtk_box_append(GTK_BOX(root), state.entry);

    state.show = gtk_check_button_new_with_mnemonic("_Show password");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(state.show), args.showPassword);
    g_signal_connect(state.show, "toggled", G_CALLBACK(OnPasswordVisibilityChanged), &state);
    gtk_box_append(GTK_BOX(root), state.show);

    if (args.canRemember) {
        state.remember = gtk_check_button_new_with_mnemonic("_Remember the password for this document");
        gtk_check_button_set_active(GTK_CHECK_BUTTON(state.remember), args.rememberPassword);
        gtk_box_append(GTK_BOX(root), state.remember);
    }

    GtkWidget* buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(buttons, GTK_ALIGN_END);
    GtkWidget* cancel = gtk_button_new_with_label("Cancel");
    GtkWidget* accept = gtk_button_new_with_label("OK");
    g_signal_connect(cancel, "clicked", G_CALLBACK(OnPasswordCancel), &state);
    g_signal_connect(accept, "clicked", G_CALLBACK(OnPasswordAccept), &state);
    gtk_box_append(GTK_BOX(buttons), cancel);
    gtk_box_append(GTK_BOX(buttons), accept);
    gtk_box_append(GTK_BOX(root), buttons);

    GtkEventController* keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
    g_signal_connect(keys, "key-pressed", G_CALLBACK(OnPasswordKey), &state);
    gtk_widget_add_controller(state.window, keys);
    g_signal_connect(state.window, "close-request", G_CALLBACK(OnPasswordClose), &state);
    gtk_window_set_default_widget(GTK_WINDOW(state.window), accept);
    gtk_window_set_child(GTK_WINDOW(state.window), root);
    gtk_window_present(GTK_WINDOW(state.window));
    gtk_widget_grab_focus(state.entry);

    g_main_loop_run(state.loop);
    gtk_window_destroy(GTK_WINDOW(state.window));
    g_main_loop_unref(state.loop);
    return result->accepted;
}
