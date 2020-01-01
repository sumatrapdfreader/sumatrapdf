/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Re-declare variables for when building libmupdf.dll,
// as exporting/importing them prevents sharing of .obj
// files for all files using them - instead we can just
// link this file along libmupdf.lib and omit it when
// building a static SumatraPDF.exe.

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4611)
#endif /* _MSC_VER */

extern "C" {
#include <mupdf/fitz/geometry.h>
}

// copied from mupdf/source/fitz/geometry.c

const fz_matrix fz_identity = {1, 0, 0, 1, 0, 0};

const fz_rect fz_infinite_rect = {1, 1, -1, -1};
const fz_rect fz_empty_rect = {0, 0, 0, 0};
const fz_rect fz_unit_rect = {0, 0, 1, 1};

const fz_irect fz_infinite_irect = {1, 1, -1, -1};
const fz_irect fz_empty_irect = {0, 0, 0, 0};
const fz_irect fz_unit_bbox = {0, 0, 1, 1};

extern "C" {
#include <mupdf/fitz/color.h>
}
// copied from colorspace.c
const fz_color_params fz_default_color_params = {FZ_RI_RELATIVE_COLORIMETRIC, 1, 0, 0};

// adapted for mupdf/source/fitz/time.c

#if 0
extern "C" void fz_redirect_dll_io_to_console() {
    // TODO(port):
    //fz_redirect_io_to_console();
}
#endif
