/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "Settings.h"
#include "Commands.h"
#include "gui/DocumentView.h"

#include <gtk/gtk.h>

#include "linux/LinuxWindow.h"

struct LinuxWindow {
    GtkWidget* window = nullptr;
    GtkWidget* root = nullptr;
    GtkWidget* toolbar = nullptr;
    GtkWidget* stack = nullptr;
    GtkWidget* status = nullptr;
    GtkWidget* pageStatus = nullptr;
    GtkWidget* prevButton = nullptr;
    GtkWidget* nextButton = nullptr;
    GtkWidget* continuousButton = nullptr;
    DocumentView* view = nullptr;
    bool fullscreen = false;
};

static void UpdateControls(LinuxWindow* window);

static void FreeLinuxWindow(gpointer data) {
    auto* window = (LinuxWindow*)data;
    delete window->view;
    delete window;
}

static GtkWidget* NewCommandButton(LinuxWindow* window, const char* label, const char* tooltip, int commandId) {
    GtkWidget* button = gtk_button_new_with_label(label);
    gtk_widget_set_tooltip_text(button, tooltip);
    g_object_set_data(G_OBJECT(button), "sumatra-command", GINT_TO_POINTER(commandId));
    g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* sender, gpointer data) {
                         auto* target = (LinuxWindow*)data;
                         int command = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(sender), "sumatra-command"));
                         LinuxWindowDispatchCommand(target, command);
                     }),
                     window);
    return button;
}

static void OnOpenDialogResponse(GtkNativeDialog* dialog, int response, gpointer data) {
    auto* window = (LinuxWindow*)data;
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile* file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
        LinuxWindowOpenFile(window, file);
        g_object_unref(file);
    }
    g_object_unref(dialog);
}

static void ShowOpenDialog(LinuxWindow* window) {
    GtkFileChooserNative* dialog = gtk_file_chooser_native_new("Open Document", GTK_WINDOW(window->window),
                                                               GTK_FILE_CHOOSER_ACTION_OPEN, "Open", "Cancel");
    g_signal_connect(dialog, "response", G_CALLBACK(OnOpenDialogResponse), window);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog));
}

static void ToggleFullscreen(LinuxWindow* window) {
    window->fullscreen = !window->fullscreen;
    if (window->fullscreen) {
        gtk_window_fullscreen(GTK_WINDOW(window->window));
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(window->window));
    }
}

void LinuxWindowDispatchCommand(LinuxWindow* window, int commandId) {
    if (!window) {
        return;
    }
    DocumentView* view = window->view;
    switch (commandId) {
        case CmdOpenFile:
            ShowOpenDialog(window);
            break;
        case CmdGoToPrevPage:
            view->GoToPage(view->CurrentPageNo() - 1);
            break;
        case CmdGoToNextPage:
            view->GoToPage(view->CurrentPageNo() + 1);
            break;
        case CmdZoomFitPage:
            view->SetZoom(kZoomFitPage);
            break;
        case CmdZoomFitWidth:
            view->SetZoom(kZoomFitWidth);
            break;
        case CmdZoomActualSize:
            view->SetZoom(kZoomActualSize);
            break;
        case CmdRotateLeft:
            view->RotateBy(-90);
            break;
        case CmdRotateRight:
            view->RotateBy(90);
            break;
        case CmdToggleContinuousView:
            view->SetContinuous(!view->IsContinuous());
            break;
        case CmdToggleFullscreen:
            ToggleFullscreen(window);
            break;
        default:
            break;
    }
    UpdateControls(window);
}

static gboolean OnWindowKey(GtkEventControllerKey*, guint keyval, guint, GdkModifierType state, gpointer data) {
    auto* window = (LinuxWindow*)data;
    bool ctrl = (state & GDK_CONTROL_MASK) != 0;
    int command = 0;
    if (ctrl && keyval == GDK_KEY_o) {
        command = CmdOpenFile;
    } else if (ctrl && keyval == GDK_KEY_0) {
        command = CmdZoomFitPage;
    } else if (ctrl && keyval == GDK_KEY_1) {
        command = CmdZoomActualSize;
    } else if (ctrl && keyval == GDK_KEY_2) {
        command = CmdZoomFitWidth;
    } else if (!ctrl && keyval == GDK_KEY_bracketleft) {
        command = CmdRotateLeft;
    } else if (!ctrl && keyval == GDK_KEY_bracketright) {
        command = CmdRotateRight;
    } else if (!ctrl && (keyval == GDK_KEY_c || keyval == GDK_KEY_C)) {
        command = CmdToggleContinuousView;
    } else if (keyval == GDK_KEY_F11) {
        command = CmdToggleFullscreen;
    }
    if (!command) {
        return FALSE;
    }
    LinuxWindowDispatchCommand(window, command);
    return TRUE;
}

static void UpdateControls(LinuxWindow* window) {
    int pageNo = window->view ? window->view->CurrentPageNo() : 0;
    int pageCount = window->view ? window->view->PageCount() : 0;
    TempStr text = pageCount > 0 ? fmt("%d / %d", pageNo, pageCount) : str::DupTemp("No document");
    gtk_label_set_text(GTK_LABEL(window->pageStatus), CStrTemp(text));
    GtkWidget* child = gtk_widget_get_first_child(window->toolbar);
    child = child ? gtk_widget_get_next_sibling(child) : nullptr;
    while (child) {
        gtk_widget_set_sensitive(child, pageCount > 0);
        child = gtk_widget_get_next_sibling(child);
    }
    gtk_widget_set_sensitive(window->prevButton, pageNo > 1);
    gtk_widget_set_sensitive(window->nextButton, pageNo > 0 && pageNo < pageCount);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(window->continuousButton),
                                 window->view && window->view->IsContinuous());
}

static void OnContinuousToggled(GtkToggleButton* button, gpointer data) {
    auto* window = (LinuxWindow*)data;
    if (window->view) {
        window->view->SetContinuous(gtk_toggle_button_get_active(button));
    }
    UpdateControls(window);
}

LinuxWindow* LinuxWindowCreate(GtkApplication* app) {
    auto* result = new LinuxWindow();
    result->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(result->window), "SumatraPDF");
    gtk_window_set_default_size(GTK_WINDOW(result->window), 900, 700);

    GtkWidget* header = gtk_header_bar_new();
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header), TRUE);
    GtkWidget* fullscreen = NewCommandButton(result, "Fullscreen", "Toggle fullscreen (F11)", CmdToggleFullscreen);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), fullscreen);
    gtk_window_set_titlebar(GTK_WINDOW(result->window), header);

    result->root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    result->toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(result->toolbar, 8);
    gtk_widget_set_margin_end(result->toolbar, 8);
    gtk_widget_set_margin_top(result->toolbar, 6);
    gtk_widget_set_margin_bottom(result->toolbar, 6);

    GtkWidget* open = NewCommandButton(result, "Open", "Open document (Ctrl+O)", CmdOpenFile);
    result->prevButton = NewCommandButton(result, "Previous", "Previous page", CmdGoToPrevPage);
    result->nextButton = NewCommandButton(result, "Next", "Next page", CmdGoToNextPage);
    result->pageStatus = gtk_label_new("No document");
    gtk_widget_set_margin_start(result->pageStatus, 4);
    gtk_widget_set_margin_end(result->pageStatus, 4);
    gtk_box_append(GTK_BOX(result->toolbar), open);
    gtk_box_append(GTK_BOX(result->toolbar), result->prevButton);
    gtk_box_append(GTK_BOX(result->toolbar), result->nextButton);
    gtk_box_append(GTK_BOX(result->toolbar), result->pageStatus);
    gtk_box_append(GTK_BOX(result->toolbar), NewCommandButton(result, "Fit Page", "Fit page (Ctrl+0)", CmdZoomFitPage));
    gtk_box_append(GTK_BOX(result->toolbar),
                   NewCommandButton(result, "Fit Width", "Fit width (Ctrl+2)", CmdZoomFitWidth));
    gtk_box_append(GTK_BOX(result->toolbar),
                   NewCommandButton(result, "100%", "Actual size (Ctrl+1)", CmdZoomActualSize));
    gtk_box_append(GTK_BOX(result->toolbar), NewCommandButton(result, "Rotate Left", "Rotate left ([)", CmdRotateLeft));
    gtk_box_append(GTK_BOX(result->toolbar),
                   NewCommandButton(result, "Rotate Right", "Rotate right (])", CmdRotateRight));
    result->continuousButton = gtk_toggle_button_new_with_label("Continuous");
    gtk_widget_set_tooltip_text(result->continuousButton, "Toggle continuous view (C)");
    g_signal_connect(result->continuousButton, "toggled", G_CALLBACK(OnContinuousToggled), result);
    gtk_box_append(GTK_BOX(result->toolbar), result->continuousButton);
    gtk_box_append(GTK_BOX(result->root), result->toolbar);

    result->stack = gtk_stack_new();
    result->status = gtk_label_new("Open a document to start reading.");
    gtk_widget_set_hexpand(result->status, TRUE);
    gtk_widget_set_vexpand(result->status, TRUE);
    gtk_widget_set_halign(result->status, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(result->status, GTK_ALIGN_CENTER);
    gtk_stack_add_named(GTK_STACK(result->stack), result->status, "status");

    result->view = DocumentView::Create();
    if (result->view) {
        result->view->onStateChanged = MkFunc0(UpdateControls, result);
        gtk_stack_add_named(GTK_STACK(result->stack), GTK_WIDGET(result->view->NativeWidget()), "document");
    }
    gtk_stack_set_visible_child_name(GTK_STACK(result->stack), "status");
    gtk_box_append(GTK_BOX(result->root), result->stack);
    gtk_window_set_child(GTK_WINDOW(result->window), result->root);

    GtkEventController* keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
    g_signal_connect(keys, "key-pressed", G_CALLBACK(OnWindowKey), result);
    gtk_widget_add_controller(result->window, keys);

    g_object_set_data_full(G_OBJECT(result->window), "sumatra-linux-window", result, FreeLinuxWindow);
    UpdateControls(result);
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
    char* baseName = g_file_get_basename(file);
    gtk_window_set_title(GTK_WINDOW(window->window), baseName ? baseName : name);
    g_free(baseName);
    UpdateControls(window);
    g_free(name);
    g_free(path);
}

void LinuxWindowPresent(LinuxWindow* window) {
    if (window) {
        gtk_window_present(GTK_WINDOW(window->window));
    }
}
