// Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
// License: Simplified BSD (see COPYING.BSD)
//
// Verify-only pkcs7 backend built on Windows CryptoAPI. Mirrors the
// pdf_pkcs7_verifier vtable defined in mupdf/pdf/form.h.

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include "mupdf/helpers/pkcs7-windows.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#include <ncrypt.h>
#include <string.h>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ncrypt.lib")

// ---- envelope parsing helpers --------------------------------------------

// Return the exact length of the outer ASN.1 SEQUENCE at `data`, so callers
// can strip trailing zero padding from a PDF /Contents signature placeholder
// before feeding it to CryptMsg (CryptoAPI rejects the envelope with
// CRYPT_E_MSG_ERROR if bytes follow the top-level SEQUENCE).
//
// Returns 0 if the header is malformed or uses BER indefinite-length
// encoding (0x80); in that case the caller should feed the full buffer.
static size_t asn1_outer_seq_len(const unsigned char* data, size_t max_len) {
    if (max_len < 2 || data[0] != 0x30) {
        return 0;
    }
    unsigned char b = data[1];
    size_t hdr;
    size_t content;
    if (b < 0x80) {
        hdr = 2;
        content = b;
    } else if (b == 0x80) {
        // indefinite length; don't try to find the 00 00 terminator
        return 0;
    } else {
        size_t n = b & 0x7F;
        if (n == 0 || n > sizeof(size_t) || max_len < 2 + n) {
            return 0;
        }
        content = 0;
        for (size_t i = 0; i < n; i++) {
            content = (content << 8) | data[2 + i];
        }
        hdr = 2 + n;
    }
    size_t total = hdr + content;
    if (total > max_len) {
        return 0;
    }
    return total;
}

// Trim a /Contents-style signature buffer down to its outer ASN.1
// SEQUENCE length, stripping the trailing zero padding that PDF writers
// leave when the actual PKCS#7 blob is shorter than the reserved placeholder.
static size_t trim_sig(unsigned char* sig, size_t sig_len) {
    size_t trimmed = asn1_outer_seq_len(sig, sig_len);
    return trimmed ? trimmed : sig_len;
}

// Parse a PKCS#7 envelope for metadata queries only (signer info + certs).
// We deliberately don't use CMSG_DETACHED_FLAG here: the envelope itself is
// self-describing (SignedData with an optional encapsulated content that's
// absent for detached sigs), so a single finalizing CryptMsgUpdate is enough
// to make CMSG_SIGNER_COUNT_PARAM / CMSG_SIGNER_INFO_PARAM / CertOpenStore
// work. This path cannot verify the digest; see windows_check_digest.
static HCRYPTMSG open_msg_for_metadata(unsigned char* sig, size_t sig_len) {
    sig_len = trim_sig(sig, sig_len);
    HCRYPTMSG hMsg = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING | X509_ASN_ENCODING, 0, 0, 0, NULL, NULL);
    if (!hMsg) {
        return NULL;
    }
    if (!CryptMsgUpdate(hMsg, sig, (DWORD)sig_len, TRUE)) {
        CryptMsgClose(hMsg);
        return NULL;
    }
    return hMsg;
}

// Fetch CMSG_SIGNER_INFO at the given signer index. Caller LocalFrees.
static PCMSG_SIGNER_INFO get_signer_info(HCRYPTMSG hMsg, DWORD idx) {
    DWORD cb = 0;
    if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, idx, NULL, &cb)) {
        return NULL;
    }
    PCMSG_SIGNER_INFO si = (PCMSG_SIGNER_INFO)LocalAlloc(LPTR, cb);
    if (!si) {
        return NULL;
    }
    if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, idx, si, &cb)) {
        LocalFree(si);
        return NULL;
    }
    return si;
}

static DWORD get_signer_count(HCRYPTMSG hMsg) {
    DWORD count = 0;
    DWORD cb = sizeof(count);
    if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_COUNT_PARAM, 0, &count, &cb)) {
        return 0;
    }
    return count;
}

// Windows crypto error codes we give symbolic names to. Names are stored as
// a double-NUL terminated SeqStrings blob; gleCodes[i] pairs with the i-th
// name in gleNames. Keep the two tables in the same order.
// clang-format off
static const char gleNames[] =
    "NTE_BAD_SIGNATURE\0"
    "NTE_BAD_HASH\0"
    "NTE_BAD_KEY\0"
    "NTE_BAD_LEN\0"
    "NTE_BAD_ALGID\0"
    "NTE_BAD_TYPE\0"
    "NTE_BAD_DATA\0"
    "NTE_NO_MEMORY\0"
    "CRYPT_E_MSG_ERROR\0"
    "CRYPT_E_UNKNOWN_ALGO\0"
    "CRYPT_E_OID_FORMAT\0"
    "CRYPT_E_INVALID_MSG_TYPE\0"
    "CRYPT_E_UNEXPECTED_ENCODING\0"
    "CRYPT_E_AUTH_ATTR_MISSING\0"
    "CRYPT_E_HASH_VALUE\0"
    "CRYPT_E_INVALID_INDEX\0"
    "CRYPT_E_ALREADY_DECRYPTED\0"
    "CRYPT_E_NOT_DECRYPTED\0"
    "CRYPT_E_RECIPIENT_NOT_FOUND\0"
    "CRYPT_E_CONTROL_TYPE\0"
    "CRYPT_E_ISSUER_SERIALNUMBER\0"
    "CRYPT_E_SIGNER_NOT_FOUND\0"
    "CRYPT_E_ATTRIBUTES_MISSING\0"
    "CRYPT_E_UNEXPECTED_MSG_TYPE\0"
    "CRYPT_E_NO_SIGNER\0"
    "CRYPT_E_NO_MATCH\0"
    "CRYPT_E_BAD_ENCODE\0"
    "CRYPT_E_OSS_ERROR\0"
    "CRYPT_E_ASN1_ERROR\0"
    "CRYPT_E_ASN1_BADTAG\0"
    "CRYPT_E_NOT_FOUND\0"
    "ERROR_INVALID_PARAMETER\0"
    "ERROR_MORE_DATA\0";

static const DWORD gleCodes[] = {
    (DWORD)NTE_BAD_SIGNATURE,
    (DWORD)NTE_BAD_HASH,
    (DWORD)NTE_BAD_KEY,
    (DWORD)NTE_BAD_LEN,
    (DWORD)NTE_BAD_ALGID,
    (DWORD)NTE_BAD_TYPE,
    (DWORD)NTE_BAD_DATA,
    (DWORD)NTE_NO_MEMORY,
    (DWORD)CRYPT_E_MSG_ERROR,
    (DWORD)CRYPT_E_UNKNOWN_ALGO,
    (DWORD)CRYPT_E_OID_FORMAT,
    (DWORD)CRYPT_E_INVALID_MSG_TYPE,
    (DWORD)CRYPT_E_UNEXPECTED_ENCODING,
    (DWORD)CRYPT_E_AUTH_ATTR_MISSING,
    (DWORD)CRYPT_E_HASH_VALUE,
    (DWORD)CRYPT_E_INVALID_INDEX,
    (DWORD)CRYPT_E_ALREADY_DECRYPTED,
    (DWORD)CRYPT_E_NOT_DECRYPTED,
    (DWORD)CRYPT_E_RECIPIENT_NOT_FOUND,
    (DWORD)CRYPT_E_CONTROL_TYPE,
    (DWORD)CRYPT_E_ISSUER_SERIALNUMBER,
    (DWORD)CRYPT_E_SIGNER_NOT_FOUND,
    (DWORD)CRYPT_E_ATTRIBUTES_MISSING,
    (DWORD)CRYPT_E_UNEXPECTED_MSG_TYPE,
    (DWORD)CRYPT_E_NO_SIGNER,
    (DWORD)CRYPT_E_NO_MATCH,
    (DWORD)CRYPT_E_BAD_ENCODE,
    (DWORD)CRYPT_E_OSS_ERROR,
    (DWORD)CRYPT_E_ASN1_ERROR,
    (DWORD)CRYPT_E_ASN1_BADTAG,
    (DWORD)CRYPT_E_NOT_FOUND,
    (DWORD)ERROR_INVALID_PARAMETER,
    (DWORD)ERROR_MORE_DATA,
};
// clang-format on

// Returns symbolic name for known Windows crypto error codes, or NULL if
// unknown. Walks gleNames / gleCodes in parallel; stops at the SeqStrings
// terminator so the two tables can't walk past each other.
static const char* gle_name(DWORD err) {
    const char* s = gleNames;
    size_t i = 0;
    while (*s && i < sizeof(gleCodes) / sizeof(gleCodes[0])) {
        if (gleCodes[i] == err) {
            return s;
        }
        s += strlen(s) + 1;
        i++;
    }
    return NULL;
}

static void warn_gle(fz_context* ctx, const char* where, DWORD err) {
    const char* name = gle_name(err);
    if (name) {
        fz_warn(ctx, "pkcs7-windows %s: %s (0x%08lX)", where, name, err);
    } else {
        fz_warn(ctx, "pkcs7-windows %s: gle=0x%08lX", where, err);
    }
}

// Locate the signer's certificate inside the envelope's embedded cert set.
static PCCERT_CONTEXT find_signer_cert(HCERTSTORE hStore, PCMSG_SIGNER_INFO si) {
    CERT_INFO ci;
    ZeroMemory(&ci, sizeof(ci));
    ci.Issuer = si->Issuer;
    ci.SerialNumber = si->SerialNumber;
    return CertFindCertificateInStore(hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SUBJECT_CERT, &ci,
                                      NULL);
}

// ---- check_certificate ---------------------------------------------------

static pdf_signature_error windows_check_certificate(fz_context* ctx, pdf_pkcs7_verifier* vf, unsigned char* sig,
                                                     size_t sig_len) {
    (void)ctx;
    (void)vf;
    pdf_signature_error rc = PDF_SIGNATURE_ERROR_UNKNOWN;
    HCRYPTMSG hMsg = NULL;
    HCERTSTORE hStore = NULL;
    PCMSG_SIGNER_INFO si = NULL;
    PCCERT_CONTEXT cert = NULL;
    PCCERT_CHAIN_CONTEXT chain = NULL;

    hMsg = open_msg_for_metadata(sig, sig_len);
    if (!hMsg) {
        warn_gle(ctx, "check_certificate parse envelope", GetLastError());
        goto done;
    }
    if (get_signer_count(hMsg) == 0) {
        rc = PDF_SIGNATURE_ERROR_NO_SIGNATURES;
        goto done;
    }
    hStore = CertOpenStore(CERT_STORE_PROV_MSG, 0, 0, 0, hMsg);
    if (!hStore) {
        warn_gle(ctx, "check_certificate CertOpenStore", GetLastError());
        goto done;
    }
    si = get_signer_info(hMsg, 0);
    if (!si) {
        warn_gle(ctx, "check_certificate get_signer_info", GetLastError());
        goto done;
    }
    cert = find_signer_cert(hStore, si);
    if (!cert) {
        rc = PDF_SIGNATURE_ERROR_NO_CERTIFICATE;
        goto done;
    }

    CERT_CHAIN_PARA chainPara;
    ZeroMemory(&chainPara, sizeof(chainPara));
    chainPara.cbSize = sizeof(chainPara);
    // No revocation check: matches openssl helper behavior (and Adobe)
    if (!CertGetCertificateChain(NULL, cert, NULL, hStore, &chainPara, 0, NULL, &chain)) {
        warn_gle(ctx, "check_certificate CertGetCertificateChain", GetLastError());
        goto done;
    }

    DWORD status = chain->TrustStatus.dwErrorStatus;
    if (status == CERT_TRUST_NO_ERROR) {
        rc = PDF_SIGNATURE_ERROR_OKAY;
    } else if (status & CERT_TRUST_IS_UNTRUSTED_ROOT) {
        // Distinguish pure self-signed (one-element chain) from a chain
        // whose root we simply don't trust.
        BOOL isSelfSigned = FALSE;
        if (chain->cChain > 0 && chain->rgpChain[0]->cElement == 1) {
            isSelfSigned = TRUE;
        }
        rc = isSelfSigned ? PDF_SIGNATURE_ERROR_SELF_SIGNED : PDF_SIGNATURE_ERROR_SELF_SIGNED_IN_CHAIN;
    } else {
        rc = PDF_SIGNATURE_ERROR_NOT_TRUSTED;
    }

done:
    if (chain) {
        CertFreeCertificateChain(chain);
    }
    if (cert) {
        CertFreeCertificateContext(cert);
    }
    if (hStore) {
        CertCloseStore(hStore, 0);
    }
    if (si) {
        LocalFree(si);
    }
    if (hMsg) {
        CryptMsgClose(hMsg);
    }
    return rc;
}

// ---- check_digest --------------------------------------------------------

static pdf_signature_error windows_check_digest(fz_context* ctx, pdf_pkcs7_verifier* vf, fz_stream* stm,
                                                unsigned char* sig, size_t sig_len) {
    (void)vf;
    pdf_signature_error rc = PDF_SIGNATURE_ERROR_UNKNOWN;
    fz_buffer* content = NULL;
    PCCERT_CONTEXT signerCert = NULL;

    fz_var(content);
    fz_try(ctx) {
        content = fz_read_all(ctx, stm, 4096);
    }
    fz_catch(ctx) {
        fz_warn(ctx, "pkcs7-windows check_digest: fz_read failed: %s", fz_caught_message(ctx));
        return PDF_SIGNATURE_ERROR_UNKNOWN;
    }

    sig_len = trim_sig(sig, sig_len);

    CRYPT_VERIFY_MESSAGE_PARA para;
    ZeroMemory(&para, sizeof(para));
    para.cbSize = sizeof(para);
    para.dwMsgAndCertEncodingType = PKCS_7_ASN_ENCODING | X509_ASN_ENCODING;

    const BYTE* pbToBeSigned[1];
    pbToBeSigned[0] = content->data;
    DWORD cbToBeSigned[1];
    cbToBeSigned[0] = (DWORD)content->len;

    // CryptVerifyDetachedMessageSignature wraps up CryptMsgOpenToDecode +
    // CryptMsgUpdate + CMSG_CTRL_VERIFY_SIGNATURE into one call and tends
    // to be more tolerant of non-strict-DER envelopes that PDF writers
    // occasionally produce (indefinite-length BER, extra-padded placeholders).
    if (CryptVerifyDetachedMessageSignature(&para, 0, sig, (DWORD)sig_len, 1, pbToBeSigned, cbToBeSigned,
                                            &signerCert)) {
        rc = PDF_SIGNATURE_ERROR_OKAY;
    } else {
        DWORD err = GetLastError();
        if (err == (DWORD)NTE_BAD_SIGNATURE || err == (DWORD)CRYPT_E_HASH_VALUE) {
            rc = PDF_SIGNATURE_ERROR_DIGEST_FAILURE;
        } else if (err == (DWORD)CRYPT_E_NO_SIGNER) {
            rc = PDF_SIGNATURE_ERROR_NO_SIGNATURES;
        }
        warn_gle(ctx, "check_digest CryptVerifyDetachedMessageSignature", err);
    }

    if (signerCert) {
        CertFreeCertificateContext(signerCert);
    }
    fz_drop_buffer(ctx, content);
    return rc;
}

// ---- get_signatory -------------------------------------------------------

// Copy a named attribute from the cert Subject into an fz-owned UTF-8
// C string. Returns NULL when the attribute is empty or missing.
//
// We go via CertGetNameStringW (not ...A) because the ANSI variant
// returns bytes in the active system code page -- e.g. a Portuguese
// "Joao da Silva" signer with 'ã' / 'á' comes back as single-byte
// 0xE3 / 0xE1, which then renders as mojibake once the rest of the
// SumatraPDF UI treats it as UTF-8. Convert UTF-16 -> UTF-8 here so
// callers get well-formed UTF-8 regardless of the signer's locale.
static char* get_name_string(fz_context* ctx, PCCERT_CONTEXT cert, LPCSTR oid) {
    DWORD n = CertGetNameStringW(cert, CERT_NAME_ATTR_TYPE, 0, (void*)oid, NULL, 0);
    if (n <= 1) {
        return NULL;
    }
    WCHAR* wbuf = fz_malloc(ctx, n * sizeof(WCHAR));
    CertGetNameStringW(cert, CERT_NAME_ATTR_TYPE, 0, (void*)oid, wbuf, n);
    int u8len = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, NULL, 0, NULL, NULL);
    if (u8len <= 1) {
        fz_free(ctx, wbuf);
        return NULL;
    }
    char* buf = fz_malloc(ctx, (size_t)u8len);
    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, buf, u8len, NULL, NULL);
    fz_free(ctx, wbuf);
    return buf;
}

static pdf_pkcs7_distinguished_name* windows_get_signatory(fz_context* ctx, pdf_pkcs7_verifier* vf, unsigned char* sig,
                                                           size_t sig_len) {
    (void)vf;
    HCRYPTMSG hMsg = NULL;
    HCERTSTORE hStore = NULL;
    PCMSG_SIGNER_INFO si = NULL;
    PCCERT_CONTEXT cert = NULL;
    pdf_pkcs7_distinguished_name* dn = NULL;

    hMsg = open_msg_for_metadata(sig, sig_len);
    if (!hMsg) {
        warn_gle(ctx, "get_signatory parse envelope", GetLastError());
        goto done;
    }
    if (get_signer_count(hMsg) == 0) {
        goto done;
    }
    hStore = CertOpenStore(CERT_STORE_PROV_MSG, 0, 0, 0, hMsg);
    if (!hStore) {
        warn_gle(ctx, "get_signatory CertOpenStore", GetLastError());
        goto done;
    }
    si = get_signer_info(hMsg, 0);
    if (!si) {
        warn_gle(ctx, "get_signatory get_signer_info", GetLastError());
        goto done;
    }
    cert = find_signer_cert(hStore, si);
    if (!cert) {
        goto done;
    }

    dn = fz_malloc_struct(ctx, pdf_pkcs7_distinguished_name);
    fz_try(ctx) {
        dn->cn = get_name_string(ctx, cert, szOID_COMMON_NAME);
        dn->o = get_name_string(ctx, cert, szOID_ORGANIZATION_NAME);
        dn->ou = get_name_string(ctx, cert, szOID_ORGANIZATIONAL_UNIT_NAME);
        dn->email = get_name_string(ctx, cert, szOID_RSA_emailAddr);
        dn->c = get_name_string(ctx, cert, szOID_COUNTRY_NAME);
    }
    fz_catch(ctx) {
        pdf_signature_drop_distinguished_name(ctx, dn);
        dn = NULL;
    }

done:
    if (cert) {
        CertFreeCertificateContext(cert);
    }
    if (hStore) {
        CertCloseStore(hStore, 0);
    }
    if (si) {
        LocalFree(si);
    }
    if (hMsg) {
        CryptMsgClose(hMsg);
    }
    return dn;
}

// ---- verifier vtable -----------------------------------------------------

static void windows_drop_verifier(fz_context* ctx, pdf_pkcs7_verifier* vf) {
    fz_free(ctx, vf);
}

pdf_pkcs7_verifier* pkcs7_windows_new_verifier(fz_context* ctx) {
    pdf_pkcs7_verifier* vf = fz_malloc_struct(ctx, pdf_pkcs7_verifier);
    vf->drop = windows_drop_verifier;
    vf->check_certificate = windows_check_certificate;
    vf->check_digest = windows_check_digest;
    vf->get_signatory = windows_get_signatory;
    return vf;
}

// ---- flat convenience wrappers (mirror the openssl helper surface) -------

pdf_signature_error pkcs7_windows_check_certificate(char* sig, size_t sig_len) {
    return windows_check_certificate(NULL, NULL, (unsigned char*)sig, sig_len);
}

pdf_signature_error pkcs7_windows_check_digest(fz_context* ctx, fz_stream* stm, char* sig, size_t sig_len) {
    return windows_check_digest(ctx, NULL, stm, (unsigned char*)sig, sig_len);
}

pdf_pkcs7_distinguished_name* pkcs7_windows_distinguished_name(fz_context* ctx, char* sig, size_t sig_len) {
    return windows_get_signatory(ctx, NULL, (unsigned char*)sig, sig_len);
}

// ---- signing -------------------------------------------------------------

typedef struct {
    pdf_pkcs7_signer base;
    int refs;
    HCERTSTORE hStore;   // owns the PFX store
    PCCERT_CONTEXT cert; // signer cert, with embedded private-key provider info
} windows_signer;

static pdf_pkcs7_signer* windows_keep_signer(fz_context* ctx, pdf_pkcs7_signer* signer) {
    (void)ctx;
    windows_signer* s = (windows_signer*)signer;
    s->refs++;
    return signer;
}

static void windows_drop_signer(fz_context* ctx, pdf_pkcs7_signer* signer) {
    if (!signer) {
        return;
    }
    windows_signer* s = (windows_signer*)signer;
    if (--s->refs > 0) {
        return;
    }
    if (s->cert) {
        CertFreeCertificateContext(s->cert);
    }
    if (s->hStore) {
        CertCloseStore(s->hStore, 0);
    }
    fz_free(ctx, s);
}

static pdf_pkcs7_distinguished_name* windows_get_signing_name(fz_context* ctx, pdf_pkcs7_signer* signer) {
    windows_signer* s = (windows_signer*)signer;
    pdf_pkcs7_distinguished_name* dn = fz_malloc_struct(ctx, pdf_pkcs7_distinguished_name);
    fz_try(ctx) {
        dn->cn = get_name_string(ctx, s->cert, szOID_COMMON_NAME);
        dn->o = get_name_string(ctx, s->cert, szOID_ORGANIZATION_NAME);
        dn->ou = get_name_string(ctx, s->cert, szOID_ORGANIZATIONAL_UNIT_NAME);
        dn->email = get_name_string(ctx, s->cert, szOID_RSA_emailAddr);
        dn->c = get_name_string(ctx, s->cert, szOID_COUNTRY_NAME);
    }
    fz_catch(ctx) {
        pdf_signature_drop_distinguished_name(ctx, dn);
        fz_rethrow(ctx);
    }
    return dn;
}

// Build a detached PKCS#7 SignedData over the bytes from `in` and write the
// DER blob to `digest`. Matches the openssl helper's semantics:
//   digest==NULL                   → size query (return bytes required)
//   digest!=NULL, digest_len big   → sign, return actual bytes written
//   digest!=NULL, digest_len small → return 0
// Passing in==NULL is a size query over empty content (used by max_digest_size).
static int windows_create_digest(fz_context* ctx, pdf_pkcs7_signer* signer, fz_stream* in, unsigned char* digest,
                                 size_t digest_len) {
    windows_signer* s = (windows_signer*)signer;
    fz_buffer* buf = NULL;
    int res = 0;

    fz_var(buf);
    fz_try(ctx) {
        const BYTE* content = (const BYTE*)"";
        DWORD contentLen = 0;
        if (in != NULL) {
            buf = fz_read_all(ctx, in, 4096);
            content = buf->data;
            contentLen = (DWORD)buf->len;
        }

        CRYPT_SIGN_MESSAGE_PARA para;
        ZeroMemory(&para, sizeof(para));
        para.cbSize = sizeof(para);
        para.dwMsgEncodingType = PKCS_7_ASN_ENCODING | X509_ASN_ENCODING;
        para.pSigningCert = s->cert;
        para.HashAlgorithm.pszObjId = (LPSTR)szOID_NIST_sha256;
        para.cMsgCert = 1;
        para.rgpMsgCert = &s->cert;

        const BYTE* pbToBeSigned[1];
        pbToBeSigned[0] = content;
        DWORD cbToBeSigned[1];
        cbToBeSigned[0] = contentLen;

        DWORD sigLen = 0;
        if (!CryptSignMessage(&para, TRUE, 1, pbToBeSigned, cbToBeSigned, NULL, &sigLen)) {
            fz_throw(ctx, FZ_ERROR_GENERIC, "CryptSignMessage size query failed (gle=%lu)", GetLastError());
        }

        if (digest == NULL) {
            res = (int)sigLen;
        } else if (sigLen > digest_len) {
            res = 0; // caller's buffer isn't big enough
        } else {
            if (!CryptSignMessage(&para, TRUE, 1, pbToBeSigned, cbToBeSigned, digest, &sigLen)) {
                fz_throw(ctx, FZ_ERROR_GENERIC, "CryptSignMessage failed (gle=%lu)", GetLastError());
            }
            res = (int)sigLen;
        }
    }
    fz_always(ctx) {
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
    return res;
}

static size_t windows_max_digest_size(fz_context* ctx, pdf_pkcs7_signer* signer) {
    // Detached PKCS#7 size doesn't depend on signed content; query with no
    // content and use that as the placeholder reservation.
    return (size_t)windows_create_digest(ctx, signer, NULL, NULL, 0);
}

// Convert UTF-8 C string to newly-allocated wide (UTF-16). Returns NULL for
// NULL input. Caller fz_free's the result.
static WCHAR* utf8_to_wide(fz_context* ctx, const char* s) {
    if (!s) {
        return NULL;
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) {
        fz_throw(ctx, FZ_ERROR_GENERIC, "MultiByteToWideChar failed (gle=%lu)", GetLastError());
    }
    WCHAR* w = (WCHAR*)fz_calloc(ctx, (size_t)n, sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static pdf_pkcs7_signer* pkcs7_windows_read_pfx_imp(fz_context* ctx, const char* pfile, fz_buffer* pfxBuf,
                                                    const char* pw) {
    windows_signer* signer = NULL;
    fz_buffer* owned = NULL;
    HCERTSTORE hStore = NULL;
    PCCERT_CONTEXT cert = NULL;
    WCHAR* pwW = NULL;

    fz_var(signer);
    fz_var(owned);
    fz_var(hStore);
    fz_var(cert);
    fz_var(pwW);

    fz_try(ctx) {
        if (!pfxBuf) {
            owned = fz_read_file(ctx, pfile);
            pfxBuf = owned;
        }

        CRYPT_DATA_BLOB blob;
        blob.cbData = (DWORD)pfxBuf->len;
        blob.pbData = pfxBuf->data;

        pwW = utf8_to_wide(ctx, pw ? pw : "");

        hStore = PFXImportCertStore(&blob, pwW, CRYPT_USER_KEYSET | CRYPT_EXPORTABLE);
        if (!hStore) {
            fz_throw(ctx, FZ_ERROR_GENERIC, "PFXImportCertStore failed (gle=%lu)", GetLastError());
        }

        cert = CertFindCertificateInStore(hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_HAS_PRIVATE_KEY,
                                          NULL, NULL);
        if (!cert) {
            fz_throw(ctx, FZ_ERROR_GENERIC, "PFX has no cert with a private key");
        }

        // Validate at load time: acquire the key once, release it. CryptSignMessage
        // reacquires via the cert's stored CERT_KEY_PROV_INFO_PROP_ID.
        HCRYPTPROV_OR_NCRYPT_KEY_HANDLE hKey = 0;
        DWORD keySpec = 0;
        BOOL mustFree = FALSE;
        if (!CryptAcquireCertificatePrivateKey(cert, CRYPT_ACQUIRE_COMPARE_KEY_FLAG, NULL, &hKey, &keySpec,
                                               &mustFree)) {
            fz_throw(ctx, FZ_ERROR_GENERIC, "CryptAcquireCertificatePrivateKey failed (gle=%lu)", GetLastError());
        }
        if (mustFree) {
            if (keySpec == CERT_NCRYPT_KEY_SPEC) {
                NCryptFreeObject((NCRYPT_KEY_HANDLE)hKey);
            } else {
                CryptReleaseContext((HCRYPTPROV)hKey, 0);
            }
        }

        signer = fz_malloc_struct(ctx, windows_signer);
        signer->base.keep = windows_keep_signer;
        signer->base.drop = windows_drop_signer;
        signer->base.get_signing_name = windows_get_signing_name;
        signer->base.max_digest_size = windows_max_digest_size;
        signer->base.create_digest = windows_create_digest;
        signer->refs = 1;
        signer->hStore = hStore;
        signer->cert = cert;
        // ownership transferred to signer — null out locals so fz_catch doesn't free them
        hStore = NULL;
        cert = NULL;
    }
    fz_always(ctx) {
        fz_free(ctx, pwW);
        fz_drop_buffer(ctx, owned);
    }
    fz_catch(ctx) {
        if (cert) {
            CertFreeCertificateContext(cert);
        }
        if (hStore) {
            CertCloseStore(hStore, 0);
        }
        fz_rethrow(ctx);
    }
    return &signer->base;
}

pdf_pkcs7_signer* pkcs7_windows_read_pfx(fz_context* ctx, const char* pfile, const char* pw) {
    return pkcs7_windows_read_pfx_imp(ctx, pfile, NULL, pw);
}

pdf_pkcs7_signer* pkcs7_windows_read_pfx_from_buffer(fz_context* ctx, fz_buffer* buf, const char* pw) {
    return pkcs7_windows_read_pfx_imp(ctx, NULL, buf, pw);
}
