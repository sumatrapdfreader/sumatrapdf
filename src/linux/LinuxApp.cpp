/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "Commands.h"

#include <gtk/gtk.h>

#include "linux/LinuxWindow.h"
#include "linux/LinuxApp.h"

struct LinuxAppState {
    LinuxWindow* window = nullptr;
};

struct EBookUI;
EBookUI* GetEBookUI() {
    return nullptr;
}

struct FileEBookUI;
FileEBookUI* GetFileEBookUI(Str) {
    return nullptr;
}

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
    for (int i = 0; i < nFiles; i++) {
        LinuxWindowOpenFile(window, files[i]);
    }
    LinuxWindowPresent(window);
}

static void FreeState(gpointer data) {
    delete (LinuxAppState*)data;
}

static void OnQuit(GSimpleAction*, GVariant*, gpointer data) {
    g_application_quit(G_APPLICATION(data));
}

static void OnWindowCommand(GSimpleAction* action, GVariant*, gpointer data) {
    GtkApplication* app = GTK_APPLICATION(data);
    LinuxWindow* window = GetState(app)->window;
    if (!window) {
        return;
    }
    const char* name = g_action_get_name(G_ACTION(action));
    int command = 0;
    if (str::Eq(Str(name), StrL("open"))) {
        command = CmdOpenFile;
    } else if (str::Eq(Str(name), StrL("close-tab"))) {
        command = CmdCloseCurrentDocument;
    } else if (str::Eq(Str(name), StrL("reopen-tab"))) {
        command = CmdReopenLastClosedFile;
    } else if (str::Eq(Str(name), StrL("next-tab"))) {
        command = CmdNextTab;
    } else if (str::Eq(Str(name), StrL("previous-tab"))) {
        command = CmdPrevTab;
    } else if (str::Eq(Str(name), StrL("fullscreen"))) {
        command = CmdToggleFullscreen;
    } else if (str::Eq(Str(name), StrL("presentation"))) {
        command = CmdTogglePresentationMode;
    } else if (str::Eq(Str(name), StrL("keyboard-help"))) {
        command = CmdToggleKeyboardHelp;
    } else if (str::Eq(Str(name), StrL("copy"))) {
        command = CmdCopySelection;
    } else if (str::Eq(Str(name), StrL("select-all"))) {
        command = CmdSelectAll;
    } else if (str::Eq(Str(name), StrL("find"))) {
        command = CmdFindFirst;
    } else if (str::Eq(Str(name), StrL("find-next"))) {
        command = CmdFindNext;
    } else if (str::Eq(Str(name), StrL("find-previous"))) {
        command = CmdFindPrev;
    } else if (str::Eq(Str(name), StrL("bookmarks"))) {
        command = CmdToggleBookmarks;
    }
    LinuxWindowDispatchCommand(window, command);
}

static void OnSearchText(GSimpleAction*, GVariant* parameter, gpointer data) {
    auto* app = GTK_APPLICATION(data);
    LinuxWindow* window = GetState(app)->window;
    const char* text = parameter ? g_variant_get_string(parameter, nullptr) : nullptr;
    if (window && text) {
        LinuxWindowFindText(window, Str(text));
    }
}

static void OnTocItem(GSimpleAction*, GVariant* parameter, gpointer data) {
    auto* app = GTK_APPLICATION(data);
    LinuxWindow* window = GetState(app)->window;
    if (window && parameter) {
        LinuxWindowGoToTocItem(window, g_variant_get_int32(parameter));
    }
}

static GMenu* CreateMainMenu() {
    GMenu* menu = g_menu_new();
    GMenu* file = g_menu_new();
    g_menu_append(file, "Open...", "app.open");
    g_menu_append(file, "Close Tab", "app.close-tab");
    g_menu_append(file, "Reopen Closed Tab", "app.reopen-tab");
    g_menu_append(file, "Quit", "app.quit");
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(file));
    g_object_unref(file);

    GMenu* edit = g_menu_new();
    g_menu_append(edit, "Copy", "app.copy");
    g_menu_append(edit, "Select All", "app.select-all");
    g_menu_append(edit, "Find...", "app.find");
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(edit));
    g_object_unref(edit);

    GMenu* view = g_menu_new();
    g_menu_append(view, "Fullscreen", "app.fullscreen");
    g_menu_append(view, "Presentation", "app.presentation");
    g_menu_append(view, "Bookmarks", "app.bookmarks");
    g_menu_append(view, "Keyboard Shortcuts", "app.keyboard-help");
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(view));
    g_object_unref(view);
    return menu;
}

static void OnStartup(GtkApplication* app, gpointer) {
    GMenu* menu = CreateMainMenu();
    gtk_application_set_menubar(app, G_MENU_MODEL(menu));
    g_object_unref(menu);
}

int RunLinuxApp(int argc, char** argv) {
    GtkApplication* app = gtk_application_new("org.sumatrapdf.SumatraPDF", G_APPLICATION_HANDLES_OPEN);
    auto* state = new LinuxAppState();
    g_object_set_data_full(G_OBJECT(app), "sumatra-linux-state", state, FreeState);
    g_signal_connect(app, "startup", G_CALLBACK(OnStartup), nullptr);
    g_signal_connect(app, "activate", G_CALLBACK(OnActivate), nullptr);
    g_signal_connect(app, "open", G_CALLBACK(OnOpen), nullptr);
    const GActionEntry actions[] = {
        {"quit", OnQuit},
        {"open", OnWindowCommand},
        {"close-tab", OnWindowCommand},
        {"reopen-tab", OnWindowCommand},
        {"next-tab", OnWindowCommand},
        {"previous-tab", OnWindowCommand},
        {"fullscreen", OnWindowCommand},
        {"presentation", OnWindowCommand},
        {"keyboard-help", OnWindowCommand},
        {"copy", OnWindowCommand},
        {"select-all", OnWindowCommand},
        {"find", OnWindowCommand},
        {"find-next", OnWindowCommand},
        {"find-previous", OnWindowCommand},
        {"search", OnSearchText, "s"},
        {"bookmarks", OnWindowCommand},
        {"toc-item", OnTocItem, "i"},
    };
    g_action_map_add_action_entries(G_ACTION_MAP(app), actions, dimofi(actions), app);

    const char* quitAccels[] = {"<Primary>q", nullptr};
    const char* openAccels[] = {"<Primary>o", nullptr};
    const char* closeAccels[] = {"<Primary>w", nullptr};
    const char* reopenAccels[] = {"<Primary><Shift>t", nullptr};
    const char* fullscreenAccels[] = {"F11", nullptr};
    const char* presentationAccels[] = {"F5", nullptr};
    const char* helpAccels[] = {"question", nullptr};
    const char* copyAccels[] = {"<Primary>c", nullptr};
    const char* selectAllAccels[] = {"<Primary>a", nullptr};
    const char* findAccels[] = {"<Primary>f", nullptr};
    const char* findNextAccels[] = {"F3", nullptr};
    const char* findPreviousAccels[] = {"<Shift>F3", nullptr};
    const char* bookmarksAccels[] = {"F12", nullptr};
    gtk_application_set_accels_for_action(app, "app.quit", quitAccels);
    gtk_application_set_accels_for_action(app, "app.open", openAccels);
    gtk_application_set_accels_for_action(app, "app.close-tab", closeAccels);
    gtk_application_set_accels_for_action(app, "app.reopen-tab", reopenAccels);
    gtk_application_set_accels_for_action(app, "app.fullscreen", fullscreenAccels);
    gtk_application_set_accels_for_action(app, "app.presentation", presentationAccels);
    gtk_application_set_accels_for_action(app, "app.keyboard-help", helpAccels);
    gtk_application_set_accels_for_action(app, "app.copy", copyAccels);
    gtk_application_set_accels_for_action(app, "app.select-all", selectAllAccels);
    gtk_application_set_accels_for_action(app, "app.find", findAccels);
    gtk_application_set_accels_for_action(app, "app.find-next", findNextAccels);
    gtk_application_set_accels_for_action(app, "app.find-previous", findPreviousAccels);
    gtk_application_set_accels_for_action(app, "app.bookmarks", bookmarksAccels);

    int code = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return code;
}
