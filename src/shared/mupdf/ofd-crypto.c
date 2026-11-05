/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// SM3 hash (GB/T 32905) and SM2 signature verification (GB/T 32918), what OFD
// signatures use. Verification only: nothing here handles secrets, so the
// arithmetic doesn't need to be constant time.

#include <string.h>
#include "ofd-crypto.h"

//--- SM3

static unsigned int sm3_rotl(unsigned int x, unsigned int n) {
    n &= 31;
    return n ? (x << n) | (x >> (32 - n)) : x;
}

static unsigned int sm3_p0(unsigned int x) {
    return x ^ sm3_rotl(x, 9) ^ sm3_rotl(x, 17);
}

static unsigned int sm3_p1(unsigned int x) {
    return x ^ sm3_rotl(x, 15) ^ sm3_rotl(x, 23);
}

static void sm3_compress(unsigned int v[8], const unsigned char* block) {
    unsigned int w[68], w1[64];
    for (int j = 0; j < 16; ++j) {
        const unsigned char* b = block + 4 * j;
        w[j] = ((unsigned int)b[0] << 24) | ((unsigned int)b[1] << 16) | ((unsigned int)b[2] << 8) | b[3];
    }
    for (int j = 16; j < 68; ++j) {
        w[j] = sm3_p1(w[j - 16] ^ w[j - 9] ^ sm3_rotl(w[j - 3], 15)) ^ sm3_rotl(w[j - 13], 7) ^ w[j - 6];
    }
    for (int j = 0; j < 64; ++j) w1[j] = w[j] ^ w[j + 4];

    unsigned int a = v[0], b = v[1], c = v[2], d = v[3], e = v[4], f = v[5], g = v[6], h = v[7];
    for (int j = 0; j < 64; ++j) {
        unsigned int t = j < 16 ? 0x79cc4519 : 0x7a879d8a;
        unsigned int ss1 = sm3_rotl(sm3_rotl(a, 12) + e + sm3_rotl(t, (unsigned int)j), 7);
        unsigned int ss2 = ss1 ^ sm3_rotl(a, 12);
        unsigned int ff = j < 16 ? a ^ b ^ c : (a & b) | (a & c) | (b & c);
        unsigned int gg = j < 16 ? e ^ f ^ g : (e & f) | (~e & g);
        unsigned int tt1 = ff + d + ss2 + w1[j];
        unsigned int tt2 = gg + h + ss1 + w[j];
        d = c;
        c = sm3_rotl(b, 9);
        b = a;
        a = tt1;
        h = g;
        g = sm3_rotl(f, 19);
        f = e;
        e = sm3_p0(tt2);
    }
    v[0] ^= a;
    v[1] ^= b;
    v[2] ^= c;
    v[3] ^= d;
    v[4] ^= e;
    v[5] ^= f;
    v[6] ^= g;
    v[7] ^= h;
}

void ofd_sm3_init(ofd_sm3_ctx* c) {
    static const unsigned int iv[8] = {0x7380166f, 0x4914b2b9, 0x172442d7, 0xda8a0600,
                                       0xa96f30bc, 0x163138aa, 0xe38dee4d, 0xb0fb0e4e};
    memcpy(c->state, iv, sizeof(iv));
    c->count = 0;
}

void ofd_sm3_update(ofd_sm3_ctx* c, const unsigned char* data, size_t len) {
    while (len > 0) {
        size_t used = (size_t)(c->count & 63);
        size_t n = 64 - used < len ? 64 - used : len;
        memcpy(c->block + used, data, n);
        c->count += n;
        data += n;
        len -= n;
        if ((c->count & 63) == 0) sm3_compress(c->state, c->block);
    }
}

void ofd_sm3_final(ofd_sm3_ctx* c, unsigned char out[OFD_SM3_SIZE]) {
    unsigned long long bits = c->count * 8;
    unsigned char pad = 0x80;
    ofd_sm3_update(c, &pad, 1);
    pad = 0;
    while ((c->count & 63) != 56) ofd_sm3_update(c, &pad, 1);
    unsigned char len[8];
    for (int i = 0; i < 8; ++i) len[i] = (unsigned char)(bits >> (56 - 8 * i));
    ofd_sm3_update(c, len, 8);
    for (int i = 0; i < 8; ++i) {
        out[4 * i] = (unsigned char)(c->state[i] >> 24);
        out[4 * i + 1] = (unsigned char)(c->state[i] >> 16);
        out[4 * i + 2] = (unsigned char)(c->state[i] >> 8);
        out[4 * i + 3] = (unsigned char)c->state[i];
    }
}

void ofd_sm3(const unsigned char* data, size_t len, unsigned char out[OFD_SM3_SIZE]) {
    ofd_sm3_ctx c;
    ofd_sm3_init(&c);
    ofd_sm3_update(&c, data, len);
    ofd_sm3_final(&c, out);
}

//--- 256-bit numbers: 8 little-endian 32-bit limbs

enum {
    BN_LIMBS = 8
};

typedef struct {
    unsigned int v[BN_LIMBS];
} bn;

static void bn_from_bytes(bn* r, const unsigned char* b) {
    for (int i = 0; i < BN_LIMBS; ++i) {
        const unsigned char* p = b + 4 * (BN_LIMBS - 1 - i);
        r->v[i] = ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3];
    }
}

static void bn_to_bytes(unsigned char* b, const bn* a) {
    for (int i = 0; i < BN_LIMBS; ++i) {
        unsigned int x = a->v[BN_LIMBS - 1 - i];
        b[4 * i] = (unsigned char)(x >> 24);
        b[4 * i + 1] = (unsigned char)(x >> 16);
        b[4 * i + 2] = (unsigned char)(x >> 8);
        b[4 * i + 3] = (unsigned char)x;
    }
}

static int bn_cmp(const bn* a, const bn* b) {
    for (int i = BN_LIMBS - 1; i >= 0; --i) {
        if (a->v[i] != b->v[i]) return a->v[i] < b->v[i] ? -1 : 1;
    }
    return 0;
}

static int bn_is_zero(const bn* a) {
    unsigned int x = 0;
    for (int i = 0; i < BN_LIMBS; ++i) x |= a->v[i];
    return x == 0;
}

// r = a + b, returns the carry
static unsigned int bn_add(bn* r, const bn* a, const bn* b) {
    unsigned long long c = 0;
    for (int i = 0; i < BN_LIMBS; ++i) {
        c += (unsigned long long)a->v[i] + b->v[i];
        r->v[i] = (unsigned int)c;
        c >>= 32;
    }
    return (unsigned int)c;
}

// r = a - b, returns the borrow
static unsigned int bn_sub(bn* r, const bn* a, const bn* b) {
    long long c = 0;
    for (int i = 0; i < BN_LIMBS; ++i) {
        c += (long long)a->v[i] - b->v[i];
        r->v[i] = (unsigned int)c;
        c >>= 32;
    }
    return c ? 1 : 0;
}

static int bn_bit(const bn* a, int i) {
    return (a->v[i / 32] >> (i % 32)) & 1;
}

// r = (a + b) mod m, for a, b < m
static void bn_mod_add(bn* r, const bn* a, const bn* b, const bn* m) {
    unsigned int carry = bn_add(r, a, b);
    if (carry || bn_cmp(r, m) >= 0) bn_sub(r, r, m);
}

// r = (a - b) mod m, for a, b < m
static void bn_mod_sub(bn* r, const bn* a, const bn* b, const bn* m) {
    if (bn_sub(r, a, b)) bn_add(r, r, m);
}

//--- SM2 curve: y^2 = x^3 + ax + b over GF(p), a = p - 3

static const unsigned char sm2_p[32] = {0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
                                        0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const unsigned char sm2_a[32] = {0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
                                        0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC};
static const unsigned char sm2_b[32] = {0x28, 0xE9, 0xFA, 0x9E, 0x9D, 0x9F, 0x5E, 0x34, 0x4D, 0x5A, 0x9E,
                                        0x4B, 0xCF, 0x65, 0x09, 0xA7, 0xF3, 0x97, 0x89, 0xF5, 0x15, 0xAB,
                                        0x8F, 0x92, 0xDD, 0xBC, 0xBD, 0x41, 0x4D, 0x94, 0x0E, 0x93};
static const unsigned char sm2_n[32] = {0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x72, 0x03, 0xDF, 0x6B, 0x21, 0xC6,
                                        0x05, 0x2B, 0x53, 0xBB, 0xF4, 0x09, 0x39, 0xD5, 0x41, 0x23};
static const unsigned char sm2_gx[32] = {0x32, 0xC4, 0xAE, 0x2C, 0x1F, 0x19, 0x81, 0x19, 0x5F, 0x99, 0x04,
                                         0x46, 0x6A, 0x39, 0xC9, 0x94, 0x8F, 0xE3, 0x0B, 0xBF, 0xF2, 0x66,
                                         0x0B, 0xE1, 0x71, 0x5A, 0x45, 0x89, 0x33, 0x4C, 0x74, 0xC7};
static const unsigned char sm2_gy[32] = {0xBC, 0x37, 0x36, 0xA2, 0xF4, 0xF6, 0x77, 0x9C, 0x59, 0xBD, 0xCE,
                                         0xE3, 0x6B, 0x69, 0x21, 0x53, 0xD0, 0xA9, 0x87, 0x7C, 0xC6, 0x2A,
                                         0x47, 0x40, 0x02, 0xDF, 0x32, 0xE5, 0x21, 0x39, 0xF0, 0xA0};

// field arithmetic mod p in Montgomery form (R = 2^256)
typedef struct {
    bn p;
    bn r2;             // R^2 mod p
    unsigned int pinv; // -p^-1 mod 2^32
} field;

static void fe_mul(const field* f, bn* r, const bn* a, const bn* b) {
    unsigned int t[BN_LIMBS + 2] = {0};
    for (int i = 0; i < BN_LIMBS; ++i) {
        unsigned long long c = 0;
        for (int j = 0; j < BN_LIMBS; ++j) {
            c += (unsigned long long)t[j] + (unsigned long long)a->v[j] * b->v[i];
            t[j] = (unsigned int)c;
            c >>= 32;
        }
        c += t[BN_LIMBS];
        t[BN_LIMBS] = (unsigned int)c;
        t[BN_LIMBS + 1] = (unsigned int)(c >> 32);

        unsigned int m = t[0] * f->pinv;
        c = (unsigned long long)t[0] + (unsigned long long)m * f->p.v[0];
        c >>= 32;
        for (int j = 1; j < BN_LIMBS; ++j) {
            c += (unsigned long long)t[j] + (unsigned long long)m * f->p.v[j];
            t[j - 1] = (unsigned int)c;
            c >>= 32;
        }
        c += t[BN_LIMBS];
        t[BN_LIMBS - 1] = (unsigned int)c;
        t[BN_LIMBS] = t[BN_LIMBS + 1] + (unsigned int)(c >> 32);
    }
    memcpy(r->v, t, sizeof(r->v));
    if (t[BN_LIMBS] || bn_cmp(r, &f->p) >= 0) bn_sub(r, r, &f->p);
}

static void fe_sqr(const field* f, bn* r, const bn* a) {
    fe_mul(f, r, a, a);
}

static void fe_add(const field* f, bn* r, const bn* a, const bn* b) {
    bn_mod_add(r, a, b, &f->p);
}

static void fe_sub(const field* f, bn* r, const bn* a, const bn* b) {
    bn_mod_sub(r, a, b, &f->p);
}

static void fe_to_mont(const field* f, bn* r, const bn* a) {
    fe_mul(f, r, a, &f->r2);
}

static void fe_from_mont(const field* f, bn* r, const bn* a) {
    bn one = {{1}};
    fe_mul(f, r, a, &one);
}

// r = a^(p-2) = a^-1 (Fermat), all in Montgomery form
static void fe_inv(const field* f, bn* r, const bn* a) {
    bn e, two = {{2}};
    bn_sub(&e, &f->p, &two);
    bn x;
    fe_to_mont(f, &x, &(bn){{1}});
    for (int i = 255; i >= 0; --i) {
        fe_sqr(f, &x, &x);
        if (bn_bit(&e, i)) fe_mul(f, &x, &x, a);
    }
    *r = x;
}

static void field_init(field* f) {
    bn_from_bytes(&f->p, sm2_p);
    unsigned int inv = 1;
    for (int i = 0; i < 5; ++i) inv *= 2 - f->p.v[0] * inv;
    f->pinv = 0u - inv;
    // R mod p = 2^256 - p, as p > 2^255; doubling it 256 times gives R^2 mod p
    bn zero = {{0}};
    bn_sub(&f->r2, &zero, &f->p);
    for (int i = 0; i < 256; ++i) bn_mod_add(&f->r2, &f->r2, &f->r2, &f->p);
}

// Jacobian point (X/Z^2, Y/Z^3), coordinates in Montgomery form; Z = 0 is infinity
typedef struct {
    bn x, y, z;
} point;

static void pt_double(const field* f, point* r, const point* p) {
    if (bn_is_zero(&p->z)) {
        *r = *p;
        return;
    }
    // dbl-2001-b, for a = -3
    bn delta, gamma, beta, alpha, t1, t2, x3, y3, z3;
    fe_sqr(f, &delta, &p->z);
    fe_sqr(f, &gamma, &p->y);
    fe_mul(f, &beta, &p->x, &gamma);
    fe_sub(f, &t1, &p->x, &delta);
    fe_add(f, &t2, &p->x, &delta);
    fe_mul(f, &alpha, &t1, &t2);
    fe_add(f, &t1, &alpha, &alpha);
    fe_add(f, &alpha, &t1, &alpha);
    fe_sqr(f, &x3, &alpha);
    fe_add(f, &t1, &beta, &beta); // 2 beta
    fe_add(f, &t1, &t1, &t1);     // 4 beta
    fe_add(f, &t2, &t1, &t1);     // 8 beta
    fe_sub(f, &x3, &x3, &t2);
    fe_add(f, &z3, &p->y, &p->z);
    fe_sqr(f, &z3, &z3);
    fe_sub(f, &z3, &z3, &gamma);
    fe_sub(f, &z3, &z3, &delta);
    fe_sub(f, &t1, &t1, &x3);
    fe_mul(f, &y3, &alpha, &t1);
    fe_sqr(f, &t2, &gamma);
    fe_add(f, &t2, &t2, &t2);
    fe_add(f, &t2, &t2, &t2);
    fe_add(f, &t2, &t2, &t2); // 8 gamma^2
    fe_sub(f, &y3, &y3, &t2);
    r->x = x3;
    r->y = y3;
    r->z = z3;
}

static void pt_add(const field* f, point* r, const point* p, const point* q) {
    if (bn_is_zero(&p->z)) {
        *r = *q;
        return;
    }
    if (bn_is_zero(&q->z)) {
        *r = *p;
        return;
    }
    bn z1z1, z2z2, u1, u2, s1, s2, h, rr, hh, hhh, v, t, x3, y3, z3;
    fe_sqr(f, &z1z1, &p->z);
    fe_sqr(f, &z2z2, &q->z);
    fe_mul(f, &u1, &p->x, &z2z2);
    fe_mul(f, &u2, &q->x, &z1z1);
    fe_mul(f, &s1, &p->y, &q->z);
    fe_mul(f, &s1, &s1, &z2z2);
    fe_mul(f, &s2, &q->y, &p->z);
    fe_mul(f, &s2, &s2, &z1z1);
    fe_sub(f, &h, &u2, &u1);
    fe_sub(f, &rr, &s2, &s1);
    if (bn_is_zero(&h)) {
        if (bn_is_zero(&rr)) {
            pt_double(f, r, p);
        } else {
            memset(r, 0, sizeof(*r));
        }
        return;
    }
    fe_sqr(f, &hh, &h);
    fe_mul(f, &hhh, &h, &hh);
    fe_mul(f, &v, &u1, &hh);
    fe_sqr(f, &x3, &rr);
    fe_sub(f, &x3, &x3, &hhh);
    fe_sub(f, &x3, &x3, &v);
    fe_sub(f, &x3, &x3, &v);
    fe_sub(f, &t, &v, &x3);
    fe_mul(f, &y3, &rr, &t);
    fe_mul(f, &t, &s1, &hhh);
    fe_sub(f, &y3, &y3, &t);
    fe_mul(f, &z3, &p->z, &q->z);
    fe_mul(f, &z3, &z3, &h);
    r->x = x3;
    r->y = y3;
    r->z = z3;
}

static void pt_from_affine(const field* f, point* r, const bn* x, const bn* y) {
    fe_to_mont(f, &r->x, x);
    fe_to_mont(f, &r->y, y);
    fe_to_mont(f, &r->z, &(bn){{1}});
}

// y^2 == x^3 + ax + b, for affine x, y < p
static int on_curve(const field* f, const bn* x, const bn* y) {
    bn a, b, mx, my, lhs, rhs, t;
    bn_from_bytes(&a, sm2_a);
    bn_from_bytes(&b, sm2_b);
    fe_to_mont(f, &a, &a);
    fe_to_mont(f, &b, &b);
    fe_to_mont(f, &mx, x);
    fe_to_mont(f, &my, y);
    fe_sqr(f, &lhs, &my);
    fe_sqr(f, &rhs, &mx);
    fe_mul(f, &rhs, &rhs, &mx);
    fe_mul(f, &t, &a, &mx);
    fe_add(f, &rhs, &rhs, &t);
    fe_add(f, &rhs, &rhs, &b);
    return bn_cmp(&lhs, &rhs) == 0;
}

// ZA = SM3(ENTL || ID || a || b || Gx || Gy || xA || yA), with the default ID of GB/T 35276
static void sm2_za(const unsigned char pub[64], unsigned char za[OFD_SM3_SIZE]) {
    static const char id[] = "1234567812345678";
    unsigned char entl[2] = {0, (unsigned char)((sizeof(id) - 1) * 8)};
    ofd_sm3_ctx c;
    ofd_sm3_init(&c);
    ofd_sm3_update(&c, entl, 2);
    ofd_sm3_update(&c, (const unsigned char*)id, sizeof(id) - 1);
    ofd_sm3_update(&c, sm2_a, 32);
    ofd_sm3_update(&c, sm2_b, 32);
    ofd_sm3_update(&c, sm2_gx, 32);
    ofd_sm3_update(&c, sm2_gy, 32);
    ofd_sm3_update(&c, pub, 64);
    ofd_sm3_final(&c, za);
}

// SM3(ZA || data): the digest SM2 signs, also used alone as a keyed digest (OID 1.2.156.10197.1.401.2)
void ofd_sm3_with_key(const unsigned char pub[2 * OFD_SM2_COORD_SIZE], const unsigned char* data, size_t len,
                      unsigned char out[OFD_SM3_SIZE]) {
    unsigned char za[OFD_SM3_SIZE];
    sm2_za(pub, za);
    ofd_sm3_ctx c;
    ofd_sm3_init(&c);
    ofd_sm3_update(&c, za, sizeof(za));
    ofd_sm3_update(&c, data, len);
    ofd_sm3_final(&c, out);
}

// pub is the key's x || y; returns 1 if (r, s) is a valid SM3-with-SM2 signature of msg
int ofd_sm2_verify(const unsigned char pub[2 * OFD_SM2_COORD_SIZE], const unsigned char* msg, size_t msg_len,
                   const unsigned char rb[OFD_SM2_COORD_SIZE], const unsigned char sb[OFD_SM2_COORD_SIZE]) {
    field f;
    field_init(&f);
    bn n, r, s, t, px, py, gx, gy, e, x1;
    bn_from_bytes(&n, sm2_n);
    bn_from_bytes(&r, rb);
    bn_from_bytes(&s, sb);
    if (bn_is_zero(&r) || bn_cmp(&r, &n) >= 0 || bn_is_zero(&s) || bn_cmp(&s, &n) >= 0) return 0;
    bn_mod_add(&t, &r, &s, &n);
    if (bn_is_zero(&t)) return 0;

    bn_from_bytes(&px, pub);
    bn_from_bytes(&py, pub + 32);
    if (bn_cmp(&px, &f.p) >= 0 || bn_cmp(&py, &f.p) >= 0 || !on_curve(&f, &px, &py)) return 0;

    unsigned char digest[OFD_SM3_SIZE];
    ofd_sm3_with_key(pub, msg, msg_len, digest);
    bn_from_bytes(&e, digest);

    // (x1, y1) = sG + tP, both scalars at once (Shamir's trick)
    point g, p, gp, acc;
    bn_from_bytes(&gx, sm2_gx);
    bn_from_bytes(&gy, sm2_gy);
    pt_from_affine(&f, &g, &gx, &gy);
    pt_from_affine(&f, &p, &px, &py);
    pt_add(&f, &gp, &g, &p);
    memset(&acc, 0, sizeof(acc));
    for (int i = 255; i >= 0; --i) {
        pt_double(&f, &acc, &acc);
        int sbit = bn_bit(&s, i), tbit = bn_bit(&t, i);
        if (sbit && tbit) {
            pt_add(&f, &acc, &acc, &gp);
        } else if (sbit) {
            pt_add(&f, &acc, &acc, &g);
        } else if (tbit) {
            pt_add(&f, &acc, &acc, &p);
        }
    }
    if (bn_is_zero(&acc.z)) return 0;
    bn zinv, zinv2;
    fe_inv(&f, &zinv, &acc.z);
    fe_sqr(&f, &zinv2, &zinv);
    fe_mul(&f, &x1, &acc.x, &zinv2);
    fe_from_mont(&f, &x1, &x1);

    // R = (e + x1) mod n must equal r; e < 2^256 < 2n and x1 < p < 2n
    if (bn_cmp(&e, &n) >= 0) bn_sub(&e, &e, &n);
    if (bn_cmp(&x1, &n) >= 0) bn_sub(&x1, &x1, &n);
    bn_mod_add(&e, &e, &x1, &n);
    unsigned char got[32], want[32];
    bn_to_bytes(got, &e);
    bn_to_bytes(want, &r);
    return memcmp(got, want, 32) == 0;
}

//--- SM3 key derivation (GB/T 32918.3 5.4.3)

// out = SM3(z || 1) || SM3(z || 2) || ..., truncated to out_len
void ofd_sm3_kdf(const unsigned char* z, size_t z_len, unsigned char* out, size_t out_len) {
    unsigned int counter = 1;
    while (out_len > 0) {
        unsigned char ct[4] = {(unsigned char)(counter >> 24), (unsigned char)(counter >> 16),
                               (unsigned char)(counter >> 8), (unsigned char)counter};
        unsigned char digest[OFD_SM3_SIZE];
        ofd_sm3_ctx c;
        ofd_sm3_init(&c);
        ofd_sm3_update(&c, z, z_len);
        ofd_sm3_update(&c, ct, sizeof(ct));
        ofd_sm3_final(&c, digest);
        size_t n = out_len < sizeof(digest) ? out_len : sizeof(digest);
        memcpy(out, digest, n);
        out += n;
        out_len -= n;
        ++counter;
    }
}

//--- SM4 (GB/T 32907) decryption in CBC mode

static const unsigned char sm4_sbox[256] = {
    0xd6, 0x90, 0xe9, 0xfe, 0xcc, 0xe1, 0x3d, 0xb7, 0x16, 0xb6, 0x14, 0xc2, 0x28, 0xfb, 0x2c, 0x05, 0x2b, 0x67, 0x9a,
    0x76, 0x2a, 0xbe, 0x04, 0xc3, 0xaa, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99, 0x9c, 0x42, 0x50, 0xf4, 0x91, 0xef,
    0x98, 0x7a, 0x33, 0x54, 0x0b, 0x43, 0xed, 0xcf, 0xac, 0x62, 0xe4, 0xb3, 0x1c, 0xa9, 0xc9, 0x08, 0xe8, 0x95, 0x80,
    0xdf, 0x94, 0xfa, 0x75, 0x8f, 0x3f, 0xa6, 0x47, 0x07, 0xa7, 0xfc, 0xf3, 0x73, 0x17, 0xba, 0x83, 0x59, 0x3c, 0x19,
    0xe6, 0x85, 0x4f, 0xa8, 0x68, 0x6b, 0x81, 0xb2, 0x71, 0x64, 0xda, 0x8b, 0xf8, 0xeb, 0x0f, 0x4b, 0x70, 0x56, 0x9d,
    0x35, 0x1e, 0x24, 0x0e, 0x5e, 0x63, 0x58, 0xd1, 0xa2, 0x25, 0x22, 0x7c, 0x3b, 0x01, 0x21, 0x78, 0x87, 0xd4, 0x00,
    0x46, 0x57, 0x9f, 0xd3, 0x27, 0x52, 0x4c, 0x36, 0x02, 0xe7, 0xa0, 0xc4, 0xc8, 0x9e, 0xea, 0xbf, 0x8a, 0xd2, 0x40,
    0xc7, 0x38, 0xb5, 0xa3, 0xf7, 0xf2, 0xce, 0xf9, 0x61, 0x15, 0xa1, 0xe0, 0xae, 0x5d, 0xa4, 0x9b, 0x34, 0x1a, 0x55,
    0xad, 0x93, 0x32, 0x30, 0xf5, 0x8c, 0xb1, 0xe3, 0x1d, 0xf6, 0xe2, 0x2e, 0x82, 0x66, 0xca, 0x60, 0xc0, 0x29, 0x23,
    0xab, 0x0d, 0x53, 0x4e, 0x6f, 0xd5, 0xdb, 0x37, 0x45, 0xde, 0xfd, 0x8e, 0x2f, 0x03, 0xff, 0x6a, 0x72, 0x6d, 0x6c,
    0x5b, 0x51, 0x8d, 0x1b, 0xaf, 0x92, 0xbb, 0xdd, 0xbc, 0x7f, 0x11, 0xd9, 0x5c, 0x41, 0x1f, 0x10, 0x5a, 0xd8, 0x0a,
    0xc1, 0x31, 0x88, 0xa5, 0xcd, 0x7b, 0xbd, 0x2d, 0x74, 0xd0, 0x12, 0xb8, 0xe5, 0xb4, 0xb0, 0x89, 0x69, 0x97, 0x4a,
    0x0c, 0x96, 0x77, 0x7e, 0x65, 0xb9, 0xf1, 0x09, 0xc5, 0x6e, 0xc6, 0x84, 0x18, 0xf0, 0x7d, 0xec, 0x3a, 0xdc, 0x4d,
    0x20, 0x79, 0xee, 0x5f, 0x3e, 0xd7, 0xcb, 0x39, 0x48};

static unsigned int sm4_tau(unsigned int a) {
    return ((unsigned int)sm4_sbox[a >> 24] << 24) | ((unsigned int)sm4_sbox[(a >> 16) & 0xff] << 16) |
           ((unsigned int)sm4_sbox[(a >> 8) & 0xff] << 8) | sm4_sbox[a & 0xff];
}

static unsigned int sm4_load(const unsigned char* b) {
    return ((unsigned int)b[0] << 24) | ((unsigned int)b[1] << 16) | ((unsigned int)b[2] << 8) | b[3];
}

static void sm4_store(unsigned char* b, unsigned int v) {
    b[0] = (unsigned char)(v >> 24);
    b[1] = (unsigned char)(v >> 16);
    b[2] = (unsigned char)(v >> 8);
    b[3] = (unsigned char)v;
}

static void sm4_round_keys(const unsigned char key[OFD_SM4_BLOCK], unsigned int rk[32]) {
    static const unsigned int fk[4] = {0xa3b1bac6, 0x56aa3350, 0x677d9197, 0xb27022dc};
    unsigned int k[36];
    for (int i = 0; i < 4; ++i) k[i] = sm4_load(key + 4 * i) ^ fk[i];
    for (int i = 0; i < 32; ++i) {
        // CK_i: bytes (4i + j) * 7 mod 256
        unsigned int ck = 0;
        for (int j = 0; j < 4; ++j) ck = (ck << 8) | (unsigned int)(((4 * i + j) * 7) & 0xff);
        unsigned int t = sm4_tau(k[i + 1] ^ k[i + 2] ^ k[i + 3] ^ ck);
        k[i + 4] = k[i] ^ t ^ sm3_rotl(t, 13) ^ sm3_rotl(t, 23);
        rk[i] = k[i + 4];
    }
}

// one block with round keys in order (encrypt) or reversed (decrypt)
static void sm4_block(const unsigned int rk[32], const unsigned char* in, unsigned char* out) {
    unsigned int x[36];
    for (int i = 0; i < 4; ++i) x[i] = sm4_load(in + 4 * i);
    for (int i = 0; i < 32; ++i) {
        unsigned int t = sm4_tau(x[i + 1] ^ x[i + 2] ^ x[i + 3] ^ rk[i]);
        x[i + 4] = x[i] ^ t ^ sm3_rotl(t, 2) ^ sm3_rotl(t, 10) ^ sm3_rotl(t, 18) ^ sm3_rotl(t, 24);
    }
    for (int i = 0; i < 4; ++i) sm4_store(out + 4 * i, x[35 - i]);
}

// Decrypts in place and removes PKCS#7 padding; returns the plaintext length, or 0 if the
// length or padding is wrong (as with a wrong key).
size_t ofd_sm4_cbc_decrypt(const unsigned char key[OFD_SM4_BLOCK], const unsigned char iv[OFD_SM4_BLOCK],
                           unsigned char* data, size_t len) {
    if (len == 0 || len % OFD_SM4_BLOCK) return 0;
    unsigned int rk[32], drk[32];
    sm4_round_keys(key, rk);
    for (int i = 0; i < 32; ++i) drk[i] = rk[31 - i];
    unsigned char prev[OFD_SM4_BLOCK], cur[OFD_SM4_BLOCK];
    memcpy(prev, iv, sizeof(prev));
    for (size_t off = 0; off < len; off += OFD_SM4_BLOCK) {
        memcpy(cur, data + off, sizeof(cur));
        sm4_block(drk, cur, data + off);
        for (int i = 0; i < OFD_SM4_BLOCK; ++i) data[off + i] ^= prev[i];
        memcpy(prev, cur, sizeof(prev));
    }
    unsigned char pad = data[len - 1];
    if (pad < 1 || pad > OFD_SM4_BLOCK) return 0;
    for (size_t i = len - pad; i < len; ++i) {
        if (data[i] != pad) return 0;
    }
    return len - pad;
}
