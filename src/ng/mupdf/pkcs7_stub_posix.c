/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// murun.c and pdfsign.c call the Windows CryptoAPI helper. That file is
// Win32-only; these symbols let the tools link and report that signing is
// unavailable.

#include "mupdf/fitz.h"
#include "pkcs7-windows.h"

pdf_pkcs7_verifier* pkcs7_windows_new_verifier(fz_context* ctx) {
    (void)ctx;
    return NULL;
}

pdf_pkcs7_signer* pkcs7_windows_read_pfx(fz_context* ctx, const char* pfile, const char* pw) {
    (void)ctx;
    (void)pfile;
    (void)pw;
    return NULL;
}

pdf_pkcs7_signer* pkcs7_windows_read_pfx_from_buffer(fz_context* ctx, fz_buffer* buf, const char* pw) {
    (void)ctx;
    (void)buf;
    (void)pw;
    return NULL;
}
