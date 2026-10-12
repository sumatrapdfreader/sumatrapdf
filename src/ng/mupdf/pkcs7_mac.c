/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// PDF signing on the macOS Security framework instead of OpenSSL:
// SecPKCS12Import reads the .pfx, CMSEncoder makes the detached signature,
// CMSDecoder verifies one.

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#include "pkcs7_mac.h"

#include <Security/Security.h>
#include <Security/CMSEncoder.h>
#include <Security/CMSDecoder.h>
#include <Security/SecCertificateOIDs.h>

typedef struct mac_signer {
    pdf_pkcs7_signer base;
    int refs;
    SecIdentityRef identity;
} mac_signer;

enum {
    BER_CONSTRUCTED = 0x20,
    BER_LONG_LEN = 0x80,
    READ_CHUNK = 4096,
};

static void der_append_header(fz_context* ctx, fz_buffer* out, unsigned char tag, size_t len) {
    fz_append_byte(ctx, out, tag);
    if (len < BER_LONG_LEN) {
        fz_append_byte(ctx, out, (int)len);
        return;
    }
    int n = 0;
    for (size_t v = len; v > 0; v >>= 8) {
        n++;
    }
    fz_append_byte(ctx, out, BER_LONG_LEN | n);
    for (int i = n - 1; i >= 0; i--) {
        fz_append_byte(ctx, out, (int)((len >> (i * 8)) & 0xff));
    }
}

// Re-encodes one BER element with definite lengths and returns the byte after
// it. CMSEncoder writes indefinite lengths; a PDF signature has to be DER.
static const unsigned char* ber_to_der(fz_context* ctx, fz_buffer* out, const unsigned char* p,
                                       const unsigned char* end) {
    if (end - p < 2) {
        fz_throw(ctx, FZ_ERROR_FORMAT, "truncated signature");
    }
    unsigned char tag = *p++;
    size_t len = *p++;
    int indefinite = len == BER_LONG_LEN;
    if (!indefinite && (len & BER_LONG_LEN)) {
        int n = (int)(len & ~BER_LONG_LEN);
        if (n > (int)sizeof(size_t) || end - p < n) {
            fz_throw(ctx, FZ_ERROR_FORMAT, "malformed signature");
        }
        len = 0;
        while (n-- > 0) {
            len = (len << 8) | *p++;
        }
    }
    if (!indefinite && (size_t)(end - p) < len) {
        fz_throw(ctx, FZ_ERROR_FORMAT, "truncated signature");
    }

    if (!(tag & BER_CONSTRUCTED)) {
        if (indefinite) {
            fz_throw(ctx, FZ_ERROR_FORMAT, "malformed signature");
        }
        der_append_header(ctx, out, tag, len);
        fz_append_data(ctx, out, p, len);
        return p + len;
    }

    const unsigned char* body_end = indefinite ? end : p + len;
    fz_buffer* body = fz_new_buffer(ctx, len);
    fz_try(ctx) {
        for (;;) {
            if (indefinite && body_end - p >= 2 && p[0] == 0 && p[1] == 0) {
                p += 2;
                break;
            }
            if (p >= body_end) {
                if (indefinite) {
                    fz_throw(ctx, FZ_ERROR_FORMAT, "truncated signature");
                }
                break;
            }
            p = ber_to_der(ctx, body, p, body_end);
        }
        unsigned char* data = NULL;
        size_t n = fz_buffer_storage(ctx, body, &data);
        der_append_header(ctx, out, tag, n);
        fz_append_data(ctx, out, data, n);
    }
    fz_always(ctx) {
        fz_drop_buffer(ctx, body);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
    return p;
}

static pdf_pkcs7_signer* keep_signer(fz_context* ctx, pdf_pkcs7_signer* signer) {
    mac_signer* ms = (mac_signer*)signer;
    return fz_keep_imp(ctx, ms, &ms->refs);
}

static void drop_signer(fz_context* ctx, pdf_pkcs7_signer* signer) {
    mac_signer* ms = (mac_signer*)signer;
    if (!fz_drop_imp(ctx, ms, &ms->refs)) {
        return;
    }
    if (ms->identity) {
        CFRelease(ms->identity);
    }
    fz_free(ctx, ms);
}

static char* cf_strdup(fz_context* ctx, CFStringRef s) {
    if (!s) {
        return NULL;
    }
    CFIndex cap = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s), kCFStringEncodingUTF8) + 1;
    char* res = fz_malloc(ctx, (size_t)cap);
    if (!CFStringGetCString(s, res, cap, kCFStringEncodingUTF8)) {
        fz_free(ctx, res);
        return NULL;
    }
    return res;
}

// The subject's attribute with this OID, e.g. "2.5.4.10" for the organization.
static char* subject_attr(fz_context* ctx, SecCertificateRef cert, CFStringRef attr_oid) {
    const void* keys[] = {kSecOIDX509V1SubjectName};
    CFArrayRef want = CFArrayCreate(NULL, keys, 1, &kCFTypeArrayCallBacks);
    CFDictionaryRef values = SecCertificateCopyValues(cert, want, NULL);
    CFRelease(want);
    if (!values) {
        return NULL;
    }
    char* res = NULL;
    CFDictionaryRef subject = CFDictionaryGetValue(values, kSecOIDX509V1SubjectName);
    CFArrayRef attrs = subject ? CFDictionaryGetValue(subject, kSecPropertyKeyValue) : NULL;
    CFIndex n = attrs && CFGetTypeID(attrs) == CFArrayGetTypeID() ? CFArrayGetCount(attrs) : 0;
    for (CFIndex i = 0; i < n && !res; i++) {
        CFDictionaryRef attr = CFArrayGetValueAtIndex(attrs, i);
        CFTypeRef label = CFDictionaryGetValue(attr, kSecPropertyKeyLabel);
        CFTypeRef value = CFDictionaryGetValue(attr, kSecPropertyKeyValue);
        if (!label || !value || !CFEqual(label, attr_oid) || CFGetTypeID(value) != CFStringGetTypeID()) {
            continue;
        }
        res = cf_strdup(ctx, value);
    }
    CFRelease(values);
    return res;
}

// Takes over the caller's reference to cert.
static pdf_pkcs7_distinguished_name* cert_distinguished_name(fz_context* ctx, SecCertificateRef cert) {
    pdf_pkcs7_distinguished_name* dn = NULL;
    fz_var(dn);
    fz_try(ctx) {
        dn = fz_malloc_struct(ctx, pdf_pkcs7_distinguished_name);
        dn->cn = subject_attr(ctx, cert, kSecOIDCommonName);
        dn->o = subject_attr(ctx, cert, kSecOIDOrganizationName);
        dn->ou = subject_attr(ctx, cert, kSecOIDOrganizationalUnitName);
        dn->email = subject_attr(ctx, cert, kSecOIDEmailAddress);
        dn->c = subject_attr(ctx, cert, kSecOIDCountryName);
    }
    fz_always(ctx) {
        CFRelease(cert);
    }
    fz_catch(ctx) {
        pdf_signature_drop_distinguished_name(ctx, dn);
        fz_rethrow(ctx);
    }
    return dn;
}

static pdf_pkcs7_distinguished_name* signer_distinguished_name(fz_context* ctx, pdf_pkcs7_signer* signer) {
    mac_signer* ms = (mac_signer*)signer;
    SecCertificateRef cert = NULL;
    if (SecIdentityCopyCertificate(ms->identity, &cert) != errSecSuccess) {
        fz_throw(ctx, FZ_ERROR_LIBRARY, "cannot read the signing certificate");
    }
    return cert_distinguished_name(ctx, cert);
}

// Returns the signature's size, 0 on failure. A NULL digest only measures.
static int signer_create_digest(fz_context* ctx, pdf_pkcs7_signer* signer, fz_stream* in, unsigned char* digest,
                                size_t digest_len) {
    mac_signer* ms = (mac_signer*)signer;
    CMSEncoderRef enc = NULL;
    CFDataRef ber = NULL;
    fz_buffer* der = NULL;
    int res = 0;
    fz_var(ber);
    fz_var(der);
    fz_var(res);

    if (CMSEncoderCreate(&enc) != errSecSuccess) {
        return 0;
    }
    fz_try(ctx) {
        OSStatus st = CMSEncoderSetSignerAlgorithm(enc, kCMSEncoderDigestAlgorithmSHA256);
        if (st == errSecSuccess) {
            st = CMSEncoderAddSigners(enc, ms->identity);
        }
        if (st == errSecSuccess) {
            st = CMSEncoderSetHasDetachedContent(enc, true);
        }
        unsigned char buf[READ_CHUNK] = {0};
        // CMSEncoder refuses to sign no content at all
        if (st == errSecSuccess && !in) {
            st = CMSEncoderUpdateContent(enc, buf, 1);
        }
        while (st == errSecSuccess && in) {
            size_t n = fz_read(ctx, in, buf, sizeof(buf));
            if (n == 0) {
                break;
            }
            st = CMSEncoderUpdateContent(enc, buf, n);
        }
        if (st == errSecSuccess) {
            st = CMSEncoderCopyEncodedContent(enc, &ber);
        }
        if (st != errSecSuccess || !ber) {
            fz_throw(ctx, FZ_ERROR_LIBRARY, "CMSEncoder failed (%d)", (int)st);
        }

        const unsigned char* p = CFDataGetBytePtr(ber);
        size_t ber_len = (size_t)CFDataGetLength(ber);
        der = fz_new_buffer(ctx, ber_len);
        ber_to_der(ctx, der, p, p + ber_len);

        unsigned char* data = NULL;
        size_t n = fz_buffer_storage(ctx, der, &data);
        if (digest && n > digest_len) {
            fz_throw(ctx, FZ_ERROR_LIBRARY, "signature doesn't fit the space reserved for it");
        }
        if (digest) {
            memcpy(digest, data, n);
        }
        res = (int)n;
    }
    fz_always(ctx) {
        fz_drop_buffer(ctx, der);
        if (ber) {
            CFRelease(ber);
        }
        CFRelease(enc);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        res = 0;
    }
    return res;
}

// The size doesn't depend on what is signed, so sign nothing to find it.
static size_t max_digest_size(fz_context* ctx, pdf_pkcs7_signer* signer) {
    return (size_t)signer_create_digest(ctx, signer, NULL, NULL, 0);
}

// Before macOS 15 SecPKCS12Import can't keep the key out of the login keychain.
static OSStatus import_identity(CFDataRef pfx, CFStringRef password, SecIdentityRef* identity_out) {
    const void* keys[2] = {kSecImportExportPassphrase, NULL};
    const void* vals[2] = {password, kCFBooleanTrue};
    CFIndex n = 1;
    if (__builtin_available(macOS 15.0, *)) {
        keys[n++] = kSecImportToMemoryOnly;
    }
    CFDictionaryRef opts =
        CFDictionaryCreate(NULL, keys, vals, n, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFArrayRef items = NULL;
    OSStatus st = SecPKCS12Import(pfx, opts, &items);
    CFRelease(opts);
    if (st == errSecSuccess && (!items || CFArrayGetCount(items) == 0)) {
        st = errSecItemNotFound;
    }
    if (st == errSecSuccess) {
        CFDictionaryRef item = CFArrayGetValueAtIndex(items, 0);
        SecIdentityRef identity = (SecIdentityRef)CFDictionaryGetValue(item, kSecImportItemIdentity);
        if (identity) {
            *identity_out = (SecIdentityRef)CFRetain(identity);
        } else {
            st = errSecItemNotFound;
        }
    }
    if (items) {
        CFRelease(items);
    }
    return st;
}

pdf_pkcs7_signer* pkcs7_mac_read_pfx(fz_context* ctx, const char* pfile, const char* pw) {
    mac_signer* signer = NULL;
    fz_buffer* buf = NULL;
    CFDataRef pfx = NULL;
    CFStringRef password = NULL;
    fz_var(signer);
    fz_var(buf);
    fz_var(pfx);
    fz_var(password);

    fz_try(ctx) {
        signer = fz_malloc_struct(ctx, mac_signer);
        signer->base.keep = keep_signer;
        signer->base.drop = drop_signer;
        signer->base.get_signing_name = signer_distinguished_name;
        signer->base.max_digest_size = max_digest_size;
        signer->base.create_digest = signer_create_digest;
        signer->refs = 1;

        buf = fz_read_file(ctx, pfile);
        unsigned char* data = NULL;
        size_t n = fz_buffer_storage(ctx, buf, &data);
        pfx = CFDataCreate(NULL, data, (CFIndex)n);
        password = CFStringCreateWithCString(NULL, pw ? pw : "", kCFStringEncodingUTF8);
        if (!pfx || !password) {
            fz_throw(ctx, FZ_ERROR_SYSTEM, "out of memory");
        }
        OSStatus st = import_identity(pfx, password, &signer->identity);
        if (st == errSecAuthFailed || st == errSecPkcs12VerifyFailure) {
            fz_throw(ctx, FZ_ERROR_ARGUMENT, "SecPKCS12Import failed: wrong password");
        }
        if (st != errSecSuccess) {
            fz_throw(ctx, FZ_ERROR_LIBRARY, "SecPKCS12Import failed (%d)", (int)st);
        }
    }
    fz_always(ctx) {
        if (password) {
            CFRelease(password);
        }
        if (pfx) {
            CFRelease(pfx);
        }
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        if (signer) {
            drop_signer(ctx, &signer->base);
        }
        fz_rethrow(ctx);
    }
    return &signer->base;
}

// The size of the DER element at sig, 0 if malformed. /Contents is padded
// with zeros that CMSDecoder takes for more message.
static size_t der_element_size(const unsigned char* sig, size_t sig_len) {
    if (sig_len < 2) {
        return 0;
    }
    size_t len = sig[1];
    size_t header = 2;
    if (len & BER_LONG_LEN) {
        size_t n = len & ~BER_LONG_LEN;
        if (n == 0 || n > sizeof(size_t) || sig_len < 2 + n) {
            return 0;
        }
        len = 0;
        for (size_t i = 0; i < n; i++) {
            len = (len << 8) | sig[2 + i];
        }
        header += n;
    }
    if (len > sig_len - header) {
        return 0;
    }
    return header + len;
}

// Decodes a detached signature over the bytes of `in` (NULL: only parse it).
// NULL if it is not a CMS message.
static CMSDecoderRef decode_signature(fz_context* ctx, fz_stream* in, const unsigned char* sig, size_t sig_len) {
    size_t der_len = der_element_size(sig, sig_len);
    CMSDecoderRef dec = NULL;
    if (der_len == 0 || CMSDecoderCreate(&dec) != errSecSuccess) {
        return NULL;
    }
    CFMutableDataRef content = NULL;
    OSStatus st = CMSDecoderUpdateMessage(dec, sig, der_len);
    fz_var(content);
    fz_var(st);
    fz_try(ctx) {
        if (st == errSecSuccess && in) {
            content = CFDataCreateMutable(NULL, 0);
            unsigned char buf[READ_CHUNK];
            for (;;) {
                size_t n = fz_read(ctx, in, buf, sizeof(buf));
                if (n == 0) {
                    break;
                }
                CFDataAppendBytes(content, buf, (CFIndex)n);
            }
            st = CMSDecoderSetDetachedContent(dec, content);
        }
        if (st == errSecSuccess) {
            st = CMSDecoderFinalizeMessage(dec);
        }
    }
    fz_always(ctx) {
        if (content) {
            CFRelease(content);
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        st = errSecDecode;
    }
    if (st != errSecSuccess) {
        CFRelease(dec);
        return NULL;
    }
    return dec;
}

static pdf_signature_error verifier_check_digest(fz_context* ctx, pdf_pkcs7_verifier* verifier, fz_stream* in,
                                                 unsigned char* sig, size_t sig_len) {
    (void)verifier;
    CMSDecoderRef dec = decode_signature(ctx, in, sig, sig_len);
    if (!dec) {
        return PDF_SIGNATURE_ERROR_UNKNOWN;
    }
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    CMSSignerStatus status = kCMSSignerUnsigned;
    OSStatus st = CMSDecoderCopySignerStatus(dec, 0, policy, false, &status, NULL, NULL);
    CFRelease(policy);
    CFRelease(dec);
    if (st != errSecSuccess) {
        return PDF_SIGNATURE_ERROR_UNKNOWN;
    }
    switch (status) {
        case kCMSSignerValid:
            return PDF_SIGNATURE_ERROR_OKAY;
        case kCMSSignerInvalidSignature:
            return PDF_SIGNATURE_ERROR_DIGEST_FAILURE;
        case kCMSSignerUnsigned:
            return PDF_SIGNATURE_ERROR_NO_SIGNATURES;
        default:
            return PDF_SIGNATURE_ERROR_UNKNOWN;
    }
}

static int cert_is_self_signed(SecCertificateRef cert) {
    CFDataRef issuer = SecCertificateCopyNormalizedIssuerSequence(cert);
    CFDataRef subject = SecCertificateCopyNormalizedSubjectSequence(cert);
    int res = issuer && subject && CFEqual(issuer, subject);
    if (issuer) {
        CFRelease(issuer);
    }
    if (subject) {
        CFRelease(subject);
    }
    return res;
}

static pdf_signature_error verifier_check_certificate(fz_context* ctx, pdf_pkcs7_verifier* verifier,
                                                      unsigned char* sig, size_t sig_len) {
    (void)verifier;
    CMSDecoderRef dec = decode_signature(ctx, NULL, sig, sig_len);
    if (!dec) {
        return PDF_SIGNATURE_ERROR_UNKNOWN;
    }
    pdf_signature_error res = PDF_SIGNATURE_ERROR_UNKNOWN;
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    SecTrustRef trust = NULL;
    SecCertificateRef cert = NULL;
    CFArrayRef certs = NULL;
    // the signer's certificate leads; the message's other ones help build the chain
    CFMutableArrayRef chain = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (CMSDecoderCopySignerCert(dec, 0, &cert) == errSecSuccess && cert) {
        CFArrayAppendValue(chain, cert);
        if (CMSDecoderCopyAllCerts(dec, &certs) == errSecSuccess && certs) {
            CFArrayAppendArray(chain, certs, CFRangeMake(0, CFArrayGetCount(certs)));
        }
        if (SecTrustCreateWithCertificates(chain, policy, &trust) != errSecSuccess) {
            trust = NULL;
        }
    }
    if (trust) {
        if (SecTrustEvaluateWithError(trust, NULL)) {
            res = PDF_SIGNATURE_ERROR_OKAY;
        } else if (cert_is_self_signed(cert)) {
            res = PDF_SIGNATURE_ERROR_SELF_SIGNED;
        } else {
            res = PDF_SIGNATURE_ERROR_NOT_TRUSTED;
        }
        CFRelease(trust);
    }
    if (certs) {
        CFRelease(certs);
    }
    if (cert) {
        CFRelease(cert);
    }
    CFRelease(chain);
    CFRelease(policy);
    CFRelease(dec);
    return res;
}

static pdf_pkcs7_distinguished_name* verifier_get_signatory(fz_context* ctx, pdf_pkcs7_verifier* verifier,
                                                            unsigned char* sig, size_t sig_len) {
    (void)verifier;
    CMSDecoderRef dec = decode_signature(ctx, NULL, sig, sig_len);
    if (!dec) {
        return NULL;
    }
    SecCertificateRef cert = NULL;
    OSStatus st = CMSDecoderCopySignerCert(dec, 0, &cert);
    CFRelease(dec);
    if (st != errSecSuccess || !cert) {
        return NULL;
    }
    return cert_distinguished_name(ctx, cert);
}

static void drop_verifier(fz_context* ctx, pdf_pkcs7_verifier* verifier) {
    fz_free(ctx, verifier);
}

pdf_pkcs7_verifier* pkcs7_mac_new_verifier(fz_context* ctx) {
    pdf_pkcs7_verifier* verifier = fz_malloc_struct(ctx, pdf_pkcs7_verifier);
    verifier->drop = drop_verifier;
    verifier->check_certificate = verifier_check_certificate;
    verifier->check_digest = verifier_check_digest;
    verifier->get_signatory = verifier_get_signatory;
    return verifier;
}
