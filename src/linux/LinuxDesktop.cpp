/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "base/File.h"

#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

#include "linux/LinuxDesktop.h"

void LinuxClipboardSetText(GtkWidget* widget, Str text) {
    if (!widget || !text) {
        return;
    }
    GdkClipboard* clipboard = gtk_widget_get_clipboard(widget);
    gdk_clipboard_set_text(clipboard, CStrTemp(text));
}

static bool IsSumatraAppInfo(GAppInfo* appInfo) {
    const char* id = appInfo ? g_app_info_get_id(appInfo) : nullptr;
    return id && str::Eq(Str(id), StrL("org.sumatrapdf.SumatraPDF.desktop"));
}

static bool IsDefaultPdfReader() {
    GAppInfo* appInfo = g_app_info_get_default_for_type("application/pdf", FALSE);
    bool result = IsSumatraAppInfo(appInfo);
    g_clear_object(&appInfo);
    return result;
}

static GAppInfo* FindSumatraAppInfo() {
    return G_APP_INFO(g_desktop_app_info_new("org.sumatrapdf.SumatraPDF.desktop"));
}

static void ShowDesktopMessage(GtkWindow* parent, const char* title, const char* message) {
    GtkWidget* dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(content, 20);
    gtk_widget_set_margin_end(content, 20);
    gtk_widget_set_margin_top(content, 20);
    gtk_widget_set_margin_bottom(content, 20);
    GtkWidget* label = gtk_label_new(message);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 60);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_box_append(GTK_BOX(content), label);
    GtkWidget* close = gtk_button_new_with_label("Close");
    gtk_widget_set_halign(close, GTK_ALIGN_END);
    g_signal_connect_swapped(close, "clicked", G_CALLBACK(gtk_window_destroy), dialog);
    gtk_box_append(GTK_BOX(content), close);
    gtk_window_set_child(GTK_WINDOW(dialog), content);
    gtk_window_present(GTK_WINDOW(dialog));
}

void LinuxMakeDefaultPdfReader(GtkWindow* parent) {
    if (IsDefaultPdfReader()) {
        ShowDesktopMessage(parent, "Default PDF Reader", "SumatraPDF is already the default PDF reader.");
        return;
    }

    GAppInfo* appInfo = FindSumatraAppInfo();
    if (!appInfo) {
        ShowDesktopMessage(parent, "Default PDF Reader",
                           "The SumatraPDF desktop entry is not installed. Install the Linux package first.");
        return;
    }

    GError* error = nullptr;
    bool ok = g_app_info_set_as_default_for_type(appInfo, "application/pdf", &error);
    g_object_unref(appInfo);
    if (ok && IsDefaultPdfReader()) {
        ShowDesktopMessage(parent, "Default PDF Reader", "SumatraPDF is now the default PDF reader.");
    } else {
        TempStr message = error
                              ? fmt("Could not set the default PDF reader: %s", Str(error->message))
                              : str::DupTemp(StrL("The desktop did not accept SumatraPDF as the default PDF reader."));
        ShowDesktopMessage(parent, "Default PDF Reader", CStrTemp(message));
    }
    g_clear_error(&error);
}

static bool ShowItemThroughFileManager(Str path) {
    GError* error = nullptr;
    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!bus) {
        g_clear_error(&error);
        return false;
    }
    char* uri = g_filename_to_uri(CStrTemp(path), nullptr, &error);
    if (!uri) {
        g_clear_error(&error);
        g_object_unref(bus);
        return false;
    }
    const char* uris[] = {uri, nullptr};
    GVariant* result = g_dbus_connection_call_sync(
        bus, "org.freedesktop.FileManager1", "/org/freedesktop/FileManager1", "org.freedesktop.FileManager1",
        "ShowItems", g_variant_new("(^ass)", uris, ""), nullptr, G_DBUS_CALL_FLAGS_NONE, 2000, nullptr, &error);
    g_free(uri);
    g_object_unref(bus);
    if (!result) {
        g_clear_error(&error);
        return false;
    }
    g_variant_unref(result);
    return true;
}

static bool ShowItemThroughWindowsExplorer(Str path) {
    if (!g_getenv("WSL_INTEROP") && !g_getenv("WSL_DISTRO_NAME")) {
        return false;
    }

    char* wslPathArgs[] = {(char*)"wslpath", (char*)"-w", CStrTemp(path), nullptr};
    char* windowsPath = nullptr;
    char* stderrText = nullptr;
    int exitStatus = 0;
    GError* error = nullptr;
    bool ok = g_spawn_sync(nullptr, wslPathArgs, nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, &windowsPath,
                           &stderrText, &exitStatus, &error);
    if (ok) {
        ok = g_spawn_check_wait_status(exitStatus, &error);
    }
    g_free(stderrText);
    g_clear_error(&error);
    if (!ok || !windowsPath) {
        g_free(windowsPath);
        return false;
    }

    g_strchomp(windowsPath);
    char* selectArg = g_strdup_printf("/select,%s", windowsPath);
    char* explorerArgs[] = {(char*)"explorer.exe", selectArg, nullptr};
    ok = g_spawn_async(nullptr, explorerArgs, nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, &error);
    g_free(selectArg);
    g_free(windowsPath);
    g_clear_error(&error);
    return ok;
}

bool LinuxShowFileInFolder(Str filePath) {
    if (!filePath || ShowItemThroughWindowsExplorer(filePath) || ShowItemThroughFileManager(filePath)) {
        return !!filePath;
    }
    TempStr dirPath = path::GetDirTemp(filePath);
    if (!dirPath) {
        return false;
    }
    GFile* dir = g_file_new_for_path(CStrTemp(dirPath));
    char* uri = g_file_get_uri(dir);
    g_object_unref(dir);
    GError* error = nullptr;
    bool ok = g_app_info_launch_default_for_uri(uri, nullptr, &error);
    if (!ok) {
        g_warning("Could not show file in folder: %s", error ? error->message : "unknown error");
    }
    g_clear_error(&error);
    g_free(uri);
    return ok;
}
