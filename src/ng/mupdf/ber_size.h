/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// C, for pkcs7_mac.c (and its unit test, src/ng/tests/BerSize_ut.cpp)

enum {
    BER_SIZE_CONSTRUCTED = 0x20,
    BER_SIZE_LONG_LEN = 0x80,
    // nesting of indefinite-length elements a signature can have
    BER_SIZE_MAX_DEPTH = 64,
};

static inline size_t ber_element_size_depth(const unsigned char* p, size_t n, int depth) {
    if (n < 2 || depth > BER_SIZE_MAX_DEPTH) {
        return 0;
    }
    size_t len = p[1];
    size_t header = 2;
    if (len == BER_SIZE_LONG_LEN) {
        // indefinite: only a constructed element, its children up to 00 00
        if (!(p[0] & BER_SIZE_CONSTRUCTED)) {
            return 0;
        }
        size_t off = header;
        for (;;) {
            if (n - off < 2) {
                return 0;
            }
            if (p[off] == 0 && p[off + 1] == 0) {
                return off + 2;
            }
            size_t child = ber_element_size_depth(p + off, n - off, depth + 1);
            if (child == 0) {
                return 0;
            }
            off += child;
        }
    }
    if (len & BER_SIZE_LONG_LEN) {
        size_t nLen = len & ~(size_t)BER_SIZE_LONG_LEN;
        if (nLen > sizeof(size_t) || n < 2 + nLen) {
            return 0;
        }
        len = 0;
        for (size_t i = 0; i < nLen; i++) {
            len = (len << 8) | p[2 + i];
        }
        header += nLen;
    }
    if (len > n - header) {
        return 0;
    }
    return header + len;
}

// The size of the BER element at p: DER, or BER with indefinite lengths as
// some signers write them. 0 if it's malformed or runs past n.
static inline size_t ber_element_size(const unsigned char* p, size_t n) {
    return ber_element_size_depth(p, n, 0);
}
