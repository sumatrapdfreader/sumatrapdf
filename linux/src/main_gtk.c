/*
 * SumatraPDF Linux - GTK 4 GUI
 *
 * A document viewer using MuPDF for rendering and GTK 4 for the interface.
 * Supports PDF, EPUB, XPS, CBZ, SVG, HTML and image formats.
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mupdf/fitz.h"

/* ─── Application State ────────────────────────────────────────────────────── */

typedef struct {
    GtkApplication *app;
    GtkWidget *window;
    GtkWidget *drawing_area;
    GtkWidget *header_bar;
    GtkWidget *page_label;
    GtkWidget *zoom_label;
    GtkWidget *scroll_window;

    fz_context *ctx;
    fz_document *doc;
    char *filename;

    int page_number;
    int page_count;
    float zoom;
    float scroll_y;

    /* Rendered page cache */
    fz_pixmap *pix;
    int render_width;
    int render_height;
} AppState;

static AppState state = {0};

/* ─── Forward Declarations ─────────────────────────────────────────────────── */

static void render_page(void);
static void update_title(void);
static void open_file(const char *path);
static void close_document(void);

/* ─── Rendering ────────────────────────────────────────────────────────────── */

static void invalidate_cache(void)
{
    if (state.pix) {
        fz_drop_pixmap(state.ctx, state.pix);
        state.pix = NULL;
    }
}

static void render_page(void)
{
    if (!state.doc)
        return;

    invalidate_cache();

    fz_try(state.ctx) {
        fz_matrix ctm = fz_scale(state.zoom, state.zoom);
        state.pix = fz_new_pixmap_from_page_number(state.ctx, state.doc,
                                                    state.page_number, ctm,
                                                    fz_device_rgb(state.ctx), 0);
        state.render_width = state.pix->w;
        state.render_height = state.pix->h;
    }
    fz_catch(state.ctx) {
        fprintf(stderr, "error rendering page: %s\n", fz_caught_message(state.ctx));
    }

    /* Update UI */
    update_title();
    if (state.drawing_area)
        gtk_widget_queue_draw(state.drawing_area);
}

static void update_title(void)
{
    if (!state.window)
        return;

    char title[512];
    if (state.doc) {
        const char *basename = strrchr(state.filename, '/');
        basename = basename ? basename + 1 : state.filename;
        snprintf(title, sizeof(title), "%s — Page %d / %d — %d%%",
                 basename, state.page_number + 1, state.page_count,
                 (int)(state.zoom * 100));
    } else {
        snprintf(title, sizeof(title), "SumatraPDF");
    }
    gtk_window_set_title(GTK_WINDOW(state.window), title);

    if (state.page_label) {
        char buf[64];
        snprintf(buf, sizeof(buf), " %d / %d ", state.page_number + 1, state.page_count);
        gtk_label_set_text(GTK_LABEL(state.page_label), buf);
    }
    if (state.zoom_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), " %d%% ", (int)(state.zoom * 100));
        gtk_label_set_text(GTK_LABEL(state.zoom_label), buf);
    }
}

/* ─── Document Management ──────────────────────────────────────────────────── */

static void close_document(void)
{
    invalidate_cache();
    if (state.doc) {
        fz_drop_document(state.ctx, state.doc);
        state.doc = NULL;
    }
    if (state.filename) {
        g_free(state.filename);
        state.filename = NULL;
    }
    state.page_number = 0;
    state.page_count = 0;
}

static void open_file(const char *path)
{
    close_document();

    fz_try(state.ctx) {
        state.doc = fz_open_document(state.ctx, path);
        state.page_count = fz_count_pages(state.ctx, state.doc);
        state.filename = g_strdup(path);
        state.page_number = 0;
        state.scroll_y = 0;
        render_page();
    }
    fz_catch(state.ctx) {
        fprintf(stderr, "error opening '%s': %s\n", path, fz_caught_message(state.ctx));
        GtkAlertDialog *dialog = gtk_alert_dialog_new("Cannot open file: %s",
                                                       fz_caught_message(state.ctx));
        gtk_alert_dialog_show(dialog, GTK_WINDOW(state.window));
        g_object_unref(dialog);
    }
}

/* ─── Navigation ───────────────────────────────────────────────────────────── */

static void go_to_page(int page)
{
    if (!state.doc)
        return;
    if (page < 0) page = 0;
    if (page >= state.page_count) page = state.page_count - 1;
    if (page == state.page_number)
        return;
    state.page_number = page;
    state.scroll_y = 0;
    render_page();
}

static void next_page(void) { go_to_page(state.page_number + 1); }
static void prev_page(void) { go_to_page(state.page_number - 1); }
static void first_page(void) { go_to_page(0); }
static void last_page(void) { go_to_page(state.page_count - 1); }

static void zoom_in(void)
{
    if (state.zoom < 5.0f) {
        state.zoom *= 1.25f;
        render_page();
    }
}

static void zoom_out(void)
{
    if (state.zoom > 0.25f) {
        state.zoom /= 1.25f;
        render_page();
    }
}

static void zoom_reset(void)
{
    state.zoom = 1.5f;
    render_page();
}

/* ─── Drawing ──────────────────────────────────────────────────────────────── */

static void cairo_data_destroy(void *data)
{
    free(data);
}

static void on_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height,
                    gpointer user_data)
{
    (void)area; (void)user_data;

    /* Background */
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_paint(cr);

    if (!state.pix)
        return;

    /* Center the page in the drawing area */
    int pw = state.render_width;
    int ph = state.render_height;
    double ox = (width - pw) / 2.0;
    double oy = (height - ph) / 2.0;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;

    /* Draw page shadow */
    cairo_set_source_rgba(cr, 0, 0, 0, 0.3);
    cairo_rectangle(cr, ox + 3, oy + 3, pw, ph);
    cairo_fill(cr);

    /* Convert RGB pixmap to Cairo RGB24 surface (Cairo needs 4 bytes/pixel: xRGB) */
    int cairo_stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, pw);
    unsigned char *cairo_data = (unsigned char *)malloc((size_t)cairo_stride * ph);
    if (!cairo_data)
        return;

    for (int y = 0; y < ph; y++) {
        const unsigned char *src = state.pix->samples + (size_t)y * state.pix->stride;
        unsigned char *dst = cairo_data + (size_t)y * cairo_stride;
        for (int x = 0; x < pw; x++) {
            /* Cairo RGB24 is stored as native-endian 0xXXRRGGBB = [B,G,R,X] on little-endian */
            dst[x * 4 + 0] = src[x * 3 + 2]; /* B */
            dst[x * 4 + 1] = src[x * 3 + 1]; /* G */
            dst[x * 4 + 2] = src[x * 3 + 0]; /* R */
            dst[x * 4 + 3] = 0xFF;           /* X (unused) */
        }
    }

    cairo_surface_t *surface = cairo_image_surface_create_for_data(
        cairo_data, CAIRO_FORMAT_RGB24, pw, ph, cairo_stride);
    /* Let Cairo free the data when the surface is destroyed */
    static const cairo_user_data_key_t key;
    cairo_surface_set_user_data(surface, &key, cairo_data, cairo_data_destroy);

    cairo_set_source_surface(cr, surface, ox, oy);
    cairo_paint(cr);
    cairo_surface_destroy(surface);
}

/* ─── Event Handlers ───────────────────────────────────────────────────────── */

static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval,
                                guint keycode, GdkModifierType mods, gpointer data)
{
    (void)controller; (void)keycode; (void)data;

    gboolean ctrl = (mods & GDK_CONTROL_MASK) != 0;

    switch (keyval) {
    case GDK_KEY_Page_Down:
    case GDK_KEY_space:
    case GDK_KEY_j:
        next_page();
        return TRUE;
    case GDK_KEY_Page_Up:
    case GDK_KEY_BackSpace:
    case GDK_KEY_k:
        prev_page();
        return TRUE;
    case GDK_KEY_Home:
        first_page();
        return TRUE;
    case GDK_KEY_End:
        last_page();
        return TRUE;
    case GDK_KEY_plus:
    case GDK_KEY_equal:
        if (ctrl) { zoom_in(); return TRUE; }
        break;
    case GDK_KEY_minus:
        if (ctrl) { zoom_out(); return TRUE; }
        break;
    case GDK_KEY_0:
        if (ctrl) { zoom_reset(); return TRUE; }
        break;
    case GDK_KEY_o:
        if (ctrl) {
            /* Trigger file open */
            GtkFileDialog *dialog = gtk_file_dialog_new();
            gtk_file_dialog_set_title(dialog, "Open Document");

            GtkFileFilter *filter = gtk_file_filter_new();
            gtk_file_filter_set_name(filter, "Documents");
            gtk_file_filter_add_mime_type(filter, "application/pdf");
            gtk_file_filter_add_mime_type(filter, "application/epub+zip");
            gtk_file_filter_add_mime_type(filter, "application/x-cbz");
            gtk_file_filter_add_mime_type(filter, "image/*");
            gtk_file_filter_add_pattern(filter, "*.xps");
            gtk_file_filter_add_pattern(filter, "*.svg");
            gtk_file_filter_add_pattern(filter, "*.fb2");

            GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
            g_list_store_append(filters, filter);
            gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));

            gtk_file_dialog_open(dialog, GTK_WINDOW(state.window), NULL,
                (GAsyncReadyCallback)({
                    void callback(GObject *src, GAsyncResult *res, gpointer ud) {
                        (void)ud;
                        GtkFileDialog *d = GTK_FILE_DIALOG(src);
                        GFile *file = gtk_file_dialog_open_finish(d, res, NULL);
                        if (file) {
                            char *path = g_file_get_path(file);
                            if (path) {
                                open_file(path);
                                g_free(path);
                            }
                            g_object_unref(file);
                        }
                    }
                    callback;
                }), NULL);

            g_object_unref(filters);
            g_object_unref(filter);
            g_object_unref(dialog);
            return TRUE;
        }
        break;
    case GDK_KEY_q:
        if (ctrl) {
            g_application_quit(G_APPLICATION(state.app));
            return TRUE;
        }
        break;
    case GDK_KEY_F11:
        if (gtk_window_is_fullscreen(GTK_WINDOW(state.window)))
            gtk_window_unfullscreen(GTK_WINDOW(state.window));
        else
            gtk_window_fullscreen(GTK_WINDOW(state.window));
        return TRUE;
    }
    return FALSE;
}

static void on_scroll(GtkEventControllerScroll *controller, double dx, double dy,
                      gpointer data)
{
    (void)controller; (void)dx; (void)data;

    GdkModifierType mods = gtk_event_controller_get_current_event_state(
        GTK_EVENT_CONTROLLER(controller));

    if (mods & GDK_CONTROL_MASK) {
        /* Ctrl+scroll = zoom */
        if (dy < 0)
            zoom_in();
        else
            zoom_out();
    } else {
        /* Scroll = page navigation when at boundaries */
        state.scroll_y += dy * 50;
        if (state.scroll_y > 100) {
            next_page();
            state.scroll_y = 0;
        } else if (state.scroll_y < -100) {
            prev_page();
            state.scroll_y = 0;
        }
    }
}

/* ─── File Drop Support ────────────────────────────────────────────────────── */

static gboolean on_drop(GtkDropTarget *target, const GValue *value,
                         double x, double y, gpointer data)
{
    (void)target; (void)x; (void)y; (void)data;

    if (G_VALUE_HOLDS(value, GDK_TYPE_FILE_LIST)) {
        GSList *files = g_value_get_boxed(value);
        if (files) {
            GFile *file = files->data;
            char *path = g_file_get_path(file);
            if (path) {
                open_file(path);
                g_free(path);
            }
        }
        return TRUE;
    }
    return FALSE;
}

/* ─── Toolbar Button Callbacks ─────────────────────────────────────────────── */

static void on_open_clicked(GtkButton *btn, gpointer data)
{
    (void)btn; (void)data;
    /* Simulate Ctrl+O */
    GdkModifierType mods = GDK_CONTROL_MASK;
    on_key_pressed(NULL, GDK_KEY_o, 0, mods, NULL);
}

static void on_prev_clicked(GtkButton *btn, gpointer data) { (void)btn; (void)data; prev_page(); }
static void on_next_clicked(GtkButton *btn, gpointer data) { (void)btn; (void)data; next_page(); }
static void on_zoom_in_clicked(GtkButton *btn, gpointer data) { (void)btn; (void)data; zoom_in(); }
static void on_zoom_out_clicked(GtkButton *btn, gpointer data) { (void)btn; (void)data; zoom_out(); }

/* ─── Application Setup ───────────────────────────────────────────────────── */

static void activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;

    /* Avoid re-creating the window if already activated */
    if (state.window) {
        gtk_window_present(GTK_WINDOW(state.window));
        return;
    }

    /* Window */
    state.window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(state.window), "SumatraPDF");
    gtk_window_set_default_size(GTK_WINDOW(state.window), 900, 700);

    /* Header bar with toolbar */
    state.header_bar = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(state.window), state.header_bar);

    /* Open button */
    GtkWidget *open_btn = gtk_button_new_from_icon_name("document-open-symbolic");
    gtk_widget_set_tooltip_text(open_btn, "Open (Ctrl+O)");
    g_signal_connect(open_btn, "clicked", G_CALLBACK(on_open_clicked), NULL);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(state.header_bar), open_btn);

    /* Navigation buttons */
    GtkWidget *nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(nav_box, "linked");

    GtkWidget *prev_btn = gtk_button_new_from_icon_name("go-previous-symbolic");
    gtk_widget_set_tooltip_text(prev_btn, "Previous Page (PgUp)");
    g_signal_connect(prev_btn, "clicked", G_CALLBACK(on_prev_clicked), NULL);
    gtk_box_append(GTK_BOX(nav_box), prev_btn);

    state.page_label = gtk_label_new(" 0 / 0 ");
    gtk_widget_set_size_request(state.page_label, 80, -1);
    gtk_box_append(GTK_BOX(nav_box), state.page_label);

    GtkWidget *next_btn = gtk_button_new_from_icon_name("go-next-symbolic");
    gtk_widget_set_tooltip_text(next_btn, "Next Page (PgDn)");
    g_signal_connect(next_btn, "clicked", G_CALLBACK(on_next_clicked), NULL);
    gtk_box_append(GTK_BOX(nav_box), next_btn);

    gtk_header_bar_pack_start(GTK_HEADER_BAR(state.header_bar), nav_box);

    /* Zoom buttons */
    GtkWidget *zoom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(zoom_box, "linked");

    GtkWidget *zout_btn = gtk_button_new_from_icon_name("zoom-out-symbolic");
    gtk_widget_set_tooltip_text(zout_btn, "Zoom Out (Ctrl+-)");
    g_signal_connect(zout_btn, "clicked", G_CALLBACK(on_zoom_out_clicked), NULL);
    gtk_box_append(GTK_BOX(zoom_box), zout_btn);

    state.zoom_label = gtk_label_new(" 150% ");
    gtk_widget_set_size_request(state.zoom_label, 60, -1);
    gtk_box_append(GTK_BOX(zoom_box), state.zoom_label);

    GtkWidget *zin_btn = gtk_button_new_from_icon_name("zoom-in-symbolic");
    gtk_widget_set_tooltip_text(zin_btn, "Zoom In (Ctrl++)");
    g_signal_connect(zin_btn, "clicked", G_CALLBACK(on_zoom_in_clicked), NULL);
    gtk_box_append(GTK_BOX(zoom_box), zin_btn);

    gtk_header_bar_pack_end(GTK_HEADER_BAR(state.header_bar), zoom_box);

    /* Drawing area */
    state.drawing_area = gtk_drawing_area_new();
    gtk_widget_set_hexpand(state.drawing_area, TRUE);
    gtk_widget_set_vexpand(state.drawing_area, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(state.drawing_area),
                                    on_draw, NULL, NULL);
    gtk_window_set_child(GTK_WINDOW(state.window), state.drawing_area);

    /* Keyboard controller */
    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_key_pressed), NULL);
    gtk_widget_add_controller(state.window, key_ctrl);

    /* Scroll controller */
    GtkEventController *scroll_ctrl = gtk_event_controller_scroll_new(
        GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect(scroll_ctrl, "scroll", G_CALLBACK(on_scroll), NULL);
    gtk_widget_add_controller(state.drawing_area, scroll_ctrl);

    /* Drag-and-drop support */
    GtkDropTarget *drop = gtk_drop_target_new(GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
    g_signal_connect(drop, "drop", G_CALLBACK(on_drop), NULL);
    gtk_widget_add_controller(state.window, GTK_EVENT_CONTROLLER(drop));

    gtk_window_present(GTK_WINDOW(state.window));
}

/* ─── GApplication "open" handler ──────────────────────────────────────────── */

static void on_app_open(GApplication *app, GFile **files, int n_files,
                        const char *hint, gpointer data)
{
    (void)hint; (void)data;

    /* Activate the window first */
    activate(GTK_APPLICATION(app), NULL);

    if (n_files > 0) {
        char *path = g_file_get_path(files[0]);
        if (path) {
            open_file(path);
            g_free(path);
        }
    }
}

/* ─── Main ─────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    /* Initialize MuPDF with larger store for big documents */
    state.ctx = fz_new_context(NULL, NULL, 512 << 20);
    if (!state.ctx) {
        fprintf(stderr, "error: cannot create mupdf context\n");
        return 1;
    }

    fz_try(state.ctx) {
        fz_register_document_handlers(state.ctx);
    }
    fz_catch(state.ctx) {
        fprintf(stderr, "error: %s\n", fz_caught_message(state.ctx));
        fz_drop_context(state.ctx);
        return 1;
    }

    state.zoom = 1.5f;

    /* Create GTK application — HANDLES_OPEN lets GLib pass file args properly */
    state.app = gtk_application_new("org.sumatrapdf.SumatraPDF",
                                     G_APPLICATION_NON_UNIQUE |
                                     G_APPLICATION_HANDLES_OPEN);
    g_signal_connect(state.app, "activate", G_CALLBACK(activate), NULL);
    g_signal_connect(state.app, "open", G_CALLBACK(on_app_open), NULL);

    int status = g_application_run(G_APPLICATION(state.app), argc, argv);

    /* Cleanup */
    close_document();
    g_object_unref(state.app);
    fz_drop_context(state.ctx);

    return status;
}
