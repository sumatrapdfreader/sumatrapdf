#ifndef _MUPDF_EXPORTS_H_
#define _MUPDF_EXPORTS_H_

// Re-declare variables for when building libmupdf.dll,
// as exporting/importing them prevents sharing of .obj
// files for all files using them - instead we can just
// link this file along libmupdf.lib and omit it when
// building a static SumatraPDF.exe.

extern "C" {
#include <fitz.h>
}

// copied from mupdf/fitz/base_geometry.c

const fz_matrix fz_identity = { 1, 0, 0, 1, 0, 0 };

const fz_rect fz_infinite_rect = { 1, 1, -1, -1 };
const fz_rect fz_empty_rect = { 0, 0, 0, 0 };
const fz_rect fz_unit_rect = { 0, 0, 1, 1 };

const fz_irect fz_infinite_irect = { 1, 1, -1, -1 };
const fz_irect fz_empty_irect = { 0, 0, 0, 0 };
const fz_irect fz_unit_bbox = { 0, 0, 1, 1 };

#endif
