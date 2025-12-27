# Change Log

For the full list of changes between versions of MuPDF see
[CHANGES](https://github.com/ArtifexSoftware/mupdf/blob/master/CHANGES).

Major changes to *existing* interfaces are listed here.

## Why does the API change?

From time to time, during development, it becomes clear that the public API
needs to change; either because the existing API has flaws in it, or to
accommodate new functionality. Such changes are not undertaken lightly, and
they are kept as minimal as possible.

The alternative would be to freeze the API and to introduce more and more
compatibility veneers, ultimately leading to a bloated API and additional
complexity. We have done this in the past, and will do it again in future if
circumstances demand it, but we consider small changes in the API to be a price
worth paying for clarity and simplicity.

To minimise the impact of such changes, we undertake to list the API changes
between different versions of the library here, to make it as simple as
possible for integrators to make the small changes that may be require to
update between versions.


> The changes listed below only affect *existing* APIs.

## Changes from 1.26 to 1.27

- Use `fz_quad` in `fz_warp_pixmap`, `fz_autowarp_pixmap`, and `fz_detect_document`.

## Changes from 1.25 to 1.26

- `fz_skew_detect` renamed `fz_detect_skew`.
- `Filespec` renamed `FileSpec` to be consistent across bindings and documentation:
    - `PDFAnnotation.hasFilespec` renamed `PDFAnnotation.hasFileSpec`
    - `PDFAnnotation.getFilespec` renamed `PDFAnnotation.getFileSpec`
    - `PDFAnnotation.setFilespec` renamed `PDFAnnotation.setFileSpec`
    - `PDFDocument.getFilespecParams` renamed `PDFDocument.getFileSpecParams`.
    - `PDFObject.isFilespec` renamed `PDFObject.isFileSpec`.
- `StrokeState.getDashes` renamed `StrokeState.getDashPattern`.


## Changes from 1.24 to 1.25

- New `wants_file` member for `fz_document_handler`.
- `fz_text_item` now has an explicit pen `advance` member.
- `pdf_annot_rect` works with the "design" rectangle.
- mutool recolor can now be used as a function.
- structured text "color" field renamed to "argb" and now includes alpha channel


## Changes from 1.23 to 1.24

- You must call `pdf_report_error` in the final `fz_catch`. Any unreported errors will be automatically reported when a new error is raised, or when closing the fitz context.
- The deprecated `pdf_check_signature` was removed.


## Changes from 1.22 to 1.23

- `pdf_field_name` renamed to `pdf_load_field_name`.
- mutool run changed many methods to match Java and new WASM library.


## Changes from 1.21 to 1.22

None.


## Changes from 1.20 to 1.21

None.


## Changes from 1.19 to 1.20

Text search API extended to be able to distinguish between separate search hits.


## Changes from 1.18 to 1.19

We were inconsistent with the behaviour of `fz_create_` and `pdf_create_` functions, in that sometimes they returned a borrowed reference, rather than a reference that the caller had to drop. We now always return a reference that the caller owns and must drop.


## Changes from 1.17 to 1.18

None.



## Changes from 1.16 to 1.17

The accessors for reading and creating `QuadPoints`, `InkList` and `Vertices` data for Highlight, Underline, StrikeOut, Squiggle, Ink, Polygon, and PolyLine annotation types have been simplified. They now take and return `fz_quad` and `fz_point` structs instead of float arrays. We have also added functions to construct the datastructures one piece at a time, removing the need to allocate a temporary array to pass.

- `typedef struct fz_quad { fz_point ul, ur, ll, lr; };`

To facilitate loading EPUB documents without laying out the entire document at once, we have introduced chapters into the document structure. Each document instead of having just a plain list of pages, now has a list of chapters, and each chapter has a list of pages. Most of the old functions have the same functionality, but we have added several new functions, which if used, will provide significant speedups when jumping to a random page in large EPUB documents.

- `int fz_count_chapters(ctx, doc)`
- `int fz_count_chapter_pages(ctx, doc, chapter)`
- `fz_page *fz_load_chapter_page(ctx, doc, chapter, page)`
- `int fz_search_chapter_page_number(ctx, doc, chapter, page, needle, hit_quads, hit_size)`

However, in order to support bookmarks and links using chapters we have had to introduce a new struct `fz_location`, and the functions to resolve links have changed to use this new struct.

- `typedef struct { int chapter, page; } fz_location;`
- `fz_location fz_resolve_link(ctx, doc, uri, x, y);`

Functions to map between a fixed page number and a `fz_location` have been added to help facilitate migration to the new API.

- `int fz_page_number_from_location(ctx, doc, location);`
- `fz_location fz_location_from_page_number(ctx, doc, number);`
- `fz_location fz_next_page(ctx, doc, location);`
- `fz_location fz_previous_page(ctx, doc, location);`
- `fz_location fz_last_page(ctx, doc);`

The layout information for each chapter can at your option be cached in an "accelerator" file for even faster loading. This will help performance when using the old page number based rather than location based functions.

- `fz_document fz_open_accelerated_document(ctx, filename, accelerator_filename);`
- `void fz_save_accelerator(ctx, document, accelerator_filename);`


## Changes from 1.15 to 1.16

There has been a major overhaul of the color management architecture. Unless you're implementing custom devices or interpreters, this should only have a minor impact.

- Argument order when passing color and colorspace to functions regularized: sourceColorspace, sourceColorArray, destinationColorspace, destinationColorArray, proofColorspace, colorParams.
- Pass `fz_color_params` argument by value.
- Changed `fz_default_parameters` from a function to a global constant.
- Replaced `fz_set_icc_engine` with `fz_enable_icc` and `fz_disable_icc` to toggle color management at runtime.
- Replaced pixmap color converter struct with a single `fz_convert_pixmap` function call.
- Replaced `fz_cal_colorspace` struct with constructor to create an ICC-backed calibrated colorspace directly.
- Passing `NULL` is not a shortcut for DeviceGray any more!
- Added public definitions for Indexed and Separation colorspaces.
- Changed colorspace constructors.


The `fz_set_stdout` and `fz_set_stderr` functions have been removed. If you were using these to capture warning and error messages, use the new user callbacks for warning and error logging instead: `fz_set_warning_callback` and `fz_set_error_callback`.

The structured text html and xhtml output formats take an additional argument: the page number. This number is used to create an id attribute for each page to use as a hyperlink target.



## Changes from 1.14 to 1.15

- PDF Portfolios
   This functionality has been removed. We do not believe anyone was using this. If you were, please contact us for assistance. This functionality can be achieved using "mutool run" and `docs/examples/pdf-portfolio.js`.
- `FZ_ERROR_TRYLATER`
   This functionality has been removed. We do not believe anyone was using this. If you were, please contact us for assistance.
- Annotations/Forms
   We are undertaking a significant rework of this functionality at the moment. We do not believe anyone is using this at the moment, and would therefore encourage anyone who is to contact us for help in upgrading.
- Various functions involving `fz_colorspace` have lost consts.
   `fz_colorspaces` are immutable once created, other than changes due to reference counting. Passing a const `fz_colorspace` to a function that might keep a reference to it either has to take a non const `fz_colorspace` pointer, or take a const one, and 'break' the const. Having some functions take const `fz_colorspace` and some not is confusing, so therefore, for simplicity, all `fz_colorspaces` are now passed as non const. This should not affect any user code.
- `fz_process_shade()`
   This now takes an extra 'scissor' argument. To upgrade old code, if you don't have an appropriate scissor rect available, it is safe (but unwise) to pass `fz_infinite_rect`.
- `fz_tint_pixmap()`
   Rather than taking `r`, `g` and `b`, values to tint with, the function now takes 8 bit hex rgb values for black and white, enabling greater control, allowing "low contrast" and "night view" effects.
- `pdf_add_image()`
   This no longer requires a mask flag. The image already knows if it is a mask.
- `pdf_processor.op_BI()`
   The `op_BI` callback is now passed an additional colorspace resource name.
