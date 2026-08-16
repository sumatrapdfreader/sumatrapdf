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
    if (str::Eq(Str(name), StrL("close-tab"))) {
        command = CmdCloseCurrentDocument;
    } else if (str::Eq(Str(name), StrL("reopen-tab"))) {
        command = CmdReopenLastClosedFile;
    } else if (str::Eq(Str(name), StrL("next-tab"))) {
        command = CmdNextTab;
    } else if (str::Eq(Str(name), StrL("previous-tab"))) {
        command = CmdPrevTab;
    }
    LinuxWindowDispatchCommand(window, command);
}

int RunLinuxApp(int argc, char** argv) {
    GtkApplication* app = gtk_application_new("org.sumatrapdf.SumatraPDF", G_APPLICATION_HANDLES_OPEN);
    auto* state = new LinuxAppState();
    g_object_set_data_full(G_OBJECT(app), "sumatra-linux-state", state, FreeState);
    g_signal_connect(app, "activate", G_CALLBACK(OnActivate), nullptr);
    g_signal_connect(app, "open", G_CALLBACK(OnOpen), nullptr);
    const GActionEntry actions[] = {
        {"quit", OnQuit},
        {"close-tab", OnWindowCommand},
        {"reopen-tab", OnWindowCommand},
        {"next-tab", OnWindowCommand},
        {"previous-tab", OnWindowCommand},
    };
    g_action_map_add_action_entries(G_ACTION_MAP(app), actions, dimofi(actions), app);
    const char* quitAccels[] = {"<Primary>q", nullptr};
    gtk_application_set_accels_for_action(app, "app.quit", quitAccels);

    int code = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return code;
}
