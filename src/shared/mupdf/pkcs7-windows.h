// Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
// License: Simplified BSD (see COPYING.BSD)
//
// Windows native crypto backend for mupdf PDF signature handling.
// Implemented on top of the Win32 CryptoAPI (crypt32.dll). Uses only
// stock Windows facilities; no external dependencies.

#ifndef MUPDF_PKCS7_WINDOWS_H
#define MUPDF_PKCS7_WINDOWS_H

#include "mupdf/pdf/document.h"
#include "mupdf/pdf/form.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

pdf_signature_error pkcs7_windows_check_digest(fz_context* ctx, fz_stream* stm, char* sig, size_t sig_len);
pdf_signature_error pkcs7_windows_check_certificate(char* sig, size_t sig_len);
pdf_pkcs7_distinguished_name* pkcs7_windows_distinguished_name(fz_context* ctx, char* sig, size_t sig_len);

pdf_pkcs7_verifier* pkcs7_windows_new_verifier(fz_context* ctx);

pdf_pkcs7_signer* pkcs7_windows_read_pfx(fz_context* ctx, const char* pfile, const char* pw);
pdf_pkcs7_signer* pkcs7_windows_read_pfx_from_buffer(fz_context* ctx, fz_buffer* buf, const char* pw);
pdf_pkcs7_signer* pkcs7_windows_read_store(fz_context* ctx, const char* thumbprint_hex);

typedef struct {
    char* signer_cn;
    char* issuer_cn;
    char* hash_algo;
    char* policy_oid;
    int64_t gen_time_unix;
    int64_t not_after_unix;
    unsigned char* cert_der;
    int cert_der_len;
} pkcs7_windows_ts_info;

#define PKCS7_WINDOWS_MAX_TS 8

typedef enum {
    PKCS7_QC_TYPE_NONE,
    PKCS7_QC_TYPE_ESIGN,
    PKCS7_QC_TYPE_ESEAL,
    PKCS7_QC_TYPE_WEB,
} pkcs7_windows_qc_type;

// non-zero if der is a trust anchor; the value ends up in eu_status
typedef int (*pkcs7_windows_anchor_fn)(void* ud, const unsigned char* der, int der_len);

typedef struct {
    // the chain reaches a root Windows trusts, with no errors
    int os_trusted;
    // what anchor_fn said about the certificate the chain reached; 0 if none
    int eu_status;
    // the chain up to that anchor has no errors of its own
    int eu_chain_ok;
    int self_signed;
    int untrusted_root;
    // the signer's certificate wasn't valid at the time checked
    int not_time_valid;
    // anything else wrong: a bad signature in the chain, wrong usage, ...
    int other_error;
    int no_certificate;
} pkcs7_windows_chain_info;

typedef struct {
    char* signer_cn;
    char* issuer_cn;
    char* hash_algo;
    char* sig_algo;
    char* digest_hex;
    char* policy_oid;
    int64_t gen_time_unix;
    int64_t not_after_unix;
    int has_qc_statement;
    // from the qcStatements extension (ETSI EN 319 412-5)
    int qc_compliance;
    int qc_sscd;
    pkcs7_windows_qc_type qc_type;
    int has_cades_attr;
    int has_sig_policy_attr;
    unsigned char* cert_der;
    int cert_der_len;
    int has_timestamp;
    int n_ts;
    pkcs7_windows_ts_info ts[PKCS7_WINDOWS_MAX_TS];
} pkcs7_windows_sig_info;

void pkcs7_windows_sig_info_clear(pkcs7_windows_sig_info* info);
void pkcs7_windows_sig_info_free(fz_context* ctx, pkcs7_windows_sig_info* info);
void pkcs7_windows_check_chain(fz_context* ctx, unsigned char* sig, size_t sig_len, int64_t when_unix,
                               void* extra_store, pkcs7_windows_anchor_fn anchor_fn, void* anchor_ud,
                               pkcs7_windows_chain_info* out);
int pkcs7_windows_inspect(fz_context* ctx, unsigned char* sig, size_t sig_len, pkcs7_windows_sig_info* info);

#ifdef __cplusplus
}
#endif

#endif
