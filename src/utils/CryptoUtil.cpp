/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef NO_LIBMUPDF
extern "C" {
#include <mupdf/fitz/crypt.h>
}
#endif

#include "utils/BaseUtil.h"
#include "utils/CryptoUtil.h"

#ifndef DWORD_MAX
#define DWORD_MAX 0xffffffffUL
#endif

#ifndef NO_LIBMUPDF

void CalcMD5Digest(const unsigned char* data, size_t byteCount, unsigned char digest[16]) {
    fz_md5 md5;
    fz_md5_init(&md5);
#ifdef _WIN64
    for (; byteCount > UINT_MAX; data += UINT_MAX, byteCount -= UINT_MAX)
        fz_md5_update(&md5, data, UINT_MAX);
#endif
    fz_md5_update(&md5, data, (unsigned int)byteCount);
    fz_md5_final(&md5, digest);
}

void CalcSHA2Digest(const unsigned char* data, size_t byteCount, unsigned char digest[32]) {
    fz_sha256 sha2;
    fz_sha256_init(&sha2);
#ifdef _WIN64
    for (; byteCount > UINT_MAX; data += UINT_MAX, byteCount -= UINT_MAX)
        fz_sha256_update(&sha2, data, UINT_MAX);
#endif
    fz_sha256_update(&sha2, data, (unsigned int)byteCount);
    fz_sha256_final(&sha2, digest);
}

#else

void CalcMD5Digest(const unsigned char* data, size_t byteCount, unsigned char digest[16]) {
    CalcMD5DigestWin(data, byteCount, digest);
}

void CalcSHA2Digest(const unsigned char* data, size_t byteCount, unsigned char digest[32]) {
    CalcSha2DigestWin(data, byteCount, digest);
}

#endif

// Note: this crashes under Win2000, use SHA2 or MD5 instad
void CalcSHA1Digest(const unsigned char* data, size_t byteCount, unsigned char digest[20]) {
    CalcSha1DigestWin(data, byteCount, digest);
}

// Note: MS_ENH_RSA_AES_PROV, etc. aren't defined in the SDK shipping with VS2008
#ifndef MS_ENH_RSA_AES_PROV
#define MS_ENH_RSA_AES_PROV L"Microsoft Enhanced RSA and AES Cryptographic Provider"
#endif
#ifndef MS_ENH_RSA_AES_PROV_XP
#define MS_ENH_RSA_AES_PROV_XP L"Microsoft Enhanced RSA and AES Cryptographic Provider (Prototype)"
#endif
#ifndef PROV_RSA_AES
#define PROV_RSA_AES 24
#endif
// Note: CALG_SHA_256 isn't available for Windows XP SP2 and below
#ifndef CALG_SHA_256
#define ALG_SID_SHA_256 12
#define CALG_SHA_256 (ALG_CLASS_HASH | ALG_TYPE_ANY | ALG_SID_SHA_256)
#endif

// MD5 digest that uses Windows' CryptoAPI. It's good for code that doesn't already
// have MD5 code (smaller code) and it's probably faster than most other implementations
// TODO: could try to use CryptoNG available starting in Vista. But then again, would that be worth it?
void CalcMD5DigestWin(const void* data, size_t byteCount, unsigned char digest[16]) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    // http://stackoverflow.com/questions/9794745/ms-cryptoapi-doesnt-work-on-windows-xp-with-cryptacquirecontext
    BOOL ok = CryptAcquireContext(&hProv, nullptr, MS_DEF_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    if (!ok)
        ok = CryptAcquireContext(&hProv, nullptr, MS_ENH_RSA_AES_PROV_XP, PROV_RSA_AES, CRYPT_VERIFYCONTEXT);

    CrashAlwaysIf(!ok);
    ok = CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash);
    CrashAlwaysIf(!ok);
#ifdef _WIN64
    for (; byteCount > DWORD_MAX; data = (const BYTE*)data + DWORD_MAX, byteCount -= DWORD_MAX) {
        ok = CryptHashData(hHash, (const BYTE*)data, DWORD_MAX, 0);
        CrashAlwaysIf(!ok);
    }
#endif
    CrashAlwaysIf(byteCount > DWORD_MAX);
    ok = CryptHashData(hHash, (const BYTE*)data, (DWORD)byteCount, 0);
    CrashAlwaysIf(!ok);

    DWORD hashLen;
    DWORD argSize = sizeof(DWORD);
    ok = CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&hashLen, &argSize, 0);
    CrashIf(sizeof(DWORD) != argSize);
    CrashAlwaysIf(!ok);
    CrashAlwaysIf(16 != hashLen);
    ok = CryptGetHashParam(hHash, HP_HASHVAL, digest, &hashLen, 0);
    CrashAlwaysIf(!ok);
    CrashAlwaysIf(16 != hashLen);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
}

// SHA1 digest that uses Windows' CryptoAPI. It's good for code that doesn't already
// have SHA1 code (smaller code) and it's probably faster than most other implementations
// TODO: hasn't been tested for corectness
void CalcSha1DigestWin(const void* data, size_t byteCount, unsigned char digest[20]) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    BOOL ok = CryptAcquireContext(&hProv, nullptr, MS_DEF_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    CrashAlwaysIf(!ok);
    ok = CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash);
    CrashAlwaysIf(!ok);
#ifdef _WIN64
    for (; byteCount > DWORD_MAX; data = (const BYTE*)data + DWORD_MAX, byteCount -= DWORD_MAX) {
        ok = CryptHashData(hHash, (const BYTE*)data, DWORD_MAX, 0);
        CrashAlwaysIf(!ok);
    }
#endif
    CrashAlwaysIf(byteCount > DWORD_MAX);
    ok = CryptHashData(hHash, (const BYTE*)data, (DWORD)byteCount, 0);
    CrashAlwaysIf(!ok);

    DWORD hashLen;
    DWORD argSize = sizeof(DWORD);
    ok = CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&hashLen, &argSize, 0);
    CrashIf(sizeof(DWORD) != argSize);
    CrashAlwaysIf(!ok);
    CrashAlwaysIf(20 != hashLen);
    ok = CryptGetHashParam(hHash, HP_HASHVAL, digest, &hashLen, 0);
    CrashAlwaysIf(!ok);
    CrashAlwaysIf(20 != hashLen);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
}

void CalcSha2DigestWin(const void* data, size_t byteCount, unsigned char digest[32]) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    BOOL ok = CryptAcquireContext(&hProv, nullptr, MS_ENH_RSA_AES_PROV, PROV_RSA_AES, CRYPT_VERIFYCONTEXT);
    if (!ok) {
        // TODO: test this on XP SP3
        ok = CryptAcquireContext(&hProv, nullptr, MS_ENH_RSA_AES_PROV_XP, PROV_RSA_AES, CRYPT_VERIFYCONTEXT);
    }
    CrashAlwaysIf(!ok);
    ok = CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash);
    CrashAlwaysIf(!ok);
#ifdef _WIN64
    for (; byteCount > DWORD_MAX; data = (const BYTE*)data + DWORD_MAX, byteCount -= DWORD_MAX) {
        ok = CryptHashData(hHash, (const BYTE*)data, DWORD_MAX, 0);
        CrashAlwaysIf(!ok);
    }
#endif
    CrashAlwaysIf(byteCount > DWORD_MAX);
    ok = CryptHashData(hHash, (const BYTE*)data, (DWORD)byteCount, 0);
    CrashAlwaysIf(!ok);

    DWORD hashLen;
    DWORD argSize = sizeof(DWORD);
    ok = CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&hashLen, &argSize, 0);
    CrashIf(sizeof(DWORD) != argSize);
    CrashAlwaysIf(!ok);
    CrashAlwaysIf(32 != hashLen);
    ok = CryptGetHashParam(hHash, HP_HASHVAL, digest, &hashLen, 0);
    CrashAlwaysIf(!ok);
    CrashAlwaysIf(32 != hashLen);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
}

static bool ExtractSignature(const char* hexSignature, const void* data, size_t& dataLen, ScopedMem<BYTE>& signature,
                             size_t& signatureLen) {
    // verify hexSignature format - must be either
    // * a string starting with "sha1:" followed by the signature (and optionally whitespace and further content)
    // * nullptr, then the signature must be found on the last line of non-binary data, starting at " Signature sha1:"
    if (str::StartsWith(hexSignature, "sha1:"))
        hexSignature += 5;
    else if (!hexSignature) {
        if (dataLen < 20 || memchr(data, 0, dataLen))
            return false;
        const char* lastLine = (const char*)data + dataLen - 1;
        while (lastLine > data && *(lastLine - 1) != '\n')
            lastLine--;
        if (lastLine == data || !str::Find(lastLine, " Signature sha1:"))
            return false;
        dataLen = lastLine - (const char*)data;
        hexSignature = str::Find(lastLine, " Signature sha1:") + 16;
    } else
        return false;

    Vec<BYTE> signatureBytes;
    for (const char* c = hexSignature; *c && !str::IsWs(*c); c += 2) {
        int val;
        if (1 != sscanf_s(c, "%02x", &val))
            return false;
        signatureBytes.Append((BYTE)val);
    }
    signatureLen = signatureBytes.size();
    signature.Set(signatureBytes.StealData());
    return true;
}

bool VerifySHA1Signature(const void* data, size_t dataLen, const char* hexSignature, const void* pubkey,
                         size_t pubkeyLen) {
    HCRYPTPROV hProv = 0;
    HCRYPTKEY hPubKey = 0;
    HCRYPTHASH hHash = 0;
    BOOL ok = false;
    ScopedMem<BYTE> signature;
    size_t signatureLen;

#define Check(val)             \
    if ((ok = (val)) == FALSE) \
    goto CleanUp
    Check(ExtractSignature(hexSignature, data, dataLen, signature, signatureLen));
    Check(CryptAcquireContext(&hProv, nullptr, MS_DEF_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT));
    Check(CryptImportKey(hProv, (const BYTE*)pubkey, (DWORD)pubkeyLen, 0, 0, &hPubKey));
    Check(CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash));
#ifdef _WIN64
    for (; dataLen > DWORD_MAX; data = (const BYTE*)data + DWORD_MAX, dataLen -= DWORD_MAX) {
        Check(CryptHashData(hHash, (const BYTE*)data, DWORD_MAX, 0));
    }
#endif
    Check(dataLen <= DWORD_MAX && pubkeyLen <= DWORD_MAX && signatureLen <= DWORD_MAX);
    Check(CryptHashData(hHash, (const BYTE*)data, (DWORD)dataLen, 0));
    Check(CryptVerifySignature(hHash, signature, (DWORD)signatureLen, hPubKey, nullptr, 0));
#undef Check

CleanUp:
    if (hHash)
        CryptDestroyHash(hHash);
    if (hProv)
        CryptReleaseContext(hProv, 0);
    return ok;
}
