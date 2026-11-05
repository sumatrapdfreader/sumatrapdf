/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "mupdf/fitz.h"
#include "ofd-seal.h"

enum {
    OFD_DER_INTEGER = 0x02,
    OFD_DER_OCTET_STRING = 0x04,
    OFD_DER_IA5_STRING = 0x16,
    OFD_DER_SEQUENCE = 0x30,
    OFD_DER_LONG_LENGTH = 0x80
};

typedef struct {
    const unsigned char* data;
    size_t len;
} ofd_der;

static ofd_der ofd_der_take(fz_context* ctx, ofd_der* input, int tag) {
    if (input->len < 2 || input->data[0] != tag) fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD seal ASN.1 tag");
    size_t header = 2;
    size_t size = input->data[1];
    if (size & OFD_DER_LONG_LENGTH) {
        size_t count = size & ~OFD_DER_LONG_LENGTH;
        if (!count || count > sizeof(size_t) || count > input->len - header)
            fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD seal ASN.1 length");
        size = 0;
        for (size_t i = 0; i < count; ++i) size = (size << 8) | input->data[header++];
    }
    if (size > input->len - header) fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated OFD seal ASN.1 value");
    ofd_der value = {input->data + header, size};
    input->data += header + size;
    input->len -= header + size;
    return value;
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
