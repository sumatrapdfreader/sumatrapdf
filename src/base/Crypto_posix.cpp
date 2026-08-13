/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Crypto.h"

#if OS_DARWIN
#include <CommonCrypto/CommonDigest.h>

void CalcMD5Digest(Str data, u8 digest[16]) {
    CC_MD5(data.s, (CC_LONG)data.len, digest);
}

void CalcSHA1Digest(Str data, u8 digest[20]) {
    CC_SHA1(data.s, (CC_LONG)data.len, digest);
}

void CalcSHA2Digest(Str data, u8 digest[32]) {
    CC_SHA256(data.s, (CC_LONG)data.len, digest);
}

#else
// Linux (and other non-Darwin POSIX): self-contained hashes so we don't
// require openssl headers for base/. mupdf still links -lcrypto separately.

static u32 Rol32(u32 x, int n) {
    return (x << n) | (x >> (32 - n));
}

static u32 LoadBE32(const u8* p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static void StoreBE32(u8* p, u32 v) {
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

static u32 LoadLE32(const u8* p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static void StoreLE32(u8* p, u32 v) {
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}

static void Md5Transform(u32 state[4], const u8 block[64]) {
    u32 a = state[0], b = state[1], c = state[2], d = state[3];
    u32 x[16];
    for (int i = 0; i < 16; i++) {
        x[i] = LoadLE32(block + i * 4);
    }
#define MD5_F(x, y, z) ((x & y) | (~x & z))
#define MD5_G(x, y, z) ((x & z) | (y & ~z))
#define MD5_H(x, y, z) (x ^ y ^ z)
#define MD5_I(x, y, z) (y ^ (x | ~z))
#define MD5_STEP(f, a, b, c, d, xk, s, t) \
    a += f(b, c, d) + xk + t;             \
    a = Rol32(a, s) + b
    MD5_STEP(MD5_F, a, b, c, d, x[0], 7, 0xd76aa478);
    MD5_STEP(MD5_F, d, a, b, c, x[1], 12, 0xe8c7b756);
    MD5_STEP(MD5_F, c, d, a, b, x[2], 17, 0x242070db);
    MD5_STEP(MD5_F, b, c, d, a, x[3], 22, 0xc1bdceee);
    MD5_STEP(MD5_F, a, b, c, d, x[4], 7, 0xf57c0faf);
    MD5_STEP(MD5_F, d, a, b, c, x[5], 12, 0x4787c62a);
    MD5_STEP(MD5_F, c, d, a, b, x[6], 17, 0xa8304613);
    MD5_STEP(MD5_F, b, c, d, a, x[7], 22, 0xfd469501);
    MD5_STEP(MD5_F, a, b, c, d, x[8], 7, 0x698098d8);
    MD5_STEP(MD5_F, d, a, b, c, x[9], 12, 0x8b44f7af);
    MD5_STEP(MD5_F, c, d, a, b, x[10], 17, 0xffff5bb1);
    MD5_STEP(MD5_F, b, c, d, a, x[11], 22, 0x895cd7be);
    MD5_STEP(MD5_F, a, b, c, d, x[12], 7, 0x6b901122);
    MD5_STEP(MD5_F, d, a, b, c, x[13], 12, 0xfd987193);
    MD5_STEP(MD5_F, c, d, a, b, x[14], 17, 0xa679438e);
    MD5_STEP(MD5_F, b, c, d, a, x[15], 22, 0x49b40821);
    MD5_STEP(MD5_G, a, b, c, d, x[1], 5, 0xf61e2562);
    MD5_STEP(MD5_G, d, a, b, c, x[6], 9, 0xc040b340);
    MD5_STEP(MD5_G, c, d, a, b, x[11], 14, 0x265e5a51);
    MD5_STEP(MD5_G, b, c, d, a, x[0], 20, 0xe9b6c7aa);
    MD5_STEP(MD5_G, a, b, c, d, x[5], 5, 0xd62f105d);
    MD5_STEP(MD5_G, d, a, b, c, x[10], 9, 0x02441453);
    MD5_STEP(MD5_G, c, d, a, b, x[15], 14, 0xd8a1e681);
    MD5_STEP(MD5_G, b, c, d, a, x[4], 20, 0xe7d3fbc8);
    MD5_STEP(MD5_G, a, b, c, d, x[9], 5, 0x21e1cde6);
    MD5_STEP(MD5_G, d, a, b, c, x[14], 9, 0xc33707d6);
    MD5_STEP(MD5_G, c, d, a, b, x[3], 14, 0xf4d50d87);
    MD5_STEP(MD5_G, b, c, d, a, x[8], 20, 0x455a14ed);
    MD5_STEP(MD5_G, a, b, c, d, x[13], 5, 0xa9e3e905);
    MD5_STEP(MD5_G, d, a, b, c, x[2], 9, 0xfcefa3f8);
    MD5_STEP(MD5_G, c, d, a, b, x[7], 14, 0x676f02d9);
    MD5_STEP(MD5_G, b, c, d, a, x[12], 20, 0x8d2a4c8a);
    MD5_STEP(MD5_H, a, b, c, d, x[5], 4, 0xfffa3942);
    MD5_STEP(MD5_H, d, a, b, c, x[8], 11, 0x8771f681);
    MD5_STEP(MD5_H, c, d, a, b, x[11], 16, 0x6d9d6122);
    MD5_STEP(MD5_H, b, c, d, a, x[14], 23, 0xfde5380c);
    MD5_STEP(MD5_H, a, b, c, d, x[1], 4, 0xa4beea44);
    MD5_STEP(MD5_H, d, a, b, c, x[4], 11, 0x4bdecfa9);
    MD5_STEP(MD5_H, c, d, a, b, x[7], 16, 0xf6bb4b60);
    MD5_STEP(MD5_H, b, c, d, a, x[10], 23, 0xbebfbc70);
    MD5_STEP(MD5_H, a, b, c, d, x[13], 4, 0x289b7ec6);
    MD5_STEP(MD5_H, d, a, b, c, x[0], 11, 0xeaa127fa);
    MD5_STEP(MD5_H, c, d, a, b, x[3], 16, 0xd4ef3085);
    MD5_STEP(MD5_H, b, c, d, a, x[6], 23, 0x04881d05);
    MD5_STEP(MD5_H, a, b, c, d, x[9], 4, 0xd9d4d039);
    MD5_STEP(MD5_H, d, a, b, c, x[12], 11, 0xe6db99e5);
    MD5_STEP(MD5_H, c, d, a, b, x[15], 16, 0x1fa27cf8);
    MD5_STEP(MD5_H, b, c, d, a, x[2], 23, 0xc4ac5665);
    MD5_STEP(MD5_I, a, b, c, d, x[0], 6, 0xf4292244);
    MD5_STEP(MD5_I, d, a, b, c, x[7], 10, 0x432aff97);
    MD5_STEP(MD5_I, c, d, a, b, x[14], 15, 0xab9423a7);
    MD5_STEP(MD5_I, b, c, d, a, x[5], 21, 0xfc93a039);
    MD5_STEP(MD5_I, a, b, c, d, x[12], 6, 0x655b59c3);
    MD5_STEP(MD5_I, d, a, b, c, x[3], 10, 0x8f0ccc92);
    MD5_STEP(MD5_I, c, d, a, b, x[10], 15, 0xffeff47d);
    MD5_STEP(MD5_I, b, c, d, a, x[1], 21, 0x85845dd1);
    MD5_STEP(MD5_I, a, b, c, d, x[8], 6, 0x6fa87e4f);
    MD5_STEP(MD5_I, d, a, b, c, x[15], 10, 0xfe2ce6e0);
    MD5_STEP(MD5_I, c, d, a, b, x[6], 15, 0xa3014314);
    MD5_STEP(MD5_I, b, c, d, a, x[13], 21, 0x4e0811a1);
    MD5_STEP(MD5_I, a, b, c, d, x[4], 6, 0xf7537e82);
    MD5_STEP(MD5_I, d, a, b, c, x[11], 10, 0xbd3af235);
    MD5_STEP(MD5_I, c, d, a, b, x[2], 15, 0x2ad7d2bb);
    MD5_STEP(MD5_I, b, c, d, a, x[9], 21, 0xeb86d391);
#undef MD5_F
#undef MD5_G
#undef MD5_H
#undef MD5_I
#undef MD5_STEP
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

static void HashMd5(const u8* data, size_t len, u8 digest[16]) {
    u32 state[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    u8 block[64];
    size_t i = 0;
    for (; i + 64 <= len; i += 64) {
        Md5Transform(state, data + i);
    }
    size_t rem = len - i;
    memcpy(block, data + i, rem);
    block[rem] = 0x80;
    if (rem < 56) {
        memset(block + rem + 1, 0, 55 - rem);
    } else {
        memset(block + rem + 1, 0, 63 - rem);
        Md5Transform(state, block);
        memset(block, 0, 56);
    }
    u64 bits = (u64)len * 8;
    StoreLE32(block + 56, (u32)bits);
    StoreLE32(block + 60, (u32)(bits >> 32));
    Md5Transform(state, block);
    for (int k = 0; k < 4; k++) {
        StoreLE32(digest + k * 4, state[k]);
    }
}

static void Sha1Transform(u32 state[5], const u8 block[64]) {
    u32 w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = LoadBE32(block + i * 4);
    }
    for (int i = 16; i < 80; i++) {
        w[i] = Rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }
    u32 a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    for (int i = 0; i < 80; i++) {
        u32 f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5a827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ed9eba1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8f1bbcdc;
        } else {
            f = b ^ c ^ d;
            k = 0xca62c1d6;
        }
        u32 temp = Rol32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = Rol32(b, 30);
        b = a;
        a = temp;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

static void HashSha1(const u8* data, size_t len, u8 digest[20]) {
    u32 state[5] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};
    u8 block[64];
    size_t i = 0;
    for (; i + 64 <= len; i += 64) {
        Sha1Transform(state, data + i);
    }
    size_t rem = len - i;
    memcpy(block, data + i, rem);
    block[rem] = 0x80;
    if (rem < 56) {
        memset(block + rem + 1, 0, 55 - rem);
    } else {
        memset(block + rem + 1, 0, 63 - rem);
        Sha1Transform(state, block);
        memset(block, 0, 56);
    }
    u64 bits = (u64)len * 8;
    StoreBE32(block + 56, (u32)(bits >> 32));
    StoreBE32(block + 60, (u32)bits);
    Sha1Transform(state, block);
    for (int k = 0; k < 5; k++) {
        StoreBE32(digest + k * 4, state[k]);
    }
}

static const u32 kSha256K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static void Sha256Transform(u32 state[8], const u8 block[64]) {
    u32 w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = LoadBE32(block + i * 4);
    }
    for (int i = 16; i < 64; i++) {
        u32 s0 = Rol32(w[i - 15], 25) ^ Rol32(w[i - 15], 14) ^ (w[i - 15] >> 3);
        u32 s1 = Rol32(w[i - 2], 15) ^ Rol32(w[i - 2], 13) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    u32 a = state[0], b = state[1], c = state[2], d = state[3];
    u32 e = state[4], f = state[5], g = state[6], h = state[7];
    for (int i = 0; i < 64; i++) {
        u32 S1 = Rol32(e, 26) ^ Rol32(e, 21) ^ Rol32(e, 7);
        u32 ch = (e & f) ^ (~e & g);
        u32 temp1 = h + S1 + ch + kSha256K[i] + w[i];
        u32 S0 = Rol32(a, 30) ^ Rol32(a, 19) ^ Rol32(a, 10);
        u32 maj = (a & b) ^ (a & c) ^ (b & c);
        u32 temp2 = S0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

static void HashSha256(const u8* data, size_t len, u8 digest[32]) {
    u32 state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    u8 block[64];
    size_t i = 0;
    for (; i + 64 <= len; i += 64) {
        Sha256Transform(state, data + i);
    }
    size_t rem = len - i;
    memcpy(block, data + i, rem);
    block[rem] = 0x80;
    if (rem < 56) {
        memset(block + rem + 1, 0, 55 - rem);
    } else {
        memset(block + rem + 1, 0, 63 - rem);
        Sha256Transform(state, block);
        memset(block, 0, 56);
    }
    u64 bits = (u64)len * 8;
    StoreBE32(block + 56, (u32)(bits >> 32));
    StoreBE32(block + 60, (u32)bits);
    Sha256Transform(state, block);
    for (int k = 0; k < 8; k++) {
        StoreBE32(digest + k * 4, state[k]);
    }
}

void CalcMD5Digest(Str data, u8 digest[16]) {
    HashMd5((const u8*)(data.s ? data.s : ""), (size_t)data.len, digest);
}

void CalcSHA1Digest(Str data, u8 digest[20]) {
    HashSha1((const u8*)(data.s ? data.s : ""), (size_t)data.len, digest);
}

void CalcSHA2Digest(Str data, u8 digest[32]) {
    HashSha256((const u8*)(data.s ? data.s : ""), (size_t)data.len, digest);
}

#endif

bool VerifySHA1Signature(Str /*data*/, Str /*hexSignature*/, Str /*pubkey*/) {
    return false;
}

// extracts the content (e.g. PDF) from a PKCS#7 / .p7m wrapper using Win32 crypto APIs
Str ExtractP7m(Str /*d*/) {
    return {};
}

// Authenticode / PE signature helpers (Windows only; stubs return false/null on POSIX)
bool IsPEFileSigned(Str /*filePath*/) {
    return false;
}

TempStr GetExecutableSignerTemp(Str /*exePath*/) {
    return {};
}
