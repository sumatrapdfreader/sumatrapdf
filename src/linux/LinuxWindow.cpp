/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "Settings.h"
#include "Commands.h"
#include "gui/DocumentView.h"

#include <gtk/gtk.h>

#include "linux/LinuxTab.h"
#include "linux/LinuxWindow.h"

struct LinuxWindow {
    GtkWidget* window = nullptr;
    GtkWidget* root = nullptr;
    GtkWidget* toolbar = nullptr;
    GtkWidget* content = nullptr;
    GtkWidget* notebook = nullptr;
    GtkWidget* emptyStatus = nullptr;
    GtkWidget* pageStatus = nullptr;
    GtkWidget* prevButton = nullptr;
    GtkWidget* nextButton = nullptr;
    GtkWidget* closeButton = nullptr;
    GtkWidget* reopenButton = nullptr;
    GtkWidget* continuousButton = nullptr;
    Vec<LinuxTab*> tabs;
    StrVec closedPaths;
    bool fullscreen = false;
};

static void UpdateControls(LinuxWindow* window);

static LinuxTab* ActiveTab(LinuxWindow* window) {
    int index = gtk_notebook_get_current_page(GTK_NOTEBOOK(window->notebook));
    GtkWidget* child = index >= 0 ? gtk_notebook_get_nth_page(GTK_NOTEBOOK(window->notebook), index) : nullptr;
    return child ? (LinuxTab*)g_object_get_data(G_OBJECT(child), "sumatra-linux-tab") : nullptr;
}

static DocumentView* ActiveView(LinuxWindow* window) {
    return LinuxTabView(ActiveTab(window));
}

static int FindTabIndex(LinuxWindow* window, LinuxTab* tab) {
    for (int i = 0; i < len(window->tabs); i++) {
        if (window->tabs[i] == tab) {
            return i;
        }
    }
    return -1;
}

static void FreeLinuxWindow(gpointer data) {
    auto* window = (LinuxWindow*)data;
    for (LinuxTab* tab : window->tabs) {
        LinuxTabDestroy(tab);
    }
    window->tabs.Reset();
    window->closedPaths.Reset();
    delete window;
}

static void OnCommandClicked(GtkButton* sender, gpointer data) {
    auto* window = (LinuxWindow*)data;
    int command = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(sender), "sumatra-command"));
    LinuxWindowDispatchCommand(window, command);
}

static GtkWidget* NewCommandButton(LinuxWindow* window, const char* label, const char* tooltip, int commandId) {
    GtkWidget* button = gtk_button_new_with_label(label);
    gtk_widget_set_tooltip_text(button, tooltip);
    g_object_set_data(G_OBJECT(button), "sumatra-command", GINT_TO_POINTER(commandId));
    g_signal_connect(button, "clicked", G_CALLBACK(OnCommandClicked), window);
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

static void SelectRelativeTab(LinuxWindow* window, int direction) {
    int count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(window->notebook));
    if (count < 2) {
        return;
    }
    int current = gtk_notebook_get_current_page(GTK_NOTEBOOK(window->notebook));
    gtk_notebook_set_current_page(GTK_NOTEBOOK(window->notebook), (current + direction + count) % count);
}

static void RememberClosedPath(LinuxWindow* window, Str path) {
    if (len(path) == 0) {
        return;
    }
    while (len(window->closedPaths) >= 10) {
        window->closedPaths.RemoveAt(0);
    }
    window->closedPaths.Append(path);
}

static void CloseTab(LinuxWindow* window, LinuxTab* tab) {
    int tabIndex = FindTabIndex(window, tab);
    if (tabIndex < 0) {
        return;
    }
    RememberClosedPath(window, LinuxTabPath(tab));
    int pageIndex = gtk_notebook_page_num(GTK_NOTEBOOK(window->notebook), LinuxTabWidget(tab));
    gtk_notebook_remove_page(GTK_NOTEBOOK(window->notebook), pageIndex);
    window->tabs.RemoveAt(tabIndex);
    LinuxTabDestroy(tab);
    UpdateControls(window);
}

static void ReopenClosedTab(LinuxWindow* window) {
    if (len(window->closedPaths) == 0) {
        return;
    }
    TempStr path = str::DupTemp(window->closedPaths[len(window->closedPaths) - 1]);
    window->closedPaths.RemoveAt(len(window->closedPaths) - 1);
    GFile* file = g_file_new_for_path(CStrTemp(path));
    LinuxWindowOpenFile(window, file);
    g_object_unref(file);
}

void LinuxWindowDispatchCommand(LinuxWindow* window, int commandId) {
    if (!window) {
        return;
    }
    switch (commandId) {
        case CmdOpenFile:
            ShowOpenDialog(window);
            return;
        case CmdCloseCurrentDocument:
            CloseTab(window, ActiveTab(window));
            return;
        case CmdReopenLastClosedFile:
            ReopenClosedTab(window);
            return;
        case CmdNextTab:
            SelectRelativeTab(window, 1);
            return;
        case CmdPrevTab:
            SelectRelativeTab(window, -1);
            return;
        case CmdToggleFullscreen:
            ToggleFullscreen(window);
            return;
    }

    DocumentView* view = ActiveView(window);
    if (!view) {
        return;
    }
    switch (commandId) {
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
        default:
            break;
    }
    UpdateControls(window);
}

static gboolean OnWindowKey(GtkEventControllerKey*, guint keyval, guint, GdkModifierType state, gpointer data) {
    auto* window = (LinuxWindow*)data;
    bool ctrl = (state & GDK_CONTROL_MASK) != 0;
    bool shift = (state & GDK_SHIFT_MASK) != 0;
    int command = 0;
    if (ctrl && shift && (keyval == GDK_KEY_t || keyval == GDK_KEY_T)) {
        command = CmdReopenLastClosedFile;
    } else if (ctrl && (keyval == GDK_KEY_w || keyval == GDK_KEY_W)) {
        command = CmdCloseCurrentDocument;
    } else if (ctrl && keyval == GDK_KEY_Tab) {
        command = shift ? CmdPrevTab : CmdNextTab;
    } else if (ctrl && keyval == GDK_KEY_Page_Down) {
        command = CmdNextTab;
    } else if (ctrl && keyval == GDK_KEY_Page_Up) {
        command = CmdPrevTab;
    } else if (ctrl && (keyval == GDK_KEY_o || keyval == GDK_KEY_O)) {
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
    LinuxTab* tab = ActiveTab(window);
    DocumentView* view = LinuxTabView(tab);
    int pageNo = view ? view->CurrentPageNo() : 0;
    int pageCount = view ? view->PageCount() : 0;
    TempStr text = pageCount > 0 ? fmt("%d / %d", pageNo, pageCount) : str::DupTemp("No document");
    gtk_label_set_text(GTK_LABEL(window->pageStatus), CStrTemp(text));

    GtkWidget* child = gtk_widget_get_first_child(window->toolbar);
    child = child ? gtk_widget_get_next_sibling(child) : nullptr;
    while (child) {
        gtk_widget_set_sensitive(child, pageCount > 0);
        child = gtk_widget_get_next_sibling(child);
    }
    gtk_widget_set_sensitive(window->reopenButton, len(window->closedPaths) > 0);
    gtk_widget_set_sensitive(window->closeButton, tab != nullptr);
    gtk_widget_set_sensitive(window->prevButton, pageNo > 1);
    gtk_widget_set_sensitive(window->nextButton, pageNo > 0 && pageNo < pageCount);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(window->continuousButton), view && view->IsContinuous());

    int tabCount = gtk_notebook_get_n_pages(GTK_NOTEBOOK(window->notebook));
    gtk_stack_set_visible_child_name(GTK_STACK(window->content), tabCount > 0 ? "notebook" : "empty");
    Str title = LinuxTabTitle(tab);
    gtk_window_set_title(GTK_WINDOW(window->window), len(title) == 0 ? "SumatraPDF" : CStrTemp(title));
}

static void OnContinuousToggled(GtkToggleButton* button, gpointer data) {
    auto* window = (LinuxWindow*)data;
    DocumentView* view = ActiveView(window);
    if (view) {
        view->SetContinuous(gtk_toggle_button_get_active(button));
    }
    UpdateControls(window);
}

static void OnSwitchPage(GtkNotebook*, GtkWidget*, guint, gpointer data) {
    auto* window = (LinuxWindow*)data;
    UpdateControls(window);
    DocumentView* view = ActiveView(window);
    if (view) {
        view->Focus();
    }
}

static void OnCloseTabClicked(GtkButton* button, gpointer data) {
    auto* window = (LinuxWindow*)data;
    GtkWidget* child = (GtkWidget*)g_object_get_data(G_OBJECT(button), "sumatra-tab-child");
    auto* tab = (LinuxTab*)g_object_get_data(G_OBJECT(child), "sumatra-linux-tab");
    CloseTab(window, tab);
}

static GtkWidget* NewTabLabel(LinuxWindow* window, LinuxTab* tab) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* label = gtk_label_new(CStrTemp(LinuxTabTitle(tab)));
    GtkWidget* close = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_button_set_has_frame(GTK_BUTTON(close), FALSE);
    gtk_widget_set_focusable(close, FALSE);
    gtk_widget_set_tooltip_text(close, "Close tab");
    g_object_set_data(G_OBJECT(close), "sumatra-tab-child", LinuxTabWidget(tab));
    g_signal_connect(close, "clicked", G_CALLBACK(OnCloseTabClicked), window);
    gtk_box_append(GTK_BOX(box), label);
    gtk_box_append(GTK_BOX(box), close);
    return box;
}

LinuxWindow* LinuxWindowCreate(GtkApplication* app) {
    auto* result = new LinuxWindow();
    result->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(result->window), "SumatraPDF");
    gtk_window_set_default_size(GTK_WINDOW(result->window), 1000, 720);

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
    result->reopenButton =
        NewCommandButton(result, "Reopen", "Reopen closed tab (Ctrl+Shift+T)", CmdReopenLastClosedFile);
    result->closeButton = NewCommandButton(result, "Close", "Close current tab (Ctrl+W)", CmdCloseCurrentDocument);
    result->prevButton = NewCommandButton(result, "Previous", "Previous page", CmdGoToPrevPage);
    result->nextButton = NewCommandButton(result, "Next", "Next page", CmdGoToNextPage);
    result->pageStatus = gtk_label_new("No document");
    gtk_widget_set_margin_start(result->pageStatus, 4);
    gtk_widget_set_margin_end(result->pageStatus, 4);
    gtk_box_append(GTK_BOX(result->toolbar), open);
    gtk_box_append(GTK_BOX(result->toolbar), result->reopenButton);
    gtk_box_append(GTK_BOX(result->toolbar), result->closeButton);
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

    result->content = gtk_stack_new();
    result->emptyStatus = gtk_label_new("Open a document to start reading.");
    gtk_widget_set_hexpand(result->emptyStatus, TRUE);
    gtk_widget_set_vexpand(result->emptyStatus, TRUE);
    gtk_widget_set_halign(result->emptyStatus, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(result->emptyStatus, GTK_ALIGN_CENTER);
    gtk_stack_add_named(GTK_STACK(result->content), result->emptyStatus, "empty");
    result->notebook = gtk_notebook_new();
    gtk_widget_set_hexpand(result->notebook, TRUE);
    gtk_widget_set_vexpand(result->notebook, TRUE);
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(result->notebook), TRUE);
    g_signal_connect(result->notebook, "switch-page", G_CALLBACK(OnSwitchPage), result);
    gtk_stack_add_named(GTK_STACK(result->content), result->notebook, "notebook");
    gtk_stack_set_visible_child_name(GTK_STACK(result->content), "empty");
    gtk_box_append(GTK_BOX(result->root), result->content);
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
    char* baseName = g_file_get_basename(file);
    Str title(baseName ? baseName : "Document");
    LinuxTab* tab = LinuxTabCreate(title, MkFunc0(UpdateControls, window));
    g_free(baseName);
    if (!tab) {
        return;
    }

    GtkWidget* child = LinuxTabWidget(tab);
    GtkWidget* label = NewTabLabel(window, tab);
    int index = gtk_notebook_append_page(GTK_NOTEBOOK(window->notebook), child, label);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(window->notebook), child, TRUE);
    window->tabs.Append(tab);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(window->notebook), index);
    LinuxTabOpenFile(tab, file);
    UpdateControls(window);
}

void LinuxWindowPresent(LinuxWindow* window) {
    if (window) {
        gtk_window_present(GTK_WINDOW(window->window));
    }
}
