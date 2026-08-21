// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#ifndef MUPDF_PKCS7_OPENSSL_H
#define MUPDF_PKCS7_OPENSSL_H

#include "mupdf/pdf/document.h"
#include "mupdf/pdf/form.h"

/* This an example pkcs7 implementation using openssl. These are the types of functions that you
 * will likely need to sign documents and check signatures within documents. In particular, to
 * sign a document, you need a function that derives a pdf_pkcs7_signer object from a certificate
 * stored by the operating system or within a file. */

/* Check a signature's digest against ranges of bytes drawn from a stream */
pdf_signature_error pkcs7_openssl_check_digest(fz_context *ctx, fz_stream *stm, char *sig, size_t sig_len);

/* Check a signature's certificate is trusted */
pdf_signature_error pkcs7_openssl_check_certificate(char *sig, size_t sig_len);

/* Obtain the distinguished name information from signature's certificate */
pdf_pkcs7_distinguished_name *pkcs7_openssl_distinguished_name(fz_context *ctx, char *sig, size_t sig_len);

/* Read the certificate and private key from a pfx file, holding it as an opaque structure */
pdf_pkcs7_signer *pkcs7_openssl_read_pfx(fz_context *ctx, const char *pfile, const char *pw);

/* Read the certificate and private key from a buffer, holding it as an opaque structure */
pdf_pkcs7_signer *pkcs7_openssl_read_pfx_from_buffer(fz_context *ctx, fz_buffer *buf, const char *pw);

pdf_pkcs7_verifier *pkcs7_openssl_new_verifier(fz_context *ctx);

#endif
