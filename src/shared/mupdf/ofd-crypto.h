/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum {
    OFD_SM3_SIZE = 32,
    OFD_SM2_COORD_SIZE = 32
};

typedef struct {
    unsigned int state[8];
    unsigned char block[64];
    unsigned long long count;
} ofd_sm3_ctx;

void ofd_sm3_init(ofd_sm3_ctx* c);
void ofd_sm3_update(ofd_sm3_ctx* c, const unsigned char* data, size_t len);
void ofd_sm3_final(ofd_sm3_ctx* c, unsigned char out[OFD_SM3_SIZE]);
void ofd_sm3(const unsigned char* data, size_t len, unsigned char out[OFD_SM3_SIZE]);
void ofd_sm3_with_key(const unsigned char pub[2 * OFD_SM2_COORD_SIZE], const unsigned char* data, size_t len,
                      unsigned char out[OFD_SM3_SIZE]);

int ofd_sm2_verify(const unsigned char pub[2 * OFD_SM2_COORD_SIZE], const unsigned char* msg, size_t msg_len,
                   const unsigned char r[OFD_SM2_COORD_SIZE], const unsigned char s[OFD_SM2_COORD_SIZE]);

enum {
    OFD_SM4_BLOCK = 16
};

void ofd_sm3_kdf(const unsigned char* z, size_t z_len, unsigned char* out, size_t out_len);
size_t ofd_sm4_cbc_decrypt(const unsigned char key[OFD_SM4_BLOCK], const unsigned char iv[OFD_SM4_BLOCK],
                           unsigned char* data, size_t len);
