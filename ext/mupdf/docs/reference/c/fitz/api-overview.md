# API Overview

The MuPDF `fitz` library is the core C API for rendering and manipulating PDF,
XPS, EPUB, and other document formats. The public API is exposed through the
`include/mupdf/fitz/` header files, all of which are collected under the
umbrella header `mupdf/fitz.h`.

The library is organised into functional groups:

- **Core** – context, geometry, system utilities
- **I/O** – buffers, streams, compression, archives
- **Resources** – pixmaps, images, bitmaps, fonts, colour spaces, paths
- **Rendering** – devices, display lists, structured text
- **Document** – documents, pages, links, outlines, bookmarks
- **Utilities** – high-level helpers combining the above

> **Note:** All functions that allocate memory or may fail take an `fz_context *ctx`
> as their first argument. Errors are signalled via `setjmp`/`longjmp`
> exception handling; callers should wrap operations in
> `fz_try` / `fz_catch` blocks.

> 📒 For more see the full {doc}`../api/index`.

---

## mupdf/fitz.h – Umbrella Header

```c
#include "mupdf/fitz.h"
```

This single header pulls in the entire public fitz API. It is the recommended
way to include MuPDF in your project.

**Include groups** (in order of declaration):

- Core: `version.h`, `config.h`, `system.h`, `context.h`, `output.h`, `log.h`
- Utilities: `crypt.h`, `getopt.h`, `geometry.h`, `hash.h`, `pool.h`,
  `string-util.h`, `tree.h`, `bidi.h`, `xml.h`, `json.h`, `hyphen.h`
- I/O: `buffer.h`, `stream.h`, `compress.h`, `compressed-buffer.h`,
  `filter.h`, `archive.h`, `heap.h`
- Resources: `store.h`, `color.h`, `pixmap.h`, `image.h`, `bitmap.h`,
  `shade.h`, `font.h`, `path.h`, `text.h`, `separation.h`, `glyph.h`
- Rendering: `device.h`, `display-list.h`, `structured-text.h`,
  `transition.h`, `glyph-cache.h`
- Document: `link.h`, `outline.h`, `document.h`

---

## context.h – Context & Error Handling

```c
#include "mupdf/fitz/context.h"
```

The `fz_context` is the central state object passed to virtually every fitz
function. It holds the allocator, exception stack, and resource caches.

### Exception Macros

```c
fz_try(ctx)      { /* code that may throw */ }
fz_catch(ctx)    { /* handle the error */ }
fz_always(ctx)   { /* always executed (cleanup) */ }

fz_throw(ctx, FZ_ERROR_GENERIC, "format %s", msg);
fz_rethrow(ctx);
fz_warn(ctx, "warning: %s", msg);
```

### Error codes (selected)

| Constant | Meaning |
|---|---|
| `FZ_ERROR_NONE` | No error. |
| `FZ_ERROR_GENERIC` | General runtime error. |
| `FZ_ERROR_ARGUMENT` | Invalid argument passed to a function. |
| `FZ_ERROR_MEMORY` | Memory allocation failure. |
| `FZ_ERROR_IO` | I/O failure. |
| `FZ_ERROR_FORMAT` | Document format error. |

### Context lifecycle

**`fz_context *fz_new_context(const fz_alloc_context *alloc, const fz_locks_context *locks, size_t max_store)`**

Create a new fitz context.

- `alloc` – Custom allocator, or `NULL` for the system default.
- `locks` – Locking callbacks for thread safety, or `NULL`.
- `max_store` – Maximum size of the resource store in bytes (e.g. `FZ_STORE_DEFAULT`).
- Returns: A new `fz_context`, or `NULL` on failure.

---

**`fz_context *fz_clone_context(fz_context *ctx)`**

Clone a context for use in a new thread. Shares the same resource store
and locks as the original.

- `ctx` – The context to clone.
- Returns: A new context; must be freed with `fz_drop_context`.

---

**`void fz_drop_context(fz_context *ctx)`**

Free a context and all associated resources. Never throws exceptions.

- `ctx` – The context to destroy.

### Memory helpers

**`void *fz_malloc(fz_context *ctx, size_t size)`**

Allocate `size` bytes. Throws `FZ_ERROR_MEMORY` on failure.

---

**`void *fz_calloc(fz_context *ctx, size_t count, size_t size)`**

Allocate and zero-initialise `count * size` bytes.

---

**`void fz_free(fz_context *ctx, void *p)`**

Free memory previously allocated with `fz_malloc` / `fz_calloc`.

---

## geometry.h – Geometric Types

```c
#include "mupdf/fitz/geometry.h"
```

### Types

**`fz_point`** – A 2-D point.

```c
typedef struct { float x, y; } fz_point;
```

**`fz_rect`** – An axis-aligned rectangle with `float` coordinates.

```c
typedef struct { float x0, y0, x1, y1; } fz_rect;
```

The `fz_empty_rect` and `fz_infinite_rect` constants are provided for convenience.

**`fz_irect`** – An axis-aligned rectangle with `int` coordinates (used for pixmap bounds).

```c
typedef struct { int x0, y0, x1, y1; } fz_irect;
```

**`fz_matrix`** – A 3×2 affine transformation matrix.

```c
typedef struct { float a, b, c, d, e, f; } fz_matrix;
```

Pre-defined matrices: `fz_identity`.

**`fz_quad`** – A quadrilateral (four corners), used for text hit quads.

```c
typedef struct { fz_point ul, ur, ll, lr; } fz_quad;
```

### Selected functions

**`fz_matrix fz_scale(float sx, float sy)`** – Create a scaling matrix.

**`fz_matrix fz_rotate(float degrees)`** – Create a rotation matrix (counter-clockwise, in degrees).

**`fz_matrix fz_translate(float tx, float ty)`** – Create a translation matrix.

**`fz_matrix fz_concat(fz_matrix left, fz_matrix right)`** – Concatenate two matrices (`left` is applied first).

**`fz_matrix fz_invert_matrix(fz_matrix m)`** – Invert a matrix. Returns `fz_identity` if the matrix is singular.

**`fz_point fz_transform_point(fz_point p, fz_matrix m)`** – Transform a point by a matrix.

**`fz_rect fz_transform_rect(fz_rect r, fz_matrix m)`** – Transform a rectangle by a matrix (returns the bounding box of the result).

**`fz_rect fz_intersect_rect(fz_rect a, fz_rect b)`** – Return the intersection of two rectangles.

**`fz_rect fz_union_rect(fz_rect a, fz_rect b)`** – Return the smallest rectangle containing both `a` and `b`.

**`fz_irect fz_round_rect(fz_rect r)`** – Convert a float rect to an integer rect by rounding outwards.

**`int fz_is_empty_rect(fz_rect r)`** – Return non-zero if the rectangle is empty.

**`int fz_is_infinite_rect(fz_rect r)`** – Return non-zero if the rectangle is the infinite rectangle.

---

## archive.h – Archive Access

```c
#include "mupdf/fitz/archive.h"
```

An `fz_archive` provides a uniform interface over ZIP files, TAR files, and
plain directories.

**`fz_archive`** – Opaque handle to an archive.

---

**`fz_archive *fz_open_archive(fz_context *ctx, const char *filename)`**

Open a ZIP or TAR archive by filename. The archive type is detected
automatically from the file signature.

- `ctx` – Active fitz context.
- `filename` – Path to the archive file.
- Returns: A new `fz_archive` whose ownership is returned to the caller.
- Raises: `FZ_ERROR_IO` if the file cannot be opened.
- Raises: `FZ_ERROR_FORMAT` if the file format is not recognised.

---

**`fz_archive *fz_open_archive_with_stream(fz_context *ctx, fz_stream *file)`** – Open an archive from an existing stream.

**`fz_archive *fz_open_directory(fz_context *ctx, const char *path)`** – Open a directory as an archive.

**`int fz_is_directory(fz_context *ctx, fz_archive *arch)`** – Return non-zero if the archive is backed by a directory.

**`int fz_has_archive_entry(fz_context *ctx, fz_archive *arch, const char *name)`** – Return non-zero if `name` exists in the archive.

**`int fz_count_archive_entries(fz_context *ctx, fz_archive *arch)`** – Return the number of entries in the archive.

**`const char *fz_list_archive_entry(fz_context *ctx, fz_archive *arch, int idx)`** – Return the name of entry `idx`.

**`fz_stream *fz_open_archive_entry(fz_context *ctx, fz_archive *arch, const char *name)`** – Open an archive entry as a stream.

**`fz_buffer *fz_read_archive_entry(fz_context *ctx, fz_archive *arch, const char *name)`** – Read an archive entry entirely into a buffer.

**`void fz_drop_archive(fz_context *ctx, fz_archive *arch)`** – Release an archive.

---

## pixmap.h – Raster Images (Pixmaps)

```c
#include "mupdf/fitz/pixmap.h"
```

A `fz_pixmap` represents a raster image: a rectangular region of pixels, each
with *n* colour components (process colours + spot colours + optional alpha) stored
contiguously in row-major order. Samples are in premultiplied alpha during
rendering, but non-premultiplied for colorspace conversions.

**`fz_pixmap`** – Opaque pixmap type. Key fields accessible via accessor functions:

- width / height (pixels)
- x / y origin (in device space)
- n (components per pixel)
- alpha (1 if alpha channel present, else 0)
- stride (bytes per row)
- samples (`unsigned char *` pixel data)
- colorspace (`fz_colorspace *`)
- xres / yres (resolution in dpi)

### Creating pixmaps

**`fz_pixmap *fz_new_pixmap(fz_context *ctx, fz_colorspace *cs, int w, int h, fz_separations *seps, int alpha)`**

Allocate a pixmap of size `w × h`.

- `cs` – Colour space (or `NULL` for alpha-only).
- `seps` – Spot separations, or `NULL`.
- `alpha` – Non-zero to include an alpha channel.

---

**`fz_pixmap *fz_new_pixmap_with_bbox(fz_context *ctx, fz_colorspace *cs, fz_irect bbox, fz_separations *seps, int alpha)`** – Allocate a pixmap covering `bbox` (sets the x/y origin accordingly).

**`fz_pixmap *fz_new_pixmap_with_data(fz_context *ctx, fz_colorspace *cs, int w, int h, fz_separations *seps, int alpha, int stride, unsigned char *samples)`** – Wrap an existing sample buffer; the buffer is **not** freed on drop.

**`fz_pixmap *fz_new_pixmap_with_bbox_and_data(fz_context *ctx, fz_colorspace *cs, fz_irect bbox, fz_separations *seps, int alpha, unsigned char *samples)`** – Wrap an existing buffer with a bounding box origin.

### Reference counting

**`fz_pixmap *fz_keep_pixmap(fz_context *ctx, fz_pixmap *pix)`** – Increment the reference count. Returns the same pointer. Never throws exceptions.

**`void fz_drop_pixmap(fz_context *ctx, fz_pixmap *pix)`** – Decrement the reference count; the pixmap is freed when it reaches zero. Never throws exceptions.

### Accessors

**`fz_irect fz_pixmap_bbox(fz_context *ctx, const fz_pixmap *pix)`** – Return the bounding box of the pixmap.

**`int fz_pixmap_width(fz_context *ctx, const fz_pixmap *pix)`** – Return the width in pixels.

**`int fz_pixmap_height(fz_context *ctx, const fz_pixmap *pix)`** – Return the height in pixels.

**`int fz_pixmap_x(fz_context *ctx, const fz_pixmap *pix)`** – Return the x origin of the pixmap (left edge, in device space).

**`int fz_pixmap_y(fz_context *ctx, const fz_pixmap *pix)`** – Return the y origin of the pixmap.

**`int fz_pixmap_stride(fz_context *ctx, const fz_pixmap *pix)`** – Return the stride (bytes per row).

**`int fz_pixmap_components(fz_context *ctx, const fz_pixmap *pix)`** – Return the total number of components (including alpha if present).

**`unsigned char *fz_pixmap_samples(fz_context *ctx, fz_pixmap *pix)`** – Return a pointer to the raw sample data.

**`fz_colorspace *fz_pixmap_colorspace(fz_context *ctx, fz_pixmap *pix)`** – Return the colour space of the pixmap (may be `NULL`).

### Manipulation

**`void fz_clear_pixmap(fz_context *ctx, fz_pixmap *pix)`** – Set all components (including alpha) of all pixels to zero.

**`void fz_clear_pixmap_with_value(fz_context *ctx, fz_pixmap *pix, int value)`** – Clear all pixels to `value` (0–255); alpha is always set to 255 (opaque).

**`void fz_fill_pixmap_with_color(fz_context *ctx, fz_pixmap *pix, fz_colorspace *cs, float *color, fz_color_params color_params)`** – Fill the pixmap with a solid colour.

**`void fz_copy_pixmap_rect(fz_context *ctx, fz_pixmap *dest, fz_pixmap *src, fz_irect r, const fz_default_colorspaces *default_cs)`** – Copy a rectangular region from `src` to `dest`.

**`fz_pixmap *fz_clone_pixmap(fz_context *ctx, const fz_pixmap *old)`** – Clone a pixmap to new storage. The reference count of `old` is unchanged.

---

**`fz_pixmap *fz_convert_pixmap(fz_context *ctx, const fz_pixmap *pix, fz_colorspace *cs_des, fz_colorspace *prf, fz_default_colorspaces *default_cs, fz_color_params color_params, int keep_alpha)`**

Convert the pixmap to a different colour space.

- `cs_des` – Destination colour space (`NULL` = alpha only).
- `prf` – Proofing colour space (or `NULL`).
- `keep_alpha` – If 0, alpha is stripped.

---

**`fz_pixmap *fz_scale_pixmap(fz_context *ctx, fz_pixmap *src, float x, float y, float w, float h, const fz_irect *clip)`** – Scale a pixmap to the given `w × h` size, optionally clipping.

**`void fz_invert_pixmap(fz_context *ctx, fz_pixmap *pix)`** – Invert all colour components (not alpha) in place.

**`void fz_gamma_pixmap(fz_context *ctx, fz_pixmap *pix, float gamma)`** – Apply gamma correction.

**`int fz_is_pixmap_monochrome(fz_context *ctx, fz_pixmap *pix)`** – Return non-zero if the pixmap contains only values 0 and 255.

---

## bitmap.h – 1-bit Bitmaps

```c
#include "mupdf/fitz/bitmap.h"
```

A `fz_bitmap` stores 1 bit per component, used for halftoned output (e.g. for
sending to a printer). Samples are stored MSB-first, compatible with PBM format.

**`fz_bitmap`**

```c
typedef struct {
    int refs;
    int w, h, stride, n;
    int xres, yres;
    unsigned char *samples;
} fz_bitmap;
```

**`fz_bitmap *fz_keep_bitmap(fz_context *ctx, fz_bitmap *bit)`** – Increment the reference count. Never throws exceptions.

**`void fz_drop_bitmap(fz_context *ctx, fz_bitmap *bit)`** – Decrement the reference count; bitmap is freed when it reaches zero. Never throws exceptions.

**`fz_bitmap *fz_new_bitmap_from_pixmap(fz_context *ctx, fz_pixmap *pix, fz_halftone *ht)`** – Convert a pixmap to a halftoned 1-bit bitmap.

**`fz_bitmap *fz_new_bitmap_from_pixmap_band(fz_context *ctx, fz_pixmap *pix, fz_halftone *ht, int band_start)`** – Same as above, rendering a single horizontal band.

---

## path.h – Vector Paths

```c
#include "mupdf/fitz/path.h"
```

Vector paths can be filled or stroked, and may have a dash pattern.

**`fz_path`** – Opaque path type.

**`fz_linecap`**

```c
typedef enum {
    FZ_LINECAP_BUTT     = 0,
    FZ_LINECAP_ROUND    = 1,
    FZ_LINECAP_SQUARE   = 2,
    FZ_LINECAP_TRIANGLE = 3,
} fz_linecap;
```

**`fz_linejoin`**

```c
typedef enum {
    FZ_LINEJOIN_MITER     = 0,
    FZ_LINEJOIN_ROUND     = 1,
    FZ_LINEJOIN_BEVEL     = 2,
    FZ_LINEJOIN_MITER_XPS = 3,
} fz_linejoin;
```

**`fz_stroke_state`** – Stroke parameters:

```c
typedef struct {
    int refs;
    fz_linecap start_cap, dash_cap, end_cap;
    fz_linejoin linejoin;
    float linewidth;
    float miterlimit;
    float dash_phase;
    int dash_len;
    float dash_list[FZ_FLEXIBLE_ARRAY];
} fz_stroke_state;
```

### Path construction

**`fz_path *fz_new_path(fz_context *ctx)`** – Create a new empty path.

**`fz_path *fz_keep_path(fz_context *ctx, const fz_path *path)`** – Increment the reference count.

**`void fz_drop_path(fz_context *ctx, const fz_path *path)`** – Decrement the reference count; frees when zero.

**`void fz_moveto(fz_context *ctx, fz_path *path, float x, float y)`** – Start a new subpath at `(x, y)`.

**`void fz_lineto(fz_context *ctx, fz_path *path, float x, float y)`** – Add a straight line segment to `(x, y)`.

**`void fz_curveto(fz_context *ctx, fz_path *path, float cx1, float cy1, float cx2, float cy2, float ex, float ey)`** – Add a cubic Bézier curve.

**`void fz_closepath(fz_context *ctx, fz_path *path)`** – Close the current subpath.

**`void fz_rectto(fz_context *ctx, fz_path *path, float x1, float y1, float x2, float y2)`** – Add a rectangle as a closed subpath.

**`fz_rect fz_bound_path(fz_context *ctx, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm)`** – Return the bounding box of a path (for fill or, if `stroke` is not NULL, stroke).

**`fz_path *fz_clone_path(fz_context *ctx, fz_path *old)`** – Return a writable clone of a (possibly shared) path.

### Stroke state

**`fz_stroke_state *fz_new_stroke_state(fz_context *ctx)`** – Allocate a default stroke state (1pt width, butt caps, miter join).

**`fz_stroke_state *fz_keep_stroke_state(fz_context *ctx, const fz_stroke_state *stroke)`** – Increment the reference count.

**`void fz_drop_stroke_state(fz_context *ctx, const fz_stroke_state *stroke)`** – Decrement the reference count; frees when zero.

---

## document.h – Documents & Pages

```c
#include "mupdf/fitz/document.h"
```

### Types

**`fz_document`** – Opaque document handle.

**`fz_page`** – Opaque page handle.

**`fz_location`** – A document location (chapter + page).

```c
typedef struct { int chapter, page; } fz_location;
```

**`fz_bookmark`** – An opaque bookmark (`intptr_t`).

**`fz_box_type`** – Page box type.

```c
typedef enum {
    FZ_MEDIA_BOX,
    FZ_CROP_BOX,
    FZ_BLEED_BOX,
    FZ_TRIM_BOX,
    FZ_ART_BOX,
    FZ_UNKNOWN_BOX,
} fz_box_type;
```

### Opening documents

**`fz_document *fz_open_document(fz_context *ctx, const char *filename)`**

Open a document from `filename`. The format is detected automatically.

- Returns: A new `fz_document`; ownership is returned to the caller.

---

**`fz_document *fz_open_document_with_stream(fz_context *ctx, const char *magic, fz_stream *stream)`** – Open a document from a stream, using `magic` (MIME type or file extension) for format detection.

**`fz_document *fz_open_accelerated_document(fz_context *ctx, const char *filename, const char *accel)`** – Open a document with an accelerator file (e.g. pre-computed linearisation).

**`void fz_drop_document(fz_context *ctx, fz_document *doc)`** – Release a document.

**`fz_document *fz_keep_document(fz_context *ctx, fz_document *doc)`** – Increment the reference count.

### Authentication

**`int fz_needs_password(fz_context *ctx, fz_document *doc)`** – Return non-zero if the document requires a password.

**`int fz_authenticate_password(fz_context *ctx, fz_document *doc, const char *password)`** – Attempt to authenticate with `password`. Returns non-zero on success.

### Layout

**`void fz_style_document(fz_context *ctx, fz_document *doc, int publisher_css, const char *user_css)`**

Control use of publisher and user stylesheets.

- `publisher_css` – Enable/disable use of publisher styles.
- `user_css` – User stylesheet (the contents; not a file name).

**`void fz_layout_document(fz_context *ctx, fz_document *doc, float w, float h, float em)`**

Set the page layout for reflowable documents (EPUB, FB2, etc.).

- `w` – Page width in points.
- `h` – Page height in points.
- `em` – Default em-size in points.

Pre-defined layout constants:

```c
FZ_LAYOUT_KINDLE_W   = 260   /* 6in 4:3 */
FZ_LAYOUT_KINDLE_H   = 346
FZ_LAYOUT_KINDLE_EM  = 9
FZ_LAYOUT_A5_W       = 420
FZ_LAYOUT_A5_H       = 595
FZ_LAYOUT_A5_EM      = 11
```

### Page counting & navigation

**`int fz_count_chapters(fz_context *ctx, fz_document *doc)`** – Return the number of chapters.

**`int fz_count_pages(fz_context *ctx, fz_document *doc)`** – Return the total number of pages (across all chapters).

**`int fz_count_chapter_pages(fz_context *ctx, fz_document *doc, int chapter)`** – Return the number of pages in a chapter.

**`fz_location fz_last_page(fz_context *ctx, fz_document *doc)`** – Return the location of the last page.

**`fz_location fz_next_page(fz_context *ctx, fz_document *doc, fz_location loc)`** – Return the next page location, wrapping across chapters.

**`fz_location fz_previous_page(fz_context *ctx, fz_document *doc, fz_location loc)`** – Return the previous page location.

**`int fz_page_number_from_location(fz_context *ctx, fz_document *doc, fz_location loc)`** – Convert a chapter/page location to a flat (zero-based) page number.

### Loading pages

**`fz_page *fz_load_page(fz_context *ctx, fz_document *doc, int number)`** – Load page by flat page number (zero-based).

**`fz_page *fz_load_chapter_page(fz_context *ctx, fz_document *doc, int chapter, int page)`** – Load a page by chapter and page index.

**`void fz_drop_page(fz_context *ctx, fz_page *page)`** – Release a page.

### Page bounds & rendering

**`fz_rect fz_bound_page(fz_context *ctx, fz_page *page)`** – Return the bounding box of the page (in points, with y increasing downward).

**`fz_rect fz_bound_page_box(fz_context *ctx, fz_page *page, fz_box_type box)`** – Return the bounding box of a specific page box type.

**`void fz_run_page(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie)`** – Render the page (content, annotations, and widgets) to a device.

**`void fz_run_page_contents(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie)`** – Render only the page content stream (no annotations/widgets).

**`void fz_run_page_annots(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie)`** – Render only the annotations.

**`void fz_run_page_widgets(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie)`** – Render only the widgets (interactive form fields).

### Metadata

**`int fz_lookup_metadata(fz_context *ctx, fz_document *doc, const char *key, char *buf, size_t size)`**

Look up a metadata string. Standard keys include `"format"`,
`"encryption"`, `"info:Title"`, `"info:Author"`, etc.

- Returns: Length of the value string (or -1 if not found).

---

**`void fz_set_metadata(fz_context *ctx, fz_document *doc, const char *key, const char *value)`** – Set a metadata entry (for formats that support it).

### Bookmarks

**`fz_bookmark fz_make_bookmark(fz_context *ctx, fz_document *doc, fz_location loc)`** – Create a persistent bookmark for the given location.

**`fz_location fz_lookup_bookmark(fz_context *ctx, fz_document *doc, fz_bookmark mark)`** – Resolve a bookmark to a location.

### Document handler registration

**`void fz_register_document_handler(fz_context *ctx, const fz_document_handler *handler)`** – Register a document format handler (e.g. a custom renderer).

**`void fz_register_document_handlers(fz_context *ctx)`** – Register all built-in document handlers (PDF, XPS, EPUB, etc.).

---

## util.h – High-Level Utilities

```c
#include "mupdf/fitz/util.h"
```

Convenience wrappers that combine several lower-level calls.

### Display lists

**`fz_display_list *fz_new_display_list_from_page(fz_context *ctx, fz_page *page)`** – Record the complete rendering of `page` into a reusable display list. Ownership of the returned display list is returned to the caller.

**`fz_display_list *fz_new_display_list_from_page_number(fz_context *ctx, fz_document *doc, int number)`** – As above, addressed by flat page number.

**`fz_display_list *fz_new_display_list_from_page_contents(fz_context *ctx, fz_page *page)`** – Record only the page content stream (no annotations).

### Rendering to pixmaps

**`fz_pixmap *fz_new_pixmap_from_page(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, int alpha)`**

Render `page` to a freshly-allocated pixmap in one step.

- `ctm` – View transform (typically a scale + rotation).
- `cs` – Output colour space (e.g. `fz_device_rgb(ctx)`).
- `alpha` – Non-zero to include an alpha channel.

---

**`fz_pixmap *fz_new_pixmap_from_page_number(fz_context *ctx, fz_document *doc, int number, fz_matrix ctm, fz_colorspace *cs, int alpha)`** – Render page by flat page number.

**`fz_pixmap *fz_new_pixmap_from_display_list(fz_context *ctx, fz_display_list *list, fz_matrix ctm, fz_colorspace *cs, int alpha)`** – Render a display list to a pixmap.

**`fz_pixmap *fz_new_pixmap_from_page_contents(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, int alpha)`** – Render page content only (no annotations).

### Search

**`int fz_search_page(fz_context *ctx, fz_page *page, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max)`**

Search for text on a page.

- `needle` – UTF-8 search string.
- `hit_mark` – Array of hit marks (length `hit_max`); filled on return.
- `hit_bbox` – Array of `fz_quad` hit rectangles (length `hit_max`).
- `hit_max` – Capacity of the hit arrays.
- Returns: Number of hits found (may be less than the actual count if capped).

---

**`int fz_search_page_number(fz_context *ctx, fz_document *doc, int number, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max)`** – Search by flat page number.

**`int fz_search_display_list(fz_context *ctx, fz_display_list *list, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max)`** – Search a display list.

---

## Quick-start Example

The following example renders page 0 of a PDF to a PNG file at 150 dpi:

```c
#include "mupdf/fitz.h"

int main(void)
{
    fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
    fz_register_document_handlers(ctx);

    fz_document *doc = NULL;
    fz_page     *page = NULL;
    fz_pixmap   *pix  = NULL;

    fz_try(ctx) {
        doc  = fz_open_document(ctx, "input.pdf");
        page = fz_load_page(ctx, doc, 0);

        /* 150 dpi = 150/72 scale */
        fz_matrix ctm = fz_scale(150.0f / 72, 150.0f / 72);
        pix = fz_new_pixmap_from_page(ctx, page, ctm,
                                      fz_device_rgb(ctx), 0);

        fz_save_pixmap_as_png(ctx, pix, "output.png");
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, pix);
        fz_drop_page(ctx, page);
        fz_drop_document(ctx, doc);
    }
    fz_catch(ctx) {
        fprintf(stderr, "error: %s\n", fz_caught_message(ctx));
    }

    fz_drop_context(ctx);
    return 0;
}
```

---

## Header File Index

The following headers make up the complete public fitz API:

| Header | Contents |
|---|---|
| `fitz/version.h` | Version constants (`FZ_VERSION`, `FZ_VERSION_MAJOR`, etc.) |
| `fitz/config.h` | Compile-time feature flags (`FZ_ENABLE_PDF`, etc.) |
| `fitz/system.h` | Platform portability macros and type aliases |
| `fitz/context.h` | Context lifecycle, error handling, memory allocation |
| `fitz/output.h` | Buffered output writers (`fz_output`) |
| `fitz/log.h` | Diagnostic logging |
| `fitz/geometry.h` | Points, rects, matrices, quads, and transformations |
| `fitz/buffer.h` | Growable byte buffers (`fz_buffer`) |
| `fitz/stream.h` | Seekable/non-seekable byte streams (`fz_stream`) |
| `fitz/compress.h` | Zlib/deflate compression helpers |
| `fitz/archive.h` | ZIP, TAR, and directory archives |
| `fitz/color.h` | Colour spaces and colour conversion |
| `fitz/pixmap.h` | Raster pixel data |
| `fitz/image.h` | Compressed image objects (`fz_image`) |
| `fitz/bitmap.h` | 1-bit halftoned bitmaps |
| `fitz/font.h` | Font objects and glyph metrics |
| `fitz/path.h` | Vector paths, stroke state |
| `fitz/text.h` | Glyph sequences for text rendering |
| `fitz/shade.h` | Gradient shading |
| `fitz/separation.h` | Spot colour separations |
| `fitz/device.h` | Rendering device interface |
| `fitz/display-list.h` | Recorded rendering command lists |
| `fitz/structured-text.h` | Extracted text blocks, lines, and characters |
| `fitz/link.h` | Hyperlinks (`fz_link`) |
| `fitz/outline.h` | Document outline / bookmarks tree |
| `fitz/document.h` | Document and page lifecycle, metadata |
| `fitz/util.h` | High-level rendering and search helpers |
| `fitz/transition.h` | Page transition effects |
| `fitz/xml.h` | Lightweight XML DOM |
| `fitz/json.h` | Lightweight JSON parser |
| `fitz/string-util.h` | String utility functions |
| `fitz/hash.h` | Simple hash table |
| `fitz/pool.h` | Memory pool allocator |
| `fitz/tree.h` | Balanced binary tree |
| `fitz/store.h` | Reference-counted resource store / cache |
| `fitz/crypt.h` | MD5, SHA, AES helpers |
| `fitz/getopt.h` | Portable `getopt` implementation |

---

## Notes

- All string parameters are UTF-8 encoded unless otherwise noted.
- Page numbers are **zero-based** throughout the API.
- The `fz_context` is **not** thread-safe; create one context per thread
  using `fz_clone_context` and share the resource store via locks.
- Memory management follows a **keep/drop** pattern:
  `fz_keep_*` increments a reference count, `fz_drop_*` decrements it
  and frees when the count reaches zero.
