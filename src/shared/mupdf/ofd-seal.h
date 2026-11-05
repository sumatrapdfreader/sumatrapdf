/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

typedef enum {
    OFD_SEAL_RASTER,
    OFD_SEAL_OFD
} ofd_seal_type;

fz_buffer* ofd_seal_picture(fz_context* ctx, fz_buffer* signed_value, ofd_seal_type* type);
