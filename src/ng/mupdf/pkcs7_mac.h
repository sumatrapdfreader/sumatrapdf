/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef MUPDF_PKCS7_MAC_H
#define MUPDF_PKCS7_MAC_H

#include "mupdf/pdf/document.h"
#include "mupdf/pdf/form.h"

pdf_pkcs7_signer* pkcs7_mac_read_pfx(fz_context* ctx, const char* pfile, const char* pw);

#endif
