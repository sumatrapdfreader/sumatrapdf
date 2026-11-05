/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "mupdf/fitz.h"
#include "ofd-crypto.h"
#include "ofd-seal.h"

enum {
    OFD_DER_INTEGER = 0x02,
    OFD_DER_BIT_STRING = 0x03,
    OFD_DER_OCTET_STRING = 0x04,
    OFD_DER_OID = 0x06,
    OFD_DER_UTF8_STRING = 0x0c,
    OFD_DER_PRINTABLE_STRING = 0x13,
    OFD_DER_T61_STRING = 0x14,
    OFD_DER_IA5_STRING = 0x16,
    OFD_DER_UTC_TIME = 0x17,
    OFD_DER_GENERALIZED_TIME = 0x18,
    OFD_DER_BMP_STRING = 0x1e,
    OFD_DER_SEQUENCE = 0x30,
    OFD_DER_SET = 0x31,
    OFD_DER_CONTEXT_0 = 0xa0,
    OFD_DER_CONTEXT_1 = 0xa1,
    OFD_DER_LONG_LENGTH = 0x80
};

// OIDs as DER content bytes
static const unsigned char oid_sm2_sm3[] = {0x2A, 0x81, 0x1C, 0xCF, 0x55, 0x01, 0x83, 0x75}; // 1.2.156.10197.1.501
static const unsigned char oid_sm2_sign[] = {0x2A, 0x81, 0x1C, 0xCF, 0x55,
                                             0x01, 0x82, 0x2D, 0x01};                    // 1.2.156.10197.1.301.1
static const unsigned char oid_sm3[] = {0x2A, 0x81, 0x1C, 0xCF, 0x55, 0x01, 0x83, 0x11}; // 1.2.156.10197.1.401
static const unsigned char oid_message_digest[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x09, 0x04};
static const unsigned char oid_common_name[] = {0x55, 0x04, 0x03};
static const unsigned char oid_organization[] = {0x55, 0x04, 0x0A};

typedef struct {
    const unsigned char* data;
    size_t len;
} ofd_der;

// The next element: its content, and in *full the element with its header.
static ofd_der ofd_der_next(fz_context* ctx, ofd_der* input, int* tag, ofd_der* full) {
    if (input->len < 2) fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated OFD ASN.1 value");
    size_t header = 2;
    size_t size = input->data[1];
    if (size & OFD_DER_LONG_LENGTH) {
        size_t count = size & ~OFD_DER_LONG_LENGTH;
        if (!count || count > sizeof(size_t) || count > input->len - header)
            fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD ASN.1 length");
        size = 0;
        for (size_t i = 0; i < count; ++i) size = (size << 8) | input->data[header++];
    }
    if (size > input->len - header) fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated OFD ASN.1 value");
    if (tag) *tag = input->data[0];
    if (full) *full = (ofd_der){input->data, header + size};
    ofd_der value = {input->data + header, size};
    input->data += header + size;
    input->len -= header + size;
    return value;
}

static ofd_der ofd_der_take(fz_context* ctx, ofd_der* input, int tag) {
    if (input->len < 2 || input->data[0] != tag) fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD ASN.1 tag");
    return ofd_der_next(ctx, input, NULL, NULL);
}

static int ofd_der_peek(ofd_der* input) {
    return input->len ? input->data[0] : -1;
}

static int ofd_der_is(ofd_der v, const unsigned char* bytes, size_t len) {
    return v.len == len && !memcmp(v.data, bytes, len);
}

static void ofd_seal_dimension(fz_context* ctx, ofd_der* input) {
    ofd_der value = ofd_der_take(ctx, input, OFD_DER_INTEGER);
    if (!value.len || value.len > sizeof(int) || (value.data[0] & 0x80))
        fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD seal dimensions");
    unsigned int n = 0;
    for (size_t i = 0; i < value.len; ++i) n = (n << 8) | value.data[i];
    if (!n) fz_throw(ctx, FZ_ERROR_FORMAT, "Empty OFD seal dimensions");
}

/* Read the SES picture field only; decoding artwork does not verify the signature. */
fz_buffer* ofd_seal_picture(fz_context* ctx, fz_buffer* signed_value, ofd_seal_type* type) {
    unsigned char* data;
    size_t size = fz_buffer_storage(ctx, signed_value, &data);
    ofd_der input = {data, size};
    ofd_der signature = ofd_der_take(ctx, &input, OFD_DER_SEQUENCE);
    ofd_der to_sign = ofd_der_take(ctx, &signature, OFD_DER_SEQUENCE);
    ofd_der_take(ctx, &to_sign, OFD_DER_INTEGER);
    ofd_der seal = ofd_der_take(ctx, &to_sign, OFD_DER_SEQUENCE);
    ofd_der info = ofd_der_take(ctx, &seal, OFD_DER_SEQUENCE);
    ofd_der header = ofd_der_take(ctx, &info, OFD_DER_SEQUENCE);
    ofd_der id = ofd_der_take(ctx, &header, OFD_DER_IA5_STRING);
    if (id.len != 2 || memcmp(id.data, "ES", 2)) fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD electronic seal header");
    ofd_der_take(ctx, &info, OFD_DER_IA5_STRING);
    ofd_der_take(ctx, &info, OFD_DER_SEQUENCE);
    ofd_der picture = ofd_der_take(ctx, &info, OFD_DER_SEQUENCE);
    ofd_der format = ofd_der_take(ctx, &picture, OFD_DER_IA5_STRING);
    ofd_der image = ofd_der_take(ctx, &picture, OFD_DER_OCTET_STRING);
    ofd_seal_dimension(ctx, &picture);
    ofd_seal_dimension(ctx, &picture);
    if (picture.len || !image.len) fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD seal picture");
    *type = format.len == 3 && !fz_strncasecmp((const char*)format.data, "ofd", 3) ? OFD_SEAL_OFD : OFD_SEAL_RASTER;
    return fz_new_buffer_from_copied_data(ctx, image.data, image.len);
}

//--- verification

// A string value as UTF-8; BMPString is UTF-16BE.
static void ofd_der_string(char* out, size_t size, int tag, ofd_der v) {
    size_t n = 0;
    if (tag == OFD_DER_BMP_STRING) {
        for (size_t i = 0; i + 1 < v.len && n + 4 < size; i += 2) {
            char utf8[8];
            int rune = (v.data[i] << 8) | v.data[i + 1];
            int len = fz_runetochar(utf8, rune);
            memcpy(out + n, utf8, len);
            n += len;
        }
    } else {
        n = v.len < size - 1 ? v.len : size - 1;
        memcpy(out, v.data, n);
    }
    out[n] = 0;
}

// keeps the first problem: it is the one that explains the others
static void ofd_set_problem(ofd_sig_check* out, const char* problem) {
    if (!out->problem) out->problem = problem;
}

static int ofd_der_is_string(int tag) {
    return tag == OFD_DER_UTF8_STRING || tag == OFD_DER_PRINTABLE_STRING || tag == OFD_DER_T61_STRING ||
           tag == OFD_DER_IA5_STRING || tag == OFD_DER_BMP_STRING;
}

// The value of an attribute in an X.501 Name: SEQUENCE OF SET OF SEQUENCE { OID, value }
static int ofd_name_attr(fz_context* ctx, ofd_der name, const unsigned char* oid, size_t oid_len, char* out,
                         size_t size) {
    while (name.len) {
        ofd_der set = ofd_der_take(ctx, &name, OFD_DER_SET);
        while (set.len) {
            ofd_der attr = ofd_der_take(ctx, &set, OFD_DER_SEQUENCE);
            ofd_der type = ofd_der_take(ctx, &attr, OFD_DER_OID);
            int tag;
            ofd_der value = ofd_der_next(ctx, &attr, &tag, NULL);
            if (!ofd_der_is(type, oid, oid_len) || !ofd_der_is_string(tag)) continue;
            ofd_der_string(out, size, tag, value);
            return 1;
        }
    }
    return 0;
}

static void ofd_name(fz_context* ctx, ofd_der name, char* out, size_t size) {
    if (!ofd_name_attr(ctx, name, oid_common_name, sizeof(oid_common_name), out, size))
        ofd_name_attr(ctx, name, oid_organization, sizeof(oid_organization), out, size);
}

// "20260804020809Z" (GeneralizedTime) or "260804020809Z" (UTCTime) as "2026-08-04 02:08:09 UTC"
static void ofd_der_time(int tag, ofd_der v, char* out, size_t size) {
    char digits[16] = {0};
    size_t n = 0;
    if (tag == OFD_DER_UTC_TIME && v.len >= 12) {
        digits[0] = v.data[0] < '5' ? '2' : '1';
        digits[1] = v.data[0] < '5' ? '0' : '9';
        n = 2;
    } else if (tag != OFD_DER_GENERALIZED_TIME || v.len < 14) {
        out[0] = 0;
        return;
    }
    for (size_t i = 0; i < v.len && n < 14; ++i) {
        if (v.data[i] < '0' || v.data[i] > '9') break;
        digits[n++] = (char)v.data[i];
    }
    if (n < 14) {
        out[0] = 0;
        return;
    }
    fz_snprintf(out, size, "%.4s-%.2s-%.2s %.2s:%.2s:%.2s UTC", digits, digits + 4, digits + 6, digits + 8, digits + 10,
                digits + 12);
}

// Reads what we show of a certificate, and its SM2 public key (x || y)
static void ofd_parse_cert(fz_context* ctx, ofd_der cert, ofd_sig_check* out) {
    ofd_der input = cert;
    ofd_der body = ofd_der_take(ctx, &input, OFD_DER_SEQUENCE);
    ofd_der tbs = ofd_der_take(ctx, &body, OFD_DER_SEQUENCE);
    if (ofd_der_peek(&tbs) == OFD_DER_CONTEXT_0) ofd_der_next(ctx, &tbs, NULL, NULL); // version
    ofd_der_take(ctx, &tbs, OFD_DER_INTEGER);
    ofd_der_take(ctx, &tbs, OFD_DER_SEQUENCE);
    ofd_name(ctx, ofd_der_take(ctx, &tbs, OFD_DER_SEQUENCE), out->issuer, sizeof(out->issuer));
    ofd_der validity = ofd_der_take(ctx, &tbs, OFD_DER_SEQUENCE);
    int tag;
    ofd_der t = ofd_der_next(ctx, &validity, &tag, NULL);
    ofd_der_time(tag, t, out->not_before, sizeof(out->not_before));
    t = ofd_der_next(ctx, &validity, &tag, NULL);
    ofd_der_time(tag, t, out->not_after, sizeof(out->not_after));
    ofd_name(ctx, ofd_der_take(ctx, &tbs, OFD_DER_SEQUENCE), out->signer, sizeof(out->signer));
    ofd_der spki = ofd_der_take(ctx, &tbs, OFD_DER_SEQUENCE);
    ofd_der_take(ctx, &spki, OFD_DER_SEQUENCE);
    ofd_der key = ofd_der_take(ctx, &spki, OFD_DER_BIT_STRING);
    fz_drop_buffer(ctx, out->cert);
    out->cert = fz_new_buffer_from_copied_data(ctx, cert.data, cert.len);
    // unused-bits byte, then an uncompressed point 04 || x || y
    out->has_key = key.len == 66 && key.data[0] == 0 && key.data[1] == 4;
    if (out->has_key) memcpy(out->key, key.data + 2, sizeof(out->key));
}

// An SM2 signature: DER SEQUENCE { r, s } or raw r || s, possibly in a BIT STRING
static int ofd_parse_sm2_sig(fz_context* ctx, ofd_der v, int is_bit_string, unsigned char r[32], unsigned char s[32]) {
    if (is_bit_string) {
        if (!v.len || v.data[0] != 0) return 0;
        v.data++;
        v.len--;
    }
    if (v.len == 64) {
        memcpy(r, v.data, 32);
        memcpy(s, v.data + 32, 32);
        return 1;
    }
    ofd_der seq = ofd_der_take(ctx, &v, OFD_DER_SEQUENCE);
    unsigned char* outs[2] = {r, s};
    for (int i = 0; i < 2; ++i) {
        ofd_der n = ofd_der_take(ctx, &seq, OFD_DER_INTEGER);
        while (n.len > 32 && n.data[0] == 0) {
            n.data++;
            n.len--;
        }
        if (n.len > 32) return 0;
        memset(outs[i], 0, 32);
        memcpy(outs[i] + 32 - n.len, n.data, n.len);
    }
    return 1;
}

// needs ofd_parse_cert() first
static void ofd_check_sm2(fz_context* ctx, ofd_sig_check* out, ofd_der alg, ofd_der sig, int sig_is_bits,
                          ofd_der signed_bytes) {
    unsigned char r[32], s[32];
    if (!ofd_der_is(alg, oid_sm2_sm3, sizeof(oid_sm2_sm3)) && !ofd_der_is(alg, oid_sm2_sign, sizeof(oid_sm2_sign))) {
        out->signature = OFD_CHECK_UNCHECKED;
        ofd_set_problem(out, "the signature algorithm isn't supported");
        return;
    }
    if (!out->has_key) {
        out->signature = OFD_CHECK_UNCHECKED;
        ofd_set_problem(out, "the certificate's key isn't an SM2 key");
        return;
    }
    if (!ofd_parse_sm2_sig(ctx, sig, sig_is_bits, r, s)) {
        out->signature = OFD_CHECK_FAILED;
        ofd_set_problem(out, "the signature value is malformed");
        return;
    }
    out->signature =
        ofd_sm2_verify(out->key, signed_bytes.data, signed_bytes.len, r, s) ? OFD_CHECK_OK : OFD_CHECK_FAILED;
    if (out->signature == OFD_CHECK_FAILED)
        ofd_set_problem(out, "the signature doesn't match the signer's certificate");
}

// expected is SM3(tbs), or SM3(ZA || tbs) with the signer's key; needs ofd_parse_cert() first
static void ofd_check_hash(ofd_sig_check* out, ofd_der expected, const unsigned char* tbs, size_t tbs_len) {
    unsigned char digest[OFD_SM3_SIZE];
    int ok = 0;
    if (expected.len == sizeof(digest)) {
        ofd_sm3(tbs, tbs_len, digest);
        ok = !memcmp(expected.data, digest, sizeof(digest));
        if (!ok && out->has_key) {
            ofd_sm3_with_key(out->key, tbs, tbs_len, digest);
            ok = !memcmp(expected.data, digest, sizeof(digest));
        }
    }
    out->data = ok ? OFD_CHECK_OK : OFD_CHECK_FAILED;
    if (out->data == OFD_CHECK_FAILED) ofd_set_problem(out, "Signature.xml was changed after signing");
}

// SES_Signature, GM/T 0031 v1 or v4 / GB/T 38540 (v5): an SM2 signature over TBS_Sign, which
// carries the SM3 hash of Signature.xml.
static void ofd_verify_ses(fz_context* ctx, ofd_der input, const unsigned char* tbs, size_t tbs_len,
                           ofd_sig_check* out) {
    ofd_der sig = ofd_der_take(ctx, &input, OFD_DER_SEQUENCE);
    int tag;
    ofd_der to_sign_full;
    ofd_der to_sign = ofd_der_next(ctx, &sig, &tag, &to_sign_full);
    if (tag != OFD_DER_SEQUENCE) fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD seal signature");
    ofd_der_take(ctx, &to_sign, OFD_DER_INTEGER);
    ofd_der eseal = ofd_der_take(ctx, &to_sign, OFD_DER_SEQUENCE);

    // seal name: esealInfo { header, esID, property { type, name, ... }, picture, ... }
    ofd_der info = ofd_der_take(ctx, &eseal, OFD_DER_SEQUENCE);
    ofd_der_take(ctx, &info, OFD_DER_SEQUENCE);
    ofd_der_take(ctx, &info, OFD_DER_IA5_STRING);
    ofd_der property = ofd_der_take(ctx, &info, OFD_DER_SEQUENCE);
    ofd_der_take(ctx, &property, OFD_DER_INTEGER);
    ofd_der name = ofd_der_next(ctx, &property, &tag, NULL);
    if (ofd_der_is_string(tag)) ofd_der_string(out->seal_name, sizeof(out->seal_name), tag, name);

    ofd_der cert, alg, value;
    ofd_der time = ofd_der_next(ctx, &to_sign, &tag, NULL);
    if (tag == OFD_DER_BIT_STRING) {
        // v1: timeInfo, dataHash, propertyInfo, cert, signatureAlgorithm inside TBS_Sign
        out->format = "electronic seal (SES v1)";
        ofd_der hash = ofd_der_take(ctx, &to_sign, OFD_DER_BIT_STRING);
        ofd_der_take(ctx, &to_sign, OFD_DER_IA5_STRING);
        cert = ofd_der_take(ctx, &to_sign, OFD_DER_OCTET_STRING);
        alg = ofd_der_take(ctx, &to_sign, OFD_DER_OID);
        value = ofd_der_take(ctx, &sig, OFD_DER_BIT_STRING);
        ofd_parse_cert(ctx, cert, out);
        if (hash.len) ofd_check_hash(out, (ofd_der){hash.data + 1, hash.len - 1}, tbs, tbs_len);
    } else {
        // v4 / v5: cert, algorithm and signature after TBS_Sign
        out->format = "electronic seal (SES)";
        ofd_der_time(tag, time, out->time, sizeof(out->time));
        ofd_der hash = ofd_der_take(ctx, &to_sign, OFD_DER_BIT_STRING);
        cert = ofd_der_take(ctx, &sig, OFD_DER_OCTET_STRING);
        alg = ofd_der_take(ctx, &sig, OFD_DER_OID);
        value = ofd_der_take(ctx, &sig, OFD_DER_BIT_STRING);
        ofd_parse_cert(ctx, cert, out);
        if (hash.len) ofd_check_hash(out, (ofd_der){hash.data + 1, hash.len - 1}, tbs, tbs_len);
    }
    ofd_check_sm2(ctx, out, alg, value, 1, to_sign_full);
}

// GB/T 35275: PKCS#7 SignedData with SM2 / SM3, detached over Signature.xml
static void ofd_verify_pkcs7(fz_context* ctx, ofd_der input, const unsigned char* tbs, size_t tbs_len,
                             ofd_sig_check* out) {
    out->format = "digital signature (GB/T 35275)";
    ofd_der ci = ofd_der_take(ctx, &input, OFD_DER_SEQUENCE);
    ofd_der_take(ctx, &ci, OFD_DER_OID);
    ofd_der explicit0 = ofd_der_take(ctx, &ci, OFD_DER_CONTEXT_0);
    ofd_der sd = ofd_der_take(ctx, &explicit0, OFD_DER_SEQUENCE);
    ofd_der_take(ctx, &sd, OFD_DER_INTEGER);
    ofd_der_take(ctx, &sd, OFD_DER_SET);
    ofd_der content_info = ofd_der_take(ctx, &sd, OFD_DER_SEQUENCE);
    ofd_der content = {tbs, tbs_len};
    ofd_der_take(ctx, &content_info, OFD_DER_OID);
    if (ofd_der_peek(&content_info) == OFD_DER_CONTEXT_0) {
        ofd_der wrapped = ofd_der_take(ctx, &content_info, OFD_DER_CONTEXT_0);
        content = ofd_der_take(ctx, &wrapped, OFD_DER_OCTET_STRING);
    }
    ofd_der cert = {NULL, 0};
    if (ofd_der_peek(&sd) == OFD_DER_CONTEXT_0) {
        ofd_der certs = ofd_der_take(ctx, &sd, OFD_DER_CONTEXT_0);
        ofd_der_next(ctx, &certs, NULL, &cert);
    }
    if (ofd_der_peek(&sd) == OFD_DER_CONTEXT_1) ofd_der_next(ctx, &sd, NULL, NULL);
    ofd_der infos = ofd_der_take(ctx, &sd, OFD_DER_SET);
    ofd_der si = ofd_der_take(ctx, &infos, OFD_DER_SEQUENCE);
    ofd_der_take(ctx, &si, OFD_DER_INTEGER);
    ofd_der_next(ctx, &si, NULL, NULL); // issuerAndSerialNumber or subjectKeyIdentifier
    ofd_der digest_alg = ofd_der_take(ctx, &si, OFD_DER_SEQUENCE);
    ofd_der digest_oid = ofd_der_take(ctx, &digest_alg, OFD_DER_OID);
    ofd_der signed_bytes = content;
    if (cert.len) ofd_parse_cert(ctx, cert, out);
    unsigned char* attrs_copy = NULL;
    fz_var(attrs_copy);
    fz_try(ctx) {
        if (ofd_der_peek(&si) == OFD_DER_CONTEXT_0) {
            // signed attributes: messageDigest is the content's hash; the signature covers them as a SET
            ofd_der attrs_full;
            ofd_der attrs = ofd_der_next(ctx, &si, NULL, &attrs_full);
            int supported = ofd_der_is(digest_oid, oid_sm3, sizeof(oid_sm3));
            if (!supported) ofd_set_problem(out, "the digest algorithm isn't supported");
            while (attrs.len && supported) {
                ofd_der attr = ofd_der_take(ctx, &attrs, OFD_DER_SEQUENCE);
                ofd_der type = ofd_der_take(ctx, &attr, OFD_DER_OID);
                if (!ofd_der_is(type, oid_message_digest, sizeof(oid_message_digest))) continue;
                ofd_der values = ofd_der_take(ctx, &attr, OFD_DER_SET);
                ofd_check_hash(out, ofd_der_take(ctx, &values, OFD_DER_OCTET_STRING), content.data, content.len);
            }
            attrs_copy = fz_malloc(ctx, attrs_full.len);
            memcpy(attrs_copy, attrs_full.data, attrs_full.len);
            attrs_copy[0] = OFD_DER_SET;
            signed_bytes = (ofd_der){attrs_copy, attrs_full.len};
        }
        ofd_der enc_alg = ofd_der_take(ctx, &si, OFD_DER_SEQUENCE);
        ofd_der alg = ofd_der_take(ctx, &enc_alg, OFD_DER_OID);
        ofd_der value = ofd_der_take(ctx, &si, OFD_DER_OCTET_STRING);
        if (!cert.len) {
            out->signature = OFD_CHECK_UNCHECKED;
            ofd_set_problem(out, "the signature has no certificate");
        } else {
            ofd_check_sm2(ctx, out, alg, value, 0, signed_bytes);
        }
    }
    fz_always(ctx) {
        fz_free(ctx, attrs_copy);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
}

static int ofd_is_pkcs7(ofd_der input) {
    // SEQUENCE { OID signedData, ... }: the first element inside is an OID
    if (input.len < 4 || input.data[0] != OFD_DER_SEQUENCE) return 0;
    size_t header = 2 + ((input.data[1] & OFD_DER_LONG_LENGTH) ? (input.data[1] & 0x7f) : 0);
    return header < input.len && input.data[header] == OFD_DER_OID;
}

// Checks SignedValue.dat against the Signature.xml bytes it signs (tbs). is_seal: Type="Seal"
void ofd_verify_signed_value(fz_context* ctx, fz_buffer* signed_value, int is_seal, const unsigned char* tbs,
                             size_t tbs_len, ofd_sig_check* out) {
    unsigned char* data;
    size_t size = fz_buffer_storage(ctx, signed_value, &data);
    ofd_der input = {data, size};
    out->data = OFD_CHECK_UNCHECKED;
    out->signature = OFD_CHECK_UNCHECKED;
    fz_try(ctx) {
        if (is_seal) {
            ofd_verify_ses(ctx, input, tbs, tbs_len, out);
        } else if (ofd_is_pkcs7(input)) {
            ofd_verify_pkcs7(ctx, input, tbs, tbs_len, out);
        } else {
            // a bare signature value: the verifier is expected to know the key
            out->format = "digital signature";
            ofd_set_problem(out, "the signature has no certificate");
        }
    }
    fz_catch(ctx) {
        fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
        fz_report_error(ctx);
        out->signature = OFD_CHECK_UNCHECKED;
        ofd_set_problem(out, "the signature data couldn't be read");
    }
}
