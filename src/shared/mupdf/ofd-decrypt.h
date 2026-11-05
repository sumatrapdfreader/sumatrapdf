/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

int ofd_is_encrypted(fz_context* ctx, fz_archive* archive);
fz_archive* ofd_decrypt_package(fz_context* ctx, fz_archive* archive, const char* password);
