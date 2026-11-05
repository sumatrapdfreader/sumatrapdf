/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

typedef enum {
    OFD_SEAL_RASTER,
    OFD_SEAL_OFD
} ofd_seal_type;

fz_buffer* ofd_seal_picture(fz_context* ctx, fz_buffer* signed_value, ofd_seal_type* type);

typedef enum {
    OFD_CHECK_UNCHECKED,
    OFD_CHECK_OK,
    OFD_CHECK_FAILED
} ofd_check;

// what checking one SignedValue.dat found
typedef struct {
    ofd_check data;      // its hash of Signature.xml
    ofd_check signature; // its signature, with the certificate it carries
    const char* problem; // why a check failed or couldn't be made
    const char* format;
    char signer[256];
    char issuer[256];
    char seal_name[256];
    char time[40];
    char not_before[40];
    char not_after[40];
    fz_buffer* cert;
    int has_key;
    unsigned char key[64]; // the certificate's SM2 public key, x || y
} ofd_sig_check;

void ofd_verify_signed_value(fz_context* ctx, fz_buffer* signed_value, int is_seal, const unsigned char* tbs,
                             size_t tbs_len, ofd_sig_check* out);
