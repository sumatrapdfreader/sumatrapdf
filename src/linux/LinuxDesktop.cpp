/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "base/File.h"

#include <gtk/gtk.h>

#include "linux/LinuxDesktop.h"

void LinuxClipboardSetText(GtkWidget* widget, Str text) {
    if (!widget || !text) {
        return;
    }
    GdkClipboard* clipboard = gtk_widget_get_clipboard(widget);
    gdk_clipboard_set_text(clipboard, CStrTemp(text));
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

bool LinuxShowFileInFolder(Str filePath) {
    if (!filePath || ShowItemThroughFileManager(filePath)) {
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
