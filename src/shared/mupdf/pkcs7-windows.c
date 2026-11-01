// Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
// License: Simplified BSD (see COPYING.BSD)
//
// PKCS#7 signing and verification with Windows CryptoAPI.

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include "pkcs7-windows.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#include <ncrypt.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ncrypt.lib")

// ---- envelope parsing helpers --------------------------------------------

static int asn1_read_len(const unsigned char* p, size_t n, size_t max_bytes, size_t* hdr, size_t* body) {
    if (n < 2) {
        return -1;
    }
    if (p[1] < 0x80) {
        *hdr = 2;
        *body = p[1];
    } else if (p[1] == 0x80) {
        return -1;
    } else {
        size_t ln = p[1] & 0x7F;
        size_t i;
        if (ln == 0 || ln > max_bytes || n < 2 + ln) {
            return -1;
        }
        *body = 0;
        for (i = 0; i < ln; i++) {
            *body = (*body << 8) | p[2 + i];
        }
        *hdr = 2 + ln;
    }
    if (*hdr + *body > n) {
        return -1;
    }
    return 0;
}

// Strip PDF signature padding only when the outer SEQUENCE has a definite length.
static size_t asn1_outer_seq_len(const unsigned char* data, size_t max_len) {
    size_t hdr, body;
    if (max_len < 2 || data[0] != 0x30 || asn1_read_len(data, max_len, sizeof(size_t), &hdr, &body) != 0) {
        return 0;
    }
    return hdr + body;
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

// Symbolic names for Windows crypto errors.
static const struct {
    DWORD code;
    const char* name;
} gleErrors[] = {
    {(DWORD)NTE_BAD_SIGNATURE, "NTE_BAD_SIGNATURE"},
    {(DWORD)NTE_BAD_HASH, "NTE_BAD_HASH"},
    {(DWORD)NTE_BAD_KEY, "NTE_BAD_KEY"},
    {(DWORD)NTE_BAD_LEN, "NTE_BAD_LEN"},
    {(DWORD)NTE_BAD_ALGID, "NTE_BAD_ALGID"},
    {(DWORD)NTE_BAD_TYPE, "NTE_BAD_TYPE"},
    {(DWORD)NTE_BAD_DATA, "NTE_BAD_DATA"},
    {(DWORD)NTE_NO_MEMORY, "NTE_NO_MEMORY"},
    {(DWORD)CRYPT_E_MSG_ERROR, "CRYPT_E_MSG_ERROR"},
    {(DWORD)CRYPT_E_UNKNOWN_ALGO, "CRYPT_E_UNKNOWN_ALGO"},
    {(DWORD)CRYPT_E_OID_FORMAT, "CRYPT_E_OID_FORMAT"},
    {(DWORD)CRYPT_E_INVALID_MSG_TYPE, "CRYPT_E_INVALID_MSG_TYPE"},
    {(DWORD)CRYPT_E_UNEXPECTED_ENCODING, "CRYPT_E_UNEXPECTED_ENCODING"},
    {(DWORD)CRYPT_E_AUTH_ATTR_MISSING, "CRYPT_E_AUTH_ATTR_MISSING"},
    {(DWORD)CRYPT_E_HASH_VALUE, "CRYPT_E_HASH_VALUE"},
    {(DWORD)CRYPT_E_INVALID_INDEX, "CRYPT_E_INVALID_INDEX"},
    {(DWORD)CRYPT_E_ALREADY_DECRYPTED, "CRYPT_E_ALREADY_DECRYPTED"},
    {(DWORD)CRYPT_E_NOT_DECRYPTED, "CRYPT_E_NOT_DECRYPTED"},
    {(DWORD)CRYPT_E_RECIPIENT_NOT_FOUND, "CRYPT_E_RECIPIENT_NOT_FOUND"},
    {(DWORD)CRYPT_E_CONTROL_TYPE, "CRYPT_E_CONTROL_TYPE"},
    {(DWORD)CRYPT_E_ISSUER_SERIALNUMBER, "CRYPT_E_ISSUER_SERIALNUMBER"},
    {(DWORD)CRYPT_E_SIGNER_NOT_FOUND, "CRYPT_E_SIGNER_NOT_FOUND"},
    {(DWORD)CRYPT_E_ATTRIBUTES_MISSING, "CRYPT_E_ATTRIBUTES_MISSING"},
    {(DWORD)CRYPT_E_UNEXPECTED_MSG_TYPE, "CRYPT_E_UNEXPECTED_MSG_TYPE"},
    {(DWORD)CRYPT_E_NO_SIGNER, "CRYPT_E_NO_SIGNER"},
    {(DWORD)CRYPT_E_NO_MATCH, "CRYPT_E_NO_MATCH"},
    {(DWORD)CRYPT_E_BAD_ENCODE, "CRYPT_E_BAD_ENCODE"},
    {(DWORD)CRYPT_E_OSS_ERROR, "CRYPT_E_OSS_ERROR"},
    {(DWORD)CRYPT_E_ASN1_ERROR, "CRYPT_E_ASN1_ERROR"},
    {(DWORD)CRYPT_E_ASN1_BADTAG, "CRYPT_E_ASN1_BADTAG"},
    {(DWORD)CRYPT_E_NOT_FOUND, "CRYPT_E_NOT_FOUND"},
    {(DWORD)ERROR_INVALID_PARAMETER, "ERROR_INVALID_PARAMETER"},
    {(DWORD)ERROR_MORE_DATA, "ERROR_MORE_DATA"},
};

static const char* gle_name(DWORD err) {
    for (size_t i = 0; i < sizeof(gleErrors) / sizeof(gleErrors[0]); i++) {
        if (gleErrors[i].code == err) {
            return gleErrors[i].name;
        }
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

static void close_metadata(HCRYPTMSG hMsg, HCERTSTORE hStore, PCMSG_SIGNER_INFO si, PCCERT_CONTEXT cert) {
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
    close_metadata(hMsg, hStore, si, cert);
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

// Return fz-owned UTF-8; the ANSI API would use the system code page.
static char* get_cert_name(fz_context* ctx, PCCERT_CONTEXT cert, DWORD type, DWORD flags, LPCSTR oid) {
    DWORD n = CertGetNameStringW(cert, type, flags, (void*)oid, NULL, 0);
    if (n <= 1) {
        return NULL;
    }
    WCHAR* wbuf = fz_malloc(ctx, n * sizeof(WCHAR));
    CertGetNameStringW(cert, type, flags, (void*)oid, wbuf, n);
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

static char* get_name_string(fz_context* ctx, PCCERT_CONTEXT cert, LPCSTR oid) {
    return get_cert_name(ctx, cert, CERT_NAME_ATTR_TYPE, 0, oid);
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
    close_metadata(hMsg, hStore, si, cert);
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

// Takes ownership of hStore and cert on success. On failure the caller still
// owns them. The private key is acquired once here to fail early; CryptSignMessage
// reacquires it via the cert's CERT_KEY_PROV_INFO_PROP_ID.
static pdf_pkcs7_signer* windows_make_signer(fz_context* ctx, HCERTSTORE hStore, PCCERT_CONTEXT cert) {
    HCRYPTPROV_OR_NCRYPT_KEY_HANDLE hKey = 0;
    DWORD keySpec = 0;
    BOOL mustFree = FALSE;
    // ALLOW_NCRYPT: CurrentUser\MY certs are often CNG (software or smart card).
    // COMPARE_KEY is CAPI-oriented and fails those with NTE_BAD_PROV_TYPE.
    if (!CryptAcquireCertificatePrivateKey(cert, CRYPT_ACQUIRE_ALLOW_NCRYPT_KEY_FLAG, NULL, &hKey, &keySpec,
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

    windows_signer* signer = fz_malloc_struct(ctx, windows_signer);
    signer->base.keep = windows_keep_signer;
    signer->base.drop = windows_drop_signer;
    signer->base.get_signing_name = windows_get_signing_name;
    signer->base.max_digest_size = windows_max_digest_size;
    signer->base.create_digest = windows_create_digest;
    signer->refs = 1;
    signer->hStore = hStore;
    signer->cert = cert;
    return &signer->base;
}

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

// Decode a SHA-1 thumbprint (40 hex digits, optional spaces or colons) into
// 20 bytes. Returns 0 on success.
static int parse_thumbprint(const char* hex, BYTE out[20]) {
    int n = 0;
    if (!hex) {
        return -1;
    }
    for (const char* p = hex; *p; p++) {
        if (*p == ' ' || *p == ':' || *p == '-') {
            continue;
        }
        int hi = hex_nibble(*p);
        if (hi < 0 || !p[1]) {
            return -1;
        }
        int lo = hex_nibble(*++p);
        if (lo < 0 || n >= 20) {
            return -1;
        }
        out[n++] = (BYTE)((hi << 4) | lo);
    }
    return n == 20 ? 0 : -1;
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

        signer = (windows_signer*)windows_make_signer(ctx, hStore, cert);
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

// Load a signing cert from the current user's Personal (MY) store by SHA-1
// thumbprint. The private key stays in the store; Windows will prompt for a
// PIN if the key container is protected.
pdf_pkcs7_signer* pkcs7_windows_read_store(fz_context* ctx, const char* thumbprint_hex) {
    windows_signer* signer = NULL;
    HCERTSTORE hStore = NULL;
    PCCERT_CONTEXT cert = NULL;
    BYTE hash[20];
    CRYPT_HASH_BLOB blob;

    fz_var(signer);
    fz_var(hStore);
    fz_var(cert);

    fz_try(ctx) {
        if (parse_thumbprint(thumbprint_hex, hash) != 0) {
            fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid certificate thumbprint");
        }
        hStore = CertOpenSystemStoreW(0, L"MY");
        if (!hStore) {
            fz_throw(ctx, FZ_ERROR_GENERIC, "CertOpenSystemStore failed (gle=%lu)", GetLastError());
        }
        blob.cbData = sizeof(hash);
        blob.pbData = hash;
        cert =
            CertFindCertificateInStore(hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_HASH, &blob, NULL);
        if (!cert) {
            fz_throw(ctx, FZ_ERROR_GENERIC, "certificate %s not found in the Windows certificate store",
                     thumbprint_hex ? thumbprint_hex : "");
        }
        signer = (windows_signer*)windows_make_signer(ctx, hStore, cert);
        hStore = NULL;
        cert = NULL;
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

// ---- inspect (algorithms, issuer, expiry, digest, RFC 3161 timestamp) ----

#ifndef szOID_RFC3161_counterSign
#define szOID_RFC3161_counterSign "1.2.840.113549.1.9.16.2.14"
#endif
#ifndef szOID_QC_STATEMENTS
#define szOID_QC_STATEMENTS "1.3.6.1.5.5.7.1.3"
#endif
#ifndef szOID_PKCS9_COUNTER_SIGNATURE
#define szOID_PKCS9_COUNTER_SIGNATURE "1.2.840.113549.1.9.6"
#endif

static int64_t filetime_to_unix(const FILETIME* ft) {
    ULARGE_INTEGER u;
    const uint64_t kEpochDiff = 11644473600ULL;
    if (!ft || (ft->dwLowDateTime == 0 && ft->dwHighDateTime == 0)) {
        return 0;
    }
    u.LowPart = ft->dwLowDateTime;
    u.HighPart = ft->dwHighDateTime;
    if (u.QuadPart < kEpochDiff * 10000000ULL) {
        return 0;
    }
    return (int64_t)(u.QuadPart / 10000000ULL - kEpochDiff);
}

static char* fz_dup_cstr(fz_context* ctx, const char* s) {
    if (!s || !s[0]) {
        return NULL;
    }
    return fz_strdup(ctx, s);
}

static const char* hash_algo_name(const char* oid) {
    if (!oid) {
        return NULL;
    }
    if (!strcmp(oid, szOID_OIWSEC_sha1) || !strcmp(oid, szOID_RSA_SHA1RSA)) {
        return "SHA1";
    }
    if (!strcmp(oid, szOID_NIST_sha256) || !strcmp(oid, szOID_RSA_SHA256RSA)) {
        return "SHA256";
    }
    if (!strcmp(oid, szOID_NIST_sha384) || !strcmp(oid, szOID_RSA_SHA384RSA)) {
        return "SHA384";
    }
    if (!strcmp(oid, szOID_NIST_sha512) || !strcmp(oid, szOID_RSA_SHA512RSA)) {
        return "SHA512";
    }
    if (!strcmp(oid, szOID_RSA_MD5) || !strcmp(oid, szOID_RSA_MD5RSA)) {
        return "MD5";
    }
    return NULL;
}

static const char* sig_algo_name(const char* oid) {
    if (!oid) {
        return NULL;
    }
    if (!strcmp(oid, szOID_RSA_RSA) || !strcmp(oid, szOID_RSA_SHA1RSA) || !strcmp(oid, szOID_RSA_SHA256RSA) ||
        !strcmp(oid, szOID_RSA_SHA384RSA) || !strcmp(oid, szOID_RSA_SHA512RSA) || !strcmp(oid, szOID_RSA_MD5RSA)) {
        return "RSA with PKCS n\xC2\xBA 1 v.1.5";
    }
    if (!strcmp(oid, szOID_RSA_SSA_PSS) || !strcmp(oid, "1.2.840.113549.1.1.10")) {
        return "RSA-PSS";
    }
    if (!strcmp(oid, szOID_ECC_PUBLIC_KEY) || !strncmp(oid, "1.2.840.10045.4.", 16) ||
        !strcmp(oid, "1.2.840.10045.2.1")) {
        return "ECDSA";
    }
    return oid;
}

static CRYPT_ATTRIBUTE* find_attr(PCRYPT_ATTRIBUTES attrs, const char* oid) {
    DWORD i;
    if (!attrs || !oid) {
        return NULL;
    }
    for (i = 0; i < attrs->cAttr; i++) {
        if (attrs->rgAttr[i].pszObjId && !strcmp(attrs->rgAttr[i].pszObjId, oid)) {
            return &attrs->rgAttr[i];
        }
    }
    return NULL;
}

static char* hex_encode(fz_context* ctx, const unsigned char* p, DWORD n) {
    static const char hex[] = "0123456789abcdef";
    char* out;
    DWORD i;
    if (!p || n == 0) {
        return NULL;
    }
    out = fz_malloc(ctx, (size_t)n * 2 + 1);
    for (i = 0; i < n; i++) {
        out[i * 2] = hex[p[i] >> 4];
        out[i * 2 + 1] = hex[p[i] & 0xF];
    }
    out[n * 2] = 0;
    return out;
}

static unsigned char* dup_bytes(fz_context* ctx, const unsigned char* p, int n) {
    unsigned char* out;
    if (!p || n <= 0) {
        return NULL;
    }
    out = fz_malloc(ctx, (size_t)n);
    memcpy(out, p, (size_t)n);
    return out;
}

static int asn1_len(const unsigned char* p, size_t n, size_t* hdr, size_t* body) {
    const size_t max_bytes = 4;
    return asn1_read_len(p, n, max_bytes, hdr, body);
}

static int asn1_skip(const unsigned char** p, size_t* n) {
    size_t hdr, body;
    if (*n < 1 || asn1_len(*p, *n, &hdr, &body) != 0) {
        return -1;
    }
    *p += hdr + body;
    *n -= hdr + body;
    return 0;
}

static char* oid_to_dotted(fz_context* ctx, const unsigned char* p, size_t n) {
    char buf[128];
    size_t used = 0;
    size_t i;
    unsigned int v;
    if (n < 1) {
        return NULL;
    }
    used = (size_t)snprintf(buf, sizeof(buf), "%u.%u", p[0] / 40, p[0] % 40);
    v = 0;
    for (i = 1; i < n && used + 12 < sizeof(buf); i++) {
        v = (v << 7) | (p[i] & 0x7F);
        if ((p[i] & 0x80) == 0) {
            used += (size_t)snprintf(buf + used, sizeof(buf) - used, ".%u", v);
            v = 0;
        }
    }
    return fz_dup_cstr(ctx, buf);
}

static void parse_tstinfo(fz_context* ctx, const unsigned char* p, size_t n, pkcs7_windows_ts_info* ts) {
    size_t hdr, body;
    const unsigned char* q;
    size_t left;
    if (n < 2 || p[0] != 0x30 || asn1_len(p, n, &hdr, &body) != 0) {
        return;
    }
    q = p + hdr;
    left = body;
    if (left < 1 || q[0] != 0x02 || asn1_skip(&q, &left) != 0) {
        return;
    }
    if (left >= 2 && q[0] == 0x06 && asn1_len(q, left, &hdr, &body) == 0) {
        ts->policy_oid = oid_to_dotted(ctx, q + hdr, body);
        q += hdr + body;
        left -= hdr + body;
    }
    // messageImprint: SEQ { SEQ { algorithm OID, ... }, OCTET STRING }
    if (left >= 2 && q[0] == 0x30 && asn1_len(q, left, &hdr, &body) == 0) {
        const unsigned char* mi = q + hdr;
        size_t miLeft = body;
        size_t h2, b2;
        if (miLeft >= 2 && mi[0] == 0x30 && asn1_len(mi, miLeft, &h2, &b2) == 0) {
            const unsigned char* alg = mi + h2;
            size_t h3, b3;
            if (b2 >= 2 && alg[0] == 0x06 && asn1_len(alg, b2, &h3, &b3) == 0) {
                ts->imprint_oid = oid_to_dotted(ctx, alg + h3, b3);
            }
            mi += h2 + b2;
            miLeft -= h2 + b2;
            if (miLeft >= 2 && mi[0] == 0x04 && asn1_len(mi, miLeft, &h3, &b3) == 0) {
                ts->imprint = dup_bytes(ctx, mi + h3, (int)b3);
                ts->imprint_len = (int)b3;
            }
        }
    }
    if (asn1_skip(&q, &left) != 0) {
        return;
    }
    if (asn1_skip(&q, &left) != 0) {
        return;
    }
    if (left >= 2 && (q[0] == 0x18 || q[0] == 0x17) && asn1_len(q, left, &hdr, &body) == 0 && body >= 10 && body < 32) {
        char tmp[32];
        SYSTEMTIME st;
        FILETIME ft;
        memcpy(tmp, q + hdr, body);
        tmp[body] = 0;
        ZeroMemory(&st, sizeof(st));
        if (sscanf(tmp, "%4hu%2hu%2hu%2hu%2hu%2hu", &st.wYear, &st.wMonth, &st.wDay, &st.wHour, &st.wMinute,
                   &st.wSecond) >= 4) {
            if (SystemTimeToFileTime(&st, &ft)) {
                ts->gen_time_unix = filetime_to_unix(&ft);
            }
        }
    }
}

static void free_ts(fz_context* ctx, pkcs7_windows_ts_info* ts);

static void inspect_timestamp_token(fz_context* ctx, unsigned char* tok, DWORD tok_len, pkcs7_windows_ts_info* ts) {
    HCRYPTMSG hMsg = NULL;
    HCERTSTORE hStore = NULL;
    PCMSG_SIGNER_INFO si = NULL;
    PCCERT_CONTEXT cert = NULL;
    DWORD cb = 0;
    unsigned char* inner = NULL;

    hMsg = open_msg_for_metadata(tok, tok_len);
    if (!hMsg) {
        return;
    }
    if (get_signer_count(hMsg) == 0) {
        goto done;
    }
    hStore = CertOpenStore(CERT_STORE_PROV_MSG, 0, 0, 0, hMsg);
    si = get_signer_info(hMsg, 0);
    if (hStore && si) {
        cert = find_signer_cert(hStore, si);
    }
    if (cert) {
        ts->signer_cn = get_name_string(ctx, cert, szOID_COMMON_NAME);
        ts->issuer_cn = get_cert_name(ctx, cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG, NULL);
        ts->not_after_unix = filetime_to_unix(&cert->pCertInfo->NotAfter);
        ts->cert_der_len = (int)cert->cbCertEncoded;
        ts->cert_der = dup_bytes(ctx, cert->pbCertEncoded, ts->cert_der_len);
    }
    if (si && si->HashAlgorithm.pszObjId) {
        const char* name = hash_algo_name(si->HashAlgorithm.pszObjId);
        ts->hash_algo = fz_dup_cstr(ctx, name ? name : si->HashAlgorithm.pszObjId);
    }
    if (CryptMsgGetParam(hMsg, CMSG_CONTENT_PARAM, 0, NULL, &cb) && cb > 0) {
        inner = (unsigned char*)LocalAlloc(LPTR, cb);
        if (inner && CryptMsgGetParam(hMsg, CMSG_CONTENT_PARAM, 0, inner, &cb)) {
            parse_tstinfo(ctx, inner, cb, ts);
        }
        if (inner) {
            LocalFree(inner);
        }
    }

done:
    close_metadata(hMsg, hStore, si, cert);
}

static LPCWSTR cng_hash_name(const char* oid) {
    if (!strcmp(oid, szOID_NIST_sha256)) {
        return L"SHA256";
    }
    if (!strcmp(oid, szOID_NIST_sha384)) {
        return L"SHA384";
    }
    if (!strcmp(oid, szOID_NIST_sha512)) {
        return L"SHA512";
    }
    if (!strcmp(oid, szOID_OIWSEC_sha1)) {
        return L"SHA1";
    }
    return NULL;
}

// does the imprint match these bytes?
static int imprint_matches(const pkcs7_windows_ts_info* ts, const unsigned char* data, size_t n) {
    BYTE digest[64];
    DWORD cb = sizeof(digest);
    LPCWSTR alg;
    if (!ts->imprint_oid || !ts->imprint || !data) {
        return 0;
    }
    alg = cng_hash_name(ts->imprint_oid);
    if (!alg || !CryptHashCertificate2(alg, 0, NULL, data, (DWORD)n, digest, &cb)) {
        return 0;
    }
    return (int)cb == ts->imprint_len && memcmp(digest, ts->imprint, cb) == 0;
}

// the token is a SignedData that carries what it signs
static int token_signature_ok(const unsigned char* tok, DWORD tok_len) {
    CRYPT_VERIFY_MESSAGE_PARA para;
    ZeroMemory(&para, sizeof(para));
    para.cbSize = sizeof(para);
    para.dwMsgAndCertEncodingType = PKCS_7_ASN_ENCODING | X509_ASN_ENCODING;
    return CryptVerifyMessageSignature(&para, 0, tok, tok_len, NULL, NULL, NULL) ? 1 : 0;
}

// A document timestamp stamps the bytes its /ByteRange names. Returns non-zero
// if the token is good, and when it says that was.
int pkcs7_windows_verify_ts(fz_context* ctx, unsigned char* tok, size_t tok_len, const unsigned char* stamped,
                            size_t stamped_len, int64_t* gen_time_out) {
    pkcs7_windows_ts_info ts;
    int ok;
    memset(&ts, 0, sizeof(ts));
    tok_len = trim_sig(tok, tok_len);
    inspect_timestamp_token(ctx, tok, (DWORD)tok_len, &ts);
    ok = token_signature_ok(tok, (DWORD)tok_len) && imprint_matches(&ts, stamped, stamped_len);
    if (gen_time_out) {
        *gen_time_out = ts.gen_time_unix;
    }
    free_ts(ctx, &ts);
    return ok;
}

static int bytes_contain(const BYTE* p, DWORD n, const unsigned char* needle, DWORD needle_len) {
    DWORD i;
    for (i = 0; n >= needle_len && i <= n - needle_len; i++) {
        if (memcmp(p + i, needle, needle_len) == 0) {
            return 1;
        }
    }
    return 0;
}

static void read_qc_statements(PCCERT_CONTEXT cert, pkcs7_windows_sig_info* info) {
    // DER of the ETSI EN 319 412-5 OIDs 0.4.0.1862.1.{1,4} and 1.6.{1,2,3}
    static const unsigned char kCompliance[] = {0x06, 0x06, 0x04, 0x00, 0x8E, 0x46, 0x01, 0x01};
    static const unsigned char kSscd[] = {0x06, 0x06, 0x04, 0x00, 0x8E, 0x46, 0x01, 0x04};
    static const unsigned char kEsign[] = {0x06, 0x07, 0x04, 0x00, 0x8E, 0x46, 0x01, 0x06, 0x01};
    static const unsigned char kEseal[] = {0x06, 0x07, 0x04, 0x00, 0x8E, 0x46, 0x01, 0x06, 0x02};
    static const unsigned char kWeb[] = {0x06, 0x07, 0x04, 0x00, 0x8E, 0x46, 0x01, 0x06, 0x03};
    PCERT_EXTENSION ext;
    const BYTE* p;
    DWORD n;
    if (!cert || !cert->pCertInfo) {
        return;
    }
    ext = CertFindExtension(szOID_QC_STATEMENTS, cert->pCertInfo->cExtension, cert->pCertInfo->rgExtension);
    if (!ext) {
        return;
    }
    info->has_qc_statement = 1;
    p = ext->Value.pbData;
    n = ext->Value.cbData;
    info->qc_compliance = bytes_contain(p, n, kCompliance, sizeof(kCompliance));
    info->qc_sscd = bytes_contain(p, n, kSscd, sizeof(kSscd));
    // a qualified certificate without a type is for electronic signatures
    info->qc_type = info->qc_compliance ? PKCS7_QC_TYPE_ESIGN : PKCS7_QC_TYPE_NONE;
    if (bytes_contain(p, n, kEseal, sizeof(kEseal))) {
        info->qc_type = PKCS7_QC_TYPE_ESEAL;
    } else if (bytes_contain(p, n, kWeb, sizeof(kWeb))) {
        info->qc_type = PKCS7_QC_TYPE_WEB;
    } else if (bytes_contain(p, n, kEsign, sizeof(kEsign))) {
        info->qc_type = PKCS7_QC_TYPE_ESIGN;
    }
}

static void unix_to_filetime(int64_t secs, FILETIME* ft) {
    ULARGE_INTEGER u;
    const uint64_t kEpochDiff = 11644473600ULL;
    u.QuadPart = ((uint64_t)secs + kEpochDiff) * 10000000ULL;
    ft->dwLowDateTime = u.LowPart;
    ft->dwHighDateTime = u.HighPart;
}

// ---- revocation ----------------------------------------------------------

#define PKCS7_MAX_OCSP 32

struct pkcs7_windows_rev_data {
    HCERTSTORE crls;
    int n_ocsp;
    unsigned char* ocsp[PKCS7_MAX_OCSP];
    DWORD ocsp_len[PKCS7_MAX_OCSP];
    fz_context* ctx;
};

pkcs7_windows_rev_data* pkcs7_windows_rev_new(void) {
    pkcs7_windows_rev_data* rev = (pkcs7_windows_rev_data*)calloc(1, sizeof(pkcs7_windows_rev_data));
    if (rev) {
        rev->crls = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, NULL);
    }
    return rev;
}

void pkcs7_windows_rev_free(pkcs7_windows_rev_data* rev) {
    int i;
    if (!rev) {
        return;
    }
    for (i = 0; i < rev->n_ocsp; i++) {
        free(rev->ocsp[i]);
    }
    if (rev->crls) {
        CertCloseStore(rev->crls, 0);
    }
    free(rev);
}

void pkcs7_windows_rev_add_crl(pkcs7_windows_rev_data* rev, const unsigned char* der, size_t n) {
    if (rev && rev->crls && der && n > 0) {
        CertAddEncodedCRLToStore(rev->crls, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, der, (DWORD)n,
                                 CERT_STORE_ADD_USE_EXISTING, NULL);
    }
}

void pkcs7_windows_rev_add_ocsp(fz_context* ctx, pkcs7_windows_rev_data* rev, const unsigned char* der, size_t n) {
    unsigned char* copy;
    (void)ctx;
    if (!rev || !der || n == 0 || rev->n_ocsp >= PKCS7_MAX_OCSP) {
        return;
    }
    copy = (unsigned char*)malloc(n);
    if (!copy) {
        return;
    }
    memcpy(copy, der, n);
    rev->ocsp[rev->n_ocsp] = copy;
    rev->ocsp_len[rev->n_ocsp] = (DWORD)n;
    rev->n_ocsp++;
}

// Adobe's revocation archive, an attribute signed along with the signature:
// SEQ { [0] { SEQ OF CRL }, [1] { SEQ OF OCSPResponse } }
static void read_archival_attr(PCMSG_SIGNER_INFO si, pkcs7_windows_rev_data* rev) {
    CRYPT_ATTRIBUTE* attr = find_attr(&si->AuthAttrs, "1.2.840.113583.1.1.8");
    const unsigned char* p;
    size_t left, hdr, body;
    if (!attr || attr->cValue == 0) {
        return;
    }
    p = attr->rgValue[0].pbData;
    left = attr->rgValue[0].cbData;
    if (left < 2 || p[0] != 0x30 || asn1_len(p, left, &hdr, &body) != 0) {
        return;
    }
    p += hdr;
    left = body;
    while (left >= 2) {
        const unsigned char* list;
        size_t list_left, h2, b2;
        unsigned char tag = p[0];
        if (asn1_len(p, left, &hdr, &body) != 0) {
            return;
        }
        list = p + hdr;
        list_left = body;
        p += hdr + body;
        left -= hdr + body;
        if (list_left < 2 || list[0] != 0x30 || asn1_len(list, list_left, &h2, &b2) != 0) {
            continue;
        }
        list += h2;
        list_left = b2;
        while (list_left >= 2) {
            size_t h3, b3;
            if (asn1_len(list, list_left, &h3, &b3) != 0) {
                break;
            }
            if (tag == 0xA0) {
                pkcs7_windows_rev_add_crl(rev, list, h3 + b3);
            } else if (tag == 0xA1) {
                pkcs7_windows_rev_add_ocsp(NULL, rev, list, h3 + b3);
            }
            list += h3 + b3;
            list_left -= h3 + b3;
        }
    }
}

// What the CRLs we hold say about cert. One issued after the time checked is
// what proves it wasn't revoked by then; an older one that was still current
// at that time says the same.
static pkcs7_windows_rev_status crl_status(HCERTSTORE crls, PCCERT_CONTEXT cert, PCCERT_CONTEXT issuer, FILETIME when,
                                           int* revoked_later) {
    pkcs7_windows_rev_status res = PKCS7_REV_UNKNOWN;
    PCCRL_CONTEXT crl = NULL;
    while ((crl = CertFindCRLInStore(crls, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, CRL_FIND_ISSUED_BY_SIGNATURE_FLAG,
                                     CRL_FIND_ISSUED_BY, issuer, crl)) != NULL) {
        PCRL_ENTRY entry = NULL;
        PCRL_INFO info = crl->pCrlInfo;
        int covers = CompareFileTime(&info->NextUpdate, &when) >= 0;
        if (!CertFindCertificateInCRL(cert, crl, 0, NULL, &entry)) {
            continue;
        }
        if (entry && CompareFileTime(&entry->RevocationDate, &when) <= 0) {
            CertFreeCRLContext(crl);
            return PKCS7_REV_REVOKED;
        }
        if (entry) {
            *revoked_later = 1;
        }
        if (covers) {
            res = PKCS7_REV_GOOD;
        }
    }
    return res;
}

// the responder is the issuer itself, or a certificate from it for this purpose
static int ocsp_responder_ok(PCCERT_CONTEXT responder, PCCERT_CONTEXT issuer) {
    BYTE usage[256];
    DWORD cb = sizeof(usage);
    PCERT_ENHKEY_USAGE eku = (PCERT_ENHKEY_USAGE)usage;
    DWORD i;
    if (CertCompareCertificate(X509_ASN_ENCODING, responder->pCertInfo, issuer->pCertInfo)) {
        return 1;
    }
    if (!CryptVerifyCertificateSignatureEx(0, X509_ASN_ENCODING, CRYPT_VERIFY_CERT_SIGN_SUBJECT_CERT, (void*)responder,
                                           CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT, (void*)issuer, 0, NULL)) {
        return 0;
    }
    if (!CertGetEnhancedKeyUsage(responder, CERT_FIND_EXT_ONLY_ENHKEY_USAGE_FLAG, eku, &cb)) {
        return 0;
    }
    for (i = 0; i < eku->cUsageIdentifier; i++) {
        if (!strcmp(eku->rgpszUsageIdentifier[i], szOID_PKIX_KP_OCSP_SIGNING)) {
            return 1;
        }
    }
    return 0;
}

// checks a BasicOCSPResponse: SEQ { tbsResponseData, sigAlg, signature, [0] certs }.
// Re-wrapped without the certs it has the shape the verify API takes.
static int ocsp_signature_ok(const unsigned char* basic, size_t basic_len, POCSP_BASIC_SIGNED_RESPONSE_INFO signed_info,
                             PCCERT_CONTEXT issuer) {
    size_t hdr, body, h2, b2;
    const unsigned char* p;
    size_t left, signed_len = 0;
    int i, ok = 0;
    unsigned char* wrapped;
    DWORD wrapped_len = 0;
    DWORD k;
    if (basic_len < 2 || basic[0] != 0x30 || asn1_len(basic, basic_len, &hdr, &body) != 0) {
        return 0;
    }
    p = basic + hdr;
    left = body;
    for (i = 0; i < 3; i++) {
        if (left < 2 || asn1_len(p + signed_len, left - signed_len, &h2, &b2) != 0) {
            return 0;
        }
        signed_len += h2 + b2;
    }
    wrapped = (unsigned char*)malloc(signed_len + 8);
    if (!wrapped) {
        return 0;
    }
    wrapped[0] = 0x30;
    if (signed_len < 0x80) {
        wrapped[1] = (unsigned char)signed_len;
        wrapped_len = 2;
    } else {
        int n = signed_len > 0xFFFFFF ? 4 : signed_len > 0xFFFF ? 3 : signed_len > 0xFF ? 2 : 1;
        int b;
        wrapped[1] = (unsigned char)(0x80 | n);
        for (b = 0; b < n; b++) {
            wrapped[2 + b] = (unsigned char)(signed_len >> (8 * (n - 1 - b)));
        }
        wrapped_len = (DWORD)(2 + n);
    }
    memcpy(wrapped + wrapped_len, p, signed_len);
    wrapped_len += (DWORD)signed_len;

    // try the certificates it carries, and the issuer in case it carries none
    for (k = 0; !ok && k <= signed_info->SignatureInfo.cCertEncoded; k++) {
        PCCERT_CONTEXT responder = issuer;
        if (k < signed_info->SignatureInfo.cCertEncoded) {
            CERT_BLOB* blob = &signed_info->SignatureInfo.rgCertEncoded[k];
            responder = CertCreateCertificateContext(X509_ASN_ENCODING, blob->pbData, blob->cbData);
            if (!responder) {
                continue;
            }
        }
        {
            CRYPT_DATA_BLOB subject;
            subject.cbData = wrapped_len;
            subject.pbData = wrapped;
            ok = ocsp_responder_ok(responder, issuer) &&
                 CryptVerifyCertificateSignatureEx(0, X509_ASN_ENCODING, CRYPT_VERIFY_CERT_SIGN_SUBJECT_BLOB, &subject,
                                                   CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT, (void*)responder, 0, NULL);
        }
        if (responder != issuer) {
            CertFreeCertificateContext(responder);
        }
    }
    free(wrapped);
    return ok;
}

static int ocsp_id_matches(const OCSP_CERT_ID* id, PCCERT_CONTEXT cert) {
    BYTE digest[64];
    DWORD cb = sizeof(digest);
    LPCWSTR alg = cng_hash_name(id->HashAlgorithm.pszObjId);
    if (!alg || !CertCompareIntegerBlob((PCRYPT_INTEGER_BLOB)&id->SerialNumber, &cert->pCertInfo->SerialNumber)) {
        return 0;
    }
    if (!CryptHashCertificate2(alg, 0, NULL, cert->pCertInfo->Issuer.pbData, cert->pCertInfo->Issuer.cbData, digest,
                               &cb)) {
        return 0;
    }
    return cb == id->IssuerNameHash.cbData && memcmp(digest, id->IssuerNameHash.pbData, cb) == 0;
}

// the same rules as for a CRL, from the OCSP responses we hold
static pkcs7_windows_rev_status ocsp_status(pkcs7_windows_rev_data* rev, PCCERT_CONTEXT cert, PCCERT_CONTEXT issuer,
                                            FILETIME when, int* revoked_later) {
    pkcs7_windows_rev_status res = PKCS7_REV_UNKNOWN;
    int k;
    if (!rev) {
        return res;
    }
    for (k = 0; k < rev->n_ocsp && res != PKCS7_REV_REVOKED; k++) {
        POCSP_RESPONSE_INFO outer = NULL;
        POCSP_BASIC_SIGNED_RESPONSE_INFO signed_info = NULL;
        POCSP_BASIC_RESPONSE_INFO info = NULL;
        DWORD cb = 0, e;
        if (!CryptDecodeObjectEx(X509_ASN_ENCODING, OCSP_RESPONSE, rev->ocsp[k], rev->ocsp_len[k],
                                 CRYPT_DECODE_ALLOC_FLAG, NULL, &outer, &cb)) {
            continue;
        }
        if (outer->dwStatus == 0 && outer->pszObjId &&
            !strcmp(outer->pszObjId, szOID_PKIX_OCSP_BASIC_SIGNED_RESPONSE) &&
            CryptDecodeObjectEx(X509_ASN_ENCODING, OCSP_BASIC_SIGNED_RESPONSE, outer->Value.pbData, outer->Value.cbData,
                                CRYPT_DECODE_ALLOC_FLAG, NULL, &signed_info, &cb) &&
            ocsp_signature_ok(outer->Value.pbData, outer->Value.cbData, signed_info, issuer) &&
            CryptDecodeObjectEx(X509_ASN_ENCODING, OCSP_BASIC_RESPONSE, signed_info->ToBeSigned.pbData,
                                signed_info->ToBeSigned.cbData, CRYPT_DECODE_ALLOC_FLAG, NULL, &info, &cb)) {
            for (e = 0; e < info->cResponseEntry; e++) {
                POCSP_BASIC_RESPONSE_ENTRY entry = &info->rgResponseEntry[e];
                if (!ocsp_id_matches(&entry->CertId, cert)) {
                    continue;
                }
                if (entry->dwCertStatus == OCSP_BASIC_REVOKED_CERT_STATUS) {
                    if (!entry->pRevokedInfo || CompareFileTime(&entry->pRevokedInfo->RevocationDate, &when) <= 0) {
                        res = PKCS7_REV_REVOKED;
                        break;
                    }
                    *revoked_later = 1;
                    res = PKCS7_REV_GOOD;
                } else if (entry->dwCertStatus == OCSP_BASIC_GOOD_CERT_STATUS &&
                           CompareFileTime(&entry->NextUpdate, &when) >= 0) {
                    res = PKCS7_REV_GOOD;
                }
            }
        }
        LocalFree(info);
        LocalFree(signed_info);
        LocalFree(outer);
    }
    return res;
}

#define kChainRevocationFlags \
    (CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT | CERT_CHAIN_REVOCATION_ACCUMULATIVE_TIMEOUT)

// errors that say nothing against a chain stopping at an EU-listed CA, which
// Windows may not know as a root
#define kChainErrorsAboveAnchor                                                                          \
    (CERT_TRUST_IS_UNTRUSTED_ROOT | CERT_TRUST_IS_PARTIAL_CHAIN | CERT_TRUST_REVOCATION_STATUS_UNKNOWN | \
     CERT_TRUST_IS_OFFLINE_REVOCATION)

// Builds the signer's chain as of when_unix (0 means now). extra_store holds
// certificates a chain may need that the signature doesn't carry.
void pkcs7_windows_check_chain(fz_context* ctx, unsigned char* sig, size_t sig_len, int64_t when_unix,
                               void* extra_store, pkcs7_windows_rev_data* doc_rev, pkcs7_windows_anchor_fn anchor_fn,
                               void* anchor_ud, pkcs7_windows_chain_info* out) {
    HCRYPTMSG hMsg = NULL;
    HCERTSTORE hMsgStore = NULL;
    HCERTSTORE hAll = NULL;
    PCMSG_SIGNER_INFO si = NULL;
    PCCERT_CONTEXT cert = NULL;
    PCCERT_CHAIN_CONTEXT chain = NULL;
    CERT_CHAIN_PARA para;
    FILETIME when;
    DWORD status, i;
    DWORD chain_flags = kChainRevocationFlags | CERT_CHAIN_REVOCATION_CHECK_CACHE_ONLY;
    pkcs7_windows_rev_data* sig_rev = NULL;

    memset(out, 0, sizeof(*out));
    out->no_certificate = 1;
    hMsg = open_msg_for_metadata(sig, sig_len);
    if (!hMsg || get_signer_count(hMsg) == 0) {
        goto done;
    }
    hMsgStore = CertOpenStore(CERT_STORE_PROV_MSG, 0, 0, 0, hMsg);
    si = get_signer_info(hMsg, 0);
    if (!hMsgStore || !si) {
        goto done;
    }
    cert = find_signer_cert(hMsgStore, si);
    if (!cert) {
        goto done;
    }
    out->no_certificate = 0;

    hAll = CertOpenStore(CERT_STORE_PROV_COLLECTION, 0, 0, 0, NULL);
    if (!hAll) {
        goto done;
    }
    CertAddStoreToCollection(hAll, hMsgStore, 0, 0);
    if (extra_store) {
        CertAddStoreToCollection(hAll, (HCERTSTORE)extra_store, 0, 0);
    }
    sig_rev = pkcs7_windows_rev_new();
    if (sig_rev) {
        read_archival_attr(si, sig_rev);
    }

    ZeroMemory(&para, sizeof(para));
    para.cbSize = sizeof(para);
    if (when_unix > 0) {
        unix_to_filetime(when_unix, &when);
    } else {
        GetSystemTimeAsFileTime(&when);
    }
    if (!CertGetCertificateChain(NULL, cert, &when, hAll, &para, chain_flags, NULL, &chain)) {
        warn_gle(ctx, "check_chain CertGetCertificateChain", GetLastError());
        goto done;
    }

    // no data about revocation isn't an error in the chain itself
    status = chain->TrustStatus.dwErrorStatus &
             ~(DWORD)(CERT_TRUST_REVOCATION_STATUS_UNKNOWN | CERT_TRUST_IS_OFFLINE_REVOCATION);
    out->os_trusted = status == CERT_TRUST_NO_ERROR;
    if (chain->cChain > 0 && chain->rgpChain[0]->cElement > 0) {
        PCERT_SIMPLE_CHAIN simple = chain->rgpChain[0];
        int bad_below_anchor = 0;
        out->not_time_valid = (simple->rgpElement[0]->TrustStatus.dwErrorStatus & CERT_TRUST_IS_NOT_TIME_VALID) != 0;
        out->self_signed = simple->cElement == 1 && (status & CERT_TRUST_IS_UNTRUSTED_ROOT) != 0;
        out->untrusted_root = (status & (CERT_TRUST_IS_UNTRUSTED_ROOT | CERT_TRUST_IS_PARTIAL_CHAIN)) != 0;
        for (i = 0; i < simple->cElement; i++) {
            PCERT_CHAIN_ELEMENT el = simple->rgpElement[i];
            DWORD err = el->TrustStatus.dwErrorStatus;
            int anchor =
                anchor_fn ? anchor_fn(anchor_ud, el->pCertContext->pbCertEncoded, (int)el->pCertContext->cbCertEncoded)
                          : 0;
            // the end entity itself is not an anchor: the list names CAs
            if (anchor && i > 0) {
                // the anchor's own validity is the list's business
                err &= ~(DWORD)(kChainErrorsAboveAnchor | CERT_TRUST_IS_NOT_TIME_VALID | CERT_TRUST_IS_NOT_TIME_NESTED);
                if (err) {
                    bad_below_anchor = 1;
                }
                out->eu_status = anchor;
                break;
            }
            if (err & ~(DWORD)kChainErrorsAboveAnchor) {
                bad_below_anchor = 1;
            }
        }
        out->eu_chain_ok = out->eu_status != 0 && !bad_below_anchor;

        // the revocation status of each certificate up to the anchor or root
        {
            DWORD last = simple->cElement - 1;
            int all_good = simple->cElement > 1;
            int all_embedded = all_good;
            for (i = 0; i < last; i++) {
                PCERT_CHAIN_ELEMENT el = simple->rgpElement[i];
                PCCERT_CONTEXT issuer = simple->rgpElement[i + 1]->pCertContext;
                pkcs7_windows_rev_status st = PKCS7_REV_UNKNOWN;
                DWORD err = el->TrustStatus.dwErrorStatus;
                if (doc_rev && doc_rev->crls) {
                    st = crl_status(doc_rev->crls, el->pCertContext, issuer, when, &out->revoked_later);
                }
                if (st == PKCS7_REV_UNKNOWN && sig_rev && sig_rev->crls) {
                    st = crl_status(sig_rev->crls, el->pCertContext, issuer, when, &out->revoked_later);
                }
                if (st == PKCS7_REV_UNKNOWN) {
                    st = crl_status(hMsgStore, el->pCertContext, issuer, when, &out->revoked_later);
                }
                if (st == PKCS7_REV_UNKNOWN) {
                    st = ocsp_status(doc_rev, el->pCertContext, issuer, when, &out->revoked_later);
                }
                if (st == PKCS7_REV_UNKNOWN) {
                    st = ocsp_status(sig_rev, el->pCertContext, issuer, when, &out->revoked_later);
                }
                if (st == PKCS7_REV_REVOKED || (err & CERT_TRUST_IS_REVOKED)) {
                    out->revocation = PKCS7_REV_REVOKED;
                    out->revoked_later = 0;
                    out->os_trusted = 0;
                    out->eu_chain_ok = 0;
                    goto done;
                }
                if (st != PKCS7_REV_GOOD) {
                    all_embedded = 0;
                    if (err & (CERT_TRUST_REVOCATION_STATUS_UNKNOWN | CERT_TRUST_IS_OFFLINE_REVOCATION)) {
                        all_good = 0;
                    }
                }
                if (anchor_fn && i + 1 <= last && out->eu_status != 0 &&
                    anchor_fn(anchor_ud, issuer->pbCertEncoded, (int)issuer->cbCertEncoded)) {
                    break;
                }
            }
            if (all_good) {
                out->revocation = PKCS7_REV_GOOD;
                out->revocation_embedded = all_embedded;
            } else {
                out->revocation_embedded = 0;
            }
        }
    }
    {
        DWORD known = CERT_TRUST_IS_UNTRUSTED_ROOT | CERT_TRUST_IS_PARTIAL_CHAIN | CERT_TRUST_IS_NOT_TIME_VALID |
                      CERT_TRUST_IS_NOT_TIME_NESTED | CERT_TRUST_REVOCATION_STATUS_UNKNOWN |
                      CERT_TRUST_IS_OFFLINE_REVOCATION;
        out->other_error = (status & ~known) != 0;
    }

done:
    pkcs7_windows_rev_free(sig_rev);
    if (chain) {
        CertFreeCertificateChain(chain);
    }
    if (hAll) {
        CertCloseStore(hAll, 0);
    }
    close_metadata(hMsg, hMsgStore, si, cert);
}

void pkcs7_windows_fetch_revocation(unsigned char* sig, size_t sig_len, int64_t when_unix, void* extra_store) {
    HCRYPTMSG hMsg = open_msg_for_metadata(sig, sig_len);
    HCERTSTORE hMsgStore = NULL;
    HCERTSTORE hAll = NULL;
    PCMSG_SIGNER_INFO si = NULL;
    PCCERT_CONTEXT cert = NULL;
    PCCERT_CHAIN_CONTEXT chain = NULL;
    CERT_CHAIN_PARA para;
    FILETIME when;

    if (!hMsg || get_signer_count(hMsg) == 0) {
        goto done;
    }
    hMsgStore = CertOpenStore(CERT_STORE_PROV_MSG, 0, 0, 0, hMsg);
    si = get_signer_info(hMsg, 0);
    cert = (hMsgStore && si) ? find_signer_cert(hMsgStore, si) : NULL;
    hAll = CertOpenStore(CERT_STORE_PROV_COLLECTION, 0, 0, 0, NULL);
    if (!cert || !hAll) {
        goto done;
    }
    CertAddStoreToCollection(hAll, hMsgStore, 0, 0);
    if (extra_store) {
        CertAddStoreToCollection(hAll, (HCERTSTORE)extra_store, 0, 0);
    }
    ZeroMemory(&para, sizeof(para));
    para.cbSize = sizeof(para);
    if (when_unix > 0) {
        unix_to_filetime(when_unix, &when);
    } else {
        GetSystemTimeAsFileTime(&when);
    }
    if (CertGetCertificateChain(NULL, cert, &when, hAll, &para, kChainRevocationFlags, NULL, &chain)) {
        CertFreeCertificateChain(chain);
    }

done:
    if (hAll) {
        CertCloseStore(hAll, 0);
    }
    close_metadata(hMsg, hMsgStore, si, cert);
}

void pkcs7_windows_sig_info_clear(pkcs7_windows_sig_info* info) {
    if (info) {
        memset(info, 0, sizeof(*info));
    }
}

static void free_ts(fz_context* ctx, pkcs7_windows_ts_info* ts) {
    fz_free(ctx, ts->signer_cn);
    fz_free(ctx, ts->issuer_cn);
    fz_free(ctx, ts->hash_algo);
    fz_free(ctx, ts->policy_oid);
    fz_free(ctx, ts->cert_der);
    fz_free(ctx, ts->imprint_oid);
    fz_free(ctx, ts->imprint);
    fz_free(ctx, ts->token);
}

void pkcs7_windows_sig_info_free(fz_context* ctx, pkcs7_windows_sig_info* info) {
    int i;
    if (!info) {
        return;
    }
    fz_free(ctx, info->signer_cn);
    fz_free(ctx, info->issuer_cn);
    fz_free(ctx, info->hash_algo);
    fz_free(ctx, info->sig_algo);
    fz_free(ctx, info->digest_hex);
    fz_free(ctx, info->policy_oid);
    fz_free(ctx, info->cert_der);
    for (i = 0; i < info->n_ts; i++) {
        free_ts(ctx, &info->ts[i]);
    }
    memset(info, 0, sizeof(*info));
}

static int ts_filled(const pkcs7_windows_ts_info* ts) {
    return ts->signer_cn || ts->gen_time_unix || ts->cert_der;
}

// sig_value is what a signature timestamp stamps: the signature's own value
static void add_ts_token(fz_context* ctx, unsigned char* p, DWORD n, const CRYPT_DATA_BLOB* sig_value,
                         pkcs7_windows_sig_info* info) {
    pkcs7_windows_ts_info* dst;
    if (!info || info->n_ts >= PKCS7_WINDOWS_MAX_TS || !p || n == 0) {
        return;
    }
    dst = &info->ts[info->n_ts];
    memset(dst, 0, sizeof(*dst));
    inspect_timestamp_token(ctx, p, n, dst);
    if (!ts_filled(dst)) {
        free_ts(ctx, dst);
        memset(dst, 0, sizeof(*dst));
        return;
    }
    dst->token = dup_bytes(ctx, p, (int)n);
    dst->token_len = (int)n;
    dst->verified = token_signature_ok(p, n) && imprint_matches(dst, sig_value->pbData, sig_value->cbData);
    info->n_ts++;
    info->has_timestamp = 1;
}

static int is_ts_oid(const char* oid) {
    return oid && (!strcmp(oid, szOID_RFC3161_counterSign) || !strcmp(oid, szOID_PKCS9_COUNTER_SIGNATURE) ||
                   !strcmp(oid, "1.2.840.113549.1.9.16.2.27") || !strcmp(oid, "1.2.840.113549.1.9.16.2.48") ||
                   !strcmp(oid, "0.4.0.19122.1.1"));
}

static void collect_ts_from_unauth(fz_context* ctx, PCRYPT_ATTRIBUTES attrs, const CRYPT_DATA_BLOB* sig_value,
                                   pkcs7_windows_sig_info* info) {
    DWORD i, v;
    if (!attrs) {
        return;
    }
    for (i = 0; i < attrs->cAttr; i++) {
        CRYPT_ATTRIBUTE* attr = &attrs->rgAttr[i];
        if (!is_ts_oid(attr->pszObjId)) {
            continue;
        }
        for (v = 0; v < attr->cValue; v++) {
            add_ts_token(ctx, attr->rgValue[v].pbData, attr->rgValue[v].cbData, sig_value, info);
        }
    }
}

static void collect_ts_from_raw(fz_context* ctx, unsigned char* sig, size_t sig_len, const CRYPT_DATA_BLOB* sig_value,
                                pkcs7_windows_sig_info* info) {
    static const unsigned char kOid14[] = {0x06, 0x0B, 0x2A, 0x86, 0x48, 0x86, 0xF7,
                                           0x0D, 0x01, 0x09, 0x10, 0x02, 0x0E};
    static const unsigned char kOid27[] = {0x06, 0x0B, 0x2A, 0x86, 0x48, 0x86, 0xF7,
                                           0x0D, 0x01, 0x09, 0x10, 0x02, 0x1B};
    static const unsigned char kOid48[] = {0x06, 0x0B, 0x2A, 0x86, 0x48, 0x86, 0xF7,
                                           0x0D, 0x01, 0x09, 0x10, 0x02, 0x30};
    size_t i;
    sig_len = trim_sig(sig, sig_len);
    for (i = 0; i + sizeof(kOid14) + 4 < sig_len && info->n_ts < PKCS7_WINDOWS_MAX_TS; i++) {
        int hit = 0;
        if (memcmp(sig + i, kOid14, sizeof(kOid14)) == 0) {
            hit = (int)sizeof(kOid14);
        } else if (memcmp(sig + i, kOid27, sizeof(kOid27)) == 0) {
            hit = (int)sizeof(kOid27);
        } else if (memcmp(sig + i, kOid48, sizeof(kOid48)) == 0) {
            hit = (int)sizeof(kOid48);
        }
        if (!hit) {
            continue;
        }
        {
            const unsigned char* p = sig + i + (size_t)hit;
            size_t left = sig_len - (i + (size_t)hit);
            size_t hdr, body;
            if (left < 2 || p[0] != 0x31 || asn1_len(p, left, &hdr, &body) != 0) {
                continue;
            }
            p += hdr;
            left = body;
            if (left < 2 || p[0] != 0x30 || asn1_len(p, left, &hdr, &body) != 0) {
                continue;
            }
            add_ts_token(ctx, (unsigned char*)p, (DWORD)(hdr + body), sig_value, info);
        }
        i += (size_t)hit - 1;
    }
}

int pkcs7_windows_inspect(fz_context* ctx, unsigned char* sig, size_t sig_len, pkcs7_windows_sig_info* info) {
    HCRYPTMSG hMsg = NULL;
    HCERTSTORE hStore = NULL;
    PCMSG_SIGNER_INFO si = NULL;
    PCCERT_CONTEXT cert = NULL;
    CRYPT_ATTRIBUTE* attr;
    int ok = 0;

    if (!info) {
        return 0;
    }
    pkcs7_windows_sig_info_clear(info);

    hMsg = open_msg_for_metadata(sig, sig_len);
    if (!hMsg) {
        warn_gle(ctx, "inspect parse envelope", GetLastError());
        return 0;
    }
    if (get_signer_count(hMsg) == 0) {
        goto done;
    }
    hStore = CertOpenStore(CERT_STORE_PROV_MSG, 0, 0, 0, hMsg);
    si = get_signer_info(hMsg, 0);
    if (!si) {
        goto done;
    }
    if (hStore) {
        cert = find_signer_cert(hStore, si);
    }
    if (cert) {
        info->signer_cn = get_name_string(ctx, cert, szOID_COMMON_NAME);
        info->issuer_cn = get_cert_name(ctx, cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG, NULL);
        info->not_after_unix = filetime_to_unix(&cert->pCertInfo->NotAfter);
        info->cert_der_len = (int)cert->cbCertEncoded;
        info->cert_der = dup_bytes(ctx, cert->pbCertEncoded, info->cert_der_len);
        read_qc_statements(cert, info);
    }
    if (si->HashAlgorithm.pszObjId) {
        const char* name = hash_algo_name(si->HashAlgorithm.pszObjId);
        info->hash_algo = fz_dup_cstr(ctx, name ? name : si->HashAlgorithm.pszObjId);
    }
    if (si->HashEncryptionAlgorithm.pszObjId) {
        info->sig_algo = fz_dup_cstr(ctx, sig_algo_name(si->HashEncryptionAlgorithm.pszObjId));
    }
    attr = find_attr(&si->AuthAttrs, szOID_RSA_messageDigest);
    if (attr && attr->cValue > 0) {
        CRYPT_DATA_BLOB* blob = NULL;
        DWORD cb = 0;
        if (CryptDecodeObjectEx(X509_ASN_ENCODING, X509_OCTET_STRING, attr->rgValue[0].pbData, attr->rgValue[0].cbData,
                                CRYPT_DECODE_ALLOC_FLAG, NULL, &blob, &cb) &&
            blob) {
            info->digest_hex = hex_encode(ctx, blob->pbData, blob->cbData);
            LocalFree(blob);
        }
    }
    if (find_attr(&si->AuthAttrs, "1.2.840.113549.1.9.16.2.12") ||
        find_attr(&si->AuthAttrs, "1.2.840.113549.1.9.16.2.47")) {
        info->has_cades_attr = 1;
    }
    if (find_attr(&si->AuthAttrs, "1.2.840.113549.1.9.16.2.15")) {
        info->has_sig_policy_attr = 1;
    }
    collect_ts_from_unauth(ctx, &si->UnauthAttrs, &si->EncryptedHash, info);
    if (info->n_ts == 0) {
        collect_ts_from_raw(ctx, sig, sig_len, &si->EncryptedHash, info);
    }
    {
        DWORD cb = 0;
        if (CryptMsgGetParam(hMsg, CMSG_CONTENT_PARAM, 0, NULL, &cb) && cb > 0) {
            unsigned char* inner = (unsigned char*)LocalAlloc(LPTR, cb);
            if (inner && CryptMsgGetParam(hMsg, CMSG_CONTENT_PARAM, 0, inner, &cb)) {
                pkcs7_windows_ts_info ts;
                memset(&ts, 0, sizeof(ts));
                parse_tstinfo(ctx, inner, cb, &ts);
                info->gen_time_unix = ts.gen_time_unix;
                info->policy_oid = ts.policy_oid;
                fz_free(ctx, ts.imprint_oid);
                fz_free(ctx, ts.imprint);
            }
            if (inner) {
                LocalFree(inner);
            }
        }
    }
    ok = 1;

done:
    close_metadata(hMsg, hStore, si, cert);
    return ok;
}
