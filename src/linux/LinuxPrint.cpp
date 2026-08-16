/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"

#include "gui/DocumentView.h"
#include "gui/Gfx.h"

#include <gtk/gtk.h>

#include "linux/LinuxPrint.h"

struct LinuxPrintJob {
    DocumentView* view = nullptr;
};

static GtkPrintSettings* gPrintSettings = nullptr;
static GtkPageSetup* gPageSetup = nullptr;

static void BeginPrint(GtkPrintOperation* operation, GtkPrintContext*, gpointer data) {
    auto* job = (LinuxPrintJob*)data;
    gtk_print_operation_set_n_pages(operation, job->view->PageCount());
}

static void DrawPrintPage(GtkPrintOperation*, GtkPrintContext* context, int pageIndex, gpointer data) {
    auto* job = (LinuxPrintJob*)data;
    DocumentView* view = job->view;
    int pageNo = pageIndex + 1;
    RectF mediaBox = view->PageMediabox(pageNo);
    if (mediaBox.IsEmpty()) {
        return;
    }

    int rotation = view->Rotation();
    double mediaWidth = mediaBox.dx * 72.0 / view->FileDPI();
    double mediaHeight = mediaBox.dy * 72.0 / view->FileDPI();
    if (rotation == 90 || rotation == 270) {
        std::swap(mediaWidth, mediaHeight);
    }
    double printWidth = gtk_print_context_get_width(context);
    double printHeight = gtk_print_context_get_height(context);
    double fit = std::min(printWidth / mediaWidth, printHeight / mediaHeight);
    double printerDpi = std::min(gtk_print_context_get_dpi_x(context), gtk_print_context_get_dpi_y(context));
    double renderDpi = std::max(72.0, std::min(300.0, printerDpi));
    float renderZoom = (float)(fit * renderDpi / view->FileDPI());
    Pixmap* pixmap = view->RenderPageForPrint(pageNo, renderZoom);
    if (!pixmap) {
        return;
    }

    int width = (int)(mediaWidth * fit + 0.5);
    int height = (int)(mediaHeight * fit + 0.5);
    int x = (int)((printWidth - width) / 2.0 + 0.5);
    int y = (int)((printHeight - height) / 2.0 + 0.5);
    Gfx* gfx = GfxCreate(gtk_print_context_get_cairo_context(context));
    gfx->DrawPixmap(pixmap, {x, y, width, height});
    delete gfx;
    FreePixmap(pixmap);
}

void ShowLinuxPrintDialog(GtkWindow* parent, DocumentView* view) {
    if (!parent || !view || !view->CanPrint()) {
        return;
    }
    LinuxPrintJob job{view};
    GtkPrintOperation* operation = gtk_print_operation_new();
    gtk_print_operation_set_unit(operation, GTK_UNIT_POINTS);
    gtk_print_operation_set_use_full_page(operation, FALSE);
    gtk_print_operation_set_embed_page_setup(operation, TRUE);
    if (gPrintSettings) {
        gtk_print_operation_set_print_settings(operation, gPrintSettings);
    }
    if (gPageSetup) {
        gtk_print_operation_set_default_page_setup(operation, gPageSetup);
    }
    g_signal_connect(operation, "begin-print", G_CALLBACK(BeginPrint), &job);
    g_signal_connect(operation, "draw-page", G_CALLBACK(DrawPrintPage), &job);

    GError* error = nullptr;
    GtkPrintOperationResult result =
        gtk_print_operation_run(operation, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, parent, &error);
    if (result == GTK_PRINT_OPERATION_RESULT_APPLY) {
        g_clear_object(&gPrintSettings);
        g_clear_object(&gPageSetup);
        gPrintSettings = gtk_print_operation_get_print_settings(operation);
        gPageSetup = gtk_print_operation_get_default_page_setup(operation);
        if (gPrintSettings) {
            g_object_ref(gPrintSettings);
        }
        if (gPageSetup) {
            g_object_ref(gPageSetup);
        }
    } else if (result == GTK_PRINT_OPERATION_RESULT_ERROR) {
        g_warning("Could not print document: %s", error ? error->message : "unknown error");
    }
    g_clear_error(&error);
    g_object_unref(operation);
}

void LinuxPrintShutdown() {
    g_clear_object(&gPrintSettings);
    g_clear_object(&gPageSetup);
}
