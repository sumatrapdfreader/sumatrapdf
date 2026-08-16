/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"

#include "Settings.h"
#include "Commands.h"
#include "GlobalPrefs.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#include "KeyboardHelp.h"
#include "gui/DocumentView.h"

#include <gtk/gtk.h>

#include "linux/LinuxTab.h"
#include "linux/LinuxCommandPalette.h"
#include "linux/LinuxDesktop.h"
#include "linux/LinuxPrint.h"
#include "linux/LinuxPrefs.h"
#include "linux/LinuxWindow.h"

struct LinuxWindow {
    GtkWidget* window = nullptr;
    GtkWidget* header = nullptr;
    GtkWidget* root = nullptr;
    GtkWidget* toolbar = nullptr;
    GtkWidget* findBar = nullptr;
    GtkWidget* findEntry = nullptr;
    GtkWidget* findStatus = nullptr;
    GtkWidget* content = nullptr;
    GtkWidget* documentPane = nullptr;
    GtkWidget* sidebar = nullptr;
    GtkWidget* tocScroll = nullptr;
    GtkWidget* tocList = nullptr;
    GtkWidget* favoritesScroll = nullptr;
    GtkWidget* favoritesList = nullptr;
    GtkWidget* notebook = nullptr;
    GtkWidget* emptyStatus = nullptr;
    GtkWidget* pageStatus = nullptr;
    GtkWidget* prevButton = nullptr;
    GtkWidget* nextButton = nullptr;
    GtkWidget* closeButton = nullptr;
    GtkWidget* reopenButton = nullptr;
    GtkWidget* continuousButton = nullptr;
    GtkWidget* menuButton = nullptr;
    Vec<LinuxTab*> tabs;
    StrVec closedPaths;
    bool fullscreen = false;
    bool presentation = false;
    bool tocVisible = false;
    bool favoritesVisible = false;
    bool previousTocVisible = false;
    bool previousFavoritesVisible = false;
    bool wasFullscreen = false;
    bool previousContinuous = false;
    bool suppressStateSave = false;
    float previousZoom = kZoomFitWidth;
    LinuxTab* presentationTab = nullptr;
};

static void UpdateControls(LinuxWindow* window);
static void UpdateToc(LinuxWindow* window);
static void UpdateFavorites(LinuxWindow* window);
static void UpdateSidebar(LinuxWindow* window);

static LinuxTab* ActiveTab(LinuxWindow* window) {
    int index = gtk_notebook_get_current_page(GTK_NOTEBOOK(window->notebook));
    GtkWidget* child = index >= 0 ? gtk_notebook_get_nth_page(GTK_NOTEBOOK(window->notebook), index) : nullptr;
    return child ? (LinuxTab*)g_object_get_data(G_OBJECT(child), "sumatra-linux-tab") : nullptr;
}

static DocumentView* ActiveView(LinuxWindow* window) {
    return LinuxTabView(ActiveTab(window));
}

static void OnDocumentStateChanged(LinuxWindow* window) {
    LinuxTab* tab = ActiveTab(window);
    if (tab && !window->suppressStateSave) {
        LinuxPrefsSaveView(LinuxTabView(tab), LinuxTabPath(tab));
    }
    UpdateControls(window);
}

static void OnDocumentReloaded(LinuxWindow* window) {
    UpdateToc(window);
    UpdateControls(window);
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

void LinuxWindowSaveSession(LinuxWindow* window) {
    if (!window) {
        return;
    }
    Vec<DocumentView*> views;
    StrVec paths;
    LinuxTab* selectedTab = ActiveTab(window);
    int activeTab = 0;
    int pageCount = gtk_notebook_get_n_pages(GTK_NOTEBOOK(window->notebook));
    for (int i = 0; i < pageCount; i++) {
        GtkWidget* child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(window->notebook), i);
        auto* tab = (LinuxTab*)g_object_get_data(G_OBJECT(child), "sumatra-linux-tab");
        DocumentView* view = LinuxTabView(tab);
        Str path = LinuxTabPath(tab);
        if (!view || view->PageCount() < 1 || !path) {
            continue;
        }
        if (tab == selectedTab) {
            activeTab = len(views);
        }
        views.Append(view);
        paths.Append(path);
    }
    LinuxPrefsSaveSession(views, paths, activeTab);
}

static gboolean OnWindowCloseRequest(GtkWindow*, gpointer data) {
    LinuxWindowSaveSession((LinuxWindow*)data);
    return FALSE;
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

static void OpenLinkedUrl(LinuxWindow*, Str url) {
    if (!IsExternalUrl(url)) {
        return;
    }
    GError* error = nullptr;
    if (!g_app_info_launch_default_for_uri(CStrTemp(url), nullptr, &error)) {
        g_warning("Could not open link: %s", error ? error->message : "unknown error");
        g_clear_error(&error);
    }
}

static void OpenLinkedFile(LinuxWindow* window, Str linkPath) {
    if (!linkPath) {
        return;
    }
    TempStr fullPath = linkPath;
    if (!path::IsAbsolute(fullPath)) {
        LinuxTab* source = ActiveTab(window);
        Str sourcePath = LinuxTabPath(source);
        fullPath = path::NormalizeTemp(path::JoinTemp(path::GetDirTemp(sourcePath), fullPath));
    }
    GFile* file = g_file_new_for_path(CStrTemp(fullPath));
    LinuxWindowOpenFile(window, file);
    g_object_unref(file);
}

static void CopyText(LinuxWindow* window, Str text) {
    LinuxClipboardSetText(window->window, text);
}

static void OnTocRowActivated(GtkListBox*, GtkListBoxRow* row, gpointer data) {
    auto* window = (LinuxWindow*)data;
    int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "sumatra-toc-index"));
    LinuxWindowGoToTocItem(window, index);
}

static void OnFavoriteRowActivated(GtkListBox*, GtkListBoxRow* row, gpointer data) {
    int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "sumatra-favorite-index"));
    LinuxWindowGoToFavorite((LinuxWindow*)data, index);
}

static void UpdateSidebar(LinuxWindow* window) {
    DocumentView* view = ActiveView(window);
    bool showToc = window->tocVisible && view && view->TocItemCount() > 0;
    bool showFavorites = window->favoritesVisible;
    if (showFavorites) {
        gtk_stack_set_visible_child_name(GTK_STACK(window->sidebar), "favorites");
    } else if (showToc) {
        gtk_stack_set_visible_child_name(GTK_STACK(window->sidebar), "toc");
    }
    gtk_widget_set_visible(window->sidebar, !window->presentation && (showToc || showFavorites));
}

static void UpdateToc(LinuxWindow* window) {
    GtkWidget* child = gtk_widget_get_first_child(window->tocList);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(window->tocList), child);
        child = next;
    }

    DocumentView* view = ActiveView(window);
    int count = view ? view->TocItemCount() : 0;
    for (int i = 0; i < count; i++) {
        Str title = view->TocItemTitle(i);
        GtkWidget* label = gtk_label_new(CStrTemp(title));
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_widget_set_margin_start(label, 8 + view->TocItemDepth(i) * 16);
        gtk_widget_set_margin_end(label, 8);
        gtk_widget_set_margin_top(label, 4);
        gtk_widget_set_margin_bottom(label, 4);
        gtk_widget_set_tooltip_text(label, CStrTemp(title));
        GtkWidget* row = gtk_list_box_row_new();
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data(G_OBJECT(row), "sumatra-toc-index", GINT_TO_POINTER(i));
        gtk_list_box_append(GTK_LIST_BOX(window->tocList), row);
    }
    UpdateSidebar(window);
}

static void ToggleToc(LinuxWindow* window) {
    window->tocVisible = !window->tocVisible;
    if (window->tocVisible) {
        window->favoritesVisible = false;
    }
    if (gGlobalPrefs) {
        gGlobalPrefs->showToc = window->tocVisible;
        gGlobalPrefs->showFavorites = window->favoritesVisible;
    }
    UpdateToc(window);
}

static void UpdateFavorites(LinuxWindow* window) {
    GtkWidget* child = gtk_widget_get_first_child(window->favoritesList);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(window->favoritesList), child);
        child = next;
    }

    int count = LinuxPrefsFavoriteCount();
    for (int i = 0; i < count; i++) {
        Str filePath = LinuxPrefsFavoritePath(i);
        Str favoriteLabel = LinuxPrefsFavoriteLabel(i);
        int pageNo = LinuxPrefsFavoritePageNo(i);
        TempStr page = favoriteLabel ? str::DupTemp(favoriteLabel) : fmt("Page %d", pageNo);
        TempStr text = fmt("%s — %s", path::GetBaseNameTemp(filePath), page);
        GtkWidget* label = gtk_label_new(CStrTemp(text));
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_widget_set_margin_start(label, 8);
        gtk_widget_set_margin_end(label, 8);
        gtk_widget_set_margin_top(label, 4);
        gtk_widget_set_margin_bottom(label, 4);
        gtk_widget_set_tooltip_text(label, CStrTemp(filePath));
        GtkWidget* row = gtk_list_box_row_new();
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data(G_OBJECT(row), "sumatra-favorite-index", GINT_TO_POINTER(i));
        gtk_list_box_append(GTK_LIST_BOX(window->favoritesList), row);
    }
    UpdateSidebar(window);
}

static void ToggleFavorites(LinuxWindow* window) {
    window->favoritesVisible = !window->favoritesVisible;
    if (window->favoritesVisible) {
        window->tocVisible = false;
    }
    if (gGlobalPrefs) {
        gGlobalPrefs->showFavorites = window->favoritesVisible;
        gGlobalPrefs->showToc = window->tocVisible;
    }
    UpdateFavorites(window);
}

static void ShowProperties(LinuxWindow* window) {
    DocumentView* view = ActiveView(window);
    if (!view) {
        return;
    }

    GtkWidget* dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Document Properties");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 620, 480);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(root, 16);
    gtk_widget_set_margin_end(root, 16);
    gtk_widget_set_margin_top(root, 16);
    gtk_widget_set_margin_bottom(root, 16);
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    int count = view->PropertyCount();
    for (int i = 0; i < count; i++) {
        TempStr propertyName = fmt("%s:", view->PropertyName(i));
        GtkWidget* name = gtk_label_new(CStrTemp(propertyName));
        gtk_label_set_xalign(GTK_LABEL(name), 1);
        gtk_widget_set_valign(name, GTK_ALIGN_START);
        GtkWidget* value = gtk_label_new(CStrTemp(view->PropertyValue(i)));
        gtk_label_set_xalign(GTK_LABEL(value), 0);
        gtk_label_set_wrap(GTK_LABEL(value), TRUE);
        gtk_label_set_selectable(GTK_LABEL(value), TRUE);
        gtk_widget_set_hexpand(value, TRUE);
        gtk_grid_attach(GTK_GRID(grid), name, 0, i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), value, 1, i, 1, 1);
    }
    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), grid);
    gtk_box_append(GTK_BOX(root), scroll);
    GtkWidget* close = gtk_button_new_with_label("Close");
    gtk_widget_set_halign(close, GTK_ALIGN_END);
    g_signal_connect_swapped(close, "clicked", G_CALLBACK(gtk_window_destroy), dialog);
    gtk_box_append(GTK_BOX(root), close);
    gtk_window_set_child(GTK_WINDOW(dialog), root);
    gtk_window_present(GTK_WINDOW(dialog));
}

static bool RunFind(LinuxWindow* window, bool forward, bool restart) {
    DocumentView* view = ActiveView(window);
    const char* text = gtk_editable_get_text(GTK_EDITABLE(window->findEntry));
    bool found = view && text && text[0] && view->FindText(Str(text), forward, restart);
    gtk_label_set_text(GTK_LABEL(window->findStatus), found ? "Found" : "No matches");
    return found;
}

static void ShowFindBar(LinuxWindow* window) {
    gtk_widget_set_visible(window->findBar, TRUE);
    gtk_widget_grab_focus(window->findEntry);
    gtk_editable_select_region(GTK_EDITABLE(window->findEntry), 0, -1);
}

static void OnFindEntryActivate(GtkEntry*, gpointer data) {
    RunFind((LinuxWindow*)data, true, false);
}

static void OnFindPrevious(GtkButton*, gpointer data) {
    RunFind((LinuxWindow*)data, false, false);
}

static void OnFindNext(GtkButton*, gpointer data) {
    RunFind((LinuxWindow*)data, true, false);
}

static void OnFindClose(GtkButton*, gpointer data) {
    auto* window = (LinuxWindow*)data;
    gtk_widget_set_visible(window->findBar, FALSE);
    DocumentView* view = ActiveView(window);
    if (view) {
        view->Focus();
    }
}

static void SetFullscreen(LinuxWindow* window, bool fullscreen) {
    window->fullscreen = fullscreen;
    if (fullscreen) {
        gtk_window_fullscreen(GTK_WINDOW(window->window));
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(window->window));
    }
}

static void ToggleFullscreen(LinuxWindow* window) {
    SetFullscreen(window, !window->fullscreen);
}

static void TogglePresentation(LinuxWindow* window) {
    if (!window->presentation) {
        LinuxTab* tab = ActiveTab(window);
        DocumentView* view = LinuxTabView(tab);
        if (!view || view->PageCount() == 0) {
            return;
        }
        window->presentation = true;
        window->presentationTab = tab;
        window->previousContinuous = view->IsContinuous();
        window->previousZoom = view->Zoom();
        window->wasFullscreen = window->fullscreen;
        window->previousTocVisible = window->tocVisible;
        window->previousFavoritesVisible = window->favoritesVisible;
        gtk_widget_set_visible(window->toolbar, FALSE);
        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(window->notebook), FALSE);
        gtk_widget_set_visible(window->sidebar, FALSE);
        window->suppressStateSave = true;
        view->SetContinuous(false);
        view->SetZoom(kZoomFitPage);
        window->suppressStateSave = false;
        SetFullscreen(window, true);
    } else {
        LinuxTab* tab = window->presentationTab;
        DocumentView* view = FindTabIndex(window, tab) >= 0 ? LinuxTabView(tab) : nullptr;
        bool wasFullscreen = window->wasFullscreen;
        window->presentation = false;
        window->presentationTab = nullptr;
        window->tocVisible = window->previousTocVisible;
        window->favoritesVisible = window->previousFavoritesVisible;
        gtk_widget_set_visible(window->toolbar, TRUE);
        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(window->notebook), TRUE);
        if (view) {
            window->suppressStateSave = true;
            view->SetContinuous(window->previousContinuous);
            view->SetZoom(window->previousZoom);
            window->suppressStateSave = false;
        }
        if (!wasFullscreen) {
            SetFullscreen(window, false);
        }
        UpdateToc(window);
        UpdateFavorites(window);
    }
    UpdateControls(window);
}

static void ShowKeyboardHelp(LinuxWindow* window) {
    KeyboardHelpArgs args;
    args.parent = window->window;
    args.parentFullscreen = window->fullscreen;
    args.dataSource = GetDefaultKeyboardHelpDataSource();
    ToggleKeyboardHelp(args);
}

static void DispatchPaletteCommand(LinuxWindow* window, int commandId) {
    LinuxWindowDispatchCommand(window, commandId);
}

static void ShowCommandPalette(LinuxWindow* window) {
    static const int commands[] = {
        CmdOpenFile,
        CmdPrint,
        CmdShowInFolder,
        CmdCloseCurrentDocument,
        CmdReopenLastClosedFile,
        CmdNextTab,
        CmdPrevTab,
        CmdGoToPrevPage,
        CmdGoToNextPage,
        CmdZoomFitPage,
        CmdZoomFitWidth,
        CmdZoomActualSize,
        CmdRotateLeft,
        CmdRotateRight,
        CmdToggleContinuousView,
        CmdToggleFullscreen,
        CmdTogglePresentationMode,
        CmdToggleKeyboardHelp,
        CmdToggleBookmarks,
        CmdFavoriteAdd,
        CmdFavoriteDel,
        CmdFavoriteToggle,
        CmdProperties,
        CmdFindFirst,
        CmdFindNext,
        CmdFindPrev,
        CmdCopySelection,
        CmdCopyFilePath,
        CmdSelectAll,
    };
    ShowLinuxCommandPalette(GTK_WINDOW(window->window), commands, dimofi(commands),
                            MkFunc1(DispatchPaletteCommand, window));
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
    if (tab == window->presentationTab) {
        TogglePresentation(window);
    }
    RememberClosedPath(window, LinuxTabPath(tab));
    LinuxPrefsSaveView(LinuxTabView(tab), LinuxTabPath(tab));
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
    gtk_menu_button_popdown(GTK_MENU_BUTTON(window->menuButton));
    switch (commandId) {
        case CmdOpenFile:
            ShowOpenDialog(window);
            return;
        case CmdPrint:
            ShowLinuxPrintDialog(GTK_WINDOW(window->window), ActiveView(window));
            return;
        case CmdShowInFolder:
            LinuxShowFileInFolder(LinuxTabPath(ActiveTab(window)));
            return;
        case CmdCloseCurrentDocument:
            CloseTab(window, ActiveTab(window));
            return;
        case CmdReopenLastClosedFile:
            ReopenClosedTab(window);
            return;
        case CmdNextTab:
            if (window->presentation) {
                TogglePresentation(window);
            }
            SelectRelativeTab(window, 1);
            return;
        case CmdPrevTab:
            if (window->presentation) {
                TogglePresentation(window);
            }
            SelectRelativeTab(window, -1);
            return;
        case CmdToggleFullscreen:
            ToggleFullscreen(window);
            return;
        case CmdTogglePresentationMode:
            TogglePresentation(window);
            return;
        case CmdToggleKeyboardHelp:
            ShowKeyboardHelp(window);
            return;
        case CmdCommandPalette:
            ShowCommandPalette(window);
            return;
        case CmdToggleBookmarks:
        case CmdToggleTableOfContents:
            ToggleToc(window);
            return;
        case CmdFavoriteToggle:
            ToggleFavorites(window);
            return;
        case CmdProperties:
            ShowProperties(window);
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
        case CmdCopySelection:
            view->CopySelection();
            break;
        case CmdCopyFilePath:
            LinuxClipboardSetText(window->window, LinuxTabPath(ActiveTab(window)));
            break;
        case CmdSelectAll:
            view->SelectAll();
            break;
        case CmdFindFirst:
            ShowFindBar(window);
            break;
        case CmdFindNext:
            RunFind(window, true, false);
            break;
        case CmdFindPrev:
            RunFind(window, false, false);
            break;
        case CmdFavoriteAdd:
            LinuxPrefsAddFavorite(LinuxTabPath(ActiveTab(window)), view->CurrentPageNo());
            UpdateFavorites(window);
            break;
        case CmdFavoriteDel:
            LinuxPrefsRemoveFavorite(LinuxTabPath(ActiveTab(window)), view->CurrentPageNo());
            UpdateFavorites(window);
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
    } else if (keyval == GDK_KEY_F5) {
        command = CmdTogglePresentationMode;
    } else if (keyval == GDK_KEY_question) {
        command = CmdToggleKeyboardHelp;
    } else if (keyval == GDK_KEY_Escape && window->presentation) {
        command = CmdTogglePresentationMode;
    } else if (keyval == GDK_KEY_Escape && gtk_widget_get_visible(window->findBar)) {
        OnFindClose(nullptr, window);
        return TRUE;
    } else if (keyval == GDK_KEY_Escape && window->fullscreen) {
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
    UpdateToc(window);
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

    result->header = gtk_header_bar_new();
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(result->header), TRUE);
    result->menuButton = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(result->menuButton), "open-menu-symbolic");
    gtk_widget_set_tooltip_text(result->menuButton, "Main menu");
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(result->menuButton), gtk_application_get_menubar(app));
    gtk_header_bar_pack_end(GTK_HEADER_BAR(result->header), result->menuButton);
    GtkWidget* fullscreen = NewCommandButton(result, "Fullscreen", "Toggle fullscreen (F11)", CmdToggleFullscreen);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(result->header), fullscreen);
    gtk_window_set_titlebar(GTK_WINDOW(result->window), result->header);

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

    result->findBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(result->findBar, 8);
    gtk_widget_set_margin_end(result->findBar, 8);
    gtk_widget_set_margin_bottom(result->findBar, 6);
    result->findEntry = gtk_search_entry_new();
    gtk_widget_set_hexpand(result->findEntry, TRUE);
    g_signal_connect(result->findEntry, "activate", G_CALLBACK(OnFindEntryActivate), result);
    GtkWidget* findPrevious = gtk_button_new_with_label("Previous");
    GtkWidget* findNext = gtk_button_new_with_label("Next");
    GtkWidget* findClose = gtk_button_new_with_label("Close");
    result->findStatus = gtk_label_new("");
    g_signal_connect(findPrevious, "clicked", G_CALLBACK(OnFindPrevious), result);
    g_signal_connect(findNext, "clicked", G_CALLBACK(OnFindNext), result);
    g_signal_connect(findClose, "clicked", G_CALLBACK(OnFindClose), result);
    gtk_box_append(GTK_BOX(result->findBar), result->findEntry);
    gtk_box_append(GTK_BOX(result->findBar), findPrevious);
    gtk_box_append(GTK_BOX(result->findBar), findNext);
    gtk_box_append(GTK_BOX(result->findBar), result->findStatus);
    gtk_box_append(GTK_BOX(result->findBar), findClose);
    gtk_widget_set_visible(result->findBar, FALSE);
    gtk_box_append(GTK_BOX(result->root), result->findBar);

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
    result->documentPane = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    result->sidebar = gtk_stack_new();
    result->tocList = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(result->tocList), GTK_SELECTION_SINGLE);
    g_signal_connect(result->tocList, "row-activated", G_CALLBACK(OnTocRowActivated), result);
    result->tocScroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(result->tocScroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(result->tocScroll), result->tocList);
    gtk_stack_add_named(GTK_STACK(result->sidebar), result->tocScroll, "toc");
    result->favoritesList = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(result->favoritesList), GTK_SELECTION_SINGLE);
    g_signal_connect(result->favoritesList, "row-activated", G_CALLBACK(OnFavoriteRowActivated), result);
    result->favoritesScroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(result->favoritesScroll), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(result->favoritesScroll), result->favoritesList);
    gtk_stack_add_named(GTK_STACK(result->sidebar), result->favoritesScroll, "favorites");
    gtk_widget_set_size_request(result->sidebar, 240, -1);
    gtk_paned_set_start_child(GTK_PANED(result->documentPane), result->sidebar);
    gtk_paned_set_end_child(GTK_PANED(result->documentPane), result->notebook);
    gtk_paned_set_resize_start_child(GTK_PANED(result->documentPane), FALSE);
    result->tocVisible = gGlobalPrefs && gGlobalPrefs->showToc;
    result->favoritesVisible = gGlobalPrefs && gGlobalPrefs->showFavorites;
    gtk_widget_set_visible(result->sidebar, FALSE);
    gtk_stack_add_named(GTK_STACK(result->content), result->documentPane, "notebook");
    gtk_stack_set_visible_child_name(GTK_STACK(result->content), "empty");
    gtk_box_append(GTK_BOX(result->root), result->content);
    gtk_window_set_child(GTK_WINDOW(result->window), result->root);
    g_signal_connect(result->window, "close-request", G_CALLBACK(OnWindowCloseRequest), result);

    GtkEventController* keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
    g_signal_connect(keys, "key-pressed", G_CALLBACK(OnWindowKey), result);
    gtk_widget_add_controller(result->window, keys);

    g_object_set_data_full(G_OBJECT(result->window), "sumatra-linux-window", result, FreeLinuxWindow);
    UpdateToc(result);
    UpdateFavorites(result);
    UpdateControls(result);
    return result;
}

static void OpenFile(LinuxWindow* window, GFile* file, int sessionIndex) {
    if (!window || !file) {
        return;
    }
    char* baseName = g_file_get_basename(file);
    Str title(baseName ? baseName : "Document");
    LinuxTab* tab =
        LinuxTabCreate(title, MkFunc0(OnDocumentStateChanged, window), MkFunc0(OnDocumentReloaded, window),
                       MkFunc1(OpenLinkedUrl, window), MkFunc1(OpenLinkedFile, window), MkFunc1(CopyText, window));
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
    if (LinuxTabOpenFile(tab, file)) {
        window->suppressStateSave = true;
        LinuxPrefsOpenView(LinuxTabView(tab), LinuxTabPath(tab));
        if (sessionIndex >= 0) {
            LinuxPrefsRestoreSessionView(LinuxTabView(tab), sessionIndex);
        }
        window->suppressStateSave = false;
    }
    UpdateToc(window);
    UpdateFavorites(window);
    UpdateControls(window);
}

void LinuxWindowOpenFile(LinuxWindow* window, GFile* file) {
    OpenFile(window, file, -1);
}

void LinuxWindowRestoreSession(LinuxWindow* window) {
    if (!window) {
        return;
    }
    int tabCount = LinuxPrefsSessionTabCount();
    int restoredCount = 0;
    int activeTab = LinuxPrefsSessionActiveTab();
    int restoredActiveTab = 0;
    for (int i = 0; i < tabCount; i++) {
        Str path = LinuxPrefsSessionPath(i);
        if (!file::Exists(path)) {
            continue;
        }
        GFile* file = g_file_new_for_path(CStrTemp(path));
        OpenFile(window, file, i);
        g_object_unref(file);
        if (i == activeTab) {
            restoredActiveTab = restoredCount;
        }
        restoredCount++;
    }
    if (restoredCount > 0) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(window->notebook), restoredActiveTab);
        UpdateToc(window);
        UpdateControls(window);
    }
}

void LinuxWindowFindText(LinuxWindow* window, Str text) {
    if (!window || !text) {
        return;
    }
    gtk_editable_set_text(GTK_EDITABLE(window->findEntry), CStrTemp(text));
    ShowFindBar(window);
    RunFind(window, true, true);
}

void LinuxWindowGoToFavorite(LinuxWindow* window, int index) {
    if (!window) {
        return;
    }
    TempStr filePath = str::DupTemp(LinuxPrefsFavoritePath(index));
    int pageNo = LinuxPrefsFavoritePageNo(index);
    if (!filePath || pageNo < 1) {
        return;
    }
    for (int i = 0; i < len(window->tabs); i++) {
        LinuxTab* tab = window->tabs[i];
        if (str::Eq(LinuxTabPath(tab), filePath)) {
            int pageIndex = gtk_notebook_page_num(GTK_NOTEBOOK(window->notebook), LinuxTabWidget(tab));
            gtk_notebook_set_current_page(GTK_NOTEBOOK(window->notebook), pageIndex);
            LinuxTabView(tab)->GoToPage(pageNo);
            UpdateControls(window);
            return;
        }
    }
    GFile* file = g_file_new_for_path(CStrTemp(filePath));
    LinuxWindowOpenFile(window, file);
    g_object_unref(file);
    LinuxWindowGoToPage(window, pageNo);
}

void LinuxWindowGoToTocItem(LinuxWindow* window, int index) {
    DocumentView* view = window ? ActiveView(window) : nullptr;
    if (view && view->GoToTocItem(index)) {
        view->Focus();
        UpdateControls(window);
    }
}

void LinuxWindowGoToPage(LinuxWindow* window, int pageNo) {
    DocumentView* view = window ? ActiveView(window) : nullptr;
    if (view) {
        view->GoToPage(pageNo);
        UpdateControls(window);
    }
}

void LinuxWindowPresent(LinuxWindow* window) {
    if (window) {
        gtk_window_present(GTK_WINDOW(window->window));
    }
}
